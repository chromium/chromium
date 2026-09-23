// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/image_pattern.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

std::unique_ptr<ImagePattern> ImagePattern::Create(
    scoped_refptr<Image> image,
    RepeatMode repeat_mode,
    RespectImageOrientationEnum respect_orientation) {
  return base::WrapUnique(
      new ImagePattern(std::move(image), repeat_mode, respect_orientation));
}

ImagePattern::ImagePattern(scoped_refptr<Image> image,
                           RepeatMode repeat_mode,
                           RespectImageOrientationEnum respect_orientation)
    : Pattern(repeat_mode) {
  CHECK(image);
  PaintImage paint_image = image->PaintImageForCurrentFrame();
  if (paint_image && respect_orientation == kRespectImageOrientation &&
      !image->HasDefaultOrientation()) {
    tile_image_ =
        Image::ResizeAndOrientImage(paint_image, image->Orientation());
  } else {
    tile_image_ = std::move(paint_image);
  }
}

sk_sp<PaintShader> ImagePattern::CreateShader(
    const SkMatrix& local_matrix) const {
  if (!tile_image_) {
    return PaintShader::MakeColor(SkColors::kTransparent);
  }

  return PaintShader::MakeImage(
      tile_image_, IsRepeatX() ? SkTileMode::kRepeat : SkTileMode::kDecal,
      IsRepeatY() ? SkTileMode::kRepeat : SkTileMode::kDecal, &local_matrix);
}

bool ImagePattern::IsTextureBacked() const {
  return tile_image_ && tile_image_.IsTextureBacked();
}

}  // namespace blink
