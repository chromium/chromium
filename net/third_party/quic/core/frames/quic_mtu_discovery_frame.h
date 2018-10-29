// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_MTU_DISCOVERY_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_MTU_DISCOVERY_FRAME_H_

#include "net/third_party/quic/core/frames/quic_inlined_frame.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

// A path MTU discovery frame contains no payload and is serialized as a ping
// frame.
struct QUIC_EXPORT_PRIVATE QuicMtuDiscoveryFrame
    : public QuicInlinedFrame<QuicMtuDiscoveryFrame> {
  QuicMtuDiscoveryFrame() : QuicInlinedFrame(MTU_DISCOVERY_FRAME) {}
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_MTU_DISCOVERY_FRAME_H_
