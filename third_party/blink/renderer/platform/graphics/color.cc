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

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/decimal.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

// VS 2015 and above allow these definitions and in this case require them
#if !defined(COMPILER_MSVC) || _MSC_VER >= 1900
// FIXME: Use C++11 enum classes to avoid static data member initializer
// definition problems.
const RGBA32 Color::kBlack;
const RGBA32 Color::kWhite;
const RGBA32 Color::kDarkGray;
const RGBA32 Color::kGray;
const RGBA32 Color::kLightGray;
const RGBA32 Color::kTransparent;
#endif

namespace {

const RGBA32 kLightenedBlack = 0xFF545454;
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

double CalcHue(double temp1, double temp2, double hue_val) {
  if (hue_val < 0.0)
    hue_val += 6.0;
  else if (hue_val >= 6.0)
    hue_val -= 6.0;
  if (hue_val < 1.0)
    return temp1 + (temp2 - temp1) * hue_val;
  if (hue_val < 3.0)
    return temp2;
  if (hue_val < 4.0)
    return temp1 + (temp2 - temp1) * (4.0 - hue_val);
  return temp1;
}

int ColorFloatToRGBAByte(float f) {
  return clampTo(static_cast<int>(lroundf(255.0f * f)), 0, 255);
}

// originally moved here from the CSS parser
template <typename CharacterType>
inline bool ParseHexColorInternal(const CharacterType* name,
                                  unsigned length,
                                  RGBA32& rgb) {
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
    rgb = 0xFF000000 | value;
    return true;
  }
  if (length == 8) {
    // We parsed the values into RGBA order, but the RGBA32 type
    // expects them to be in ARGB order, so we right rotate eight bits.
    rgb = value << 24 | value >> 8;
    return true;
  }
  if (length == 4) {
    // #abcd converts to ddaabbcc in RGBA32.
    rgb = (value & 0xF) << 28 | (value & 0xF) << 24 | (value & 0xF000) << 8 |
          (value & 0xF000) << 4 | (value & 0xF00) << 4 | (value & 0xF00) |
          (value & 0xF0) | (value & 0xF0) >> 4;
    return true;
  }
  // #abc converts to #aabbcc
  rgb = 0xFF000000 | (value & 0xF00) << 12 | (value & 0xF00) << 8 |
        (value & 0xF0) << 8 | (value & 0xF0) << 4 | (value & 0xF) << 4 |
        (value & 0xF);
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

}  // namespace

RGBA32 MakeRGB(int r, int g, int b) {
  return 0xFF000000 | clampTo(r, 0, 255) << 16 | clampTo(g, 0, 255) << 8 |
         clampTo(b, 0, 255);
}

RGBA32 MakeRGBA(int r, int g, int b, int a) {
  return clampTo(a, 0, 255) << 24 | clampTo(r, 0, 255) << 16 |
         clampTo(g, 0, 255) << 8 | clampTo(b, 0, 255);
}

RGBA32 MakeRGBA32FromFloats(float r, float g, float b, float a) {
  return ColorFloatToRGBAByte(a) << 24 | ColorFloatToRGBAByte(r) << 16 |
         ColorFloatToRGBAByte(g) << 8 | ColorFloatToRGBAByte(b);
}

// Explanation of this algorithm can be found in the CSS Color 4 Module
// specification at https://drafts.csswg.org/css-color-4/#hsl-to-rgb with
// further explanation available at http://en.wikipedia.org/wiki/HSL_color_space

// Hue is in the range of 0 to 6.0, the remainder are in the range 0 to 1.0
RGBA32 MakeRGBAFromHSLA(double hue,
                        double saturation,
                        double lightness,
                        double alpha) {
  const double scale_factor = 255.0;

  if (!saturation) {
    int grey_value = static_cast<int>(round(lightness * scale_factor));
    return MakeRGBA(grey_value, grey_value, grey_value,
                    static_cast<int>(round(alpha * scale_factor)));
  }

  double temp2 = lightness <= 0.5
                     ? lightness * (1.0 + saturation)
                     : lightness + saturation - lightness * saturation;
  double temp1 = 2.0 * lightness - temp2;

  return MakeRGBA(
      static_cast<int>(round(CalcHue(temp1, temp2, hue + 2.0) * scale_factor)),
      static_cast<int>(round(CalcHue(temp1, temp2, hue) * scale_factor)),
      static_cast<int>(round(CalcHue(temp1, temp2, hue - 2.0) * scale_factor)),
      static_cast<int>(round(alpha * scale_factor)));
}

RGBA32 MakeRGBAFromCMYKA(float c, float m, float y, float k, float a) {
  double colors = 1 - k;
  int r = static_cast<int>(nextafter(256, 0) * (colors * (1 - c)));
  int g = static_cast<int>(nextafter(256, 0) * (colors * (1 - m)));
  int b = static_cast<int>(nextafter(256, 0) * (colors * (1 - y)));
  return MakeRGBA(r, g, b, static_cast<float>(nextafter(256, 0) * a));
}

bool Color::ParseHexColor(const LChar* name, unsigned length, RGBA32& rgb) {
  return ParseHexColorInternal(name, length, rgb);
}

bool Color::ParseHexColor(const UChar* name, unsigned length, RGBA32& rgb) {
  return ParseHexColorInternal(name, length, rgb);
}

bool Color::ParseHexColor(const StringView& name, RGBA32& rgb) {
  if (name.IsEmpty())
    return false;
  if (name.Is8Bit())
    return ParseHexColor(name.Characters8(), name.length(), rgb);
  return ParseHexColor(name.Characters16(), name.length(), rgb);
}

int DifferenceSquared(const Color& c1, const Color& c2) {
  int d_r = c1.Red() - c2.Red();
  int d_g = c1.Green() - c2.Green();
  int d_b = c1.Blue() - c2.Blue();
  return d_r * d_r + d_g * d_g + d_b * d_b;
}

bool Color::SetFromString(const String& name) {
  if (name[0] != '#')
    return SetNamedColor(name);
  if (name.Is8Bit())
    return ParseHexColor(name.Characters8() + 1, name.length() - 1, color_);
  return ParseHexColor(name.Characters16() + 1, name.length() - 1, color_);
}

String Color::Serialized() const {
  if (!HasAlpha())
    return String::Format("#%02x%02x%02x", Red(), Green(), Blue());

  StringBuilder result;
  result.ReserveCapacity(28);

  result.Append("rgba(");
  result.AppendNumber(Red());
  result.Append(", ");
  result.AppendNumber(Green());
  result.Append(", ");
  result.AppendNumber(Blue());
  result.Append(", ");

  if (!Alpha())
    result.Append('0');
  else {
    result.Append(Decimal::FromDouble(Alpha() / 255.0).ToString());
  }

  result.Append(')');
  return result.ToString();
}

String Color::NameForLayoutTreeAsText() const {
  if (Alpha() < 0xFF)
    return String::Format("#%02X%02X%02X%02X", Red(), Green(), Blue(), Alpha());
  return String::Format("#%02X%02X%02X", Red(), Green(), Blue());
}

bool Color::SetNamedColor(const String& name) {
  const NamedColor* found_color = FindNamedColor(name);
  color_ = found_color ? found_color->argb_value : 0;
  return found_color;
}

Color Color::Light() const {
  // Hardcode this common case for speed.
  if (color_ == kBlack)
    return kLightenedBlack;

  const float scale_factor = nextafterf(256.0f, 0.0f);

  float r, g, b, a;
  GetRGBA(r, g, b, a);

  float v = std::max(r, std::max(g, b));

  if (v == 0.0f)
    // Lightened black with alpha.
    return Color(0x54, 0x54, 0x54, Alpha());

  float multiplier = std::min(1.0f, v + 0.33f) / v;

  return Color(static_cast<int>(multiplier * r * scale_factor),
               static_cast<int>(multiplier * g * scale_factor),
               static_cast<int>(multiplier * b * scale_factor), Alpha());
}

Color Color::Dark() const {
  // Hardcode this common case for speed.
  if (color_ == kWhite)
    return kDarkenedWhite;

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
  return rgb_only | ColorFloatToRGBAByte(override_alpha) << 24;
}

Color Color::Blend(const Color& source) const {
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

void Color::GetHSL(double& hue, double& saturation, double& lightness) const {
  // http://en.wikipedia.org/wiki/HSL_color_space. This is a direct copy of
  // the algorithm therein, although it's 360^o based and we end up wanting
  // [0...1) based. It's clearer if we stick to 360^o until the end.
  double r = static_cast<double>(Red()) / 255.0;
  double g = static_cast<double>(Green()) / 255.0;
  double b = static_cast<double>(Blue()) / 255.0;
  double max = std::max(std::max(r, g), b);
  double min = std::min(std::min(r, g), b);

  if (max == min)
    hue = 0.0;
  else if (max == r)
    hue = (60.0 * ((g - b) / (max - min))) + 360.0;
  else if (max == g)
    hue = (60.0 * ((b - r) / (max - min))) + 120.0;
  else
    hue = (60.0 * ((r - g) / (max - min))) + 240.0;

  if (hue >= 360.0)
    hue -= 360.0;

  // makeRGBAFromHSLA assumes that hue is in [0...1).
  hue /= 360.0;

  lightness = 0.5 * (max + min);
  if (max == min)
    saturation = 0.0;
  else if (lightness <= 0.5)
    saturation = ((max - min) / (max + min));
  else
    saturation = ((max - min) / (2.0 - (max + min)));
}

Color ColorFromPremultipliedARGB(RGBA32 pixel_color) {
  int alpha = AlphaChannel(pixel_color);
  if (alpha && alpha < 255) {
    return Color::CreateUnchecked(RedChannel(pixel_color) * 255 / alpha,
                                  GreenChannel(pixel_color) * 255 / alpha,
                                  BlueChannel(pixel_color) * 255 / alpha,
                                  alpha);
  } else
    return Color(pixel_color);
}

RGBA32 PremultipliedARGBFromColor(const Color& color) {
  unsigned pixel_color;

  unsigned alpha = color.Alpha();
  if (alpha < 255) {
    pixel_color =
        Color::CreateUnchecked((color.Red() * alpha + 254) / 255,
                               (color.Green() * alpha + 254) / 255,
                               (color.Blue() * alpha + 254) / 255, alpha)
            .Rgb();
  } else
    pixel_color = color.Rgb();

  return pixel_color;
}

}  // namespace blink
