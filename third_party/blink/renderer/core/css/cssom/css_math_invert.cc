// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_math_invert.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_sum_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

V8CSSNumberish* CSSMathInvert::value() {
  return MakeGarbageCollected<V8CSSNumberish>(value_);
}

std::optional<CSSNumericSumValue> CSSMathInvert::SumValue() const {
  auto sum = value_->SumValue();
  if (!sum.has_value() || sum->terms.size() != 1) {
    return std::nullopt;
  }

  for (auto& unit_exponent : sum->terms[0].units) {
    unit_exponent.value *= -1;
  }

  sum->terms[0].value = 1.0 / sum->terms[0].value;
  return sum;
}

void CSSMathInvert::BuildCSSText(Nested nested,
                                 ParenLess paren_less,
                                 StringBuilder& result) const {
  if (paren_less == ParenLess::kNo) {
    result.Append(nested == Nested::kYes ? "(" : "calc(");
  }

  result.Append("1 / ");
  value_->BuildCSSText(Nested::kYes, ParenLess::kNo, result);

  if (paren_less == ParenLess::kNo) {
    result.Append(")");
  }
}

CSSMathExpressionNode* CSSMathInvert::ToCalcExpressionNode() const {
  CSSMathExpressionNode* right_side = value_->ToCalcExpressionNode();
  if (!right_side) {
    return nullptr;
  }
  return CSSMathExpressionOperation::CreateArithmeticOperation(
      CSSMathExpressionNumericLiteral::Create(
          1, CSSPrimitiveValue::UnitType::kNumber),
      right_side, CSSMathOperator::kDivide);
}

}  // namespace blink
