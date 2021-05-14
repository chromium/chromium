// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_uncaptured_error_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_uncaptured_error_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuoutofmemoryerror_gpuvalidationerror.h"

namespace blink {

// static
GPUUncapturedErrorEvent* GPUUncapturedErrorEvent::Create(
    const AtomicString& type,
    const GPUUncapturedErrorEventInit* gpuUncapturedErrorEventInitDict) {
  return MakeGarbageCollected<GPUUncapturedErrorEvent>(
      type, gpuUncapturedErrorEventInitDict);
}

GPUUncapturedErrorEvent::GPUUncapturedErrorEvent(
    const AtomicString& type,
    const GPUUncapturedErrorEventInit* gpuUncapturedErrorEventInitDict)
    : Event(type, Bubbles::kNo, Cancelable::kYes) {
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  const auto& old_union = gpuUncapturedErrorEventInitDict->error();
  if (old_union.IsGPUOutOfMemoryError()) {
    error_ =
        MakeGarbageCollected<V8GPUError>(old_union.GetAsGPUOutOfMemoryError());
  } else if (old_union.IsGPUValidationError()) {
    error_ =
        MakeGarbageCollected<V8GPUError>(old_union.GetAsGPUValidationError());
  } else {
    NOTREACHED();
  }
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  error_ = gpuUncapturedErrorEventInitDict->error();
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
}

void GPUUncapturedErrorEvent::Trace(Visitor* visitor) const {
  visitor->Trace(error_);
  Event::Trace(visitor);
}

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
const V8GPUError* GPUUncapturedErrorEvent::error() const {
  return error_;
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
void GPUUncapturedErrorEvent::error(
    GPUOutOfMemoryErrorOrGPUValidationError& error) const {
  error = error_;
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

}  // namespace blink
