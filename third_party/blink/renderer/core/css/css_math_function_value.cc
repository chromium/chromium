// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_math_function_value.h"

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_clamping_utils.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

struct SameSizeAsCSSMathFunctionValue : CSSPrimitiveValue {
  Member<void*> expression;
  ValueRange value_range_in_target_context_;
};
ASSERT_SIZE(CSSMathFunctionValue, SameSizeAsCSSMathFunctionValue);

void CSSMathFunctionValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(expression_);
  CSSPrimitiveValue::TraceAfterDispatch(visitor);
}

CSSMathFunctionValue::CSSMathFunctionValue(
    const CSSMathExpressionNode* expression,
    CSSPrimitiveValue::ValueRange range)
    : CSSPrimitiveValue(kMathFunctionClass),
      expression_(expression),
      value_range_in_target_context_(range) {
  needs_tree_scope_population_ = !expression->IsScopedValue();
}

// static
CSSMathFunctionValue* CSSMathFunctionValue::Create(
    const CSSMathExpressionNode* expression,
    CSSPrimitiveValue::ValueRange range) {
  if (!expression) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSMathFunctionValue>(expression, range);
}

// static
CSSMathFunctionValue* CSSMathFunctionValue::Create(const Length& length,
                                                   float zoom) {
  DCHECK(length.IsCalculated());
  const auto* calc = length.GetCalculationValue().Zoom(1.0 / zoom);
  return Create(
      CSSMathExpressionNode::Create(*calc),
      CSSPrimitiveValue::ValueRangeForLengthValueRange(calc->GetValueRange()));
}

bool CSSMathFunctionValue::MayHaveRelativeUnit() const {
  return expression_->MayHaveRelativeUnit();
}

double CSSMathFunctionValue::ComputeDegrees(
    const CSSLengthResolver& length_resolver) const {
  DCHECK_EQ(kCalcAngle, expression_->Category());
  return ClampToPermittedRange(expression_->ComputeNumber(length_resolver));
}

double CSSMathFunctionValue::ComputeSeconds(
    const CSSLengthResolver& length_resolver) const {
  DCHECK_EQ(kCalcTime, expression_->Category());
  return ClampToPermittedRange(expression_->ComputeNumber(length_resolver));
}

double CSSMathFunctionValue::ComputeDotsPerPixel(
    const CSSLengthResolver& length_resolver) const {
  DCHECK_EQ(kCalcResolution, expression_->Category());
  return ClampToPermittedRange(expression_->ComputeNumber(length_resolver));
}

double CSSMathFunctionValue::ComputeLengthPx(
    const CSSLengthResolver& length_resolver) const {
  // |CSSToLengthConversionData| only resolves relative length units, but not
  // percentages.
  DCHECK_EQ(kCalcLength, expression_->Category());
  DCHECK(!expression_->HasPercentage());
  return ClampToPermittedRange(expression_->ComputeLengthPx(length_resolver));
}

int CSSMathFunctionValue::ComputeInteger(
    const CSSLengthResolver& length_resolver) const {
  // |CSSToLengthConversionData| only resolves relative length units, but not
  // percentages.
  DCHECK_EQ(kCalcNumber, expression_->Category());
  DCHECK(!expression_->HasPercentage());
  return ClampToWithNaNTo0<int>(
      ClampToPermittedRange(expression_->ComputeNumber(length_resolver)));
}

double CSSMathFunctionValue::ComputeNumber(
    const CSSLengthResolver& length_resolver) const {
  if (expression_->Category() == kCalcNumber) {
    // |CSSToLengthConversionData| only resolves relative length units, but not
    // percentages.
    DCHECK(!expression_->HasPercentage());
  } else {
    DCHECK_EQ(expression_->Category(), kCalcPercent);
  }
  double value =
      ClampToPermittedRange(expression_->ComputeNumber(length_resolver));
  if (expression_->Category() == kCalcPercent) {
    value /= 100.0;
  }
  return std::isnan(value) ? 0.0 : CSSValueClampingUtils::ClampDouble(value);
}

double CSSMathFunctionValue::ComputePercentage(
    const CSSLengthResolver& length_resolver) const {
  // |CSSToLengthConversionData| only resolves relative length units, but not
  // percentages.
  DCHECK_EQ(kCalcPercent, expression_->Category());
  double value =
      ClampToPermittedRange(expression_->ComputeNumber(length_resolver));
  return CSSValueClampingUtils::ClampDouble(value);
}

double CSSMathFunctionValue::ComputeValueInCanonicalUnit(
    const CSSLengthResolver& length_resolver) const {
  // Don't use it for mix of length and percentage or similar,
  // as it would compute 10px + 10% to 20.
  DCHECK(IsResolvableBeforeLayout());
  std::optional<double> optional_value =
      expression_->ComputeValueInCanonicalUnit(length_resolver);
  DCHECK(optional_value.has_value());
  double value = ClampToPermittedRange(optional_value.value());
  return std::isnan(value) ? 0.0 : value;
}

std::optional<double> CSSMathFunctionValue::GetValueIfKnown() const {
  std::optional<double> val = expression_->GetValueIfKnown();
  if (val.has_value()) {
    return ClampToPermittedRange(CSSValueClampingUtils::ClampDouble(*val));
  }
  return val;
}

bool CSSMathFunctionValue::AccumulateLengthArray(CSSLengthArray& length_array,
                                                 double multiplier) const {
  return expression_->AccumulateLengthArray(length_array, multiplier);
}

Length CSSMathFunctionValue::ConvertToLength(
    const CSSLengthResolver& length_resolver) const {
  if (IsResolvableLength()) {
    return Length::Fixed(ComputeLengthPx(length_resolver));
  }
  return Length(ToCalcValue(length_resolver));
}

static String BuildCSSText(const String& expression) {
  StringBuilder result;
  result.Append("calc");
  // https://drafts.csswg.org/css-values-4/#serialize-a-math-function
  // “If a result of this serialization starts with a "(" (open parenthesis) and
  // ends with a ")" (close parenthesis), remove those characters from the
  // result.”
  if (expression.StartsWith('(')) {
    DCHECK(expression.EndsWith(')'));
    result.Append(expression);
  } else {
    result.Append('(');
    result.Append(expression);
    result.Append(')');
  }
  return result.ReleaseString();
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
  return base::ValuesEquivalent(expression_, other.expression_);
}

double CSSMathFunctionValue::ClampToPermittedRange(double value) const {
  switch (PermittedValueRange()) {
    case CSSPrimitiveValue::ValueRange::kInteger:
      return RoundHalfTowardsPositiveInfinity(value);
    case CSSPrimitiveValue::ValueRange::kNonNegativeInteger:
      return RoundHalfTowardsPositiveInfinity(std::max(value, 0.0));
    case CSSPrimitiveValue::ValueRange::kPositiveInteger:
      return RoundHalfTowardsPositiveInfinity(std::max(value, 1.0));
    case CSSPrimitiveValue::ValueRange::kNonNegative:
      return std::max(value, 0.0);
    case CSSPrimitiveValue::ValueRange::kAll:
      return value;
  }
}

bool CSSMathFunctionValue::IsPx() const {
  // TODO(crbug.com/979895): This is the result of refactoring, which might be
  // an existing bug. Fix it if necessary.
  return Category() == kCalcLength;
}

bool CSSMathFunctionValue::IsComputationallyIndependent() const {
  return expression_->IsComputationallyIndependent();
}

bool CSSMathFunctionValue::IsElementDependent() const {
  return expression_->IsElementDependent();
}

const CalculationValue* CSSMathFunctionValue::ToCalcValue(
    const CSSLengthResolver& length_resolver) const {
  DCHECK_NE(value_range_in_target_context_,
            CSSPrimitiveValue::ValueRange::kInteger);
  DCHECK_NE(value_range_in_target_context_,
            CSSPrimitiveValue::ValueRange::kNonNegativeInteger);
  DCHECK_NE(value_range_in_target_context_,
            CSSPrimitiveValue::ValueRange::kPositiveInteger);
  return expression_->ToCalcValue(
      length_resolver,
      CSSPrimitiveValue::ConversionToLengthValueRange(PermittedValueRange()),
      AllowsNegativePercentageReference());
}

const CSSValue& CSSMathFunctionValue::PopulateWithTreeScope(
    const TreeScope* tree_scope) const {
  return *MakeGarbageCollected<CSSMathFunctionValue>(
      &expression_->PopulateWithTreeScope(tree_scope),
      value_range_in_target_context_);
}

const CSSMathFunctionValue* CSSMathFunctionValue::TransformAnchors(
    LogicalAxis logical_axis,
    const TryTacticTransform& transform,
    const WritingDirectionMode& writing_direction) const {
  const CSSMathExpressionNode* transformed =
      expression_->TransformAnchors(logical_axis, transform, writing_direction);
  if (transformed != expression_) {
    return MakeGarbageCollected<CSSMathFunctionValue>(
        transformed, value_range_in_target_context_);
  }
  return this;
}

const CSSValue*
CSSMathFunctionValue::CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
    const CSSPropertyName& property_name,
    wtf_size_t property_value_index) const {
  if (expression_ && expression_->NeedsPropertyNameAndValueIndexForRandom()) {
    return MakeGarbageCollected<CSSMathFunctionValue>(
        expression_->CopyRandomWithPropertyNameAndValueIndexIfNeeded(
            property_name, property_value_index),
        value_range_in_target_context_);
  }
  return this;
}

}  // namespace blink
