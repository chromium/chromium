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

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

typedef unsigned RGBA32;  // RGBA quadruplet

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
  constexpr Color()
      : param0_is_none_(0),
        param1_is_none_(0),
        param2_is_none_(0),
        alpha_is_none_(0) {}

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

  // Create a color using the rgba() syntax, with float arguments. All
  // parameters will be clamped to the [0, 1] interval.
  static Color FromRGBAFloat(float r, float g, float b, float a);

  // Create a color using the hsl() syntax.
  static Color FromHSLA(double h, double s, double l, double a);

  // Create a color using the hwb() syntax.
  static Color FromHWBA(double h, double w, double b, double a);

  // Create a color using the color() function. This includes both predefined
  // color spaces and xyz spaces. Parameters that are none should be specified
  // as absl::nullopt. The value for `alpha` will be clamped to the [0, 1]
  // interval.
  enum class ColorFunctionSpace : uint8_t {
    kSRGB,
    kSRGBLinear,
    kDisplayP3,
    kA98RGB,
    kProPhotoRGB,
    kRec2020,
    kXYZD50,
    kXYZD65,
  };
  static Color FromColorFunction(ColorFunctionSpace space,
                                 absl::optional<float> red_or_x,
                                 absl::optional<float> green_or_y,
                                 absl::optional<float> blue_or_z,
                                 absl::optional<float> alpha);

  // Create a color using the lab() and oklab() functions. Parameters that are
  // none should be specified as absl::nullopt. The value for `L` will be
  // clamped to be non-negative. The value for `alpha` will be clamped to the
  // [0, 1] interval.
  static Color FromLab(absl::optional<float> L,
                       absl::optional<float> a,
                       absl::optional<float> b,
                       absl::optional<float> alpha);
  static Color FromOKLab(absl::optional<float> L,
                         absl::optional<float> a,
                         absl::optional<float> b,
                         absl::optional<float> alpha);

  // Create a color using the lch() and oklch() functions. Parameters that are
  // none should be specified as absl::nullopt. The value for `L` and `chroma`
  // will be clamped to be non-negative. The value for `alpha` will be clamped
  // to the [0, 1] interval.
  static Color FromLCH(absl::optional<float> L,
                       absl::optional<float> chroma,
                       absl::optional<float> hue,
                       absl::optional<float> alpha);
  static Color FromOKLCH(absl::optional<float> L,
                         absl::optional<float> chroma,
                         absl::optional<float> hue,
                         absl::optional<float> alpha);

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
  String SerializeAsCSSColor() const;
  // Canvas colors are serialized somewhat differently:
  // https://html.spec.whatwg.org/multipage/canvas.html#serialisation-of-a-color
  String SerializeAsCanvasColor() const;

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

  // Transform to an SkColor. This will clamp to sRGB gamut and 8 bit precision.
  // TODO(crbug.com/1308932): Remove this function, and replace its use with
  // toSkColor4f.
  SkColor ToSkColorDeprecated() const;

  Color Dark() const;

  Color CombineWithAlpha(float other_alpha) const;

  // This is an implementation of Porter-Duff's "source-over" equation
  // TODO(https://crbug.com/1333988): Implement CSS Color level 4 blending,
  // including a color interpolation method parameter.
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
    return serialization_type_ == other.serialization_type_ &&
           color_function_space_ == other.color_function_space_ &&
           param0_is_none_ == other.param0_is_none_ &&
           param1_is_none_ == other.param1_is_none_ &&
           param2_is_none_ == other.param2_is_none_ &&
           alpha_is_none_ == other.alpha_is_none_ && param0_ == other.param0_ &&
           param1_ == other.param1_ && param2_ == other.param2_ &&
           alpha_ == other.alpha_;
  }
  inline bool operator!=(const Color& other) const { return !(*this == other); }

  unsigned GetHash() const;
  bool IsLegacyColor() const;

  enum class ColorInterpolationSpace : uint8_t {
    // Linear in light intensity
    kXYZD65,
    kXYZD50,
    kSRGBLinear,
    // Perceptually uniform
    kLab,
    kOKLab,
    // Maximizing chroma
    kLCH,
    kOKLCH,
    // Legacy fallback
    kSRGB,
    // Polar spaces
    kHSL,
    kHWB,
    // Not specified
    kNone,
  };
  ColorInterpolationSpace GetColorInterpolationSpace() const;
  enum class HueInterpolationMethod : uint8_t {
    kShorter,
    kLonger,
    kIncreasing,
    kDecreasing,
    kSpecified,
  };

 private:
  constexpr explicit Color(RGBA32 color)
      : param0_is_none_(0),
        param1_is_none_(0),
        param2_is_none_(0),
        alpha_is_none_(0),
        param0_(((color >> 16) & 0xFF) / 255.f),
        param1_(((color >> 8) & 0xFF) / 255.f),
        param2_(((color >> 0) & 0xFF) / 255.f),
        alpha_(((color >> 24) & 0xFF) / 255.f) {}
  static constexpr int ClampInt(int x) {
    return x < 0 ? 0 : (x > 255 ? 255 : x);
  }
  void GetHueMaxMin(double&, double&, double&) const;

  // The way that this color will be serialized. The value of
  // `serialization_type` determines the interpretation of `params_`.
  enum class SerializationType : uint8_t {
    // Serializes to rgb() or rgba(). The values of `params0_`, `params1_`, and
    // `params2_` are red, green, and blue sRGB values, and are guaranteed to be
    // present and in the [0, 1] interval.
    kRGB,
    // Serialize to the color() syntax of a given predefined color space. The
    // values of `params0_`, `params1_`, and `params2_` are red, green, and blue
    // values in the color space specified by `color_function_space_`.
    kColor,
    // Serializes to lab(). The value of `param0_` is lightness and is
    // guaranteed to be non-negative. The value of `param1_` and `param2_` are
    // the a-axis and b-axis values and are unbounded.
    kLab,
    // Serializes to oklab(). Parameter meanings are the same as for kLab.
    kOKLab,
    // Serializes to lch(). The value of `param0_` is lightness and is
    // guaranteed to be non-negative. The value of `param1_` is chroma and is
    // also guaranteed to be non-negative. The value of `param2_` is hue, and
    // is unbounded.
    kLCH,
    // Serializes to oklch(). Parameter meanings are the same as for kLCH.
    kOKLCH,
  };
  SerializationType serialization_type_ = SerializationType::kRGB;

  // The color space for serialization type kColor. For all other serialization
  // types this is not used, and must be set to kSRGB.
  ColorFunctionSpace color_function_space_ = ColorFunctionSpace::kSRGB;

  // Whether or not color parameters were specified as none (this only affects
  // interpolation behavior, the parameter values area always valid).
  unsigned param0_is_none_ : 1;
  unsigned param1_is_none_ : 1;
  unsigned param2_is_none_ : 1;
  unsigned alpha_is_none_ : 1;

  // The color parameters.
  float param0_ = 0.f;
  float param1_ = 0.f;
  float param2_ = 0.f;

  // The alpha value for the color is guaranteed to be in the [0, 1] interval.
  float alpha_ = 0.f;
};

PLATFORM_EXPORT int DifferenceSquared(const Color&, const Color&);
PLATFORM_EXPORT Color ColorFromPremultipliedARGB(RGBA32);
PLATFORM_EXPORT RGBA32 PremultipliedARGBFromColor(const Color&);

}  // namespace blink

namespace WTF {
template <>
struct DefaultHash<blink::Color> {
  STATIC_ONLY(DefaultHash);
  struct Hash {
    STATIC_ONLY(Hash);
    static unsigned GetHash(const blink::Color& key) { return key.GetHash(); }
    static bool Equal(const blink::Color& a, const blink::Color& b) {
      return a == b;
    }
    static const bool safe_to_compare_to_empty_or_deleted = true;
  };
};
}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_H_
