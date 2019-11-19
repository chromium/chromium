// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_math_sum.h"

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_negate.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

CSSNumericValueType NumericTypeFromUnitMap(
    const CSSNumericSumValue::UnitMap& units) {
  CSSNumericValueType type;
  for (const auto& unit_exponent : units) {
    bool error = false;
    type = CSSNumericValueType::Multiply(
        type, CSSNumericValueType(unit_exponent.value, unit_exponent.key),
        error);
    DCHECK(!error);
  }
  return type;
}

bool CanCreateNumericTypeFromSumValue(const CSSNumericSumValue& sum) {
  DCHECK(!sum.terms.IsEmpty());

  const auto first_type = NumericTypeFromUnitMap(sum.terms[0].units);
  return std::all_of(sum.terms.begin(), sum.terms.end(),
                     [&first_type](const CSSNumericSumValue::Term& term) {
                       bool error = false;
                       CSSNumericValueType::Add(
                           first_type, NumericTypeFromUnitMap(term.units),
                           error);
                       return !error;
                     });
}

struct UnitMapComparator {
  CSSNumericSumValue::Term term;
};

bool operator==(const CSSNumericSumValue::Term& a, const UnitMapComparator& b) {
  return a.units == b.term.units;
}

}  // namespace

CSSMathSum* CSSMathSum::Create(const HeapVector<CSSNumberish>& args,
                               ExceptionState& exception_state) {
  if (args.IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Arguments can't be empty");
    return nullptr;
  }

  CSSMathSum* result = Create(CSSNumberishesToNumericValues(args));
  if (!result) {
    exception_state.ThrowTypeError("Incompatible types");
    return nullptr;
  }

  return result;
}

CSSMathSum* CSSMathSum::Create(CSSNumericValueVector values) {
  bool error = false;
  CSSNumericValueType final_type =
      CSSMathVariadic::TypeCheck(values, CSSNumericValueType::Add, error);
  return error ? nullptr
               : MakeGarbageCollected<CSSMathSum>(
                     MakeGarbageCollected<CSSNumericArray>(std::move(values)),
                     final_type);
}

base::Optional<CSSNumericSumValue> CSSMathSum::SumValue() const {
  CSSNumericSumValue sum;
  for (const auto& value : NumericValues()) {
    const auto child_sum = value->SumValue();
    if (!child_sum)
      return base::nullopt;

    // Collect like-terms
    for (const auto& term : child_sum->terms) {
      wtf_size_t index = sum.terms.Find(UnitMapComparator{term});
      if (index == kNotFound)
        sum.terms.push_back(term);
      else
        sum.terms[index].value += term.value;
    }
  }

  if (!CanCreateNumericTypeFromSumValue(sum))
    return base::nullopt;

  return sum;
}

CSSMathExpressionNode* CSSMathSum::ToCalcExpressionNode() const {
  // TODO(crbug.com/782103): Handle the single value case correctly.
  if (NumericValues().size() == 1)
    return NumericValues()[0]->ToCalcExpressionNode();

  CSSMathExpressionNode* node = CSSMathExpressionBinaryOperation::Create(
      NumericValues()[0]->ToCalcExpressionNode(),
      NumericValues()[1]->ToCalcExpressionNode(), CSSMathOperator::kAdd);

  for (wtf_size_t i = 2; i < NumericValues().size(); i++) {
    node = CSSMathExpressionBinaryOperation::Create(
        node, NumericValues()[i]->ToCalcExpressionNode(),
        CSSMathOperator::kAdd);
  }

  return node;
}

void CSSMathSum::BuildCSSText(Nested nested,
                              ParenLess paren_less,
                              StringBuilder& result) const {
  if (paren_less == ParenLess::kNo)
    result.Append(nested == Nested::kYes ? "(" : "calc(");

  const auto& values = NumericValues();
  DCHECK(!values.IsEmpty());
  values[0]->BuildCSSText(Nested::kYes, ParenLess::kNo, result);

  for (wtf_size_t i = 1; i < values.size(); i++) {
    const auto& arg = *values[i];
    if (arg.GetType() == CSSStyleValue::kNegateType) {
      result.Append(" - ");
      static_cast<const CSSMathNegate&>(arg).Value().BuildCSSText(
          Nested::kYes, ParenLess::kNo, result);
    } else {
      result.Append(" + ");
      arg.BuildCSSText(Nested::kYes, ParenLess::kNo, result);
    }
  }

  if (paren_less == ParenLess::kNo)
    result.Append(")");
}

}  // namespace blink
