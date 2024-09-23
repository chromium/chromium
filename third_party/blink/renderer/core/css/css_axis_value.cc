// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_axis_value.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_length_resolver.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSAxisValue::CSSAxisValue(CSSValueID axis_name)
    : CSSValueList(kAxisClass, kSpaceSeparator), axis_name_(axis_name) {
  double x = 0;
  double y = 0;
  double z = 0;
  switch (axis_name) {
    case CSSValueID::kX:
      x = 1;
      break;

    case CSSValueID::kY:
      y = 1;
      break;

    case CSSValueID::kZ:
      z = 1;
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }
  Append(
      *CSSNumericLiteralValue::Create(x, CSSPrimitiveValue::UnitType::kNumber));
  Append(
      *CSSNumericLiteralValue::Create(y, CSSPrimitiveValue::UnitType::kNumber));
  Append(
      *CSSNumericLiteralValue::Create(z, CSSPrimitiveValue::UnitType::kNumber));
}

CSSAxisValue::CSSAxisValue(const CSSPrimitiveValue* x_value,
                           const CSSPrimitiveValue* y_value,
                           const CSSPrimitiveValue* z_value)
    : CSSValueList(kAxisClass, kSpaceSeparator),
      axis_name_(CSSValueID::kInvalid) {
  if (x_value->IsNumericLiteralValue() && y_value->IsNumericLiteralValue() &&
      z_value->IsNumericLiteralValue()) {
    double x = To<CSSNumericLiteralValue>(x_value)->ComputeNumber();
    double y = To<CSSNumericLiteralValue>(y_value)->ComputeNumber();
    double z = To<CSSNumericLiteralValue>(z_value)->ComputeNumber();
    // Normalize axis that are parallel to x, y or z axis.
    if (x > 0 && y == 0 && z == 0) {
      x = 1;
      axis_name_ = CSSValueID::kX;
    } else if (x == 0 && y > 0 && z == 0) {
      y = 1;
      axis_name_ = CSSValueID::kY;
    } else if (x == 0 && y == 0 && z > 0) {
      z = 1;
      axis_name_ = CSSValueID::kZ;
    }
    Append(*CSSNumericLiteralValue::Create(
        x, CSSPrimitiveValue::UnitType::kNumber));
    Append(*CSSNumericLiteralValue::Create(
        y, CSSPrimitiveValue::UnitType::kNumber));
    Append(*CSSNumericLiteralValue::Create(
        z, CSSPrimitiveValue::UnitType::kNumber));
    return;
  }
  Append(*x_value);
  Append(*y_value);
  Append(*z_value);
}

String CSSAxisValue::CustomCSSText() const {
  StringBuilder result;
  if (IsValidCSSValueID(axis_name_)) {
    result.Append(AtomicString(getValueName(axis_name_)));
  } else {
    result.Append(CSSValueList::CustomCSSText());
  }
  return result.ReleaseString();
}

CSSAxisValue::Axis CSSAxisValue::ComputeAxis(
    const CSSLengthResolver& length_resolver) const {
  double x = To<CSSPrimitiveValue>(Item(0)).ComputeNumber(length_resolver);
  double y = To<CSSPrimitiveValue>(Item(1)).ComputeNumber(length_resolver);
  double z = To<CSSPrimitiveValue>(Item(2)).ComputeNumber(length_resolver);
  // Normalize axis that are parallel to x, y or z axis.
  if (x > 0 && y == 0 && z == 0) {
    x = 1;
  } else if (x == 0 && y > 0 && z == 0) {
    y = 1;
  } else if (x == 0 && y == 0 && z > 0) {
    z = 1;
  }
  return {{x, y, z}};
}

}  // namespace cssvalue
}  // namespace blink
