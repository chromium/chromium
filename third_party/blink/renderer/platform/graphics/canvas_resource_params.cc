// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_params.h"

#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/khronos/GLES3/gl3.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/color_space.h"

namespace blink {

namespace {

// The CanvasColorSpace value definitions are specified in the CSS Color Level 4
// specification.
gfx::ColorSpace CanvasColorSpaceToGfxColorSpace(CanvasColorSpace color_space) {
  switch (color_space) {
    case CanvasColorSpace::kSRGB:
      return gfx::ColorSpace::CreateSRGB();
      break;
    case CanvasColorSpace::kRec2020:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                             gfx::ColorSpace::TransferID::GAMMA24);
      break;
    case CanvasColorSpace::kP3:
      return gfx::ColorSpace::CreateDisplayP3D65();
      break;
  }
  NOTREACHED();
}

}  // namespace

CanvasResourceParams::CanvasResourceParams() = default;

CanvasResourceParams::CanvasResourceParams(CanvasColorSpace color_space,
                                           CanvasPixelFormat pixel_format,
                                           OpacityMode opacity_mode)
    : color_space_(color_space),
      pixel_format_(pixel_format),
      opacity_mode_(opacity_mode) {}

CanvasResourceParams::CanvasResourceParams(const SkImageInfo& info)
    : CanvasResourceParams(info.refColorSpace(), info.colorType()) {}

SkColorType CanvasResourceParams::GetSkColorType() const {
  switch (pixel_format_) {
    case CanvasPixelFormat::kF16:
      return kRGBA_F16_SkColorType;
    case CanvasPixelFormat::kRGBA8:
      return kRGBA_8888_SkColorType;
    case CanvasPixelFormat::kBGRA8:
      return kBGRA_8888_SkColorType;
    case CanvasPixelFormat::kRGBX8:
      return kRGB_888x_SkColorType;
  }
  NOTREACHED();
  return kN32_SkColorType;
}

SkAlphaType CanvasResourceParams::GetSkAlphaType() const {
  return opacity_mode_ == kOpaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
}

const SkSurfaceProps* CanvasResourceParams::GetSkSurfaceProps() const {
  static const SkSurfaceProps disable_lcd_props(0, kUnknown_SkPixelGeometry);
  if (opacity_mode_ == kOpaque)
    return nullptr;
  return &disable_lcd_props;
}

uint8_t CanvasResourceParams::BytesPerPixel() const {
  return SkColorTypeBytesPerPixel(GetSkColorType());
}

gfx::ColorSpace CanvasResourceParams::GetSamplerGfxColorSpace() const {
  // TODO(ccameron): If we add support for uint8srgb as a pixel format, this
  // will need to take into account whether or not this texture will be sampled
  // in linear or nonlinear space.
  return CanvasColorSpaceToGfxColorSpace(color_space_);
}

gfx::ColorSpace CanvasResourceParams::GetStorageGfxColorSpace() const {
  return CanvasColorSpaceToGfxColorSpace(color_space_);
}

sk_sp<SkColorSpace> CanvasResourceParams::GetSkColorSpace() const {
  static_assert(kN32_SkColorType == kRGBA_8888_SkColorType ||
                    kN32_SkColorType == kBGRA_8888_SkColorType,
                "Unexpected kN32_SkColorType value.");
  return CanvasColorSpaceToSkColorSpace(color_space_);
}

gfx::BufferFormat CanvasResourceParams::GetBufferFormat() const {
  switch (GetSkColorType()) {
    case kRGBA_F16_SkColorType:
      return gfx::BufferFormat::RGBA_F16;
    case kRGBA_8888_SkColorType:
      return gfx::BufferFormat::RGBA_8888;
    case kBGRA_8888_SkColorType:
      return gfx::BufferFormat::BGRA_8888;
    case kRGB_888x_SkColorType:
      return gfx::BufferFormat::RGBX_8888;
    default:
      NOTREACHED();
  }

  return gfx::BufferFormat::RGBA_8888;
}

GLenum CanvasResourceParams::GLUnsizedInternalFormat() const {
  // TODO(junov): try GL_RGB when opacity_mode_ == kOpaque
  switch (GetSkColorType()) {
    case kRGBA_F16_SkColorType:
      return GL_RGBA;
    case kRGBA_8888_SkColorType:
      return GL_RGBA;
    case kBGRA_8888_SkColorType:
      return GL_BGRA_EXT;
    case kRGB_888x_SkColorType:
      return GL_RGB;
    default:
      NOTREACHED();
  }

  return GL_RGBA;
}

GLenum CanvasResourceParams::GLSizedInternalFormat() const {
  switch (GetSkColorType()) {
    case kRGBA_F16_SkColorType:
      return GL_RGBA16F;
    case kRGBA_8888_SkColorType:
      return GL_RGBA8;
    case kBGRA_8888_SkColorType:
      return GL_BGRA8_EXT;
    case kRGB_888x_SkColorType:
      return GL_RGB8;
    default:
      NOTREACHED();
  }

  return GL_RGBA8;
}

GLenum CanvasResourceParams::GLType() const {
  switch (GetSkColorType()) {
    case kRGBA_F16_SkColorType:
      return GL_HALF_FLOAT_OES;
    case kRGBA_8888_SkColorType:
    case kBGRA_8888_SkColorType:
    case kRGB_888x_SkColorType:
      return GL_UNSIGNED_BYTE;
    default:
      NOTREACHED();
  }

  return GL_UNSIGNED_BYTE;
}

viz::ResourceFormat CanvasResourceParams::TransferableResourceFormat() const {
  return viz::GetResourceFormat(GetBufferFormat());
}

CanvasResourceParams::CanvasResourceParams(
    const sk_sp<SkColorSpace> sk_color_space,
    SkColorType sk_color_type) {
  color_space_ = CanvasColorSpaceFromSkColorSpace(sk_color_space.get());
  pixel_format_ = GetNativeCanvasPixelFormat();

  if (sk_color_type == kRGBA_F16_SkColorType)
    pixel_format_ = CanvasPixelFormat::kF16;
  else if (sk_color_type == kRGBA_8888_SkColorType)
    pixel_format_ = CanvasPixelFormat::kRGBA8;
  else if (sk_color_type == kRGB_888x_SkColorType)
    pixel_format_ = CanvasPixelFormat::kRGBX8;
}

}  // namespace blink
