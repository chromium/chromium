// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/congestion_control/general_loss_algorithm.h"

#include "net/third_party/quic/core/congestion_control/rtt_stats.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"

namespace quic {

namespace {

// The minimum delay before a packet will be considered lost,
// regardless of SRTT.  Half of the minimum TLP, since the loss algorithm only
// triggers when a nack has been receieved for the packet.
static const size_t kMinLossDelayMs = 5;

// Default fraction of an RTT the algorithm waits before determining a packet is
// lost due to early retransmission by time based loss detection.
static const int kDefaultLossDelayShift = 2;
// Default fraction of an RTT when doing adaptive loss detection.
static const int kDefaultAdaptiveLossDelayShift = 4;

}  // namespace

GeneralLossAlgorithm::GeneralLossAlgorithm() : GeneralLossAlgorithm(kNack) {}

GeneralLossAlgorithm::GeneralLossAlgorithm(LossDetectionType loss_type)
    : loss_detection_timeout_(QuicTime::Zero()),
      largest_lost_(0),
      least_in_flight_(1),
      faster_detect_loss_(GetQuicReloadableFlag(quic_faster_detect_loss)) {
  SetLossDetectionType(loss_type);
}

void GeneralLossAlgorithm::SetLossDetectionType(LossDetectionType loss_type) {
  loss_detection_timeout_ = QuicTime::Zero();
  largest_sent_on_spurious_retransmit_ = 0;
  loss_type_ = loss_type;
  reordering_shift_ = loss_type == kAdaptiveTime
                          ? kDefaultAdaptiveLossDelayShift
                          : kDefaultLossDelayShift;
  if (GetQuicReloadableFlag(quic_eighth_rtt_loss_detection) &&
      loss_type == kTime) {
    QUIC_FLAG_COUNT(quic_reloadable_flag_quic_eighth_rtt_loss_detection);
    reordering_shift_ = 3;
  }
  largest_previously_acked_ = 0;
}

LossDetectionType GeneralLossAlgorithm::GetLossDetectionType() const {
  return loss_type_;
}

// Uses nack counts to decide when packets are lost.
void GeneralLossAlgorithm::DetectLosses(
    const QuicUnackedPacketMap& unacked_packets,
    QuicTime time,
    const RttStats& rtt_stats,
    QuicPacketNumber largest_newly_acked,
    const AckedPacketVector& packets_acked,
    LostPacketVector* packets_lost) {
  loss_detection_timeout_ = QuicTime::Zero();
  if (faster_detect_loss_ && !packets_acked.empty() &&
      packets_acked.front().packet_number == least_in_flight_) {
    if (least_in_flight_ + packets_acked.size() - 1 == largest_newly_acked) {
      // Optimization for the case when no packet is missing.
      QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_faster_detect_loss, 1, 3);
      least_in_flight_ = largest_newly_acked + 1;
      largest_previously_acked_ = largest_newly_acked;
      return;
    }
    QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_faster_detect_loss, 2, 3);
    // There is hole in acked_packets, increment least_in_flight_ if possible.
    for (const auto& acked : packets_acked) {
      if (acked.packet_number != least_in_flight_) {
        break;
      }
      ++least_in_flight_;
    }
  }
  QuicTime::Delta max_rtt =
      std::max(rtt_stats.previous_srtt(), rtt_stats.latest_rtt());
  QuicTime::Delta loss_delay =
      std::max(QuicTime::Delta::FromMilliseconds(kMinLossDelayMs),
               max_rtt + (max_rtt >> reordering_shift_));
  QuicPacketNumber packet_number = unacked_packets.GetLeastUnacked();
  auto it = unacked_packets.begin();
  if (faster_detect_loss_) {
    if (least_in_flight_ >= packet_number) {
      if (least_in_flight_ > unacked_packets.largest_sent_packet() + 1) {
        QUIC_BUG << "least_in_flight: " << least_in_flight_
                 << " is greater than largest_sent_packet + 1: "
                 << unacked_packets.largest_sent_packet() + 1;
      } else {
        QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_faster_detect_loss, 3, 3);
        it += (least_in_flight_ - packet_number);
        packet_number = least_in_flight_;
      }
    }
    // Clear least_in_flight_.
    least_in_flight_ = kInvalidPacketNumber;
  } else {
    if (largest_lost_ >= packet_number) {
      if (largest_lost_ > unacked_packets.largest_sent_packet()) {
        QUIC_BUG << "largest_lost: " << largest_lost_
                 << " is greater than largest_sent_packet: "
                 << unacked_packets.largest_sent_packet();
      } else {
        it += (largest_lost_ - packet_number + 1);
        packet_number = largest_lost_ + 1;
      }
    }
  }
  for (; it != unacked_packets.end() && packet_number <= largest_newly_acked;
       ++it, ++packet_number) {
    if (!it->in_flight) {
      continue;
    }

    if (loss_type_ == kNack) {
      // FACK based loss detection.
      if (largest_newly_acked - packet_number >=
          kNumberOfNacksBeforeRetransmission) {
        packets_lost->push_back(LostPacket(packet_number, it->bytes_sent));
        continue;
      }
    } else if (loss_type_ == kLazyFack) {
      // Require two in order acks to invoke FACK, which avoids spuriously
      // retransmitting packets when one packet is reordered by a large amount.
      if (largest_newly_acked > largest_previously_acked_ &&
          largest_previously_acked_ > packet_number &&
          largest_previously_acked_ - packet_number >=
              (kNumberOfNacksBeforeRetransmission - 1)) {
        packets_lost->push_back(LostPacket(packet_number, it->bytes_sent));
        continue;
      }
    }

    // Only early retransmit(RFC5827) when the last packet gets acked and
    // there are retransmittable packets in flight.
    // This also implements a timer-protected variant of FACK.
    if (unacked_packets.largest_sent_retransmittable_packet() <=
            largest_newly_acked ||
        loss_type_ == kTime || loss_type_ == kAdaptiveTime) {
      QuicTime when_lost = it->sent_time + loss_delay;
      if (time < when_lost) {
        loss_detection_timeout_ = when_lost;
        if (least_in_flight_ == kInvalidPacketNumber) {
          // At this point, packet_number is in flight and not detected as lost.
          least_in_flight_ = packet_number;
        }
        break;
      }
      packets_lost->push_back(LostPacket(packet_number, it->bytes_sent));
      continue;
    }

    // NACK-based loss detection allows for a max reordering window of 1 RTT.
    if (it->sent_time + rtt_stats.smoothed_rtt() <
        unacked_packets.GetTransmissionInfo(largest_newly_acked).sent_time) {
      packets_lost->push_back(LostPacket(packet_number, it->bytes_sent));
      continue;
    }
    if (least_in_flight_ == kInvalidPacketNumber) {
      // At this point, packet_number is in flight and not detected as lost.
      least_in_flight_ = packet_number;
    }
  }
  if (least_in_flight_ == kInvalidPacketNumber) {
    // There is no in flight packet.
    least_in_flight_ = largest_newly_acked + 1;
  }
  largest_previously_acked_ = largest_newly_acked;
  if (!packets_lost->empty()) {
    DCHECK_LT(largest_lost_, packets_lost->back().packet_number);
    largest_lost_ = packets_lost->back().packet_number;
  }
}

QuicTime GeneralLossAlgorithm::GetLossTimeout() const {
  return loss_detection_timeout_;
}

void GeneralLossAlgorithm::SpuriousRetransmitDetected(
    const QuicUnackedPacketMap& unacked_packets,
    QuicTime time,
    const RttStats& rtt_stats,
    QuicPacketNumber spurious_retransmission) {
  if (loss_type_ != kAdaptiveTime || reordering_shift_ == 0) {
    return;
  }
  // Calculate the extra time needed so this wouldn't have been declared lost.
  // Extra time needed is based on how long it's been since the spurious
  // retransmission was sent, because the SRTT and latest RTT may have changed.
  QuicTime::Delta extra_time_needed =
      time -
      unacked_packets.GetTransmissionInfo(spurious_retransmission).sent_time;
  // Increase the reordering fraction until enough time would be allowed.
  QuicTime::Delta max_rtt =
      std::max(rtt_stats.previous_srtt(), rtt_stats.latest_rtt());
  if (GetQuicReloadableFlag(quic_fix_adaptive_time_loss)) {
    QUIC_FLAG_COUNT(quic_reloadable_flag_quic_fix_adaptive_time_loss);
    while ((max_rtt >> reordering_shift_) <= extra_time_needed &&
           reordering_shift_ > 0) {
      --reordering_shift_;
    }
    return;
  }

  if (spurious_retransmission <= largest_sent_on_spurious_retransmit_) {
    return;
  }
  largest_sent_on_spurious_retransmit_ = unacked_packets.largest_sent_packet();
  QuicTime::Delta proposed_extra_time(QuicTime::Delta::Zero());
  do {
    proposed_extra_time = max_rtt >> reordering_shift_;
    --reordering_shift_;
  } while (proposed_extra_time < extra_time_needed && reordering_shift_ > 0);
}

}  // namespace quic
