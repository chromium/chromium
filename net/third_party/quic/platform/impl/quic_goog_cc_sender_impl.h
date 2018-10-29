// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_GOOG_CC_SENDER_IMPL_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_GOOG_CC_SENDER_IMPL_H_

#include "net/third_party/quic/core/congestion_control/bbr_sender.h"
#include "net/third_party/quic/core/congestion_control/rtt_stats.h"
#include "net/third_party/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_connection_stats.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/core/quic_unacked_packet_map.h"
#include "net/third_party/quic/platform/api/quic_clock.h"

namespace quic {

// Implementation for creating a GoogCC SendAlgorithmInterface.
SendAlgorithmInterface* CreateGoogCcSenderImpl(
    const QuicClock* clock,
    const RttStats* rtt_stats,
    const QuicUnackedPacketMap* unacked_packets,
    QuicRandom* random,
    QuicConnectionStats* stats,
    QuicPacketCount initial_congestion_window,
    QuicPacketCount max_congestion_window) {
  // TODO(mellem):  Switch to GoogCCSender once it exists.
  return new BbrSender(rtt_stats, unacked_packets, initial_congestion_window,
                       max_congestion_window, random);
}

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_GOOG_CC_SENDER_IMPL_H_
