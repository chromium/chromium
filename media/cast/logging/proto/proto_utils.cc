// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/proto/proto_utils.h"

#include "base/notreached.h"

#define TO_PROTO_ENUM(enum)  \
  case enum:                 \
    return proto::enum

namespace media {
namespace cast {

proto::EventType ToProtoEventType(CastLoggingEvent event) {
  switch (event) {
    TO_PROTO_ENUM(UNKNOWN);
    TO_PROTO_ENUM(FRAME_CAPTURE_BEGIN);
    TO_PROTO_ENUM(FRAME_CAPTURE_END);
    TO_PROTO_ENUM(FRAME_ENCODED);
    TO_PROTO_ENUM(FRAME_ACK_RECEIVED);
    TO_PROTO_ENUM(FRAME_ACK_SENT);
    TO_PROTO_ENUM(FRAME_DECODED);
    TO_PROTO_ENUM(FRAME_PLAYOUT);
    TO_PROTO_ENUM(PACKET_SENT_TO_NETWORK);
    TO_PROTO_ENUM(PACKET_RETRANSMITTED);
    TO_PROTO_ENUM(PACKET_RTX_REJECTED);
    TO_PROTO_ENUM(PACKET_RECEIVED);
  }
  NOTREACHED();
  return proto::UNKNOWN;
}

}  // namespace cast
}  // namespace media
