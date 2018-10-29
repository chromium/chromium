// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_RST_STREAM_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_RST_STREAM_FRAME_H_

#include <ostream>

#include "net/third_party/quic/core/quic_error_codes.h"
#include "net/third_party/quic/core/quic_types.h"

namespace quic {

struct QUIC_EXPORT_PRIVATE QuicRstStreamFrame {
  QuicRstStreamFrame();
  QuicRstStreamFrame(QuicControlFrameId control_frame_id,
                     QuicStreamId stream_id,
                     QuicRstStreamErrorCode error_code,
                     QuicStreamOffset bytes_written);
  QuicRstStreamFrame(QuicControlFrameId control_frame_id,
                     QuicStreamId stream_id,
                     uint16_t ietf_error_code,
                     QuicStreamOffset bytes_written);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicRstStreamFrame& r);

  // A unique identifier of this control frame. 0 when this frame is received,
  // and non-zero when sent.
  QuicControlFrameId control_frame_id;

  QuicStreamId stream_id;

  // Caller must know whether IETF- or Google- QUIC is in use and
  // set the appropriate error code.
  union {
    QuicRstStreamErrorCode error_code;
    // In IETF QUIC the code is up to the app on top of quic, so is
    // more general than QuicRstStreamErrorCode allows.
    uint16_t ietf_error_code;
  };

  // Used to update flow control windows. On termination of a stream, both
  // endpoints must inform the peer of the number of bytes they have sent on
  // that stream. This can be done through normal termination (data packet with
  // FIN) or through a RST.
  QuicStreamOffset byte_offset;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_RST_STREAM_FRAME_H_
