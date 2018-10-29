// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/tools/quic_tcp_like_trace_converter.h"

#include "net/third_party/quic/core/quic_constants.h"

namespace quic {

QuicTcpLikeTraceConverter::QuicTcpLikeTraceConverter()
    : largest_observed_control_frame_id_(kInvalidControlFrameId),
      connection_offset_(0) {}

QuicTcpLikeTraceConverter::StreamOffsetSegment::StreamOffsetSegment()
    : connection_offset(0) {}

QuicTcpLikeTraceConverter::StreamOffsetSegment::StreamOffsetSegment(
    QuicStreamOffset stream_offset,
    uint64_t connection_offset,
    QuicByteCount data_length)
    : stream_data(stream_offset, stream_offset + data_length),
      connection_offset(connection_offset) {}

QuicTcpLikeTraceConverter::StreamInfo::StreamInfo() : fin(false) {}

QuicIntervalSet<uint64_t> QuicTcpLikeTraceConverter::OnStreamFrameSent(
    QuicStreamId stream_id,
    QuicStreamOffset offset,
    QuicByteCount data_length,
    bool fin) {
  QuicIntervalSet<uint64_t> connection_offsets;
  if (fin) {
    // Stream fin consumes a connection offset.
    ++data_length;
  }
  StreamInfo* stream_info =
      &streams_info_.emplace(stream_id, StreamInfo()).first->second;
  // Get connection offsets of retransmission data in this frame.
  for (const auto& segment : stream_info->segments) {
    QuicInterval<QuicStreamOffset> retransmission(offset, offset + data_length);
    retransmission.IntersectWith(segment.stream_data);
    if (retransmission.Empty()) {
      continue;
    }
    const uint64_t connection_offset = segment.connection_offset +
                                       retransmission.min() -
                                       segment.stream_data.min();
    connection_offsets.Add(connection_offset,
                           connection_offset + retransmission.Length());
  }

  if (stream_info->fin) {
    return connection_offsets;
  }

  // Get connection offsets of new data in this frame.
  QuicStreamOffset least_unsent_offset =
      stream_info->segments.empty()
          ? 0
          : stream_info->segments.back().stream_data.max();
  if (least_unsent_offset >= offset + data_length) {
    return connection_offsets;
  }
  // Ignore out-of-order stream data so that as connection offset increases,
  // stream offset increases.
  QuicStreamOffset new_data_offset = std::max(least_unsent_offset, offset);
  QuicByteCount new_data_length = offset + data_length - new_data_offset;
  connection_offsets.Add(connection_offset_,
                         connection_offset_ + new_data_length);
  if (!stream_info->segments.empty() &&
      new_data_offset == least_unsent_offset &&
      connection_offset_ ==
          stream_info->segments.back().connection_offset +
              stream_info->segments.back().stream_data.Length()) {
    // Extend the last segment if both stream and connection offsets are
    // contiguous.
    stream_info->segments.back().stream_data.SetMax(new_data_offset +
                                                    new_data_length);
  } else {
    stream_info->segments.emplace_back(new_data_offset, connection_offset_,
                                       new_data_length);
  }
  stream_info->fin = fin;
  connection_offset_ += new_data_length;

  return connection_offsets;
}

QuicInterval<uint64_t> QuicTcpLikeTraceConverter::OnControlFrameSent(
    QuicControlFrameId control_frame_id,
    QuicByteCount control_frame_length) {
  if (control_frame_id > largest_observed_control_frame_id_) {
    // New control frame.
    QuicInterval<uint64_t> connection_offset = QuicInterval<uint64_t>(
        connection_offset_, connection_offset_ + control_frame_length);
    connection_offset_ += control_frame_length;
    control_frames_info_[control_frame_id] = QuicInterval<uint64_t>(
        connection_offset_, connection_offset_ + control_frame_length);
    largest_observed_control_frame_id_ = control_frame_id;
    return connection_offset;
  }
  const auto iter = control_frames_info_.find(control_frame_id);
  if (iter == control_frames_info_.end()) {
    // Ignore out of order control frames.
    return {};
  }
  return iter->second;
}

}  // namespace quic
