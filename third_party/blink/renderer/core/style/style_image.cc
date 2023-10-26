// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_image.h"

#include "third_party/blink/renderer/platform/graphics/image.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

scoped_refptr<Image> StyleImage::GetImage(const ImageResourceObserver& observer,
                                          const Document& document,
                                          const ComputedStyle& style,
                                          const gfx::SizeF& target_size) const {
  return GetImage(observer, document, style, target_size, gfx::RectF());
}

gfx::SizeF StyleImage::ApplyZoom(const gfx::SizeF& size, float multiplier) {
  if (multiplier == 1.0f) {
    return size;
  }

  gfx::SizeF scaled_size = gfx::ScaleSize(size, multiplier);

  // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
  if (size.width() > 0) {
    scaled_size.set_width(std::max(1.0f, scaled_size.width()));
  }

  if (size.height() > 0) {
    scaled_size.set_height(std::max(1.0f, scaled_size.height()));
  }

  return scaled_size;
}

}  // namespace blink
