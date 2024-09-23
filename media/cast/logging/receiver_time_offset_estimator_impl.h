// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_RECEIVER_TIME_OFFSET_ESTIMATOR_IMPL_H_
#define MEDIA_CAST_LOGGING_RECEIVER_TIME_OFFSET_ESTIMATOR_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/cast/common/mod_util.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/receiver_time_offset_estimator.h"

namespace media {
namespace cast {


// This should be large enough so that we can collect all 3 events before
// the entry gets removed from the map.
const size_t kMaxEventTimesMapSize = 500;

// The lower, this is, the faster we adjust to clock drift.
// (But with more jitter.)
const size_t kClockDriftSpeed = 500;


// This implementation listens to two pair of events
// 1. FRAME_ACK_SENT / FRAME_ACK_RECEIVED  (receiver->sender)
// 2. PACKET_SENT_TO_NETWORK / PACKET_RECEIVED (sender->receiver)
// There is a causal relationship between these events in that these events
// must happen in order. This class obtains the lower and upper bounds for
// the offset by taking the difference of timestamps.
class ReceiverTimeOffsetEstimatorImpl final
    : public ReceiverTimeOffsetEstimator {
 public:
  ReceiverTimeOffsetEstimatorImpl();

  ReceiverTimeOffsetEstimatorImpl(const ReceiverTimeOffsetEstimatorImpl&) =
      delete;
  ReceiverTimeOffsetEstimatorImpl& operator=(
      const ReceiverTimeOffsetEstimatorImpl&) = delete;

  ~ReceiverTimeOffsetEstimatorImpl() final;

  // RawEventSubscriber implementations.
  void OnReceiveFrameEvent(const FrameEvent& frame_event) final;
  void OnReceivePacketEvent(const PacketEvent& packet_event) final;

  // ReceiverTimeOffsetEstimator implementation.
  bool GetReceiverOffsetBounds(base::TimeDelta* lower_bound,
                               base::TimeDelta* upper_bound) final;

 private:
  // This helper uses the difference between sent and received event
  // to calculate an upper bound on the difference between the clocks
  // on the sender and receiver. Note that this difference can take
  // very large positive or negative values, but the smaller value is
  // always the better estimate, since a receive event cannot possibly
  // happen before a send event.  Note that we use this to calculate
  // both upper and lower bounds by reversing the sender/receiver
  // relationship.
  class BoundCalculator {
   public:
    typedef std::pair<base::TimeTicks, base::TimeTicks> TimeTickPair;
    typedef std::map<uint64_t, TimeTickPair> EventMap;

    BoundCalculator();
    ~BoundCalculator();
    bool has_bound() const { return has_bound_; }
    base::TimeDelta bound() const { return bound_; }

    void SetSent(RtpTimeTicks rtp,
                 uint16_t packet_id,
                 bool audio,
                 base::TimeTicks t);

    void SetReceived(RtpTimeTicks rtp,
                     uint16_t packet_id,
                     bool audio,
                     base::TimeTicks t);

   private:
    void UpdateBound(base::TimeTicks a, base::TimeTicks b);
    void CheckUpdate(uint64_t key);

   private:
    EventMap events_;
    bool has_bound_;
    base::TimeDelta bound_;
  };

  // Fixed size storage to store event times for recent frames.
  BoundCalculator upper_bound_;
  BoundCalculator lower_bound_;

  base::ThreadChecker thread_checker_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_RECEIVER_TIME_OFFSET_ESTIMATOR_IMPL_H_
