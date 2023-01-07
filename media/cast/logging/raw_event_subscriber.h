// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_RAW_EVENT_SUBSCRIBER_H_
#define MEDIA_CAST_LOGGING_RAW_EVENT_SUBSCRIBER_H_

#include "media/cast/logging/logging_defines.h"

namespace media {
namespace cast {

// A subscriber interface to subscribe to cast raw event logs.
// Those who wish to subscribe to raw event logs must implement this interface,
// and call LoggingImpl::AddRawEventSubscriber() with the subscriber, in order
// to start receiving raw event logs.
class RawEventSubscriber {
 public:
  virtual ~RawEventSubscriber() {}

  // Called on main thread when a FrameEvent, given by |frame_event|, is logged.
  virtual void OnReceiveFrameEvent(const FrameEvent& frame_event) = 0;

  // Called on main thread when a PacketEvent, given by |packet_event|,
  // is logged.
  virtual void OnReceivePacketEvent(const PacketEvent& packet_event) = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_RAW_EVENT_SUBSCRIBER_H_
