// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_void_request_script_promise_resolver_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"

namespace blink {

RTCVoidRequestScriptPromiseResolverImpl::
    RTCVoidRequestScriptPromiseResolverImpl(ScriptPromiseResolver* resolver,
                                            const char* interface_name,
                                            const char* property_name)
    : resolver_(resolver),
      interface_name_(interface_name),
      property_name_(property_name) {
  DCHECK(resolver_);
}

RTCVoidRequestScriptPromiseResolverImpl::
    ~RTCVoidRequestScriptPromiseResolverImpl() = default;

void RTCVoidRequestScriptPromiseResolverImpl::RequestSucceeded() {
  resolver_->Resolve();
}

void RTCVoidRequestScriptPromiseResolverImpl::RequestFailed(
    const webrtc::RTCError& error) {
  ScriptState::Scope scope(resolver_->GetScriptState());
  ExceptionState exception_state(resolver_->GetScriptState()->GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 interface_name_, property_name_);
  ThrowExceptionFromRTCError(error, exception_state);
  resolver_->Reject(exception_state);
}

void RTCVoidRequestScriptPromiseResolverImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  RTCVoidRequest::Trace(visitor);
}

}  // namespace blink
