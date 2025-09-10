// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport.h"

#include <optional>
#include <string_view>

#include "base/notimplemented.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {
RtcTransport* RtcTransport::Create(ExecutionContext* context,
                                   const RtcTransportConfig* config) {
  return MakeGarbageCollected<RtcTransport>(PassKey(), context, config);
}

RtcTransport::RtcTransport(PassKey,
                           ExecutionContext* context,
                           const RtcTransportConfig* config)
    : ExecutionContextLifecycleObserver(context) {}

RtcTransport::~RtcTransport() = default;

HeapVector<Member<RtcReceivedPacket>> RtcTransport::getReceivedPackets() {
  auto packets = HeapVector<Member<RtcReceivedPacket>>();
  std::swap(packets, received_packets_);
  return packets;
}

void RtcTransport::sendPackets(
    HeapVector<Member<RtcSendPacketParameters>> packets) {
  // TODO(crbug.com/443019066): Hook up to an actual transport.
  NOTIMPLEMENTED();
}

void RtcTransport::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(received_packets_);
}

}  // namespace blink
