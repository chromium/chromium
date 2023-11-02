// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace skia {

OpacityFilterCanvas::OpacityFilterCanvas(SkCanvas* canvas,
                                         float opacity,
                                         bool disable_image_filtering)
    : INHERITED(canvas),
      opacity_(opacity),
      disable_image_filtering_(disable_image_filtering) {}

bool OpacityFilterCanvas::onFilter(SkPaint& paint) const {
  if (opacity_ < 1.f)
    paint.setAlphaf(paint.getAlphaf() * opacity_);

  return true;
}

void OpacityFilterCanvas::onDrawImage2(const SkImage* image,
                                       SkScalar dx,
                                       SkScalar dy,
                                       const SkSamplingOptions& sampling,
                                       const SkPaint* paint) {
  this->INHERITED::onDrawImage2(
      image, dx, dy, disable_image_filtering_ ? SkSamplingOptions() : sampling,
      paint);
}

void OpacityFilterCanvas::onDrawImageRect2(const SkImage* image,
                                           const SkRect& src,
                                           const SkRect& dst,
                                           const SkSamplingOptions& sampling,
                                           const SkPaint* paint,
                                           SrcRectConstraint constraint) {
  this->INHERITED::onDrawImageRect2(
      image, src, dst,
      disable_image_filtering_ ? SkSamplingOptions() : sampling, paint,
      constraint);
}

void OpacityFilterCanvas::onDrawEdgeAAImageSet2(
    const ImageSetEntry imageSet[],
    int count,
    const SkPoint dstClips[],
    const SkMatrix preViewMatrices[],
    const SkSamplingOptions& sampling,
    const SkPaint* paint,
    SrcRectConstraint constraint) {
  this->INHERITED::onDrawEdgeAAImageSet2(
      imageSet, count, dstClips, preViewMatrices,
      disable_image_filtering_ ? SkSamplingOptions() : sampling, paint,
      constraint);
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
