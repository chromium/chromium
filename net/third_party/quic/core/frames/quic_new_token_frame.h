// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_NEW_TOKEN_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_NEW_TOKEN_FRAME_H_

#include <memory>
#include <ostream>

#include "net/third_party/quic/core/quic_buffer_allocator.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

struct QUIC_EXPORT_PRIVATE QuicNewTokenFrame {
  QuicNewTokenFrame();
  QuicNewTokenFrame(QuicControlFrameId control_frame_id, QuicString token);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicNewTokenFrame& s);

  // A unique identifier of this control frame. 0 when this frame is received,
  // and non-zero when sent.
  QuicControlFrameId control_frame_id;

  QuicString token;
};
static_assert(sizeof(QuicNewTokenFrame) <= 64,
              "Keep the QuicNewTokenFrame size to a cacheline.");

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_NEW_TOKEN_FRAME_H_
