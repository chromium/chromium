// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_math_max.h"

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_sum_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSMathMax* CSSMathMax::Create(const HeapVector<CSSNumberish>& args,
                               ExceptionState& exception_state) {
  if (args.IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Arguments can't be empty");
    return nullptr;
  }

  CSSMathMax* result = Create(CSSNumberishesToNumericValues(args));
  if (!result) {
    exception_state.ThrowTypeError("Incompatible types");
    return nullptr;
  }

  return result;
}

CSSMathMax* CSSMathMax::Create(CSSNumericValueVector values) {
  bool error = false;
  CSSNumericValueType final_type =
      CSSMathVariadic::TypeCheck(values, CSSNumericValueType::Add, error);
  return error ? nullptr
               : MakeGarbageCollected<CSSMathMax>(
                     MakeGarbageCollected<CSSNumericArray>(std::move(values)),
                     final_type);
}

base::Optional<CSSNumericSumValue> CSSMathMax::SumValue() const {
  auto cur_max = NumericValues()[0]->SumValue();
  if (!cur_max || cur_max->terms.size() != 1)
    return base::nullopt;

  for (const auto& value : NumericValues()) {
    const auto child_sum = value->SumValue();
    if (!child_sum || child_sum->terms.size() != 1 ||
        child_sum->terms[0].units != cur_max->terms[0].units)
      return base::nullopt;

    if (child_sum->terms[0].value > cur_max->terms[0].value)
      cur_max = child_sum;
  }
  return cur_max;
}

void CSSMathMax::BuildCSSText(Nested, ParenLess, StringBuilder& result) const {
  result.Append("max(");

  bool first_iteration = true;
  for (const auto& value : NumericValues()) {
    if (!first_iteration)
      result.Append(", ");
    first_iteration = false;

    DCHECK(value);
    value->BuildCSSText(Nested::kYes, ParenLess::kYes, result);
  }

  result.Append(")");
}

CSSMathExpressionNode* CSSMathMax::ToCalcExpressionNode() const {
  if (!RuntimeEnabledFeatures::CSSComparisonFunctionsEnabled())
    return nullptr;

  CSSMathExpressionVariadicOperation::Operands operands;
  operands.ReserveCapacity(NumericValues().size());
  for (const auto& value : NumericValues()) {
    CSSMathExpressionNode* operand = value->ToCalcExpressionNode();
    if (!operand) {
      // TODO(crbug.com/983784): Remove this when all ToCalcExpressionNode()
      // overrides are implemented.
      NOTREACHED();
      continue;
    }
    operands.push_back(value->ToCalcExpressionNode());
  }
  if (!operands.size()) {
    // TODO(crbug.com/983784): Remove this when all ToCalcExpressionNode()
    // overrides are implemented.
    NOTREACHED();
    return nullptr;
  }
  return CSSMathExpressionVariadicOperation::Create(std::move(operands),
                                                    CSSMathOperator::kMax);
}

}  // namespace blink
