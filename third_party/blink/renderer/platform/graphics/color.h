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

PLATFORM_EXPORT int DifferenceSquared(const Color&, const Color&);

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
  constexpr Color() = default;

  // TODO(crbug.com/1351544): Replace these constructors with explicit From
  // functions below.
  Color(int r, int g, int b);
  Color(int r, int g, int b, int a);

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

  // Create a color using the rgba() syntax, with float arguments.
  static Color FromRGBAFloat(float r, float g, float b, float a);

  // Create a color using the hsl() syntax.
  static Color FromHSLA(double h, double s, double l, double a);

  // Create a color using the hwb() syntax.
  static Color FromHWBA(double h, double w, double b, double a);

  // TODO(crbug.com/1308932): These three functions are just helpers for while
  // we're converting platform/graphics to float color.
  static Color FromSkColor4f(SkColor4f fc);
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

  // Return true if the color is not opaque.
  bool HasAlpha() const;

  // Access the color as though it were created using rgba syntax. This will
  // clamp all colors to an 8-bit sRGB representation. All callers of these
  // functions should be audited. The function Rgb(), despite the name, does
  // not drop the alpha value.
  int Red() const;
  int Green() const;
  int Blue() const;
  int Alpha() const;
  RGBA32 Rgb() const;
  void GetRGBA(float& r, float& g, float& b, float& a) const;
  void GetRGBA(double& r, double& g, double& b, double& a) const;

  // Access the color as though it were created using the hsl() syntax.
  void GetHSL(double& h, double& s, double& l) const;

  // Access the color as though it were created using the hwb() syntax.
  void GetHWB(double& h, double& w, double& b) const;

  // TODO(crbug.com/1308932): Remove this function, and replace its use with
  // toSkColor4f.
  explicit operator SkColor() const;

  Color Dark() const;

  Color CombineWithAlpha(float other_alpha) const;

  // This is an implementation of Porter-Duff's "source-over" equation
  Color Blend(const Color&) const;
  Color BlendWithWhite() const;

  static bool ParseHexColor(const StringView&, Color&);
  static bool ParseHexColor(const LChar*, unsigned, Color&);
  static bool ParseHexColor(const UChar*, unsigned, Color&);

  static const Color kBlack;
  static const Color kWhite;
  static const Color kDarkGray;
  static const Color kGray;
  static const Color kLightGray;
  static const Color kTransparent;

  inline bool operator==(const Color& other) const {
    return param0_ == other.param0_ && param1_ == other.param1_ &&
           param2_ == other.param2_ && alpha_ == other.alpha_;
  }
  inline bool operator!=(const Color& other) const { return !(*this == other); }

 private:
  constexpr explicit Color(RGBA32 color)
      : param0_(((color >> 16) & 0xFF) / 255.f),
        param1_(((color >> 8) & 0xFF) / 255.f),
        param2_(((color >> 0) & 0xFF) / 255.f),
        alpha_(((color >> 24) & 0xFF) / 255.f) {}
  static constexpr int ClampInt(int x) {
    return x < 0 ? 0 : (x > 255 ? 255 : x);
  }
  void GetHueMaxMin(double&, double&, double&) const;

  // The parameters for the color. These are currently red, green, and blue sRGB
  // values.
  float param0_ = 0.f;
  float param1_ = 0.f;
  float param2_ = 0.f;

  // The alpha value for the color is guaranteed to be in the interval [0, 1].
  float alpha_ = 0.f;
};

PLATFORM_EXPORT Color ColorFromPremultipliedARGB(RGBA32);
PLATFORM_EXPORT RGBA32 PremultipliedARGBFromColor(const Color&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_H_
