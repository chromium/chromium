// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/simple_event_subscriber.h"

#include "base/check.h"

namespace media {
namespace cast {

SimpleEventSubscriber::SimpleEventSubscriber() = default;

SimpleEventSubscriber::~SimpleEventSubscriber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SimpleEventSubscriber::OnReceiveFrameEvent(const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  frame_events_.push_back(frame_event);
}

void SimpleEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  packet_events_.push_back(packet_event);
}

void SimpleEventSubscriber::GetFrameEventsAndReset(
    std::vector<FrameEvent>* frame_events) {
  DCHECK(thread_checker_.CalledOnValidThread());
  frame_events->swap(frame_events_);
  frame_events_.clear();
}

void SimpleEventSubscriber::GetPacketEventsAndReset(
    std::vector<PacketEvent>* packet_events) {
  DCHECK(thread_checker_.CalledOnValidThread());
  packet_events->swap(packet_events_);
  packet_events_.clear();
}

}  // namespace cast
}  // namespace media
