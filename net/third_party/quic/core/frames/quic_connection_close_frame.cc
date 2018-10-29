// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_connection_close_frame.h"

namespace quic {

QuicConnectionCloseFrame::QuicConnectionCloseFrame()
    : error_code(QUIC_NO_ERROR), frame_type(0) {}

QuicConnectionCloseFrame::QuicConnectionCloseFrame(QuicErrorCode error_code,
                                                   QuicString error_details)
    : error_code(error_code),
      error_details(std::move(error_details)),
      frame_type(0) {}

QuicConnectionCloseFrame::QuicConnectionCloseFrame(
    QuicIetfTransportErrorCodes ietf_error_code,
    QuicString error_details,
    uint64_t frame_type)
    : ietf_error_code(ietf_error_code),
      error_details(std::move(error_details)),
      frame_type(frame_type) {}

std::ostream& operator<<(
    std::ostream& os,
    const QuicConnectionCloseFrame& connection_close_frame) {
  os << "{ error_code: " << connection_close_frame.error_code
     << ", error_details: '" << connection_close_frame.error_details
     << "', frame_type: " << connection_close_frame.frame_type << "}\n";
  return os;
}

}  // namespace quic
