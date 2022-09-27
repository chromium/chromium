// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_request_promise_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"

namespace blink {

RTCSessionDescriptionRequestPromiseImpl*
RTCSessionDescriptionRequestPromiseImpl::Create(RTCPeerConnection* requester,
                                                ScriptPromiseResolver* resolver,
                                                const char* interface_name,
                                                const char* property_name) {
  return MakeGarbageCollected<RTCSessionDescriptionRequestPromiseImpl>(
      requester, resolver, interface_name, property_name);
}

RTCSessionDescriptionRequestPromiseImpl::
    RTCSessionDescriptionRequestPromiseImpl(RTCPeerConnection* requester,
                                            ScriptPromiseResolver* resolver,
                                            const char* interface_name,
                                            const char* property_name)
    : requester_(requester),
      resolver_(resolver),
      interface_name_(interface_name),
      property_name_(property_name) {
  DCHECK(requester_);
  DCHECK(resolver_);
}

RTCSessionDescriptionRequestPromiseImpl::
    ~RTCSessionDescriptionRequestPromiseImpl() = default;

void RTCSessionDescriptionRequestPromiseImpl::RequestSucceeded(
    RTCSessionDescriptionPlatform* platform_session_description) {
  if (requester_ && requester_->ShouldFireDefaultCallbacks()) {
    auto* description =
        RTCSessionDescription::Create(platform_session_description);
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
    ScriptState::Scope scope(resolver_->GetScriptState());
    ExceptionState exception_state(resolver_->GetScriptState()->GetIsolate(),
                                   ExceptionState::kExecutionContext,
                                   interface_name_, property_name_);
    ThrowExceptionFromRTCError(error, exception_state);
    resolver_->Reject(exception_state);
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
