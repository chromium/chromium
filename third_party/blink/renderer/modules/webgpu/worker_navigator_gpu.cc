// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/worker_navigator_gpu.h"

#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"

namespace blink {

// static
WorkerNavigatorGPU& WorkerNavigatorGPU::From(WorkerNavigator& navigator) {
  WorkerNavigatorGPU* supplement =
      Supplement<WorkerNavigator>::From<WorkerNavigatorGPU>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<WorkerNavigatorGPU>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
GPU* WorkerNavigatorGPU::gpu(ScriptState* script_state,
                             WorkerNavigator& navigator) {
  return WorkerNavigatorGPU::From(navigator).gpu(script_state);
}

GPU* WorkerNavigatorGPU::gpu(ScriptState* script_state) {
  if (!gpu_) {
    ExecutionContext* context = ExecutionContext::From(script_state);
    DCHECK(context);

    gpu_ = GPU::Create(*context);
  }
  return gpu_;
}

void WorkerNavigatorGPU::Trace(blink::Visitor* visitor) {
  visitor->Trace(gpu_);
  Supplement<WorkerNavigator>::Trace(visitor);
}

WorkerNavigatorGPU::WorkerNavigatorGPU(WorkerNavigator& navigator)
    : Supplement<WorkerNavigator>(navigator) {}

const char WorkerNavigatorGPU::kSupplementName[] = "WorkerNavigatorGPU";

}  // namespace blink
