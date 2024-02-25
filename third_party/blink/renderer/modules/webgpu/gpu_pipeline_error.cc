// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_error.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_pipeline_error_init.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
GPUPipelineError* GPUPipelineError::Create(
    String message,
    const GPUPipelineErrorInit* options) {
  // For consistency with `new DOMException()`, we don't AttachStackProperty.
  return MakeGarbageCollected<GPUPipelineError>(std::move(message),
                                                options->reason().AsEnum());
}

// static
v8::Local<v8::Value> GPUPipelineError::Create(
    v8::Isolate* isolate,
    const String& message,
    V8GPUPipelineErrorReason::Enum reason) {
  auto* exception = MakeGarbageCollected<GPUPipelineError>(message, reason);
  return V8ThrowDOMException::AttachStackProperty(isolate, exception);
}

GPUPipelineError::GPUPipelineError(const String& message,
                                   V8GPUPipelineErrorReason::Enum reason)
    : DOMException(DOMExceptionCode::kGPUPipelineError, message),
      reason_(reason) {}

V8GPUPipelineErrorReason GPUPipelineError::reason() const {
  return V8GPUPipelineErrorReason(reason_);
}

}  // namespace blink
