// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_2d_color_params.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"

namespace blink {

Canvas2DColorParams::Canvas2DColorParams() = default;

Canvas2DColorParams::Canvas2DColorParams(PredefinedColorSpace color_space,
                                         CanvasPixelFormat pixel_format,
                                         bool has_alpha)
    : color_space_(color_space),
      pixel_format_(pixel_format),
      has_alpha_(has_alpha) {}

SkColorType Canvas2DColorParams::GetSkColorType() const {
  switch (pixel_format_) {
    case CanvasPixelFormat::kF16:
      return kRGBA_F16_SkColorType;
    case CanvasPixelFormat::kUint8:
      return kN32_SkColorType;
  }
  NOTREACHED();
}

gfx::ColorSpace Canvas2DColorParams::GetGfxColorSpace() const {
  return PredefinedColorSpaceToGfxColorSpace(color_space_);
}

}  // namespace blink
