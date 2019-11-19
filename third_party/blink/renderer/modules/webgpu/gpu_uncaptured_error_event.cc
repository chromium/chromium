// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_uncaptured_error_event.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_uncaptured_error_event_init.h"

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
  error_ = gpuUncapturedErrorEventInitDict->error();
}

void GPUUncapturedErrorEvent::Trace(Visitor* visitor) {
  visitor->Trace(error_);
  Event::Trace(visitor);
}

void GPUUncapturedErrorEvent::error(
    GPUOutOfMemoryErrorOrGPUValidationError& error) const {
  error = error_;
}

}  // namespace blink
