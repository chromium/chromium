// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_custom_transform_function_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolable_transform_list.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"

namespace blink {

InterpolationValue
CSSCustomTransformFunctionInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers&) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableTransformList>(
      EmptyTransformOperations(),
      TransformOperations::BoxSizeDependentMatrixBlending::kDisallow));
}

InterpolationValue
CSSCustomTransformFunctionInterpolationType::MaybeConvertTransformFunction(
    const CSSValue& value,
    const CSSToLengthConversionData& conversion_data) const {
  const auto* function_value = DynamicTo<CSSFunctionValue>(value);
  if (!function_value || !IsTransformFunction(function_value->FunctionType())) {
    return nullptr;
  }

  InterpolableTransformList* interpolable =
      InterpolableTransformList::ConvertCSSValue(
          value, conversion_data,
          TransformOperations::BoxSizeDependentMatrixBlending::kDisallow);
  CHECK_EQ(interpolable->operations().size(), 1u);
  return InterpolationValue(std::move(interpolable));
}

InterpolationValue
CSSCustomTransformFunctionInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers&) const {
  return MaybeConvertTransformFunction(value,
                                       state.CssToLengthConversionData());
}

const CSSValue* CSSCustomTransformFunctionInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  auto* list_value = DynamicTo<InterpolableTransformList>(value);
  if (!list_value) {
    return nullptr;
  }
  // The list of operations must be exactly 1. Otherwise we will have a CHECK
  // failure inside ValueForTransformFunction().
  return ComputedStyleUtils::ValueForTransformFunction(
      list_value->operations());
}

InterpolationValue CSSCustomTransformFunctionInterpolationType::
    MaybeConvertCustomPropertyUnderlyingValue(const CSSValue& value) const {
  return MaybeConvertTransformFunction(
      value, CSSToLengthConversionData(/*element=*/nullptr));
}

InterpolationValue
CSSCustomTransformFunctionInterpolationType::PreInterpolationCompositeIfNeeded(
    InterpolationValue value,
    const InterpolationValue& underlying,
    EffectModel::CompositeOperation composite,
    ConversionCheckers& conversion_checkers) const {
  if (composite == EffectModel::CompositeOperation::kCompositeAdd) {
    // Transform interpolations will represent kCompositeAdd as separate
    // transform function. For a single <transform-function>, fall back to
    // accumulate to get a valid <tranform-function> value.
    composite = EffectModel::CompositeOperation::kCompositeAccumulate;
  }
  return CSSTransformInterpolationType::PreInterpolationCompositeIfNeeded(
      std::move(value), underlying, composite, conversion_checkers);
}

}  // namespace blink
