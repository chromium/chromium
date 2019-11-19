// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_PCC_SENDER_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_PCC_SENDER_IMPL_H_

#include "net/third_party/quiche/src/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/third_party/quiche/src/quic/core/congestion_control/tcp_cubic_sender_bytes.h"

namespace quic {

// Interface for creating a PCC SendAlgorithmInterface.  For chromium,
// this implementation is currently a stub that passes through to
// cubic bytes.
SendAlgorithmInterface* CreatePccSenderImpl(
    const QuicClock* clock,
    const RttStats* rtt_stats,
    const QuicUnackedPacketMap* unacked_packets,
    QuicRandom* random,
    QuicConnectionStats* stats,
    QuicPacketCount initial_congestion_window,
    QuicPacketCount max_congestion_window) {
  return new TcpCubicSenderBytes(clock, rtt_stats, false /* don't use Reno */,
                                 initial_congestion_window,
                                 max_congestion_window, stats);
}

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_PCC_SENDER_IMPL_H_
