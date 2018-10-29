// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_COLOR_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_COLOR_PARAMS_H_

#include "components/viz/common/resources/resource_format.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/buffer_types.h"

class SkCanvas;

namespace cc {
class PaintCanvas;
}

namespace gfx {
class ColorSpace;
}

namespace blink {

enum CanvasColorSpace {
  kSRGBCanvasColorSpace,
  kLinearRGBCanvasColorSpace,
  kRec2020CanvasColorSpace,
  kP3CanvasColorSpace,
};

enum CanvasPixelFormat {
  kRGBA8CanvasPixelFormat,
  kF16CanvasPixelFormat,
};

class PLATFORM_EXPORT CanvasColorParams {
 public:
  // The default constructor will create an output-blended 8-bit surface.
  CanvasColorParams();
  CanvasColorParams(CanvasColorSpace, CanvasPixelFormat, OpacityMode);
  explicit CanvasColorParams(const sk_sp<SkColorSpace> color_space,
                             SkColorType color_type);
  explicit CanvasColorParams(const SkImageInfo&);
  CanvasColorSpace ColorSpace() const { return color_space_; }
  CanvasPixelFormat PixelFormat() const { return pixel_format_; }
  OpacityMode GetOpacityMode() const { return opacity_mode_; }

  void SetCanvasColorSpace(CanvasColorSpace);
  void SetCanvasPixelFormat(CanvasPixelFormat);
  void SetOpacityMode(OpacityMode);

  // Indicates whether rendering needs to go through an SkColorSpaceXformCanvas
  // in order to enforce non-gamma-aware pixel math behaviour.
  bool NeedsSkColorSpaceXformCanvas() const;

  // Indicates if pixels in this canvas color settings require any color
  // conversion to be used in the passed canvas color settings.
  bool NeedsColorConversion(const CanvasColorParams&) const;

  // The SkColorSpace to use in the SkImageInfo for allocated SkSurfaces. This
  // is nullptr in legacy rendering mode and when the surface is supposed to be
  // in sRGB (for which we wrap the canvas into a PaintCanvas along with an
  // SkColorSpaceXformCanvas).
  sk_sp<SkColorSpace> GetSkColorSpaceForSkSurfaces() const;

  // Wraps an SkCanvas into a PaintCanvas, along with an SkColorSpaceXformCanvas
  // if necessary.
  std::unique_ptr<cc::PaintCanvas> WrapCanvas(SkCanvas*) const;

  // The pixel format to use for allocating SkSurfaces.
  SkColorType GetSkColorType() const;
  uint8_t BytesPerPixel() const;

  // The color space in which pixels read from the canvas via a shader will be
  // returned. Note that for canvases with linear pixel math, these will be
  // converted from their storage space into a linear space.
  gfx::ColorSpace GetSamplerGfxColorSpace() const;

  // Return the color space of the underlying data for the canvas.
  gfx::ColorSpace GetStorageGfxColorSpace() const;
  sk_sp<SkColorSpace> GetSkColorSpace() const;
  SkAlphaType GetSkAlphaType() const;
  const SkSurfaceProps* GetSkSurfaceProps() const;

  // Gpu memory buffer parameters
  gfx::BufferFormat GetBufferFormat() const;
  uint32_t GLSizedInternalFormat() const;  // For GLES2, use Unsized
  uint32_t GLUnsizedInternalFormat() const;
  uint32_t GLType() const;

  viz::ResourceFormat TransferableResourceFormat() const;

 private:
  CanvasColorSpace color_space_ = kSRGBCanvasColorSpace;
  CanvasPixelFormat pixel_format_ = kRGBA8CanvasPixelFormat;
  OpacityMode opacity_mode_ = kNonOpaque;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_COLOR_PARAMS_H_
