// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation_frame/worker_animation_frame_provider.h"

#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

WorkerAnimationFrameProvider::WorkerAnimationFrameProvider(
    ExecutionContext* context,
    const BeginFrameProviderParams& begin_frame_provider_params)
    : begin_frame_provider_(
          MakeGarbageCollected<BeginFrameProvider>(begin_frame_provider_params,
                                                   this,
                                                   context)),
      callback_collection_(context),
      context_(context) {}

int WorkerAnimationFrameProvider::RegisterCallback(FrameCallback* callback) {
  if (!begin_frame_provider_->IsValidFrameProvider()) {
    return WorkerAnimationFrameProvider::kInvalidCallbackId;
  }

  FrameRequestCallbackCollection::CallbackId id =
      callback_collection_.RegisterFrameCallback(callback);
  begin_frame_provider_->RequestBeginFrame();
  return id;
}

void WorkerAnimationFrameProvider::CancelCallback(int id) {
  callback_collection_.CancelFrameCallback(id);
}

void WorkerAnimationFrameProvider::BeginFrame(const viz::BeginFrameArgs& args) {
  TRACE_EVENT_WITH_FLOW0("blink", "WorkerAnimationFrameProvider::BeginFrame",
                         TRACE_ID_GLOBAL(args.trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  context_->GetAgent()->event_loop()->EnqueueMicrotask(WTF::BindOnce(
      [](WeakPersistent<WorkerAnimationFrameProvider> provider,
         const viz::BeginFrameArgs& args) {
        if (!provider)
          return;
        TRACE_EVENT_WITH_FLOW0(
            "blink", "WorkerAnimationFrameProvider::RequestAnimationFrame",
            TRACE_ID_GLOBAL(args.trace_id), TRACE_EVENT_FLAG_FLOW_IN);
        {
          OffscreenCanvas::ScopedInsideWorkerRAF inside_raf_scope(args);
          for (auto& offscreen_canvas : provider->offscreen_canvases_) {
            // If one of the OffscreenCanvas has too many pending frames,
            // we abort the whole process.
            if (!inside_raf_scope.AddOffscreenCanvas(offscreen_canvas)) {
              provider->begin_frame_provider_->FinishBeginFrame(args);
              provider->begin_frame_provider_->RequestBeginFrame();
              return;
            }
          }

          auto* global_scope =
              DynamicTo<WorkerGlobalScope>(provider->context_.Get());
          DCHECK(global_scope);
          base::TimeDelta relative_time =
              args.frame_time.is_null()
                  ? base::TimeDelta()
                  : args.frame_time - global_scope->TimeOrigin();
          double time = Performance::ClampTimeResolution(
              relative_time,
              provider->context_->CrossOriginIsolatedCapability());
          provider->callback_collection_.ExecuteFrameCallbacks(time, time);
        }
        provider->begin_frame_provider_->FinishBeginFrame(args);
      },
      WrapWeakPersistent(this), args));
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerAnimationFrameProvider::GetCompositorTaskRunner() {
  return context_->GetScheduler()->CompositorTaskRunner();
}

void WorkerAnimationFrameProvider::RegisterOffscreenCanvas(
    OffscreenCanvas* context) {
  auto result = offscreen_canvases_.insert(context);
  DCHECK(result.is_new_entry);
}

void WorkerAnimationFrameProvider::DeregisterOffscreenCanvas(
    OffscreenCanvas* offscreen_canvas) {
  offscreen_canvases_.erase(offscreen_canvas);
}

void WorkerAnimationFrameProvider::Trace(Visitor* visitor) const {
  visitor->Trace(begin_frame_provider_);
  visitor->Trace(callback_collection_);
  visitor->Trace(offscreen_canvases_);
  visitor->Trace(context_);
  BeginFrameProviderClient::Trace(visitor);
}

}  // namespace blink
