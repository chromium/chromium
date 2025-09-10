// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_received_packet.h"

namespace blink {

DOMArrayBuffer* RtcReceivedPacket::data() {
  return data_.Get();
}

void RtcReceivedPacket::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(data_);
}

}  // namespace blink
