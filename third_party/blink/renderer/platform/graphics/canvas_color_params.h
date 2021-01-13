// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_COLOR_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_COLOR_PARAMS_H_

#include "components/viz/common/resources/resource_format.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/buffer_types.h"

namespace gfx {
class ColorSpace;
}

namespace blink {

class CanvasResourceParams;

enum class CanvasColorSpace {
  kSRGB,
  kRec2020,
  kP3,
};

enum class CanvasPixelFormat {
  kUint8,
  kF16,
};

constexpr const char* kSRGBCanvasColorSpaceName = "srgb";
constexpr const char* kRec2020CanvasColorSpaceName = "rec2020";
constexpr const char* kP3CanvasColorSpaceName = "display-p3";

constexpr const char* kUint8CanvasPixelFormatName = "uint8";
constexpr const char* kF16CanvasPixelFormatName = "float16";

// Return the CanvasColorSpace for the specified |name|. On invalid inputs,
// returns CanvasColorSpace::kSRGB.
CanvasColorSpace PLATFORM_EXPORT
CanvasColorSpaceFromName(const String& color_space_name);

// Return the SkColorSpace for the specified |color_space|.
sk_sp<SkColorSpace> PLATFORM_EXPORT
CanvasColorSpaceToSkColorSpace(CanvasColorSpace color_space);

// Return the named CanvasColorSpace that best matches |sk_color_space|.
CanvasColorSpace PLATFORM_EXPORT
CanvasColorSpaceFromSkColorSpace(const SkColorSpace* sk_color_space);

class PLATFORM_EXPORT CanvasColorParams {
  DISALLOW_NEW();

 public:
  // The default constructor will create an output-blended 8-bit surface.
  CanvasColorParams();
  CanvasColorParams(CanvasColorSpace, CanvasPixelFormat, OpacityMode);
  CanvasColorParams(const WTF::String& color_space,
                    const WTF::String& pixel_format,
                    bool has_alpha);

  CanvasColorSpace ColorSpace() const { return color_space_; }
  CanvasPixelFormat PixelFormat() const { return pixel_format_; }
  OpacityMode GetOpacityMode() const { return opacity_mode_; }

  const char* GetColorSpaceAsString() const;
  const char* GetPixelFormatAsString() const;

  CanvasResourceParams GetAsResourceParams() const;

  // The pixel format to use for allocating SkSurfaces.
  SkColorType GetSkColorType() const;

  // Return the color space of the underlying data for the canvas.
  gfx::ColorSpace GetStorageGfxColorSpace() const;
  sk_sp<SkColorSpace> GetSkColorSpace() const;

  uint8_t BytesPerPixel() const;

 private:

  CanvasColorSpace color_space_ = CanvasColorSpace::kSRGB;
  CanvasPixelFormat pixel_format_ = CanvasPixelFormat::kUint8;
  OpacityMode opacity_mode_ = kNonOpaque;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_COLOR_PARAMS_H_
