// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_skew_y.h"

#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

bool IsValidSkewYAngle(CSSNumericValue* value) {
  return value &&
         value->Type().MatchesBaseType(CSSNumericValueType::BaseType::kAngle);
}

}  // namespace

CSSSkewY* CSSSkewY::Create(CSSNumericValue* ay,
                           ExceptionState& exception_state) {
  if (!IsValidSkewYAngle(ay)) {
    exception_state.ThrowTypeError("CSSSkewY does not support non-angles");
    return nullptr;
  }
  return MakeGarbageCollected<CSSSkewY>(ay);
}

void CSSSkewY::setAy(CSSNumericValue* value, ExceptionState& exception_state) {
  if (!IsValidSkewYAngle(value)) {
    exception_state.ThrowTypeError("Must specify an angle unit");
    return;
  }
  ay_ = value;
}

CSSSkewY* CSSSkewY::FromCSSValue(const CSSFunctionValue& value) {
  DCHECK_GT(value.length(), 0U);
  DCHECK_EQ(value.FunctionType(), CSSValueID::kSkewY);
  if (value.length(), 1U) {
    return CSSSkewY::Create(
        CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(0))));
  }
  NOTREACHED();
  return nullptr;
}

DOMMatrix* CSSSkewY::toMatrix(ExceptionState&) const {
  CSSUnitValue* ay = ay_->to(CSSPrimitiveValue::UnitType::kRadians);
  DCHECK(ay);
  DOMMatrix* result = DOMMatrix::Create();
  result->skewYSelf(ay->value());
  return result;
}

const CSSFunctionValue* CSSSkewY::ToCSSValue() const {
  const CSSValue* ay = ay_->ToCSSValue();
  if (!ay)
    return nullptr;

  CSSFunctionValue* result =
      MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSkewY);
  result->Append(*ay);
  return result;
}

CSSSkewY::CSSSkewY(CSSNumericValue* ay)
    : CSSTransformComponent(true /* is2D */), ay_(ay) {
  DCHECK(ay);
}

}  // namespace blink
