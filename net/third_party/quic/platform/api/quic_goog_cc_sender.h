// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_GOOG_CC_SENDER_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_GOOG_CC_SENDER_H_

#include "net/third_party/quic/core/congestion_control/rtt_stats.h"
#include "net/third_party/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_connection_stats.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/core/quic_unacked_packet_map.h"
#include "net/third_party/quic/platform/api/quic_clock.h"
#include "net/third_party/quic/platform/impl/quic_goog_cc_sender_impl.h"

namespace quic {

// Interface for creating a GoogCC SendAlgorithmInterface.
SendAlgorithmInterface* CreateGoogCcSender(
    const QuicClock* clock,
    const RttStats* rtt_stats,
    const QuicUnackedPacketMap* unacked_packets,
    QuicRandom* random,
    QuicConnectionStats* stats,
    QuicPacketCount initial_congestion_window,
    QuicPacketCount max_congestion_window) {
  return CreateGoogCcSenderImpl(clock, rtt_stats, unacked_packets, random,
                                stats, initial_congestion_window,
                                max_congestion_window);
}

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_GOOG_CC_SENDER_H_
