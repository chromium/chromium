// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/web_transport_send_stats_tracker.h"

#include <algorithm>
#include <cstdint>

#include "base/check.h"

namespace net {

WebTransportSendStatsTracker::WebTransportSendStatsTracker() = default;
WebTransportSendStatsTracker::~WebTransportSendStatsTracker() = default;

void WebTransportSendStatsTracker::RegisterStream(
    quic::QuicStreamId stream_id,
    quic::QuicStreamOffset application_data_offset) {
  const bool inserted =
      streams_.try_emplace(stream_id, application_data_offset).second;
  CHECK(inserted);
}

void WebTransportSendStatsTracker::RecordSent(quic::QuicStreamId stream_id,
                                              quic::QuicStreamOffset offset,
                                              quic::QuicByteCount data_length) {
  if (data_length == 0) {
    return;
  }

  const auto stream_it = streams_.find(stream_id);
  if (stream_it == streams_.end()) {
    return;
  }

  StreamState& state = stream_it->second;
  const quic::QuicStreamOffset end = offset + data_length;
  state.sent_end_offset = std::max(state.sent_end_offset, end);
}

void WebTransportSendStatsTracker::RecordAcknowledged(
    quic::QuicStreamId stream_id,
    quic::QuicStreamOffset offset,
    quic::QuicByteCount data_length) {
  if (data_length == 0) {
    return;
  }

  const auto stream_it = streams_.find(stream_id);
  if (stream_it == streams_.end()) {
    return;
  }

  StreamState& state = stream_it->second;
  const quic::QuicStreamOffset end = offset + data_length;
  if (end <= state.acknowledged_prefix_end) {
    return;
  }
  state.out_of_order_acknowledged.Add(
      std::max(offset, state.acknowledged_prefix_end), end);

  const auto acknowledged =
      state.out_of_order_acknowledged.Find(state.acknowledged_prefix_end);
  if (acknowledged == state.out_of_order_acknowledged.end()) {
    return;
  }

  state.acknowledged_prefix_end = acknowledged->max();
  // Retain only ranges beyond the contiguous prefix, bounding memory to
  // unresolved out-of-order acknowledgements.
  state.out_of_order_acknowledged.TrimLessThan(state.acknowledged_prefix_end);
}

void WebTransportSendStatsTracker::UnregisterStream(
    quic::QuicStreamId stream_id) {
  streams_.erase(stream_id);
}

std::optional<WebTransportSendStreamStats>
WebTransportSendStatsTracker::GetStats(quic::QuicStreamId stream_id) const {
  const auto stream_it = streams_.find(stream_id);
  if (stream_it == streams_.end()) {
    return std::nullopt;
  }

  const StreamState& state = stream_it->second;
  const uint64_t bytes_sent =
      state.sent_end_offset < state.application_data_offset
          ? 0
          : state.sent_end_offset - state.application_data_offset;
  const uint64_t bytes_acknowledged =
      std::min(bytes_sent,
               state.acknowledged_prefix_end - state.application_data_offset);

  return WebTransportSendStreamStats{
      .bytes_sent = bytes_sent,
      .bytes_acknowledged = bytes_acknowledged,
  };
}

WebTransportSendStatsTracker::StreamState::StreamState(
    quic::QuicStreamOffset application_data_offset)
    : application_data_offset(application_data_offset),
      acknowledged_prefix_end(application_data_offset) {}
WebTransportSendStatsTracker::StreamState::StreamState(StreamState&&) = default;
WebTransportSendStatsTracker::StreamState&
WebTransportSendStatsTracker::StreamState::operator=(StreamState&&) = default;
WebTransportSendStatsTracker::StreamState::~StreamState() = default;

}  // namespace net
