// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_new_token_frame.h"
#include "net/third_party/quic/core/quic_constants.h"
#include "net/third_party/quic/platform/api/quic_logging.h"

namespace quic {

QuicNewTokenFrame::QuicNewTokenFrame()
    : control_frame_id(kInvalidControlFrameId) {}

QuicNewTokenFrame::QuicNewTokenFrame(QuicControlFrameId control_frame_id,
                                     QuicString token)
    : control_frame_id(control_frame_id), token(token) {}

std::ostream& operator<<(std::ostream& os, const QuicNewTokenFrame& s) {
  os << "{ control_frame_id: " << s.control_frame_id << ", token: " << s.token
     << " }\n";
  return os;
}

}  // namespace quic
