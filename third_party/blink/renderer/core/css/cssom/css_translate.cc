// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_translate.h"

#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

bool IsValidTranslateXY(const CSSNumericValue* value) {
  return value && value->Type().MatchesBaseTypePercentage(
                      CSSNumericValueType::BaseType::kLength);
}

bool IsValidTranslateZ(const CSSNumericValue* value) {
  return value &&
         value->Type().MatchesBaseType(CSSNumericValueType::BaseType::kLength);
}

CSSTranslate* FromCSSTranslate(const CSSFunctionValue& value) {
  DCHECK_GT(value.length(), 0UL);

  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(0)));

  if (value.length() == 1) {
    return CSSTranslate::Create(
        x, CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels));
  }

  DCHECK_EQ(value.length(), 2UL);

  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(1)));

  return CSSTranslate::Create(x, y);
}

CSSTranslate* FromCSSTranslateXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);

  CSSNumericValue* length =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(0)));

  switch (value.FunctionType()) {
    case CSSValueID::kTranslateX:
      return CSSTranslate::Create(
          length,
          CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels));
    case CSSValueID::kTranslateY:
      return CSSTranslate::Create(
          CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels),
          length);
    case CSSValueID::kTranslateZ:
      return CSSTranslate::Create(
          CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels),
          CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels),
          length);
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

CSSTranslate* FromCSSTranslate3D(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 3UL);

  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(0)));
  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(1)));
  CSSNumericValue* z =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(2)));

  return CSSTranslate::Create(x, y, z);
}

}  // namespace

CSSTranslate* CSSTranslate::Create(CSSNumericValue* x,
                                   CSSNumericValue* y,
                                   ExceptionState& exception_state) {
  if (!IsValidTranslateXY(x) || !IsValidTranslateXY(y)) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to X and Y of CSSTranslate");
    return nullptr;
  }
  return MakeGarbageCollected<CSSTranslate>(
      x, y, CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels),
      true /* is2D */);
}

CSSTranslate* CSSTranslate::Create(CSSNumericValue* x,
                                   CSSNumericValue* y,
                                   CSSNumericValue* z,
                                   ExceptionState& exception_state) {
  if (!IsValidTranslateXY(x) || !IsValidTranslateXY(y) ||
      !IsValidTranslateZ(z)) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to X, Y and Z of CSSTranslate");
    return nullptr;
  }
  return MakeGarbageCollected<CSSTranslate>(x, y, z, false /* is2D */);
}

CSSTranslate* CSSTranslate::Create(CSSNumericValue* x, CSSNumericValue* y) {
  return MakeGarbageCollected<CSSTranslate>(
      x, y, CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels),
      true /* is2D */);
}

CSSTranslate* CSSTranslate::Create(CSSNumericValue* x,
                                   CSSNumericValue* y,
                                   CSSNumericValue* z) {
  return MakeGarbageCollected<CSSTranslate>(x, y, z, false /* is2D */);
}

CSSTranslate* CSSTranslate::FromCSSValue(const CSSFunctionValue& value) {
  switch (value.FunctionType()) {
    case CSSValueID::kTranslateX:
    case CSSValueID::kTranslateY:
    case CSSValueID::kTranslateZ:
      return FromCSSTranslateXYZ(value);
    case CSSValueID::kTranslate:
      return FromCSSTranslate(value);
    case CSSValueID::kTranslate3d:
      return FromCSSTranslate3D(value);
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

void CSSTranslate::setX(CSSNumericValue* x, ExceptionState& exception_state) {
  if (!IsValidTranslateXY(x)) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to X of CSSTranslate");
    return;
  }
  x_ = x;
}

void CSSTranslate::setY(CSSNumericValue* y, ExceptionState& exception_state) {
  if (!IsValidTranslateXY(y)) {
    exception_state.ThrowTypeError(
        "Must pass length or percent to Y of CSSTranslate");
    return;
  }
  y_ = y;
}

void CSSTranslate::setZ(CSSNumericValue* z, ExceptionState& exception_state) {
  if (!IsValidTranslateZ(z)) {
    exception_state.ThrowTypeError("Must pass length to Z of CSSTranslate");
    return;
  }
  z_ = z;
}

DOMMatrix* CSSTranslate::toMatrix(ExceptionState& exception_state) const {
  CSSUnitValue* x = x_->to(CSSPrimitiveValue::UnitType::kPixels);
  CSSUnitValue* y = y_->to(CSSPrimitiveValue::UnitType::kPixels);
  CSSUnitValue* z = z_->to(CSSPrimitiveValue::UnitType::kPixels);

  if (!x || !y || !z) {
    exception_state.ThrowTypeError(
        "Cannot create matrix if units are not compatible with px");
    return nullptr;
  }

  DOMMatrix* matrix = DOMMatrix::Create();
  if (is2D()) {
    matrix->translateSelf(x->value(), y->value());
  } else {
    matrix->translateSelf(x->value(), y->value(), z->value());
  }

  return matrix;
}

const CSSFunctionValue* CSSTranslate::ToCSSValue() const {
  const CSSValue* x = x_->ToCSSValue();
  const CSSValue* y = y_->ToCSSValue();

  CSSFunctionValue* result = MakeGarbageCollected<CSSFunctionValue>(
      is2D() ? CSSValueID::kTranslate : CSSValueID::kTranslate3d);
  result->Append(*x);
  result->Append(*y);
  if (!is2D()) {
    const CSSValue* z = z_->ToCSSValue();
    result->Append(*z);
  }
  return result;
}

CSSTranslate::CSSTranslate(CSSNumericValue* x,
                           CSSNumericValue* y,
                           CSSNumericValue* z,
                           bool is2D)
    : CSSTransformComponent(is2D), x_(x), y_(y), z_(z) {
  DCHECK(IsValidTranslateXY(x));
  DCHECK(IsValidTranslateXY(y));
  DCHECK(IsValidTranslateZ(z));
}

}  // namespace blink
