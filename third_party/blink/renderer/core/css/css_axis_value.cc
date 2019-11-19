// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_axis_value.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
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
      NOTREACHED();
  }
  Append(
      *CSSNumericLiteralValue::Create(x, CSSPrimitiveValue::UnitType::kNumber));
  Append(
      *CSSNumericLiteralValue::Create(y, CSSPrimitiveValue::UnitType::kNumber));
  Append(
      *CSSNumericLiteralValue::Create(z, CSSPrimitiveValue::UnitType::kNumber));
}

CSSAxisValue::CSSAxisValue(double x, double y, double z)
    : CSSValueList(kAxisClass, kSpaceSeparator),
      axis_name_(CSSValueID::kInvalid) {
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
  Append(
      *CSSNumericLiteralValue::Create(x, CSSPrimitiveValue::UnitType::kNumber));
  Append(
      *CSSNumericLiteralValue::Create(y, CSSPrimitiveValue::UnitType::kNumber));
  Append(
      *CSSNumericLiteralValue::Create(z, CSSPrimitiveValue::UnitType::kNumber));
}

String CSSAxisValue::CustomCSSText() const {
  StringBuilder result;
  if (IsValidCSSValueID(axis_name_)) {
    result.Append(AtomicString(getValueName(axis_name_)));
  } else {
    result.Append(CSSValueList::CustomCSSText());
  }
  return result.ToString();
}

double CSSAxisValue::X() const {
  return To<CSSPrimitiveValue>(Item(0)).GetDoubleValue();
}

double CSSAxisValue::Y() const {
  return To<CSSPrimitiveValue>(Item(1)).GetDoubleValue();
}

double CSSAxisValue::Z() const {
  return To<CSSPrimitiveValue>(Item(2)).GetDoubleValue();
}

}  // namespace cssvalue
}  // namespace blink
