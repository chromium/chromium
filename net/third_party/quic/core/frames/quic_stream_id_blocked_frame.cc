// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_stream_id_blocked_frame.h"

namespace quic {

QuicStreamIdBlockedFrame::QuicStreamIdBlockedFrame()
    : QuicInlinedFrame(STREAM_ID_BLOCKED_FRAME),
      control_frame_id(kInvalidControlFrameId) {}

QuicStreamIdBlockedFrame::QuicStreamIdBlockedFrame(
    QuicControlFrameId control_frame_id,
    QuicStreamId stream_id)
    : QuicInlinedFrame(STREAM_ID_BLOCKED_FRAME),
      control_frame_id(control_frame_id),
      stream_id(stream_id) {}

std::ostream& operator<<(std::ostream& os,
                         const QuicStreamIdBlockedFrame& frame) {
  os << "{ control_frame_id: " << frame.control_frame_id
     << ", stream id: " << frame.stream_id << " }\n";
  return os;
}

}  // namespace quic
