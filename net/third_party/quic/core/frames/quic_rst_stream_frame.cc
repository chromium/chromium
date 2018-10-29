// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_rst_stream_frame.h"
#include "net/third_party/quic/core/quic_constants.h"

namespace quic {

QuicRstStreamFrame::QuicRstStreamFrame()
    : control_frame_id(kInvalidControlFrameId),
      stream_id(0),
      error_code(QUIC_STREAM_NO_ERROR),
      byte_offset(0) {}

QuicRstStreamFrame::QuicRstStreamFrame(QuicControlFrameId control_frame_id,
                                       QuicStreamId stream_id,
                                       QuicRstStreamErrorCode error_code,
                                       QuicStreamOffset bytes_written)
    : control_frame_id(control_frame_id),
      stream_id(stream_id),
      error_code(error_code),
      byte_offset(bytes_written) {}

QuicRstStreamFrame::QuicRstStreamFrame(QuicControlFrameId control_frame_id,
                                       QuicStreamId stream_id,
                                       uint16_t ietf_error_code,
                                       QuicStreamOffset bytes_written)
    : control_frame_id(control_frame_id),
      stream_id(stream_id),
      ietf_error_code(ietf_error_code),
      byte_offset(bytes_written) {}

std::ostream& operator<<(std::ostream& os,
                         const QuicRstStreamFrame& rst_frame) {
  os << "{ control_frame_id: " << rst_frame.control_frame_id
     << ", stream_id: " << rst_frame.stream_id
     << ", error_code: " << rst_frame.error_code << " }\n";
  return os;
}

}  // namespace quic
