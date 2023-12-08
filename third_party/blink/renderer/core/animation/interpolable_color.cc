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

namespace blink {

InterpolableColor* InterpolableColor::Create(Color color) {
  InterpolableColor* result = MakeGarbageCollected<InterpolableColor>();
  result->color_space_ = color.GetColorInterpolationSpace();

  // A color is not necessarily "in" it's desired interpolation space.
  color.ConvertToColorSpace(result->color_space_);

  // All params are stored pre-multiplied.
  // https://www.w3.org/TR/css-color-4/#interpolation-alpha
  result->param0_.Set(color.Param0() * color.Alpha());
  result->param1_.Set(color.Param1() * color.Alpha());
  result->param2_.Set(color.Param2() * color.Alpha());
  result->alpha_.Set(color.Alpha());

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
      result->current_color_ = InlinedInterpolableNumber(1);
      break;
    case ColorKeyword::kWebkitActivelink:
      result->webkit_active_link_ = InlinedInterpolableNumber(1);
      break;
    case ColorKeyword::kWebkitLink:
      result->webkit_link_ = InlinedInterpolableNumber(1);
      break;
    case ColorKeyword::kQuirkInherit:
      result->quirk_inherit_ = InlinedInterpolableNumber(1);
      break;
  }
  // Keyword colors are functionally legacy colors for interpolation.
  result->color_space_ = Color::ColorSpace::kSRGBLegacy;

  return result;
}

InterpolableColor* InterpolableColor::Create(CSSValueID keyword) {
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
      // TODO(crbug.com/929098) Need to pass an appropriate color scheme here.
      return Create(LayoutTheme::GetTheme().FocusRingColor(
          mojom::blink::ColorScheme::kLight));
    default:
      DCHECK(StyleColor::IsColorKeyword(keyword));
      // TODO(crbug.com/929098) Need to pass an appropriate color scheme here.
      return Create(StyleColor::ColorFromKeyword(
          keyword, mojom::blink::ColorScheme::kLight));
  }
}

InterpolableColor::InterpolableColor(
    InlinedInterpolableNumber param0,
    InlinedInterpolableNumber param1,
    InlinedInterpolableNumber param2,
    InlinedInterpolableNumber alpha,
    InlinedInterpolableNumber current_color,
    InlinedInterpolableNumber webkit_active_link,
    InlinedInterpolableNumber webkit_link,
    InlinedInterpolableNumber quirk_inherit,
    Color::ColorSpace color_space)
    : param0_(std::move(param0)),
      param1_(std::move(param1)),
      param2_(std::move(param2)),
      alpha_(std::move(alpha)),
      current_color_(std::move(current_color)),
      webkit_active_link_(std::move(webkit_active_link)),
      webkit_link_(std::move(webkit_link)),
      quirk_inherit_(std::move(quirk_inherit)),
      color_space_(std::move(color_space)) {}

InterpolableColor* InterpolableColor::RawClone() const {
  return MakeGarbageCollected<InterpolableColor>(
      param0_, param1_, param2_, alpha_, current_color_, webkit_active_link_,
      webkit_link_, quirk_inherit_, color_space_);
}

InterpolableColor* InterpolableColor::RawCloneAndZero() const {
  return MakeGarbageCollected<InterpolableColor>(
      InlinedInterpolableNumber(0), InlinedInterpolableNumber(0),
      InlinedInterpolableNumber(0), InlinedInterpolableNumber(0),
      InlinedInterpolableNumber(0), InlinedInterpolableNumber(0),
      InlinedInterpolableNumber(0), InlinedInterpolableNumber(0), color_space_);
}

Color InterpolableColor::GetColor() const {
  // Prevent dividing by zero.
  if (alpha_.Value() == 0) {
    return Color::kTransparent;
  }

  // All params are stored pre-multiplied.
  float param0 = param0_.Value() / alpha_.Value();
  float param1 = param1_.Value() / alpha_.Value();
  float param2 = param2_.Value() / alpha_.Value();
  float alpha = ClampTo<double>(alpha_.Value(), 0, 1);

  switch (color_space_) {
    // There is no way for the user to specify which color spaces should be
    // used for interpolation, so sRGB (for legacy colors) and Oklab are the
    // only possibilities.
    case Color::ColorSpace::kSRGBLegacy:
    case Color::ColorSpace::kOklab:
      return Color::FromColorSpace(color_space_, param0, param1, param2, alpha);
    default:
      NOTREACHED();
      return Color();
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
  return current_color_.Value() || webkit_active_link_.Value() ||
         webkit_link_.Value() || quirk_inherit_.Value();
}

void InterpolableColor::ConvertToColorSpace(Color::ColorSpace color_space) {
  if (color_space_ == color_space) {
    return;
  }

  Color underlying_color = GetColor();
  underlying_color.ConvertToColorSpace(color_space);
  param0_.Set(underlying_color.Param0() * underlying_color.Alpha());
  param1_.Set(underlying_color.Param1() * underlying_color.Alpha());
  param2_.Set(underlying_color.Param2() * underlying_color.Alpha());
  alpha_.Set(underlying_color.Alpha());

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
  bool param0_is_positive = param0_.Value() > 0.0;
  bool param1_is_positive = param1_.Value() > 0.0;
  bool param2_is_positive = param2_.Value() > 0.0;
  bool alpha_is_positive = alpha_.Value() > 0.0;
#endif

  param0_.Scale(scale);
  param1_.Scale(scale);
  param2_.Scale(scale);
  alpha_.Scale(scale);
  current_color_.Scale(scale);
  webkit_active_link_.Scale(scale);
  webkit_link_.Scale(scale);
  quirk_inherit_.Scale(scale);

#if DCHECK_IS_ON()
  DCHECK_EQ(param0_is_positive * (scale > 0), param0_.Value() > 0.0);
  DCHECK_EQ(param1_is_positive * (scale > 0), param1_.Value() > 0.0);
  DCHECK_EQ(param2_is_positive * (scale > 0), param2_.Value() > 0.0);
  DCHECK_EQ(alpha_is_positive * (scale > 0), alpha_.Value() > 0.0);
  DCHECK_GE(current_color_.Value(), 0.);
  DCHECK_LE(current_color_.Value(), 1.);
  DCHECK_GE(webkit_active_link_.Value(), 0.);
  DCHECK_LE(webkit_active_link_.Value(), 1.);
  DCHECK_GE(webkit_link_.Value(), 0.);
  DCHECK_LE(webkit_link_.Value(), 1.);
  DCHECK_GE(quirk_inherit_.Value(), 0.);
  DCHECK_LE(quirk_inherit_.Value(), 1.);
#endif
}

void InterpolableColor::Add(const InterpolableValue& other) {
  const InterpolableColor& other_color = To<InterpolableColor>(other);
  param0_.Add(other_color.param0_.Value());
  param1_.Add(other_color.param1_.Value());
  param2_.Add(other_color.param2_.Value());
  alpha_.Add(other_color.alpha_.Value());
  current_color_.Add(other_color.current_color_.Value());
  webkit_active_link_.Add(other_color.webkit_active_link_.Value());
  webkit_link_.Add(other_color.webkit_link_.Value());
  quirk_inherit_.Add(other_color.quirk_inherit_.Value());
}

void InterpolableColor::Interpolate(const InterpolableValue& to,
                                    const double progress,
                                    InterpolableValue& result) const {
  const InterpolableColor& to_color = To<InterpolableColor>(to);
  InterpolableColor& result_color = To<InterpolableColor>(result);

  DCHECK_EQ(to_color.color_space_, color_space_);
  DCHECK_EQ(result_color.color_space_, color_space_);

  result_color.param0_.Set(
      param0_.Interpolate(to_color.param0_.Value(), progress));
  result_color.param1_.Set(
      param1_.Interpolate(to_color.param1_.Value(), progress));
  result_color.param2_.Set(
      param2_.Interpolate(to_color.param2_.Value(), progress));
  result_color.alpha_.Set(
      alpha_.Interpolate(to_color.alpha_.Value(), progress));

  result_color.current_color_.Set(
      current_color_.Interpolate(to_color.current_color_.Value(), progress));
  result_color.webkit_active_link_.Set(webkit_active_link_.Interpolate(
      to_color.webkit_active_link_.Value(), progress));
  result_color.webkit_link_.Set(
      webkit_link_.Interpolate(to_color.webkit_link_.Value(), progress));
  result_color.quirk_inherit_.Set(
      quirk_inherit_.Interpolate(to_color.quirk_inherit_.Value(), progress));
}

void InterpolableColor::Composite(const InterpolableColor& other,
                                  double fraction) {
  param0_.ScaleAndAdd(fraction, other.param0_.Value());
  param1_.ScaleAndAdd(fraction, other.param1_.Value());
  param2_.ScaleAndAdd(fraction, other.param2_.Value());
  // TODO(crbug.com/981326): Test coverage has historically been missing for
  // composition of transparent colors. We should aim for interop with Firefox
  // and Safari.
  if (alpha_.Value() != other.alpha_.Value()) {
    alpha_.ScaleAndAdd(fraction, other.alpha_.Value());
  }

  current_color_.ScaleAndAdd(fraction, other.current_color_.Value());
  webkit_active_link_.ScaleAndAdd(fraction, other.webkit_active_link_.Value());
  webkit_link_.ScaleAndAdd(fraction, other.webkit_link_.Value());
  quirk_inherit_.ScaleAndAdd(fraction, other.quirk_inherit_.Value());
}

}  // namespace blink
