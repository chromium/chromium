// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTCP_RECEIVER_RTCP_EVENT_SUBSCRIBER_H_
#define MEDIA_CAST_NET_RTCP_RECEIVER_RTCP_EVENT_SUBSCRIBER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/containers/circular_deque.h"
#include "base/threading/thread_checker.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/raw_event_subscriber.h"
#include "media/cast/net/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

static const size_t kNumResends = 3;
static const size_t kResendDelay = 10;
static const size_t kMaxEventsPerRTCP = 20;

// A RawEventSubscriber implementation with the following properties:
// - Only processes raw event types that are relevant for sending from cast
//   receiver to cast sender via RTCP.
// - Captures information to be sent over to RTCP from raw event logs into the
//   more compact RtcpEvent struct.
// - Orders events by RTP timestamp with a multimap.
// - Internally, the map is capped at a maximum size configurable by the caller.
//   The subscriber only keeps the most recent events (determined by RTP
//   timestamp) up to the size limit.
class ReceiverRtcpEventSubscriber final : public RawEventSubscriber {
 public:
  typedef std::pair<RtpTimeTicks, RtcpEvent> RtcpEventPair;
  typedef std::vector<std::pair<RtpTimeTicks, RtcpEvent>> RtcpEvents;

  // |max_size_to_retain|: The object will keep up to |max_size_to_retain|
  // events
  // in the map. Once threshold has been reached, an event with the smallest
  // RTP timestamp will be removed.
  // |type|: Determines whether the subscriber will process only audio or video
  // events.
  ReceiverRtcpEventSubscriber(const size_t max_size_to_retain,
      EventMediaType type);

  ReceiverRtcpEventSubscriber(const ReceiverRtcpEventSubscriber&) = delete;
  ReceiverRtcpEventSubscriber& operator=(const ReceiverRtcpEventSubscriber&) =
      delete;

  ~ReceiverRtcpEventSubscriber() final;

  // RawEventSubscriber implementation.
  void OnReceiveFrameEvent(const FrameEvent& frame_event) final;
  void OnReceivePacketEvent(const PacketEvent& packet_event) final;

  // Assigns events collected to |rtcp_events|. If there is space, some
  // older events will be added for redundancy as well.
  void GetRtcpEventsWithRedundancy(RtcpEvents* rtcp_events);

 private:
  // If |rtcp_events_.size()| exceeds |max_size_to_retain_|, remove an oldest
  // entry (determined by RTP timestamp) so its size no greater than
  // |max_size_to_retain_|.
  void TruncateMapIfNeeded();

  // Returns |true| if events of |event_type| and |media_type|
  // should be processed.
  bool ShouldProcessEvent(CastLoggingEvent event_type,
      EventMediaType media_type);

  const size_t max_size_to_retain_;
  EventMediaType type_;

  // The key should really be something more than just a RTP timestamp in order
  // to differentiate between video and audio frames, but since the
  // implementation doesn't mix audio and video frame events, RTP timestamp
  // only as key is fine.
  base::circular_deque<RtcpEventPair> rtcp_events_;

  // Counts how many events have been removed from rtcp_events_.
  uint64_t popped_events_;

  // Events greater than send_ptrs_[0] have not been sent yet.
  // Events greater than send_ptrs_[1] have been transmit once.
  // Note that these counters use absolute numbers, so you need
  // to subtract popped_events_ before looking up the events in
  // rtcp_events_.
  uint64_t send_ptrs_[kNumResends];

  // For each frame, we push how many events have been added to
  // rtcp_events_ so far. We use this to make sure that
  // send_ptrs_[N+1] is always at least kResendDelay frames behind
  // send_ptrs_[N]. Old information is removed so that information
  // for (kNumResends + 1) * kResendDelay frames remain.
  base::circular_deque<uint64_t> event_levels_for_past_frames_;

  // Ensures methods are only called on the main thread.
  base::ThreadChecker thread_checker_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTCP_RECEIVER_RTCP_EVENT_SUBSCRIBER_H_
