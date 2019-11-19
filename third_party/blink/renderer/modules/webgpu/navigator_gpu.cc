// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/navigator_gpu.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"

namespace blink {

// static
NavigatorGPU& NavigatorGPU::From(Navigator& navigator) {
  NavigatorGPU* supplement =
      Supplement<Navigator>::From<NavigatorGPU>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorGPU>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
GPU* NavigatorGPU::gpu(ScriptState* script_state, Navigator& navigator) {
  return NavigatorGPU::From(navigator).gpu(script_state);
}

GPU* NavigatorGPU::gpu(ScriptState* script_state) {
  if (!gpu_) {
    ExecutionContext* context = ExecutionContext::From(script_state);
    DCHECK(context);

    gpu_ = GPU::Create(*context);
  }
  return gpu_;
}

void NavigatorGPU::Trace(blink::Visitor* visitor) {
  visitor->Trace(gpu_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorGPU::NavigatorGPU(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

const char NavigatorGPU::kSupplementName[] = "NavigatorGPU";

}  // namespace blink
