// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_path_response_frame.h"
#include "net/third_party/quic/core/quic_constants.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"

namespace quic {

QuicPathResponseFrame::QuicPathResponseFrame()
    : control_frame_id(kInvalidControlFrameId) {}

QuicPathResponseFrame::QuicPathResponseFrame(
    QuicControlFrameId control_frame_id,
    const QuicPathFrameBuffer& data_buff)
    : control_frame_id(control_frame_id) {
  memcpy(data_buffer.data(), data_buff.data(), data_buffer.size());
}

QuicPathResponseFrame::~QuicPathResponseFrame() {}

std::ostream& operator<<(std::ostream& os, const QuicPathResponseFrame& frame) {
  os << "{ control_frame_id: " << frame.control_frame_id
     << ", data: " << static_cast<unsigned>(frame.data_buffer[0]) << " "
     << static_cast<unsigned>(frame.data_buffer[1]) << " "
     << static_cast<unsigned>(frame.data_buffer[2]) << " "
     << static_cast<unsigned>(frame.data_buffer[3]) << " "
     << static_cast<unsigned>(frame.data_buffer[4]) << " "
     << static_cast<unsigned>(frame.data_buffer[5]) << " "
     << static_cast<unsigned>(frame.data_buffer[6]) << " "
     << static_cast<unsigned>(frame.data_buffer[7]) << " }\n";
  return os;
}

}  // namespace quic
