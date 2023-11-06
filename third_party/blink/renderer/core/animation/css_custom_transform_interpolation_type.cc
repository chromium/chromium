// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_custom_transform_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolable_transform_list.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"

namespace blink {

InterpolationValue CSSCustomTransformInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers&) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableTransformList>(
      EmptyTransformOperations(),
      TransformOperations::BoxSizeDependentMatrixBlending::kDisallow));
}

InterpolationValue CSSCustomTransformInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  auto* list_value = DynamicTo<CSSValueList>(value);
  if (!list_value) {
    return nullptr;
  }
  // An empty list value does not represent a <transform-list> as it contains at
  // least one transform function. This is also assuming that no other syntaxes
  // represent values as empty CSSValueLists, which in itself would be
  // problematic.
  CHECK_GT(list_value->length(), 0u);
  const auto* first_function = DynamicTo<CSSFunctionValue>(list_value->First());
  if (!first_function || !IsTransformFunction(first_function->FunctionType())) {
    return nullptr;
  }

  return InterpolationValue(InterpolableTransformList::ConvertCSSValue(
      value, CSSToLengthConversionData(),
      TransformOperations::BoxSizeDependentMatrixBlending::kDisallow));
}

const CSSValue* CSSCustomTransformInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  auto* list_value = DynamicTo<InterpolableTransformList>(value);
  if (!list_value) {
    return nullptr;
  }
  return ComputedStyleUtils::ValueForTransformList(list_value->operations(), 1);
}

}  // namespace blink
