// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_FENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_FENCE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class ScriptPromiseResolver;
class ScriptState;

class GPUFence : public DawnObject<WGPUFence> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUFence* Create(GPUDevice* device, WGPUFence fence);
  explicit GPUFence(GPUDevice* device, WGPUFence fence);
  ~GPUFence() override;

  // gpu_fence.idl
  uint64_t getCompletedValue() const;
  ScriptPromise onCompletion(ScriptState* script_state, uint64_t value);

 private:
  void OnCompletionCallback(ScriptPromiseResolver* resolver,
                            WGPUFenceCompletionStatus status);

  DISALLOW_COPY_AND_ASSIGN(GPUFence);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_FENCE_H_
