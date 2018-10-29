// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_STOP_SENDING_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_STOP_SENDING_FRAME_H_

#include <ostream>

#include "net/third_party/quic/core/quic_error_codes.h"
#include "net/third_party/quic/core/quic_types.h"

namespace quic {

struct QUIC_EXPORT_PRIVATE QuicStopSendingFrame {
  QuicStopSendingFrame();
  QuicStopSendingFrame(QuicControlFrameId control_frame_id,
                       QuicStreamId stream_id,
                       uint16_t application_error_code);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicStopSendingFrame& frame);

  // A unique identifier of this control frame. 0 when this frame is received,
  // and non-zero when sent.
  QuicControlFrameId control_frame_id;
  QuicStreamId stream_id;
  QuicApplicationErrorCode application_error_code;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_STOP_SENDING_FRAME_H_
