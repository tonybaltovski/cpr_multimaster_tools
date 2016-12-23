/**
Software License Agreement (proprietary)

\file      service_relay.h
\authors   Paul Bovbel <pbovbel@clearpathrobotics.com>
\copyright Copyright (c) 2015, Clearpath Robotics, Inc., All rights reserved.

Redistribution and use in source and binary forms, with or without modification, is not permitted without the
express permission of Clearpath Robotics.
*/
#ifndef MESSAGE_RELAY_RELAY_SERVICE_RELAY_H
#define MESSAGE_RELAY_RELAY_SERVICE_RELAY_H

#include "message_relay/processor/message_processor.h"
#include "ros/callback_queue.h"
#include "ros/ros.h"

#include <string>

namespace message_relay
{

struct ServiceRelayParams
{
  std::string type;
  std::string service;
  boost::shared_ptr<ros::NodeHandle> origin;
  boost::shared_ptr<ros::NodeHandle> target;
  FrameIdProcessor::ConstPtr frame_id_processor;
  TimeProcessor::ConstPtr time_processor;
  double check_for_server_frequency;
  boost::shared_ptr<ros::CallbackQueue> callback_queue;

  ServiceRelayParams()
      : check_for_server_frequency(1.0)
  { }
};

class ServiceRelay
{
public:
  typedef boost::shared_ptr<ServiceRelay> Ptr;

  virtual ~ServiceRelay()
  { }

protected:
  ServiceRelay()
  { }
};

template< typename ServiceType >
class ServiceRelayImpl : public ServiceRelay
{
  friend ServiceRelay::Ptr createServiceRelay(const ServiceRelayParams &params);

private:
  explicit ServiceRelayImpl(const ServiceRelayParams &params)
      : origin_(params.origin), target_(params.target), frame_id_processor_(params.frame_id_processor),
        time_processor_(params.time_processor), check_for_server_frequency_(params.check_for_server_frequency)
  {
    frame_id_processor_inverse_ = FrameIdProcessor::inverse(frame_id_processor_);
    time_processor_inverse_ = TimeProcessor::inverse(time_processor_);

    server_options_ = ros::AdvertiseServiceOptions::create<ServiceType>(
        params.service,
        boost::bind(&ServiceRelayImpl<ServiceType>::serviceCb, this, _1, _2),
        ros::VoidConstPtr(),
        params.callback_queue.get());
    client_ = origin_->serviceClient<ServiceType>(server_options_.service);
    ROS_DEBUG_STREAM("Created service client at " << origin_->getNamespace() << "/" << server_options_.service
        << ", waiting for connection...");

    ros::TimerOptions wait_timer_options(
        ros::Duration(1.0),
        boost::bind(&ServiceRelayImpl<ServiceType>::waitCb, this),
        params.callback_queue.get());
    wait_timer_ = origin_->createTimer(wait_timer_options);
  }

  void waitCb()
  {
    ROS_INFO_STREAM("Searching for service server at " << origin_->getNamespace() << "/" << server_options_.service
        << "...");
    if (client_.waitForExistence(ros::Duration(0.1)))
    {
      ROS_INFO_STREAM("...found, creating relay server at " << target_->getNamespace() << "/"
          << server_options_.service);
      server_ = target_->advertiseService(server_options_);
      wait_timer_.stop();
    }
    else
    {
      ROS_WARN_STREAM("...not found");
    }
  }


  bool serviceCb(typename ServiceType::Request &req, typename ServiceType::Response &res)
  {
    if (frame_id_processor_inverse_)
    {
      ServiceProcessor<ServiceType, FrameIdProcessor>::processRequest(req, frame_id_processor_inverse_);
    }
    if (time_processor_inverse_)
    {
      ServiceProcessor<ServiceType, TimeProcessor>::processRequest(req, time_processor_inverse_);
    }

    client_.call(req, res);

    if (frame_id_processor_)
    {
      ServiceProcessor<ServiceType, FrameIdProcessor>::processResponse(res, frame_id_processor_);
    }
    if (time_processor_)
    {
      ServiceProcessor<ServiceType, TimeProcessor>::processResponse(res, time_processor_);
    }
    return true;
  };

  ros::AdvertiseServiceOptions server_options_;
  boost::shared_ptr<ros::NodeHandle> origin_, target_;
  FrameIdProcessor::ConstPtr frame_id_processor_, frame_id_processor_inverse_;
  TimeProcessor::ConstPtr time_processor_, time_processor_inverse_;
  double check_for_server_frequency_;

  ros::ServiceServer server_;
  ros::ServiceClient client_;
  ros::Timer wait_timer_;
};

}  // namespace message_relay

#endif  // MESSAGE_RELAY_RELAY_SERVICE_RELAY_H
