// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_FRAME_WORKER_ANIMATION_FRAME_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_FRAME_WORKER_ANIMATION_FRAME_PROVIDER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/platform/graphics/begin_frame_provider.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class OffscreenCanvas;

// WorkerAnimationFrameProvider is a member of WorkerGlobalScope and it provides
// RequestAnimationFrame capabilities to Workers.
//
// It's responsible for registering and dealing with callbacks.
// And maintains a connection with the Display process, through
// CompositorFrameSink, that is used to v-sync with the display.
//
// OffscreenCanvases can notify when there's been a change on any
// OffscreenCanvas that is connected to a Canvas, and this class signals
// OffscreenCanvases when it's time to dispatch frames.
class CORE_EXPORT WorkerAnimationFrameProvider
    : public GarbageCollected<WorkerAnimationFrameProvider>,
      public BeginFrameProviderClient {
 public:
  WorkerAnimationFrameProvider(
      ExecutionContext* context,
      const BeginFrameProviderParams& begin_frame_provider_params);

  int RegisterCallback(FrameRequestCallbackCollection::FrameCallback* callback);
  void CancelCallback(int id);

  void Trace(blink::Visitor* visitor);

  // BeginFrameProviderClient
  void BeginFrame(const viz::BeginFrameArgs&) override;

  void RegisterOffscreenCanvas(OffscreenCanvas*);
  void DeregisterOffscreenCanvas(OffscreenCanvas*);

  static const int kInvalidCallbackId = -1;

 private:
  const std::unique_ptr<BeginFrameProvider> begin_frame_provider_;
  DISALLOW_COPY_AND_ASSIGN(WorkerAnimationFrameProvider);
  FrameRequestCallbackCollection callback_collection_;

  // To avoid leaking OffscreenCanvas objects, the following vector must
  // not hold strong references.
  Vector<UntracedMember<OffscreenCanvas>> offscreen_canvases_;

  Member<ExecutionContext> context_;

  base::WeakPtrFactory<WorkerAnimationFrameProvider> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_FRAME_WORKER_ANIMATION_FRAME_PROVIDER_H_
