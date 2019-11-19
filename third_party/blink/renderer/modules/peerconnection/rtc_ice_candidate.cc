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

#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

RTCIceCandidate* RTCIceCandidate::Create(
    ExecutionContext* context,
    const RTCIceCandidateInit* candidate_init,
    ExceptionState& exception_state) {
  if (!candidate_init->hasSdpMid() && !candidate_init->hasSdpMLineIndex()) {
    exception_state.ThrowTypeError("sdpMid and sdpMLineIndex are both null.");
    return nullptr;
  }

  String sdp_mid;
  if (candidate_init->hasSdpMid())
    sdp_mid = candidate_init->sdpMid();

  base::Optional<uint16_t> sdp_m_line_index;
  if (candidate_init->hasSdpMLineIndex()) {
    sdp_m_line_index = candidate_init->sdpMLineIndex();
  } else {
    UseCounter::Count(context,
                      WebFeature::kRTCIceCandidateDefaultSdpMLineIndex);
  }

  return MakeGarbageCollected<RTCIceCandidate>(WebRTCICECandidate::Create(
      candidate_init->candidate(), sdp_mid, std::move(sdp_m_line_index),
      candidate_init->usernameFragment()));
}

RTCIceCandidate* RTCIceCandidate::Create(
    scoped_refptr<WebRTCICECandidate> web_candidate) {
  return MakeGarbageCollected<RTCIceCandidate>(std::move(web_candidate));
}

RTCIceCandidate::RTCIceCandidate(
    scoped_refptr<WebRTCICECandidate> web_candidate)
    : web_candidate_(std::move(web_candidate)) {}

String RTCIceCandidate::candidate() const {
  return web_candidate_->Candidate();
}

String RTCIceCandidate::sdpMid() const {
  return web_candidate_->SdpMid();
}

uint16_t RTCIceCandidate::sdpMLineIndex(bool& is_null) const {
  is_null = !web_candidate_->SdpMLineIndex().has_value();
  return is_null ? 0 : *web_candidate_->SdpMLineIndex();
}

scoped_refptr<WebRTCICECandidate> RTCIceCandidate::WebCandidate() const {
  return web_candidate_;
}

String RTCIceCandidate::foundation() const {
  return web_candidate_->Foundation();
}

String RTCIceCandidate::component() const {
  return web_candidate_->Component();
}

uint32_t RTCIceCandidate::priority(bool& is_null) const {
  is_null = !web_candidate_->Priority().has_value();
  return is_null ? 0 : *web_candidate_->Priority();
}

String RTCIceCandidate::address() const {
  return web_candidate_->Address();
}

String RTCIceCandidate::protocol() const {
  return web_candidate_->Protocol();
}

uint16_t RTCIceCandidate::port(bool& is_null) const {
  is_null = !web_candidate_->Port().has_value();
  return is_null ? 0 : *web_candidate_->Port();
}

String RTCIceCandidate::type() const {
  return web_candidate_->Type();
}

String RTCIceCandidate::tcpType() const {
  return web_candidate_->TcpType();
}

String RTCIceCandidate::relatedAddress() const {
  return web_candidate_->RelatedAddress();
}

uint16_t RTCIceCandidate::relatedPort(bool& is_null) const {
  is_null = !web_candidate_->RelatedPort().has_value();
  return is_null ? 0 : *web_candidate_->RelatedPort();
}

String RTCIceCandidate::usernameFragment() const {
  return web_candidate_->UsernameFragment();
}

ScriptValue RTCIceCandidate::toJSONForBinding(ScriptState* script_state) {
  V8ObjectBuilder result(script_state);
  result.AddString("candidate", web_candidate_->Candidate());
  result.AddString("sdpMid", web_candidate_->SdpMid());
  if (web_candidate_->SdpMLineIndex())
    result.AddNumber("sdpMLineIndex", *web_candidate_->SdpMLineIndex());
  return result.GetScriptValue();
}

}  // namespace blink
