// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_messaging_proxy.h"

#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"
#include "third_party/blink/renderer/modules/worklet/animation_and_paint_worklet_thread.h"

namespace blink {

PaintWorkletMessagingProxy::PaintWorkletMessagingProxy(
    ExecutionContext* execution_context)
    : ThreadedWorkletMessagingProxy(execution_context) {}

void PaintWorkletMessagingProxy::Trace(Visitor* visitor) const {
  ThreadedWorkletMessagingProxy::Trace(visitor);
}

PaintWorkletMessagingProxy::~PaintWorkletMessagingProxy() = default;

std::unique_ptr<WorkerThread> PaintWorkletMessagingProxy::CreateWorkerThread() {
  return AnimationAndPaintWorkletThread::CreateForPaintWorklet(
      WorkletObjectProxy());
}

}  // namespace blink
