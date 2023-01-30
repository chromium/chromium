// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_time_interpolation_type.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"

namespace blink {

InterpolationValue CSSTimeInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return CreateTimeValue(0);
}

InterpolationValue CSSTimeInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value || !primitive_value->IsTime())
    return nullptr;
  return CreateTimeValue(primitive_value->ComputeSeconds());
}

const CSSValue* CSSTimeInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  return CSSNumericLiteralValue::Create(To<InterpolableNumber>(value).Value(),
                                        CSSPrimitiveValue::UnitType::kSeconds);
}

InterpolationValue CSSTimeInterpolationType::CreateTimeValue(
    double seconds) const {
  return InterpolationValue(std::make_unique<InterpolableNumber>(seconds));
}

// static
absl::optional<double> CSSTimeInterpolationType::GetSeconds(
    const CSSPropertyID& property,
    const ComputedStyle& style) {
  switch (property) {
    // No properties currently use CSSTimeInterpolationType.
    default:
      NOTREACHED();
      return absl::optional<double>();
  }
}

absl::optional<double> CSSTimeInterpolationType::GetSeconds(
    const ComputedStyle& style) const {
  return GetSeconds(CssProperty().PropertyID(), style);
}

// This function considers both (a) whether the property allows negative values
// and (b) whether it's stored as double or float.
// TODO: These functions, if they get larger, should probably move into a
// dedicated time_property_functions.h, similar to number_property_functions.
double CSSTimeInterpolationType::ClampTime(const CSSPropertyID& property,
                                           double value) const {
  switch (property) {
    // No properties currently use CSSTimeInterpolationType.
    default:
      NOTREACHED();
      return 0;
  }
}

InterpolationValue
CSSTimeInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  if (auto underlying_seconds = GetSeconds(style))
    return CreateTimeValue(*underlying_seconds);
  return nullptr;
}

void CSSTimeInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  auto property = CssProperty().PropertyID();
  switch (property) {
    // No properties currently use CSSTimeInterpolationType.
    default:
      NOTREACHED();
      break;
  }
}

InterpolationValue CSSTimeInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (auto seconds =
          GetSeconds(state.GetDocument().GetStyleResolver().InitialStyle())) {
    return CreateTimeValue(*seconds);
  }
  return nullptr;
}

class InheritedTimeChecker : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedTimeChecker(const CSSProperty& property,
                       absl::optional<double> seconds)
      : property_(property), seconds_(seconds) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    absl::optional<double> parent_seconds =
        CSSTimeInterpolationType::GetSeconds(property_.PropertyID(),
                                             *state.ParentStyle());
    return seconds_ == parent_seconds;
  }
  const CSSProperty& property_;
  const absl::optional<double> seconds_;
};

InterpolationValue CSSTimeInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  absl::optional<double> inherited_seconds = GetSeconds(*state.ParentStyle());
  conversion_checkers.push_back(
      std::make_unique<InheritedTimeChecker>(CssProperty(), inherited_seconds));
  if (!inherited_seconds)
    return nullptr;
  return CreateTimeValue(*inherited_seconds);
}

}  // namespace blink
