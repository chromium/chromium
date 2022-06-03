// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/time/tick_clock.h"
#include "media/cast/logging/receiver_time_offset_estimator_impl.h"

namespace media {
namespace cast {

namespace {

// Bitwise merging of values to produce an ordered key for entries in the
// BoundCalculator::events_ map.
uint64_t MakeEventKey(RtpTimeTicks rtp, uint16_t packet_id, bool audio) {
  return (static_cast<uint64_t>(rtp.lower_32_bits()) << 32) |
         (static_cast<uint64_t>(packet_id) << 1) |
         (audio ? UINT64_C(1) : UINT64_C(0));
}

}  // namespace

ReceiverTimeOffsetEstimatorImpl::BoundCalculator::BoundCalculator()
    : has_bound_(false) {}
ReceiverTimeOffsetEstimatorImpl::BoundCalculator::~BoundCalculator() = default;

void ReceiverTimeOffsetEstimatorImpl::BoundCalculator::SetSent(
    RtpTimeTicks rtp,
    uint16_t packet_id,
    bool audio,
    base::TimeTicks t) {
  const uint64_t key = MakeEventKey(rtp, packet_id, audio);
  events_[key].first = t;
  CheckUpdate(key);
}

void ReceiverTimeOffsetEstimatorImpl::BoundCalculator::SetReceived(
    RtpTimeTicks rtp,
    uint16_t packet_id,
    bool audio,
    base::TimeTicks t) {
  const uint64_t key = MakeEventKey(rtp, packet_id, audio);
  events_[key].second = t;
  CheckUpdate(key);
}

void ReceiverTimeOffsetEstimatorImpl::BoundCalculator::UpdateBound(
    base::TimeTicks sent, base::TimeTicks received) {
    base::TimeDelta delta = received - sent;
    if (has_bound_) {
      if (delta < bound_) {
        bound_ = delta;
      } else {
        bound_ += (delta - bound_) / kClockDriftSpeed;
      }
    } else {
      bound_ = delta;
    }
    has_bound_ = true;
  }

  void ReceiverTimeOffsetEstimatorImpl::BoundCalculator::CheckUpdate(
      uint64_t key) {
  const TimeTickPair& ticks = events_[key];
  if (!ticks.first.is_null() && !ticks.second.is_null()) {
    UpdateBound(ticks.first, ticks.second);
    events_.erase(key);
    return;
  }

  if (events_.size() > kMaxEventTimesMapSize) {
    auto i = ModMapOldest(&events_);
    if (i != events_.end()) {
      events_.erase(i);
    }
  }
}

ReceiverTimeOffsetEstimatorImpl::ReceiverTimeOffsetEstimatorImpl() = default;

ReceiverTimeOffsetEstimatorImpl::~ReceiverTimeOffsetEstimatorImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}


void ReceiverTimeOffsetEstimatorImpl::OnReceiveFrameEvent(
    const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (frame_event.type) {
    case FRAME_ACK_SENT:
      lower_bound_.SetSent(frame_event.rtp_timestamp,
                           0,
                           frame_event.media_type == AUDIO_EVENT,
                           frame_event.timestamp);
      break;
    case FRAME_ACK_RECEIVED:
      lower_bound_.SetReceived(frame_event.rtp_timestamp,
                               0,
                               frame_event.media_type == AUDIO_EVENT,
                               frame_event.timestamp);
      break;
    default:
      // Ignored
      break;
  }
}

bool ReceiverTimeOffsetEstimatorImpl::GetReceiverOffsetBounds(
    base::TimeDelta* lower_bound,
    base::TimeDelta* upper_bound) {
  if (!lower_bound_.has_bound() || !upper_bound_.has_bound())
    return false;

  *lower_bound = -lower_bound_.bound();
  *upper_bound = upper_bound_.bound();

  // Sanitize the output, we don't want the upper bound to be
  // lower than the lower bound, make them the same.
  if (upper_bound < lower_bound) {
    lower_bound += (lower_bound - upper_bound) / 2;
    upper_bound = lower_bound;
  }
  return true;
}

void ReceiverTimeOffsetEstimatorImpl::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (packet_event.type) {
    case PACKET_SENT_TO_NETWORK:
      upper_bound_.SetSent(packet_event.rtp_timestamp,
                           packet_event.packet_id,
                           packet_event.media_type == AUDIO_EVENT,
                           packet_event.timestamp);
      break;
    case PACKET_RECEIVED:
      upper_bound_.SetReceived(packet_event.rtp_timestamp,
                               packet_event.packet_id,
                               packet_event.media_type == AUDIO_EVENT,
                               packet_event.timestamp);
      break;
    default:
      // Ignored
      break;
  }
}


}  // namespace cast
}  // namespace media
