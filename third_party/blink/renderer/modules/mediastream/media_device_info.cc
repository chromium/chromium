/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/mediastream/media_device_info.h"

#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

MediaDeviceInfo::MediaDeviceInfo(const String& device_id,
                                 const String& label,
                                 const String& group_id,
                                 mojom::blink::MediaDeviceType device_type)
    : device_id_(device_id),
      label_(label),
      group_id_(group_id),
      device_type_(device_type) {}

String MediaDeviceInfo::deviceId() const {
  return device_id_;
}

String MediaDeviceInfo::kind() const {
  switch (device_type_) {
    case mojom::blink::MediaDeviceType::kMediaAudioInput:
      return "audioinput";
    case mojom::blink::MediaDeviceType::kMediaAudioOutput:
      return "audiooutput";
    case mojom::blink::MediaDeviceType::kMediaVideoInput:
      return "videoinput";
    default:
      NOTREACHED_IN_MIGRATION();
      return String();
  }
}

String MediaDeviceInfo::label() const {
  return label_;
}

String MediaDeviceInfo::groupId() const {
  return group_id_;
}

mojom::blink::MediaDeviceType MediaDeviceInfo::DeviceType() const {
  return device_type_;
}

ScriptValue MediaDeviceInfo::toJSONForBinding(ScriptState* script_state) {
  V8ObjectBuilder result(script_state);
  result.AddString("deviceId", deviceId());
  result.AddString("kind", kind());
  result.AddString("label", label());
  result.AddString("groupId", groupId());
  return result.GetScriptValue();
}

}  // namespace blink
