// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_VOID_REQUEST_PROMISE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_VOID_REQUEST_PROMISE_IMPL_H_

#include "base/optional.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_enums.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScriptPromiseResolver;
class RTCPeerConnection;

// TODO(https://crbug.com/908468): Split up the operation-specific codepaths
// into separate request implementations and find a way to consolidate the
// shared code as to not repeat the majority of the implementations.
class RTCVoidRequestPromiseImpl final : public RTCVoidRequest {
 public:
  RTCVoidRequestPromiseImpl(base::Optional<RTCSetSessionDescriptionOperation>,
                            RTCPeerConnection*,
                            ScriptPromiseResolver*,
                            const char* interface_name,
                            const char* property_name);
  ~RTCVoidRequestPromiseImpl() override;

  // RTCVoidRequest
  void RequestSucceeded() override;
  void RequestFailed(const webrtc::RTCError&) override;

  void Trace(blink::Visitor*) override;

 private:
  void Clear();

  base::Optional<RTCSetSessionDescriptionOperation> operation_;
  Member<RTCPeerConnection> requester_;
  Member<ScriptPromiseResolver> resolver_;
  const char* interface_name_;
  const char* property_name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_VOID_REQUEST_PROMISE_IMPL_H_
