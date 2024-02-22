// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/number_property_functions.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"

namespace blink {

class InheritedNumberChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedNumberChecker(const CSSProperty& property,
                         std::optional<double> number)
      : property_(property), number_(number) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    std::optional<double> parent_number =
        NumberPropertyFunctions::GetNumber(property_, *state.ParentStyle());
    return number_ == parent_number;
  }

  const CSSProperty& property_;
  const std::optional<double> number_;
};

const CSSValue* CSSNumberInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  double number = To<InterpolableNumber>(value).Value();
  return CSSNumericLiteralValue::Create(
      round_to_integer_ ? round(number) : number, UnitType());
}

InterpolationValue CSSNumberInterpolationType::CreateNumberValue(
    double number) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(number));
}

InterpolationValue CSSNumberInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return CreateNumberValue(0);
}

InterpolationValue CSSNumberInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  std::optional<double> initial_number =
      NumberPropertyFunctions::GetInitialNumber(
          CssProperty(), state.GetDocument().GetStyleResolver().InitialStyle());
  if (!initial_number)
    return nullptr;
  return CreateNumberValue(*initial_number);
}

InterpolationValue CSSNumberInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  std::optional<double> inherited =
      NumberPropertyFunctions::GetNumber(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedNumberChecker>(CssProperty(), inherited));
  if (!inherited)
    return nullptr;
  return CreateNumberValue(*inherited);
}

InterpolationValue CSSNumberInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value ||
      !(primitive_value->IsNumber() || primitive_value->IsPercentage()))
    return nullptr;
  return CreateNumberValue(primitive_value->GetDoubleValue());
}

InterpolationValue
CSSNumberInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  std::optional<double> underlying_number =
      NumberPropertyFunctions::GetNumber(CssProperty(), style);
  if (!underlying_number)
    return nullptr;
  return CreateNumberValue(*underlying_number);
}

void CSSNumberInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  double clamped_number = NumberPropertyFunctions::ClampNumber(
      CssProperty(), To<InterpolableNumber>(interpolable_value).Value());
  if (!NumberPropertyFunctions::SetNumber(CssProperty(), state.StyleBuilder(),
                                          clamped_number)) {
    StyleBuilder::ApplyProperty(
        GetProperty().GetCSSProperty(), state,
        *CSSNumericLiteralValue::Create(clamped_number, UnitType()));
  }
}

}  // namespace blink
