// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/worklet/animation_and_paint_worklet_thread.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worklet_thread_holder.h"
#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/web_thread_supporting_gc.h"

namespace blink {

namespace {
unsigned s_ref_count = 0;
}  // namespace

std::unique_ptr<AnimationAndPaintWorkletThread>
AnimationAndPaintWorkletThread::CreateForAnimationWorklet(
    WorkerReportingProxy& worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
               "AnimationAndPaintWorkletThread::CreateForAnimationWorklet");
  DCHECK(IsMainThread());
  return base::WrapUnique(new AnimationAndPaintWorkletThread(
      WorkletType::ANIMATION_WORKLET, worker_reporting_proxy));
}

template class WorkletThreadHolder<AnimationAndPaintWorkletThread>;

AnimationAndPaintWorkletThread::AnimationAndPaintWorkletThread(
    WorkletType worklet_type,
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy), worklet_type_(worklet_type) {
  DCHECK(IsMainThread());
  if (++s_ref_count == 1) {
    EnsureSharedBackingThread();
  }
}

AnimationAndPaintWorkletThread::~AnimationAndPaintWorkletThread() {
  DCHECK(IsMainThread());
  if (--s_ref_count == 0) {
    ClearSharedBackingThread();
  }
}

WorkerBackingThread& AnimationAndPaintWorkletThread::GetWorkerBackingThread() {
  return *WorkletThreadHolder<AnimationAndPaintWorkletThread>::GetInstance()
              ->GetThread();
}

static void CollectAllGarbageOnThread(WaitableEvent* done_event) {
  blink::ThreadState::Current()->CollectAllGarbage();
  done_event->Signal();
}

void AnimationAndPaintWorkletThread::CollectAllGarbage() {
  DCHECK(IsMainThread());
  WaitableEvent done_event;
  auto* holder =
      WorkletThreadHolder<AnimationAndPaintWorkletThread>::GetInstance();
  if (!holder)
    return;
  holder->GetThread()->BackingThread().PostTask(
      FROM_HERE, CrossThreadBind(&CollectAllGarbageOnThread,
                                 CrossThreadUnretained(&done_event)));
  done_event.Wait();
}

WorkerOrWorkletGlobalScope*
AnimationAndPaintWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  switch (worklet_type_) {
    case WorkletType::ANIMATION_WORKLET: {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
                   "AnimationAndPaintWorkletThread::CreateWorkerGlobalScope");
      return AnimationWorkletGlobalScope::Create(std::move(creation_params),
                                                 this);
    }
    case WorkletType::PAINT_WORKLET:
      // TODO(smcgruer): Add ability to create a PaintWorkletGlobalScope.
      NOTREACHED();
      return nullptr;
  };
}

void AnimationAndPaintWorkletThread::EnsureSharedBackingThread() {
  DCHECK(IsMainThread());
  WorkletThreadHolder<AnimationAndPaintWorkletThread>::EnsureInstance(
      ThreadCreationParams(WebThreadType::kAnimationAndPaintWorkletThread));
}

void AnimationAndPaintWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  DCHECK_EQ(s_ref_count, 0u);
  WorkletThreadHolder<AnimationAndPaintWorkletThread>::ClearInstance();
}

// static
WorkletThreadHolder<AnimationAndPaintWorkletThread>*
AnimationAndPaintWorkletThread::GetWorkletThreadHolderForTesting() {
  return WorkletThreadHolder<AnimationAndPaintWorkletThread>::GetInstance();
}

}  // namespace blink
