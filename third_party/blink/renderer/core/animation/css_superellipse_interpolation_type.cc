// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_superellipse_interpolation_type.h"

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/number_property_functions.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_superellipse_value.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/superellipse.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

static constexpr double kNotchInterpolationValue = 0;
static constexpr double kBevelInterpolationValue = 0.5;
static constexpr double kSquareInterpolationValue = 1;

// https://drafts.csswg.org/css-borders-4/#corner-shape-interpolation
double SuperellipseToInterpolableValue(Superellipse superellipse) {
  if (superellipse.IsDegenerate()) {
    return kSquareInterpolationValue;
  }
  if (superellipse.IsFullyConcave()) {
    return kNotchInterpolationValue;
  }
  const double convex_half_corner =
      std::pow(0.5, std::pow(0.5, std::abs(superellipse.Parameter())));
  CHECK_GE(convex_half_corner, 0.5);
  CHECK_LE(convex_half_corner, 1);
  return superellipse.IsConvex() ? convex_half_corner : 1 - convex_half_corner;
}

// https://drafts.csswg.org/css-borders-4/#corner-shape-interpolation
Superellipse SuperellipseFromInterpolableValue(
    const InterpolableValue& interpolable_value) {
  static constexpr double kEpsilon = 0.001;
  const double value = To<InterpolableNumber>(interpolable_value).Value();
  static const double kScoopInterpolationValue =
      SuperellipseToInterpolableValue(Superellipse::Scoop());
  static const double kRoundInterpolationValue =
      SuperellipseToInterpolableValue(Superellipse::Round());
  static const double kSquircleInterpolationValue =
      SuperellipseToInterpolableValue(Superellipse::Squircle());

  if (value <= kNotchInterpolationValue) {
    return Superellipse::Notch();
  }
  if (value >= kSquareInterpolationValue) {
    return Superellipse::Square();
  }
  if (std::abs(value - kBevelInterpolationValue) < kEpsilon) {
    return Superellipse::Bevel();
  }
  if (std::abs(value - kRoundInterpolationValue) < kEpsilon) {
    return Superellipse::Round();
  }
  if (std::abs(value - kScoopInterpolationValue) < kEpsilon) {
    return Superellipse::Scoop();
  }
  if (std::abs(value - kSquircleInterpolationValue) < kEpsilon) {
    return Superellipse::Squircle();
  }

  const bool is_convex = value > 0.5;
  const double log_half = std::log(0.5);
  const double convex_half_corner = is_convex ? value : 1 - value;
  // This is equivalent to log(base 1/2) of log(base 1/2) of convex_half_corner.
  const double param =
      std::log(std::log(convex_half_corner) / log_half) / log_half;
  return Superellipse(is_convex ? param : -param);
}

Superellipse ExtractSuperellipseValueFromStyle(const CSSProperty& property,
                                               const ComputedStyle& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kCornerTopLeftShape:
      return style.CornerTopLeftShape();
    case CSSPropertyID::kCornerTopRightShape:
      return style.CornerTopRightShape();
    case CSSPropertyID::kCornerBottomRightShape:
      return style.CornerBottomRightShape();
    case CSSPropertyID::kCornerBottomLeftShape:
      return style.CornerBottomLeftShape();
    default:
      NOTREACHED();
  }
}

}  // namespace

void CSSSuperellipseInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  Superellipse value = SuperellipseFromInterpolableValue(interpolable_value);
  switch (CssProperty().PropertyID()) {
    case CSSPropertyID::kCornerTopLeftShape:
      state.StyleBuilder().SetCornerTopLeftShape(value);
      break;
    case CSSPropertyID::kCornerTopRightShape:
      state.StyleBuilder().SetCornerTopRightShape(value);
      break;
    case CSSPropertyID::kCornerBottomRightShape:
      state.StyleBuilder().SetCornerBottomRightShape(value);
      break;
    case CSSPropertyID::kCornerBottomLeftShape:
      state.StyleBuilder().SetCornerBottomLeftShape(value);
      break;
    default:
      NOTREACHED();
  }
}

class InheritedSuperellipseChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedSuperellipseChecker(const CSSProperty& property,
                               std::optional<Superellipse> value)
      : property_(property), value_(value) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return value_ ==
           ExtractSuperellipseValueFromStyle(property_, *state.ParentStyle());
  }

  const CSSProperty& property_;
  const std::optional<Superellipse> value_;
};

const CSSValue* CSSSuperellipseInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  return ComputedStyleUtils::ValueForCornerShape(
      SuperellipseFromInterpolableValue(value));
}

InterpolationValue CSSSuperellipseInterpolationType::CreateNumberValue(
    Superellipse superellipse) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(
      SuperellipseToInterpolableValue(superellipse)));
}

InterpolationValue CSSSuperellipseInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(0));
}

InterpolationValue CSSSuperellipseInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  std::optional<Superellipse> initial_value = ExtractSuperellipseValueFromStyle(
      CssProperty(), state.GetDocument().GetStyleResolver().InitialStyle());
  if (!initial_value) {
    return nullptr;
  }
  return CreateNumberValue(*initial_value);
}

InterpolationValue CSSSuperellipseInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return nullptr;
  }

  Superellipse inherited =
      ExtractSuperellipseValueFromStyle(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedSuperellipseChecker>(CssProperty(),
                                                         inherited));
  return CreateNumberValue(inherited);
}

InterpolationValue CSSSuperellipseInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers&) const {
  return CreateNumberValue(
      StyleBuilderConverter::ConvertCornerShape(state, value));
}

InterpolationValue
CSSSuperellipseInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateNumberValue(
      ExtractSuperellipseValueFromStyle(CssProperty(), style));
}
}  // namespace blink
