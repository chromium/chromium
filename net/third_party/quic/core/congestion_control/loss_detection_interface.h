// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The pure virtual class for send side loss detection algorithm.

#ifndef NET_THIRD_PARTY_QUIC_CORE_CONGESTION_CONTROL_LOSS_DETECTION_INTERFACE_H_
#define NET_THIRD_PARTY_QUIC_CORE_CONGESTION_CONTROL_LOSS_DETECTION_INTERFACE_H_

#include "net/third_party/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_time.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

class QuicUnackedPacketMap;
class RttStats;

class QUIC_EXPORT_PRIVATE LossDetectionInterface {
 public:
  virtual ~LossDetectionInterface() {}

  virtual LossDetectionType GetLossDetectionType() const = 0;

  // Called when a new ack arrives or the loss alarm fires.
  virtual void DetectLosses(const QuicUnackedPacketMap& unacked_packets,
                            QuicTime time,
                            const RttStats& rtt_stats,
                            QuicPacketNumber largest_newly_acked,
                            const AckedPacketVector& packets_acked,
                            LostPacketVector* packets_lost) = 0;

  // Get the time the LossDetectionAlgorithm wants to re-evaluate losses.
  // Returns QuicTime::Zero if no alarm needs to be set.
  virtual QuicTime GetLossTimeout() const = 0;

  // Called when a |spurious_retransmission| is detected.  The original
  // transmission must have been caused by DetectLosses.
  virtual void SpuriousRetransmitDetected(
      const QuicUnackedPacketMap& unacked_packets,
      QuicTime time,
      const RttStats& rtt_stats,
      QuicPacketNumber spurious_retransmission) = 0;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_CONGESTION_CONTROL_LOSS_DETECTION_INTERFACE_H_
