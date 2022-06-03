// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_percentage_interpolation_type.h"

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"

namespace blink {

InterpolationValue CSSPercentageInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(std::make_unique<InterpolableNumber>(0));
}

InterpolationValue CSSPercentageInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value || !primitive_value->IsPercentage())
    return nullptr;
  return InterpolationValue(
      std::make_unique<InterpolableNumber>(primitive_value->GetDoubleValue()));
}

const CSSValue* CSSPercentageInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  return CSSNumericLiteralValue::Create(
      To<InterpolableNumber>(value).Value(),
      CSSPrimitiveValue::UnitType::kPercentage);
}

}  // namespace blink
