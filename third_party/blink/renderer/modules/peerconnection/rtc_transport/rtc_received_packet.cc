// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_received_packet.h"

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/array_buffer_util.h"

namespace blink {

uint64_t RtcReceivedPacket::payloadByteLength() const {
  return data_.size();
}

void RtcReceivedPacket::copyPayloadTo(
    const AllowSharedBufferSource* destination,
    ExceptionState& exception_state) {
  // Validate destination buffer.
  auto dest_wrapper = RtcTransportBufferSourceAsByteSpan(*destination);
  if (dest_wrapper.size() < data_.size()) {
    exception_state.ThrowTypeError("destination is not large enough.");
    return;
  }

  // Copy data.
  dest_wrapper.copy_prefix_from(data_);
}

void RtcReceivedPacket::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
