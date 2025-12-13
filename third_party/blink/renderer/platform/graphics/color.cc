/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/color.h"

#include <math.h>

#include <array>
#include <optional>
#include <tuple>

#include "base/check_op.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_view_util.h"
#include "build/build_config.h"
#include "skia/ext/skcms_ext.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/color_conversions.h"

namespace blink {

const Color Color::kBlack = Color(0xFF000000);
const Color Color::kWhite = Color(0xFFFFFFFF);
const Color Color::kDarkGray = Color(0xFF808080);
const Color Color::kGray = Color(0xFFA0A0A0);
const Color Color::kLightGray = Color(0xFFC0C0C0);
const Color Color::kTransparent = Color(0x00000000);

namespace {

// For lch/oklch colors, the value of chroma underneath which the color is
// considered to be "achromatic", relevant for color conversions.
// https://www.w3.org/TR/css-color-4/#lab-to-lch
// This is set to be slightly higher than white's chroma value of 0.0188.
const float kAchromaticChromaThreshold = 0.02;

const int kCStartAlpha = 153;     // 60%
const int kCEndAlpha = 204;       // 80%;
const int kCAlphaIncrement = 17;  // Increments in between.

int BlendComponent(int c, int a) {
  // We use white.
  float alpha = a / 255.0f;
  int white_blend = 255 - a;
  c -= white_blend;
  return static_cast<int>(c / alpha);
}

// originally moved here from the CSS parser
template <typename CharacterType>
inline bool ParseHexColorInternal(base::span<const CharacterType> name,
                                  Color& color) {
  if (name.size() != 3 && name.size() != 4 && name.size() != 6 &&
      name.size() != 8) {
    return false;
  }
  if ((name.size() == 8 || name.size() == 4) &&
      !RuntimeEnabledFeatures::CSSHexAlphaColorEnabled()) {
    return false;
  }
  uint32_t value = 0;
  for (unsigned i = 0; i < name.size(); ++i) {
    if (!IsASCIIHexDigit(name[i]))
      return false;
    value <<= 4;
    value |= ToASCIIHexValue(name[i]);
  }
  if (name.size() == 6) {
    color = Color::FromRGBA32(0xFF000000 | value);
    return true;
  }
  if (name.size() == 8) {
    // We parsed the values into RGBA order, but the RGBA32 type
    // expects them to be in ARGB order, so we right rotate eight bits.
    color = Color::FromRGBA32(value << 24 | value >> 8);
    return true;
  }
  if (name.size() == 4) {
    // #abcd converts to ddaabbcc in RGBA32.
    color = Color::FromRGBA32((value & 0xF) << 28 | (value & 0xF) << 24 |
                              (value & 0xF000) << 8 | (value & 0xF000) << 4 |
                              (value & 0xF00) << 4 | (value & 0xF00) |
                              (value & 0xF0) | (value & 0xF0) >> 4);
    return true;
  }
  // #abc converts to #aabbcc
  color = Color::FromRGBA32(0xFF000000 | (value & 0xF00) << 12 |
                            (value & 0xF00) << 8 | (value & 0xF0) << 8 |
                            (value & 0xF0) << 4 | (value & 0xF) << 4 |
                            (value & 0xF));
  return true;
}

inline const NamedColor* FindNamedColor(const String& name) {
  std::array<char, 64> buffer;  // easily big enough for the longest color name
  wtf_size_t length = name.length();
  if (length > buffer.size() - 1) {
    return nullptr;
  }
  for (wtf_size_t i = 0; i < length; ++i) {
    const UChar c = name[i];
    if (!c || c > 0x7F)
      return nullptr;
    buffer[i] = ToASCIILower(static_cast<char>(c));
  }
  return FindColor(base::as_string_view(base::span(buffer).first(length)));
}

constexpr int RedChannel(RGBA32 color) {
  return (color >> 16) & 0xFF;
}

constexpr int GreenChannel(RGBA32 color) {
  return (color >> 8) & 0xFF;
}

constexpr int BlueChannel(RGBA32 color) {
  return color & 0xFF;
}

float AngleToUnitCircleDegrees(float angle) {
  return fmod(fmod(angle, 360.f) + 360.f, 360.f);
}

float QuantizeTo8Bit(float v) {
  const float scale_factor = nextafterf(256.0f, 0.0f);
  return static_cast<int>(v * scale_factor) / 255.0f;
}

// Many of the Color::ColorSpaces can be represented by an SkColorSpace. This
// function will return the matrix and transfer function for those spaces, and
// will CHECK for all other spaces.
void GetSkColorSpaceParams(Color::ColorSpace color_space,
                           skcms_Matrix3x3& m,
                           skcms_TransferFunction& t) {
  switch (color_space) {
    case Color::ColorSpace::kSRGB:
      m = SkNamedGamut::kSRGB;
      t = SkNamedTransferFn::kSRGB;
      return;
    case Color::ColorSpace::kSRGBLinear:
      m = SkNamedGamut::kSRGB;
      t = SkNamedTransferFn::kLinear;
      return;
    case Color::ColorSpace::kDisplayP3:
      m = SkNamedGamut::kDisplayP3;
      t = SkNamedTransferFn::kSRGB;
      return;
    case Color::ColorSpace::kDisplayP3Linear:
      m = SkNamedGamut::kDisplayP3;
      t = SkNamedTransferFn::kLinear;
      return;
    case Color::ColorSpace::kA98RGB:
      m = SkNamedGamut::kAdobeRGB;
      t = SkNamedTransferFn::k2Dot2;
      return;
    case Color::ColorSpace::kProPhotoRGB: {
      SkNamedPrimaries::kProPhotoRGB.toXYZD50(&m);
      t = SkNamedTransferFn::kProPhotoRGB;
      return;
    }
    case Color::ColorSpace::kRec2020:
      m = SkNamedGamut::kRec2020;
      t = SkNamedTransferFn::kRec2020;
      return;
    case Color::ColorSpace::kRec2100Linear:
      m = SkNamedGamut::kRec2020;
      t = SkNamedTransferFn::kLinear;
      return;
    case Color::ColorSpace::kXYZD50:
      m = SkNamedGamut::kXYZ;
      t = SkNamedTransferFn::kLinear;
      return;
    case Color::ColorSpace::kXYZD65: {
      constexpr float kD65_x = 0.3127f;
      constexpr float kD65_y = 0.3290f;
      skcms_AdaptToXYZD50(kD65_x, kD65_y, &m);
      t = SkNamedTransferFn::kLinear;
      return;
    }
    case Color::ColorSpace::kSRGBLegacy:
    case Color::ColorSpace::kLab:
    case Color::ColorSpace::kOklab:
    case Color::ColorSpace::kLch:
    case Color::ColorSpace::kOklch:
    case Color::ColorSpace::kHSL:
    case Color::ColorSpace::kHWB:
    case Color::ColorSpace::kNone:
      NOTREACHED();
  }
}

}  // namespace

// The color parameters will use 16 bytes (for 4 floats). Ensure that the
// remaining parameters fit into another 4 bytes.
static_assert(sizeof(Color) <= 20, "blink::Color should be <= 20 bytes.");

Color::Color(int r, int g, int b) {
  *this = FromRGB(r, g, b);
}

Color::Color(int r, int g, int b, int a) {
  *this = FromRGBA(r, g, b, a);
}

// static
Color Color::FromColorSpace(ColorSpace color_space,
                            std::optional<float> param0,
                            std::optional<float> param1,
                            std::optional<float> param2,
                            std::optional<float> alpha) {
  Color result;
  result.color_space_ = color_space;
  result.param0_is_none_ = !param0;
  result.param1_is_none_ = !param1;
  result.param2_is_none_ = !param2;
  result.alpha_is_none_ = !alpha;
  result.param0_ = param0.value_or(0.f);
  result.param1_ = param1.value_or(0.f);
  result.param2_ = param2.value_or(0.f);
  if (alpha) {
    // Alpha is clamped to the range [0,1], no matter what colorspace.
    result.alpha_ = ClampTo(alpha.value(), 0.f, 1.f);
  } else {
    result.alpha_ = 0.0f;
  }

  if (IsLightnessFirstComponent(color_space) && !isnan(result.param0_)) {
    // param0_ is lightness which cannot be negative or above 100%.
    // lab/lch have lightness in the range [0, 100].
    // oklab/okch have lightness in the range [0, 1].
    if (color_space == ColorSpace::kLab || color_space == ColorSpace::kLch) {
      result.param0_ = std::min(100.f, std::max(result.param0_, 0.f));
    } else {
      result.param0_ = std::min(1.f, std::max(result.param0_, 0.f));
    }
  }
  if (IsChromaSecondComponent(color_space)) {
    result.param1_ = std::max(result.param1_, 0.f);
  }

  return result;
}

// static
Color Color::FromHSLA(std::optional<float> h,
                      std::optional<float> s,
                      std::optional<float> l,
                      std::optional<float> a) {
  return FromColorSpace(ColorSpace::kHSL, h, s, l, a);
}

// static
Color Color::FromHWBA(std::optional<float> h,
                      std::optional<float> w,
                      std::optional<float> b,
                      std::optional<float> a) {
  return FromColorSpace(ColorSpace::kHWB, h, w, b, a);
}

// static
Color Color::FromColorMix(Color::ColorSpace interpolation_space,
                          std::optional<HueInterpolationMethod> hue_method,
                          Color color1,
                          Color color2,
                          float percentage,
                          float alpha_multiplier) {
  DCHECK(alpha_multiplier >= 0.0f && alpha_multiplier <= 1.0f);
  Color result = InterpolateColors(interpolation_space, hue_method, color1,
                                   color2, percentage);

  result.alpha_ *= alpha_multiplier;

  // Legacy colors that are the result of color-mix should serialize as
  // color(srgb ... ).
  // See: https://github.com/mozilla/wg-decisions/issues/1125
  if (result.IsLegacyColorSpace(result.color_space_)) {
    result.ConvertToColorSpace(Color::ColorSpace::kSRGB);
  }
  return result;
}

// static
float Color::HueInterpolation(float value1,
                              float value2,
                              float percentage,
                              Color::HueInterpolationMethod hue_method) {
  DCHECK(value1 >= 0.0f && value1 < 360.0f) << value1;
  DCHECK(value2 >= 0.0f && value2 < 360.0f) << value2;
  // Adapt values of angles if needed, depending on the hue_method.
  switch (hue_method) {
    case Color::HueInterpolationMethod::kShorter: {
      float diff = value2 - value1;
      if (diff > 180.0f) {
        value1 += 360.0f;
      } else if (diff < -180.0f) {
        value2 += 360.0f;
      }
      DCHECK(value2 - value1 >= -180.0f && value2 - value1 <= 180.0f);
    } break;
    case Color::HueInterpolationMethod::kLonger: {
      float diff = value2 - value1;
      if (diff > 0.0f && diff < 180.0f) {
        value1 += 360.0f;
      } else if (diff > -180.0f && diff <= 0.0f) {
        value2 += 360.0f;
      }
      DCHECK((value2 - value1 >= -360.0f && value2 - value1 <= -180.0f) ||
             (value2 - value1 >= 180.0f && value2 - value1 <= 360.0f))
          << value2 - value1;
    } break;
    case Color::HueInterpolationMethod::kIncreasing:
      if (value2 < value1)
        value2 += 360.0f;
      DCHECK(value2 - value1 >= 0.0f && value2 - value1 < 360.0f);
      break;
    case Color::HueInterpolationMethod::kDecreasing:
      if (value1 < value2)
        value1 += 360.0f;
      DCHECK(-360.0f < value2 - value1 && value2 - value1 <= 0.f);
      break;
  }
  return AngleToUnitCircleDegrees(blink::Blend(value1, value2, percentage));
}

std::array<bool, 3> Color::GetAnalogousMissingComponents(
    Color::ColorSpace interpolation_space) const {
  DCHECK_NE(color_space_, interpolation_space);

  auto is_rgb_or_xyz = [](ColorSpace color_space) {
    return color_space == ColorSpace::kSRGB ||
           color_space == ColorSpace::kSRGBLinear ||
           color_space == ColorSpace::kDisplayP3 ||
           color_space == ColorSpace::kDisplayP3Linear ||
           color_space == ColorSpace::kA98RGB ||
           color_space == ColorSpace::kProPhotoRGB ||
           color_space == ColorSpace::kRec2020 ||
           color_space == ColorSpace::kRec2100Linear ||
           color_space == ColorSpace::kXYZD50 ||
           color_space == ColorSpace::kXYZD65 ||
           color_space == ColorSpace::kSRGBLegacy;
  };
  auto is_lab = [](ColorSpace color_space) {
    return color_space == ColorSpace::kLab || color_space == ColorSpace::kOklab;
  };
  auto is_lch = [](ColorSpace color_space) {
    return color_space == ColorSpace::kLch || color_space == ColorSpace::kOklch;
  };

  const bool param0_is_none = Param0IsNone();
  const bool param1_is_none = Param1IsNone();
  const bool param2_is_none = Param2IsNone();

  switch (color_space_) {
    case ColorSpace::kSRGB:
    case ColorSpace::kSRGBLinear:
    case ColorSpace::kDisplayP3:
    case ColorSpace::kDisplayP3Linear:
    case ColorSpace::kA98RGB:
    case ColorSpace::kProPhotoRGB:
    case ColorSpace::kRec2020:
    case ColorSpace::kRec2100Linear:
    case ColorSpace::kXYZD50:
    case ColorSpace::kXYZD65:
    case ColorSpace::kSRGBLegacy:
      // Between RGB/XYZ spaces all components are analogous and in the same
      // order.
      if (is_rgb_or_xyz(interpolation_space)) {
        return {param0_is_none, param1_is_none, param2_is_none};
      }
      break;
    case ColorSpace::kLab:
    case ColorSpace::kOklab:
      // *Lab -> *Lab
      if (is_lab(interpolation_space)) {
        return {param0_is_none, param1_is_none, param2_is_none};
      }
      // Lightness carries forward to *lch (component 0).
      if (is_lch(interpolation_space)) {
        return {param0_is_none, false, false};
      }
      // Lightness carries forward to hsl (component 2).
      if (interpolation_space == ColorSpace::kHSL) {
        return {false, false, param0_is_none};
      }
      break;
    case ColorSpace::kLch:
    case ColorSpace::kOklch:
      // *Lch -> *Lch
      if (is_lch(interpolation_space)) {
        return {param0_is_none, param1_is_none, param2_is_none};
      }
      // All components carry forward to hsl, swapping component 0 with
      // component 2.
      if (interpolation_space == ColorSpace::kHSL) {
        return {param2_is_none, param1_is_none, param0_is_none};
      }
      // Lightness carries forward to *Lab (component 0).
      if (is_lab(interpolation_space)) {
        return {param0_is_none, false, false};
      }
      break;
    case ColorSpace::kHSL:
      // All components carry forward to *Lch, swapping component 0 with
      // component 2.
      if (is_lch(interpolation_space)) {
        return {param2_is_none, param1_is_none, param0_is_none};
      }
      // Lightness carries forward to *Lab (component 0).
      if (is_lab(interpolation_space)) {
        return {param2_is_none, false, false};
      }
      // Hue carries forward to hwb (component 0).
      if (interpolation_space == ColorSpace::kHWB) {
        return {param0_is_none, false, false};
      }
      break;
    case ColorSpace::kHWB:
      // Hue carries forward to hsl (component 0).
      if (interpolation_space == ColorSpace::kHSL) {
        return {param0_is_none, false, false};
      }
      // Hue carries forward to *Lch (component 2).
      if (is_lch(interpolation_space)) {
        return {false, false, param0_is_none};
      }
      break;
    case ColorSpace::kNone:
      break;
  }
  // There are no analogous components.
  return {};
}

void Color::CarryForwardAnalogousMissingComponents(
    const std::array<bool, 3>& missing_components) {
  if (missing_components[0]) {
    param0_ = 0;
    param0_is_none_ = true;
  }
  if (missing_components[1]) {
    param1_ = 0;
    param1_is_none_ = true;
  }
  if (missing_components[2]) {
    param2_ = 0;
    param2_is_none_ = true;
  }
}

// static
bool Color::SubstituteMissingParameters(Color& color1, Color& color2) {
  if (color1.color_space_ != color2.color_space_) {
    return false;
  }

  if (color1.param0_is_none_ && !color2.param0_is_none_) {
    color1.param0_ = color2.param0_;
    color1.param0_is_none_ = false;
  } else if (color2.param0_is_none_ && !color1.param0_is_none_) {
    color2.param0_ = color1.param0_;
    color2.param0_is_none_ = false;
  }

  if (color1.param1_is_none_ && !color2.param1_is_none_) {
    color1.param1_ = color2.param1_;
    color1.param1_is_none_ = false;
  } else if (color2.param1_is_none_ && !color1.param1_is_none_) {
    color2.param1_ = color1.param1_;
    color2.param1_is_none_ = false;
  }

  if (color1.param2_is_none_ && !color2.param2_is_none_) {
    color1.param2_ = color2.param2_;
    color1.param2_is_none_ = false;
  } else if (color2.param2_is_none_ && !color1.param2_is_none_) {
    color2.param2_ = color1.param2_;
    color2.param2_is_none_ = false;
  }

  if (color1.alpha_is_none_ && !color2.alpha_is_none_) {
    color1.alpha_ = color2.alpha_;
    color1.alpha_is_none_ = false;
  } else if (color2.alpha_is_none_ && !color1.alpha_is_none_) {
    color2.alpha_ = color1.alpha_;
    color2.alpha_is_none_ = false;
  }

  return true;
}

// static
Color Color::InterpolateColors(Color::ColorSpace interpolation_space,
                               std::optional<HueInterpolationMethod> hue_method,
                               Color color1,
                               Color color2,
                               float percentage) {
  color1.ConvertToColorSpaceForInterpolation(interpolation_space);
  color2.ConvertToColorSpaceForInterpolation(interpolation_space);

  if (!SubstituteMissingParameters(color1, color2)) {
    NOTREACHED();
  }

  float alpha1 = color1.PremultiplyColor();
  float alpha2 = color2.PremultiplyColor();

  if (!hue_method.has_value()) {
    // https://www.w3.org/TR/css-color-4/#hue-interpolation
    // Unless otherwise specified, if no specific hue interpolation algorithm
    // is selected by the host syntax, the default is shorter.
    hue_method = HueInterpolationMethod::kShorter;
  }

  std::optional<float> param0 =
      (color1.param0_is_none_ && color2.param0_is_none_)
          ? std::optional<float>(std::nullopt)
      : (interpolation_space == ColorSpace::kHSL ||
         interpolation_space == ColorSpace::kHWB)
          ? HueInterpolation(color1.param0_, color2.param0_, percentage,
                             hue_method.value())
          : blink::Blend(color1.param0_, color2.param0_, percentage);

  std::optional<float> param1 =
      (color1.param1_is_none_ && color2.param1_is_none_)
          ? std::optional<float>(std::nullopt)
          : blink::Blend(color1.param1_, color2.param1_, percentage);

  std::optional<float> param2 =
      (color1.param2_is_none_ && color2.param2_is_none_)
          ? std::optional<float>(std::nullopt)
      : (IsChromaSecondComponent(interpolation_space))
          ? HueInterpolation(color1.param2_, color2.param2_, percentage,
                             hue_method.value())
          : blink::Blend(color1.param2_, color2.param2_, percentage);

  std::optional<float> alpha = (color1.alpha_is_none_ && color2.alpha_is_none_)
                                   ? std::optional<float>(std::nullopt)
                                   : blink::Blend(alpha1, alpha2, percentage);

  Color result =
      FromColorSpace(interpolation_space, param0, param1, param2, alpha);

  result.UnpremultiplyColor();

  return result;
}
std::tuple<float, float, float> Color::ToSRGB(bool gamut_map) const {
  switch (color_space_) {
    case ColorSpace::kSRGB:
      return std::make_tuple(param0_, param1_, param2_);
    case ColorSpace::kSRGBLegacy:
      return gfx::SRGBLegacyToSRGB(param0_, param1_, param2_);
    case ColorSpace::kSRGBLinear: {
      // Several SVG rendering tests expect the inaccurate results from this
      // formulation and need to be rebaselined.
      // https://crbug.com/450045076
      skcms_TransferFunction tf_inv;
      skcms_TransferFunction_invert(&SkNamedTransferFn::kSRGB, &tf_inv);
      return std::make_tuple(skcms_TransferFunction_eval(&tf_inv, param0_),
                             skcms_TransferFunction_eval(&tf_inv, param1_),
                             skcms_TransferFunction_eval(&tf_inv, param2_));
    }
    case ColorSpace::kHSL:
      return gfx::HSLToSRGB(param0_, param1_, param2_);
    case ColorSpace::kHWB:
      return gfx::HWBToSRGB(param0_, param1_, param2_);

    case ColorSpace::kDisplayP3:
    case ColorSpace::kDisplayP3Linear:
    case ColorSpace::kA98RGB:
    case ColorSpace::kProPhotoRGB:
    case ColorSpace::kRec2020:
    case ColorSpace::kRec2100Linear:
    case ColorSpace::kXYZD50:
    case ColorSpace::kXYZD65:
    case ColorSpace::kLab:
    case ColorSpace::kOklab:
    case ColorSpace::kLch:
    case ColorSpace::kOklch: {
      // All remaining spaces go through XYZD50.
      auto [x, y, z] = ToXYZD50(gamut_map);
      return gfx::XYZD50ToSRGB(x, y, z);
    }
    case ColorSpace::kNone:
      NOTIMPLEMENTED();
      return std::make_tuple(0.f, 0.f, 0.f);
  }
}

std::tuple<float, float, float> Color::ToXYZD50(bool gamut_map) const {
  switch (color_space_) {
    case ColorSpace::kSRGBLegacy: {
      auto [r, g, b] = gfx::SRGBLegacyToSRGB(param0_, param1_, param2_);
      return gfx::SRGBToXYZD50(r, g, b);
    }
    case ColorSpace::kSRGB:
    case ColorSpace::kSRGBLinear:
    case ColorSpace::kDisplayP3:
    case ColorSpace::kDisplayP3Linear:
    case ColorSpace::kA98RGB:
    case ColorSpace::kProPhotoRGB:
    case ColorSpace::kRec2020:
    case ColorSpace::kRec2100Linear:
    case ColorSpace::kXYZD50:
    case ColorSpace::kXYZD65: {
      skcms_Matrix3x3 m;
      skcms_TransferFunction t;
      GetSkColorSpaceParams(color_space_, m, t);
      skcms::Vector3 c{{param0_, param1_, param2_}};
      c = skcms::TransferFunction_apply(t, c);
      c = skcms::Matrix3x3_apply(m, c);
      return std::make_tuple(c.vals[0], c.vals[1], c.vals[2]);
    }
    case ColorSpace::kLab:
      return gfx::LabToXYZD50(param0_, param1_, param2_);
    case ColorSpace::kOklab:
      return gfx::OklabToXYZD50(param0_, param1_, param2_, gamut_map);
    case ColorSpace::kLch: {
      auto [l, a, b] = gfx::LchToLab(param0_, param1_, param2_);
      return gfx::LabToXYZD50(l, a, b);
    }
    case ColorSpace::kOklch: {
      auto [l, a, b] = gfx::LchToLab(param0_, param1_, param2_);
      return gfx::OklabToXYZD50(l, a, b, gamut_map);
    }
    case ColorSpace::kHSL: {
      auto [r, g, b] = gfx::HSLToSRGB(param0_, param1_, param2_);
      return gfx::SRGBToXYZD50(r, g, b);
    }
    case ColorSpace::kHWB: {
      auto [r, g, b] = gfx::HWBToSRGB(param0_, param1_, param2_);
      return gfx::SRGBToXYZD50(r, g, b);
    }
    case ColorSpace::kNone:
      NOTREACHED();
  }
}

std::tuple<float, float, float> Color::ExportAsXYZD50Floats() const {
  return ToXYZD50(/*gamut_map=*/false);
}

// https://www.w3.org/TR/css-color-4/#missing:
// "[Except for interpolations] a missing component behaves as a zero value, in
// the appropriate unit for that component: 0, 0%, or 0deg. This includes
// rendering the color directly, converting it to another color space,
// performing computations on the color component values, etc."
// So we simply turn "none"s into zeros here. Note that this does not happen for
// interpolations.
void Color::ResolveMissingComponents() {
  if (param0_is_none_) {
    param0_ = 0;
    param0_is_none_ = false;
  }
  if (param1_is_none_) {
    param1_ = 0;
    param1_is_none_ = false;
  }
  if (param2_is_none_) {
    param2_ = 0;
    param2_is_none_ = false;
  }
}

void Color::ConvertToColorSpace(ColorSpace destination_color_space,
                                bool resolve_missing_components) {
  if (color_space_ == destination_color_space) {
    return;
  }

  if (resolve_missing_components) {
    ResolveMissingComponents();
  }

  switch (destination_color_space) {
    case ColorSpace::kXYZD65:
    case ColorSpace::kXYZD50:
    case ColorSpace::kSRGBLinear:
    case ColorSpace::kDisplayP3:
    case ColorSpace::kDisplayP3Linear:
    case ColorSpace::kA98RGB:
    case ColorSpace::kProPhotoRGB:
    case ColorSpace::kRec2020:
    case ColorSpace::kRec2100Linear: {
      skcms_Matrix3x3 m;
      skcms_TransferFunction t;
      GetSkColorSpaceParams(destination_color_space, m, t);
      auto [x, y, z] = ExportAsXYZD50Floats();
      skcms::Vector3 c({x, y, z});
      c = skcms::Matrix3x3_apply_inverse(m, c);
      c = skcms::TransferFunction_apply_inverse(t, c);
      param0_ = c.vals[0];
      param1_ = c.vals[1];
      param2_ = c.vals[2];
      break;
    }
    case ColorSpace::kLab:
      if (color_space_ == ColorSpace::kLch) {
        std::tie(param0_, param1_, param2_) =
            gfx::LchToLab(param0_, param1_, param2_);
      } else {
        auto [x, y, z] = ExportAsXYZD50Floats();
        std::tie(param0_, param1_, param2_) = gfx::XYZD50ToLab(x, y, z);
      }
      break;
    case ColorSpace::kOklab:
    // As per CSS Color 4 Spec, "If the host syntax does not define what color
    // space interpolation should take place in, it defaults to OKLab".
    // (https://www.w3.org/TR/css-color-4/#interpolation-space)
    case ColorSpace::kNone:
      if (color_space_ == ColorSpace::kOklch) {
        std::tie(param0_, param1_, param2_) =
            gfx::LchToLab(param0_, param1_, param2_);
      } else if (color_space_ != ColorSpace::kOklab) {
        auto [x, y, z] = ExportAsXYZD50Floats();
        std::tie(param0_, param1_, param2_) = gfx::XYZD50ToOklab(x, y, z);
      }
      break;
    case ColorSpace::kLch:
      // Conversion to lch is done through lab.
      // https://www.w3.org/TR/css-color-4/#lab-to-lch
      if (color_space_ == ColorSpace::kLab) {
        std::tie(param0_, param1_, param2_) =
            gfx::LabToLch(param0_, param1_, param2_);
      } else {
        auto [x, y, z] = ExportAsXYZD50Floats();
        auto [l, a, b] = gfx::XYZD50ToLab(x, y, z);
        std::tie(param0_, param1_, param2_) = gfx::LabToLch(l, a, b);
      }
      param2_ = AngleToUnitCircleDegrees(param2_);

      // Hue component is powerless for achromatic colors.
      if (param1_ <= kAchromaticChromaThreshold) {
        param2_is_none_ = true;
      }
      break;
    case ColorSpace::kOklch:
      if (color_space_ == ColorSpace::kOklab) {
        std::tie(param0_, param1_, param2_) =
            gfx::LabToLch(param0_, param1_, param2_);
      } else {
        auto [x, y, z] = ExportAsXYZD50Floats();
        auto [l, a, b] = gfx::XYZD50ToOklab(x, y, z);
        std::tie(param0_, param1_, param2_) = gfx::LabToLch(l, a, b);
        param2_ = AngleToUnitCircleDegrees(param2_);
      }

      // Hue component is powerless for archromatic colors.
      if (param1_ <= kAchromaticChromaThreshold) {
        param2_is_none_ = true;
      }
      break;
    case ColorSpace::kSRGB:
      std::tie(param0_, param1_, param2_) = ToSRGB();
      break;
    case ColorSpace::kSRGBLegacy:
      std::tie(param0_, param1_, param2_) = ToSRGB();
      std::tie(param0_, param1_, param2_) =
          gfx::SRGBToSRGBLegacy(param0_, param1_, param2_);
      break;
    case ColorSpace::kHSL:
      std::tie(param0_, param1_, param2_) = ToSRGB();
      std::tie(param0_, param1_, param2_) =
          gfx::SRGBToHSL(param0_, param1_, param2_);

      // Hue component is powerless for achromatic (s==0) colors.
      if (param1_ == 0) {
        param0_is_none_ = true;
      }
      break;
    case ColorSpace::kHWB:
      std::tie(param0_, param1_, param2_) = ToSRGB();
      std::tie(param0_, param1_, param2_) =
          gfx::SRGBToHWB(param0_, param1_, param2_);

      // Hue component is powerless for achromatic colors.
      if (param1_ + param2_ >= 1) {
        param0_is_none_ = true;
      }
      break;
  }

  if (destination_color_space == ColorSpace::kNone) {
    color_space_ = ColorSpace::kOklab;
  } else {
    color_space_ = destination_color_space;
  }
}

void Color::ConvertToColorSpaceForInterpolation(
    ColorSpace destination_color_space) {
  if (color_space_ == destination_color_space) {
    return;
  }

  // https://www.w3.org/TR/css-color-4/#missing:
  // When interpolating colors, missing components do not behave as zero values
  // for color space conversions.
  // https://www.w3.org/TR/css-color-4/#interpolation:
  // 1. Checking the two colors for analogous components which will be
  // carried forward.
  auto analogous = GetAnalogousMissingComponents(destination_color_space);
  // 2. Converting them to a given color space which will be referred to as
  // the interpolation color space below.
  ConvertToColorSpace(destination_color_space);
  // 3. (If required) Re-inserting carried-forward values in the converted
  // colors.
  CarryForwardAnalogousMissingComponents(analogous);
}

SkColor4f Color::toSkColor4f() const {
  auto [r, g, b] = ToSRGB(IsBakedGamutMappingEnabled());
  return SkColor4f{r, g, b, alpha_};
}

SkColor4f
Color::ToGradientStopSkColor4f(ColorSpace interpolation_space) const {
  // Do not apply gamut mapping to gradient stops. Skia will perform
  // gamut mapping on a per-pixel basis internally.
  auto [r, g, b] = ToSRGB(/*gamut_map=*/false);
  return SkColor4f{r, g, b, alpha_};
}

// static
bool Color::IsBakedGamutMappingEnabled() {
  static bool enabled =
      base::FeatureList::IsEnabled(blink::features::kBakedGamutMapping);
  return enabled;
}

float Color::PremultiplyColor() {
  // By the spec (https://www.w3.org/TR/css-color-4/#interpolation) Hue values
  // are not premultiplied, and if alpha is none, the color premultiplied value
  // is the same as unpremultiplied.
  if (alpha_is_none_)
    return alpha_;
  float alpha = alpha_;
  if (color_space_ != ColorSpace::kHSL && color_space_ != ColorSpace::kHWB)
    param0_ = param0_ * alpha_;
  param1_ = param1_ * alpha_;
  if (!IsChromaSecondComponent(color_space_)) {
    param2_ = param2_ * alpha_;
  }
  alpha_ = 1.0f;
  return alpha;
}

void Color::UnpremultiplyColor() {
  // By the spec (https://www.w3.org/TR/css-color-4/#interpolation) Hue values
  // are not premultiplied, and if alpha is none, the color premultiplied value
  // is the same as unpremultiplied.
  if (alpha_is_none_ || alpha_ == 0.0f)
    return;

  if (color_space_ != ColorSpace::kHSL && color_space_ != ColorSpace::kHWB)
    param0_ = param0_ / alpha_;
  param1_ = param1_ / alpha_;
  if (!IsChromaSecondComponent(color_space_)) {
    param2_ = param2_ / alpha_;
  }
}

unsigned Color::GetHash() const {
  unsigned result = HashInt(static_cast<uint8_t>(color_space_));
  AddFloatToHash(result, param0_);
  AddFloatToHash(result, param1_);
  AddFloatToHash(result, param2_);
  AddFloatToHash(result, alpha_);
  AddIntToHash(result, param0_is_none_);
  AddIntToHash(result, param1_is_none_);
  AddIntToHash(result, param2_is_none_);
  AddIntToHash(result, alpha_is_none_);
  return result;
}

int Color::Red() const {
  return RedChannel(Rgb());
}
int Color::Green() const {
  return GreenChannel(Rgb());
}
int Color::Blue() const {
  return BlueChannel(Rgb());
}

RGBA32 Color::Rgb() const {
  return toSkColor4f().toSkColor();
}

bool Color::ParseHexColor(base::span<const LChar> name, Color& color) {
  return ParseHexColorInternal(name, color);
}

bool Color::ParseHexColor(base::span<const UChar> name, Color& color) {
  return ParseHexColorInternal(name, color);
}

bool Color::ParseHexColor(const StringView& name, Color& color) {
  if (name.empty())
    return false;
  return VisitCharacters(name, [&color](auto chars) {
    return ParseHexColorInternal(chars, color);
  });
}

int DifferenceSquared(const Color& c1, const Color& c2) {
  int d_r = c1.Red() - c2.Red();
  int d_g = c1.Green() - c2.Green();
  int d_b = c1.Blue() - c2.Blue();
  return d_r * d_r + d_g * d_g + d_b * d_b;
}

bool Color::SetFromString(const String& name) {
  // TODO(https://crbug.com/1434423): Implement CSS Color level 4 parsing.
  if (name[0] != '#')
    return SetNamedColor(name);
  return VisitCharacters(name, [this](auto chars) {
    return ParseHexColorInternal(chars.template subspan<1>(), *this);
  });
}

// static
String Color::ColorSpaceToString(Color::ColorSpace color_space) {
  switch (color_space) {
    case Color::ColorSpace::kSRGB:
      return "srgb";
    case Color::ColorSpace::kSRGBLinear:
      return "srgb-linear";
    case Color::ColorSpace::kDisplayP3:
      return "display-p3";
    case Color::ColorSpace::kDisplayP3Linear:
      return "display-p3-linear";
    case Color::ColorSpace::kA98RGB:
      return "a98-rgb";
    case Color::ColorSpace::kProPhotoRGB:
      return "prophoto-rgb";
    case Color::ColorSpace::kRec2020:
      return "rec2020";
    case Color::ColorSpace::kRec2100Linear:
      return "rec2100-linear";
    case Color::ColorSpace::kXYZD50:
      return "xyz-d50";
    case Color::ColorSpace::kXYZD65:
      return "xyz-d65";
    case Color::ColorSpace::kLab:
      return "lab";
    case Color::ColorSpace::kOklab:
      return "oklab";
    case Color::ColorSpace::kLch:
      return "lch";
    case Color::ColorSpace::kOklch:
      return "oklch";
    case Color::ColorSpace::kSRGBLegacy:
      return "rgb";
    case Color::ColorSpace::kHSL:
      return "hsl";
    case Color::ColorSpace::kHWB:
      return "hwb";
    case ColorSpace::kNone:
      NOTREACHED();
  }
}

static String ColorParamToString(float param, int precision = 6) {
  StringBuilder result;
  if (!isfinite(param)) {
    // https://www.w3.org/TR/css-values-4/#calc-serialize
    result.Append("calc(");
    if (isinf(param)) {
      // "Infinity" gets capitalized, so we can't use AppendNumber().
      (param < 0) ? result.Append("-infinity") : result.Append("infinity");
    } else {
      result.AppendNumber(param, precision);
    }
    result.Append(")");
    return result.ToString();
  }

  result.AppendNumber(param, precision);
  return result.ToString();
}

String Color::SerializeAsCanvasColor() const {
  if (IsOpaque() && IsLegacyColorSpace(color_space_)) {
    return String::Format("#%02x%02x%02x", Red(), Green(), Blue());
  }

  return SerializeAsCSSColor();
}

String Color::SerializeLegacyColorAsCSSColor() const {
  StringBuilder result;
  if (IsOpaque() && isfinite(alpha_)) {
    result.Append("rgb(");
  } else {
    result.Append("rgba(");
  }

  constexpr float kEpsilon = 1e-07;
  auto [r, g, b] = std::make_tuple(param0_, param1_, param2_);
  if (color_space_ == Color::ColorSpace::kHWB ||
      color_space_ == Color::ColorSpace::kHSL) {
    // hsl and hwb colors need to be serialized in srgb.
    if (color_space_ == Color::ColorSpace::kHSL) {
      std::tie(r, g, b) = gfx::HSLToSRGB(param0_, param1_, param2_);
    } else if (color_space_ == Color::ColorSpace::kHWB) {
      std::tie(r, g, b) = gfx::HWBToSRGB(param0_, param1_, param2_);
    }
    // Legacy color channels get serialized with integers in the range [0,255].
    // Channels that have a value of exactly 0.5 can get incorrectly rounded
    // down to 127 when being converted to an integer. Add a small epsilon to
    // avoid this. See crbug.com/1425856.
    std::tie(r, g, b) =
        gfx::SRGBToSRGBLegacy(r + kEpsilon, g + kEpsilon, b + kEpsilon);
  }

  result.AppendNumber(round(ClampTo(r, 0.0, 255.0)));
  result.Append(", ");
  result.AppendNumber(round(ClampTo(g, 0.0, 255.0)));
  result.Append(", ");
  result.AppendNumber(round(ClampTo(b, 0.0, 255.0)));

  if (!IsOpaque()) {
    result.Append(", ");

    // See <alphavalue> section in
    // https://www.w3.org/TR/cssom/#serializing-css-values
    // First we need an 8-bit integer alpha to begin the algorithm described in
    // the link above.
    int int_alpha = ClampTo(round((alpha_ + kEpsilon) * 255.0), 0.0, 255.0);

    // If there exists a two decimal float in [0,1] that is exactly equal to the
    // integer we calculated above, used that.
    float two_decimal_rounded_alpha = round(int_alpha * 100.0 / 255.0) / 100.0;
    if (round(two_decimal_rounded_alpha * 255) == int_alpha) {
      result.Append(ColorParamToString(two_decimal_rounded_alpha, 2));
    } else {
      // Otherwise, round to 3 decimals.
      float three_decimal_rounded_alpha =
          round(int_alpha * 1000.0 / 255.0) / 1000.0;
      result.Append(ColorParamToString(three_decimal_rounded_alpha, 3));
    }
  }

  result.Append(')');
  return result.ToString();
}

String Color::SerializeInternal() const {
  StringBuilder result;
  if (IsLightnessFirstComponent(color_space_)) {
    result.Append(ColorSpaceToString(color_space_));
    result.Append("(");
  } else {
    result.Append("color(");
    result.Append(ColorSpaceToString(color_space_));
    result.Append(" ");
  }

  param0_is_none_ ? result.Append("none")
                  : result.Append(ColorParamToString(param0_));
  result.Append(" ");
  param1_is_none_ ? result.Append("none")
                  : result.Append(ColorParamToString(param1_));
  result.Append(" ");
  param2_is_none_ ? result.Append("none")
                  : result.Append(ColorParamToString(param2_));

  if (alpha_ != 1.0 || alpha_is_none_) {
    result.Append(" / ");
    alpha_is_none_ ? result.Append("none") : result.AppendNumber(alpha_);
  }
  result.Append(")");
  return result.ToString();
}

String Color::SerializeAsCSSColor() const {
  if (IsLegacyColorSpace(color_space_)) {
    return SerializeLegacyColorAsCSSColor();
  }

  return SerializeInternal();
}

String Color::NameForLayoutTreeAsText() const {
  if (!IsLegacyColorSpace(color_space_)) {
    return SerializeAsCSSColor();
  }

  if (!IsOpaque()) {
    return String::Format("#%02X%02X%02X%02X", Red(), Green(), Blue(),
                          AlphaAsInteger());
  }

  return String::Format("#%02X%02X%02X", Red(), Green(), Blue());
}

bool Color::SetNamedColor(const String& name) {
  const NamedColor* found_color = FindNamedColor(name);
  *this =
      found_color ? Color::FromRGBA32(found_color->argb_value) : kTransparent;
  return found_color;
}

Color Color::Light() const {
  static constexpr Color kLightenedBlack = Color::FromRGB(84, 84, 84);

  // Hardcode this common case for speed.
  if (*this == kBlack) {
    return kLightenedBlack;
  }

  Color srgb_color = *this;
  srgb_color.ConvertToColorSpace(ColorSpace::kSRGB);

  const float v =
      std::max({srgb_color.Param0(), srgb_color.Param1(), srgb_color.Param2()});
  if (v == 0.0f) {
    // Lightened black with alpha.
    Color lightened_black_with_alpha = kLightenedBlack;
    lightened_black_with_alpha.SetAlpha(srgb_color.Alpha());
    return lightened_black_with_alpha;
  }

  const float multiplier = std::min(1.0f, v + 0.33f) / v;
  srgb_color.param0_ = QuantizeTo8Bit(srgb_color.param0_ * multiplier);
  srgb_color.param1_ = QuantizeTo8Bit(srgb_color.param1_ * multiplier);
  srgb_color.param2_ = QuantizeTo8Bit(srgb_color.param2_ * multiplier);
  return srgb_color;
}

Color Color::Dark() const {
  // Hardcode this common case for speed.
  if (*this == kWhite) {
    static constexpr Color kDarkenedWhite = Color::FromRGB(171, 171, 171);
    return kDarkenedWhite;
  }

  Color srgb_color = *this;
  srgb_color.ConvertToColorSpace(ColorSpace::kSRGB);

  const float v =
      std::max({srgb_color.Param0(), srgb_color.Param1(), srgb_color.Param2()});
  const float multiplier = (v == 0.0f) ? 0.0f : std::max(0.0f, (v - 0.33f) / v);
  srgb_color.param0_ = QuantizeTo8Bit(srgb_color.param0_ * multiplier);
  srgb_color.param1_ = QuantizeTo8Bit(srgb_color.param1_ * multiplier);
  srgb_color.param2_ = QuantizeTo8Bit(srgb_color.param2_ * multiplier);
  return srgb_color;
}

float Color::GetLightness(ColorSpace lightness_colorspace) const {
  DCHECK(lightness_colorspace == ColorSpace::kLab ||
         lightness_colorspace == ColorSpace::kOklab ||
         lightness_colorspace == ColorSpace::kLch ||
         lightness_colorspace == ColorSpace::kOklch ||
         lightness_colorspace == ColorSpace::kHSL);
  Color color_with_l = *this;
  color_with_l.ConvertToColorSpace(lightness_colorspace);
  if (lightness_colorspace == ColorSpace::kHSL) {
    return color_with_l.Param2();
  }
  DCHECK(IsLightnessFirstComponent(lightness_colorspace));
  return color_with_l.Param0();
}

Color Color::Blend(const Color& source) const {
  // TODO(https://crbug.com/1434423): CSS Color level 4 blending is implemented.
  // Remove this function.
  if (IsFullyTransparent() || source.IsOpaque()) {
    return source;
  }

  if (source.IsFullyTransparent()) {
    return *this;
  }

  const SkRGBA4f<kPremul_SkAlphaType> pm_src = source.toSkColor4f().premul();
  auto pm_result = this->toSkColor4f().premul() * (1.0f - pm_src.fA);
  pm_result.fA += pm_src.fA;
  pm_result.fR += pm_src.fR;
  pm_result.fG += pm_src.fG;
  pm_result.fB += pm_src.fB;
  return Color(pm_result.unpremul());
}

Color Color::BlendWithWhite() const {
  // If the color contains alpha already, we leave it alone.
  if (!IsOpaque()) {
    return *this;
  }

  Color new_color;
  for (int alpha = kCStartAlpha; alpha <= kCEndAlpha;
       alpha += kCAlphaIncrement) {
    // We have a solid color.  Convert to an equivalent color that looks the
    // same when blended with white at the current alpha.  Try using less
    // transparency if the numbers end up being negative.
    int r = BlendComponent(Red(), alpha);
    int g = BlendComponent(Green(), alpha);
    int b = BlendComponent(Blue(), alpha);

    new_color = Color(r, g, b, alpha);

    if (r >= 0 && g >= 0 && b >= 0)
      break;
  }
  return new_color;
}

Color Color::InvertSRGB() const {
  Color inv_color = *this;
  inv_color.ConvertToColorSpace(ColorSpace::kSRGB);
  inv_color.param0_ = 1.0f - inv_color.param0_;
  inv_color.param1_ = 1.0f - inv_color.param1_;
  inv_color.param2_ = 1.0f - inv_color.param2_;
  return inv_color;
}

// From https://www.w3.org/TR/css-color-4/#interpolation
// If the host syntax does not define what color space interpolation should
// take place in, it defaults to Oklab.
// However, user agents may handle interpolation between legacy sRGB color
// formats (hex colors, named colors, rgb(), hsl() or hwb() and the equivalent
// alpha-including forms) in gamma-encoded sRGB space.
Color::ColorSpace Color::GetColorInterpolationSpace() const {
  // If the color space is legacy and does not contain none, it should be
  // interpolated in srgb-legacy.
  if (IsLegacyColorSpace(color_space_) && !param0_is_none_ &&
      !param1_is_none_ && !param2_is_none_ && !alpha_is_none_) {
    return ColorSpace::kSRGBLegacy;
  }

  return ColorSpace::kOklab;
}

// static
String Color::SerializeInterpolationSpace(
    Color::ColorSpace color_space,
    Color::HueInterpolationMethod hue_interpolation_method) {
  StringBuilder result;
  switch (color_space) {
    case Color::ColorSpace::kLab:
      result.Append("lab");
      break;
    case Color::ColorSpace::kOklab:
      result.Append("oklab");
      break;
    case Color::ColorSpace::kLch:
      result.Append("lch");
      break;
    case Color::ColorSpace::kOklch:
      result.Append("oklch");
      break;
    case Color::ColorSpace::kSRGBLinear:
      result.Append("srgb-linear");
      break;
    case Color::ColorSpace::kSRGB:
    case Color::ColorSpace::kSRGBLegacy:
      result.Append("srgb");
      break;
    case Color::ColorSpace::kXYZD65:
      result.Append("xyz-d65");
      break;
    case Color::ColorSpace::kXYZD50:
      result.Append("xyz-d50");
      break;
    case Color::ColorSpace::kHSL:
      result.Append("hsl");
      break;
    case Color::ColorSpace::kHWB:
      result.Append("hwb");
      break;
    case Color::ColorSpace::kNone:
      result.Append("none");
      break;
    case ColorSpace::kDisplayP3:
      result.Append("display-p3");
      break;
    case ColorSpace::kDisplayP3Linear:
      result.Append("display-p3-linear");
      break;
    case ColorSpace::kA98RGB:
      result.Append("a98-rgb");
      break;
    case ColorSpace::kProPhotoRGB:
      result.Append("prophoto-rgb");
      break;
    case ColorSpace::kRec2020:
      result.Append("rec2020");
      break;
    case ColorSpace::kRec2100Linear:
      result.Append("rec2100-linear");
      break;
  }

  if (ColorSpaceHasHue(color_space)) {
    switch (hue_interpolation_method) {
      case Color::HueInterpolationMethod::kDecreasing:
        result.Append(" decreasing hue");
        break;
      case Color::HueInterpolationMethod::kIncreasing:
        result.Append(" increasing hue");
        break;
      case Color::HueInterpolationMethod::kLonger:
        result.Append(" longer hue");
        break;
      // Shorter is the default value and does not get serialized
      case Color::HueInterpolationMethod::kShorter:
        break;
    }
  }

  return result.ReleaseString();
}

static float ResolveNonFiniteChannel(float value,
                                     float negative_infinity_substitution,
                                     float positive_infinity_substitution) {
  // Finite values should be unchanged, even if they are out-of-gamut.
  if (isfinite(value)) {
    return value;
  } else {
    if (isnan(value)) {
      return 0.0f;
    } else {
      if (value < 0) {
        return negative_infinity_substitution;
      }
      return positive_infinity_substitution;
    }
  }
}

void Color::ResolveNonFiniteValues() {
  // calc(NaN) and calc(Infinity) need to be serialized for colors at parse
  // time, but eventually their true values need to be computed. calc(NaN) will
  // always become zero and +/-infinity become the upper/lower bound of the
  // channel, respectively, if it exists.
  // Crucially, this function does not clamp channels that are finite, this is
  // to allow for things like blending out-of-gamut colors.
  // See: https://github.com/w3c/csswg-drafts/issues/8629

  // Lightness is clamped to [0, 100].
  if (IsLightnessFirstComponent(color_space_)) {
    param0_ = ResolveNonFiniteChannel(param0_, 0.0f, 100.0f);
  }

  // Chroma cannot be negative.
  if (IsChromaSecondComponent(color_space_) && isinf(param1_) &&
      param1_ < 0.0f) {
    param1_ = 0.0f;
  }

  // Legacy sRGB does not respresent out-of-gamut colors.
  if (color_space_ == Color::ColorSpace::kSRGBLegacy) {
    param0_ = ResolveNonFiniteChannel(param0_, 0.0f, 1.0f);
    param1_ = ResolveNonFiniteChannel(param1_, 0.0f, 1.0f);
    param2_ = ResolveNonFiniteChannel(param2_, 0.0f, 1.0f);
  }

  // Parsed values are `calc(NaN)` but computed values are 0 for NaN.
  param0_ = isnan(param0_) ? 0.0f : param0_;
  param1_ = isnan(param1_) ? 0.0f : param1_;
  param2_ = isnan(param2_) ? 0.0f : param2_;
  alpha_ = ResolveNonFiniteChannel(alpha_, 0.0f, 1.0f);
}

std::ostream& operator<<(std::ostream& os, const Color& color) {
  return os << color.SerializeAsCSSColor();
}

}  // namespace blink
