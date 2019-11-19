// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_WORKER_NAVIGATOR_GPU_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_WORKER_NAVIGATOR_GPU_H_

#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class GPU;
class WorkerNavigator;

class WorkerNavigatorGPU final : public GarbageCollected<WorkerNavigatorGPU>,
                                 public Supplement<WorkerNavigator> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerNavigatorGPU);

 public:
  static const char kSupplementName[];

  // Gets, or creates, WorkerNavigatorGPU supplement on WorkerNavigator.
  // See platform/Supplementable.h
  static WorkerNavigatorGPU& From(WorkerNavigator&);

  static GPU* gpu(ScriptState* script_state, WorkerNavigator&);
  GPU* gpu(ScriptState* script_state);

  explicit WorkerNavigatorGPU(WorkerNavigator&);

  void Trace(blink::Visitor*) override;

 private:
  Member<GPU> gpu_;

  DISALLOW_COPY_AND_ASSIGN(WorkerNavigatorGPU);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_WORKER_NAVIGATOR_GPU_H_
