/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_request_impl.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_session_description_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"

namespace blink {

RTCSessionDescriptionRequestImpl* RTCSessionDescriptionRequestImpl::Create(
    ExecutionContext* context,
    RTCPeerConnection* requester,
    V8RTCSessionDescriptionCallback* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback) {
  return MakeGarbageCollected<RTCSessionDescriptionRequestImpl>(
      context, requester, success_callback, error_callback);
}

RTCSessionDescriptionRequestImpl::RTCSessionDescriptionRequestImpl(
    ExecutionContext* context,
    RTCPeerConnection* requester,
    V8RTCSessionDescriptionCallback* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback)
    : ExecutionContextLifecycleObserver(context),
      success_callback_(success_callback),
      error_callback_(error_callback),
      requester_(requester) {
  DCHECK(requester_);
}

RTCSessionDescriptionRequestImpl::~RTCSessionDescriptionRequestImpl() = default;

void RTCSessionDescriptionRequestImpl::RequestSucceeded(
    RTCSessionDescriptionPlatform* description_platform) {
  bool should_fire_callback =
      requester_ ? requester_->ShouldFireDefaultCallbacks() : false;
  if (should_fire_callback && success_callback_) {
    RTCSessionDescriptionInit* description =
        RTCSessionDescriptionInit::Create();
    if (description_platform->GetType())
      description->setType(description_platform->GetType());
    description->setSdp(description_platform->Sdp());

    requester_->NoteSdpCreated(*description);
    success_callback_->InvokeAndReportException(nullptr, description);
  }
  Clear();
}

void RTCSessionDescriptionRequestImpl::RequestFailed(
    const webrtc::RTCError& error) {
  bool should_fire_callback =
      requester_ ? requester_->ShouldFireDefaultCallbacks() : false;
  if (should_fire_callback && error_callback_) {
    error_callback_->InvokeAndReportException(
        nullptr, CreateDOMExceptionFromRTCError(error));
  }
  Clear();
}

void RTCSessionDescriptionRequestImpl::ContextDestroyed() {
  Clear();
}

void RTCSessionDescriptionRequestImpl::Clear() {
  success_callback_.Clear();
  error_callback_.Clear();
  requester_.Clear();
}

void RTCSessionDescriptionRequestImpl::Trace(Visitor* visitor) const {
  visitor->Trace(success_callback_);
  visitor->Trace(error_callback_);
  visitor->Trace(requester_);
  RTCSessionDescriptionRequest::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
