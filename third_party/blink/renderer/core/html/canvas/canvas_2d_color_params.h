// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_2D_COLOR_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_2D_COLOR_PARAMS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

// Parameters used by CanvasRenderingContext2D and
// OffscreenCanvasRenderingContext2D.
class CORE_EXPORT Canvas2DColorParams {
  DISALLOW_NEW();

 public:
  // The default constructor will create an output-blended 8-bit surface.
  Canvas2DColorParams();
  Canvas2DColorParams(PredefinedColorSpace, CanvasPixelFormat, bool has_alpha);

  PredefinedColorSpace ColorSpace() const { return color_space_; }
  CanvasPixelFormat PixelFormat() const { return pixel_format_; }
  SkAlphaType GetAlphaType() const {
    return has_alpha_ ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
  }

  SkColorInfo GetSkColorInfo() const {
    return SkColorInfo(GetSkColorType(), GetAlphaType(), GetSkColorSpace());
  }
  SkColorType GetSkColorType() const;
  sk_sp<SkColorSpace> GetSkColorSpace() const;

 private:
  PredefinedColorSpace color_space_ = PredefinedColorSpace::kSRGB;
  CanvasPixelFormat pixel_format_ = CanvasPixelFormat::kUint8;
  bool has_alpha_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_2D_COLOR_PARAMS_H_
