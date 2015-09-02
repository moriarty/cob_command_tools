/*!
 *****************************************************************
 * \file
 *
 * \note
 *   Copyright (c) 2015 \n
 *   Fraunhofer Institute for Manufacturing Engineering
 *   and Automation (IPA) \n\n
 *
 *****************************************************************
 *
 * \note
 *   Project name: care-o-bot
 * \note
 *   ROS stack name: cob_command_tools
 * \note
 *   ROS package name: cob_teleop
 *
 * \author
 *   Author: Nadia Hammoudeh García, email:nadia.hammoudeh.garcia@ipa.fhg.de
 * \author
 *   Supervised by: Nadia Hammoudeh García, email:nadia.hammoudeh.garcia@ipa.fhg.de
 *
 * \date Date of creation: August 2015
 *
 * \brief
 *   Implementation of teleoperation node.
 *
 *****************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     - Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer. \n
 *     - Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution. \n
 *     - Neither the name of the Fraunhofer Institute for Manufacturing
 *       Engineering and Automation (IPA) nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission. \n
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License LGPL as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License LGPL for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License LGPL along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************/
#include <ros/ros.h>
#include <XmlRpcValue.h>
#include <geometry_msgs/Twist.h>
#include <sensor_msgs/Joy.h>
#include <sensor_msgs/JointState.h>
#include <std_msgs/Float64MultiArray.h>
#include <sensor_msgs/JoyFeedbackArray.h>

#include <actionlib/client/simple_action_client.h>
#include <cob_script_server/ScriptAction.h>
#include <string>
#include <vector>
#include <std_srvs/Trigger.h>

const int PUBLISH_FREQ = 100.0;

class CobTeleop
{
public:

  struct component_config{
    std::string key;
    std::string twist_topic_name;
    std::string vel_group_topic_name;
    std::string default_target;
    std::vector<double> joint_vel;
    ros::Publisher vel_group_controller_publisher_;
    ros::Publisher twist_controller_publisher_;
    std::vector<double> max_vel; //max_vx_,max_vy_,max_vth_;
    //std::vector<double> max_acc; //max_ax_,max_ay_,max_ath_???????;
  };

  std::map<std::string,component_config> component_config_; //std::vector<std::string> components_names;

	//axis
  int axis_vx_,axis_vy_,axis_vth_;

  //Sensorring not implemented
	//buttons
	//mode 1: Base
  int run_button_;
	//mode 2: Trajectory controller (to default target position using sss.move)
  int torso_default_target_button_;
  int sensorring_default_target_button_;
  int head_default_target_button_;
  int arm_left_default_target_button_;
  int arm_right_default_target_button_;
  int gripper_left_default_target_button_;
  int gripper_right_default_target_button_;
	//mode 3: Velocity group controller
  int torso_joint23_button_;
  int sensorring_joint1_button_;
  int head_joint23_button_;
  int arm_right_joint12_button_;
  int arm_right_joint34_button_;
  int arm_right_joint56_button_;
  int arm_right_joint7_button_;
  int arm_left_joint12_button_;
  int arm_left_joint34_button_;
  int arm_left_joint56_button_;
  int arm_left_joint7_button_;
  int right_indicator_button_;
  int left_indicator_button_;
  int up_down_button_;
  int right_left_button_;
	//mode 4: Twist controller
  int torso_twist_button_;
  int head_twist_button_;
  int arm_left_twist_button_;
  int arm_right_twist_button_;
  
	//common
  int deadman_button_;
  int safety_button_;
  int init_button_;
  bool joy_active_, stopped_, base_safety_;
  double run_factor_, run_factor_param_;
  int joy_num_buttons_;
  int joy_num_axes_;
  int joy_num_modes_;
  int mode_switch_button_;
  int mode_;
  XmlRpc::XmlRpcValue LEDS_;
  XmlRpc::XmlRpcValue led_mode_;
  sensor_msgs::JoyFeedbackArray joyfb;

  ros::NodeHandle n_;
  ros::Subscriber joy_sub_;  //subscribe topic joy
  ros::Subscriber joint_states_sub_;  //subscribe topic joint_states

  double time_for_init_;
  typedef actionlib::SimpleActionClient<cob_script_server::ScriptAction> Client_;
  cob_script_server::ScriptGoal sss_;

  void getConfigurationFromParameters();
  void init();
  void joy_cb(const sensor_msgs::Joy::ConstPtr &joy_msg);
  sensor_msgs::JoyFeedbackArray switch_mode();

};

void CobTeleop::getConfigurationFromParameters()
{
  if(n_.hasParam("components"))
  {
    XmlRpc::XmlRpcValue components;
    ROS_DEBUG("components found ");
    n_.getParam("components", components);
    if(components.getType() == XmlRpc::XmlRpcValue::TypeStruct)
    {
      ROS_DEBUG("components are of type struct with size %d",(int)components.size());
      for(std::map<std::string,XmlRpc::XmlRpcValue>::iterator p=components.begin();p!=components.end();++p)
      {
        std::string comp_name = p->first;
        ROS_DEBUG("component name: %s",comp_name.c_str());
        XmlRpc::XmlRpcValue comp_struc = p->second;
        if(comp_struc.getType() != XmlRpc::XmlRpcValue::TypeStruct)
          ROS_WARN("invalid component, name: %s",comp_name.c_str());
        component_config tempComponent;
        for(std::map<std::string,XmlRpc::XmlRpcValue>::iterator ps=comp_struc.begin();ps!=comp_struc.end();++ps)
        {
          std::string par_name = ps->first;
          ROS_DEBUG("par name: %s",par_name.c_str());
          if(par_name.compare("twist_topic_name")==0)
          {
            ROS_DEBUG("twist topic name found");
            XmlRpc::XmlRpcValue twist_topic_name = ps->second;
            ROS_ASSERT(twist_topic_name.getType() == XmlRpc::XmlRpcValue::TypeString);
            std::string s((std::string)twist_topic_name);
            ROS_DEBUG("twist_topic_name found = %s",s.c_str());
            tempComponent.twist_topic_name = s;
            tempComponent.twist_controller_publisher_ = n_.advertise<geometry_msgs::Twist>((s),1);
          }
          else if(par_name.compare("topic_name")==0)
          {
            ROS_DEBUG("topic name found");
            XmlRpc::XmlRpcValue vel_group_topic_name = ps->second;
            ROS_ASSERT(vel_group_topic_name.getType() == XmlRpc::XmlRpcValue::TypeString);
            std::string s((std::string)vel_group_topic_name);
            ROS_DEBUG("topic_name found = %s",s.c_str());
            tempComponent.vel_group_topic_name = s;
            tempComponent.vel_group_controller_publisher_ = n_.advertise<std_msgs::Float64MultiArray>((s),1);
          }
          else if(par_name.compare("default_target")==0)
          {
            ROS_DEBUG("default target position found");
            XmlRpc::XmlRpcValue default_target = ps->second;
            ROS_ASSERT(default_target.getType() == XmlRpc::XmlRpcValue::TypeString);
            std::string s((std::string)default_target);
            ROS_DEBUG("default_target found = %s",s.c_str());
            tempComponent.default_target = s;
          }
          else if(par_name.compare("joint_vel")==0){
            ROS_DEBUG("joint vels found");
            XmlRpc::XmlRpcValue joint_vel = ps->second;
            ROS_ASSERT(joint_vel.getType() == XmlRpc::XmlRpcValue::TypeArray);
            ROS_DEBUG("joint_vel.size: %d \n", joint_vel.size());
            for(int i=0;i<joint_vel.size();i++)
            {
              ROS_ASSERT(joint_vel[i].getType() == XmlRpc::XmlRpcValue::TypeDouble);
              double vel((double)joint_vel[i]);
              ROS_DEBUG("joint_vel found = %f",vel);
              tempComponent.joint_vel.push_back(vel);
            }
          }else if(par_name.compare("max_velocity")==0){
            ROS_DEBUG("max Velocity found");
            XmlRpc::XmlRpcValue max_velocity = ps->second;
            ROS_ASSERT(max_velocity.getType() == XmlRpc::XmlRpcValue::TypeArray);
            ROS_DEBUG("max_velocity.size: %d \n", max_velocity.size());
            for(int i=0;i<max_velocity.size();i++)
            {
              ROS_ASSERT(max_velocity[i].getType() == XmlRpc::XmlRpcValue::TypeDouble);
              double vel((double)max_velocity[i]);
              ROS_DEBUG("max_velocity found = %f",vel);
              tempComponent.max_vel.push_back(vel);
            }
          }
      }
        ROS_DEBUG("%s module stored",comp_name.c_str());
        component_config_.insert(std::pair<std::string,component_config>(comp_name,tempComponent));
      }
    }
  }
}
sensor_msgs::JoyFeedbackArray CobTeleop::switch_mode(){

  ++mode_;
  if (mode_ > joy_num_modes_)
  {
    mode_ = 1;
  }

  ROS_INFO("Mode switched to: %d",mode_);
  LEDS_=led_mode_[mode_];
  for (int i=0; i<4; i++)
  {
      joyfb.array.resize(4);
      joyfb.array[i].type=0;
      joyfb.array[i].id=i;
      joyfb.array[i].intensity=static_cast<int>(LEDS_[i]);
  }
  return joyfb;
}

void CobTeleop::joy_cb(const sensor_msgs::Joy::ConstPtr &joy_msg){
  getConfigurationFromParameters();

  if(mode_switch_button_>=0 && mode_switch_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[mode_switch_button_]==1)
  {
    ROS_INFO("Switch mode button pressed");
    switch_mode();
  }

  if(deadman_button_>=0 && deadman_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[deadman_button_]==1)
  {
    if (!joy_active_)
    {
      ROS_INFO("joystick is active");
      joy_active_ = true;
    }
  }else
  {
    ROS_DEBUG("joystick is not active");
    joy_active_ = false;
    return;
  }

  if(run_button_>=0 && run_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[run_button_]==1)
  {
    run_factor_ = run_factor_param_;
  }
  else //button release
  {
    run_factor_ = 1.0;
  }
	
  Client_ * client;
  client = new Client_("script_server", true);
  ROS_INFO("Connecting to script_server");
  client->waitForServer();
  ROS_INFO("Connected");

  if(init_button_>=0 && init_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[init_button_]==1)
  {
    ROS_INFO("Init button pressed");
    XmlRpc::XmlRpcValue components;
    n_.getParam("components", components);

    for(std::map<std::string,XmlRpc::XmlRpcValue>::iterator p=components.begin();p!=components.end();++p)
    {
      std::string comp_name = p->first;
      sss_.component_name = comp_name.c_str();
      sss_.function_name="init";
      client->sendGoal(sss_);
      ROS_INFO("Init %s",comp_name.c_str());
    }
  }

//-------MODE 1
  if (mode_==1){
    ROS_INFO("Mode 1: Move the base using twist controller");
    geometry_msgs::Twist base_cmd;
    if(axis_vx_>=0 && axis_vx_<(int)joy_msg->axes.size()){
      base_cmd.linear.x = joy_msg->axes[axis_vx_]*component_config_["base"].max_vel[0]*run_factor_;
    }else
      base_cmd.linear.x =0.0;
    if(axis_vy_>=0 && axis_vy_<(int)joy_msg->axes.size())
      base_cmd.linear.y = joy_msg->axes[axis_vy_]*component_config_["base"].max_vel[1]*run_factor_;
    else
      base_cmd.linear.y =0.0;
    if(axis_vth_>=0 && axis_vth_<(int)joy_msg->axes.size())
      base_cmd.angular.z = joy_msg->axes[axis_vth_]*component_config_["base"].max_vel[2]*run_factor_;
    else
      base_cmd.angular.z =0.0;
    component_config_["base"].twist_controller_publisher_.publish(base_cmd);
  }

//-------MODE 2
  if (mode_==2){
    ROS_INFO("Mode 2: Move the actuators to a default position (Trajectory controller)");
    XmlRpc::XmlRpcValue components;
    n_.getParam("components", components);
    for(std::map<std::string,XmlRpc::XmlRpcValue>::iterator p=components.begin();p!=components.end();++p)
    {
      int component_default_target_button_temp;
      std::string comp_name = p->first;
      std::string component_default_target_button = comp_name + "_default_target_button";
      n_.getParam(component_default_target_button,component_default_target_button_temp);

      if(component_default_target_button_temp>=0 && component_default_target_button_temp<(int)joy_msg->buttons.size() && joy_msg->buttons[component_default_target_button_temp]==1){
        sss_.component_name = comp_name;
        sss_.function_name = "move";
        sss_.parameter_name = component_config_[comp_name].default_target.c_str();
        client->sendGoal(sss_);
        ROS_INFO("Move %s to %s",comp_name.c_str(), component_config_[comp_name].default_target.c_str()); 
      }
    }
  }


   if (mode_==3){
     ROS_INFO("Mode 3: Move the actuators using the group velocity controller");
    XmlRpc::XmlRpcValue components;
    n_.getParam("components", components);

    for(std::map<std::string,XmlRpc::XmlRpcValue>::iterator p=components.begin();p!=components.end();++p)
    {
      int component_joint_button_temp;
      std::string comp_name = p->first;
      for(int i=1; i <= ceil((component_config_[comp_name].joint_vel.size())/2); ++i){
        std::ostringstream component_joint_button_stream;
        component_joint_button_stream << comp_name << "_joint" << i << "_button";
        std::string component_joint_button = component_joint_button_stream.str();
        n_.getParam(component_joint_button,component_joint_button_temp);
        std_msgs::Float64MultiArray vel_cmd;

       if(!(comp_name.find("left") != std::string::npos) && !(comp_name.find("right") != std::string::npos)){
         ROS_INFO("torso, head");
         if(component_joint_button_temp>=0 && component_joint_button_temp<(int)joy_msg->buttons.size() && joy_msg->buttons[component_joint_button_temp]==1 && joy_msg->buttons[right_indicator_button_]==0 && joy_msg->buttons[left_indicator_button_]==0){
           ROS_INFO("%s velocity mode",comp_name.c_str());
           vel_cmd.data.resize(component_config_["comp_name"].joint_vel.size()); 
           if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
             vel_cmd.data[i-1]=component_config_["comp_name"].joint_vel[i-1];
           }
           else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
             vel_cmd.data[i-1]=-component_config_["comp_name"].joint_vel[i-1];
           }
           else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
             vel_cmd.data[i]=component_config_["comp_name"].joint_vel[i];
           }
           else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
             vel_cmd.data[i]=-component_config_["comp_name"].joint_vel[i];
           }
           component_config_["comp_name"].vel_group_controller_publisher_.publish(vel_cmd);
        }
      }
      else if(!(comp_name.find("left") != std::string::npos) && (comp_name.find("right") != std::string::npos)){
         ROS_INFO("right");
         if(component_joint_button_temp>=0 && component_joint_button_temp<(int)joy_msg->buttons.size() && joy_msg->buttons[component_joint_button_temp]==1 && joy_msg->buttons[right_indicator_button_]==1 && joy_msg->buttons[left_indicator_button_]==0){
           ROS_INFO("%s velocity mode",comp_name.c_str());
           vel_cmd.data.resize(component_config_["comp_name"].joint_vel.size()); 
           if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
             vel_cmd.data[i-1]=component_config_["comp_name"].joint_vel[i-1];
           }
           else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
             vel_cmd.data[i-1]=-component_config_["comp_name"].joint_vel[i-1];
           }
           else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
             vel_cmd.data[i]=component_config_["comp_name"].joint_vel[i];
           }
           else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
             vel_cmd.data[i]=-component_config_["comp_name"].joint_vel[i];
           }
           component_config_["comp_name"].vel_group_controller_publisher_.publish(vel_cmd);
        }
      }
         
      else if((comp_name.find("left") != std::string::npos) && !(comp_name.find("right") != std::string::npos)){
         ROS_INFO("left");
         if(component_joint_button_temp>=0 && component_joint_button_temp<(int)joy_msg->buttons.size() && joy_msg->buttons[component_joint_button_temp]==1 && joy_msg->buttons[right_indicator_button_]==0 && joy_msg->buttons[left_indicator_button_]==1){
           ROS_INFO("%s velocity mode",comp_name.c_str());
           vel_cmd.data.resize(component_config_["comp_name"].joint_vel.size()); 
           if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
             vel_cmd.data[i-1]=component_config_["comp_name"].joint_vel[i-1];
           }
           else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
             vel_cmd.data[i-1]=-component_config_["comp_name"].joint_vel[i-1];
           }
           else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
             vel_cmd.data[i]=component_config_["comp_name"].joint_vel[i];
           }
           else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
             vel_cmd.data[i]=-component_config_["comp_name"].joint_vel[i];
           }
           component_config_["comp_name"].vel_group_controller_publisher_.publish(vel_cmd);
        }
      }
      
    }
  }
}
//-------MODE 3
//   if (mode_==3){
//     ROS_INFO("Mode 3: Move the actuators using the group velocity controller");
//     std_msgs::Float64MultiArray vel_cmd;

// //TORSO
//     if(torso_joint23_button_>=0 && torso_joint23_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[torso_joint23_button_]==1 && joy_msg->buttons[right_indicator_button_]==0 && joy_msg->buttons[left_indicator_button_]==0){
//       ROS_INFO("torso velocity mode");
//       vel_cmd.data.resize(component_config_["torso"].joint_vel.size()); 
//       if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
//         vel_cmd.data[0]=component_config_["torso"].joint_vel[0];
//       }
//       else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
//         vel_cmd.data[0]=-component_config_["torso"].joint_vel[0];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
//         vel_cmd.data[1]=component_config_["torso"].joint_vel[1];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
//         vel_cmd.data[1]=-component_config_["torso"].joint_vel[1];
//       }
//       component_config_["torso"].vel_group_controller_publisher_.publish(vel_cmd);
//     }

// //SENSORRING
//     if(sensorring_joint1_button_>=0 && sensorring_joint1_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[sensorring_joint1_button_]==1 && joy_msg->buttons[right_indicator_button_]==0 && joy_msg->buttons[left_indicator_button_]==0){
//       ROS_INFO("sensorring velocity mode");
//       vel_cmd.data.resize(component_config_["sensorring"].joint_vel.size()); 
//       if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
//         vel_cmd.data[0]=component_config_["sensorring"].joint_vel[1];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
//         vel_cmd.data[0]=-component_config_["sensorring"].joint_vel[1];
//       }
//       component_config_["sensorring"].vel_group_controller_publisher_.publish(vel_cmd);
//     }

// //HEAD
//     if(head_joint23_button_>=0 && head_joint23_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[head_joint23_button_]==1 && joy_msg->buttons[right_indicator_button_]==0 && joy_msg->buttons[left_indicator_button_]==0){
//       ROS_INFO("head velocity mode");
//       vel_cmd.data.resize(component_config_["head"].joint_vel.size());
//       if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
//         vel_cmd.data[0]=component_config_["head"].joint_vel[0];;
//       }
//       else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
//         vel_cmd.data[0]=-component_config_["head"].joint_vel[0];;
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
//         vel_cmd.data[1]=component_config_["head"].joint_vel[1];;
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
//         vel_cmd.data[1]=-component_config_["torso"].joint_vel[1];;
//       }
//       component_config_["head"].vel_group_controller_publisher_.publish(vel_cmd);
//     }

// //ARM_RIGHT
//     if(arm_right_joint12_button_>=0 && arm_right_joint12_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[arm_right_joint12_button_]==1 && joy_msg->buttons[right_indicator_button_]==1 && joy_msg->buttons[left_indicator_button_]==0){
//       ROS_INFO("arm_right velocity mode");
//       vel_cmd.data.resize(component_config_["arm_right"].joint_vel.size());
//       if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
//         vel_cmd.data[0]=component_config_["arm_right"].joint_vel[0];
//       }
//       else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
//         vel_cmd.data[0]=-component_config_["arm_right"].joint_vel[0];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
//         vel_cmd.data[1]=component_config_["arm_right"].joint_vel[1];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
//         vel_cmd.data[1]=-component_config_["arm_right"].joint_vel[1];
//       }
//     component_config_["arm_right"].vel_group_controller_publisher_.publish(vel_cmd);
//     }else if (arm_right_joint34_button_>=0 && arm_right_joint34_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[arm_right_joint34_button_]==1 && joy_msg->buttons[right_indicator_button_]==1 && joy_msg->buttons[left_indicator_button_]==0){
//       ROS_INFO("arm_right velocity mode");
//       vel_cmd.data.resize(component_config_["arm_right"].joint_vel.size());
//       if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
//         vel_cmd.data[2]=component_config_["arm_right"].joint_vel[2];
//       }
//       else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
//         vel_cmd.data[2]=-component_config_["arm_right"].joint_vel[2];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
//         vel_cmd.data[3]=component_config_["arm_right"].joint_vel[3];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
//         vel_cmd.data[3]=-component_config_["arm_right"].joint_vel[3];
//       }
//       component_config_["arm_right"].vel_group_controller_publisher_.publish(vel_cmd);
//     }else if (arm_right_joint56_button_>=0 && arm_right_joint56_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[arm_right_joint56_button_]==1 && joy_msg->buttons[right_indicator_button_]==1 && joy_msg->buttons[left_indicator_button_]==0){
//       ROS_INFO("arm_right velocity mode");
//       vel_cmd.data.resize(component_config_["arm_right"].joint_vel.size());
//       if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
//         vel_cmd.data[4]=component_config_["arm_right"].joint_vel[4];
//       }
//       else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
//         vel_cmd.data[4]=-component_config_["arm_right"].joint_vel[4];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
//         vel_cmd.data[5]=component_config_["arm_right"].joint_vel[5];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
//         vel_cmd.data[5]=-component_config_["arm_right"].joint_vel[5];
//       }
//       component_config_["arm_right"].vel_group_controller_publisher_.publish(vel_cmd);
//     }else if (arm_right_joint7_button_>=0 && arm_right_joint7_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[arm_right_joint7_button_]==1 && joy_msg->buttons[right_indicator_button_]==1 && joy_msg->buttons[left_indicator_button_]==0){
//       ROS_INFO("arm_right velocity mode");
//       vel_cmd.data.resize(component_config_["arm_right"].joint_vel.size());
//       if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
//         vel_cmd.data[6]=component_config_["arm_right"].joint_vel[6];
//       }
//       else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
//         vel_cmd.data[6]=-component_config_["arm_right"].joint_vel[6];
//       }
//       component_config_["arm_right"].vel_group_controller_publisher_.publish(vel_cmd);
//     }
//     if(arm_left_joint12_button_>=0 && arm_left_joint12_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[arm_left_joint12_button_]==1 && joy_msg->buttons[right_indicator_button_]==0 && joy_msg->buttons[left_indicator_button_]==1){
//       ROS_INFO("arm_left velocity mode");
//       vel_cmd.data.resize(component_config_["arm_left"].joint_vel.size());
//       if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
//         vel_cmd.data[0]=component_config_["arm_left"].joint_vel[0];
//       }
//       else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
//         vel_cmd.data[0]=-component_config_["arm_left"].joint_vel[0];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
//         vel_cmd.data[1]=component_config_["arm_left"].joint_vel[1];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
//         vel_cmd.data[1]=-component_config_["arm_left"].joint_vel[1];
//       }
// //ARM_LEFT
//       component_config_["arm_left"].vel_group_controller_publisher_.publish(vel_cmd);
//     }else if (arm_left_joint34_button_>=0 && arm_left_joint34_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[arm_left_joint34_button_]==1 && joy_msg->buttons[right_indicator_button_]==0 && joy_msg->buttons[left_indicator_button_]==1){
//       ROS_INFO("arm_left velocity mode");
//       vel_cmd.data.resize(component_config_["arm_left"].joint_vel.size());
//       if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
//         vel_cmd.data[2]=component_config_["arm_left"].joint_vel[2];
//       }
//       else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
//         vel_cmd.data[2]=-component_config_["arm_left"].joint_vel[2];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
//         vel_cmd.data[3]=component_config_["arm_left"].joint_vel[3];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
//         vel_cmd.data[3]=-component_config_["arm_left"].joint_vel[3];
//       }
//       component_config_["arm_left"].vel_group_controller_publisher_.publish(vel_cmd);
//     }else if (arm_left_joint56_button_>=0 && arm_left_joint56_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[arm_left_joint56_button_]==1 && joy_msg->buttons[right_indicator_button_]==0 && joy_msg->buttons[left_indicator_button_]==1){
//       ROS_INFO("arm_left velocity mode");
//       vel_cmd.data.resize(component_config_["arm_left"].joint_vel.size());
//       if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
//         vel_cmd.data[4]=component_config_["arm_left"].joint_vel[4];
//       }
//       else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
//         vel_cmd.data[4]=-component_config_["arm_left"].joint_vel[4];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]<0.0){
//         vel_cmd.data[5]=component_config_["arm_left"].joint_vel[5];
//       }
//       else if(right_left_button_>=0 && right_left_button_<(int)joy_msg->axes.size() && joy_msg->axes[right_left_button_]>0.0){
//         vel_cmd.data[5]=-component_config_["arm_left"].joint_vel[5];
//       }
//       component_config_["arm_left"].vel_group_controller_publisher_.publish(vel_cmd);
//     }else if (arm_left_joint7_button_>=0 && arm_left_joint7_button_<(int)joy_msg->buttons.size() && joy_msg->buttons[arm_left_joint7_button_]==1 && joy_msg->buttons[right_indicator_button_]==0 && joy_msg->buttons[left_indicator_button_]==1){
//       ROS_INFO("arm_left velocity mode");
//       vel_cmd.data.resize(component_config_["arm_left"].joint_vel.size());
//       if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]<0.0){
//         vel_cmd.data[6]=component_config_["arm_left"].joint_vel[6];
//       }
//       else if(up_down_button_>=0 && up_down_button_<(int)joy_msg->axes.size() && joy_msg->axes[up_down_button_]>0.0){
//         vel_cmd.data[6]=-component_config_["arm_left"].joint_vel[6];
//       }
//       component_config_["arm_left"].vel_group_controller_publisher_.publish(vel_cmd);
//     }
//   }
  
//-------MODE 4
  if (mode_==4){
    ROS_INFO("Mode 4: Move the actuators using the twist controller");
    XmlRpc::XmlRpcValue components;
    n_.getParam("components", components);
    for(std::map<std::string,XmlRpc::XmlRpcValue>::iterator p=components.begin();p!=components.end();++p)
    {
      int component_twist_button_temp;
      std::string comp_name = p->first;
      std::string component_twist_button = comp_name + "_twist_button";
      n_.getParam(component_twist_button,component_twist_button_temp);
      //ToDo: we can command 6 axis linear.x linear.y linear.z angular.x angular.y angular.z
      geometry_msgs::Twist twist_cmd;
      if (component_twist_button_temp>=0 && component_twist_button_temp<(int)joy_msg->buttons.size() && joy_msg->buttons[component_twist_button_temp]==1){
        ROS_INFO("%s twist mode",comp_name.c_str());
        if(axis_vx_>=0 && axis_vx_<(int)joy_msg->axes.size())
          twist_cmd.linear.x = joy_msg->axes[axis_vx_]*component_config_[comp_name].max_vel[0]; //*run_factor_;
        else
          twist_cmd.linear.x =0.0;
        if(axis_vy_>=0 && axis_vy_<(int)joy_msg->axes.size())
          twist_cmd.linear.y = joy_msg->axes[axis_vy_]*component_config_[comp_name].max_vel[1]; //*run_factor_;
        else
          twist_cmd.linear.y =0.0;
        if(axis_vth_>=0 && axis_vth_<(int)joy_msg->axes.size())
          twist_cmd.angular.z = joy_msg->axes[axis_vth_]*component_config_[comp_name].max_vel[2]; //*run_factor_;
        else
          twist_cmd.angular.z =0.0;
        component_config_[comp_name].twist_controller_publisher_.publish(twist_cmd);
      }
    }
  }
}

/*!
 * \brief Initializes node to get parameters, subscribe to topics.
 */
void CobTeleop::init()
{
	// common
  n_.param("run_factor",run_factor_param_,1.5);

	// joy config
  n_.param("joy_num_buttons",joy_num_buttons_,12);
  n_.param("joy_num_axes",joy_num_axes_,6);
  n_.param("joy_num_modes",joy_num_modes_,2);
  n_.param("mode_switch_button",mode_switch_button_,0);

	// assign axis
  n_.param("axis_vx",axis_vx_,17);
  n_.param("axis_vy",axis_vy_,16);
  n_.param("axis_vth",axis_vth_,19);

	// assign buttons
  n_.param("deadman_button",deadman_button_,11);
  n_.param("safety_button",safety_button_,10);
  n_.param("init_button",init_button_,3);

  n_.param("run_button",run_button_,9);

  n_.param("torso_default_target_button",torso_default_target_button_,6);
  n_.param("sensorring_default_target_button",sensorring_default_target_button_,12);
  n_.param("head_default_target_button",head_default_target_button_,4);
  n_.param("arm_left_default_target_button",arm_left_default_target_button_,7);
  n_.param("arm_right_default_target_button",arm_right_default_target_button_,5);
  n_.param("gripper_left_default_target_button",gripper_left_default_target_button_,15);
  n_.param("gripper_right_default_target_button",gripper_right_default_target_button_,13);

  n_.param("torso_joint23_button",torso_joint23_button_,15);
  n_.param("sensorring_joint1_button",sensorring_joint1_button_,14);
  n_.param("head_joint23_button",head_joint23_button_,13);
  n_.param("arm_right_joint12_button",arm_right_joint12_button_,15);
  n_.param("arm_right_joint34_button",arm_right_joint34_button_,14);
  n_.param("arm_right_joint56_button",arm_right_joint56_button_,13);
  n_.param("arm_right_joint7_button",arm_right_joint7_button_,12);
  n_.param("arm_left_joint12_button",arm_left_joint12_button_,15);
  n_.param("arm_left_joint34_button",arm_left_joint34_button_,14);
  n_.param("arm_left_joint56_button",arm_left_joint56_button_,13);
  n_.param("arm_left_joint7_button",arm_left_joint7_button_,12);

  n_.param("right_indicator_button",right_indicator_button_,9);
  n_.param("left_indicator_button",left_indicator_button_,8);
  n_.param("up_down_button",up_down_button_,4);
  n_.param("right_left_button",right_left_button_,5);

  n_.param("torso_twist_button",torso_twist_button_,6);
  n_.param("head_twist_button",head_twist_button_,4);
  n_.param("arm_left_twist_button",arm_left_twist_button_,7);
  n_.param("arm_right_twist_button",arm_right_twist_button_,5);

	// output for debugging
  ROS_DEBUG("init::axis_vx: %d",axis_vx_);
  ROS_DEBUG("init::axis_vy: %d",axis_vy_);
  ROS_DEBUG("init::axis_vth: %d",axis_vth_);

  ROS_DEBUG("init::deadman_button: %d",deadman_button_);
  ROS_DEBUG("init::safety_button: %d",safety_button_);
  ROS_DEBUG("init::init_button: %d",init_button_);
  ROS_DEBUG("init::run_button: %d",run_button_);

  ROS_DEBUG("init::torso_default_target_button: %d",torso_default_target_button_);
  ROS_DEBUG("init::sensorring_default_target_button: %d",sensorring_default_target_button_);
  ROS_DEBUG("init::head_default_target_button: %d",head_default_target_button_);
  ROS_DEBUG("init::arm_left_default_target_button: %d",arm_left_default_target_button_);
  ROS_DEBUG("init::arm_right_default_target_button: %d",arm_right_default_target_button_);
  ROS_DEBUG("init::gripper_left_default_target_button: %d",gripper_left_default_target_button_);
  ROS_DEBUG("init::gripper_right_default_target_button: %d",gripper_right_default_target_button_);

  ROS_DEBUG("init::torso_joint23_button: %d",torso_joint23_button_);
  ROS_DEBUG("init::sensorring_joint1_button: %d",sensorring_joint1_button_);
  ROS_DEBUG("init::head_joint23_button: %d",head_joint23_button_);
  ROS_DEBUG("init::arm_right_joint12_button: %d",arm_right_joint12_button_);
  ROS_DEBUG("init::arm_right_joint34_button: %d",arm_right_joint34_button_);
  ROS_DEBUG("init::arm_right_joint56_button: %d",arm_right_joint56_button_);
  ROS_DEBUG("init::arm_right_joint7_button: %d",arm_right_joint7_button_);
  ROS_DEBUG("init::arm_left_joint12_button: %d",arm_left_joint12_button_);
  ROS_DEBUG("init::arm_left_joint34_button: %d",arm_left_joint34_button_);
  ROS_DEBUG("init::arm_left_joint56_button: %d",arm_left_joint56_button_);
  ROS_DEBUG("init::arm_left_joint7_button: %d",arm_left_joint7_button_);

  ROS_DEBUG("init::right_indicator_button: %d",right_indicator_button_);
  ROS_DEBUG("init::left_indicator_button: %d",left_indicator_button_);
  ROS_DEBUG("init::up_down_button: %d",up_down_button_);
  ROS_DEBUG("init::right_left_button: %d",right_left_button_);

  joy_sub_ = n_.subscribe("/joy",1,&CobTeleop::joy_cb,this);
  mode_ = 1;
  LEDS_=led_mode_[mode_];
}


int main(int argc, char** argv)
{
  ros::init(argc, argv, "cob_teleop");
  CobTeleop  cob_teleop;
  cob_teleop.init();

  ros::Rate loop_rate(PUBLISH_FREQ); //Hz
  while(cob_teleop.n_.ok())
  {
    ros::spinOnce();
    loop_rate.sleep();
  }

  exit(0);
  return(0);
}


