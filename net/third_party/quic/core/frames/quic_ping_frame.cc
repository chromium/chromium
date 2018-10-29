// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_ping_frame.h"

namespace quic {

QuicPingFrame::QuicPingFrame()
    : QuicInlinedFrame(PING_FRAME), control_frame_id(kInvalidControlFrameId) {}

QuicPingFrame::QuicPingFrame(QuicControlFrameId control_frame_id)
    : QuicInlinedFrame(PING_FRAME), control_frame_id(control_frame_id) {}

std::ostream& operator<<(std::ostream& os, const QuicPingFrame& ping_frame) {
  os << "{ control_frame_id: " << ping_frame.control_frame_id << " }\n";
  return os;
}

}  // namespace quic
