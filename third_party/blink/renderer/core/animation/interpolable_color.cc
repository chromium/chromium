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

// All colors are zero-initialized (transparent black).
InterpolableColor::InterpolableColor() = default;

std::unique_ptr<InterpolableColor> InterpolableColor::Create(Color color) {
  std::unique_ptr<InterpolableColor> result =
      std::make_unique<InterpolableColor>();
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

std::unique_ptr<InterpolableColor> InterpolableColor::Create(
    ColorKeyword color_keyword) {
  std::unique_ptr<InterpolableColor> result =
      std::make_unique<InterpolableColor>();
  unsigned keyword_index = static_cast<int>(color_keyword);
  DCHECK_LT(keyword_index, kColorKeywordCount);
  // color_keyword_fractions_ keeps track of keyword colors (like
  // "currentcolor") for interpolation. These keyword colors are not known at
  // specified value time, so we need to wait until we resolve them. Upon
  // creation the entry for the correct keyword is set to "1" and all others are
  // "0". These values are interpolated as normal. When the color is resolved
  // the proper fraction of the keyword color is added in.
  result->color_keyword_fractions_.Set(keyword_index, InterpolableNumber(1));
  // Keyword colors are functionally legacy colors for interpolation.
  result->color_space_ = Color::ColorSpace::kSRGBLegacy;

  return result;
}

std::unique_ptr<InterpolableColor> InterpolableColor::Create(
    CSSValueID keyword) {
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
    InterpolableNumber param0,
    InterpolableNumber param1,
    InterpolableNumber param2,
    InterpolableNumber alpha,
    InterpolableNumberList color_keyword_fractions,
    Color::ColorSpace color_space)
    : param0_(std::move(param0)),
      param1_(std::move(param1)),
      param2_(std::move(param2)),
      alpha_(std::move(alpha)),
      color_keyword_fractions_(std::move(color_keyword_fractions)),
      color_space_(color_space) {
  DCHECK_EQ(color_keyword_fractions_.length(), kColorKeywordCount);
}

InterpolableColor* InterpolableColor::RawClone() const {
  DCHECK_EQ(color_keyword_fractions_.length(), kColorKeywordCount);
  return new InterpolableColor(param0_, param1_, param2_, alpha_,
                               color_keyword_fractions_.Clone(), color_space_);
}

InterpolableColor* InterpolableColor::RawCloneAndZero() const {
  return new InterpolableColor(InterpolableNumber(0), InterpolableNumber(0),
                               InterpolableNumber(0), InterpolableNumber(0),
                               color_keyword_fractions_.CloneAndZero(),
                               color_space_);
}

Color InterpolableColor::GetColor() const {
  // Prevent dividing by zero.
  if (alpha_.Value() == 0)
    return Color::kTransparent;

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
  param0_.AssertCanInterpolateWith(other_color.param0_);
  param1_.AssertCanInterpolateWith(other_color.param1_);
  param2_.AssertCanInterpolateWith(other_color.param2_);
  alpha_.AssertCanInterpolateWith(other_color.alpha_);
  color_keyword_fractions_.AssertCanInterpolateWith(
      other_color.color_keyword_fractions_);
}

bool InterpolableColor::IsKeywordColor() const {
  // color_keyword_fractions_ indicate fractional blending amounts and are
  // important for resolving the color. If any of these store a non-zero value,
  // then the interpolated color is not the same as the color produced by simply
  // looking at the param values and color interpolation space.
  for (wtf_size_t i = 0; i < color_keyword_fractions_.length(); i++) {
    double keyword_fraction = color_keyword_fractions_.Get(i).Value();
    if (keyword_fraction != 0)
      return false;
  }
  return true;
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
  color_keyword_fractions_.Scale(scale);

#if DCHECK_IS_ON()
  DCHECK_EQ(param0_is_positive * (scale > 0), param0_.Value() > 0.0);
  DCHECK_EQ(param1_is_positive * (scale > 0), param1_.Value() > 0.0);
  DCHECK_EQ(param2_is_positive * (scale > 0), param2_.Value() > 0.0);
  DCHECK_EQ(alpha_is_positive * (scale > 0), alpha_.Value() > 0.0);
  for (unsigned i = 0; i < color_keyword_fractions_.length(); i++) {
    double fraction = color_keyword_fractions_.Get(i).Value();
    DCHECK_GE(fraction, 0);
    DCHECK_LE(fraction, 1);
  }
#endif
}

void InterpolableColor::Add(const InterpolableValue& other) {
  const InterpolableColor& other_color = To<InterpolableColor>(other);
  param0_.Add(other_color.param0_);
  param1_.Add(other_color.param1_);
  param2_.Add(other_color.param2_);
  alpha_.Add(other_color.alpha_);
  color_keyword_fractions_.Add(other_color.color_keyword_fractions_);
}

void InterpolableColor::Interpolate(const InterpolableValue& to,
                                    const double progress,
                                    InterpolableValue& result) const {
  const InterpolableColor& to_color = To<InterpolableColor>(to);
  InterpolableColor& result_color = To<InterpolableColor>(result);

  DCHECK_EQ(to_color.color_space_, color_space_);
  DCHECK_EQ(result_color.color_space_, color_space_);

  param0_.Interpolate(to_color.param0_, progress, result_color.param0_);
  param1_.Interpolate(to_color.param1_, progress, result_color.param1_);
  param2_.Interpolate(to_color.param2_, progress, result_color.param2_);
  alpha_.Interpolate(to_color.alpha_, progress, result_color.alpha_);

  color_keyword_fractions_.Interpolate(to_color.color_keyword_fractions_,
                                       progress,
                                       result_color.color_keyword_fractions_);
}

void InterpolableColor::Composite(const InterpolableColor& other,
                                  double fraction) {
  param0_.ScaleAndAdd(fraction, other.param0_);
  param1_.ScaleAndAdd(fraction, other.param1_);
  param2_.ScaleAndAdd(fraction, other.param2_);
  // TODO(crbug.com/981326): Test coverage has historically been missing for
  // composition of transparent colors. We should aim for interop with Firefox
  // and Safari.
  if (alpha_.Value() != other.alpha_.Value())
    alpha_.ScaleAndAdd(fraction, other.alpha_);

  color_keyword_fractions_.ScaleAndAdd(fraction,
                                       other.color_keyword_fractions_);
}

}  // namespace blink
