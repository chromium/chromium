// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_request_promise_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_session_description_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"

namespace blink {

RTCSessionDescriptionRequestPromiseImpl*
RTCSessionDescriptionRequestPromiseImpl::Create(
    RTCPeerConnection* requester,
    ScriptPromiseResolver<RTCSessionDescriptionInit>* resolver) {
  return MakeGarbageCollected<RTCSessionDescriptionRequestPromiseImpl>(
      requester, resolver);
}

RTCSessionDescriptionRequestPromiseImpl::
    RTCSessionDescriptionRequestPromiseImpl(
        RTCPeerConnection* requester,
        ScriptPromiseResolver<RTCSessionDescriptionInit>* resolver)
    : requester_(requester), resolver_(resolver) {
  DCHECK(requester_);
  DCHECK(resolver_);
}

RTCSessionDescriptionRequestPromiseImpl::
    ~RTCSessionDescriptionRequestPromiseImpl() = default;

void RTCSessionDescriptionRequestPromiseImpl::RequestSucceeded(
    RTCSessionDescriptionPlatform* platform_session_description) {
  if (requester_ && requester_->ShouldFireDefaultCallbacks()) {
    auto* description = RTCSessionDescriptionInit::Create();
    description->setType(platform_session_description->GetType());
    description->setSdp(platform_session_description->Sdp());
    requester_->NoteSdpCreated(*description);
    resolver_->Resolve(description);
  } else {
    // This is needed to have the resolver release its internal resources
    // while leaving the associated promise pending as specified.
    resolver_->Detach();
  }

  Clear();
}

void RTCSessionDescriptionRequestPromiseImpl::RequestFailed(
    const webrtc::RTCError& error) {
  if (requester_ && requester_->ShouldFireDefaultCallbacks()) {
    RejectPromiseFromRTCError(error, resolver_);
  } else {
    // This is needed to have the resolver release its internal resources
    // while leaving the associated promise pending as specified.
    resolver_->Detach();
  }

  Clear();
}

void RTCSessionDescriptionRequestPromiseImpl::Clear() {
  requester_.Clear();
}

void RTCSessionDescriptionRequestPromiseImpl::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  visitor->Trace(requester_);
  RTCSessionDescriptionRequest::Trace(visitor);
}

}  // namespace blink
