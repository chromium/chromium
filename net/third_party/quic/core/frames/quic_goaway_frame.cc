// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_goaway_frame.h"
#include "net/third_party/quic/core/quic_constants.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

QuicGoAwayFrame::QuicGoAwayFrame()
    : control_frame_id(kInvalidControlFrameId),
      error_code(QUIC_NO_ERROR),
      last_good_stream_id(0) {}

QuicGoAwayFrame::QuicGoAwayFrame(QuicControlFrameId control_frame_id,
                                 QuicErrorCode error_code,
                                 QuicStreamId last_good_stream_id,
                                 const QuicString& reason)
    : control_frame_id(control_frame_id),
      error_code(error_code),
      last_good_stream_id(last_good_stream_id),
      reason_phrase(reason) {}

std::ostream& operator<<(std::ostream& os,
                         const QuicGoAwayFrame& goaway_frame) {
  os << "{ control_frame_id: " << goaway_frame.control_frame_id
     << ", error_code: " << goaway_frame.error_code
     << ", last_good_stream_id: " << goaway_frame.last_good_stream_id
     << ", reason_phrase: '" << goaway_frame.reason_phrase << "' }\n";
  return os;
}

}  // namespace quic
