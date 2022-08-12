/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

// TODO(crbug.com/1308932): Blink classes should use SkColor4f directly,
// ulitmately this class should be deleted.
class Color;

typedef unsigned RGBA32;  // RGBA quadruplet

// TODO(crbug.com/1351544): Remove these functions.
constexpr RGBA32 MakeRGB(int r, int g, int b) {
  return 0xFF000000 | ClampTo(r, 0, 255) << 16 | ClampTo(g, 0, 255) << 8 |
         ClampTo(b, 0, 255);
}

constexpr RGBA32 MakeRGBA(int r, int g, int b, int a) {
  return ClampTo(a, 0, 255) << 24 | ClampTo(r, 0, 255) << 16 |
         ClampTo(g, 0, 255) << 8 | ClampTo(b, 0, 255);
}

PLATFORM_EXPORT RGBA32 MakeRGBA32FromFloats(float r, float g, float b, float a);

constexpr double CalcHue(double temp1, double temp2, double hue_val) {
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

// Explanation of this algorithm can be found in the CSS Color 4 Module
// specification at https://drafts.csswg.org/css-color-4/#hsl-to-rgb with
// further explanation available at http://en.wikipedia.org/wiki/HSL_color_space

// Hue is in the range of 0.0 to 6.0, the remainder are in the range 0.0 to 1.0.
// Out parameters r, g, and b are also returned in range 0.0 to 1.0.
constexpr void HSLToRGB(double hue,
                        double saturation,
                        double lightness,
                        double& r,
                        double& g,
                        double& b) {
  if (!saturation) {
    r = g = b = lightness;
  } else {
    double temp2 = lightness <= 0.5
                       ? lightness * (1.0 + saturation)
                       : lightness + saturation - lightness * saturation;
    double temp1 = 2.0 * lightness - temp2;

    r = CalcHue(temp1, temp2, hue + 2.0);
    g = CalcHue(temp1, temp2, hue);
    b = CalcHue(temp1, temp2, hue - 2.0);
  }
}

// Hue is in the range of 0 to 6.0, the remainder are in the range 0 to 1.0
constexpr RGBA32 MakeRGBAFromHSLA(double hue,
                                  double saturation,
                                  double lightness,
                                  double alpha) {
  const double scale_factor = 255.0;
  double r = 0, g = 0, b = 0;
  HSLToRGB(hue, saturation, lightness, r, g, b);

  return MakeRGBA(static_cast<int>(round(r * scale_factor)),
                  static_cast<int>(round(g * scale_factor)),
                  static_cast<int>(round(b * scale_factor)),
                  static_cast<int>(round(alpha * scale_factor)));
}

// Hue is in the range of 0 to 6.0, the remainder are in the range 0 to 1.0
constexpr RGBA32 MakeRGBAFromHWBA(double hue,
                                  double white,
                                  double black,
                                  double alpha) {
  const double scale_factor = 255.0;

  if (white + black >= 1.0) {
    int gray = static_cast<int>(round(white / (white + black) * scale_factor));
    return MakeRGBA(gray, gray, gray,
                    static_cast<int>(round(alpha * scale_factor)));
  }

  // Leverage HSL to RGB conversion to find HWB to RGB, see
  // https://drafts.csswg.org/css-color-4/#hwb-to-rgb
  double r = 0, g = 0, b = 0;
  HSLToRGB(hue, 1.0, 0.5, r, g, b);
  r += white - (white + black) * r;
  g += white - (white + black) * g;
  b += white - (white + black) * b;

  return MakeRGBA(static_cast<int>(round(r * scale_factor)),
                  static_cast<int>(round(g * scale_factor)),
                  static_cast<int>(round(b * scale_factor)),
                  static_cast<int>(round(alpha * scale_factor)));
}

PLATFORM_EXPORT RGBA32
MakeRGBAFromCMYKA(float c, float m, float y, float k, float a);

PLATFORM_EXPORT int DifferenceSquared(const Color&, const Color&);

inline int RedChannel(RGBA32 color) {
  return (color >> 16) & 0xFF;
}
inline int GreenChannel(RGBA32 color) {
  return (color >> 8) & 0xFF;
}
inline int BlueChannel(RGBA32 color) {
  return color & 0xFF;
}
inline int AlphaChannel(RGBA32 color) {
  return (color >> 24) & 0xFF;
}

struct NamedColor {
  DISALLOW_NEW();
  const char* name;
  unsigned argb_value;
};

PLATFORM_EXPORT const NamedColor* FindColor(const char* str, unsigned len);

class PLATFORM_EXPORT Color {
  DISALLOW_NEW();

 public:
  // The default constructor creates a transparent color.
  constexpr Color() : color_(0) {}

  // TODO(crbug.com/1351544): Replace these constructors with explicit From
  // functions below. Replace the CreateUnchecked functions with FromRGB and
  // FromRGBA.
  Color(int r, int g, int b) : color_(MakeRGB(r, g, b)) {}
  Color(int r, int g, int b, int a) : color_(MakeRGBA(r, g, b, a)) {}
  // Color is currently limited to 32bit RGBA. Perhaps some day we'll support
  // better colors.
  Color(float r, float g, float b, float a)
      : color_(MakeRGBA32FromFloats(r, g, b, a)) {}
  // Creates a new color from the specific CMYK and alpha values.
  Color(float c, float m, float y, float k, float a)
      : color_(MakeRGBAFromCMYKA(c, m, y, k, a)) {}
  static constexpr Color CreateUnchecked(int r, int g, int b) {
    RGBA32 color = 0xFF000000 | r << 16 | g << 8 | b;
    return Color(color);
  }
  static constexpr Color CreateUnchecked(int r, int g, int b, int a) {
    RGBA32 color = a << 24 | r << 16 | g << 8 | b;
    return Color(color);
  }

  // Create a color using rgb() syntax.
  static constexpr Color FromRGB(int r, int g, int b) {
    return Color(0xFF000000 | ClampInt(r) << 16 | ClampInt(g) << 8 |
                 ClampInt(b));
  }

  // Create a color using rgba() syntax.
  static constexpr Color FromRGBA(int r, int g, int b, int a) {
    return Color(ClampInt(a) << 24 | ClampInt(r) << 16 | ClampInt(g) << 8 |
                 ClampInt(b));
  }

  // Create a color using the hsl() syntax.
  static constexpr Color FromHSLA(double h, double s, double l, double a) {
    return Color(MakeRGBAFromHSLA(h, s, l, a));
  }

  // Create a color using the hwb() syntax.
  static constexpr Color FromHWBA(double h, double w, double b, double a) {
    return Color(MakeRGBAFromHWBA(h, w, b, a));
  }

  // TODO(crbug.com/1308932): These three functions are just helpers for while
  // we're converting platform/graphics to float color.
  static Color FromSkColor4f(SkColor4f fc) {
    return Color(MakeRGBA32FromFloats(fc.fR, fc.fG, fc.fB, fc.fA));
  }
  static constexpr Color FromSkColor(SkColor color) { return Color(color); }
  static constexpr Color FromRGBA32(RGBA32 color) { return Color(color); }

  // Convert a Color to SkColor4f, for use in painting and compositing. Once a
  // Color has been converted to SkColor4f it should not be converted back.
  SkColor4f toSkColor4f() const;

  // Returns the color serialized according to HTML5:
  // http://www.whatwg.org/specs/web-apps/current-work/#serialization-of-a-color
  String Serialized() const;

  // Returns the color serialized as either #RRGGBB or #RRGGBBAA. The latter
  // format is not a valid CSS color, and should only be seen in DRT dumps.
  String NameForLayoutTreeAsText() const;

  // Returns whether parsing succeeded. The resulting Color is arbitrary
  // if parsing fails.
  bool SetFromString(const String&);
  bool SetNamedColor(const String&);

  bool HasAlpha() const { return Alpha() < 255; }

  int Red() const { return RedChannel(color_); }
  int Green() const { return GreenChannel(color_); }
  int Blue() const { return BlueChannel(color_); }
  int Alpha() const { return AlphaChannel(color_); }

  RGBA32 Rgb() const { return color_; }  // Preserve the alpha.
  void SetRGB(int r, int g, int b) { color_ = MakeRGB(r, g, b); }
  void SetRGB(RGBA32 rgb) { color_ = rgb; }
  void GetRGBA(float& r, float& g, float& b, float& a) const;
  void GetRGBA(double& r, double& g, double& b, double& a) const;
  void GetHSL(double& h, double& s, double& l) const;
  void GetHWB(double& h, double& w, double& b) const;

  // TODO(crbug.com/1308932): Remove this function, and replace its use with
  // toSkColor4f.
  explicit operator SkColor() const;

  Color Light() const;
  Color Dark() const;

  Color CombineWithAlpha(float other_alpha) const;

  // This is an implementation of Porter-Duff's "source-over" equation
  Color Blend(const Color&) const;
  Color BlendWithWhite() const;

  static bool ParseHexColor(const StringView&, RGBA32&);
  static bool ParseHexColor(const LChar*, unsigned, RGBA32&);
  static bool ParseHexColor(const UChar*, unsigned, RGBA32&);

  static const Color kBlack;
  static const Color kWhite;
  static const Color kDarkGray;
  static const Color kGray;
  static const Color kLightGray;
  static const Color kTransparent;

 private:
  constexpr explicit Color(RGBA32 color) : color_(color) {}
  static constexpr int ClampInt(int x) {
    return x < 0 ? 0 : (x > 255 ? 255 : x);
  }
  void GetHueMaxMin(double&, double&, double&) const;

  RGBA32 color_;
};

inline bool operator==(const Color& a, const Color& b) {
  return a.Rgb() == b.Rgb();
}

inline bool operator!=(const Color& a, const Color& b) {
  return !(a == b);
}

PLATFORM_EXPORT Color ColorFromPremultipliedARGB(RGBA32);
PLATFORM_EXPORT RGBA32 PremultipliedARGBFromColor(const Color&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_H_
