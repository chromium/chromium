// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_BACKGROUND_COLOR_PAINT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_BACKGROUND_COLOR_PAINT_DEFINITION_H_

#include "base/macros.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/modules/csspaint/nativepaint/native_paint_definition.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

class Image;
class LocalFrame;
class Node;
class PaintWorkletProxyClient;

class MODULES_EXPORT BackgroundColorPaintDefinition final
    : public GarbageCollected<BackgroundColorPaintDefinition>,
      public NativePaintDefinition {
 public:
  static BackgroundColorPaintDefinition* Create(LocalFrame&);
  explicit BackgroundColorPaintDefinition(LocalFrame&);
  ~BackgroundColorPaintDefinition() final = default;
  BackgroundColorPaintDefinition(const BackgroundColorPaintDefinition&) =
      delete;
  BackgroundColorPaintDefinition& operator=(
      const BackgroundColorPaintDefinition&) = delete;

  // PaintDefinition override
  sk_sp<PaintRecord> Paint(
      const CompositorPaintWorkletInput*,
      const CompositorPaintWorkletJob::AnimatedPropertyValues&) override;

  // The |container_size| is without subpixel snapping.
  scoped_refptr<Image> Paint(const FloatSize& container_size,
                             const Node*,
                             const Vector<Color>& animated_colors,
                             const Vector<double>& offsets,
                             const absl::optional<double>& progress);

  // Get the animated colors and offsets from the animation keyframes. Moreover,
  // we obtain the progress of the animation from the main thread, such that if
  // the animation failed to run on the compositor thread, we can still paint
  // the element off the main thread with that progress + the keyframes.
  // Returning false meaning that we cannot paint background color with
  // BackgroundColorPaintWorklet.
  // A side effect of this is that it will ensure a unique_id exists.
  static bool GetBGColorPaintWorkletParams(Node* node,
                                           Vector<Color>* animated_colors,
                                           Vector<double>* offsets,
                                           absl::optional<double>* progress);

  // Shared code that is being called in multiple places.
  static Animation* GetAnimationIfCompositable(const Element* element);

  // Unregister the painter to ensure that there is no memory leakage on the
  // compositor thread.
  void UnregisterProxyClient();

  void Trace(Visitor* visitor) const override;

  // Constructor for testing purpose only.
  BackgroundColorPaintDefinition() = default;
  sk_sp<PaintRecord> PaintForTest(
      const Vector<Color>& animated_colors,
      const Vector<double>& offsets,
      const CompositorPaintWorkletJob::AnimatedPropertyValues&
          animated_property_values);

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

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_BACKGROUND_COLOR_PAINT_DEFINITION_H_
