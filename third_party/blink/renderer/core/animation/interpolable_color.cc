// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_color.h"

#include <cmath>
#include <memory>
#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"

namespace {

double ScaleAndAdd(double value, double scale, double add) {
  return value * scale + add;
}

}  // namespace

namespace blink {

namespace {
// InterpolableColors are stored premultiplied (scaled by alpha) during the
// blending process for efficiency and unpremultiplied during resolution. This
// works since restricted to rectangular color spaces. This optimization step
// would not work in polar color spaces. Fortunately, interpolation is currently
// restricted to srgb-legacy and oklab.

// Apply a color blend. The first color in the blend, expressed as doubles and a
// colorspace is expected to already be in premultiplied form (scaled by alpha).
// The result is left in premultiplied form for efficiency.
std::tuple<double, double, double, double> AddPremultipliedColor(
    double param0,
    double param1,
    double param2,
    double alpha,
    double fraction,
    Color color,
    Color::ColorSpace color_space) {
  DCHECK(color_space == Color::ColorSpace::kSRGBLegacy ||
         color_space == Color::ColorSpace::kOklab);
  color.ConvertToColorSpace(color_space);
  return std::make_tuple(param0 + fraction * color.Param0() * color.Alpha(),
                         param1 + fraction * color.Param1() * color.Alpha(),
                         param2 + fraction * color.Param2() * color.Alpha(),
                         alpha + fraction * color.Alpha());
}

// Convert color parameters back to unpremultiplied form (not scaled by alpha)
// suitable for the Color constructor.
std::tuple<double, double, double> UnpremultiplyColor(double param0,
                                                      double param1,
                                                      double param2,
                                                      double alpha) {
  return std::make_tuple(param0 / alpha, param1 / alpha, param2 / alpha);
}

}  // namespace

InterpolableColor* InterpolableColor::Create(Color color) {
  InterpolableColor* result = MakeGarbageCollected<InterpolableColor>();
  result->color_space_ = color.GetColorInterpolationSpace();

  // A color is not necessarily "in" it's desired interpolation space.
  color.ConvertToColorSpace(result->color_space_);

  // All params are stored pre-multiplied.
  // https://www.w3.org/TR/css-color-4/#interpolation-alpha
  result->param0_ = color.Param0() * color.Alpha();
  result->param1_ = color.Param1() * color.Alpha();
  result->param2_ = color.Param2() * color.Alpha();
  result->alpha_ = color.Alpha();

  return result;
}

InterpolableColor* InterpolableColor::Create(ColorKeyword color_keyword) {
  InterpolableColor* result = MakeGarbageCollected<InterpolableColor>();
  // color_keyword_fractions_ keeps track of keyword colors (like
  // "currentcolor") for interpolation. These keyword colors are not known at
  // specified value time, so we need to wait until we resolve them. Upon
  // creation the entry for the correct keyword is set to "1" and all others are
  // "0". These values are interpolated as normal. When the color is resolved
  // the proper fraction of the keyword color is added in.
  switch (color_keyword) {
    case ColorKeyword::kCurrentcolor:
      result->current_color_ = 1.0;
      break;
    case ColorKeyword::kWebkitActivelink:
      result->webkit_active_link_ = 1.0;
      break;
    case ColorKeyword::kWebkitLink:
      result->webkit_link_ = 1.0;
      break;
    case ColorKeyword::kQuirkInherit:
      result->quirk_inherit_ = 1.0;
      break;
  }
  // Keyword colors are functionally legacy colors for interpolation.
  result->color_space_ = Color::ColorSpace::kSRGBLegacy;

  return result;
}

InterpolableColor* InterpolableColor::Create(
    CSSValueID keyword,
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider) {
  switch (keyword) {
    case CSSValueID::kCurrentcolor:
      return Create(ColorKeyword::kCurrentcolor);
    case CSSValueID::kWebkitActivelink:
      return Create(ColorKeyword::kWebkitActivelink);
    case CSSValueID::kWebkitLink:
      return Create(ColorKeyword::kWebkitLink);
    case CSSValueID::kInternalQuirkInherit:
      return Create(ColorKeyword::kQuirkInherit);
    case CSSValueID::kWebkitFocusRingColor:
      return Create(LayoutTheme::GetTheme().FocusRingColor(color_scheme));
    default:
      DCHECK(StyleColor::IsColorKeyword(keyword));
      // TODO(crbug.com/40229450): Pass down if within installed webapp scope
      // from Document.
      return Create(
          StyleColor::ColorFromKeyword(keyword, color_scheme, color_provider,
                                       /*is_in_web_app_scope=*/false));
  }
}

InterpolableColor* InterpolableColor::RawClone() const {
  return MakeGarbageCollected<InterpolableColor>(
      param0_, param1_, param2_, alpha_, current_color_, webkit_active_link_,
      webkit_link_, quirk_inherit_, color_space_);
}

InterpolableColor* InterpolableColor::RawCloneAndZero() const {
  return MakeGarbageCollected<InterpolableColor>(0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                                 0.0, 0.0, color_space_);
}

Color InterpolableColor::GetColor() const {
  // Prevent dividing by zero.
  if (alpha_ == 0) {
    return Color::kTransparent;
  }

  // All params are stored pre-multiplied.
  float param0 = param0_ / alpha_;
  float param1 = param1_ / alpha_;
  float param2 = param2_ / alpha_;
  float alpha = ClampTo<double>(alpha_, 0, 1);

  switch (color_space_) {
    // There is no way for the user to specify which color spaces should be
    // used for interpolation, so sRGB (for legacy colors) and Oklab are the
    // only possibilities.
    case Color::ColorSpace::kSRGBLegacy:
    case Color::ColorSpace::kOklab:
      return Color::FromColorSpace(color_space_, param0, param1, param2, alpha);
    default:
      NOTREACHED();
  }
}

void InterpolableColor::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableColor& other_color = To<InterpolableColor>(other);
  DCHECK_EQ(color_space_, other_color.color_space_);
}

bool InterpolableColor::IsKeywordColor() const {
  // color_keyword_fractions_ indicate fractional blending amounts and are
  // important for resolving the color. If any of these store a non-zero value,
  // then the interpolated color is not the same as the color produced by simply
  // looking at the param values and color interpolation space.
  return current_color_ || webkit_active_link_ || webkit_link_ ||
         quirk_inherit_;
}

void InterpolableColor::ConvertToColorSpace(Color::ColorSpace color_space) {
  if (color_space_ == color_space) {
    return;
  }

  Color underlying_color = GetColor();
  underlying_color.ConvertToColorSpace(color_space);
  param0_ = underlying_color.Param0() * underlying_color.Alpha();
  param1_ = underlying_color.Param1() * underlying_color.Alpha();
  param2_ = underlying_color.Param2() * underlying_color.Alpha();
  alpha_ = underlying_color.Alpha();

  color_space_ = color_space;
}

// static
void InterpolableColor::SetupColorInterpolationSpaces(InterpolableColor& to,
                                                      InterpolableColor& from) {
  // In the event that the two colorspaces are the same, there's nothing to do.
  if (to.color_space_ == from.color_space_) {
    return;
  }

  // sRGB and Oklab are the only possible interpolation spaces, so one should be
  // in Oklab and we should convert the other.
  DCHECK(from.color_space_ == Color::ColorSpace::kOklab ||
         to.color_space_ == Color::ColorSpace::kOklab);

  to.ConvertToColorSpace(Color::ColorSpace::kOklab);
  from.ConvertToColorSpace(Color::ColorSpace::kOklab);
}

void InterpolableColor::Scale(double scale) {
// A guard to prevent overload with very large values.
#if DCHECK_IS_ON()
  bool param0_is_positive = param0_ > 0.0;
  bool param1_is_positive = param1_ > 0.0;
  bool param2_is_positive = param2_ > 0.0;
  bool alpha_is_positive = alpha_ > 0.0;
#endif

  param0_ *= scale;
  param1_ *= scale;
  param2_ *= scale;
  alpha_ *= scale;
  current_color_ *= scale;
  webkit_active_link_ *= scale;
  webkit_link_ *= scale;
  quirk_inherit_ *= scale;

#if DCHECK_IS_ON()
  DCHECK_EQ(param0_is_positive * (scale > 0), param0_ > 0.0);
  DCHECK_EQ(param1_is_positive * (scale > 0), param1_ > 0.0);
  DCHECK_EQ(param2_is_positive * (scale > 0), param2_ > 0.0);
  DCHECK_EQ(alpha_is_positive * (scale > 0), alpha_ > 0.0);
  DCHECK_GE(current_color_, 0.);
  DCHECK_LE(current_color_, 1.);
  DCHECK_GE(webkit_active_link_, 0.);
  DCHECK_LE(webkit_active_link_, 1.);
  DCHECK_GE(webkit_link_, 0.);
  DCHECK_LE(webkit_link_, 1.);
  DCHECK_GE(quirk_inherit_, 0.);
  DCHECK_LE(quirk_inherit_, 1.);
#endif
}

void InterpolableColor::Add(const InterpolableValue& other) {
  const InterpolableColor& other_color = To<InterpolableColor>(other);
  param0_ += other_color.param0_;
  param1_ += other_color.param1_;
  param2_ += other_color.param2_;
  alpha_ += other_color.alpha_;
  current_color_ += other_color.current_color_;
  webkit_active_link_ += other_color.webkit_active_link_;
  webkit_link_ += other_color.webkit_link_;
  quirk_inherit_ += other_color.quirk_inherit_;
}

Color InterpolableColor::Resolve(const Color& current_color,
                                 const Color& active_link_color,
                                 const Color& link_color,
                                 const Color& text_color,
                                 mojom::blink::ColorScheme color_scheme) const {
  double param0 = Param0();
  double param1 = Param1();
  double param2 = Param2();
  double alpha = Alpha();

  if (double currentcolor_fraction = current_color_) {
    std::tie(param0, param1, param2, alpha) = AddPremultipliedColor(
        param0, param1, param2, alpha, currentcolor_fraction, current_color,
        color_space_);
  }
  if (double webkit_activelink_fraction = webkit_active_link_) {
    std::tie(param0, param1, param2, alpha) = AddPremultipliedColor(
        param0, param1, param2, alpha, webkit_activelink_fraction,
        active_link_color, color_space_);
  }
  if (double webkit_link_fraction = webkit_link_) {
    std::tie(param0, param1, param2, alpha) =
        AddPremultipliedColor(param0, param1, param2, alpha,
                              webkit_link_fraction, link_color, color_space_);
  }
  if (double quirk_inherit_fraction = quirk_inherit_) {
    std::tie(param0, param1, param2, alpha) =
        AddPremultipliedColor(param0, param1, param2, alpha,
                              quirk_inherit_fraction, text_color, color_space_);
  }

  alpha = ClampTo<double>(alpha, 0, 1);
  if (alpha == 0) {
    return Color::FromColorSpace(color_space_, param0, param1, param2, 0);
  }

  std::tie(param0, param1, param2) =
      UnpremultiplyColor(param0, param1, param2, alpha);

  switch (color_space_) {
    case Color::ColorSpace::kSRGBLegacy:
    case Color::ColorSpace::kOklab:
      return Color::FromColorSpace(color_space_, param0, param1, param2, alpha);
    default:
      // There is no way for the user to specify which color spaces should be
      // used for interpolation, so sRGB (for legacy colors) and Oklab are
      // the only possibilities.
      // https://www.w3.org/TR/css-color-4/#interpolation-space
      NOTREACHED();
  }
}

void InterpolableColor::Interpolate(const InterpolableValue& to,
                                    const double progress,
                                    InterpolableValue& result) const {
  const InterpolableColor& to_color = To<InterpolableColor>(to);
  InterpolableColor& result_color = To<InterpolableColor>(result);

  DCHECK_EQ(to_color.color_space_, color_space_);

  result_color.param0_ =
      InterpolableNumber::Interpolate(param0_, to_color.param0_, progress);
  result_color.param1_ =
      InterpolableNumber::Interpolate(param1_, to_color.param1_, progress);
  result_color.param2_ =
      InterpolableNumber::Interpolate(param2_, to_color.param2_, progress);
  result_color.alpha_ =
      InterpolableNumber::Interpolate(alpha_, to_color.alpha_, progress);

  result_color.current_color_ = InterpolableNumber::Interpolate(
      current_color_, to_color.current_color_, progress);
  result_color.webkit_active_link_ = InterpolableNumber::Interpolate(
      webkit_active_link_, to_color.webkit_active_link_, progress);
  result_color.webkit_link_ = InterpolableNumber::Interpolate(
      webkit_link_, to_color.webkit_link_, progress);
  result_color.quirk_inherit_ = InterpolableNumber::Interpolate(
      quirk_inherit_, to_color.quirk_inherit_, progress);
  result_color.color_space_ = color_space_;
}

void InterpolableColor::Composite(const BaseInterpolableColor& value,
                                  double fraction) {
  auto& other = To<InterpolableColor>(value);

  param0_ = ::ScaleAndAdd(param0_, fraction, other.param0_);
  param1_ = ::ScaleAndAdd(param1_, fraction, other.param1_);
  param2_ = ::ScaleAndAdd(param2_, fraction, other.param2_);
  // TODO(crbug.com/981326): Test coverage has historically been missing for
  // composition of transparent colors. We should aim for interop with Firefox
  // and Safari.
  if (alpha_ != other.alpha_) {
    alpha_ = ::ScaleAndAdd(alpha_, fraction, other.alpha_);
  }

  current_color_ =
      ::ScaleAndAdd(current_color_, fraction, other.current_color_);
  webkit_active_link_ =
      ::ScaleAndAdd(webkit_active_link_, fraction, other.webkit_active_link_);
  webkit_link_ = ::ScaleAndAdd(webkit_link_, fraction, other.webkit_link_);
  quirk_inherit_ =
      ::ScaleAndAdd(quirk_inherit_, fraction, other.quirk_inherit_);
}

}  // namespace blink
