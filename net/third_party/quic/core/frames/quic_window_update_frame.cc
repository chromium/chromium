// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_window_update_frame.h"
#include "net/third_party/quic/core/quic_constants.h"

namespace quic {

QuicWindowUpdateFrame::QuicWindowUpdateFrame()
    : control_frame_id(kInvalidControlFrameId) {}

QuicWindowUpdateFrame::QuicWindowUpdateFrame(
    QuicControlFrameId control_frame_id,
    QuicStreamId stream_id,
    QuicStreamOffset byte_offset)
    : control_frame_id(control_frame_id),
      stream_id(stream_id),
      byte_offset(byte_offset) {}

std::ostream& operator<<(std::ostream& os,
                         const QuicWindowUpdateFrame& window_update_frame) {
  os << "{ control_frame_id: " << window_update_frame.control_frame_id
     << ", stream_id: " << window_update_frame.stream_id
     << ", byte_offset: " << window_update_frame.byte_offset << " }\n";
  return os;
}

}  // namespace quic
