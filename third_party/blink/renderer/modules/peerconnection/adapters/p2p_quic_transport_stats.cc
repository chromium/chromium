// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_stats.h"

namespace blink {

P2PQuicTransportStats::P2PQuicTransportStats()
    : timestamp(base::TimeTicks::Now()) {}

P2PQuicTransportStats::P2PQuicTransportStats(
    const quic::QuicConnectionStats& quic_stats)
    : timestamp(base::TimeTicks::Now()),
      bytes_sent(quic_stats.bytes_sent),
      packets_sent(quic_stats.packets_sent),
      stream_bytes_sent(quic_stats.stream_bytes_sent),
      stream_bytes_received(quic_stats.stream_bytes_received),
      bytes_received(quic_stats.bytes_received),
      packets_received(quic_stats.packets_received),
      packets_processed(quic_stats.packets_processed),
      bytes_retransmitted(quic_stats.bytes_retransmitted),
      packets_retransmitted(quic_stats.packets_retransmitted),
      packets_lost(quic_stats.packets_lost),
      packets_dropped(quic_stats.packets_dropped),
      crypto_retransmit_count(quic_stats.crypto_retransmit_count),
      min_rtt_us(quic_stats.min_rtt_us),
      srtt_us(quic_stats.srtt_us),
      max_packet_size(quic_stats.max_packet_size),
      max_received_packet_size(quic_stats.max_received_packet_size),
      estimated_bandwidth_bps(quic_stats.estimated_bandwidth.ToBitsPerSecond()),
      packets_reordered(quic_stats.packets_reordered),
      blocked_frames_received(quic_stats.blocked_frames_received),
      blocked_frames_sent(quic_stats.blocked_frames_sent),
      connectivity_probing_packets_received(
          quic_stats.num_connectivity_probing_received) {}
}  // namespace blink
