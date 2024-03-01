// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_COLOR_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_COLOR_PARAMS_H_

#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/buffer_types.h"

namespace gfx {
class ColorSpace;
}

namespace WTF {
class String;
}  // namespace WTF

namespace blink {

// Return the gfx::ColorSpace for the specified `predefined_color_space`.
gfx::ColorSpace PLATFORM_EXPORT
PredefinedColorSpaceToGfxColorSpace(PredefinedColorSpace color_space);

// Return the SkColorSpace for the specified |color_space|.
sk_sp<SkColorSpace> PLATFORM_EXPORT
PredefinedColorSpaceToSkColorSpace(PredefinedColorSpace color_space);

// Return the named PredefinedColorSpace that best matches |sk_color_space|.
PredefinedColorSpace PLATFORM_EXPORT
PredefinedColorSpaceFromSkColorSpace(const SkColorSpace* sk_color_space);

// Return the SkColorType that best matches the specified CanvasPixelFormat.
SkColorType PLATFORM_EXPORT
CanvasPixelFormatToSkColorType(CanvasPixelFormat pixel_format);

class PLATFORM_EXPORT CanvasColorParams {
  DISALLOW_NEW();

 public:
  // The default constructor will create an output-blended 8-bit surface.
  CanvasColorParams();
  CanvasColorParams(PredefinedColorSpace, CanvasPixelFormat, OpacityMode);
  CanvasColorParams(PredefinedColorSpace, CanvasPixelFormat, bool has_alpha);

  PredefinedColorSpace ColorSpace() const { return color_space_; }
  CanvasPixelFormat PixelFormat() const { return pixel_format_; }
  OpacityMode GetOpacityMode() const { return opacity_mode_; }

  WTF::String GetColorSpaceAsString() const;
  WTF::String GetPixelFormatAsString() const;

  SkColorInfo GetSkColorInfo() const;

  // The pixel format to use for allocating SkSurfaces.
  SkColorType GetSkColorType() const;

  // Return the color space of the underlying data for the canvas.
  gfx::ColorSpace GetStorageGfxColorSpace() const;
  sk_sp<SkColorSpace> GetSkColorSpace() const;

  uint8_t BytesPerPixel() const;

 private:
  PredefinedColorSpace color_space_ = PredefinedColorSpace::kSRGB;
  CanvasPixelFormat pixel_format_ = CanvasPixelFormat::kUint8;
  OpacityMode opacity_mode_ = kNonOpaque;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_COLOR_PARAMS_H_
