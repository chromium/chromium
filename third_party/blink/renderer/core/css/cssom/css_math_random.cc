// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_math_random.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_sum_value.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSMathRandom* CSSMathRandom::Create(double base_value,
                                     V8CSSNumberish* min,
                                     V8CSSNumberish* max,
                                     V8CSSNumberish* step,
                                     ExceptionState& exception_state) {
  auto* min_value = CSSNumericValue::FromNumberish(min);
  auto* max_value = CSSNumericValue::FromNumberish(max);
  auto* step_value = step ? CSSNumericValue::FromNumberish(step) : nullptr;
  CSSMathRandom* result = Create(base_value, min_value, max_value, step_value);
  if (!result) {
    exception_state.ThrowTypeError("Incompatible types");
    return nullptr;
  }

  return result;
}

CSSMathRandom* CSSMathRandom::Create(double base_value,
                                     CSSNumericValue* min,
                                     CSSNumericValue* max,
                                     CSSNumericValue* step) {
  bool error = false;
  CSSNumericValueType type =
      CSSMathRandom::TypeCheck(min, max, step, CSSNumericValueType::Add, error);
  return error ? nullptr
               : MakeGarbageCollected<CSSMathRandom>(base_value, min, max, step,
                                                     type);
}

double CSSMathRandom::baseValue() {
  return random_base_value_;
}

CSSNumericValue* CSSMathRandom::min() {
  return min_;
}

CSSNumericValue* CSSMathRandom::max() {
  return max_;
}

CSSNumericValue* CSSMathRandom::step() {
  return step_;
}

std::optional<CSSNumericSumValue> CSSMathRandom::SumValue() const {
  // TODO(crbug.com/413385732): Spec and implement sum value for random():
  // https://drafts.css-houdini.org/css-typed-om/#create-a-sum-value
  return std::nullopt;
}

void CSSMathRandom::BuildCSSText(Nested,
                                 ParenLess,
                                 StringBuilder& result) const {
  result.Append("random(");
  DCHECK(min_);
  min_->BuildCSSText(Nested::kYes, ParenLess::kYes, result);
  result.Append(", ");
  DCHECK(max_);
  max_->BuildCSSText(Nested::kYes, ParenLess::kYes, result);
  if (step_) {
    result.Append(", ");
    step_->BuildCSSText(Nested::kYes, ParenLess::kYes, result);
  }
  result.Append(")");
}

CSSMathExpressionNode* CSSMathRandom::ToCalcExpressionNode() const {
  CSSMathExpressionOperation::Operands operands;
  operands.reserve(3u);
  DCHECK(min_);
  DCHECK(max_);
  for (const CSSNumericValue* value : {min_, max_, step_}) {
    if (!value) {
      // step_ value can be null since it is optional
      break;
    }
    CSSMathExpressionNode* operand = value->ToCalcExpressionNode();
    if (!operand) {
      // TODO(crbug.com/41470626): Remove this when all ToCalcExpressionNode()
      // overrides are implemented.
      NOTREACHED();
    }
    operands.push_back(operand);
  }
  return CSSMathExpressionRandomFunction::Create(
      RandomValueSharing::Fixed(random_base_value_), std::move(operands));
}

}  // namespace blink
