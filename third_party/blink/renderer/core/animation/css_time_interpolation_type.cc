// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_time_interpolation_type.h"

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"

namespace blink {

InterpolationValue CSSTimeInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(std::make_unique<InterpolableNumber>(0));
}

InterpolationValue CSSTimeInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value || !primitive_value->IsTime())
    return nullptr;
  return InterpolationValue(
      std::make_unique<InterpolableNumber>(primitive_value->ComputeSeconds()));
}

const CSSValue* CSSTimeInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  return CSSNumericLiteralValue::Create(To<InterpolableNumber>(value).Value(),
                                        CSSPrimitiveValue::UnitType::kSeconds);
}

}  // namespace blink
