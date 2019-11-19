// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_OPACITY_FILTER_CANVAS_H
#define SKIA_EXT_OPACITY_FILTER_CANVAS_H

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

  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix* matrix,
                     const SkPaint* paint) override;

 private:
  typedef SkPaintFilterCanvas INHERITED;

  int alpha_;
  bool disable_image_filtering_;
};

}  // namespace skia

#endif  // SKIA_EXT_OPACITY_FILTER_CANVAS_H
