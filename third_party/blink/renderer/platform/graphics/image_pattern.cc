// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/image_pattern.h"

#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

scoped_refptr<ImagePattern> ImagePattern::Create(scoped_refptr<Image> image,
                                                 RepeatMode repeat_mode) {
  return base::AdoptRef(new ImagePattern(std::move(image), repeat_mode));
}

ImagePattern::ImagePattern(scoped_refptr<Image> image, RepeatMode repeat_mode)
    : Pattern(repeat_mode), tile_image_(image->PaintImageForCurrentFrame()) {}

sk_sp<PaintShader> ImagePattern::CreateShader(const SkMatrix& local_matrix) {
  if (!tile_image_) {
    return PaintShader::MakeColor(SK_ColorTRANSPARENT);
  }

  return PaintShader::MakeImage(
      tile_image_, IsRepeatX() ? SkTileMode::kRepeat : SkTileMode::kDecal,
      IsRepeatY() ? SkTileMode::kRepeat : SkTileMode::kDecal, &local_matrix);
}

bool ImagePattern::IsTextureBacked() const {
  return tile_image_ && tile_image_.GetSkImage()->isTextureBacked();
}

}  // namespace blink
