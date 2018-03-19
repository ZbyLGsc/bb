/*********************************************************************
 *
 *  Software License Agreement (BSD License)
 *
 *  Copyright (c) 2017, Markus Lamprecht,
 *                      Technische Universität Darmstadt
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Technische Universität Darmstadt nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *********************************************************************/

#include <bb_control/bb_control_node.hpp>

namespace ballbot
{
  //helper functions:
  double ControlNode::convertDistanceToAngle(double dis)
  {
     // std::cout<<dis<<" m"<<std::endl;
      double a =  (double) 360/(2*3.14159265359*0.06) *dis;

      //std::cout<<a<<" grad"<<std::endl;
      double b = (double) a/180 * 3.14159265359;
      //stdcout<<b<<" rad"<<std::endl;


      int c = a/360;
      double d = a-(c*360);

      //std::cout<<d<<" grad angepasst"<<std::endl;

      double e = (double) d/180 * 3.14159265359;
      std::cout<<e<<" rad angepasst"<<std::endl;

      // return valueebetwenn 0 and 2 pi!
      return e;
  }

  std::vector<double> ControlNode::convertToAngleVel(std::vector<double> input_vec)
  {
    std::vector<double> output_vec(input_vec.size());
    for(int i=0; i<input_vec.size(); i++)
    {
      output_vec.at(i)=linearVelToAngleVel(input_vec.at(i));
    }
    return output_vec;
  }

  double ControlNode::linearVelToAngleVel(double linear_vel)
  {
    //w=v*r
    double res = (double) linear_vel/(0.06);
    std::cout<<"linear_vel "<<linear_vel<<std::endl;
    std::cout<<"w= "<<res<<std::endl;
    return res;
  }

  std::vector<double> ControlNode::convertVectorToAngle(std::vector<double> input_vec)
  {
    std::vector<double> output_vec(input_vec.size());
    for(int i=0;i<input_vec.size();i++)
    {
      output_vec.at(i)=convertDistanceToAngle(input_vec.at(i));
    }
    return output_vec;
  }

  std::vector<double> ControlNode::toEulerAngle(double x, double y, double z, double w)
  {
    // roll (x-axis rotation)
    double sinr = +2.0 * (w * x + y * z);
    double cosr = +1.0 - 2.0 * (x * x + y * y);
    double roll = atan2(sinr, cosr);

    // pitch (y-axis rotation)
    double pitch = 0.0;
    double sinp = +2.0 * (w * y - z * x);
    if (fabs(sinp) >= 1)
      pitch = copysign(M_PI / 2, sinp); // use 90 degrees if out of range
    else
      pitch = asin(sinp);

    // yaw (z-axis rotation)
    double siny = +2.0 * (w * z + x * y);
    double cosy = +1.0 - 2.0 * (y * y + z * z);
    double yaw = atan2(siny, cosy);
    return {roll, pitch, yaw};
  }

  // this node is updated every 10ms.
  // this calculation takes less than 1ms.
  void ControlNode::calc2MotorCommands_withoutOdometry()
  {
    double Tx = -1.0*(gains_2D_Kxz_[1]*imu_phi_[0]+gains_2D_Kxz_[3]*imu_dphi_[0]);

    double Ty = -1.0*(gains_2D_Kyz_[1]*imu_phi_[1]+gains_2D_Kyz_[3]*imu_dphi_[1]);

    double Tz = 1.0;

    Ty=-Ty;
    double T1=(2*cb_)/(3*ca_)*Tx+(2*sb_)/(3*ca_)*Ty+1/(3*sa_)*Tz;
    double T2=-(cb_+sqrt(3)*sb_)/(3*ca_)*Tx+(-sb_+sqrt(3)*cb_)/(3*ca_)*Ty+1/(3*sa_)*Tz;
    double T3=(-cb_+sqrt(3)*sb_)/(3*ca_)*Tx-(sb_+sqrt(3)*cb_)/(3*ca_)*Ty+1/(3*sa_)*Tz;
    // 6. Tx,Ty, Tz -> T1, T2, T3 (calculate real motor torques): (P. 14)
//    double T1=0.33333333*(Tz+2.0/(cos(alpha_))*(Tx*cos(beta_)-Ty*sin(beta_)) );
//    double T2=0.33333333*(Tz+1.0/(cos(alpha_)) *(sin(beta_)*(-sqrt(3.0)*Tx+Ty) - cos(beta_)*(Tx+sqrt(3.0)*Ty)) ) ;
//    double T3=0.33333333*(Tz+1.0/(cos(alpha_)) *(sin(beta_)*(sqrt(3.0)*Tx+Ty) + cos(beta_)*(-Tx+sqrt(3.0)*Ty)) ) ;

    realT_ = {-T1, -T2, -T3}; // wheels turn the other way round!
  }


  // this node is updated every 10ms.
  // this calculation takes less than 1ms.
  void ControlNode::calc2MotorCommands_withOdometry()
  {
  // See Page 89 of ETHZ Script.
    double phi_x_dot = (2*cos(imu_phi_[1])*((6*imu_dphi_[0])/25 + (3*pow(2,0.5)*(joints_velocity_[1] - 2*joints_velocity_[0] + joints_velocity_[2]))/100))/75 + (pow(2,0.5)*cos(imu_phi_[0])*sin(imu_phi_[1])*(joints_velocity_[0] + joints_velocity_[1] + joints_velocity_[2]))/1250 - (pow(2,0.5)*pow(3,0.5)*sin(imu_phi_[0])*sin(imu_phi_[1])*(joints_velocity_[1] - joints_velocity_[2]))/1250;

    double phi_y_dot = -1*(4*imu_dphi_[1])/625 - (pow(2,0.5)*sin(imu_phi_[0])*(joints_velocity_[0] + joints_velocity_[1] + joints_velocity_[2]))/1250 - (pow(2,0.5)*pow(3,0.5)*cos(imu_phi_[0])*(joints_velocity_[1] - joints_velocity_[2]))/1250;

    //angular velocity of ball in rad/sec.
    geometry_msgs::PointStamped ball_odom;
    ball_odom.header.stamp = ros::Time::now();
    ball_odom.header.frame_id = "ball_odom_phi_x,y_dot";
    ball_odom.point.x = phi_x_dot*180/3.14159265359;
    ball_odom.point.y = phi_y_dot*180/3.14159265359;
    ball_odom.point.z = 0.0;
    calc_ball_odom_pub_.publish(ball_odom);

    double Tx = -1.0*(gains_2D_Kxz_[1]*imu_phi_[0]+gains_2D_Kxz_[3]*imu_dphi_[0]+gains_2D_Kxz_[2]*phi_x_dot);

    double Ty = -1.0*(gains_2D_Kyz_[1]*imu_phi_[1]+gains_2D_Kyz_[3]*imu_dphi_[1]+gains_2D_Kyz_[2]*phi_y_dot);

    double Tz = 0.0;

    Ty=-Ty;
    double T1=(2*cb_)/(3*ca_)*Tx+(2*sb_)/(3*ca_)*Ty+1/(3*sa_)*Tz;
    double T2=-(cb_+sqrt(3)*sb_)/(3*ca_)*Tx+(-sb_+sqrt(3)*cb_)/(3*ca_)*Ty+1/(3*sa_)*Tz;
    double T3=(-cb_+sqrt(3)*sb_)/(3*ca_)*Tx-(sb_+sqrt(3)*cb_)/(3*ca_)*Ty+1/(3*sa_)*Tz;

    realT_ = {-T1, -T2, -T3}; // wheels turn the other way round!
  }

  //2D Control calculation:
  void ControlNode::calc2DMotorCommands()
  {
    // see diagram at ETHZ skript at page 51:
    // input variables:
    // update_time_ (time discrete) [sec]
    // imu orientation  : imu_phi_  [rad]
    // imu angular vel  : imu_dphi_ [rad/sec]
    // motor angular vel: previous_joint_state_msg_.velocity [rad/sec]
    // motor position   : previous_joint_state_msg_.position [rad] TODO Wertebereich auf 0, 2pi beschränken?
    // output variables:
    // motor torques    : realT_ [Nm]

    // 1. Convert dpsi_1,2,3 -> dpsi_x,y,z
    // 1.A dpsix = dpsi1 / cos (alpha) P.71
    // 1.B dpsiy = dpsi3 * 2 / (sqrt(3) *cos(alpha))
    // 1.C dpsiz = dpsi1/sin(alpha)
    std::vector<double> dpsi_ball(3);
    dpsi_ball[0]=previous_joint_state_msg_.velocity[0]/cos(alpha_);
    dpsi_ball[1]=(previous_joint_state_msg_.velocity[2]*2)/( sqrt(3) * cos(alpha_) );
    dpsi_ball[2]=previous_joint_state_msg_.velocity[0]/sin(alpha_);

    // 2. Use dpsi_x,y,z -> psi_x,y,z
    // psi_k = psi_k-1 + dpsi * sample_time
    //std::cout<<"update rate:"<<update_time_<<std::endl;
    //std::cout<<dpsi_ball[0]<<" "<<dpsi_ball[1]<<" "<<dpsi_ball[2]<<std::endl;
    //std::cout<<psi_ball_last_[0]<<" "<<psi_ball_last_[1]<<" "<<psi_ball_last_[2]<<std::endl;

    psi_ball_actual_[0] =psi_ball_last_[0] + update_time_* dpsi_ball[0];
    psi_ball_actual_[1] =psi_ball_last_[1] + update_time_* dpsi_ball[1];
    psi_ball_actual_[2] =psi_ball_last_[2] + update_time_* dpsi_ball[2];

    //std::cout<<psi_ball_actual_[0]<<" "<<psi_ball_actual_[1]<<" "<<psi_ball_actual_[2]<<std::endl;

    // 3. psi_x,y,z -> dphi_x,y,z P. 8
    double rK = 0.07;
    double rW = 0.03;
    std::vector<double> dphi_ball(3);
    dphi_ball[0]=(dpsi_ball[0]+imu_dphi_[0])*rW/rK+imu_dphi_[0];
    dphi_ball[1]=(dpsi_ball[1]+imu_dphi_[1])*rW/rK+imu_dphi_[1];
    dphi_ball[2]=(dpsi_ball[2]/sin(alpha_))*rW/rK+imu_dphi_[2];

    // 4. dphi_x,y,z -> phi_x,y,z (integrate it!)
    phi_ball_actual_[0]=psi_ball_last_[0]+(psi_ball_actual_[0]-psi_ball_last_[0]-imu_phi_last_[0]+imu_phi_[0])*rW/rK+imu_phi_[0]-imu_phi_last_[0];
    phi_ball_actual_[1]=psi_ball_last_[1]+(psi_ball_actual_[1]-psi_ball_last_[1]-imu_phi_last_[1]+imu_phi_[1])*rW/rK+imu_phi_[1]-imu_phi_last_[1];
    phi_ball_actual_[2]=phi_ball_last_[2]+(psi_ball_actual_[2]-psi_ball_last_[2])*rW/(rK*sin(alpha_))+imu_phi_[2]-imu_phi_last_[2];

    // 5. phi_x,y,z -> Tx,Ty,Tz (virtual motor torques) P. Tx = - K [phix, dphix, thetax dthetax]
    double Tx = -(gains_2D_Kxz_[0]*phi_ball_actual_[0]+
                  gains_2D_Kxz_[1]*dphi_ball[0]+
                  gains_2D_Kxz_[2]*imu_phi_[0]+
                  gains_2D_Kxz_[3]*imu_dphi_[0]);

    double Ty = -(gains_2D_Kyz_[0]*phi_ball_actual_[1]+
                  gains_2D_Kyz_[1]*dphi_ball[1]+
                  gains_2D_Kyz_[2]*imu_phi_[1]+
                  gains_2D_Kyz_[3]*imu_dphi_[1]);

    // Tz PD controller: care:
    // Tz = - bo (e_k-1) = - 2.66 (e_k-1)
    double Tz = - Kr_*Tv_/update_time_ * (0 - imu_phi_last_[2]);

    // 6. Tx,Ty, Tz -> T1, T2, T3 (calculate real motor torques):
    // TOOD: check divisions! P. 14
    double T1=0.33333333*(Tz+2.0/(cos(alpha_))*(Tx*cos(beta_)-Ty*sin(beta_)) );
    double T2=0.33333333*(Tz+1.0/(cos(alpha_)) *(sin(beta_)*(-sqrt(3.0)*Tx+Ty) - cos(beta_)*(Tx+sqrt(3.0)*Ty)) ) ;
    double T3=0.33333333*(Tz+1.0/(cos(alpha_)) *(sin(beta_)*(sqrt(3.0)*Tx+Ty) + cos(beta_)*(-Tx+sqrt(3.0)*Ty)) ) ;

    // store old values:
    psi_ball_last_[0] = psi_ball_actual_[0];
    psi_ball_last_[1] = psi_ball_actual_[1];
    psi_ball_last_[2] = psi_ball_actual_[2];

    phi_ball_last_[0] = phi_ball_actual_[0];
    phi_ball_last_[1] = phi_ball_actual_[1];
    phi_ball_last_[2] = phi_ball_actual_[2];

    imu_phi_last_[0] = imu_phi_[0];
    imu_phi_last_[1] = imu_phi_[1];
    imu_phi_last_[2] = imu_phi_[2];

    realT_ =  {T1, T2, T3};
    //return T_motor;
  }

  //constructor is called only once:
  ControlNode::ControlNode(ros::NodeHandle& nh)
  {
    //get Params:
    controller_type_ = nh.param("control_constants/controller_type", std::string("2D"));
    motors_controller_type_ = nh.param("control_constants/motors_controller_type", std::string("VelocityJointInterface"));

    // get 2D control gains:
    gains_2D_Kxz_=nh.param("control_constants/gains_2D_Kxz",(std::vector<double> ) {1.0,2.0,3.0,4.0});
    gains_2D_Kyz_=nh.param("control_constants/gains_2D_Kyz", (std::vector<double> ) {1.0,2.0,3.0,4.0});
    Kr_ = nh.param("control_constants/Kr", (double) 5.32);
    Tv_ = nh.param("control_constants/Tv", (double) 1.0);

    alpha_=nh.param("control_constants/alpha", 0.781); // in rad!
    beta_=nh.param("control_constants/beta", 0.0);
    sa_=sin(alpha_);
    sb_=sin(beta_);
    ca_=cos(alpha_);
    cb_=cos(beta_);

    update_rate_ = nh.param("control_constants/update_rate", 10.0);

    update_time_ = 1/update_rate_;

    //Subscriber:
    imu_sub_ = nh.subscribe("/ballbot/sensor/imu", 5, &ControlNode::imuCallback,this);
    joints_sub_ = nh.subscribe("/ballbot/joints/joint_states", 5, &ControlNode::jointsCallback,this);

    //Publisher:
    if(motors_controller_type_=="PositionJointInterface")
      motors_controller_type_="position";
    if(motors_controller_type_=="VelocityJointInterface")
      motors_controller_type_="velocity";
    if(motors_controller_type_=="EffortJointInterface")
      motors_controller_type_="effort";
    joint_commands_1_pub_ = nh.advertise<std_msgs::Float64>("/ballbot/joints/wheel1_"+motors_controller_type_+"_controller/command", 5);
    joint_commands_2_pub_ = nh.advertise<std_msgs::Float64>("/ballbot/joints/wheel2_"+motors_controller_type_+"_controller/command", 5);
    joint_commands_3_pub_ = nh.advertise<std_msgs::Float64>("/ballbot/joints/wheel3_"+motors_controller_type_+"_controller/command", 5);

    //Publish rpy angles:
    rpy_pub_ =  nh.advertise<geometry_msgs::PointStamped>("rpy_angles", 5);
    calc_ball_odom_pub_ = nh.advertise<geometry_msgs::PointStamped>("ball_odom_", 5);

    //Publish desired torques:
    desired_torques_pub_ = nh.advertise<geometry_msgs::Vector3>("desired_torques", 5);

    //action server:

    // init vectors:
    imu_phi_.push_back(0.0);
    imu_phi_.push_back(0.0);
    imu_phi_.push_back(0.0);
    imu_dphi_.push_back(0.0);
    imu_dphi_.push_back(0.0);
    imu_dphi_.push_back(0.0);
    realT_.push_back(0.0);
    realT_.push_back(0.0);
    realT_.push_back(0.0);

    // for control:
    psi_ball_actual_ = {0,0,0};
    psi_ball_last_ = {0,0,0};
    phi_ball_actual_ = {0,0,0};
    phi_ball_last_ = {0,0,0};
    imu_phi_last_ = {0,0,0};

    joints_velocity_={0.0, 0.0, 0.0};
  }

  // ROS API Callbacks: http://docs.ros.org/api/sensor_msgs/html/msg/Imu.html
  // runs with 200Hz actually new messages come in every 6ms.
  void ControlNode::imuCallback(const sensor_msgs::ImuConstPtr& imu_msg)
  {
    previous_imu_msg_ = *imu_msg;
    imu_phi_ = toEulerAngle(previous_imu_msg_.orientation.x, previous_imu_msg_.orientation.y, previous_imu_msg_.orientation.z, previous_imu_msg_.orientation.w); //rad
    imu_phi_[1]=-imu_phi_[1]; // if the y axis is the real system differently. 
    // TODO: z axis should also be rotated. 
    imu_dphi_.at(0)=previous_imu_msg_.angular_velocity.x; // rad/sec
    imu_dphi_.at(1)=-previous_imu_msg_.angular_velocity.y;
    imu_dphi_.at(2)=previous_imu_msg_.angular_velocity.z;

    balancing_time_ =previous_imu_msg_.header.stamp.toSec();
  }

  void ControlNode::jointsCallback(const sensor_msgs::JointStateConstPtr& joint_state_msg)
  {
    previous_joint_state_msg_ = *joint_state_msg;
    joints_velocity_[0] = previous_joint_state_msg_.velocity[30]; // wheel1
    joints_velocity_[1] = previous_joint_state_msg_.velocity[31]; // wheel2
    joints_velocity_[2] = previous_joint_state_msg_.velocity[32]; // wheel3
  }

  //update (publish messages...) every 10ms. This is only updated every 100 ms! but why?!
  void ControlNode::update()
  {
    calc2MotorCommands_withOdometry();

    // note that the wheel torque is limited to ca. 4.1Nm.
    std_msgs::Float64 realT1;
    std_msgs::Float64 realT2;
    std_msgs::Float64 realT3;

    realT1.data=realT_.at(0);
    realT2.data=realT_.at(1);
    realT3.data=realT_.at(2);
//    realT1.data=0.0;
//    realT2.data=0.0;
//    realT3.data=1.0;
    joint_commands_1_pub_.publish(realT1);
    joint_commands_2_pub_.publish(realT2);
    joint_commands_3_pub_.publish(realT3);

    // publish desired Torques:
    desired_torques_.x =realT_.at(0);
    desired_torques_.y =realT_.at(1);
    desired_torques_.z =realT_.at(2);
    desired_torques_pub_.publish(desired_torques_);

    // the found angles are written in a geometry_msgs::Vector3
    geometry_msgs::PointStamped rpy;
    rpy.header.stamp = ros::Time::now();
    rpy.header.frame_id = "rpy_grad_angles";
    rpy.point.x = imu_phi_.at(0)*180/3.14159265359;
    rpy.point.y = imu_phi_.at(1)*180/3.14159265359;
    rpy.point.z = imu_phi_.at(2)*180/3.14159265359;
    rpy_pub_.publish(rpy);

    if(abs(rpy.point.x)>10 || abs(rpy.point.y)>10)
    {
      ROS_WARN_ONCE("x || y = higher than 10° Grad!"); // shows the simulation time!
    }
  }

}  // end namespace ballbot

int main(int argc, char** argv)
{
  ros::init(argc, argv, "ballbot_control");
  ROS_INFO("start ballbot_control_node:");

  ros::NodeHandle nh;

  // this wait is needed to ensure this ros node has gotten
  // simulation published /clock message, containing
  // simulation time.
  ros::Time last_ros_time_;
  bool wait = true;
  while (wait)
  {
    last_ros_time_ = ros::Time::now();
    if (last_ros_time_.toSec() > 0)
      wait = false;
  }

  ballbot::ControlNode node(nh);
  ros::Rate loop_rate(nh.param("/ballbot/control_constants/update_rate", 100.0));

  // TODO wait here to start publishing nodes information
  // until first non zero value from gazebo is received!
  while (ros::ok())
  {
    ros::spinOnce();
    node.update();
    loop_rate.sleep();
  }

  return 0;
}
