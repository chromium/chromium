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
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_candidate_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

RTCIceCandidate* RTCIceCandidate::Create(
    ExecutionContext* context,
    const RTCIceCandidateInit* candidate_init,
    ExceptionState& exception_state) {
  if (candidate_init->sdpMid().IsNull() &&
      !candidate_init->hasSdpMLineIndexNonNull()) {
    exception_state.ThrowTypeError("sdpMid and sdpMLineIndex are both null.");
    return nullptr;
  }

  String sdp_mid = candidate_init->sdpMid();

  std::optional<uint16_t> sdp_m_line_index;
  if (candidate_init->hasSdpMLineIndexNonNull()) {
    sdp_m_line_index = candidate_init->sdpMLineIndexNonNull();
  } else {
    UseCounter::Count(context,
                      WebFeature::kRTCIceCandidateDefaultSdpMLineIndex);
  }

  return MakeGarbageCollected<RTCIceCandidate>(
      MakeGarbageCollected<RTCIceCandidatePlatform>(
          candidate_init->candidate(), sdp_mid, std::move(sdp_m_line_index),
          candidate_init->usernameFragment(),
          /*url can not be reconstruncted*/ std::nullopt));
}

RTCIceCandidate* RTCIceCandidate::Create(
    RTCIceCandidatePlatform* platform_candidate) {
  return MakeGarbageCollected<RTCIceCandidate>(platform_candidate);
}

RTCIceCandidate::RTCIceCandidate(RTCIceCandidatePlatform* platform_candidate)
    : platform_candidate_(platform_candidate) {}

String RTCIceCandidate::candidate() const {
  return platform_candidate_->Candidate();
}

String RTCIceCandidate::sdpMid() const {
  return platform_candidate_->SdpMid();
}

std::optional<uint16_t> RTCIceCandidate::sdpMLineIndex() const {
  return platform_candidate_->SdpMLineIndex();
}

RTCIceCandidatePlatform* RTCIceCandidate::PlatformCandidate() const {
  return platform_candidate_.Get();
}

void RTCIceCandidate::Trace(Visitor* visitor) const {
  visitor->Trace(platform_candidate_);
  ScriptWrappable::Trace(visitor);
}

String RTCIceCandidate::foundation() const {
  return platform_candidate_->Foundation();
}

String RTCIceCandidate::component() const {
  return platform_candidate_->Component();
}

std::optional<uint32_t> RTCIceCandidate::priority() const {
  return platform_candidate_->Priority();
}

String RTCIceCandidate::address() const {
  return platform_candidate_->Address();
}

String RTCIceCandidate::protocol() const {
  return platform_candidate_->Protocol();
}

std::optional<uint16_t> RTCIceCandidate::port() const {
  return platform_candidate_->Port();
}

String RTCIceCandidate::type() const {
  return platform_candidate_->Type();
}

std::optional<String> RTCIceCandidate::tcpType() const {
  return platform_candidate_->TcpType();
}

String RTCIceCandidate::relatedAddress() const {
  return platform_candidate_->RelatedAddress();
}

std::optional<uint16_t> RTCIceCandidate::relatedPort() const {
  return platform_candidate_->RelatedPort();
}

String RTCIceCandidate::usernameFragment() const {
  return platform_candidate_->UsernameFragment();
}

std::optional<String> RTCIceCandidate::url() const {
  return platform_candidate_->Url();
}

std::optional<String> RTCIceCandidate::relayProtocol() const {
  return platform_candidate_->RelayProtocol();
}

ScriptValue RTCIceCandidate::toJSONForBinding(ScriptState* script_state) {
  V8ObjectBuilder result(script_state);
  result.AddString("candidate", platform_candidate_->Candidate());
  result.AddString("sdpMid", platform_candidate_->SdpMid());
  if (platform_candidate_->SdpMLineIndex())
    result.AddNumber("sdpMLineIndex", *platform_candidate_->SdpMLineIndex());
  result.AddString("usernameFragment", platform_candidate_->UsernameFragment());
  return result.GetScriptValue();
}

}  // namespace blink
