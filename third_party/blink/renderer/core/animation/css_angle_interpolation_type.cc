// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_angle_interpolation_type.h"

#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"

namespace blink {

InterpolationValue CSSAngleInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(
      0, CSSPrimitiveValue::UnitType::kDegrees));
}

InterpolationValue CSSAngleInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value || !primitive_value->IsAngle()) {
    return nullptr;
  }
  if (auto* numeric_value =
          DynamicTo<CSSNumericLiteralValue>(primitive_value)) {
    return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(
        numeric_value->ComputeDegrees(),
        CSSPrimitiveValue::UnitType::kDegrees));
  }
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(
      *To<CSSMathFunctionValue>(primitive_value)->ExpressionNode()));
}

const CSSValue* CSSAngleInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState& state) const {
  return CSSNumericLiteralValue::Create(
      To<InterpolableNumber>(value).Value(state.CssToLengthConversionData()),
      CSSPrimitiveValue::UnitType::kDegrees);
}

}  // namespace blink
