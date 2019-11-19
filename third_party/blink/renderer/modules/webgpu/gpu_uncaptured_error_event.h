// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_UNCAPTURED_ERROR_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_UNCAPTURED_ERROR_EVENT_H_

#include "third_party/blink/renderer/bindings/modules/v8/gpu_out_of_memory_error_or_gpu_validation_error.h"
#include "third_party/blink/renderer/modules/event_modules.h"

namespace blink {

class GPUOutOfMemoryErrorOrGPUValidationError;
class GPUUncapturedErrorEventInit;

class GPUUncapturedErrorEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUUncapturedErrorEvent* Create(const AtomicString& type,
                                         const GPUUncapturedErrorEventInit*);
  GPUUncapturedErrorEvent(const AtomicString& type,
                          const GPUUncapturedErrorEventInit*);

  void Trace(Visitor*) override;

  // gpu_uncaptured_error_event.idl
  void error(GPUOutOfMemoryErrorOrGPUValidationError&) const;

 private:
  GPUOutOfMemoryErrorOrGPUValidationError error_;

  DISALLOW_COPY_AND_ASSIGN(GPUUncapturedErrorEvent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_UNCAPTURED_ERROR_EVENT_H_
