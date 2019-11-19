// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_NAVIGATOR_GPU_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_NAVIGATOR_GPU_H_

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;
class GPU;

class NavigatorGPU final : public GarbageCollected<NavigatorGPU>,
                           public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorGPU);

 public:
  static const char kSupplementName[];

  // Gets, or creates, NavigatorGPU supplement on Navigator.
  // See platform/Supplementable.h
  static NavigatorGPU& From(Navigator&);

  static GPU* gpu(ScriptState* script_state, Navigator&);
  GPU* gpu(ScriptState* script_state);

  explicit NavigatorGPU(Navigator&);

  void Trace(blink::Visitor*) override;

 private:
  Member<GPU> gpu_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorGPU);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_NAVIGATOR_GPU_H_
