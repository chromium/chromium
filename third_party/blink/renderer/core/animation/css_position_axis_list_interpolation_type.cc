// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_position_axis_list_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"

namespace blink {

InterpolationValue
CSSPositionAxisListInterpolationType::ConvertPositionAxisCSSValue(
    const CSSValue& value) {
  if (const auto* pair = DynamicTo<CSSValuePair>(value)) {
    InterpolationValue result(
        InterpolableLength::MaybeConvertCSSValue(pair->Second()));
    CSSValueID side = To<CSSIdentifierValue>(pair->First()).GetValueID();
    if (side == CSSValueID::kRight || side == CSSValueID::kBottom) {
      To<InterpolableLength>(*result.interpolable_value)
          .SubtractFromOneHundredPercent();
    }
    return result;
  }

  if (value.IsPrimitiveValue())
    return InterpolationValue(InterpolableLength::MaybeConvertCSSValue(value));

  const auto* ident = DynamicTo<CSSIdentifierValue>(value);
  if (!ident)
    return nullptr;

  switch (ident->GetValueID()) {
    case CSSValueID::kLeft:
    case CSSValueID::kTop:
      return InterpolationValue(InterpolableLength::CreatePercent(0));
    case CSSValueID::kRight:
    case CSSValueID::kBottom:
      return InterpolationValue(InterpolableLength::CreatePercent(100));
    case CSSValueID::kCenter:
      return InterpolationValue(InterpolableLength::CreatePercent(50));
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

InterpolationValue CSSPositionAxisListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsBaseValueList()) {
    return ListInterpolationFunctions::CreateList(
        1, [&value](size_t) { return ConvertPositionAxisCSSValue(value); });
  }

  const auto& list = To<CSSValueList>(value);
  return ListInterpolationFunctions::CreateList(
      list.length(), [&list](wtf_size_t index) {
        return ConvertPositionAxisCSSValue(list.Item(index));
      });
}

}  // namespace blink
