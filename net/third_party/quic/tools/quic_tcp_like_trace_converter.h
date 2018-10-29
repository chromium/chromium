// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TOOLS_QUIC_TCP_LIKE_TRACE_CONVERTER_H_
#define NET_THIRD_PARTY_QUIC_TOOLS_QUIC_TCP_LIKE_TRACE_CONVERTER_H_

#include <vector>

#include "net/third_party/quic/core/frames/quic_stream_frame.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_containers.h"
#include "net/third_party/quic/platform/api/quic_interval.h"

namespace quic {

// This converter converts sent QUIC frames to connection byte offset (just like
// TCP byte sequence number).
class QuicTcpLikeTraceConverter {
 public:
  // StreamOffsetSegment stores a stream offset range which has contiguous
  // connection offset.
  struct StreamOffsetSegment {
    StreamOffsetSegment();
    StreamOffsetSegment(QuicStreamOffset stream_offset,
                        uint64_t connection_offset,
                        QuicByteCount data_length);

    QuicInterval<QuicStreamOffset> stream_data;
    uint64_t connection_offset;
  };

  QuicTcpLikeTraceConverter();
  QuicTcpLikeTraceConverter(const QuicTcpLikeTraceConverter& other) = delete;
  QuicTcpLikeTraceConverter(QuicTcpLikeTraceConverter&& other) = delete;

  ~QuicTcpLikeTraceConverter() {}

  // Called when a stream frame is sent. Returns the corresponding connection
  // offsets.
  QuicIntervalSet<uint64_t> OnStreamFrameSent(QuicStreamId stream_id,
                                              QuicStreamOffset offset,
                                              QuicByteCount data_length,
                                              bool fin);

  // Called when a control frame is sent. Returns the corresponding connection
  // offsets.
  QuicInterval<uint64_t> OnControlFrameSent(QuicControlFrameId control_frame_id,
                                            QuicByteCount control_frame_length);

 private:
  struct StreamInfo {
    StreamInfo();

    // Stores contiguous connection offset pieces.
    std::vector<StreamOffsetSegment> segments;
    // Indicates whether fin has been sent.
    bool fin;
  };

  QuicUnorderedMap<QuicStreamId, StreamInfo> streams_info_;
  QuicUnorderedMap<QuicControlFrameId, QuicInterval<uint64_t>>
      control_frames_info_;

  QuicControlFrameId largest_observed_control_frame_id_;

  uint64_t connection_offset_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TOOLS_QUIC_TCP_LIKE_TRACE_CONVERTER_H_
