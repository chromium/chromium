// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"

#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_params.h"
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

sk_sp<SkColorSpace> CanvasColorSpaceToSkColorSpace(
    CanvasColorSpace color_space) {
  return CanvasColorSpaceToGfxColorSpace(color_space).ToSkColorSpace();
}

CanvasColorSpace CanvasColorSpaceFromSkColorSpace(
    const SkColorSpace* sk_color_space) {
  // TODO(https://crbug.com/1121448): This function returns sRGB if
  // |sk_color_space| does not exactly match one of the named color spaces. It
  // should find the best named match.
  CanvasColorSpace color_spaces[] = {
      CanvasColorSpace::kSRGB,
      CanvasColorSpace::kRec2020,
      CanvasColorSpace::kP3,
  };
  for (const auto& color_space : color_spaces) {
    if (SkColorSpace::Equals(sk_color_space,
                             CanvasColorSpaceToGfxColorSpace(color_space)
                                 .ToSkColorSpace()
                                 .get())) {
      return color_space;
    }
  }
  return CanvasColorSpace::kSRGB;
}

CanvasColorSpace CanvasColorSpaceFromName(const String& color_space_name) {
  if (color_space_name == kRec2020CanvasColorSpaceName)
    return CanvasColorSpace::kRec2020;
  if (color_space_name == kP3CanvasColorSpaceName)
    return CanvasColorSpace::kP3;
  return CanvasColorSpace::kSRGB;
}

String CanvasColorSpaceToName(CanvasColorSpace color_space) {
  switch (color_space) {
    case CanvasColorSpace::kSRGB:
      return kSRGBCanvasColorSpaceName;
    case CanvasColorSpace::kRec2020:
      return kRec2020CanvasColorSpaceName;
    case CanvasColorSpace::kP3:
      return kP3CanvasColorSpaceName;
  };
  NOTREACHED();
}

CanvasColorParams::CanvasColorParams() = default;

CanvasColorParams::CanvasColorParams(CanvasColorSpace color_space,
                                     CanvasPixelFormat pixel_format,
                                     OpacityMode opacity_mode)
    : color_space_(color_space),
      pixel_format_(pixel_format),
      opacity_mode_(opacity_mode) {}

CanvasColorParams::CanvasColorParams(const WTF::String& color_space,
                                     const WTF::String& pixel_format,
                                     bool has_alpha) {
  if (color_space == kRec2020CanvasColorSpaceName)
    color_space_ = CanvasColorSpace::kRec2020;
  else if (color_space == kP3CanvasColorSpaceName)
    color_space_ = CanvasColorSpace::kP3;

  if (pixel_format == kF16CanvasPixelFormatName)
    pixel_format_ = CanvasPixelFormat::kF16;

  if (!has_alpha)
    opacity_mode_ = kOpaque;
}

CanvasResourceParams CanvasColorParams::GetAsResourceParams() const {
  SkAlphaType alpha_type =
      opacity_mode_ == kOpaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
  return CanvasResourceParams(color_space_, GetSkColorType(), alpha_type);
}

String CanvasColorParams::GetColorSpaceAsString() const {
  return CanvasColorSpaceToName(color_space_);
}

const char* CanvasColorParams::GetPixelFormatAsString() const {
  switch (pixel_format_) {
    case CanvasPixelFormat::kF16:
      return kF16CanvasPixelFormatName;
    case CanvasPixelFormat::kUint8:
      return kUint8CanvasPixelFormatName;
  };
  CHECK(false);
  return "";
}

SkColorType CanvasColorParams::GetSkColorType() const {
  switch (pixel_format_) {
    case CanvasPixelFormat::kF16:
      return kRGBA_F16_SkColorType;
    case CanvasPixelFormat::kUint8:
      return kN32_SkColorType;
  }
  NOTREACHED();
  return kN32_SkColorType;
}


uint8_t CanvasColorParams::BytesPerPixel() const {
  return SkColorTypeBytesPerPixel(GetSkColorType());
}

gfx::ColorSpace CanvasColorParams::GetStorageGfxColorSpace() const {
  return CanvasColorSpaceToGfxColorSpace(color_space_);
}

sk_sp<SkColorSpace> CanvasColorParams::GetSkColorSpace() const {
  static_assert(kN32_SkColorType == kRGBA_8888_SkColorType ||
                    kN32_SkColorType == kBGRA_8888_SkColorType,
                "Unexpected kN32_SkColorType value.");
  return CanvasColorSpaceToSkColorSpace(color_space_);
}

}  // namespace blink
