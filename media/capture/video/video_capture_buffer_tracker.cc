// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_buffer_tracker.h"

namespace media {

void VideoCaptureBufferTracker::SetHeldByProducer(bool new_held_by_producer) {
  DCHECK_NE(held_by_producer_, new_held_by_producer);
  // The producer can't take hold while a consumer still has hold.
  if (new_held_by_producer)
    DCHECK_EQ(consumer_hold_count_, 0);
  held_by_producer_ = new_held_by_producer;
}

void VideoCaptureBufferTracker::AddConsumerHolds(int count) {
  // New consumer holds may only be made while the producer hold is still on.
  // This is because the buffer may disappear out from under us as soon as
  // neither producer nor consumers have a hold on it.
  DCHECK_EQ(consumer_hold_count_, 0);
  DCHECK(held_by_producer_);
  consumer_hold_count_ += count;
  OnHeldByConsumersChanged(true);
}

void VideoCaptureBufferTracker::RemoveConsumerHolds(int count) {
  DCHECK_GE(consumer_hold_count_, count);
  consumer_hold_count_ -= count;
  if (consumer_hold_count_ == 0) {
    static uint64_t sequence_number = 0;
    last_customer_use_sequence_number_ = ++sequence_number;
    OnHeldByConsumersChanged(false);
  }
}

bool VideoCaptureBufferTracker::IsSameGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle) const {
  return false;
}

void VideoCaptureBufferTracker::OnHeldByConsumersChanged(
    bool is_held_by_consumers) {}

}  // namespace media
