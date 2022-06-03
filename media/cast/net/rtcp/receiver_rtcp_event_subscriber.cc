// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/receiver_rtcp_event_subscriber.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"

namespace media {
namespace cast {

ReceiverRtcpEventSubscriber::ReceiverRtcpEventSubscriber(
    const size_t max_size_to_retain, EventMediaType type)
    : max_size_to_retain_(
          max_size_to_retain * (kResendDelay * kNumResends + 1)),
      type_(type),
      popped_events_(0) {
  DCHECK(max_size_to_retain_ > 0u);
  DCHECK(type_ == AUDIO_EVENT || type_ == VIDEO_EVENT);
  for (size_t i = 0; i < kNumResends; i++) {
    send_ptrs_[i] = 0;
  }
}

ReceiverRtcpEventSubscriber::~ReceiverRtcpEventSubscriber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ReceiverRtcpEventSubscriber::OnReceiveFrameEvent(
    const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ShouldProcessEvent(frame_event.type, frame_event.media_type)) {
    RtcpEvent rtcp_event;
    switch (frame_event.type) {
      case FRAME_PLAYOUT:
        rtcp_event.delay_delta = frame_event.delay_delta;
        FALLTHROUGH;
      case FRAME_ACK_SENT:
      case FRAME_DECODED:
        rtcp_event.type = frame_event.type;
        rtcp_event.timestamp = frame_event.timestamp;
        rtcp_events_.push_back(
            std::make_pair(frame_event.rtp_timestamp, rtcp_event));
        break;
      default:
        break;
    }
  }

  TruncateMapIfNeeded();
}

void ReceiverRtcpEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ShouldProcessEvent(packet_event.type, packet_event.media_type)) {
    RtcpEvent rtcp_event;
    if (packet_event.type == PACKET_RECEIVED) {
      rtcp_event.type = packet_event.type;
      rtcp_event.timestamp = packet_event.timestamp;
      rtcp_event.packet_id = packet_event.packet_id;
      rtcp_events_.push_back(
          std::make_pair(packet_event.rtp_timestamp, rtcp_event));
    }
  }

  TruncateMapIfNeeded();
}

struct CompareByFirst {
  bool operator()(const std::pair<RtpTimeTicks, RtcpEvent>& a,
                  const std::pair<RtpTimeTicks, RtcpEvent>& b) {
    return a.first < b.first;
  }
};

void ReceiverRtcpEventSubscriber::GetRtcpEventsWithRedundancy(
    RtcpEvents* rtcp_events) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(rtcp_events);

  uint64_t event_level = rtcp_events_.size() + popped_events_;
  event_levels_for_past_frames_.push_back(event_level);

  for (size_t i = 0; i < kNumResends; i++) {
    size_t resend_delay = kResendDelay * i;
    if (event_levels_for_past_frames_.size() < resend_delay + 1)
      break;

    uint64_t send_limit =
        event_levels_for_past_frames_[event_levels_for_past_frames_.size() - 1 -
                                      resend_delay];

    if (send_ptrs_[i] < popped_events_) {
      send_ptrs_[i] = popped_events_;
    }

    while (send_ptrs_[i] < send_limit &&
           rtcp_events->size() < kMaxEventsPerRTCP) {
      rtcp_events->push_back(rtcp_events_[send_ptrs_[i] - popped_events_]);
      send_ptrs_[i]++;
    }
    send_limit = send_ptrs_[i];
  }

  if (event_levels_for_past_frames_.size() > kResendDelay * (kNumResends + 1)) {
    while (popped_events_ < event_levels_for_past_frames_[0]) {
      rtcp_events_.pop_front();
      popped_events_++;
    }
    event_levels_for_past_frames_.pop_front();
  }

  std::sort(rtcp_events->begin(), rtcp_events->end(), CompareByFirst());
}

void ReceiverRtcpEventSubscriber::TruncateMapIfNeeded() {
  // If map size has exceeded |max_size_to_retain_|, remove entry with
  // the smallest RTP timestamp.
  if (rtcp_events_.size() > max_size_to_retain_) {
    DVLOG(3) << "RTCP event map exceeded size limit; "
             << "removing oldest entry";
    // This is fine since we only insert elements one at a time.
    rtcp_events_.pop_front();
    popped_events_++;
  }

  DCHECK(rtcp_events_.size() <= max_size_to_retain_);
}

bool ReceiverRtcpEventSubscriber::ShouldProcessEvent(
    CastLoggingEvent event_type, EventMediaType event_media_type) {
  return type_ == event_media_type &&
      (event_type == FRAME_ACK_SENT || event_type == FRAME_DECODED ||
       event_type == FRAME_PLAYOUT || event_type == PACKET_RECEIVED);
}

}  // namespace cast
}  // namespace media
