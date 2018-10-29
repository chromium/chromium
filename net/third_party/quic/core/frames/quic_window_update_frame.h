// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_WINDOW_UPDATE_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_WINDOW_UPDATE_FRAME_H_

#include <ostream>

#include "net/third_party/quic/core/quic_types.h"

namespace quic {

// Flow control updates per-stream and at the connection level.
// Based on SPDY's WINDOW_UPDATE frame, but uses an absolute byte offset rather
// than a window delta.
// TODO(rjshade): A possible future optimization is to make stream_id and
//                byte_offset variable length, similar to stream frames.
struct QUIC_EXPORT_PRIVATE QuicWindowUpdateFrame {
  QuicWindowUpdateFrame();
  QuicWindowUpdateFrame(QuicControlFrameId control_frame_id,
                        QuicStreamId stream_id,
                        QuicStreamOffset byte_offset);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicWindowUpdateFrame& w);

  // A unique identifier of this control frame. 0 when this frame is received,
  // and non-zero when sent.
  QuicControlFrameId control_frame_id;

  // The stream this frame applies to.  0 is a special case meaning the overall
  // connection rather than a specific stream.
  QuicStreamId stream_id;

  // Byte offset in the stream or connection. The receiver of this frame must
  // not send data which would result in this offset being exceeded.
  //
  // TODO(fkastenholz): Rename this to max_data and change the type to
  // QuicByteCount because the IETF defines this as the "maximum
  // amount of data that can be sent".
  QuicStreamOffset byte_offset;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_WINDOW_UPDATE_FRAME_H_
