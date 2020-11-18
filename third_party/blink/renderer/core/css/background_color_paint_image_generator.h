// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_BACKGROUND_COLOR_PAINT_IMAGE_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_BACKGROUND_COLOR_PAINT_IMAGE_GENERATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/native_paint_image_generator.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

class Image;
class LocalFrame;

class CORE_EXPORT BackgroundColorPaintImageGenerator
    : public NativePaintImageGenerator {
 public:
  static BackgroundColorPaintImageGenerator* Create(LocalFrame&);

  ~BackgroundColorPaintImageGenerator() override = default;

  typedef BackgroundColorPaintImageGenerator* (
      *BackgroundColorPaintImageGeneratorCreateFunction)(LocalFrame&);
  static void Init(
      BackgroundColorPaintImageGeneratorCreateFunction create_function);

  virtual scoped_refptr<Image> Paint(const FloatSize& container_size,
                                     SkColor color) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_BACKGROUND_COLOR_PAINT_IMAGE_GENERATOR_H_
