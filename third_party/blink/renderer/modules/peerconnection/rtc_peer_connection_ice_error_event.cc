// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_error_event.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_peer_connection_ice_error_event_init.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

RTCPeerConnectionIceErrorEvent* RTCPeerConnectionIceErrorEvent::Create(
    const String& address,
    std::optional<uint16_t> port,
    const String& host_candidate,
    const String& url,
    int error_code,
    const String& txt) {
  DCHECK(error_code > 0 && error_code <= USHRT_MAX);
  return MakeGarbageCollected<RTCPeerConnectionIceErrorEvent>(
      address, port, host_candidate, url, static_cast<uint16_t>(error_code),
      txt);
}

RTCPeerConnectionIceErrorEvent* RTCPeerConnectionIceErrorEvent::Create(
    const AtomicString& type,
    const RTCPeerConnectionIceErrorEventInit* initializer) {
  return MakeGarbageCollected<RTCPeerConnectionIceErrorEvent>(type,
                                                              initializer);
}

RTCPeerConnectionIceErrorEvent::RTCPeerConnectionIceErrorEvent(
    const String& address,
    std::optional<uint16_t> port,
    const String& host_candidate,
    const String& url,
    uint16_t error_code,
    const String& error_text)
    : Event(event_type_names::kIcecandidateerror,
            Bubbles::kNo,
            Cancelable::kNo),
      address_(address),
      port_(port),
      host_candidate_(host_candidate),
      url_(url),
      error_code_(error_code),
      error_text_(error_text) {}

RTCPeerConnectionIceErrorEvent::RTCPeerConnectionIceErrorEvent(
    const AtomicString& type,
    const RTCPeerConnectionIceErrorEventInit* initializer)
    : Event(type, initializer), error_code_(initializer->errorCode()) {
  if (initializer->hasAddress())
    address_ = initializer->address();
  if (initializer->hasPort())
    port_ = initializer->port();
  if (initializer->hasHostCandidate())
    host_candidate_ = initializer->hostCandidate();
  if (initializer->hasUrl())
    url_ = initializer->url();
  if (initializer->hasErrorText())
    error_text_ = initializer->errorText();
}

RTCPeerConnectionIceErrorEvent::~RTCPeerConnectionIceErrorEvent() = default;

String RTCPeerConnectionIceErrorEvent::address() const {
  return address_;
}

std::optional<uint16_t> RTCPeerConnectionIceErrorEvent::port() const {
  return port_;
}

String RTCPeerConnectionIceErrorEvent::hostCandidate() const {
  return host_candidate_;
}

String RTCPeerConnectionIceErrorEvent::url() const {
  return url_;
}

uint16_t RTCPeerConnectionIceErrorEvent::errorCode() const {
  return error_code_;
}

String RTCPeerConnectionIceErrorEvent::errorText() const {
  return error_text_;
}

const AtomicString& RTCPeerConnectionIceErrorEvent::InterfaceName() const {
  return event_interface_names::kRTCPeerConnectionIceErrorEvent;
}

void RTCPeerConnectionIceErrorEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
