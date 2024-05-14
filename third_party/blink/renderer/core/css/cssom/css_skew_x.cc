// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_skew_x.h"

#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

bool IsValidSkewXAngle(CSSNumericValue* value) {
  return value &&
         value->Type().MatchesBaseType(CSSNumericValueType::BaseType::kAngle);
}

}  // namespace

CSSSkewX* CSSSkewX::Create(CSSNumericValue* ax,
                           ExceptionState& exception_state) {
  if (!IsValidSkewXAngle(ax)) {
    exception_state.ThrowTypeError("CSSSkewX does not support non-angles");
    return nullptr;
  }
  return MakeGarbageCollected<CSSSkewX>(ax);
}

void CSSSkewX::setAx(CSSNumericValue* value, ExceptionState& exception_state) {
  if (!IsValidSkewXAngle(value)) {
    exception_state.ThrowTypeError("Must specify an angle unit");
    return;
  }
  ax_ = value;
}

CSSSkewX* CSSSkewX::FromCSSValue(const CSSFunctionValue& value) {
  DCHECK_GT(value.length(), 0U);
  DCHECK_EQ(value.FunctionType(), CSSValueID::kSkewX);
  if (value.length() == 1U) {
    return CSSSkewX::Create(
        CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(value.Item(0))));
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

DOMMatrix* CSSSkewX::toMatrix(ExceptionState&) const {
  CSSUnitValue* ax = ax_->to(CSSPrimitiveValue::UnitType::kDegrees);
  DCHECK(ax);
  DOMMatrix* result = DOMMatrix::Create();
  result->skewXSelf(ax->value());
  return result;
}

const CSSFunctionValue* CSSSkewX::ToCSSValue() const {
  const CSSValue* ax = ax_->ToCSSValue();
  if (!ax) {
    return nullptr;
  }

  CSSFunctionValue* result =
      MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSkewX);
  result->Append(*ax);
  return result;
}

CSSSkewX::CSSSkewX(CSSNumericValue* ax)
    : CSSTransformComponent(true /* is2D */), ax_(ax) {
  DCHECK(ax);
}

}  // namespace blink
