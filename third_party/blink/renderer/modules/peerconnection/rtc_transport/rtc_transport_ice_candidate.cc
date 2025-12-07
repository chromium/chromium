// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_ice_candidate.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

RtcTransportIceCandidate::RtcTransportIceCandidate(
    const String& username_fragment,
    const String& password,
    const String& address,
    uint16_t port,
    V8RTCIceCandidateType type)
    : username_fragment_(username_fragment),
      password_(password),
      address_(address),
      port_(port),
      type_(type) {}

v8::Local<v8::Object> RtcTransportIceCandidate::toJSON(
    ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  builder.AddString("usernameFragment", usernameFragment());
  builder.AddString("password", password());
  builder.AddString("address", address());
  builder.AddNumber("port", port());
  builder.AddString("type", type().AsString());
  return builder.V8Object();
}

void RtcTransportIceCandidate::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
