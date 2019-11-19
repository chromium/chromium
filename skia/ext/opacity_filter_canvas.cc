// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace skia {

OpacityFilterCanvas::OpacityFilterCanvas(SkCanvas* canvas,
                                         float opacity,
                                         bool disable_image_filtering)
    : INHERITED(canvas),
      alpha_(SkScalarRoundToInt(opacity * 255)),
      disable_image_filtering_(disable_image_filtering) { }

bool OpacityFilterCanvas::onFilter(SkPaint& paint) const {
  if (alpha_ < 255)
    paint.setAlpha(alpha_);

  if (disable_image_filtering_)
    paint.setFilterQuality(kNone_SkFilterQuality);

  return true;
}

void OpacityFilterCanvas::onDrawPicture(const SkPicture* picture,
                                        const SkMatrix* matrix,
                                        const SkPaint* paint) {
  SkPaint filteredPaint(paint ? *paint : SkPaint());
  if (this->onFilter(filteredPaint)) {
    // Unfurl pictures in order to filter nested paints.
    this->SkCanvas::onDrawPicture(picture, matrix, &filteredPaint);
  }
}

}  // namespace skia
