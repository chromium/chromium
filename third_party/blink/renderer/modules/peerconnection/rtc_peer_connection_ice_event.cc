/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_peer_connection_ice_event_init.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"

namespace blink {

RTCPeerConnectionIceEvent* RTCPeerConnectionIceEvent::Create(
    RTCIceCandidate* candidate) {
  return MakeGarbageCollected<RTCPeerConnectionIceEvent>(candidate);
}

RTCPeerConnectionIceEvent* RTCPeerConnectionIceEvent::Create(
    const AtomicString& type,
    const RTCPeerConnectionIceEventInit* initializer) {
  return MakeGarbageCollected<RTCPeerConnectionIceEvent>(type, initializer);
}

RTCPeerConnectionIceEvent::RTCPeerConnectionIceEvent(RTCIceCandidate* candidate)
    : Event(event_type_names::kIcecandidate, Bubbles::kNo, Cancelable::kNo),
      candidate_(candidate) {}

// TODO(crbug.com/1070871): Use candidateOr(nullptr).
RTCPeerConnectionIceEvent::RTCPeerConnectionIceEvent(
    const AtomicString& type,
    const RTCPeerConnectionIceEventInit* initializer)
    : Event(type, initializer),
      candidate_(initializer->hasCandidate() ? initializer->candidate()
                                             : nullptr) {}

RTCPeerConnectionIceEvent::~RTCPeerConnectionIceEvent() = default;

RTCIceCandidate* RTCPeerConnectionIceEvent::candidate() const {
  return candidate_.Get();
}

const AtomicString& RTCPeerConnectionIceEvent::InterfaceName() const {
  return event_interface_names::kRTCPeerConnectionIceEvent;
}

void RTCPeerConnectionIceEvent::Trace(Visitor* visitor) const {
  visitor->Trace(candidate_);
  Event::Trace(visitor);
}

}  // namespace blink
