// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_WEB_TRANSPORT_SEND_STATS_TRACKER_H_
#define NET_QUIC_WEB_TRANSPORT_SEND_STATS_TRACKER_H_

#include <optional>

#include "net/base/net_export.h"
#include "net/quic/web_transport_client.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_interval_set.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace net {

// Tracks application bytes consumed and acknowledged for WebTransport streams.
// Stream state is retained from registration until the owner unregisters the
// stream.
class NET_EXPORT_PRIVATE WebTransportSendStatsTracker {
 public:
  WebTransportSendStatsTracker();
  WebTransportSendStatsTracker(const WebTransportSendStatsTracker&) = delete;
  WebTransportSendStatsTracker& operator=(const WebTransportSendStatsTracker&) =
      delete;
  ~WebTransportSendStatsTracker();

  void RegisterStream(quic::QuicStreamId stream_id,
                      quic::QuicStreamOffset application_data_offset);
  void RecordSent(quic::QuicStreamId stream_id,
                  quic::QuicStreamOffset offset,
                  quic::QuicByteCount data_length);
  void RecordAcknowledged(quic::QuicStreamId stream_id,
                          quic::QuicStreamOffset offset,
                          quic::QuicByteCount data_length);
  void UnregisterStream(quic::QuicStreamId stream_id);
  std::optional<WebTransportSendStreamStats> GetStats(
      quic::QuicStreamId stream_id) const;

 private:
  struct StreamState {
    explicit StreamState(quic::QuicStreamOffset application_data_offset);
    StreamState(const StreamState&) = delete;
    StreamState& operator=(const StreamState&) = delete;
    StreamState(StreamState&&);
    StreamState& operator=(StreamState&&);
    ~StreamState();

    quic::QuicStreamOffset application_data_offset = 0;
    quic::QuicStreamOffset sent_end_offset = 0;
    quic::QuicStreamOffset acknowledged_prefix_end = 0;
    quic::QuicIntervalSet<quic::QuicStreamOffset> out_of_order_acknowledged;
  };

  absl::flat_hash_map<quic::QuicStreamId, StreamState> streams_;
};

}  // namespace net

#endif  // NET_QUIC_WEB_TRANSPORT_SEND_STATS_TRACKER_H_
