// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_CLIP_PATH_PAINT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_CLIP_PATH_PAINT_DEFINITION_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/modules/csspaint/nativepaint/native_paint_definition.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class FloatRect;
class Image;
class LocalFrame;
class Node;
class PaintWorkletProxyClient;

class MODULES_EXPORT ClipPathPaintDefinition final
    : public GarbageCollected<ClipPathPaintDefinition>,
      public NativePaintDefinition {
 public:
  static ClipPathPaintDefinition* Create(LocalFrame& local_root);

  using PassKey = base::PassKey<ClipPathPaintDefinition>;
  explicit ClipPathPaintDefinition(PassKey, LocalFrame& local_root);
  ~ClipPathPaintDefinition() final = default;
  ClipPathPaintDefinition(const ClipPathPaintDefinition&) = delete;
  ClipPathPaintDefinition& operator=(const ClipPathPaintDefinition&) = delete;

  // PaintDefinition override
  sk_sp<PaintRecord> Paint(
      const CompositorPaintWorkletInput*,
      const CompositorPaintWorkletJob::AnimatedPropertyValues&) override;

  scoped_refptr<Image> Paint(float zoom,
                             const FloatRect& reference_box,
                             const Node&);

  // Unregister the painter to ensure that there is no memory leakage on the
  // compositor thread.
  void UnregisterProxyClient();

  void Trace(Visitor* visitor) const override;

 private:
  // Register the PaintWorkletProxyClient to the compositor thread that
  // will hold a cross thread persistent pointer to it. This should be called
  // during the construction of native paint worklets, to ensure that the proxy
  // client is ready on the compositor thread when dispatching a paint job.
  void RegisterProxyClient(LocalFrame&);

  int worklet_id_;
  // The worker thread that does the paint work.
  std::unique_ptr<WorkerBackingThread> worker_backing_thread_;
  Member<PaintWorkletProxyClient> proxy_client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_CLIP_PATH_PAINT_DEFINITION_H_
