// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_image.h"

#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"

namespace blink {

gfx::SizeF StyleImage::ApplyZoom(const gfx::SizeF& size, float multiplier) {
  if (multiplier == 1.0f)
    return size;

  gfx::SizeF scaled_size = gfx::ScaleSize(size, multiplier);

  // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
  if (size.width() > 0)
    scaled_size.set_width(std::max(1.0f, scaled_size.width()));

  if (size.height() > 0)
    scaled_size.set_height(std::max(1.0f, scaled_size.height()));

  return scaled_size;
}

gfx::SizeF StyleImage::ImageSizeForSVGImage(
    const SVGImage& svg_image,
    float multiplier,
    const gfx::SizeF& default_object_size) {
  gfx::SizeF unzoomed_default_object_size =
      gfx::ScaleSize(default_object_size, 1 / multiplier);
  return ApplyZoom(svg_image.ConcreteObjectSize(unzoomed_default_object_size),
                   multiplier);
}

bool StyleImage::HasIntrinsicDimensionsForSVGImage(const SVGImage& svg_image) {
  IntrinsicSizingInfo intrinsic_sizing_info;
  if (!svg_image.GetIntrinsicSizingInfo(intrinsic_sizing_info))
    return false;
  return intrinsic_sizing_info.has_width || intrinsic_sizing_info.has_height ||
         !intrinsic_sizing_info.aspect_ratio.IsEmpty();
}

}  // namespace blink
