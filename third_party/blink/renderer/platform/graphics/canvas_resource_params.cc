// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_params.h"

#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "skia/ext/legacy_display_globals.h"
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
    case CanvasColorSpace::kRec2020:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                             gfx::ColorSpace::TransferID::GAMMA24);
    case CanvasColorSpace::kP3:
      return gfx::ColorSpace::CreateDisplayP3D65();
  }
  NOTREACHED();
}

}  // namespace

CanvasResourceParams::CanvasResourceParams() = default;

CanvasResourceParams::CanvasResourceParams(CanvasColorSpace color_space,
                                           SkColorType color_type,
                                           SkAlphaType alpha_type)
    : color_space_(color_space),
      color_type_(color_type),
      alpha_type_(alpha_type) {}

CanvasResourceParams::CanvasResourceParams(const SkImageInfo& info)
    : CanvasResourceParams(info.refColorSpace(), info.colorType()) {
  // TODO(https://crbug.com/1157747): This ignores |info|'s SkAlphaType.
}

SkSurfaceProps CanvasResourceParams::GetSkSurfaceProps() const {
  return skia::LegacyDisplayGlobals::ComputeSurfaceProps(CanUseLcdText());
}

uint8_t CanvasResourceParams::BytesPerPixel() const {
  return SkColorTypeBytesPerPixel(color_type_);
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
  switch (color_type_) {
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

GLenum CanvasResourceParams::GLSizedInternalFormat() const {
  switch (color_type_) {
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
  switch (color_type_) {
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
  color_type_ = kN32_SkColorType;
  if (sk_color_type == kRGBA_F16_SkColorType ||
      sk_color_type == kRGBA_8888_SkColorType ||
      sk_color_type == kRGB_888x_SkColorType) {
    color_type_ = sk_color_type;
  }
}

}  // namespace blink
