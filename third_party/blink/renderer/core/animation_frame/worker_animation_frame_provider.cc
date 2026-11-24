// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation_frame/worker_animation_frame_provider.h"

#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace blink {

WorkerAnimationFrameProvider::WorkerAnimationFrameProvider(
    ExecutionContext* context,
    const BeginFrameProviderParams& begin_frame_provider_params)
    : ExecutionContextLifecycleStateObserver(context),
      begin_frame_provider_(
          MakeGarbageCollected<BeginFrameProvider>(begin_frame_provider_params,
                                                   this,
                                                   context)),
      callback_collection_(context) {
  UpdateStateIfNeeded();
}

int WorkerAnimationFrameProvider::RegisterCallback(FrameCallback* callback) {
  ExecutionContext* context = GetExecutionContext();
  if (!context || !begin_frame_provider_->IsValidFrameProvider()) {
    return WorkerAnimationFrameProvider::kInvalidCallbackId;
  }

  FrameRequestCallbackCollection::CallbackId id =
      callback_collection_.RegisterFrameCallback(
          callback, FrameCallbackType::kWebExposed);
  // Delay the compositor request while paused/frozen; the resume handler
  // re-requests once the context becomes runnable.
  if (!(RuntimeEnabledFeatures::WorkerRafRunsInBeginFrameTaskEnabled() &&
        context->IsContextFrozenOrPaused())) {
    begin_frame_provider_->RequestBeginFrame();
  }
  return id;
}

void WorkerAnimationFrameProvider::CancelCallback(int id) {
  callback_collection_.CancelFrameCallback(id, FrameCallbackType::kWebExposed);
}

void WorkerAnimationFrameProvider::BeginFrame(const viz::BeginFrameArgs& args) {
  TRACE_EVENT("blink", "WorkerAnimationFrameProvider::BeginFrame",
              perfetto::Flow::Global(args.trace_id));

  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    begin_frame_provider_->FinishBeginFrame(args);
    return;
  }

  const bool run_in_begin_frame_task =
      RuntimeEnabledFeatures::WorkerRafRunsInBeginFrameTaskEnabled();

  // No rAF callbacks run while the worker is paused (devtools) or frozen
  // (BFCache); ack the frame and let the resume handler ask for another.
  if (run_in_begin_frame_task && context->IsContextFrozenOrPaused()) {
    begin_frame_provider_->FinishBeginFrame(args);
    return;
  }

  if (run_in_begin_frame_task) {
    RunBeginFrameCallbacks(args, /*terminate_flow=*/false);
    return;
  }

  context->GetAgent()->event_loop()->EnqueueMicrotask(
      BindOnce(&WorkerAnimationFrameProvider::RunBeginFrameCallbacks,
               WrapWeakPersistent(this), args, /*terminate_flow=*/true));
}

void WorkerAnimationFrameProvider::RunBeginFrameCallbacks(
    const viz::BeginFrameArgs& args,
    bool terminate_flow) {
  TRACE_EVENT("blink", "WorkerAnimationFrameProvider::RequestAnimationFrame",
              [&](perfetto::EventContext ctx) {
                if (terminate_flow) {
                  ctx.event()->add_terminating_flow_ids(args.trace_id);
                } else {
                  ctx.event()->add_flow_ids(args.trace_id);
                }
              });
  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    begin_frame_provider_->FinishBeginFrame(args);
    return;
  }
  {
    OffscreenCanvas::ScopedInsideWorkerRAF inside_raf_scope(args);
    // If one of the OffscreenCanvas has too many pending frames,
    // we abort the whole process.
    for (auto& offscreen_canvas : offscreen_canvases_) {
      if (!inside_raf_scope.AddOffscreenCanvas(offscreen_canvas)) {
        begin_frame_provider_->FinishBeginFrame(args);
        begin_frame_provider_->RequestBeginFrame();
        return;
      }
    }

    auto* global_scope = DynamicTo<WorkerGlobalScope>(context);
    DCHECK(global_scope);
    base::TimeDelta relative_time =
        args.frame_time.is_null()
            ? base::TimeDelta()
            : args.frame_time - global_scope->TimeOrigin();
    double time = Performance::ClampTimeResolution(
        relative_time, context->CrossOriginIsolatedCapability());
    callback_collection_.ExecuteFrameCallbacks(time, time);
  }
  begin_frame_provider_->FinishBeginFrame(args);
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerAnimationFrameProvider::GetCompositorTaskRunner() {
  ExecutionContext* context = GetExecutionContext();
  CHECK(context);
  return context->GetScheduler()->CompositorTaskRunner();
}

void WorkerAnimationFrameProvider::RegisterOffscreenCanvas(
    OffscreenCanvas* canvas) {
  CHECK(canvas->HasPlaceholderCanvas());
  auto result = offscreen_canvases_.insert(canvas);
  DCHECK(result.is_new_entry);
}

void WorkerAnimationFrameProvider::DeregisterOffscreenCanvas(
    OffscreenCanvas* offscreen_canvas) {
  offscreen_canvases_.erase(offscreen_canvas);
}

void WorkerAnimationFrameProvider::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  if (!RuntimeEnabledFeatures::WorkerRafRunsInBeginFrameTaskEnabled()) {
    return;
  }
  if (state == mojom::blink::FrameLifecycleState::kRunning &&
      callback_collection_.HasFrameCallback()) {
    begin_frame_provider_->RequestBeginFrame();
  }
}

void WorkerAnimationFrameProvider::Trace(Visitor* visitor) const {
  visitor->Trace(begin_frame_provider_);
  visitor->Trace(callback_collection_);
  visitor->Trace(offscreen_canvases_);
  BeginFrameProviderClient::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

}  // namespace blink
