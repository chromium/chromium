// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PARAMS_H_

#include "components/viz/common/resources/resource_format.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/buffer_types.h"

class SkSurfaceProps;

namespace gfx {
class ColorSpace;
}

namespace blink {

// TODO(https://crbug.com/1157747): This class is a copy-paste of
// CanvasColorParams. This should be changed to specify a SkColorType,
// SkAlphaType, SkColorSpace, and SkFilterMode.
class PLATFORM_EXPORT CanvasResourceParams {
  DISALLOW_NEW();

 public:
  // The default constructor will create an output-blended 8-bit surface.
  CanvasResourceParams();
  CanvasResourceParams(CanvasColorSpace, SkColorType, SkAlphaType);
  explicit CanvasResourceParams(const SkImageInfo&);

  CanvasColorSpace ColorSpace() const { return color_space_; }

  void SetCanvasColorSpace(CanvasColorSpace c) { color_space_ = c; }
  void SetSkColorType(SkColorType color_type) { color_type_ = color_type; }

  // The pixel format to use for allocating SkSurfaces.
  SkColorType GetSkColorType() const { return color_type_; }
  uint8_t BytesPerPixel() const;

  // The color space in which pixels read from the canvas via a shader will be
  // returned. Note that for canvases with linear pixel math, these will be
  // converted from their storage space into a linear space.
  gfx::ColorSpace GetSamplerGfxColorSpace() const;

  // Return the color space of the underlying data for the canvas.
  gfx::ColorSpace GetStorageGfxColorSpace() const;
  sk_sp<SkColorSpace> GetSkColorSpace() const;
  SkAlphaType GetSkAlphaType() const { return alpha_type_; }
  const SkSurfaceProps* GetSkSurfaceProps() const;

  // Gpu memory buffer parameters
  gfx::BufferFormat GetBufferFormat() const;
  uint32_t GLSizedInternalFormat() const;  // For GLES2, use Unsized
  uint32_t GLUnsizedInternalFormat() const;
  uint32_t GLType() const;

  viz::ResourceFormat TransferableResourceFormat() const;

 private:
  CanvasResourceParams(const sk_sp<SkColorSpace> color_space,
                       SkColorType color_type);

  CanvasColorSpace color_space_ = CanvasColorSpace::kSRGB;
  SkColorType color_type_ = kN32_SkColorType;
  SkAlphaType alpha_type_ = kPremul_SkAlphaType;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PARAMS_H_
