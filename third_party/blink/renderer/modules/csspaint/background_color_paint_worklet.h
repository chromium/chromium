// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_BACKGROUND_COLOR_PAINT_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_BACKGROUND_COLOR_PAINT_WORKLET_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

class Image;
class LocalFrame;
class Node;

class MODULES_EXPORT BackgroundColorPaintWorklet : public NativePaintWorklet {
  DISALLOW_COPY_AND_ASSIGN(BackgroundColorPaintWorklet);

 public:
  static BackgroundColorPaintWorklet* Create(LocalFrame&);

  explicit BackgroundColorPaintWorklet(LocalFrame&);
  ~BackgroundColorPaintWorklet() final;

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

  // For testing purpose only.
  static sk_sp<cc::PaintRecord> ProxyClientPaintForTest(
      const Vector<Color>& animated_colors,
      const Vector<double>& offsets,
      const CompositorPaintWorkletJob::AnimatedPropertyValues&
          animated_property_values);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_BACKGROUND_COLOR_PAINT_WORKLET_H_
