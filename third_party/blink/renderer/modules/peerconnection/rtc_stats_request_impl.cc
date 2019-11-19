/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_request_impl.h"

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"

namespace blink {

RTCStatsRequestImpl::RTCStatsRequestImpl(ExecutionContext* context,
                                         RTCPeerConnection* requester,
                                         V8RTCStatsCallback* callback,
                                         MediaStreamTrack* selector)
    : ContextLifecycleObserver(context),
      success_callback_(callback),
      component_(selector ? selector->Component() : nullptr),
      requester_(requester) {
  DCHECK(requester_);
}

RTCStatsRequestImpl::~RTCStatsRequestImpl() = default;

RTCStatsResponseBase* RTCStatsRequestImpl::CreateResponse() {
  return MakeGarbageCollected<RTCStatsResponse>();
}

bool RTCStatsRequestImpl::HasSelector() {
  return component_;
}

MediaStreamComponent* RTCStatsRequestImpl::Component() {
  return component_;
}

void RTCStatsRequestImpl::RequestSucceeded(RTCStatsResponseBase* response) {
  bool should_fire_callback =
      requester_ ? requester_->ShouldFireGetStatsCallback() : false;
  if (should_fire_callback && success_callback_) {
    success_callback_->InvokeAndReportException(
        nullptr, static_cast<RTCStatsResponse*>(response));
  }
  Clear();
}

void RTCStatsRequestImpl::ContextDestroyed(ExecutionContext*) {
  Clear();
}

void RTCStatsRequestImpl::Clear() {
  success_callback_.Clear();
  requester_.Clear();
}

void RTCStatsRequestImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(success_callback_);
  visitor->Trace(component_);
  visitor->Trace(requester_);
  RTCStatsRequest::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
