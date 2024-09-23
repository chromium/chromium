// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_math_min.h"

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_sum_value.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSMathMin* CSSMathMin::Create(const HeapVector<Member<V8CSSNumberish>>& args,
                               ExceptionState& exception_state) {
  if (args.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Arguments can't be empty");
    return nullptr;
  }

  CSSMathMin* result = Create(CSSNumberishesToNumericValues(args));
  if (!result) {
    exception_state.ThrowTypeError("Incompatible types");
    return nullptr;
  }

  return result;
}

CSSMathMin* CSSMathMin::Create(CSSNumericValueVector values) {
  bool error = false;
  CSSNumericValueType final_type =
      CSSMathVariadic::TypeCheck(values, CSSNumericValueType::Add, error);
  return error ? nullptr
               : MakeGarbageCollected<CSSMathMin>(
                     MakeGarbageCollected<CSSNumericArray>(std::move(values)),
                     final_type);
}

std::optional<CSSNumericSumValue> CSSMathMin::SumValue() const {
  auto cur_min = NumericValues()[0]->SumValue();
  if (!cur_min.has_value() || cur_min->terms.size() != 1) {
    return std::nullopt;
  }

  for (const auto& value : NumericValues()) {
    const auto child_sum = value->SumValue();
    if (!child_sum.has_value() || child_sum->terms.size() != 1 ||
        child_sum->terms[0].units != cur_min->terms[0].units) {
      return std::nullopt;
    }

    if (child_sum->terms[0].value < cur_min->terms[0].value) {
      cur_min = child_sum;
    }
  }
  return cur_min;
}

void CSSMathMin::BuildCSSText(Nested, ParenLess, StringBuilder& result) const {
  result.Append("min(");

  bool first_iteration = true;
  for (const auto& value : NumericValues()) {
    if (!first_iteration) {
      result.Append(", ");
    }
    first_iteration = false;

    value->BuildCSSText(Nested::kYes, ParenLess::kYes, result);
  }

  result.Append(")");
}

CSSMathExpressionNode* CSSMathMin::ToCalcExpressionNode() const {
  CSSMathExpressionOperation::Operands operands;
  operands.reserve(NumericValues().size());
  for (const auto& value : NumericValues()) {
    CSSMathExpressionNode* operand = value->ToCalcExpressionNode();
    if (!operand) {
      // TODO(crbug.com/983784): Remove this when all ToCalcExpressionNode()
      // overrides are implemented.
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    operands.push_back(value->ToCalcExpressionNode());
  }
  if (!operands.size()) {
    // TODO(crbug.com/983784): Remove this when all ToCalcExpressionNode()
    // overrides are implemented.
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  return CSSMathExpressionOperation::CreateComparisonFunction(
      std::move(operands), CSSMathOperator::kMin);
}

}  // namespace blink
