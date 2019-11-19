// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_void_request_promise_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"

namespace blink {

RTCVoidRequestPromiseImpl::RTCVoidRequestPromiseImpl(
    base::Optional<RTCSetSessionDescriptionOperation> operation,
    RTCPeerConnection* requester,
    ScriptPromiseResolver* resolver,
    const char* interface_name,
    const char* property_name)
    : operation_(std::move(operation)),
      requester_(requester),
      resolver_(resolver),
      interface_name_(interface_name),
      property_name_(property_name) {
  DCHECK(requester_);
  DCHECK(resolver_);
}

RTCVoidRequestPromiseImpl::~RTCVoidRequestPromiseImpl() = default;

void RTCVoidRequestPromiseImpl::RequestSucceeded() {
  if (requester_ && requester_->ShouldFireDefaultCallbacks()) {
    if (operation_)
      requester_->NoteVoidRequestCompleted(*operation_, true);
    resolver_->Resolve();
  } else {
    // This is needed to have the resolver release its internal resources
    // while leaving the associated promise pending as specified.
    resolver_->Detach();
  }

  Clear();
}

void RTCVoidRequestPromiseImpl::RequestFailed(const webrtc::RTCError& error) {
  if (requester_ && requester_->ShouldFireDefaultCallbacks()) {
    if (operation_)
      requester_->NoteVoidRequestCompleted(*operation_, false);
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

void RTCVoidRequestPromiseImpl::Clear() {
  requester_.Clear();
}

void RTCVoidRequestPromiseImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  visitor->Trace(requester_);
  RTCVoidRequest::Trace(visitor);
}

}  // namespace blink
