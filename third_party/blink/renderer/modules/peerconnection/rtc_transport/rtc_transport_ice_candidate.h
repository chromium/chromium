// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_ICE_CANDIDATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_ICE_CANDIDATE_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_candidate_type.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_ice_candidate.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MODULES_EXPORT RtcTransportIceCandidate final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  RtcTransportIceCandidate(const String& username_fragment,
                           const String& password,
                           const String& address,
                           uint16_t port,
                           V8RTCIceCandidateType type);
  ~RtcTransportIceCandidate() override = default;

  const String& usernameFragment() const { return username_fragment_; }
  const String& password() const { return password_; }
  const String& address() const { return address_; }
  uint16_t port() const { return port_; }
  V8RTCIceCandidateType type() const { return type_; }

  v8::Local<v8::Object> toJSON(ScriptState* script_state) const;

  void Trace(Visitor*) const override;

 private:
  const String username_fragment_;
  const String password_;
  const String address_;
  const uint16_t port_;
  const V8RTCIceCandidateType type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_ICE_CANDIDATE_H_
