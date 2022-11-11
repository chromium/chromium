// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/native_paint_definition.h"

#include "base/feature_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_id_generator.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// If enabled, uses the compositor priority to avoid potential starvation
// when the compositor waits for worklets before activating pending trees.
BASE_FEATURE(kNativePaintUsesCompositorPriority,
             "NativePaintUsesCompositorPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

NativePaintDefinition::NativePaintDefinition(
    LocalFrame* local_root,
    PaintWorkletInput::PaintWorkletInputType type)
    : worklet_id_(PaintWorkletIdGenerator::NextId()) {
  DCHECK(local_root->IsLocalRoot());
  DCHECK(IsMainThread());
  ExecutionContext* context = local_root->DomWindow();
  FrameOrWorkerScheduler* scheduler =
      context ? context->GetScheduler() : nullptr;
  // TODO(crbug.com/1143407): We shouldn't need a thread for native paint
  // definitions as they're running trusted code so it is okay to be run
  // directly on the compositor thread. The main issue is that many blink
  // classes require garbage collection to construct, so we need to add
  // GC support to the compositor to avoid this.
  ThreadCreationParams params(ThreadType::kAnimationAndPaintWorkletThread);
  if (base::FeatureList::IsEnabled(kNativePaintUsesCompositorPriority)) {
    // The base thread type is used to establish the priority of the thread. As
    // the compositor waits for this thread to produce paint ops before it can
    // activate the pending tree, we use compositing thread type to reflect that
    // the work is critical to compositing.
    params.base_thread_type = base::ThreadType::kCompositing;
  }
  worker_backing_thread_ = std::make_unique<WorkerBackingThread>(
      params.SetFrameOrWorkerScheduler(scheduler));
  auto startup_data = WorkerBackingThreadStartupData::CreateDefault();
  PostCrossThreadTask(
      *worker_backing_thread_->BackingThread().GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&WorkerBackingThread::InitializeOnBackingThread,
                          CrossThreadUnretained(worker_backing_thread_.get()),
                          startup_data));
  RegisterProxyClient(local_root, type);
}

void NativePaintDefinition::RegisterProxyClient(
    LocalFrame* local_root,
    PaintWorkletInput::PaintWorkletInputType type) {
  proxy_client_ =
      PaintWorkletProxyClient::Create(local_root->DomWindow(), worklet_id_);
  proxy_client_->RegisterForNativePaintWorklet(worker_backing_thread_.get(),
                                               this, type);
}

void NativePaintDefinition::UnregisterProxyClient() {
  proxy_client_->UnregisterForNativePaintWorklet();
}

void NativePaintDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(proxy_client_);
  PaintDefinition::Trace(visitor);
}

}  // namespace blink
