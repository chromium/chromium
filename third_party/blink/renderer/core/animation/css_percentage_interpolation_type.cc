// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_percentage_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/length_units_checker.h"
#include "third_party/blink/renderer/core/animation/number_property_functions.h"
#include "third_party/blink/renderer/core/animation/tree_counting_checker.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"

namespace blink {

class InheritedPercentageChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedPercentageChecker(const CSSProperty& property,
                             std::optional<double> percent)
      : property_(property), percent_(percent) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    std::optional<double> parent_percent =
        NumberPropertyFunctions::GetPercentage(property_, *state.ParentStyle());
    return percent_ == parent_percent;
  }

  const CSSProperty& property_;
  const std::optional<double> percent_;
};

const CSSValue* CSSPercentageInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  double percentage = To<InterpolableNumber>(value).Value();
  return CSSNumericLiteralValue::Create(
      percentage, CSSPrimitiveValue::UnitType::kPercentage);
}

InterpolationValue CSSPercentageInterpolationType::CreatePercentageValue(
    double percentage) const {
  return InterpolationValue(
      MakeGarbageCollected<InterpolableNumber>(percentage));
}

InterpolationValue CSSPercentageInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return CreatePercentageValue(0);
}

InterpolationValue CSSPercentageInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  std::optional<double> initial_percentage =
      NumberPropertyFunctions::GetInitialPercentage(
          CssProperty(), state.GetDocument().GetStyleResolver().InitialStyle());
  if (!initial_percentage.has_value()) {
    return nullptr;
  }
  return CreatePercentageValue(*initial_percentage);
}

InterpolationValue CSSPercentageInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return nullptr;
  }
  std::optional<double> inherited = NumberPropertyFunctions::GetPercentage(
      CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedPercentageChecker>(CssProperty(),
                                                       inherited));
  if (!inherited.has_value()) {
    return nullptr;
  }
  return CreatePercentageValue(*inherited);
}

InterpolationValue CSSPercentageInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value || !primitive_value->IsPercentage()) {
    return nullptr;
  }
  const CSSLengthResolver& length_resolver = state.CssToLengthConversionData();
  if (primitive_value->IsElementDependent()) {
    conversion_checkers.push_back(TreeCountingChecker::Create(length_resolver));
  }
  CSSPrimitiveValue::LengthTypeFlags types;
  primitive_value->AccumulateLengthUnitTypes(types);
  if (InterpolationType::ConversionChecker* length_units_checker =
          LengthUnitsChecker::MaybeCreate(types, state)) {
    conversion_checkers.push_back(length_units_checker);
  }
  return CreatePercentageValue(
      primitive_value->ComputePercentage(length_resolver));
}

InterpolationValue
CSSPercentageInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  std::optional<double> underlying_percentage =
      NumberPropertyFunctions::GetPercentage(CssProperty(), style);
  if (!underlying_percentage.has_value()) {
    return nullptr;
  }
  return CreatePercentageValue(*underlying_percentage);
}

InterpolationValue
CSSPercentageInterpolationType::MaybeConvertCustomPropertyUnderlyingValue(
    const CSSValue& value) const {
  if (const auto* percentage_value = DynamicTo<CSSNumericLiteralValue>(value)) {
    if (percentage_value->IsPercentage()) {
      return CreatePercentageValue(percentage_value->ClampedDoubleValue());
    }
  }
  return nullptr;
}

void CSSPercentageInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  double clamped_percentage = NumberPropertyFunctions::ClampPercentage(
      CssProperty(), To<InterpolableNumber>(interpolable_value).Value());
  if (!NumberPropertyFunctions::SetPercentage(
          CssProperty(), state.StyleBuilder(), clamped_percentage)) {
    StyleBuilder::ApplyProperty(
        GetProperty().GetCSSProperty(), state,
        *CSSNumericLiteralValue::Create(
            clamped_percentage, CSSPrimitiveValue::UnitType::kPercentage));
  }
}

}  // namespace blink
