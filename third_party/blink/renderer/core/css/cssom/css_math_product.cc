// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_math_product.h"

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_invert.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

CSSNumericSumValue::UnitMap MultiplyUnitMaps(
    CSSNumericSumValue::UnitMap a,
    const CSSNumericSumValue::UnitMap& b) {
  for (const auto& unit_exponent : b) {
    DCHECK_NE(unit_exponent.value, 0);
    const auto old_value =
        a.Contains(unit_exponent.key) ? a.at(unit_exponent.key) : 0;

    // Remove any zero entries
    if (old_value + unit_exponent.value == 0)
      a.erase(unit_exponent.key);
    else
      a.Set(unit_exponent.key, old_value + unit_exponent.value);
  }
  return a;
}

}  // namespace

CSSMathProduct* CSSMathProduct::Create(const HeapVector<CSSNumberish>& args,
                                       ExceptionState& exception_state) {
  if (args.IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Arguments can't be empty");
    return nullptr;
  }

  CSSMathProduct* result = Create(CSSNumberishesToNumericValues(args));
  if (!result) {
    exception_state.ThrowTypeError("Incompatible types");
    return nullptr;
  }

  return result;
}

CSSMathProduct* CSSMathProduct::Create(CSSNumericValueVector values) {
  bool error = false;
  CSSNumericValueType final_type =
      CSSMathVariadic::TypeCheck(values, CSSNumericValueType::Multiply, error);
  return error ? nullptr
               : MakeGarbageCollected<CSSMathProduct>(
                     MakeGarbageCollected<CSSNumericArray>(std::move(values)),
                     final_type);
}

base::Optional<CSSNumericSumValue> CSSMathProduct::SumValue() const {
  CSSNumericSumValue sum;
  // Start with the number '1', which is the multiplicative identity.
  sum.terms.push_back(CSSNumericSumValue::Term{1, {}});

  for (const auto& value : NumericValues()) {
    const auto child_sum = value->SumValue();
    if (!child_sum)
      return base::nullopt;

    CSSNumericSumValue new_sum;
    for (const auto& a : sum.terms) {
      for (const auto& b : child_sum->terms) {
        new_sum.terms.emplace_back(a.value * b.value,
                                   MultiplyUnitMaps(a.units, b.units));
      }
    }

    sum = new_sum;
  }
  return sum;
}

CSSMathExpressionNode* CSSMathProduct::ToCalcExpressionNode() const {
  // TODO(crbug.com/782103): Handle the single value case correctly.
  if (NumericValues().size() == 1)
    return NumericValues()[0]->ToCalcExpressionNode();

  CSSMathExpressionNode* node = CSSMathExpressionBinaryOperation::Create(
      NumericValues()[0]->ToCalcExpressionNode(),
      NumericValues()[1]->ToCalcExpressionNode(), CSSMathOperator::kMultiply);

  for (wtf_size_t i = 2; i < NumericValues().size(); i++) {
    node = CSSMathExpressionBinaryOperation::Create(
        node, NumericValues()[i]->ToCalcExpressionNode(),
        CSSMathOperator::kMultiply);
  }

  return node;
}

void CSSMathProduct::BuildCSSText(Nested nested,
                                  ParenLess paren_less,
                                  StringBuilder& result) const {
  if (paren_less == ParenLess::kNo)
    result.Append(nested == Nested::kYes ? "(" : "calc(");

  const auto& values = NumericValues();
  DCHECK(!values.IsEmpty());
  values[0]->BuildCSSText(Nested::kYes, ParenLess::kNo, result);

  for (wtf_size_t i = 1; i < values.size(); i++) {
    const auto& arg = *values[i];
    if (arg.GetType() == CSSStyleValue::kInvertType) {
      result.Append(" / ");
      static_cast<const CSSMathInvert&>(arg).Value().BuildCSSText(
          Nested::kYes, ParenLess::kNo, result);
    } else {
      result.Append(" * ");
      arg.BuildCSSText(Nested::kYes, ParenLess::kNo, result);
    }
  }

  if (paren_less == ParenLess::kNo)
    result.Append(")");
}

}  // namespace blink
