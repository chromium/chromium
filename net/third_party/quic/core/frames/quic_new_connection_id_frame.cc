// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_new_connection_id_frame.h"
#include "net/third_party/quic/core/quic_constants.h"

namespace quic {

QuicNewConnectionIdFrame::QuicNewConnectionIdFrame()
    : control_frame_id(kInvalidControlFrameId),
      connection_id(0),
      sequence_number(0) {}

QuicNewConnectionIdFrame::QuicNewConnectionIdFrame(
    QuicControlFrameId control_frame_id,
    QuicConnectionId connection_id,
    QuicConnectionIdSequenceNumber sequence_number,
    const QuicUint128 stateless_reset_token)
    : control_frame_id(control_frame_id),
      connection_id(connection_id),
      sequence_number(sequence_number),
      stateless_reset_token(stateless_reset_token) {}

std::ostream& operator<<(std::ostream& os,
                         const QuicNewConnectionIdFrame& frame) {
  os << "{ control_frame_id: " << frame.control_frame_id
     << ", connection_id: " << frame.connection_id
     << ", sequence_number: " << frame.sequence_number << " }\n";
  return os;
}

}  // namespace quic
