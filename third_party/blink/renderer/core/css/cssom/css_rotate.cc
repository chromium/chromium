// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_rotate.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

bool IsValidRotateCoord(const CSSNumericValue* value) {
  return value && value->Type().MatchesNumber();
}

bool IsValidRotateAngle(const CSSNumericValue* value) {
  return value &&
         value->Type().MatchesBaseType(CSSNumericValueType::BaseType::kAngle);
}

CSSRotate* FromCSSRotate(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);
  CSSNumericValue* angle =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(0)));
  return CSSRotate::Create(angle);
}

CSSRotate* FromCSSRotate3d(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 4UL);

  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(0)));
  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(1)));
  CSSNumericValue* z =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(2)));
  CSSNumericValue* angle =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(3)));

  return CSSRotate::Create(x, y, z, angle);
}

CSSRotate* FromCSSRotateXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);

  CSSNumericValue* angle =
      CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(0)));

  switch (value.FunctionType()) {
    case CSSValueID::kRotateX:
      return CSSRotate::Create(CSSUnitValue::Create(1), CSSUnitValue::Create(0),
                               CSSUnitValue::Create(0), angle);
    case CSSValueID::kRotateY:
      return CSSRotate::Create(CSSUnitValue::Create(0), CSSUnitValue::Create(1),
                               CSSUnitValue::Create(0), angle);
    case CSSValueID::kRotateZ:
      return CSSRotate::Create(CSSUnitValue::Create(0), CSSUnitValue::Create(0),
                               CSSUnitValue::Create(1), angle);
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

}  // namespace

CSSRotate* CSSRotate::Create(CSSNumericValue* angle,
                             ExceptionState& exception_state) {
  if (!IsValidRotateAngle(angle)) {
    exception_state.ThrowTypeError("Must pass an angle to CSSRotate");
    return nullptr;
  }
  return MakeGarbageCollected<CSSRotate>(
      CSSUnitValue::Create(0), CSSUnitValue::Create(0), CSSUnitValue::Create(1),
      angle, true /* is2D */);
}

CSSRotate* CSSRotate::Create(const V8CSSNumberish* x,
                             const V8CSSNumberish* y,
                             const V8CSSNumberish* z,
                             CSSNumericValue* angle,
                             ExceptionState& exception_state) {
  CSSNumericValue* x_value = CSSNumericValue::FromNumberish(x);
  CSSNumericValue* y_value = CSSNumericValue::FromNumberish(y);
  CSSNumericValue* z_value = CSSNumericValue::FromNumberish(z);

  if (!IsValidRotateCoord(x_value) || !IsValidRotateCoord(y_value) ||
      !IsValidRotateCoord(z_value)) {
    exception_state.ThrowTypeError("Must specify an number unit");
    return nullptr;
  }
  if (!IsValidRotateAngle(angle)) {
    exception_state.ThrowTypeError("Must pass an angle to CSSRotate");
    return nullptr;
  }
  return MakeGarbageCollected<CSSRotate>(x_value, y_value, z_value, angle,
                                         false /* is2D */);
}

CSSRotate* CSSRotate::Create(CSSNumericValue* angle) {
  return MakeGarbageCollected<CSSRotate>(
      CSSUnitValue::Create(0), CSSUnitValue::Create(0), CSSUnitValue::Create(1),
      angle, true /* is2D */);
}

CSSRotate* CSSRotate::Create(CSSNumericValue* x,
                             CSSNumericValue* y,
                             CSSNumericValue* z,
                             CSSNumericValue* angle) {
  return MakeGarbageCollected<CSSRotate>(x, y, z, angle, false /* is2D */);
}

CSSRotate* CSSRotate::FromCSSValue(const CSSFunctionValue& value) {
  switch (value.FunctionType()) {
    case CSSValueID::kRotate:
      return FromCSSRotate(value);
    case CSSValueID::kRotate3d:
      return FromCSSRotate3d(value);
    case CSSValueID::kRotateX:
    case CSSValueID::kRotateY:
    case CSSValueID::kRotateZ:
      return FromCSSRotateXYZ(value);
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

void CSSRotate::setAngle(CSSNumericValue* angle,
                         ExceptionState& exception_state) {
  if (!IsValidRotateAngle(angle)) {
    exception_state.ThrowTypeError("Must pass an angle to CSSRotate");
    return;
  }
  angle_ = angle;
}

DOMMatrix* CSSRotate::toMatrix(ExceptionState& exception_state) const {
  CSSUnitValue* x = x_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* y = y_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* z = z_->to(CSSPrimitiveValue::UnitType::kNumber);
  if (!x || !y || !z) {
    exception_state.ThrowTypeError(
        "Cannot create matrix if units cannot be converted to CSSUnitValue");
    return nullptr;
  }

  DOMMatrix* matrix = DOMMatrix::Create();
  CSSUnitValue* angle = angle_->to(CSSPrimitiveValue::UnitType::kDegrees);
  if (is2D()) {
    matrix->rotateAxisAngleSelf(0, 0, 1, angle->value());
  } else {
    matrix->rotateAxisAngleSelf(x->value(), y->value(), z->value(),
                                angle->value());
  }
  return matrix;
}

const CSSFunctionValue* CSSRotate::ToCSSValue() const {
  CSSFunctionValue* result = MakeGarbageCollected<CSSFunctionValue>(
      is2D() ? CSSValueID::kRotate : CSSValueID::kRotate3d);
  if (!is2D()) {
    const CSSValue* x = x_->ToCSSValue();
    const CSSValue* y = y_->ToCSSValue();
    const CSSValue* z = z_->ToCSSValue();
    if (!x || !y || !z) {
      return nullptr;
    }

    result->Append(*x);
    result->Append(*y);
    result->Append(*z);
  }

  const CSSValue* angle = angle_->ToCSSValue();
  if (!angle) {
    return nullptr;
  }

  DCHECK(x_->to(CSSPrimitiveValue::UnitType::kNumber));
  DCHECK(y_->to(CSSPrimitiveValue::UnitType::kNumber));
  DCHECK(z_->to(CSSPrimitiveValue::UnitType::kNumber));
  DCHECK(angle_->to(CSSPrimitiveValue::UnitType::kRadians));

  result->Append(*angle);
  return result;
}

V8CSSNumberish* CSSRotate::x() {
  return MakeGarbageCollected<V8CSSNumberish>(x_);
}

V8CSSNumberish* CSSRotate::y() {
  return MakeGarbageCollected<V8CSSNumberish>(y_);
}

V8CSSNumberish* CSSRotate::z() {
  return MakeGarbageCollected<V8CSSNumberish>(z_);
}

void CSSRotate::setX(const V8CSSNumberish* x, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(x);
  if (!IsValidRotateCoord(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }
  x_ = value;
}

void CSSRotate::setY(const V8CSSNumberish* y, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(y);
  if (!IsValidRotateCoord(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }
  y_ = value;
}

void CSSRotate::setZ(const V8CSSNumberish* z, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(z);
  if (!IsValidRotateCoord(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }
  z_ = value;
}

CSSRotate::CSSRotate(CSSNumericValue* x,
                     CSSNumericValue* y,
                     CSSNumericValue* z,
                     CSSNumericValue* angle,
                     bool is2D)
    : CSSTransformComponent(is2D), angle_(angle), x_(x), y_(y), z_(z) {
  DCHECK(IsValidRotateCoord(x));
  DCHECK(IsValidRotateCoord(y));
  DCHECK(IsValidRotateCoord(z));
  DCHECK(IsValidRotateAngle(angle));
}

}  // namespace blink
