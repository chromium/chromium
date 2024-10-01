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

#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_session_description_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

RTCSessionDescription* RTCSessionDescription::Create(
    ExecutionContext* context,
    const RTCSessionDescriptionInit* description_init_dict) {
  String type;
  if (description_init_dict->hasType())
    type = String(description_init_dict->type());
  else
    UseCounter::Count(context, WebFeature::kRTCSessionDescriptionInitNoType);

  String sdp;
  if (description_init_dict->hasSdp())
    sdp = description_init_dict->sdp();
  else
    UseCounter::Count(context, WebFeature::kRTCSessionDescriptionInitNoSdp);

  return MakeGarbageCollected<RTCSessionDescription>(
      MakeGarbageCollected<RTCSessionDescriptionPlatform>(type, sdp));
}

RTCSessionDescription* RTCSessionDescription::Create(
    RTCSessionDescriptionPlatform* platform_session_description) {
  return MakeGarbageCollected<RTCSessionDescription>(
      platform_session_description);
}

RTCSessionDescription::RTCSessionDescription(
    RTCSessionDescriptionPlatform* platform_session_description)
    : platform_session_description_(platform_session_description) {}

String RTCSessionDescription::type() const {
  return platform_session_description_->GetType();
}

void RTCSessionDescription::setType(std::optional<V8RTCSdpType> type) {
  platform_session_description_->SetType(
      type.has_value() ? type.value().AsString() : String());
}

String RTCSessionDescription::sdp() const {
  return platform_session_description_->Sdp();
}

void RTCSessionDescription::setSdp(const String& sdp) {
  platform_session_description_->SetSdp(sdp);
}

ScriptValue RTCSessionDescription::toJSONForBinding(ScriptState* script_state) {
  V8ObjectBuilder result(script_state);
  result.AddStringOrNull("type", type());
  result.AddStringOrNull("sdp", sdp());
  return result.GetScriptValue();
}

RTCSessionDescriptionPlatform* RTCSessionDescription::WebSessionDescription() {
  return platform_session_description_.Get();
}

void RTCSessionDescription::Trace(Visitor* visitor) const {
  visitor->Trace(platform_session_description_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
