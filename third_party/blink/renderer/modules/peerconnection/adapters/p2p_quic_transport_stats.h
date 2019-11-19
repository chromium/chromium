// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_STATS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_STATS_H_

#include "base/time/time.h"
#include "net/third_party/quiche/src/quic/core/quic_connection_stats.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// These are stats associated with the P2PQuicTransport object. These
// stats are mostly copied from quic::QuicConnectionStats, which includes
// stats for a quic::QuicConnection. These are connection level stats.
// Currently QUIC does not have stats at the stream level.
struct MODULES_EXPORT P2PQuicTransportStats {
  P2PQuicTransportStats();
  // Note: The following stats are ignored from the QuicConnectionStats:
  //       -packets_spuriously_retransmitted
  //       -bytes_spuriously_retransmitted
  //       -slowstart_packets_sent
  //       -slowstart_packets_lost
  //       -slowstart_bytes_lost
  //       -loss_timeout_count
  //       -tlp_count
  //       -rto_count
  //       -max_sequence_reordering
  //       -max_time_reordering_us
  //       -tcp_loss_events
  //       -connection_creation_time
  explicit P2PQuicTransportStats(const quic::QuicConnectionStats& stats);
  ~P2PQuicTransportStats() = default;

  base::TimeTicks timestamp;
  // |bytes_sent| includes retransmissions.
  uint64_t bytes_sent = 0;
  uint64_t packets_sent = 0;
  // |stream_bytes_sent| does not include retransmissions.
  uint64_t stream_bytes_sent = 0;
  uint64_t stream_bytes_received = 0;

  // These include version negotiation and public reset packets.
  //
  // |bytes_received| includes duplicate data for a stream.
  uint64_t bytes_received = 0;
  // |packets_received| includes packets which were not processable.
  uint64_t packets_received = 0;
  uint64_t packets_processed = 0;

  uint64_t bytes_retransmitted = 0;
  uint64_t packets_retransmitted = 0;
  // Number of packets abandoned as lost by the loss detection algorithm.
  uint64_t packets_lost = 0;
  uint64_t packets_dropped = 0;
  size_t crypto_retransmit_count = 0;
  // Minimum RTT in microseconds.
  uint64_t min_rtt_us = 0;
  // Smoothed RTT in microseconds.
  uint64_t srtt_us = 0;
  uint64_t max_packet_size = 0;
  uint64_t max_received_packet_size = 0;
  // Bits per second.
  uint64_t estimated_bandwidth_bps = 0;
  // Reordering stats for received packets.
  // Number of packets received out of packet number order.
  uint64_t packets_reordered = 0;
  uint64_t blocked_frames_received = 0;
  uint64_t blocked_frames_sent = 0;
  // Number of connectivity probing packets received by this connection.
  uint64_t connectivity_probing_packets_received = 0;

  // The following are stats not taken directly from QuicConnectionStats:
  uint32_t num_outgoing_streams_created = 0;
  uint32_t num_incoming_streams_created = 0;
  uint32_t num_datagrams_lost = 0;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_STATS_H_
