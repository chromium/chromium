// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_math_clamp.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_sum_value.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSMathClamp* CSSMathClamp::Create(V8CSSNumberish* lower,
                                   V8CSSNumberish* value,
                                   V8CSSNumberish* upper,
                                   ExceptionState& exception_state) {
  auto* lower_value = CSSNumericValue::FromNumberish(lower);
  auto* value_value = CSSNumericValue::FromNumberish(value);
  auto* upper_value = CSSNumericValue::FromNumberish(upper);
  CSSMathClamp* result = Create(lower_value, value_value, upper_value);
  if (!result) {
    exception_state.ThrowTypeError("Incompatible types");
    return nullptr;
  }

  return result;
}

CSSMathClamp* CSSMathClamp::Create(CSSNumericValue* lower,
                                   CSSNumericValue* value,
                                   CSSNumericValue* upper) {
  bool error = false;
  CSSNumericValueType final_type = CSSMathClamp::TypeCheck(
      lower, value, upper, CSSNumericValueType::Add, error);
  return error ? nullptr
               : MakeGarbageCollected<CSSMathClamp>(lower, value, upper,
                                                    final_type);
}

V8CSSNumberish* CSSMathClamp::lower() {
  return MakeGarbageCollected<V8CSSNumberish>(lower_);
}

V8CSSNumberish* CSSMathClamp::value() {
  return MakeGarbageCollected<V8CSSNumberish>(value_);
}

V8CSSNumberish* CSSMathClamp::upper() {
  return MakeGarbageCollected<V8CSSNumberish>(upper_);
}

std::optional<CSSNumericSumValue> CSSMathClamp::SumValue() const {
  auto lower = lower_->SumValue();

  for (const auto& value : {lower_, value_, upper_}) {
    const auto child_sum = value->SumValue();
    if (!child_sum.has_value() || child_sum->terms.size() != 1 ||
        child_sum->terms[0].units != lower->terms[0].units) {
      return std::nullopt;
    }
  }

  auto value = value_->SumValue();
  auto upper = upper_->SumValue();
  auto lower_val = lower->terms[0].value;
  auto value_val = value->terms[0].value;
  auto upper_val = upper->terms[0].value;
  value->terms[0].value = std::max(lower_val, std::min(value_val, upper_val));

  return value;
}

void CSSMathClamp::BuildCSSText(Nested,
                                ParenLess,
                                StringBuilder& result) const {
  result.Append("clamp(");
  DCHECK(lower_);
  lower_->BuildCSSText(Nested::kYes, ParenLess::kYes, result);
  result.Append(", ");
  DCHECK(value_);
  value_->BuildCSSText(Nested::kYes, ParenLess::kYes, result);
  result.Append(", ");
  DCHECK(upper_);
  upper_->BuildCSSText(Nested::kYes, ParenLess::kYes, result);
  result.Append(")");
}

CSSMathExpressionNode* CSSMathClamp::ToCalcExpressionNode() const {
  CSSMathExpressionOperation::Operands operands;
  operands.reserve(3u);
  for (const auto& value : {lower_, value_, upper_}) {
    CSSMathExpressionNode* operand = value->ToCalcExpressionNode();
    if (!operand) {
      // TODO(crbug.com/983784): Remove this when all ToCalcExpressionNode()
      // overrides are implemented.
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    }
    operands.push_back(value->ToCalcExpressionNode());
  }
  return CSSMathExpressionOperation::CreateComparisonFunction(
      std::move(operands), CSSMathOperator::kClamp);
}

}  // namespace blink
