// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/receiver/receiver_stats.h"

#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/net/rtp/rtp_defines.h"

namespace media {
namespace cast {

namespace {

constexpr uint32_t kMaxSequenceNumber = 65536;
bool IsNewerSequenceNumber(uint16_t sequence_number,
                           uint16_t prev_sequence_number) {
  return (sequence_number != prev_sequence_number) &&
         static_cast<uint16_t>(sequence_number - prev_sequence_number) < 0x8000;
}

}  // namespace

ReceiverStats::ReceiverStats(const base::TickClock* clock)
    : clock_(clock),
      min_sequence_number_(0),
      max_sequence_number_(0),
      total_number_packets_(0),
      sequence_number_cycles_(0),
      interval_min_sequence_number_(0),
      interval_number_packets_(0),
      interval_wrap_count_(0) {}

RtpReceiverStatistics ReceiverStats::GetStatistics() {
  RtpReceiverStatistics ret;
  // Compute losses.
  if (interval_number_packets_ == 0) {
    ret.fraction_lost = 0;
  } else {
    int diff = 0;
    if (interval_wrap_count_ == 0) {
      diff = max_sequence_number_ - interval_min_sequence_number_ + 1;
    } else {
      diff = kMaxSequenceNumber * (interval_wrap_count_ - 1) +
             (max_sequence_number_ - interval_min_sequence_number_ +
              kMaxSequenceNumber + 1);
    }

    if (diff < 1) {
      ret.fraction_lost = 0;
    } else {
      float tmp_ratio =
          (1 - static_cast<float>(interval_number_packets_) / abs(diff));
      ret.fraction_lost = static_cast<uint8_t>(256 * tmp_ratio);
    }
  }

  int expected_packets_num = max_sequence_number_ - min_sequence_number_ + 1;
  if (total_number_packets_ == 0) {
    ret.cumulative_lost = 0;
  } else if (sequence_number_cycles_ == 0) {
    ret.cumulative_lost = expected_packets_num - total_number_packets_;
  } else {
    ret.cumulative_lost =
        kMaxSequenceNumber * (sequence_number_cycles_ - 1) +
        (expected_packets_num - total_number_packets_ + kMaxSequenceNumber);
  }

  // Extended high sequence number consists of the highest seq number and the
  // number of cycles (wrap).
  ret.extended_high_sequence_number =
      (sequence_number_cycles_ << 16) + max_sequence_number_;

  ret.jitter =
      static_cast<uint32_t>(std::abs(jitter_.InMillisecondsRoundedUp()));

  // Reset interval values.
  interval_min_sequence_number_ = 0;
  interval_number_packets_ = 0;
  interval_wrap_count_ = 0;

  return ret;
}

void ReceiverStats::UpdateStatistics(const RtpCastHeader& header,
                                     int rtp_timebase) {
  const uint16_t new_seq_num = header.sequence_number;

  if (interval_number_packets_ == 0) {
    // First packet in the interval.
    interval_min_sequence_number_ = new_seq_num;
  }
  if (total_number_packets_ == 0) {
    // First incoming packet.
    min_sequence_number_ = new_seq_num;
    max_sequence_number_ = new_seq_num;
  }

  if (IsNewerSequenceNumber(new_seq_num, max_sequence_number_)) {
    // Check wrap.
    if (new_seq_num < max_sequence_number_) {
      ++sequence_number_cycles_;
      ++interval_wrap_count_;
    }
    max_sequence_number_ = new_seq_num;
  }

  // Compute Jitter.
  const base::TimeTicks now = clock_->NowTicks();
  if (total_number_packets_ > 0) {
    const base::TimeDelta packet_time_difference =
        now - last_received_packet_time_;
    const base::TimeDelta media_time_differerence = ToTimeDelta(
        header.rtp_timestamp - last_received_rtp_timestamp_, rtp_timebase);
    const base::TimeDelta delta =
        packet_time_difference - media_time_differerence;
    // Update jitter.
    jitter_ += (delta - jitter_) / 16;
  }
  last_received_rtp_timestamp_ = header.rtp_timestamp;
  last_received_packet_time_ = now;

  // Increment counters.
  ++total_number_packets_;
  ++interval_number_packets_;
}

}  // namespace cast
}  // namespace media
