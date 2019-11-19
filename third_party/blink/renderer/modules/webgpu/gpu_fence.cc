// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_fence.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_callback.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

// static
GPUFence* GPUFence::Create(GPUDevice* device, WGPUFence fence) {
  return MakeGarbageCollected<GPUFence>(device, fence);
}

GPUFence::GPUFence(GPUDevice* device, WGPUFence fence)
    : DawnObject<WGPUFence>(device, fence) {}

GPUFence::~GPUFence() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().fenceRelease(GetHandle());
}

uint64_t GPUFence::getCompletedValue() const {
  return GetProcs().fenceGetCompletedValue(GetHandle());
}

void GPUFence::OnCompletionCallback(ScriptPromiseResolver* resolver,
                                    WGPUFenceCompletionStatus status) {
  switch (status) {
    case WGPUFenceCompletionStatus_Success:
      resolver->Resolve();
      break;
    case WGPUFenceCompletionStatus_Error:
    case WGPUFenceCompletionStatus_Unknown:
    case WGPUFenceCompletionStatus_DeviceLost:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError));
      break;
    default:
      NOTREACHED();
  }
}

ScriptPromise GPUFence::onCompletion(ScriptState* script_state,
                                     uint64_t value) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auto* callback =
      BindDawnCallback(&GPUFence::OnCompletionCallback, WrapPersistent(this),
                       WrapPersistent(resolver));

  GetProcs().fenceOnCompletion(GetHandle(), value, callback->UnboundCallback(),
                               callback->AsUserdata());

  return promise;
}

}  // namespace blink
