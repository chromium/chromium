// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_math_function_value.h"

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

struct SameSizeAsCSSMathFunctionValue : CSSPrimitiveValue {
  Member<void*> expression;
};
ASSERT_SIZE(CSSMathFunctionValue, SameSizeAsCSSMathFunctionValue);

void CSSMathFunctionValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(expression_);
  CSSPrimitiveValue::TraceAfterDispatch(visitor);
}

CSSMathFunctionValue::CSSMathFunctionValue(
    const CSSMathExpressionNode* expression,
    ValueRange range)
    : CSSPrimitiveValue(kMathFunctionClass), expression_(expression) {
  is_non_negative_math_function_ = range == kValueRangeNonNegative;
}

// static
CSSMathFunctionValue* CSSMathFunctionValue::Create(
    const CSSMathExpressionNode* expression,
    ValueRange range) {
  if (!expression)
    return nullptr;
  return MakeGarbageCollected<CSSMathFunctionValue>(expression, range);
}

// static
CSSMathFunctionValue* CSSMathFunctionValue::Create(const Length& length,
                                                   float zoom) {
  DCHECK(length.IsCalculated());
  auto calc = length.GetCalculationValue().Zoom(1.0 / zoom);
  return Create(CSSMathExpressionNode::Create(*calc), calc->GetValueRange());
}

bool CSSMathFunctionValue::MayHaveRelativeUnit() const {
  UnitType resolved_type = expression_->ResolvedUnitType();
  return IsRelativeUnit(resolved_type) || resolved_type == UnitType::kUnknown;
}

double CSSMathFunctionValue::DoubleValue() const {
#if DCHECK_IS_ON()
  if (IsPercentage()) {
    DCHECK(!AllowsNegativePercentageReference() ||
           !expression_->InvolvesPercentageComparisons());
  }
#endif
  return ClampToPermittedRange(expression_->DoubleValue());
}

double CSSMathFunctionValue::ComputeSeconds() const {
  DCHECK_EQ(kCalcTime, expression_->Category());
  // TODO(crbug.com/984372): We currently use 'ms' as the canonical unit of
  // <time>. Switch to 's' to follow the spec.
  return ClampToPermittedRange(*expression_->ComputeValueInCanonicalUnit() /
                               1000);
}

double CSSMathFunctionValue::ComputeDegrees() const {
  DCHECK_EQ(kCalcAngle, expression_->Category());
  return ClampToPermittedRange(*expression_->ComputeValueInCanonicalUnit());
}

double CSSMathFunctionValue::ComputeLengthPx(
    const CSSToLengthConversionData& conversion_data) const {
  // |CSSToLengthConversionData| only resolves relative length units, but not
  // percentages.
  DCHECK_EQ(kCalcLength, expression_->Category());
  return ClampToPermittedRange(expression_->ComputeLengthPx(conversion_data));
}

bool CSSMathFunctionValue::AccumulateLengthArray(CSSLengthArray& length_array,
                                                 double multiplier) const {
  return expression_->AccumulateLengthArray(length_array, multiplier);
}

Length CSSMathFunctionValue::ConvertToLength(
    const CSSToLengthConversionData& conversion_data) const {
  if (IsLength())
    return Length::Fixed(ComputeLengthPx(conversion_data));
  return Length(ToCalcValue(conversion_data));
}

static String BuildCSSText(const String& expression) {
  StringBuilder result;
  result.Append("calc");
  result.Append('(');
  result.Append(expression);
  result.Append(')');
  return result.ToString();
}

String CSSMathFunctionValue::CustomCSSText() const {
  const String& expression_text = expression_->CustomCSSText();
  if (expression_->IsMathFunction()) {
    // If |expression_| is already a math function (e.g., min/max), we don't
    // need to wrap it in |calc()|.
    return expression_text;
  }
  return BuildCSSText(expression_text);
}

bool CSSMathFunctionValue::Equals(const CSSMathFunctionValue& other) const {
  return DataEquivalent(expression_, other.expression_);
}

double CSSMathFunctionValue::ClampToPermittedRange(double value) const {
  return IsNonNegative() && value < 0 ? 0 : value;
}

bool CSSMathFunctionValue::IsZero() const {
  if (expression_->ResolvedUnitType() == UnitType::kUnknown)
    return false;
  return expression_->IsZero();
}

bool CSSMathFunctionValue::IsPx() const {
  // TODO(crbug.com/979895): This is the result of refactoring, which might be
  // an existing bug. Fix it if necessary.
  return Category() == kCalcLength;
}

bool CSSMathFunctionValue::IsComputationallyIndependent() const {
  return expression_->IsComputationallyIndependent();
}

scoped_refptr<CalculationValue> CSSMathFunctionValue::ToCalcValue(
    const CSSToLengthConversionData& conversion_data) const {
  return expression_->ToCalcValue(conversion_data, PermittedValueRange());
}

}  // namespace blink
