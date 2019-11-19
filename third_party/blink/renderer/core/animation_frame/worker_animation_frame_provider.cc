// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation_frame/worker_animation_frame_provider.h"

#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

WorkerAnimationFrameProvider::WorkerAnimationFrameProvider(
    ExecutionContext* context,
    const BeginFrameProviderParams& begin_frame_provider_params)
    : begin_frame_provider_(
          std::make_unique<BeginFrameProvider>(begin_frame_provider_params,
                                               this)),
      callback_collection_(context),
      context_(context) {}

int WorkerAnimationFrameProvider::RegisterCallback(
    FrameRequestCallbackCollection::FrameCallback* callback) {
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

  Microtask::EnqueueMicrotask(WTF::Bind(
      [](base::WeakPtr<WorkerAnimationFrameProvider> provider,
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

          double time = (args.frame_time - base::TimeTicks()).InMillisecondsF();
          provider->callback_collection_.ExecuteFrameCallbacks(time, time);
        }
        provider->begin_frame_provider_->FinishBeginFrame(args);
      },
      weak_factory_.GetWeakPtr(), args));
}

void WorkerAnimationFrameProvider::RegisterOffscreenCanvas(
    OffscreenCanvas* context) {
  DCHECK(offscreen_canvases_.Find(context) == kNotFound);
  offscreen_canvases_.push_back(context);
}

void WorkerAnimationFrameProvider::DeregisterOffscreenCanvas(
    OffscreenCanvas* offscreen_canvas) {
  wtf_size_t pos = offscreen_canvases_.Find(offscreen_canvas);
  if (pos != kNotFound) {
    offscreen_canvases_.EraseAt(pos);
  }
}

void WorkerAnimationFrameProvider::Trace(blink::Visitor* visitor) {
  visitor->Trace(callback_collection_);
  visitor->Trace(context_);
}

}  // namespace blink
