// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_void_request_promise_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"

namespace blink {

RTCVoidRequestPromiseImpl::RTCVoidRequestPromiseImpl(
    RTCPeerConnection* requester,
    ScriptPromiseResolver<IDLUndefined>* resolver)
    : requester_(requester), resolver_(resolver) {
  DCHECK(requester_);
  DCHECK(resolver_);
}

RTCVoidRequestPromiseImpl::~RTCVoidRequestPromiseImpl() = default;

void RTCVoidRequestPromiseImpl::RequestSucceeded() {
  if (requester_ && requester_->ShouldFireDefaultCallbacks()) {
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
    RejectPromiseFromRTCError(error, resolver_);
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

void RTCVoidRequestPromiseImpl::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  visitor->Trace(requester_);
  RTCVoidRequest::Trace(visitor);
}

}  // namespace blink
