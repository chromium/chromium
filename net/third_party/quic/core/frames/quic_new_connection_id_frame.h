// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_NEW_CONNECTION_ID_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_NEW_CONNECTION_ID_FRAME_H_

#include <ostream>

#include "net/third_party/quic/core/quic_error_codes.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_uint128.h"

namespace quic {

struct QUIC_EXPORT_PRIVATE QuicNewConnectionIdFrame {
  QuicNewConnectionIdFrame();
  QuicNewConnectionIdFrame(QuicControlFrameId control_frame_id,
                           QuicConnectionId connection_id,
                           QuicConnectionIdSequenceNumber sequence_number,
                           const QuicUint128 stateless_reset_token);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicNewConnectionIdFrame& frame);

  // A unique identifier of this control frame. 0 when this frame is received,
  // and non-zero when sent.
  QuicControlFrameId control_frame_id;
  QuicConnectionId connection_id;
  QuicConnectionIdSequenceNumber sequence_number;
  QuicUint128 stateless_reset_token;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_NEW_CONNECTION_ID_FRAME_H_
