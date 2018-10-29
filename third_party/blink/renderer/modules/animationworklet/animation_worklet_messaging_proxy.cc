// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_messaging_proxy.h"

#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"
#include "third_party/blink/renderer/modules/worklet/animation_and_paint_worklet_thread.h"

namespace blink {

AnimationWorkletMessagingProxy::AnimationWorkletMessagingProxy(
    ExecutionContext* execution_context)
    : ThreadedWorkletMessagingProxy(execution_context) {}

void AnimationWorkletMessagingProxy::Trace(blink::Visitor* visitor) {
  ThreadedWorkletMessagingProxy::Trace(visitor);
}

AnimationWorkletMessagingProxy::~AnimationWorkletMessagingProxy() = default;

std::unique_ptr<WorkerThread>
AnimationWorkletMessagingProxy::CreateWorkerThread() {
  return AnimationAndPaintWorkletThread::CreateForAnimationWorklet(
      WorkletObjectProxy());
}

}  // namespace blink
