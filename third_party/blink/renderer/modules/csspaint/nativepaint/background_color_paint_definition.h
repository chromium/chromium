// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_BACKGROUND_COLOR_PAINT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_BACKGROUND_COLOR_PAINT_DEFINITION_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/modules/csspaint/nativepaint/native_css_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

class Image;
class LocalFrame;
class Node;

class MODULES_EXPORT BackgroundColorPaintDefinition final
    : public GarbageCollected<BackgroundColorPaintDefinition>,
      public NativeCssPaintDefinition {
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
  scoped_refptr<Image> Paint(const gfx::SizeF& container_size,
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

  static Animation* GetAnimationIfCompositable(const Element* element);

  void Trace(Visitor* visitor) const override;

 private:
  friend class BackgroundColorPaintDefinitionTest;

  // Constructor for testing purpose only.
  BackgroundColorPaintDefinition() = default;
  sk_sp<PaintRecord> PaintForTest(
      const Vector<Color>& animated_colors,
      const Vector<double>& offsets,
      const CompositorPaintWorkletJob::AnimatedPropertyValues&
          animated_property_values,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  sk_sp<PaintRecord> Paint(
      const CompositorPaintWorkletInput* compositor_input,
      const CompositorPaintWorkletJob::AnimatedPropertyValues&
          animated_property_values,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // The instance of BackgroundColorPaintDefinition is created on the main
  // thread, which means |context_| is initialized on the main thread's heap.
  // However, |context_| is used on a worker backing thread, and that's why it
  // needs to be CrossThreadPersistent.
  // The |context_| can live as long as BackgroundColorPaintDefinition.
  CrossThreadPersistent<PaintRenderingContext2D> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_BACKGROUND_COLOR_PAINT_DEFINITION_H_
