// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_NATIVE_PAINT_IMAGE_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_NATIVE_PAINT_IMAGE_GENERATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

class Image;

class CORE_EXPORT NativePaintImageGenerator {
 public:
  static std::unique_ptr<NativePaintImageGenerator> Create();
  virtual ~NativePaintImageGenerator();

  typedef std::unique_ptr<NativePaintImageGenerator> (
      *NativePaintImageGeneratorCreateFunction)();
  static void Init(NativePaintImageGeneratorCreateFunction);

  virtual scoped_refptr<Image> Paint(const FloatSize& container_size,
                                     SkColor color) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_NATIVE_PAINT_IMAGE_GENERATOR_H_
