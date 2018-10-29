// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_STREAM_ID_BLOCKED_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_STREAM_ID_BLOCKED_FRAME_H_

#include <ostream>

#include "net/third_party/quic/core/frames/quic_inlined_frame.h"
#include "net/third_party/quic/core/quic_constants.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

// IETF format STREAM_ID_BLOCKED frame.
// The sender uses this to inform the peer that the sender wished to
// open a new stream but was blocked from doing so due due to the
// maximum stream ID limit set by the peer (via a MAX_STREAM_ID frame)
struct QUIC_EXPORT_PRIVATE QuicStreamIdBlockedFrame
    : public QuicInlinedFrame<QuicStreamIdBlockedFrame> {
  QuicStreamIdBlockedFrame();
  QuicStreamIdBlockedFrame(QuicControlFrameId control_frame_id,
                           QuicStreamId stream_id);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicStreamIdBlockedFrame& frame);

  // A unique identifier of this control frame. 0 when this frame is received,
  // and non-zero when sent.
  QuicControlFrameId control_frame_id;

  QuicStreamId stream_id;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_STREAM_ID_BLOCKED_FRAME_H_
