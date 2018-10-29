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
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

class Color;

typedef unsigned RGBA32;  // RGBA quadruplet

PLATFORM_EXPORT RGBA32 MakeRGB(int r, int g, int b);
PLATFORM_EXPORT RGBA32 MakeRGBA(int r, int g, int b, int a);

PLATFORM_EXPORT RGBA32 MakeRGBA32FromFloats(float r, float g, float b, float a);
PLATFORM_EXPORT RGBA32 MakeRGBAFromHSLA(double h, double s, double l, double a);
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
  Color() : color_(Color::kTransparent) {}
  Color(RGBA32 color) : color_(color) {}
  Color(int r, int g, int b) : color_(MakeRGB(r, g, b)) {}
  Color(int r, int g, int b, int a) : color_(MakeRGBA(r, g, b, a)) {}
  // Color is currently limited to 32bit RGBA. Perhaps some day we'll support
  // better colors.
  Color(float r, float g, float b, float a)
      : color_(MakeRGBA32FromFloats(r, g, b, a)) {}
  // Creates a new color from the specific CMYK and alpha values.
  Color(float c, float m, float y, float k, float a)
      : color_(MakeRGBAFromCMYKA(c, m, y, k, a)) {}

  static Color CreateUnchecked(int r, int g, int b) {
    RGBA32 color = 0xFF000000 | r << 16 | g << 8 | b;
    return Color(color);
  }
  static Color CreateUnchecked(int r, int g, int b, int a) {
    RGBA32 color = a << 24 | r << 16 | g << 8 | b;
    return Color(color);
  }

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

  Color Light() const;
  Color Dark() const;

  Color CombineWithAlpha(float other_alpha) const;

  // This is an implementation of Porter-Duff's "source-over" equation
  Color Blend(const Color&) const;
  Color BlendWithWhite() const;

  static bool ParseHexColor(const StringView&, RGBA32&);
  static bool ParseHexColor(const LChar*, unsigned, RGBA32&);
  static bool ParseHexColor(const UChar*, unsigned, RGBA32&);

  static const RGBA32 kBlack = 0xFF000000;
  static const RGBA32 kWhite = 0xFFFFFFFF;
  static const RGBA32 kDarkGray = 0xFF808080;
  static const RGBA32 kGray = 0xFFA0A0A0;
  static const RGBA32 kLightGray = 0xFFC0C0C0;
  static const RGBA32 kTransparent = 0x00000000;

 private:
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
