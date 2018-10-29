// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_animation_frame_provider.h"

#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

WorkerAnimationFrameProvider::WorkerAnimationFrameProvider(
    ExecutionContext* context,
    const BeginFrameProviderParams& begin_frame_provider_params)
    : begin_frame_provider_(
          std::make_unique<BeginFrameProvider>(begin_frame_provider_params,
                                               this)),
      callback_collection_(context),
      context_(context),
      weak_factory_(this) {}

int WorkerAnimationFrameProvider::RegisterCallback(
    FrameRequestCallbackCollection::FrameCallback* callback) {
  if (!begin_frame_provider_->IsValidFrameProvider()) {
    return WorkerAnimationFrameProvider::kInvalidCallbackId;
  }

  FrameRequestCallbackCollection::CallbackId id =
      callback_collection_.RegisterCallback(callback);
  begin_frame_provider_->RequestBeginFrame();
  return id;
}

void WorkerAnimationFrameProvider::CancelCallback(int id) {
  callback_collection_.CancelCallback(id);
}

void WorkerAnimationFrameProvider::BeginFrame() {
  // TODO(fserb): Remove this once the Mojo changes for scheduling are in.
  context_->GetTaskRunner(TaskType::kWorkerAnimation)
      ->PostTask(
          FROM_HERE,
          WTF::Bind(
              [](base::WeakPtr<WorkerAnimationFrameProvider> provider) {
                double time = WTF::CurrentTimeTicksInMilliseconds();
                // We don't want to expose microseconds residues to users.
                time = round(time * 60) / 60;

                provider->callback_collection_.ExecuteCallbacks(time, time);

                for (auto& offscreen_canvas : provider->offscreen_canvases_) {
                  offscreen_canvas->PushFrameIfNeeded();
                }
              },
              weak_factory_.GetWeakPtr()));
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
