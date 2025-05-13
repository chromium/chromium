// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_resolution_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/tree_counting_checker.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"

namespace blink {

InterpolationValue CSSResolutionInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(0));
}

InterpolationValue CSSResolutionInterpolationType::MaybeConvertResolution(
    const CSSValue& value,
    const CSSToLengthConversionData& conversion_data) const {
  const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value || !primitive_value->IsResolution()) {
    return nullptr;
  }
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(
      primitive_value->ComputeDotsPerPixel(conversion_data)));
}

InterpolationValue CSSResolutionInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const CSSToLengthConversionData& conversion_data =
      state.CssToLengthConversionData();
  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    if (primitive_value->IsElementDependent()) {
      conversion_checkers.push_back(
          TreeCountingChecker::Create(conversion_data));
    }
  }
  return MaybeConvertResolution(value, conversion_data);
}

const CSSValue* CSSResolutionInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  return CSSNumericLiteralValue::Create(
      To<InterpolableNumber>(value).Value(),
      CSSPrimitiveValue::UnitType::kDotsPerPixel);
}

InterpolationValue
CSSResolutionInterpolationType::MaybeConvertCustomPropertyUnderlyingValue(
    const CSSValue& value) const {
  return MaybeConvertResolution(value,
                                CSSToLengthConversionData(/*element=*/nullptr));
}

}  // namespace blink
