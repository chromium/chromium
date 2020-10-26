// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVE_PAINT_IMAGE_GENERATOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVE_PAINT_IMAGE_GENERATOR_IMPL_H_

#include "third_party/blink/renderer/core/css/native_paint_image_generator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "v8/include/v8.h"

namespace blink {

class Image;
class NativePaintWorklet;

class MODULES_EXPORT NativePaintImageGeneratorImpl final
    : public NativePaintImageGenerator {
 public:
  static std::unique_ptr<NativePaintImageGenerator> Create();

  explicit NativePaintImageGeneratorImpl(std::unique_ptr<NativePaintWorklet>);
  ~NativePaintImageGeneratorImpl() override;

  // The |container_size| is without subpixel snapping.
  scoped_refptr<Image> Paint(const FloatSize& container_size,
                             SkColor color) final;

 private:
  std::unique_ptr<NativePaintWorklet> native_paint_worklet_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVE_PAINT_IMAGE_GENERATOR_IMPL_H_
