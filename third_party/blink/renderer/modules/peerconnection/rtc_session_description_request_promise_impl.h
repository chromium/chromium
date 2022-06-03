// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_SESSION_DESCRIPTION_REQUEST_PROMISE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_SESSION_DESCRIPTION_REQUEST_PROMISE_IMPL_H_

#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_enums.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_request.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class RTCPeerConnection;
class ScriptPromiseResolver;
class RTCSessionDescriptionPlatform;

// TODO(https://crbug.com/908468): Split up the operation-specific codepaths
// into separate request implementations and find a way to consolidate the
// shared code as to not repeat the majority of the implementations.
class RTCSessionDescriptionRequestPromiseImpl final
    : public RTCSessionDescriptionRequest {
 public:
  static RTCSessionDescriptionRequestPromiseImpl* Create(
      RTCCreateSessionDescriptionOperation,
      RTCPeerConnection*,
      ScriptPromiseResolver*,
      const char* interface_name,
      const char* property_name);

  RTCSessionDescriptionRequestPromiseImpl(RTCCreateSessionDescriptionOperation,
                                          RTCPeerConnection*,
                                          ScriptPromiseResolver*,
                                          const char* interface_name,
                                          const char* property_name);
  ~RTCSessionDescriptionRequestPromiseImpl() override;

  // RTCSessionDescriptionRequest
  void RequestSucceeded(RTCSessionDescriptionPlatform*) override;
  void RequestFailed(const webrtc::RTCError& error) override;

  void Trace(Visitor*) const override;

 private:
  void Clear();

  RTCCreateSessionDescriptionOperation operation_;
  Member<RTCPeerConnection> requester_;
  Member<ScriptPromiseResolver> resolver_;
  const char* interface_name_;
  const char* property_name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_SESSION_DESCRIPTION_REQUEST_PROMISE_IMPL_H_
