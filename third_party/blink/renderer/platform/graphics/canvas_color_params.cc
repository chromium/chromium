// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"

#include "cc/paint/skia_paint_canvas.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/khronos/GLES3/gl3.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/color_space.h"

namespace blink {

// The PredefinedColorSpace value definitions are specified in the CSS Color
// Level 4 specification.
gfx::ColorSpace PredefinedColorSpaceToGfxColorSpace(
    PredefinedColorSpace color_space) {
  switch (color_space) {
    case PredefinedColorSpace::kSRGB:
      return gfx::ColorSpace::CreateSRGB();
    case PredefinedColorSpace::kRec2020:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                             gfx::ColorSpace::TransferID::GAMMA24);
    case PredefinedColorSpace::kP3:
      return gfx::ColorSpace::CreateDisplayP3D65();
    case PredefinedColorSpace::kRec2100HLG:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                             gfx::ColorSpace::TransferID::HLG);
    case PredefinedColorSpace::kRec2100PQ:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                             gfx::ColorSpace::TransferID::PQ);
    case PredefinedColorSpace::kSRGBLinear:
      return gfx::ColorSpace::CreateSRGBLinear();
  }
  NOTREACHED_IN_MIGRATION();
}

sk_sp<SkColorSpace> PredefinedColorSpaceToSkColorSpace(
    PredefinedColorSpace color_space) {
  return PredefinedColorSpaceToGfxColorSpace(color_space).ToSkColorSpace();
}

PredefinedColorSpace PredefinedColorSpaceFromSkColorSpace(
    const SkColorSpace* sk_color_space) {
  // TODO(https://crbug.com/1121448): This function returns sRGB if
  // |sk_color_space| does not exactly match one of the named color spaces. It
  // should find the best named match.
  PredefinedColorSpace color_spaces[] = {
      PredefinedColorSpace::kSRGB,      PredefinedColorSpace::kRec2020,
      PredefinedColorSpace::kP3,        PredefinedColorSpace::kRec2100HLG,
      PredefinedColorSpace::kRec2100PQ, PredefinedColorSpace::kSRGBLinear,
  };
  for (const auto& color_space : color_spaces) {
    if (SkColorSpace::Equals(sk_color_space,
                             PredefinedColorSpaceToGfxColorSpace(color_space)
                                 .ToSkColorSpace()
                                 .get())) {
      return color_space;
    }
  }
  return PredefinedColorSpace::kSRGB;
}

SkColorType CanvasPixelFormatToSkColorType(CanvasPixelFormat pixel_format) {
  switch (pixel_format) {
    case CanvasPixelFormat::kF16:
      return kRGBA_F16_SkColorType;
    case CanvasPixelFormat::kUint8:
      return kN32_SkColorType;
  }
  NOTREACHED_IN_MIGRATION();
  return kN32_SkColorType;
}

CanvasColorParams::CanvasColorParams() = default;

CanvasColorParams::CanvasColorParams(PredefinedColorSpace color_space,
                                     CanvasPixelFormat pixel_format,
                                     OpacityMode opacity_mode)
    : color_space_(color_space),
      pixel_format_(pixel_format),
      opacity_mode_(opacity_mode) {}

CanvasColorParams::CanvasColorParams(PredefinedColorSpace color_space,
                                     CanvasPixelFormat pixel_format,
                                     bool has_alpha)
    : color_space_(color_space),
      pixel_format_(pixel_format),
      opacity_mode_(has_alpha ? OpacityMode::kNonOpaque
                              : OpacityMode::kOpaque) {}

SkColorInfo CanvasColorParams::GetSkColorInfo() const {
  return SkColorInfo(
      GetSkColorType(),
      opacity_mode_ == kOpaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType,
      GetSkColorSpace());
}

String CanvasColorParams::GetColorSpaceAsString() const {
  return PredefinedColorSpaceName(color_space_);
}

String CanvasColorParams::GetPixelFormatAsString() const {
  return CanvasPixelFormatName(pixel_format_);
}

SkColorType CanvasColorParams::GetSkColorType() const {
  return CanvasPixelFormatToSkColorType(pixel_format_);
}


uint8_t CanvasColorParams::BytesPerPixel() const {
  return SkColorTypeBytesPerPixel(GetSkColorType());
}

gfx::ColorSpace CanvasColorParams::GetStorageGfxColorSpace() const {
  return PredefinedColorSpaceToGfxColorSpace(color_space_);
}

sk_sp<SkColorSpace> CanvasColorParams::GetSkColorSpace() const {
  static_assert(kN32_SkColorType == kRGBA_8888_SkColorType ||
                    kN32_SkColorType == kBGRA_8888_SkColorType,
                "Unexpected kN32_SkColorType value.");
  return PredefinedColorSpaceToSkColorSpace(color_space_);
}

}  // namespace blink
