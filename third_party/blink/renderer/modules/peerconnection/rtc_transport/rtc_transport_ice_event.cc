// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_ice_event.h"

#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

RtcTransportIceEvent::RtcTransportIceEvent(RtcTransportIceCandidate* candidate)
    : Event(event_type_names::kIcecandidate, Bubbles::kNo, Cancelable::kNo),
      candidate_(candidate) {}

RtcTransportIceCandidate* RtcTransportIceEvent::candidate() const {
  return candidate_.Get();
}

const AtomicString& RtcTransportIceEvent::InterfaceName() const {
  return event_interface_names::kRtcTransportIceEvent;
}

void RtcTransportIceEvent::Trace(Visitor* visitor) const {
  visitor->Trace(candidate_);
  Event::Trace(visitor);
}

}  // namespace blink
