// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_OPACITY_FILTER_CANVAS_H_
#define SKIA_EXT_OPACITY_FILTER_CANVAS_H_

#include "third_party/skia/include/utils/SkPaintFilterCanvas.h"

namespace skia {

// This filter canvas allows setting an opacity on every draw call to a canvas,
// and to disable image filtering. Note that the opacity setting is only
// correct in very limited conditions: when there is only zero or one opaque,
// nonlayer draw for every pixel in the surface.
class SK_API OpacityFilterCanvas : public SkPaintFilterCanvas {
 public:
  OpacityFilterCanvas(SkCanvas* canvas,
                      float opacity,
                      bool disable_image_filtering);

 protected:
  bool onFilter(SkPaint& paint) const override;

  void onDrawImage2(const SkImage*,
                    SkScalar dx,
                    SkScalar dy,
                    const SkSamplingOptions&,
                    const SkPaint*) override;
  void onDrawImageRect2(const SkImage*,
                        const SkRect& src,
                        const SkRect& dst,
                        const SkSamplingOptions&,
                        const SkPaint*,
                        SrcRectConstraint) override;
  void onDrawEdgeAAImageSet2(const ImageSetEntry imageSet[],
                             int count,
                             const SkPoint dstClips[],
                             const SkMatrix preViewMatrices[],
                             const SkSamplingOptions&,
                             const SkPaint*,
                             SrcRectConstraint) override;

  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix* matrix,
                     const SkPaint* paint) override;

 private:
  typedef SkPaintFilterCanvas INHERITED;

  float opacity_;
  bool disable_image_filtering_;
};

}  // namespace skia

#endif  // SKIA_EXT_OPACITY_FILTER_CANVAS_H_
