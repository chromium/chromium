// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_max_stream_id_frame.h"

namespace quic {

QuicMaxStreamIdFrame::QuicMaxStreamIdFrame()
    : QuicInlinedFrame(MAX_STREAM_ID_FRAME),
      control_frame_id(kInvalidControlFrameId) {}

QuicMaxStreamIdFrame::QuicMaxStreamIdFrame(QuicControlFrameId control_frame_id,
                                           QuicStreamId max_stream_id)
    : QuicInlinedFrame(MAX_STREAM_ID_FRAME),
      control_frame_id(control_frame_id),
      max_stream_id(max_stream_id) {}

std::ostream& operator<<(std::ostream& os, const QuicMaxStreamIdFrame& frame) {
  os << "{ control_frame_id: " << frame.control_frame_id
     << ", stream_id: " << frame.max_stream_id << " }\n";
  return os;
}

}  // namespace quic
