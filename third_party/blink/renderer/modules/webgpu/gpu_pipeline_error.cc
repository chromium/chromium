// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_pipeline_error_init.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
GPUPipelineError* GPUPipelineError::Create(
    String message,
    const GPUPipelineErrorInit* options) {
  return MakeGarbageCollected<GPUPipelineError>(std::move(message),
                                                options->reason().AsEnum());
}

GPUPipelineError::GPUPipelineError(String message,
                                   V8GPUPipelineErrorReason::Enum reason)
    : DOMException(DOMExceptionCode::kGPUPipelineError, std::move(message)),
      reason_(reason) {}

V8GPUPipelineErrorReason GPUPipelineError::reason() const {
  return V8GPUPipelineErrorReason(reason_);
}

}  // namespace blink
