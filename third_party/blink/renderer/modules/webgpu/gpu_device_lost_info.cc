// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_device_lost_info.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

GPUDeviceLostInfo::GPUDeviceLostInfo(const WGPUDeviceLostReason reason,
                                     const String& message) {
  message_ = message;
  switch (reason) {
    case WGPUDeviceLostReason_Destroyed:
      reason_ = "destroyed";
      break;
    default:
      // Leave the reason as null indicating that it is not an expected scenario
      // for the device to be lost.
      break;
  }
}

const String& GPUDeviceLostInfo::message() const {
  return message_;
}

const ScriptValue GPUDeviceLostInfo::reason(ScriptState* script_state) const {
  if (reason_.IsNull()) {
    v8::Isolate* isolate = script_state->GetIsolate();
    return ScriptValue(isolate, v8::Undefined(isolate));
  }
  return ScriptValue::From(script_state, reason_);
}

}  // namespace blink
