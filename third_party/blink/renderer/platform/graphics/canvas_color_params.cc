// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"

#include "cc/paint/skia_paint_canvas.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/khronos/GLES3/gl3.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/color_space.h"

namespace blink {

namespace {

gfx::ColorSpace::PrimaryID GetPrimaryID(CanvasColorSpace color_space) {
  gfx::ColorSpace::PrimaryID primary_id = gfx::ColorSpace::PrimaryID::BT709;
  switch (color_space) {
    case kSRGBCanvasColorSpace:
    case kLinearRGBCanvasColorSpace:
      primary_id = gfx::ColorSpace::PrimaryID::BT709;
      break;
    case kRec2020CanvasColorSpace:
      primary_id = gfx::ColorSpace::PrimaryID::BT2020;
      break;
    case kP3CanvasColorSpace:
      primary_id = gfx::ColorSpace::PrimaryID::SMPTEST432_1;
      break;
  }
  return primary_id;
}

}  // namespace

CanvasColorParams::CanvasColorParams() = default;

CanvasColorParams::CanvasColorParams(CanvasColorSpace color_space,
                                     CanvasPixelFormat pixel_format,
                                     OpacityMode opacity_mode)
    : color_space_(color_space),
      pixel_format_(pixel_format),
      opacity_mode_(opacity_mode) {}

CanvasColorParams::CanvasColorParams(const sk_sp<SkColorSpace> color_space,
                                     SkColorType color_type) {
  color_space_ = kSRGBCanvasColorSpace;
  pixel_format_ = kRGBA8CanvasPixelFormat;
  // When there is no color space information, the SkImage is in legacy mode and
  // the color type is kN32_SkColorType (which translates to kRGBA8 canvas pixel
  // format).
  if (!color_space)
    return;
  // kSRGBCanvasColorSpace covers sRGB and e-sRGB. We need to check for
  // linear-rgb, rec2020 and p3.
  if (SkColorSpace::Equals(color_space.get(),
                           SkColorSpace::MakeSRGB()->makeLinearGamma().get())) {
    color_space_ = kLinearRGBCanvasColorSpace;
  } else if (SkColorSpace::Equals(
                 color_space.get(),
                 SkColorSpace::MakeRGB(SkColorSpace::kLinear_RenderTargetGamma,
                                       SkColorSpace::kRec2020_Gamut)
                     .get())) {
    color_space_ = kRec2020CanvasColorSpace;
  } else if (SkColorSpace::Equals(
                 color_space.get(),
                 SkColorSpace::MakeRGB(SkColorSpace::kLinear_RenderTargetGamma,
                                       SkColorSpace::kDCIP3_D65_Gamut)
                     .get())) {
    color_space_ = kP3CanvasColorSpace;
  }
  if (color_type == kRGBA_F16_SkColorType)
    pixel_format_ = kF16CanvasPixelFormat;
}

CanvasColorParams::CanvasColorParams(const SkImageInfo& info)
    : CanvasColorParams(info.refColorSpace(), info.colorType()) {}

void CanvasColorParams::SetCanvasColorSpace(CanvasColorSpace color_space) {
  color_space_ = color_space;
}

void CanvasColorParams::SetCanvasPixelFormat(CanvasPixelFormat pixel_format) {
  pixel_format_ = pixel_format;
}

void CanvasColorParams::SetOpacityMode(OpacityMode opacity_mode) {
  opacity_mode_ = opacity_mode;
}

bool CanvasColorParams::NeedsSkColorSpaceXformCanvas() const {
  return color_space_ == kSRGBCanvasColorSpace &&
         pixel_format_ == kRGBA8CanvasPixelFormat;
}

std::unique_ptr<cc::PaintCanvas> CanvasColorParams::WrapCanvas(
    SkCanvas* canvas) const {
  if (NeedsSkColorSpaceXformCanvas()) {
    return std::make_unique<cc::SkiaPaintCanvas>(canvas, GetSkColorSpace());
  }
  // |canvas| already does its own color correction.
  return std::make_unique<cc::SkiaPaintCanvas>(canvas);
}

sk_sp<SkColorSpace> CanvasColorParams::GetSkColorSpaceForSkSurfaces() const {
  if (NeedsSkColorSpaceXformCanvas())
    return nullptr;
  return GetSkColorSpace();
}

bool CanvasColorParams::NeedsColorConversion(
    const CanvasColorParams& dest_color_params) const {
  if ((color_space_ == dest_color_params.ColorSpace() &&
       pixel_format_ == dest_color_params.PixelFormat()) ||
      (NeedsSkColorSpaceXformCanvas() &&
       dest_color_params.NeedsSkColorSpaceXformCanvas()))
    return false;
  return true;
}

SkColorType CanvasColorParams::GetSkColorType() const {
  if (pixel_format_ == kF16CanvasPixelFormat)
    return kRGBA_F16_SkColorType;
  return kN32_SkColorType;
}

SkAlphaType CanvasColorParams::GetSkAlphaType() const {
  if (opacity_mode_ == kOpaque)
    return kOpaque_SkAlphaType;
  return kPremul_SkAlphaType;
}

const SkSurfaceProps* CanvasColorParams::GetSkSurfaceProps() const {
  static const SkSurfaceProps disable_lcd_props(0, kUnknown_SkPixelGeometry);
  if (opacity_mode_ == kOpaque)
    return nullptr;
  return &disable_lcd_props;
}

uint8_t CanvasColorParams::BytesPerPixel() const {
  return SkColorTypeBytesPerPixel(GetSkColorType());
}

gfx::ColorSpace CanvasColorParams::GetSamplerGfxColorSpace() const {
  gfx::ColorSpace::PrimaryID primary_id = GetPrimaryID(color_space_);

  // TODO(ccameron): This needs to take into account whether or not this texture
  // will be sampled in linear or nonlinear space.
  gfx::ColorSpace::TransferID transfer_id =
      gfx::ColorSpace::TransferID::IEC61966_2_1;
  if (pixel_format_ == kF16CanvasPixelFormat)
    transfer_id = gfx::ColorSpace::TransferID::LINEAR_HDR;

  return gfx::ColorSpace(primary_id, transfer_id);
}

gfx::ColorSpace CanvasColorParams::GetStorageGfxColorSpace() const {
  gfx::ColorSpace::PrimaryID primary_id = GetPrimaryID(color_space_);

  gfx::ColorSpace::TransferID transfer_id =
      gfx::ColorSpace::TransferID::IEC61966_2_1;
  // Only sRGB and e-sRGB use sRGB transfer function. Other canvas color spaces,
  // i.e., linear-rgb, p3 and rec2020 use linear transfer function.
  if (color_space_ != kSRGBCanvasColorSpace)
    transfer_id = gfx::ColorSpace::TransferID::LINEAR_HDR;

  return gfx::ColorSpace(primary_id, transfer_id);
}

sk_sp<SkColorSpace> CanvasColorParams::GetSkColorSpace() const {
  SkColorSpace::Gamut gamut = SkColorSpace::kSRGB_Gamut;
  SkColorSpace::RenderTargetGamma gamma = SkColorSpace::kSRGB_RenderTargetGamma;
  switch (color_space_) {
    case kSRGBCanvasColorSpace:
      break;
    case kLinearRGBCanvasColorSpace:
      gamma = SkColorSpace::kLinear_RenderTargetGamma;
      break;
    case kRec2020CanvasColorSpace:
      gamut = SkColorSpace::kRec2020_Gamut;
      gamma = SkColorSpace::kLinear_RenderTargetGamma;
      break;
    case kP3CanvasColorSpace:
      gamut = SkColorSpace::kDCIP3_D65_Gamut;
      gamma = SkColorSpace::kLinear_RenderTargetGamma;
      break;
  }
  return SkColorSpace::MakeRGB(gamma, gamut);
}

gfx::BufferFormat CanvasColorParams::GetBufferFormat() const {
  static_assert(kN32_SkColorType == kRGBA_8888_SkColorType ||
                    kN32_SkColorType == kBGRA_8888_SkColorType,
                "Unexpected kN32_SkColorType value.");
  constexpr gfx::BufferFormat kN32BufferFormat =
      kN32_SkColorType == kRGBA_8888_SkColorType ? gfx::BufferFormat::RGBA_8888
                                                 : gfx::BufferFormat::BGRA_8888;

  if (pixel_format_ == kF16CanvasPixelFormat)
    return gfx::BufferFormat::RGBA_F16;

  return kN32BufferFormat;
}

GLenum CanvasColorParams::GLUnsizedInternalFormat() const {
  // TODO(junov): try GL_RGB when opacity_mode_ == kOpaque
  static_assert(kN32_SkColorType == kRGBA_8888_SkColorType ||
                    kN32_SkColorType == kBGRA_8888_SkColorType,
                "Unexpected kN32_SkColorType value.");
  constexpr GLenum kN32GLUnsizedInternalBufferFormat =
      kN32_SkColorType == kRGBA_8888_SkColorType ? GL_RGBA : GL_BGRA_EXT;
  if (pixel_format_ == kF16CanvasPixelFormat)
    return GL_RGBA;

  return kN32GLUnsizedInternalBufferFormat;
}

GLenum CanvasColorParams::GLSizedInternalFormat() const {
  static_assert(kN32_SkColorType == kRGBA_8888_SkColorType ||
                    kN32_SkColorType == kBGRA_8888_SkColorType,
                "Unexpected kN32_SkColorType value.");
  constexpr GLenum kN32GLSizedInternalBufferFormat =
      kN32_SkColorType == kRGBA_8888_SkColorType ? GL_RGBA8 : GL_BGRA8_EXT;
  if (pixel_format_ == kF16CanvasPixelFormat)
    return GL_RGBA16F;
  return kN32GLSizedInternalBufferFormat;
}

GLenum CanvasColorParams::GLType() const {
  switch (pixel_format_) {
    case kRGBA8CanvasPixelFormat:
      return GL_UNSIGNED_BYTE;
    case kF16CanvasPixelFormat:
      return GL_HALF_FLOAT_OES;
    default:
      break;
  }
  NOTREACHED();
  return GL_UNSIGNED_BYTE;
}

viz::ResourceFormat CanvasColorParams::TransferableResourceFormat() const {
  switch (pixel_format_) {
    case kRGBA8CanvasPixelFormat:
      return viz::RGBA_8888;
    case kF16CanvasPixelFormat:
      return viz::RGBA_F16;
    default:
      break;
  }
  NOTREACHED();
  return viz::RGBA_8888;
}

}  // namespace blink
