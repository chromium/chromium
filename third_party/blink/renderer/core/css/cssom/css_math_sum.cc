// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_math_sum.h"

#include "base/ranges/algorithm.h"
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
  DCHECK(!sum.terms.empty());

  const auto first_type = NumericTypeFromUnitMap(sum.terms[0].units);
  return base::ranges::all_of(
      sum.terms, [&first_type](const CSSNumericSumValue::Term& term) {
        bool error = false;
        CSSNumericValueType::Add(first_type, NumericTypeFromUnitMap(term.units),
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

CSSMathSum* CSSMathSum::Create(const HeapVector<Member<V8CSSNumberish>>& args,
                               ExceptionState& exception_state) {
  if (args.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Arguments can't be empty");
    return nullptr;
  }

  CSSMathSum* result =
      Create(CSSNumberishesToNumericValues(args), exception_state);
  if (!result) {
    exception_state.ThrowTypeError("Incompatible types");
    return nullptr;
  }

  return result;
}

CSSMathSum* CSSMathSum::Create(CSSNumericValueVector values,
                               ExceptionState& exception_state) {
  bool error = false;
  CSSNumericValueType final_type =
      CSSMathVariadic::TypeCheck(values, CSSNumericValueType::Add, error);
  CSSMathSum* result =
      error ? nullptr
            : MakeGarbageCollected<CSSMathSum>(
                  MakeGarbageCollected<CSSNumericArray>(std::move(values)),
                  final_type);
  if (!result) {
    exception_state.ThrowTypeError("Incompatible types");
  }

  return result;
}

std::optional<CSSNumericSumValue> CSSMathSum::SumValue() const {
  CSSNumericSumValue sum;
  for (const auto& value : NumericValues()) {
    const auto child_sum = value->SumValue();
    if (!child_sum.has_value()) {
      return std::nullopt;
    }

    // Collect like-terms
    for (const auto& term : child_sum->terms) {
      wtf_size_t index = sum.terms.Find(UnitMapComparator{term});
      if (index == kNotFound) {
        sum.terms.push_back(term);
      } else {
        sum.terms[index].value += term.value;
      }
    }
  }

  if (!CanCreateNumericTypeFromSumValue(sum)) {
    return std::nullopt;
  }

  return sum;
}

CSSMathExpressionNode* CSSMathSum::ToCalcExpressionNode() const {
  return ToCalcExporessionNodeForVariadic(CSSMathOperator::kAdd);
}

void CSSMathSum::BuildCSSText(Nested nested,
                              ParenLess paren_less,
                              StringBuilder& result) const {
  if (paren_less == ParenLess::kNo) {
    result.Append(nested == Nested::kYes ? "(" : "calc(");
  }

  const auto& values = NumericValues();
  DCHECK(!values.empty());
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

  if (paren_less == ParenLess::kNo) {
    result.Append(")");
  }
}

}  // namespace blink
