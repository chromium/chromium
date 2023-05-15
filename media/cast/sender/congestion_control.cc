// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The purpose of this file is determine what bitrate to use for mirroring.
// Ideally this should be as much as possible, without causing any frames to
// arrive late.

// The current algorithm is to measure how much bandwidth we've been using
// recently. We also keep track of how much data has been queued up for sending
// in a virtual "buffer" (this virtual buffer represents all the buffers between
// the sender and the receiver, including retransmissions and so forth.)
// If we estimate that our virtual buffer is mostly empty, we try to use
// more bandwidth than our recent usage, otherwise we use less.

#include "media/cast/sender/congestion_control.h"

#include <algorithm>
#include <deque>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/cast/constants.h"

namespace media {
namespace cast {

class AdaptiveCongestionControl final : public CongestionControl {
 public:
  AdaptiveCongestionControl(const base::TickClock* clock,
                            int max_bitrate_configured,
                            int min_bitrate_configured,
                            double max_frame_rate);

  AdaptiveCongestionControl(const AdaptiveCongestionControl&) = delete;
  AdaptiveCongestionControl& operator=(const AdaptiveCongestionControl&) =
      delete;

  ~AdaptiveCongestionControl() final;

  // CongestionControl implementation.
  void UpdateRtt(base::TimeDelta rtt) final;
  void UpdateTargetPlayoutDelay(base::TimeDelta delay) final;
  void WillSendFrameToTransport(FrameId frame_id,
                                size_t frame_size_in_bytes,
                                base::TimeTicks when) final;
  void AckFrame(FrameId frame_id, base::TimeTicks when) final;
  void AckLaterFrames(std::vector<FrameId> received_frames,
                      base::TimeTicks when) final;
  int GetBitrate(base::TimeTicks playout_time,
                 base::TimeDelta playout_delay) final;

 private:
  struct FrameStats {
    FrameStats();
    // Time this frame was first enqueued for transport.
    base::TimeTicks enqueue_time;
    // Time this frame was acked.
    base::TimeTicks ack_time;
    // Size of encoded frame in bits.
    size_t frame_size_in_bits;
  };

  // Calculate how much "dead air" (idle time) there is between two frames.
  static base::TimeDelta DeadTime(const FrameStats& a, const FrameStats& b);
  // Get the FrameStats for a given |frame_id|, auto-creating a new FrameStats
  // for newer frames, but possibly returning nullptr for older frames that have
  // been pruned. Never returns nullptr for |frame_id|s equal to or greater than
  // |last_checkpoint_frame_|.
  // Note: Older FrameStats will be removed automatically.
  FrameStats* GetFrameStats(FrameId frame_id);
  // Discard old FrameStats.
  void PruneFrameStats();
  // Calculate a safe bitrate. This is based on how much we've been
  // sending in the past.
  double CalculateSafeBitrate();

  // Estimate when the transport will start sending the data for a given frame.
  // |estimated_bitrate| is the current estimated transmit bitrate in bits per
  // second.
  base::TimeTicks EstimatedSendingTime(FrameId frame_id,
                                       double estimated_bitrate);

  const raw_ptr<const base::TickClock> clock_;  // Not owned by this class.
  const int max_bitrate_configured_;
  const int min_bitrate_configured_;
  const double max_frame_rate_;

  // This can not be a base::circular_deque because the AckFrame implementation
  // preserves a FrameStats* pointing inside the deque across mutations.
  std::deque<FrameStats> frame_stats_;

  FrameId last_frame_stats_;
  // This is the latest known frame that all previous frames (having smaller
  // |frame_id|) and this frame were acked by receiver.
  FrameId last_checkpoint_frame_;
  // This is the first time that |last_checkpoint_frame_| is marked.
  base::TimeTicks last_checkpoint_time_;
  FrameId last_enqueued_frame_;
  base::TimeDelta rtt_;
  size_t history_size_;
  size_t acked_bits_in_history_;
  base::TimeDelta dead_time_in_history_;
};

class FixedCongestionControl final : public CongestionControl {
 public:
  explicit FixedCongestionControl(int bitrate) : bitrate_(bitrate) {}

  FixedCongestionControl(const FixedCongestionControl&) = delete;
  FixedCongestionControl& operator=(const FixedCongestionControl&) = delete;

  ~FixedCongestionControl() final = default;

  // CongestionControl implementation.
  void UpdateRtt(base::TimeDelta rtt) final {}
  void UpdateTargetPlayoutDelay(base::TimeDelta delay) final {}
  void WillSendFrameToTransport(FrameId frame_id,
                                size_t frame_size_in_bytes,
                                base::TimeTicks when) final {}
  void AckFrame(FrameId frame_id, base::TimeTicks when) final {}
  void AckLaterFrames(std::vector<FrameId> received_frames,
                      base::TimeTicks when) final {}
  int GetBitrate(base::TimeTicks playout_time,
                 base::TimeDelta playout_delay) final {
    return bitrate_;
  }

 private:
  const int bitrate_;
};

CongestionControl* NewAdaptiveCongestionControl(const base::TickClock* clock,
                                                int max_bitrate_configured,
                                                int min_bitrate_configured,
                                                double max_frame_rate) {
  return new AdaptiveCongestionControl(clock,
                                       max_bitrate_configured,
                                       min_bitrate_configured,
                                       max_frame_rate);
}

CongestionControl* NewFixedCongestionControl(int bitrate) {
  return new FixedCongestionControl(bitrate);
}

// This means that we *try* to keep our buffer 90% empty.
// If it is less full, we increase the bandwidth, if it is more
// we decrease the bandwidth. Making this smaller makes the
// congestion control more aggressive.
static const double kTargetEmptyBufferFraction = 0.9;

// This is the size of our history in frames. Larger values makes the
// congestion control adapt slower.
static const size_t kHistorySize = 100;

AdaptiveCongestionControl::FrameStats::FrameStats() : frame_size_in_bits(0) {
}

AdaptiveCongestionControl::AdaptiveCongestionControl(
    const base::TickClock* clock,
    int max_bitrate_configured,
    int min_bitrate_configured,
    double max_frame_rate)
    : clock_(clock),
      max_bitrate_configured_(max_bitrate_configured),
      min_bitrate_configured_(min_bitrate_configured),
      max_frame_rate_(max_frame_rate),
      last_frame_stats_(FrameId::first() - 1),
      last_checkpoint_frame_(FrameId::first() - 1),
      last_enqueued_frame_(FrameId::first() - 1),
      history_size_(kHistorySize),
      acked_bits_in_history_(0) {
  DCHECK_GE(max_bitrate_configured, min_bitrate_configured) << "Invalid config";
  DCHECK_GT(min_bitrate_configured, 0);
  frame_stats_.resize(2);
  base::TimeTicks now = clock->NowTicks();
  frame_stats_[0].ack_time = now;
  frame_stats_[0].enqueue_time = now;
  frame_stats_[1].ack_time = now;
  frame_stats_[1].enqueue_time = now;
  last_checkpoint_time_ = now;
  DCHECK(!frame_stats_[0].ack_time.is_null());
}

CongestionControl::~CongestionControl() = default;
AdaptiveCongestionControl::~AdaptiveCongestionControl() = default;

void AdaptiveCongestionControl::UpdateRtt(base::TimeDelta rtt) {
  rtt_ = (7 * rtt_ + rtt) / 8;
}

void AdaptiveCongestionControl::UpdateTargetPlayoutDelay(
    base::TimeDelta delay) {
  const int max_unacked_frames = std::min<int>(
      kMaxUnackedFrames, 1 + (delay * max_frame_rate_).InSeconds());
  DCHECK_GT(max_unacked_frames, 0);
  history_size_ = max_unacked_frames + kHistorySize;
  PruneFrameStats();
}

// Calculate how much "dead air" there is between two frames.
base::TimeDelta AdaptiveCongestionControl::DeadTime(const FrameStats& a,
                                                    const FrameStats& b) {
  if (b.enqueue_time > a.ack_time) {
    return b.enqueue_time - a.ack_time;
  } else {
    return base::TimeDelta();
  }
}

double AdaptiveCongestionControl::CalculateSafeBitrate() {
  DCHECK(!frame_stats_.empty());
  base::TimeDelta transmit_time =
      GetFrameStats(last_checkpoint_frame_)->ack_time -
      frame_stats_.front().enqueue_time - dead_time_in_history_;

  if (acked_bits_in_history_ == 0 || transmit_time <= base::TimeDelta()) {
    return min_bitrate_configured_;
  }
  transmit_time = std::max(transmit_time, base::Milliseconds(1));
  return acked_bits_in_history_ / transmit_time.InSecondsF();
}

AdaptiveCongestionControl::FrameStats* AdaptiveCongestionControl::GetFrameStats(
    FrameId frame_id) {
  int offset = frame_id - last_frame_stats_;
  if (offset > 0) {
    // Sanity-check: Make sure the new |frame_id| will not cause an unreasonably
    // large increase in the history dataset.
    DCHECK_LE(offset, kMaxUnackedFrames + 1);

    frame_stats_.resize(frame_stats_.size() + offset);
    last_frame_stats_ = frame_id;
    offset = 0;
  }
  PruneFrameStats();
  offset += frame_stats_.size() - 1;
  if (offset < 0) {
    DCHECK_LT(frame_id, last_checkpoint_frame_);
    return nullptr;  // Old frame has been pruned from the dataset.
  }
  return &frame_stats_[offset];
}

void AdaptiveCongestionControl::PruneFrameStats() {
  // Maintain a minimal amount of history, specified by |history_size_|, that
  // MUST also include all frames from the last ACK'ed frame.
  const size_t retention_count = std::max<size_t>(
      history_size_, last_frame_stats_ - last_checkpoint_frame_ + 1);

  // Sanity-check: At least one entry must be kept, but the dataset should not
  // grow indefinitely.
  DCHECK_GE(retention_count, 1u);
  constexpr size_t kMaxInFlightRangeSize =
      kMaxUnackedFrames +  // Maximum unACKed frames.
      1 +                  // The last ACKed frame.
      1;  // One not-yet-enqueued frame (see call to EstimatedSendingTime()).
  DCHECK_LE(retention_count, std::max(history_size_, kMaxInFlightRangeSize));

  while (frame_stats_.size() > retention_count) {
    DCHECK(!frame_stats_[0].ack_time.is_null());
    acked_bits_in_history_ -= frame_stats_[0].frame_size_in_bits;
    dead_time_in_history_ -= DeadTime(frame_stats_[0], frame_stats_[1]);
    DCHECK_GE(acked_bits_in_history_, 0UL);
    VLOG(2) << "DT: " << dead_time_in_history_.InSecondsF();
    DCHECK_GE(dead_time_in_history_, base::TimeDelta());
    frame_stats_.pop_front();
  }
}

void AdaptiveCongestionControl::AckFrame(FrameId frame_id,
                                         base::TimeTicks when) {
  FrameStats* frame_stats = GetFrameStats(last_checkpoint_frame_);
  while (last_checkpoint_frame_ < frame_id) {
    FrameStats* last_frame_stats = frame_stats;
    frame_stats = GetFrameStats(last_checkpoint_frame_ + 1);
    // Note: This increment must happen AFTER the GetFrameStats() call to
    // prevent the |last_frame_stats| pointer from being invalidated.
    last_checkpoint_frame_++;
    // When ACKing a frame that was never sent, just pretend it was sent and
    // ACKed at the same point-in-time.
    if (frame_stats->enqueue_time.is_null())
      frame_stats->enqueue_time = when;
    else if (when < frame_stats->enqueue_time)
      when = frame_stats->enqueue_time;
    // Don't overwrite the ack time for those frames that were already acked in
    // previous extended ACKs.
    if (frame_stats->ack_time.is_null())
      frame_stats->ack_time = when;
    DCHECK_GE(when, frame_stats->ack_time);
    acked_bits_in_history_ += frame_stats->frame_size_in_bits;
    dead_time_in_history_ += DeadTime(*last_frame_stats, *frame_stats);
    last_checkpoint_time_ = when;
  }
}

void AdaptiveCongestionControl::AckLaterFrames(
    std::vector<FrameId> received_frames,
    base::TimeTicks when) {
  DCHECK(std::is_sorted(received_frames.begin(), received_frames.end()));
  for (FrameId frame_id : received_frames) {
    if (frame_id <= last_checkpoint_frame_)
      continue;
    FrameStats* frame_stats = GetFrameStats(frame_id);
    // When ACKing a frame that was never sent, just pretend it was sent and
    // ACKed at the same point-in-time.
    if (frame_stats->enqueue_time.is_null())
      frame_stats->enqueue_time = when;
    else if (when < frame_stats->enqueue_time)
      when = frame_stats->enqueue_time;
    // Don't overwrite the ack time for those frames that were acked before.
    if (frame_stats->ack_time.is_null())
      frame_stats->ack_time = when;
    DCHECK_GE(when, frame_stats->ack_time);
  }
}

void AdaptiveCongestionControl::WillSendFrameToTransport(
    FrameId frame_id,
    size_t frame_size_in_bytes,
    base::TimeTicks when) {
  last_enqueued_frame_ = frame_id;
  FrameStats* frame_stats = GetFrameStats(frame_id);
  DCHECK(frame_stats);
  frame_stats->enqueue_time = when;
  frame_stats->frame_size_in_bits = frame_size_in_bytes * 8;
}

base::TimeTicks AdaptiveCongestionControl::EstimatedSendingTime(
    FrameId frame_id,
    double estimated_bitrate) {
  const base::TimeTicks now = clock_->NowTicks();

  // Starting with the time of the latest acknowledgement, extrapolate forward
  // to determine an estimated sending time for |frame_id|.
  //
  // |estimated_sending_time| will contain the estimated sending time for each
  // frame after the last ACK'ed frame.  It is possible for multiple frames to
  // be in-flight; and therefore it is common for the |estimated_sending_time|
  // for those frames to be before |now|.  The initial estimate is based on the
  // last ACKed frame and the RTT.
  base::TimeTicks estimated_sending_time = last_checkpoint_time_ - rtt_;
  for (FrameId f = last_checkpoint_frame_ + 1; f < frame_id; ++f) {
    FrameStats* const stats = GetFrameStats(f);

    // |estimated_ack_time| is the local time when the sender receives the ACK,
    // and not the time when the ACK left the receiver.
    base::TimeTicks estimated_ack_time = stats->ack_time;

    // Do not update the estimate if this frame's packets will never again enter
    // the packet send queue; unless there is no estimate yet.
    if (!estimated_ack_time.is_null())
      continue;

    // Model: The |estimated_sending_time| is the time at which the first byte
    // of the encoded frame is transmitted.  Then, assume the transmission of
    // the remaining bytes is paced such that the last byte has just left the
    // sender at |frame_transmit_time| later.  This last byte then takes
    // ~RTT/2 amount of time to travel to the receiver.  Finally, the ACK from
    // the receiver is sent and this takes another ~RTT/2 amount of time to
    // reach the sender.
    const base::TimeDelta frame_transmit_time =
        base::Seconds(stats->frame_size_in_bits / estimated_bitrate);
    estimated_ack_time = std::max(estimated_sending_time, stats->enqueue_time) +
                         frame_transmit_time + rtt_;

    if (estimated_ack_time < now) {
      // The current frame has not yet been ACK'ed and the yet the computed
      // |estimated_ack_time| is before |now|.  This contradiction must be
      // resolved.
      //
      // The solution below is a little counter-intuitive, but it seems to
      // work.  Basically, when we estimate that the ACK should have already
      // happened, we figure out how long ago it should have happened and
      // guess that the ACK will happen half of that time in the future.  This
      // will cause some over-estimation when acks are late, which is actually
      // the desired behavior.
      estimated_ack_time = now + (now - estimated_ack_time) / 2;
    }

    // Since we [in the common case] do not wait for an ACK before we start
    // sending the next frame, estimate the next frame's sending time as the
    // time just after the last byte of the current frame left the sender (see
    // Model comment above).
    estimated_sending_time =
        std::max(estimated_sending_time, estimated_ack_time - rtt_);
  }

  FrameStats* const frame_stats = GetFrameStats(frame_id);
  DCHECK(frame_stats);
  if (frame_stats->enqueue_time.is_null()) {
    // The frame has not yet been enqueued for transport.  Since it cannot be
    // enqueued in the past, ensure the result is lower-bounded by |now|.
    estimated_sending_time = std::max(estimated_sending_time, now);
  } else {
    // |frame_stats->enqueue_time| is the time the frame was enqueued for
    // transport.  The frame may not actually start being sent until a
    // point-in-time after that, because the transport is waiting for prior
    // frames to be acknowledged.
    estimated_sending_time =
        std::max(estimated_sending_time, frame_stats->enqueue_time);
  }

  return estimated_sending_time;
}

int AdaptiveCongestionControl::GetBitrate(base::TimeTicks playout_time,
                                          base::TimeDelta playout_delay) {
  const double safe_bitrate = CalculateSafeBitrate();
  // Estimate when we might start sending the next frame.
  const base::TimeDelta time_to_catch_up =
      playout_time -
      EstimatedSendingTime(last_enqueued_frame_ + 1, safe_bitrate);

  double empty_buffer_fraction = time_to_catch_up / playout_delay;
  empty_buffer_fraction = std::min(empty_buffer_fraction, 1.0);
  empty_buffer_fraction = std::max(empty_buffer_fraction, 0.0);

  const int bits_per_second = base::ClampRound(
      safe_bitrate * (empty_buffer_fraction / kTargetEmptyBufferFraction));
  VLOG(3) << " FBR:" << (bits_per_second / 1E6)
          << " EBF:" << empty_buffer_fraction
          << " SBR:" << (safe_bitrate / 1E6);
  TRACE_COUNTER_ID1("cast.stream", "Empty Buffer Fraction", this,
                    empty_buffer_fraction);

  return std::clamp(bits_per_second, min_bitrate_configured_,
                    max_bitrate_configured_);
}

}  // namespace cast
}  // namespace media
