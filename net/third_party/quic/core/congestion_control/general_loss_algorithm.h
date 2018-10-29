// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_CONGESTION_CONTROL_GENERAL_LOSS_ALGORITHM_H_
#define NET_THIRD_PARTY_QUIC_CORE_CONGESTION_CONTROL_GENERAL_LOSS_ALGORITHM_H_

#include <algorithm>
#include <map>

#include "base/macros.h"
#include "net/third_party/quic/core/congestion_control/loss_detection_interface.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_time.h"
#include "net/third_party/quic/core/quic_unacked_packet_map.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

// Class which can be configured to implement's TCP's approach of detecting loss
// when 3 nacks have been received for a packet or with a time threshold.
// Also implements TCP's early retransmit(RFC5827).
class QUIC_EXPORT_PRIVATE GeneralLossAlgorithm : public LossDetectionInterface {
 public:
  // TCP retransmits after 3 nacks.
  static const QuicPacketCount kNumberOfNacksBeforeRetransmission = 3;

  GeneralLossAlgorithm();
  explicit GeneralLossAlgorithm(LossDetectionType loss_type);
  GeneralLossAlgorithm(const GeneralLossAlgorithm&) = delete;
  GeneralLossAlgorithm& operator=(const GeneralLossAlgorithm&) = delete;
  ~GeneralLossAlgorithm() override {}

  LossDetectionType GetLossDetectionType() const override;

  // Switches the loss detection type to |loss_type| and resets the loss
  // algorithm.
  void SetLossDetectionType(LossDetectionType loss_type);

  // Uses |largest_acked| and time to decide when packets are lost.
  void DetectLosses(const QuicUnackedPacketMap& unacked_packets,
                    QuicTime time,
                    const RttStats& rtt_stats,
                    QuicPacketNumber largest_newly_acked,
                    const AckedPacketVector& packets_acked,
                    LostPacketVector* packets_lost) override;

  // Returns a non-zero value when the early retransmit timer is active.
  QuicTime GetLossTimeout() const override;

  // Increases the loss detection threshold for time loss detection.
  void SpuriousRetransmitDetected(
      const QuicUnackedPacketMap& unacked_packets,
      QuicTime time,
      const RttStats& rtt_stats,
      QuicPacketNumber spurious_retransmission) override;

  int reordering_shift() const { return reordering_shift_; }

 private:
  QuicTime loss_detection_timeout_;
  // Largest sent packet when a spurious retransmit is detected.
  // Prevents increasing the reordering threshold multiple times per epoch.
  // TODO(ianswett): Deprecate when
  // quic_reloadable_flag_quic_fix_adaptive_time_loss is deprecated.
  QuicPacketNumber largest_sent_on_spurious_retransmit_;
  LossDetectionType loss_type_;
  // Fraction of a max(SRTT, latest_rtt) to permit reordering before declaring
  // loss.  Fraction calculated by shifting max(SRTT, latest_rtt) to the right
  // by reordering_shift.
  int reordering_shift_;
  // The largest newly acked from the previous call to DetectLosses.
  QuicPacketNumber largest_previously_acked_;
  // The largest lost packet.
  // TODO(fayang): Remove this variable when deprecate
  // quic_reloadable_flag_quic_faster_detect_loss.
  QuicPacketNumber largest_lost_;
  // The least in flight packet. Loss detection should start from this. Please
  // note, least_in_flight_ could be largest packet ever sent + 1.
  QuicPacketNumber least_in_flight_;
  // Latched value of quic_reloadable_flag_quic_faster_detect_loss.
  const bool faster_detect_loss_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_CONGESTION_CONTROL_GENERAL_LOSS_ALGORITHM_H_
