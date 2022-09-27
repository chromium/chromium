// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/worklet/animation_and_paint_worklet_thread.h"

#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worklet_thread_holder.h"
#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

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
      WorkletType::kAnimation, worker_reporting_proxy));
}

std::unique_ptr<AnimationAndPaintWorkletThread>
AnimationAndPaintWorkletThread::CreateForPaintWorklet(
    WorkerReportingProxy& worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("paint-worklet"),
               "AnimationAndPaintWorkletThread::CreateForPaintWorklet");
  DCHECK(IsMainThread());
  return base::WrapUnique(new AnimationAndPaintWorkletThread(
      WorkletType::kPaint, worker_reporting_proxy));
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

static void CollectAllGarbageOnThreadForTesting(
    base::WaitableEvent* done_event) {
  blink::ThreadState::Current()->CollectAllGarbageForTesting();
  done_event->Signal();
}

void AnimationAndPaintWorkletThread::CollectAllGarbageForTesting() {
  DCHECK(IsMainThread());
  base::WaitableEvent done_event;
  auto* holder =
      WorkletThreadHolder<AnimationAndPaintWorkletThread>::GetInstance();
  if (!holder)
    return;
  PostCrossThreadTask(*holder->GetThread()->BackingThread().GetTaskRunner(),
                      FROM_HERE,
                      CrossThreadBindOnce(&CollectAllGarbageOnThreadForTesting,
                                          CrossThreadUnretained(&done_event)));
  done_event.Wait();
}

WorkerOrWorkletGlobalScope*
AnimationAndPaintWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  switch (worklet_type_) {
    case WorkletType::kAnimation: {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
                   "AnimationAndPaintWorkletThread::CreateWorkerGlobalScope");
      return MakeGarbageCollected<AnimationWorkletGlobalScope>(
          std::move(creation_params), this);
    }
    case WorkletType::kPaint:
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("paint-worklet"),
                   "AnimationAndPaintWorkletThread::CreateWorkerGlobalScope");
      return PaintWorkletGlobalScope::Create(std::move(creation_params), this);
  };
}

void AnimationAndPaintWorkletThread::EnsureSharedBackingThread() {
  DCHECK(IsMainThread());
  WorkletThreadHolder<AnimationAndPaintWorkletThread>::EnsureInstance(
      ThreadCreationParams(ThreadType::kAnimationAndPaintWorkletThread));
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
