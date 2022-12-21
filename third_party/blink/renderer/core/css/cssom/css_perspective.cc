// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_perspective.h"

#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

// Given the union provided, return null if it's invalid, and either the
// original union or a newly-created one if it is valid.
V8CSSPerspectiveValue* HandleInputPerspective(V8CSSPerspectiveValue* value) {
  if (!value) {
    return nullptr;
  }
  switch (value->GetContentType()) {
    case V8CSSPerspectiveValue::ContentType::kCSSNumericValue: {
      if (!value->GetAsCSSNumericValue()->Type().MatchesBaseType(
              CSSNumericValueType::BaseType::kLength)) {
        return nullptr;
      }
      break;
    }
    case V8CSSPerspectiveValue::ContentType::kString: {
      CSSKeywordValue* keyword =
          MakeGarbageCollected<CSSKeywordValue>(value->GetAsString());
      // Replace the parameter |value| with a new object.
      value = MakeGarbageCollected<V8CSSPerspectiveValue>(keyword);
      ABSL_FALLTHROUGH_INTENDED;
    }
    case V8CSSPerspectiveValue::ContentType::kCSSKeywordValue: {
      if (value->GetAsCSSKeywordValue()->KeywordValueID() !=
          CSSValueID::kNone) {
        return nullptr;
      }
      break;
    }
  }
  return value;
}

}  // namespace

CSSPerspective* CSSPerspective::Create(V8CSSPerspectiveValue* length,
                                       ExceptionState& exception_state) {
  length = HandleInputPerspective(length);
  if (!length) {
    exception_state.ThrowTypeError(
        "Must pass length or none to CSSPerspective");
    return nullptr;
  }
  return MakeGarbageCollected<CSSPerspective>(length);
}

void CSSPerspective::setLength(V8CSSPerspectiveValue* length,
                               ExceptionState& exception_state) {
  length = HandleInputPerspective(length);
  if (!length) {
    exception_state.ThrowTypeError(
        "Must pass length or none to CSSPerspective");
    return;
  }
  length_ = length;
}

CSSPerspective* CSSPerspective::FromCSSValue(const CSSFunctionValue& value) {
  DCHECK_EQ(value.FunctionType(), CSSValueID::kPerspective);
  DCHECK_EQ(value.length(), 1U);
  const CSSValue& arg = value.Item(0);
  V8CSSPerspectiveValue* length;
  if (arg.IsPrimitiveValue()) {
    length = MakeGarbageCollected<V8CSSPerspectiveValue>(
        CSSNumericValue::FromCSSValue(To<CSSPrimitiveValue>(arg)));
  } else {
    DCHECK(arg.IsIdentifierValue() &&
           To<CSSIdentifierValue>(arg).GetValueID() == CSSValueID::kNone);
    length = MakeGarbageCollected<V8CSSPerspectiveValue>(
        CSSKeywordValue::FromCSSValue(arg));
  }
  return MakeGarbageCollected<CSSPerspective>(length);
}

DOMMatrix* CSSPerspective::toMatrix(ExceptionState& exception_state) const {
  if (!length_->IsCSSNumericValue()) {
    DCHECK(length_->IsCSSKeywordValue());
    // 'none' is an identity matrix
    return DOMMatrix::Create();
  }
  const CSSNumericValue* numeric = length_->GetAsCSSNumericValue();
  if (numeric->IsUnitValue() && To<CSSUnitValue>(numeric)->value() < 0) {
    // Negative values are invalid.
    // https://github.com/w3c/css-houdini-drafts/issues/420
    return nullptr;
  }
  CSSUnitValue* length = numeric->to(CSSPrimitiveValue::UnitType::kPixels);
  if (!length) {
    exception_state.ThrowTypeError(
        "Cannot create matrix if units are not compatible with px");
    return nullptr;
  }
  DOMMatrix* matrix = DOMMatrix::Create();
  matrix->perspectiveSelf(length->value());
  return matrix;
}

const CSSFunctionValue* CSSPerspective::ToCSSValue() const {
  const CSSValue* length = nullptr;
  if (!length_->IsCSSNumericValue()) {
    CHECK(length_->IsCSSKeywordValue());
    length = length_->GetAsCSSKeywordValue()->ToCSSValue();
  } else {
    const CSSNumericValue* numeric = length_->GetAsCSSNumericValue();
    if (numeric->IsUnitValue() && To<CSSUnitValue>(numeric)->value() < 0) {
      // Wrap out of range length with a calc.
      CSSMathExpressionNode* node = numeric->ToCalcExpressionNode();
      node->SetIsNestedCalc();
      length = CSSMathFunctionValue::Create(node);
    } else {
      length = numeric->ToCSSValue();
    }
  }

  // TODO(crbug.com/983784): We currently don't fully support typed
  // arithmetic, which can cause `length` to be nullptr here.
  if (!length) {
    return nullptr;
  }

  auto* result =
      MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kPerspective);
  result->Append(*length);
  return result;
}

CSSPerspective::CSSPerspective(V8CSSPerspectiveValue* length)
    : CSSTransformComponent(false /* is2D */), length_(length) {
  DCHECK(length);
}

}  // namespace blink
