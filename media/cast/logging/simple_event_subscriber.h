// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_SIMPLE_EVENT_SUBSCRIBER_H_
#define MEDIA_CAST_LOGGING_SIMPLE_EVENT_SUBSCRIBER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "media/cast/logging/raw_event_subscriber.h"

namespace media {
namespace cast {

// RawEventSubscriber implementation that records all incoming raw events
// in std::vector's.
// The user of this class can call the GetXXXEventsAndReset functions to get
// list of events that have acccumulated since last inovcation.
class SimpleEventSubscriber final : public RawEventSubscriber {
 public:
  SimpleEventSubscriber();

  SimpleEventSubscriber(const SimpleEventSubscriber&) = delete;
  SimpleEventSubscriber& operator=(const SimpleEventSubscriber&) = delete;

  ~SimpleEventSubscriber() final;

  // RawEventSubscriber implementations.
  void OnReceiveFrameEvent(const FrameEvent& frame_event) final;
  void OnReceivePacketEvent(const PacketEvent& packet_event) final;

  // Assigns frame events received so far to |frame_events| and clears them
  // from this object.
  void GetFrameEventsAndReset(std::vector<FrameEvent>* frame_events);

  // Assigns packet events received so far to |packet_events| and clears them
  // from this object.
  void GetPacketEventsAndReset(std::vector<PacketEvent>* packet_events);

 private:
  std::vector<FrameEvent> frame_events_;
  std::vector<PacketEvent> packet_events_;

  // All functions must be called on the main thread.
  base::ThreadChecker thread_checker_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_SIMPLE_EVENT_SUBSCRIBER_H_
