// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_scale.h"

#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"

namespace blink {

namespace {

bool IsValidScaleCoord(CSSNumericValue* coord) {
  return coord && coord->Type().MatchesNumber();
}

CSSScale* FromScale(const CSSFunctionValue& value) {
  DCHECK(value.length() == 1U || value.length() == 2U);
  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(0)));
  if (value.length() == 1U) {
    return CSSScale::Create(x, x);
  }

  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(1)));
  return CSSScale::Create(x, y);
}

CSSScale* FromScaleXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1U);

  CSSNumericValue* numeric_value =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(0)));
  CSSUnitValue* default_value = CSSUnitValue::Create(1);
  switch (value.FunctionType()) {
    case CSSValueID::kScaleX:
      return CSSScale::Create(numeric_value, default_value);
    case CSSValueID::kScaleY:
      return CSSScale::Create(default_value, numeric_value);
    case CSSValueID::kScaleZ:
      return CSSScale::Create(default_value, default_value, numeric_value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSScale* FromScale3d(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 3U);

  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(0)));
  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(1)));
  CSSNumericValue* z =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(2)));

  return CSSScale::Create(x, y, z);
}

}  // namespace

CSSScale* CSSScale::Create(const CSSNumberish& x,
                           const CSSNumberish& y,
                           ExceptionState& exception_state) {
  CSSNumericValue* x_value = CSSNumericValue::FromNumberish(x);
  CSSNumericValue* y_value = CSSNumericValue::FromNumberish(y);

  if (!IsValidScaleCoord(x_value) || !IsValidScaleCoord(y_value)) {
    exception_state.ThrowTypeError("Must specify an number unit");
    return nullptr;
  }

  return CSSScale::Create(x_value, y_value);
}

CSSScale* CSSScale::Create(const CSSNumberish& x,
                           const CSSNumberish& y,
                           const CSSNumberish& z,
                           ExceptionState& exception_state) {
  CSSNumericValue* x_value = CSSNumericValue::FromNumberish(x);
  CSSNumericValue* y_value = CSSNumericValue::FromNumberish(y);
  CSSNumericValue* z_value = CSSNumericValue::FromNumberish(z);

  if (!IsValidScaleCoord(x_value) || !IsValidScaleCoord(y_value) ||
      !IsValidScaleCoord(z_value)) {
    exception_state.ThrowTypeError("Must specify a number for X, Y and Z");
    return nullptr;
  }

  return CSSScale::Create(x_value, y_value, z_value);
}

CSSScale* CSSScale::FromCSSValue(const CSSFunctionValue& value) {
  switch (value.FunctionType()) {
    case CSSValueID::kScale:
      return FromScale(value);
    case CSSValueID::kScaleX:
    case CSSValueID::kScaleY:
    case CSSValueID::kScaleZ:
      return FromScaleXYZ(value);
    case CSSValueID::kScale3d:
      return FromScale3d(value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

void CSSScale::setX(const CSSNumberish& x, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(x);

  if (!IsValidScaleCoord(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }

  x_ = value;
}

void CSSScale::setY(const CSSNumberish& y, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(y);

  if (!IsValidScaleCoord(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }

  y_ = value;
}

void CSSScale::setZ(const CSSNumberish& z, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(z);

  if (!IsValidScaleCoord(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }

  z_ = value;
}

DOMMatrix* CSSScale::toMatrix(ExceptionState& exception_state) const {
  CSSUnitValue* x = x_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* y = y_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* z = z_->to(CSSPrimitiveValue::UnitType::kNumber);

  if (!x || !y || !z) {
    exception_state.ThrowTypeError(
        "Cannot create matrix if values are not numbers");
    return nullptr;
  }

  DOMMatrix* matrix = DOMMatrix::Create();
  if (is2D())
    matrix->scaleSelf(x->value(), y->value());
  else
    matrix->scaleSelf(x->value(), y->value(), z->value());

  return matrix;
}

const CSSFunctionValue* CSSScale::ToCSSValue() const {
  const CSSValue* x = x_->ToCSSValue();
  const CSSValue* y = y_->ToCSSValue();
  if (!x || !y)
    return nullptr;

  CSSFunctionValue* result = MakeGarbageCollected<CSSFunctionValue>(
      is2D() ? CSSValueID::kScale : CSSValueID::kScale3d);
  result->Append(*x);
  result->Append(*y);
  if (!is2D()) {
    const CSSValue* z = z_->ToCSSValue();
    if (!z)
      return nullptr;
    result->Append(*z);
  }
  return result;
}

CSSScale::CSSScale(CSSNumericValue* x,
                   CSSNumericValue* y,
                   CSSNumericValue* z,
                   bool is2D)
    : CSSTransformComponent(is2D), x_(x), y_(y), z_(z) {
  DCHECK(IsValidScaleCoord(x));
  DCHECK(IsValidScaleCoord(y));
  DCHECK(IsValidScaleCoord(z));
}

}  // namespace blink
