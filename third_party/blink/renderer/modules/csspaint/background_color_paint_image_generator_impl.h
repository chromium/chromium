// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_BACKGROUND_COLOR_PAINT_IMAGE_GENERATOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_BACKGROUND_COLOR_PAINT_IMAGE_GENERATOR_IMPL_H_

#include "third_party/blink/renderer/core/css/background_color_paint_image_generator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "v8/include/v8.h"

namespace blink {

class Image;
class BackgroundColorPaintWorklet;

class MODULES_EXPORT BackgroundColorPaintImageGeneratorImpl final
    : public BackgroundColorPaintImageGenerator {
 public:
  static BackgroundColorPaintImageGenerator* Create(LocalFrame&);

  explicit BackgroundColorPaintImageGeneratorImpl(BackgroundColorPaintWorklet*);
  ~BackgroundColorPaintImageGeneratorImpl() override = default;

  // The |container_size| is without subpixel snapping.
  scoped_refptr<Image> Paint(const FloatSize& container_size,
                             const Node*,
                             const Vector<Color>& animated_colors,
                             const Vector<double>& offsets) final;

  bool GetBGColorPaintWorkletParams(Node* node,
                                    Vector<Color>* animated_colors,
                                    Vector<double>* offsets) final;

  void Shutdown() final;

  void Trace(Visitor*) const override;

 private:
  Member<BackgroundColorPaintWorklet> background_color_paint_worklet_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_BACKGROUND_COLOR_PAINT_IMAGE_GENERATOR_IMPL_H_
