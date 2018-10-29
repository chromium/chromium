// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_PADDING_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_PADDING_FRAME_H_

#include <cstdint>
#include <ostream>

#include "net/third_party/quic/core/frames/quic_inlined_frame.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

// A padding frame contains no payload.
struct QUIC_EXPORT_PRIVATE QuicPaddingFrame
    : public QuicInlinedFrame<QuicPaddingFrame> {
  QuicPaddingFrame() : QuicInlinedFrame(PADDING_FRAME), num_padding_bytes(-1) {}
  explicit QuicPaddingFrame(int num_padding_bytes)
      : QuicInlinedFrame(PADDING_FRAME), num_padding_bytes(num_padding_bytes) {}

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicPaddingFrame& s);

  // -1: full padding to the end of a max-sized packet
  // otherwise: only pad up to num_padding_bytes bytes
  int num_padding_bytes;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_PADDING_FRAME_H_
