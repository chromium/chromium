// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/congestion_control/send_algorithm_interface.h"

#include "net/third_party/quic/core/congestion_control/bbr_sender.h"
#include "net/third_party/quic/core/congestion_control/tcp_cubic_sender_bytes.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_fallthrough.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_pcc_sender.h"

namespace quic {

class RttStats;

// Factory for send side congestion control algorithm.
SendAlgorithmInterface* SendAlgorithmInterface::Create(
    const QuicClock* clock,
    const RttStats* rtt_stats,
    const QuicUnackedPacketMap* unacked_packets,
    CongestionControlType congestion_control_type,
    QuicRandom* random,
    QuicConnectionStats* stats,
    QuicPacketCount initial_congestion_window) {
  QuicPacketCount max_congestion_window = kDefaultMaxCongestionWindowPackets;
  switch (congestion_control_type) {
    case kGoogCC:  // GoogCC is not supported by quic/core, fall back to BBR.
    case kBBR:
      return new BbrSender(rtt_stats, unacked_packets,
                           initial_congestion_window, max_congestion_window,
                           random);
    case kPCC:
      if (GetQuicReloadableFlag(quic_enable_pcc3)) {
        return CreatePccSender(clock, rtt_stats, unacked_packets, random, stats,
                               initial_congestion_window,
                               max_congestion_window);
      }
      // Fall back to CUBIC if PCC is disabled.
      QUIC_FALLTHROUGH_INTENDED;
    case kCubicBytes:
      return new TcpCubicSenderBytes(
          clock, rtt_stats, false /* don't use Reno */,
          initial_congestion_window, max_congestion_window, stats);
    case kRenoBytes:
      return new TcpCubicSenderBytes(clock, rtt_stats, true /* use Reno */,
                                     initial_congestion_window,
                                     max_congestion_window, stats);
  }
  return nullptr;
}

}  // namespace quic
