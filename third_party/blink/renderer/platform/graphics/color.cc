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

#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_conversions.h"

namespace blink {

const Color Color::kBlack = Color(0xFF000000);
const Color Color::kWhite = Color(0xFFFFFFFF);
const Color Color::kDarkGray = Color(0xFF808080);
const Color Color::kGray = Color(0xFFA0A0A0);
const Color Color::kLightGray = Color(0xFFC0C0C0);
const Color Color::kTransparent = Color(0x00000000);

namespace {

const RGBA32 kDarkenedWhite = 0xFFABABAB;

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

int ColorFloatToRGBAByte(float f) {
  return ClampTo(static_cast<int>(lroundf(255.0f * f)), 0, 255);
}

// originally moved here from the CSS parser
template <typename CharacterType>
inline bool ParseHexColorInternal(const CharacterType* name,
                                  unsigned length,
                                  Color& color) {
  if (length != 3 && length != 4 && length != 6 && length != 8)
    return false;
  if ((length == 8 || length == 4) &&
      !RuntimeEnabledFeatures::CSSHexAlphaColorEnabled())
    return false;
  unsigned value = 0;
  for (unsigned i = 0; i < length; ++i) {
    if (!IsASCIIHexDigit(name[i]))
      return false;
    value <<= 4;
    value |= ToASCIIHexValue(name[i]);
  }
  if (length == 6) {
    color = Color::FromRGBA32(0xFF000000 | value);
    return true;
  }
  if (length == 8) {
    // We parsed the values into RGBA order, but the RGBA32 type
    // expects them to be in ARGB order, so we right rotate eight bits.
    color = Color::FromRGBA32(value << 24 | value >> 8);
    return true;
  }
  if (length == 4) {
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
  char buffer[64];  // easily big enough for the longest color name
  unsigned length = name.length();
  if (length > sizeof(buffer) - 1)
    return nullptr;
  for (unsigned i = 0; i < length; ++i) {
    UChar c = name[i];
    if (!c || c > 0x7F)
      return nullptr;
    buffer[i] = ToASCIILower(static_cast<char>(c));
  }
  buffer[length] = '\0';
  return FindColor(buffer, length);
}

bool ValidColorSpaceForFromColorFunction(Color::ColorSpace color_space) {
  switch (color_space) {
    case Color::ColorSpace::kSRGB:
    case Color::ColorSpace::kSRGBLinear:
    case Color::ColorSpace::kDisplayP3:
    case Color::ColorSpace::kA98RGB:
    case Color::ColorSpace::kProPhotoRGB:
    case Color::ColorSpace::kRec2020:
    case Color::ColorSpace::kXYZD50:
    case Color::ColorSpace::kXYZD65:
      return true;
    default:
      return false;
  }
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

constexpr int AlphaChannel(RGBA32 color) {
  return (color >> 24) & 0xFF;
}

}  // namespace

// The color parameters will use 16 bytes (for 4 floats). Ensure that the
// remaining parameters fit into another 4 bytes (or 8 bytes, on Windows)
#if BUILDFLAG(IS_WIN)
static_assert(sizeof(Color) <= 24, "blink::Color should be <= 24 bytes.");
#else
static_assert(sizeof(Color) <= 20, "blink::Color should be <= 20 bytes.");
#endif

Color::Color(int r, int g, int b) {
  *this = FromRGB(r, g, b);
}

Color::Color(int r, int g, int b, int a) {
  *this = FromRGBA(r, g, b, a);
}

// static
Color Color::FromHSLA(double h, double s, double l, double a) {
  Color result = Color(gfx::HSLToSkColor4f(h, s, l, a));
  result.color_space_ = ColorSpace::kHSL;
  return result;
}

// static
Color Color::FromHWBA(double h, double w, double b, double a) {
  Color result = Color(gfx::HWBToSkColor4f(h, w, b, a));
  result.color_space_ = ColorSpace::kHWB;
  return result;
}

// static
Color Color::FromColorFunction(ColorSpace space,
                               absl::optional<float> red_or_x,
                               absl::optional<float> green_or_y,
                               absl::optional<float> blue_or_z,
                               absl::optional<float> alpha) {
  if (!ValidColorSpaceForFromColorFunction(space)) {
    NOTREACHED();
    return Color();
  }

  Color result;
  result.color_space_ = space;
  result.param0_is_none_ = !red_or_x;
  result.param1_is_none_ = !green_or_y;
  result.param2_is_none_ = !blue_or_z;
  result.alpha_is_none_ = !alpha;
  result.param0_ = red_or_x.value_or(0.f);
  result.param1_ = green_or_y.value_or(0.f);
  result.param2_ = blue_or_z.value_or(0.f);
  result.alpha_ = ClampTo(alpha.value_or(1.f), 0.f, 1.f);
  return result;
}

// static
Color Color::FromLab(absl::optional<float> L,
                     absl::optional<float> a,
                     absl::optional<float> b,
                     absl::optional<float> alpha) {
  Color result;
  result.color_space_ = ColorSpace::kLab;
  result.param0_is_none_ = !L;
  result.param1_is_none_ = !a;
  result.param2_is_none_ = !b;
  result.alpha_is_none_ = !alpha;
  result.param0_ = std::max(L.value_or(0.f), 0.f);
  result.param1_ = a.value_or(0.f);
  result.param2_ = b.value_or(0.f);
  result.alpha_ = ClampTo(alpha.value_or(1.f), 0.f, 1.f);
  return result;
}

// static
Color Color::FromOKLab(absl::optional<float> L,
                       absl::optional<float> a,
                       absl::optional<float> b,
                       absl::optional<float> alpha) {
  Color result;
  result.color_space_ = ColorSpace::kOklab;
  result.param0_is_none_ = !L;
  result.param1_is_none_ = !a;
  result.param2_is_none_ = !b;
  result.alpha_is_none_ = !alpha;
  result.param0_ = std::max(L.value_or(0.f), 0.f);
  result.param1_ = a.value_or(0.f);
  result.param2_ = b.value_or(0.f);
  result.alpha_ = ClampTo(alpha.value_or(1.f), 0.f, 1.f);
  return result;
}

// static
Color Color::FromLCH(absl::optional<float> L,
                     absl::optional<float> chroma,
                     absl::optional<float> hue,
                     absl::optional<float> alpha) {
  Color result;
  result.color_space_ = ColorSpace::kLch;
  result.param0_is_none_ = !L;
  result.param1_is_none_ = !chroma;
  result.param2_is_none_ = !hue;
  result.alpha_is_none_ = !alpha;
  result.param0_ = std::max(L.value_or(0.f), 0.f);
  result.param1_ = std::max(chroma.value_or(0.f), 0.f);
  result.param2_ = hue.value_or(0.f);
  result.alpha_ = ClampTo(alpha.value_or(1.f), 0.f, 1.f);
  return result;
}

// static
Color Color::FromOKLCH(absl::optional<float> L,
                       absl::optional<float> chroma,
                       absl::optional<float> hue,
                       absl::optional<float> alpha) {
  Color result;
  result.color_space_ = ColorSpace::kOklch;
  result.param0_is_none_ = !L;
  result.param1_is_none_ = !chroma;
  result.param2_is_none_ = !hue;
  result.alpha_is_none_ = !alpha;
  result.param0_ = std::max(L.value_or(0.f), 0.f);
  result.param1_ = std::max(chroma.value_or(0.f), 0.f);
  result.param2_ = hue.value_or(0.f);
  result.alpha_ = ClampTo(alpha.value_or(1.f), 0.f, 1.f);
  return result;
}

// static
Color Color::FromColorMix(Color::ColorInterpolationSpace interpolation_space,
                          absl::optional<HueInterpolationMethod> hue_method,
                          Color color1,
                          Color color2,
                          float percentage,
                          float alpha_multiplier) {
  // todo(1092638) : Support other color spaces, and conversions to the given
  // color space.
  if (interpolation_space != ColorInterpolationSpace::kSRGB) {
    NOTIMPLEMENTED();
    return Color();
  }

  color1.ConvertToColorInterpolationSpace(interpolation_space);
  color2.ConvertToColorInterpolationSpace(interpolation_space);

  float alpha1 = color1.PremultiplyColor();
  float alpha2 = color2.PremultiplyColor();

  Color result = FromColorFunction(
      ColorSpace::kSRGB,
      blink::Blend(color2.param0_, color1.param0_, percentage),
      blink::Blend(color2.param1_, color1.param1_, percentage),
      blink::Blend(color2.param2_, color1.param2_, percentage),
      blink::Blend(alpha2, alpha1, percentage));

  result.UnpremultiplyColor();

  result.alpha_ *= alpha_multiplier;

  return result;
}

std::tuple<float, float, float> Color::ExportAsXYZD50Floats() const {
  switch (color_space_) {
    case ColorSpace::kRGBLegacy:
    case ColorSpace::kHSL:
    case ColorSpace::kHWB:
    case ColorSpace::kSRGB:
      return gfx::SRGBToXYZD50(param0_, param1_, param2_);
    case ColorSpace::kSRGBLinear:
      return gfx::SRGBLinearToXYZD50(param0_, param1_, param2_);
    case ColorSpace::kDisplayP3:
      return gfx::DisplayP3ToXYZD50(param0_, param1_, param2_);
    case ColorSpace::kA98RGB:
      return gfx::AdobeRGBToXYZD50(param0_, param1_, param2_);
    case ColorSpace::kProPhotoRGB:
      return gfx::ProPhotoToXYZD50(param0_, param1_, param2_);
    case ColorSpace::kRec2020:
      return gfx::Rec2020ToXYZD50(param0_, param1_, param2_);
    case ColorSpace::kXYZD50:
      return {param0_, param1_, param2_};
    case ColorSpace::kXYZD65:
      return gfx::XYZD65ToD50(param0_, param1_, param2_);
    case ColorSpace::kLab:
      return gfx::LabToXYZD50(param0_, param1_, param2_);
    case ColorSpace::kOklab: {
      auto [x, y, z] = gfx::OklabToXYZD65(param0_, param1_, param2_);
      return gfx::XYZD65ToD50(x, y, z);
    }
    case ColorSpace::kLch: {
      auto [l, a, b] = gfx::LchToLab(param0_, param1_, param2_);
      return gfx::LabToXYZD50(l, a, b);
    }
    case ColorSpace::kOklch: {
      auto [l, a, b] = gfx::LchToLab(param0_, param1_, param2_);
      auto [x, y, z] = gfx::OklabToXYZD65(l, a, b);
      return gfx::XYZD65ToD50(x, y, z);
    }
    default:
      NOTIMPLEMENTED();
      return {0.f, 0.f, 0.f};
  }
}

void Color::ConvertToColorInterpolationSpace(
    Color::ColorInterpolationSpace interpolation_space) {
  switch (interpolation_space) {
    case ColorInterpolationSpace::kXYZD65: {
      if (color_space_ == ColorSpace::kXYZD65)
        return;
      if (color_space_ == ColorSpace::kOklab) {
        std::tie(param0_, param1_, param2_) =
            gfx::OklabToXYZD65(param0_, param1_, param2_);
      } else {
        auto [x, y, z] = ExportAsXYZD50Floats();
        std::tie(param0_, param1_, param2_) = gfx::XYZD50ToD65(x, y, z);
      }
      color_space_ = ColorSpace::kXYZD65;
      return;
    }
    case ColorInterpolationSpace::kXYZD50: {
      if (color_space_ == ColorSpace::kXYZD50)
        return;
      std::tie(param0_, param1_, param2_) = ExportAsXYZD50Floats();
      color_space_ = ColorSpace::kXYZD50;
      return;
    }
    case ColorInterpolationSpace::kSRGBLinear: {
      if (color_space_ == ColorSpace::kSRGBLinear)
        return;
      auto [x, y, z] = ExportAsXYZD50Floats();
      std::tie(param0_, param1_, param2_) = gfx::XYZD50TosRGBLinear(x, y, z);
      color_space_ = ColorSpace::kSRGBLinear;
      return;
    }
    case ColorInterpolationSpace::kLab: {
      if (color_space_ == ColorSpace::kLab) {
        return;
      }
      if (color_space_ == ColorSpace::kLch) {
        std::tie(param0_, param1_, param2_) =
            gfx::LchToLab(param0_, param1_, param2_);
      } else {
        auto [x, y, z] = ExportAsXYZD50Floats();
        std::tie(param0_, param1_, param2_) = gfx::XYZD50ToLab(x, y, z);
      }
      color_space_ = ColorSpace::kLab;
      return;
    }
    case ColorInterpolationSpace::kOKLab:
    // As per CSS Color 4 Spec, "If the host syntax does not define what color
    // space interpolation should take place in, it defaults to OKLab".
    // (https://www.w3.org/TR/css-color-4/#interpolation-space)
    case ColorInterpolationSpace::kNone: {
      if (color_space_ == ColorSpace::kOklab) {
        return;
      }
      if (color_space_ == ColorSpace::kOklch) {
        std::tie(param0_, param1_, param2_) =
            gfx::LchToLab(param0_, param1_, param2_);
        color_space_ = ColorSpace::kOklab;
        return;
      }
      // Conversion to Oklab is done through XYZD65.
      auto [xd65, yd65, zd65] = [&]() {
        if (color_space_ == ColorSpace::kXYZD65) {
          return std::make_tuple(param0_, param1_, param2_);
        } else {
          auto [xd50, yd50, zd50] = ExportAsXYZD50Floats();
          return gfx::XYZD50ToD65(xd50, yd50, zd50);
        }
      }();

      std::tie(param0_, param1_, param2_) =
          gfx::XYZD65ToOklab(xd65, yd65, zd65);
      color_space_ = ColorSpace::kOklab;
      return;
    }
    case ColorInterpolationSpace::kLCH: {
      if (color_space_ == ColorSpace::kLch) {
        return;
      }
      // Conversion to lch is done through lab.
      auto [l, a, b] = [&]() {
        if (color_space_ == ColorSpace::kLab) {
          return std::make_tuple(param0_, param1_, param2_);
        } else {
          auto [xd50, yd50, zd50] = ExportAsXYZD50Floats();
          return gfx::XYZD50ToLab(xd50, yd50, zd50);
        }
      }();

      std::tie(param0_, param1_, param2_) = gfx::LabToLch(l, a, b);
      color_space_ = ColorSpace::kLch;
      return;
    }
    case ColorInterpolationSpace::kOKLCH: {
      if (color_space_ == ColorSpace::kOklch) {
        return;
      }
      if (color_space_ == ColorSpace::kOklab) {
        std::tie(param0_, param1_, param2_) =
            gfx::LabToLch(param0_, param1_, param2_);
        color_space_ = ColorSpace::kOklch;
        return;
      }

      // Conversion to Oklch is done through XYZD65.
      auto [xd65, yd65, zd65] = [&]() {
        if (color_space_ == ColorSpace::kXYZD65) {
          return std::make_tuple(param0_, param1_, param2_);
        } else {
          auto [xd50, yd50, zd50] = ExportAsXYZD50Floats();
          return gfx::XYZD50ToD65(xd50, yd50, zd50);
        }
      }();

      auto [l, a, b] = gfx::XYZD65ToOklab(xd65, yd65, zd65);
      std::tie(param0_, param1_, param2_) = gfx::LabToLch(l, a, b);
      color_space_ = ColorSpace::kOklch;
      return;
    }
    case ColorInterpolationSpace::kHSL:
    case ColorInterpolationSpace::kHWB:
    case ColorInterpolationSpace::kSRGB: {
      if (color_space_ != ColorSpace::kHSL &&
          color_space_ != ColorSpace::kHWB &&
          color_space_ != ColorSpace::kSRGB) {
        SkColor4f sRGB_color = toSkColor4f();
        param0_ = sRGB_color.fR;
        param1_ = sRGB_color.fG;
        param2_ = sRGB_color.fB;
      }
      if (interpolation_space == ColorInterpolationSpace::kHSL)
        color_space_ = ColorSpace::kHSL;
      else if (interpolation_space == ColorInterpolationSpace::kHWB)
        color_space_ = ColorSpace::kHWB;
      else  //(interpolation_space == ColorInterpolationSpace::kSRGB)
        color_space_ = ColorSpace::kSRGB;
      return;
    }
  }
}

SkColor4f Color::toSkColor4f() const {
  switch (color_space_) {
    case ColorSpace::kSRGB:
      return SkColor4f{param0_, param1_, param2_, alpha_};
    case ColorSpace::kSRGBLinear:
      return gfx::SRGBLinearToSkColor4f(param0_, param1_, param2_, alpha_);
    case ColorSpace::kDisplayP3:
      return gfx::DisplayP3ToSkColor4f(param0_, param1_, param2_, alpha_);
    case ColorSpace::kA98RGB:
      return gfx::AdobeRGBToSkColor4f(param0_, param1_, param2_, alpha_);
    case ColorSpace::kProPhotoRGB:
      return gfx::ProPhotoToSkColor4f(param0_, param1_, param2_, alpha_);
    case ColorSpace::kRec2020:
      return gfx::Rec2020ToSkColor4f(param0_, param1_, param2_, alpha_);
    case ColorSpace::kXYZD50:
      return gfx::XYZD50ToSkColor4f(param0_, param1_, param2_, alpha_);
    case ColorSpace::kXYZD65:
      return gfx::XYZD65ToSkColor4f(param0_, param1_, param2_, alpha_);
    case ColorSpace::kLab:
      return gfx::LabToSkColor4f(param0_, param1_, param2_, alpha_);
    case ColorSpace::kOklab:
      return gfx::OklabToSkColor4f(param0_, param1_, param2_, alpha_);
    case ColorSpace::kLch:
      return gfx::LchToSkColor4f(
          param0_, param1_,
          param2_is_none_ ? absl::nullopt : absl::optional<float>(param2_),
          alpha_);
    case ColorSpace::kOklch:
      return gfx::OklchToSkColor4f(
          param0_, param1_,
          param2_is_none_ ? absl::nullopt : absl::optional<float>(param2_),
          alpha_);
    case ColorSpace::kRGBLegacy:
    case ColorSpace::kHSL:
    case ColorSpace::kHWB:
      return SkColor4f{param0_, param1_, param2_, alpha_};
    default:
      NOTIMPLEMENTED();
      return SkColor4f{0.f, 0.f, 0.f, 0.f};
  }
}

float Color::PremultiplyColor() {
  float alpha = alpha_;
  param0_ = param0_ * alpha_;
  param1_ = param1_ * alpha_;
  param2_ = param2_ * alpha_;
  alpha_ = 1.0f;
  return alpha;
}

void Color::UnpremultiplyColor() {
  if (alpha_ == 0.0f)
    return;

  param0_ = param0_ / alpha_;
  param1_ = param1_ / alpha_;
  param2_ = param2_ / alpha_;
}

// static
Color Color::FromRGBAFloat(float r, float g, float b, float a) {
  return Color(SkColor4f{r, g, b, a});
}

// static
Color Color::FromSkColor4f(SkColor4f fc) {
  return Color(fc);
}

// This converts -0.0 to 0.0, so that they have the same hash value. This
// ensures that equal FontDescription have the same hash value.
float NormalizeSign(float number) {
  if (UNLIKELY(number == 0.0))
    return 0.0;
  return number;
}

unsigned Color::GetHash() const {
  unsigned result = WTF::HashInt(static_cast<uint8_t>(color_space_));
  WTF::AddFloatToHash(result, NormalizeSign(param0_));
  WTF::AddFloatToHash(result, NormalizeSign(param1_));
  WTF::AddFloatToHash(result, NormalizeSign(param2_));
  WTF::AddFloatToHash(result, NormalizeSign(alpha_));
  WTF::AddIntToHash(result, param0_is_none_);
  WTF::AddIntToHash(result, param1_is_none_);
  WTF::AddIntToHash(result, param2_is_none_);
  WTF::AddIntToHash(result, alpha_is_none_);
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

bool Color::ParseHexColor(const LChar* name, unsigned length, Color& color) {
  return ParseHexColorInternal(name, length, color);
}

bool Color::ParseHexColor(const UChar* name, unsigned length, Color& color) {
  return ParseHexColorInternal(name, length, color);
}

bool Color::ParseHexColor(const StringView& name, Color& color) {
  if (name.empty())
    return false;
  if (name.Is8Bit())
    return ParseHexColor(name.Characters8(), name.length(), color);
  return ParseHexColor(name.Characters16(), name.length(), color);
}

int DifferenceSquared(const Color& c1, const Color& c2) {
  int d_r = c1.Red() - c2.Red();
  int d_g = c1.Green() - c2.Green();
  int d_b = c1.Blue() - c2.Blue();
  return d_r * d_r + d_g * d_g + d_b * d_b;
}

bool Color::SetFromString(const String& name) {
  // TODO(https://crbug.com/1333988): Implement CSS Color level 4 parsing.
  if (name[0] != '#')
    return SetNamedColor(name);
  if (name.Is8Bit())
    return ParseHexColor(name.Characters8() + 1, name.length() - 1, *this);
  return ParseHexColor(name.Characters16() + 1, name.length() - 1, *this);
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
    case Color::ColorSpace::kA98RGB:
      return "a98-rgb";
    case Color::ColorSpace::kProPhotoRGB:
      return "prophoto-rgb";
    case Color::ColorSpace::kRec2020:
      return "rec2020";
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
      return "oklab";
    case Color::ColorSpace::kRGBLegacy:
      return "RGB Legacy";
    case Color::ColorSpace::kHSL:
      return "HSL";
    case Color::ColorSpace::kHWB:
      return "HWB";
  }
}

String Color::SerializeAsCanvasColor() const {
  if ((color_space_ == ColorSpace::kRGBLegacy ||
       color_space_ == ColorSpace::kHSL || color_space_ == ColorSpace::kHWB) &&
      !HasAlpha())
    return String::Format("#%02x%02x%02x", Red(), Green(), Blue());

  return SerializeAsCSSColor();
}

String Color::SerializeAsCSSColor() const {
  StringBuilder result;
  result.ReserveCapacity(28);

  switch (color_space_) {
    case ColorSpace::kRGBLegacy:
    case ColorSpace::kHSL:
    case ColorSpace::kHWB:
      if (HasAlpha())
        result.Append("rgba(");
      else
        result.Append("rgb(");

      result.AppendNumber(Red());
      result.Append(", ");
      result.AppendNumber(Green());
      result.Append(", ");
      result.AppendNumber(Blue());

      if (HasAlpha()) {
        result.Append(", ");
        // See <alphavalue> section in
        // https://drafts.csswg.org/cssom/#serializing-css-values
        float rounded = round(Alpha() * 100 / 255.0f) / 100;
        if (round(rounded * 255) == Alpha()) {
          result.AppendNumber(rounded, 2);
        } else {
          rounded = round(Alpha() * 1000 / 255.0f) / 1000;
          result.AppendNumber(rounded, 3);
        }
      }

      result.Append(')');
      return result.ToString();

    case ColorSpace::kLab:
    case ColorSpace::kOklab:
    case ColorSpace::kLch:
    case ColorSpace::kOklch:
      if (color_space_ == ColorSpace::kLab)
        result.Append("lab(");
      if (color_space_ == ColorSpace::kOklab)
        result.Append("oklab(");
      if (color_space_ == ColorSpace::kLch)
        result.Append("lch(");
      if (color_space_ == ColorSpace::kOklch)
        result.Append("oklch(");

      if (param0_is_none_) {
        result.Append("none ");
      } else {
        result.AppendNumber(param0_);
        result.Append(" ");
      }

      if (param1_is_none_)
        result.Append("none");
      else
        result.AppendNumber(param1_);
      result.Append(" ");

      if (param2_is_none_)
        result.Append("none");
      else
        result.AppendNumber(param2_);

      if (alpha_ != 1.0 || alpha_is_none_) {
        result.Append(" / ");
        if (alpha_is_none_)
          result.Append("none");
        else
          result.AppendNumber(alpha_);
      }
      result.Append(")");
      return result.ToString();

    case ColorSpace::kSRGB:
    case ColorSpace::kSRGBLinear:
    case ColorSpace::kDisplayP3:
    case ColorSpace::kA98RGB:
    case ColorSpace::kProPhotoRGB:
    case ColorSpace::kRec2020:
    case ColorSpace::kXYZD50:
    case ColorSpace::kXYZD65:
      result.Append("color(");
      result.Append(ColorSpaceToString(color_space_));

      result.Append(" ");
      if (param0_is_none_)
        result.Append("none");
      else
        result.AppendNumber(param0_);

      result.Append(" ");
      if (param1_is_none_)
        result.Append("none");
      else
        result.AppendNumber(param1_);

      result.Append(" ");
      if (param2_is_none_)
        result.Append("none");
      else
        result.AppendNumber(param2_);

      if (alpha_ != 1.0 || alpha_is_none_) {
        result.Append(" / ");
        if (alpha_is_none_)
          result.Append("none");
        else
          result.AppendNumber(alpha_);
      }
      result.Append(")");
      return result.ToString();

    default:
      NOTIMPLEMENTED();
      return "rgb(0, 0, 0)";
  }
}

String Color::NameForLayoutTreeAsText() const {
  if (color_space_ != ColorSpace::kRGBLegacy &&
      color_space_ != ColorSpace::kHSL && color_space_ != ColorSpace::kHWB) {
    // TODO(https://crbug.com/1333988): Determine if CSS Color Level 4 colors
    // should use this representation here.
    return SerializeAsCSSColor();
  }
  if (Alpha() < 0xFF)
    return String::Format("#%02X%02X%02X%02X", Red(), Green(), Blue(), Alpha());
  return String::Format("#%02X%02X%02X", Red(), Green(), Blue());
}

bool Color::SetNamedColor(const String& name) {
  const NamedColor* found_color = FindNamedColor(name);
  *this =
      found_color ? Color::FromRGBA32(found_color->argb_value) : kTransparent;
  return found_color;
}

SkColor Color::ToSkColorDeprecated() const {
  return SkColorSetARGB(Alpha(), Red(), Green(), Blue());
}

Color Color::Dark() const {
  // Hardcode this common case for speed.
  if (*this == kWhite)
    return Color(kDarkenedWhite);

  const float scale_factor = nextafterf(256.0f, 0.0f);

  float r, g, b, a;
  GetRGBA(r, g, b, a);

  float v = std::max(r, std::max(g, b));
  float multiplier = (v == 0.0f) ? 0.0f : std::max(0.0f, (v - 0.33f) / v);

  return Color(static_cast<int>(multiplier * r * scale_factor),
               static_cast<int>(multiplier * g * scale_factor),
               static_cast<int>(multiplier * b * scale_factor), Alpha());
}

Color Color::CombineWithAlpha(float other_alpha) const {
  RGBA32 rgb_only = Rgb() & 0x00FFFFFF;
  float override_alpha = (Alpha() / 255.f) * other_alpha;
  return Color(rgb_only | ColorFloatToRGBAByte(override_alpha) << 24);
}

Color Color::Blend(const Color& source) const {
  // TODO(https://crbug.com/1333988): Implement CSS Color level 4 blending.
  if (!Alpha() || !source.HasAlpha())
    return source;

  if (!source.Alpha())
    return *this;

  int d = 255 * (Alpha() + source.Alpha()) - Alpha() * source.Alpha();
  int a = d / 255;
  int r = (Red() * Alpha() * (255 - source.Alpha()) +
           255 * source.Alpha() * source.Red()) /
          d;
  int g = (Green() * Alpha() * (255 - source.Alpha()) +
           255 * source.Alpha() * source.Green()) /
          d;
  int b = (Blue() * Alpha() * (255 - source.Alpha()) +
           255 * source.Alpha() * source.Blue()) /
          d;
  return Color(r, g, b, a);
}

Color Color::BlendWithWhite() const {
  // If the color contains alpha already, we leave it alone.
  if (HasAlpha())
    return *this;

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

void Color::GetRGBA(float& r, float& g, float& b, float& a) const {
  r = Red() / 255.0f;
  g = Green() / 255.0f;
  b = Blue() / 255.0f;
  a = Alpha() / 255.0f;
}

void Color::GetRGBA(double& r, double& g, double& b, double& a) const {
  r = Red() / 255.0;
  g = Green() / 255.0;
  b = Blue() / 255.0;
  a = Alpha() / 255.0;
}

// Hue, max and min are returned in range of 0.0 to 1.0.
void Color::GetHueMaxMin(double& hue, double& max, double& min) const {
  // This is a helper function to calculate intermediate quantities needed
  // for conversion to HSL or HWB formats. The algorithm contained below
  // is a copy of http://en.wikipedia.org/wiki/HSL_color_space.
  double r = static_cast<double>(Red()) / 255.0;
  double g = static_cast<double>(Green()) / 255.0;
  double b = static_cast<double>(Blue()) / 255.0;
  max = std::max(std::max(r, g), b);
  min = std::min(std::min(r, g), b);

  if (max == min)
    hue = 0.0;
  else if (max == r)
    hue = (60.0 * ((g - b) / (max - min))) + 360.0;
  else if (max == g)
    hue = (60.0 * ((b - r) / (max - min))) + 120.0;
  else
    hue = (60.0 * ((r - g) / (max - min))) + 240.0;

  // Adjust for rounding errors and scale to interval 0.0 to 1.0.
  if (hue >= 360.0)
    hue -= 360.0;
  hue /= 360.0;
}

// Hue, saturation and lightness are returned in range of 0.0 to 1.0.
void Color::GetHSL(double& hue, double& saturation, double& lightness) const {
  double max, min;
  GetHueMaxMin(hue, max, min);

  lightness = 0.5 * (max + min);
  if (max == min)
    saturation = 0.0;
  else if (lightness <= 0.5)
    saturation = ((max - min) / (max + min));
  else
    saturation = ((max - min) / (2.0 - (max + min)));
}

// Output parameters hue, white and black are in the range 0.0 to 1.0.
void Color::GetHWB(double& hue, double& white, double& black) const {
  // https://drafts.csswg.org/css-color-4/#the-hwb-notation. This is an
  // implementation of the algorithm to transform sRGB to HWB.
  double max;
  GetHueMaxMin(hue, max, white);
  black = 1.0 - max;
}

Color ColorFromPremultipliedARGB(RGBA32 pixel_color) {
  int alpha = AlphaChannel(pixel_color);
  if (alpha && alpha < 255) {
    return Color::FromRGBA(RedChannel(pixel_color) * 255 / alpha,
                           GreenChannel(pixel_color) * 255 / alpha,
                           BlueChannel(pixel_color) * 255 / alpha, alpha);
  } else {
    return Color::FromRGBA32(pixel_color);
  }
}

RGBA32 PremultipliedARGBFromColor(const Color& color) {
  unsigned pixel_color;

  unsigned alpha = color.Alpha();
  if (alpha < 255) {
    pixel_color = Color::FromRGBA((color.Red() * alpha + 254) / 255,
                                  (color.Green() * alpha + 254) / 255,
                                  (color.Blue() * alpha + 254) / 255, alpha)
                      .Rgb();
  } else {
    pixel_color = color.Rgb();
  }

  return pixel_color;
}

// https://www.w3.org/TR/css-color-4/#legacy-color-syntax
bool Color::IsLegacyColor() const {
  return (color_space_ == ColorSpace::kRGBLegacy ||
          color_space_ == ColorSpace::kHSL || color_space_ == ColorSpace::kHWB);
}

// From https://www.w3.org/TR/css-color-4/#interpolation
// If the host syntax does not define what color space interpolation should
// take place in, it defaults to Oklab.
// However, user agents may handle interpolation between legacy sRGB color
// formats (hex colors, named colors, rgb(), hsl() or hwb() and the equivalent
// alpha-including forms) in gamma-encoded sRGB space.
Color::ColorInterpolationSpace Color::GetColorInterpolationSpace() const {
  if (IsLegacyColor())
    return ColorInterpolationSpace::kSRGB;

  return ColorInterpolationSpace::kOKLab;
}

// static
String Color::ColorInterpolationSpaceToString(
    Color::ColorInterpolationSpace color_space,
    Color::HueInterpolationMethod hue_interpolation_method) {
  StringBuilder result;
  switch (color_space) {
    case Color::ColorInterpolationSpace::kLab:
      result.Append("lab");
      break;
    case Color::ColorInterpolationSpace::kOKLab:
      result.Append("oklab");
      break;
    case Color::ColorInterpolationSpace::kLCH:
      result.Append("lch");
      break;
    case Color::ColorInterpolationSpace::kOKLCH:
      result.Append("oklch");
      break;
    case Color::ColorInterpolationSpace::kSRGBLinear:
      result.Append("srgb-linear");
      break;
    case Color::ColorInterpolationSpace::kSRGB:
      result.Append("srgb");
      break;
    case Color::ColorInterpolationSpace::kXYZD65:
      result.Append("xyz-d65");
      break;
    case Color::ColorInterpolationSpace::kXYZD50:
      result.Append("xyz-d50");
      break;
    case Color::ColorInterpolationSpace::kHSL:
      result.Append("hsl");
      break;
    case Color::ColorInterpolationSpace::kHWB:
      result.Append("hwb");
      break;
    case Color::ColorInterpolationSpace::kNone:
      result.Append("none");
      break;
  }

  if (color_space == Color::ColorInterpolationSpace::kLCH ||
      color_space == Color::ColorInterpolationSpace::kOKLCH ||
      color_space == Color::ColorInterpolationSpace::kHSL ||
      color_space == Color::ColorInterpolationSpace::kHWB) {
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
      case Color::HueInterpolationMethod::kSpecified:
        result.Append(" specified hue");
        break;
      // Shorter is the default value and does not get serialized
      case Color::HueInterpolationMethod::kShorter:
        break;
    }
  }

  return result.ReleaseString();
}

}  // namespace blink
