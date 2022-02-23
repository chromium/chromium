/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_value_clamping_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static CalculationCategory UnitCategory(CSSPrimitiveValue::UnitType type) {
  switch (type) {
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kInteger:
      return kCalcNumber;
    case CSSPrimitiveValue::UnitType::kPercentage:
      return kCalcPercent;
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kExs:
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kCentimeters:
    case CSSPrimitiveValue::UnitType::kMillimeters:
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
    case CSSPrimitiveValue::UnitType::kInches:
    case CSSPrimitiveValue::UnitType::kPoints:
    case CSSPrimitiveValue::UnitType::kPicas:
    case CSSPrimitiveValue::UnitType::kUserUnits:
    case CSSPrimitiveValue::UnitType::kRems:
    case CSSPrimitiveValue::UnitType::kChs:
    case CSSPrimitiveValue::UnitType::kViewportWidth:
    case CSSPrimitiveValue::UnitType::kViewportHeight:
    case CSSPrimitiveValue::UnitType::kViewportMin:
    case CSSPrimitiveValue::UnitType::kViewportMax:
      return kCalcLength;
    case CSSPrimitiveValue::UnitType::kViewportInlineSize:
    case CSSPrimitiveValue::UnitType::kViewportBlockSize:
    case CSSPrimitiveValue::UnitType::kSmallViewportWidth:
    case CSSPrimitiveValue::UnitType::kSmallViewportHeight:
    case CSSPrimitiveValue::UnitType::kSmallViewportInlineSize:
    case CSSPrimitiveValue::UnitType::kSmallViewportBlockSize:
    case CSSPrimitiveValue::UnitType::kSmallViewportMin:
    case CSSPrimitiveValue::UnitType::kSmallViewportMax:
    case CSSPrimitiveValue::UnitType::kLargeViewportWidth:
    case CSSPrimitiveValue::UnitType::kLargeViewportHeight:
    case CSSPrimitiveValue::UnitType::kLargeViewportInlineSize:
    case CSSPrimitiveValue::UnitType::kLargeViewportBlockSize:
    case CSSPrimitiveValue::UnitType::kLargeViewportMin:
    case CSSPrimitiveValue::UnitType::kLargeViewportMax:
    case CSSPrimitiveValue::UnitType::kDynamicViewportWidth:
    case CSSPrimitiveValue::UnitType::kDynamicViewportHeight:
    case CSSPrimitiveValue::UnitType::kDynamicViewportInlineSize:
    case CSSPrimitiveValue::UnitType::kDynamicViewportBlockSize:
    case CSSPrimitiveValue::UnitType::kDynamicViewportMin:
    case CSSPrimitiveValue::UnitType::kDynamicViewportMax:
      return RuntimeEnabledFeatures::CSSViewportUnits4Enabled() ? kCalcLength
                                                                : kCalcOther;
    case CSSPrimitiveValue::UnitType::kContainerWidth:
    case CSSPrimitiveValue::UnitType::kContainerHeight:
    case CSSPrimitiveValue::UnitType::kContainerInlineSize:
    case CSSPrimitiveValue::UnitType::kContainerBlockSize:
    case CSSPrimitiveValue::UnitType::kContainerMin:
    case CSSPrimitiveValue::UnitType::kContainerMax:
      return RuntimeEnabledFeatures::CSSContainerRelativeUnitsEnabled()
                 ? kCalcLength
                 : kCalcOther;
    case CSSPrimitiveValue::UnitType::kDegrees:
    case CSSPrimitiveValue::UnitType::kGradians:
    case CSSPrimitiveValue::UnitType::kRadians:
    case CSSPrimitiveValue::UnitType::kTurns:
      return kCalcAngle;
    case CSSPrimitiveValue::UnitType::kMilliseconds:
    case CSSPrimitiveValue::UnitType::kSeconds:
      return kCalcTime;
    case CSSPrimitiveValue::UnitType::kHertz:
    case CSSPrimitiveValue::UnitType::kKilohertz:
      return kCalcFrequency;
    default:
      return kCalcOther;
  }
}

static bool HasDoubleValue(CSSPrimitiveValue::UnitType type) {
  switch (type) {
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kPercentage:
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kExs:
    case CSSPrimitiveValue::UnitType::kChs:
    case CSSPrimitiveValue::UnitType::kRems:
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kCentimeters:
    case CSSPrimitiveValue::UnitType::kMillimeters:
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
    case CSSPrimitiveValue::UnitType::kInches:
    case CSSPrimitiveValue::UnitType::kPoints:
    case CSSPrimitiveValue::UnitType::kPicas:
    case CSSPrimitiveValue::UnitType::kUserUnits:
    case CSSPrimitiveValue::UnitType::kDegrees:
    case CSSPrimitiveValue::UnitType::kRadians:
    case CSSPrimitiveValue::UnitType::kGradians:
    case CSSPrimitiveValue::UnitType::kTurns:
    case CSSPrimitiveValue::UnitType::kMilliseconds:
    case CSSPrimitiveValue::UnitType::kSeconds:
    case CSSPrimitiveValue::UnitType::kHertz:
    case CSSPrimitiveValue::UnitType::kKilohertz:
    case CSSPrimitiveValue::UnitType::kViewportWidth:
    case CSSPrimitiveValue::UnitType::kViewportHeight:
    case CSSPrimitiveValue::UnitType::kViewportMin:
    case CSSPrimitiveValue::UnitType::kViewportMax:
    case CSSPrimitiveValue::UnitType::kContainerWidth:
    case CSSPrimitiveValue::UnitType::kContainerHeight:
    case CSSPrimitiveValue::UnitType::kContainerInlineSize:
    case CSSPrimitiveValue::UnitType::kContainerBlockSize:
    case CSSPrimitiveValue::UnitType::kContainerMin:
    case CSSPrimitiveValue::UnitType::kContainerMax:
    case CSSPrimitiveValue::UnitType::kDotsPerPixel:
    case CSSPrimitiveValue::UnitType::kDotsPerInch:
    case CSSPrimitiveValue::UnitType::kDotsPerCentimeter:
    case CSSPrimitiveValue::UnitType::kFraction:
    case CSSPrimitiveValue::UnitType::kInteger:
      return true;
    default:
      return false;
  }
}

namespace {

const PixelsAndPercent CreateClampedSamePixelsAndPercent(float value) {
  return PixelsAndPercent(CSSValueClampingUtils::ClampLength(value),
                          CSSValueClampingUtils::ClampLength(value));
}

bool IsNaN(PixelsAndPercent value, bool allows_negative_percentage_reference) {
  if (std::isnan(value.pixels + value.percent) ||
      (allows_negative_percentage_reference && std::isinf(value.percent))) {
    return true;
  }
  return false;
}

absl::optional<PixelsAndPercent> EvaluateValueIfNaNorInfinity(
    scoped_refptr<const blink::CalculationExpressionNode> value,
    bool allows_negative_percentage_reference) {
  float evaluated_value = value->Evaluate(1);
  if (std::isnan(evaluated_value) || std::isinf(evaluated_value)) {
    return CreateClampedSamePixelsAndPercent(evaluated_value);
  }
  if (allows_negative_percentage_reference) {
    evaluated_value = value->Evaluate(-1);
    if (std::isnan(evaluated_value) || std::isinf(evaluated_value)) {
      return CreateClampedSamePixelsAndPercent(evaluated_value);
    }
  }
  return absl::nullopt;
}

}  // namespace

// ------ Start of CSSMathExpressionNumericLiteral member functions ------

// static
CSSMathExpressionNumericLiteral* CSSMathExpressionNumericLiteral::Create(
    const CSSNumericLiteralValue* value) {
  return MakeGarbageCollected<CSSMathExpressionNumericLiteral>(value);
}

// static
CSSMathExpressionNumericLiteral* CSSMathExpressionNumericLiteral::Create(
    double value,
    CSSPrimitiveValue::UnitType type) {
  if (!RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled() &&
      (std::isnan(value) || std::isinf(value)))
    return nullptr;
  return MakeGarbageCollected<CSSMathExpressionNumericLiteral>(
      CSSNumericLiteralValue::Create(value, type));
}

CSSMathExpressionNumericLiteral::CSSMathExpressionNumericLiteral(
    const CSSNumericLiteralValue* value)
    : CSSMathExpressionNode(UnitCategory(value->GetType()),
                            false /* has_comparisons*/),
      value_(value) {}

bool CSSMathExpressionNumericLiteral::IsZero() const {
  return !value_->GetDoubleValue();
}

String CSSMathExpressionNumericLiteral::CustomCSSText() const {
  return value_->CssText();
}

absl::optional<PixelsAndPercent>
CSSMathExpressionNumericLiteral::ToPixelsAndPercent(
    const CSSToLengthConversionData& conversion_data) const {
  PixelsAndPercent value(0, 0);
  switch (category_) {
    case kCalcLength:
      // When CSSCalcInfinityAndNaN is enabled, we allow infinity and NaN in
      // PixelsAndPercent. Therefore, we need to use a function that doesn't
      // internally clamp the result to the float range.
      if (RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled())
        value.pixels = value_->ComputeLengthPx(conversion_data);
      else
        value.pixels = value_->ComputeLength<float>(conversion_data);
      break;
    case kCalcPercent:
      DCHECK(value_->IsPercentage());
      // When CSSCalcInfinityAndNaN is enabled, we allow infinity and NaN in
      // PixelsAndPercent. Therefore, we need to use a function that doesn't
      // internally clamp the result to the float range.
      if (RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled())
        value.percent = value_->GetDoubleValueWithoutClamping();
      else
        value.percent = value_->GetFloatValue();
      break;
    case kCalcNumber:
      // TODO(alancutter): Stop treating numbers like pixels unconditionally
      // in calcs to be able to accomodate border-image-width
      // https://drafts.csswg.org/css-backgrounds-3/#the-border-image-width
      value.pixels = value_->GetFloatValue() * conversion_data.Zoom();
      break;
    default:
      NOTREACHED();
  }
  return value;
}

scoped_refptr<const CalculationExpressionNode>
CSSMathExpressionNumericLiteral::ToCalculationExpression(
    const CSSToLengthConversionData& conversion_data) const {
  return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
      *ToPixelsAndPercent(conversion_data));
}

double CSSMathExpressionNumericLiteral::DoubleValue() const {
  if (HasDoubleValue(ResolvedUnitType()))
    return value_->GetDoubleValueWithoutClamping();
  NOTREACHED();
  return 0;
}

absl::optional<double>
CSSMathExpressionNumericLiteral::ComputeValueInCanonicalUnit() const {
  switch (category_) {
    case kCalcNumber:
    case kCalcPercent:
      return value_->DoubleValue();
    case kCalcLength:
      if (CSSPrimitiveValue::IsRelativeUnit(value_->GetType()))
        return absl::nullopt;
      U_FALLTHROUGH;
    case kCalcAngle:
    case kCalcTime:
    case kCalcFrequency:
      return value_->DoubleValue() *
             CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                 value_->GetType());
    default:
      return absl::nullopt;
  }
}

double CSSMathExpressionNumericLiteral::ComputeLengthPx(
    const CSSToLengthConversionData& conversion_data) const {
  switch (category_) {
    case kCalcLength:
      // When CSSCalcInfinityAndNaN is enabled, we allow infinity and NaN in
      // PixelsAndPercent. Therefore, we need to use a function that doesn't
      // internally clamp the result to the float range.
      if (RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled())
        return value_->ComputeLengthPx(conversion_data);
      return value_->ComputeLength<double>(conversion_data);
    case kCalcNumber:
    case kCalcPercent:
    case kCalcAngle:
    case kCalcFrequency:
    case kCalcPercentLength:
    case kCalcTime:
    case kCalcOther:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return 0;
}

bool CSSMathExpressionNumericLiteral::AccumulateLengthArray(
    CSSLengthArray& length_array,
    double multiplier) const {
  DCHECK_NE(Category(), kCalcNumber);
  return value_->AccumulateLengthArray(length_array, multiplier);
}

void CSSMathExpressionNumericLiteral::AccumulateLengthUnitTypes(
    CSSPrimitiveValue::LengthTypeFlags& types) const {
  value_->AccumulateLengthUnitTypes(types);
}

bool CSSMathExpressionNumericLiteral::operator==(
    const CSSMathExpressionNode& other) const {
  if (!other.IsNumericLiteral())
    return false;

  return base::ValuesEquivalent(
      value_, To<CSSMathExpressionNumericLiteral>(other).value_);
}

CSSPrimitiveValue::UnitType CSSMathExpressionNumericLiteral::ResolvedUnitType()
    const {
  return value_->GetType();
}

bool CSSMathExpressionNumericLiteral::IsComputationallyIndependent() const {
  return value_->IsComputationallyIndependent();
}

void CSSMathExpressionNumericLiteral::Trace(Visitor* visitor) const {
  visitor->Trace(value_);
  CSSMathExpressionNode::Trace(visitor);
}

#if DCHECK_IS_ON()
bool CSSMathExpressionNumericLiteral::InvolvesPercentageComparisons() const {
  return false;
}
#endif

// ------ End of CSSMathExpressionNumericLiteral member functions

static const CalculationCategory kAddSubtractResult[kCalcOther][kCalcOther] = {
    /* CalcNumber */ {kCalcNumber, kCalcOther, kCalcOther, kCalcOther,
                      kCalcOther, kCalcOther, kCalcOther},
    /* CalcLength */
    {kCalcOther, kCalcLength, kCalcPercentLength, kCalcPercentLength,
     kCalcOther, kCalcOther, kCalcOther},
    /* CalcPercent */
    {kCalcOther, kCalcPercentLength, kCalcPercent, kCalcPercentLength,
     kCalcOther, kCalcOther, kCalcOther},
    /* CalcPercentLength */
    {kCalcOther, kCalcPercentLength, kCalcPercentLength, kCalcPercentLength,
     kCalcOther, kCalcOther, kCalcOther},
    /* CalcAngle  */
    {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcAngle, kCalcOther,
     kCalcOther},
    /* CalcTime */
    {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcTime,
     kCalcOther},
    /* CalcFrequency */
    {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
     kCalcFrequency}};

static CalculationCategory DetermineCategory(
    const CSSMathExpressionNode& left_side,
    const CSSMathExpressionNode& right_side,
    CSSMathOperator op) {
  CalculationCategory left_category = left_side.Category();
  CalculationCategory right_category = right_side.Category();

  if (left_category == kCalcOther || right_category == kCalcOther)
    return kCalcOther;

  switch (op) {
    case CSSMathOperator::kAdd:
    case CSSMathOperator::kSubtract:
      return kAddSubtractResult[left_category][right_category];
    case CSSMathOperator::kMultiply:
      if (left_category != kCalcNumber && right_category != kCalcNumber)
        return kCalcOther;
      return left_category == kCalcNumber ? right_category : left_category;
    case CSSMathOperator::kDivide:
      if (right_category != kCalcNumber ||
          (!RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled() &&
           right_side.IsZero()))
        return kCalcOther;
      return left_category;
    default:
      break;
  }

  NOTREACHED();
  return kCalcOther;
}

// ------ Start of CSSMathExpressionOperation member functions ------

// static
CSSMathExpressionNode* CSSMathExpressionOperation::CreateArithmeticOperation(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  DCHECK_NE(left_side->Category(), kCalcOther);
  DCHECK_NE(right_side->Category(), kCalcOther);

  CalculationCategory new_category =
      DetermineCategory(*left_side, *right_side, op);
  if (new_category == kCalcOther)
    return nullptr;

  return MakeGarbageCollected<CSSMathExpressionOperation>(left_side, right_side,
                                                          op, new_category);
}

// static
CSSMathExpressionNode* CSSMathExpressionOperation::CreateComparisonFunction(
    Operands&& operands,
    CSSMathOperator op) {
  DCHECK(op == CSSMathOperator::kMin || op == CSSMathOperator::kMax ||
         op == CSSMathOperator::kClamp);
  DCHECK(operands.size());
  bool is_first = true;
  CalculationCategory category;
  for (const CSSMathExpressionNode* operand : operands) {
    if (is_first)
      category = operand->Category();
    else
      category = kAddSubtractResult[category][operand->Category()];

    is_first = false;
    if (category == kCalcOther)
      return nullptr;
  }
  return MakeGarbageCollected<CSSMathExpressionOperation>(
      category, std::move(operands), op);
}

// static
CSSMathExpressionNode*
CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  if (left_side->IsMathFunction() || right_side->IsMathFunction())
    return CreateArithmeticOperation(left_side, right_side, op);

  CalculationCategory left_category = left_side->Category();
  CalculationCategory right_category = right_side->Category();
  DCHECK_NE(left_category, kCalcOther);
  DCHECK_NE(right_category, kCalcOther);

  // Simplify numbers.
  if (left_category == kCalcNumber && right_category == kCalcNumber) {
    return CSSMathExpressionNumericLiteral::Create(
        EvaluateOperator({left_side->DoubleValue(), right_side->DoubleValue()},
                         op),
        CSSPrimitiveValue::UnitType::kNumber);
  }

  // Simplify addition and subtraction between same types.
  if (op == CSSMathOperator::kAdd || op == CSSMathOperator::kSubtract) {
    if (left_category == right_side->Category()) {
      CSSPrimitiveValue::UnitType left_type = left_side->ResolvedUnitType();
      if (HasDoubleValue(left_type)) {
        CSSPrimitiveValue::UnitType right_type = right_side->ResolvedUnitType();
        if (left_type == right_type) {
          return CSSMathExpressionNumericLiteral::Create(
              EvaluateOperator(
                  {left_side->DoubleValue(), right_side->DoubleValue()}, op),
              left_type);
        }
        CSSPrimitiveValue::UnitCategory left_unit_category =
            CSSPrimitiveValue::UnitTypeToUnitCategory(left_type);
        if (left_unit_category != CSSPrimitiveValue::kUOther &&
            left_unit_category ==
                CSSPrimitiveValue::UnitTypeToUnitCategory(right_type)) {
          CSSPrimitiveValue::UnitType canonical_type =
              CSSPrimitiveValue::CanonicalUnitTypeForCategory(
                  left_unit_category);
          if (canonical_type != CSSPrimitiveValue::UnitType::kUnknown) {
            double left_value = ClampTo<double>(
                left_side->DoubleValue() *
                CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                    left_type));
            double right_value = ClampTo<double>(
                right_side->DoubleValue() *
                CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                    right_type));
            return CSSMathExpressionNumericLiteral::Create(
                EvaluateOperator({left_value, right_value}, op),
                canonical_type);
          }
        }
      }
    }
  } else {
    // Simplify multiplying or dividing by a number for simplifiable types.
    DCHECK(op == CSSMathOperator::kMultiply || op == CSSMathOperator::kDivide);
    const CSSMathExpressionNode* number_side =
        GetNumberSide(left_side, right_side);
    if (!number_side)
      return CreateArithmeticOperation(left_side, right_side, op);
    if (number_side == left_side && op == CSSMathOperator::kDivide)
      return nullptr;
    const CSSMathExpressionNode* other_side =
        left_side == number_side ? right_side : left_side;

    double number = number_side->DoubleValue();

    if (!RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled()) {
      if (std::isnan(number) || std::isinf(number))
        return nullptr;
      if (op == CSSMathOperator::kDivide && !number)
        return nullptr;
    }

    CSSPrimitiveValue::UnitType other_type = other_side->ResolvedUnitType();
    if (HasDoubleValue(other_type)) {
      return CSSMathExpressionNumericLiteral::Create(
          EvaluateOperator({other_side->DoubleValue(), number}, op),
          other_type);
    }
  }

  return CreateArithmeticOperation(left_side, right_side, op);
}

CSSMathExpressionOperation::CSSMathExpressionOperation(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op,
    CalculationCategory category)
    : CSSMathExpressionNode(
          category,
          left_side->HasComparisons() || right_side->HasComparisons()),
      operands_({left_side, right_side}),
      operator_(op) {}

static bool AnyOperandHasComparisons(
    CSSMathExpressionOperation::Operands& operands) {
  for (const CSSMathExpressionNode* operand : operands) {
    if (operand->HasComparisons())
      return true;
  }
  return false;
}

CSSMathExpressionOperation::CSSMathExpressionOperation(
    CalculationCategory category,
    Operands&& operands,
    CSSMathOperator op)
    : CSSMathExpressionNode(
          category,
          IsComparison(op) || AnyOperandHasComparisons(operands)),
      operands_(std::move(operands)),
      operator_(op) {}

bool CSSMathExpressionOperation::IsZero() const {
  absl::optional<double> maybe_value = ComputeValueInCanonicalUnit();
  return maybe_value && !*maybe_value;
}

absl::optional<PixelsAndPercent> CSSMathExpressionOperation::ToPixelsAndPercent(
    const CSSToLengthConversionData& conversion_data) const {
  absl::optional<PixelsAndPercent> result;
  switch (operator_) {
    case CSSMathOperator::kAdd:
    case CSSMathOperator::kSubtract: {
      DCHECK_EQ(operands_.size(), 2u);
      result = operands_[0]->ToPixelsAndPercent(conversion_data);
      if (!result)
        return absl::nullopt;

      absl::optional<PixelsAndPercent> other_side =
          operands_[1]->ToPixelsAndPercent(conversion_data);
      if (!other_side)
        return absl::nullopt;
      if (operator_ == CSSMathOperator::kAdd) {
        result->pixels += other_side->pixels;
        result->percent += other_side->percent;
      } else {
        result->pixels -= other_side->pixels;
        result->percent -= other_side->percent;
      }
      break;
    }
    case CSSMathOperator::kMultiply:
    case CSSMathOperator::kDivide: {
      DCHECK_EQ(operands_.size(), 2u);
      const CSSMathExpressionNode* number_side =
          GetNumberSide(operands_[0], operands_[1]);
      const CSSMathExpressionNode* other_side =
          operands_[0] == number_side ? operands_[1] : operands_[0];
      result = other_side->ToPixelsAndPercent(conversion_data);
      if (!result)
        return absl::nullopt;
      float number = number_side->DoubleValue();
      if (operator_ == CSSMathOperator::kDivide)
        number = 1.0 / number;
      result->pixels *= number;
      result->percent *= number;
      break;
    }
    case CSSMathOperator::kMin:
    case CSSMathOperator::kMax:
    case CSSMathOperator::kClamp:
      return absl::nullopt;
    case CSSMathOperator::kInvalid:
      NOTREACHED();
  }
  return result;
}

scoped_refptr<const CalculationExpressionNode>
CSSMathExpressionOperation::ToCalculationExpression(
    const CSSToLengthConversionData& conversion_data) const {
  switch (operator_) {
    case CSSMathOperator::kAdd:
      DCHECK_EQ(operands_.size(), 2u);
      return CalculationExpressionOperationNode::CreateSimplified(
          CalculationExpressionOperationNode::Children(
              {operands_[0]->ToCalculationExpression(conversion_data),
               operands_[1]->ToCalculationExpression(conversion_data)}),
          CalculationOperator::kAdd);
    case CSSMathOperator::kSubtract:
      DCHECK_EQ(operands_.size(), 2u);
      return CalculationExpressionOperationNode::CreateSimplified(
          CalculationExpressionOperationNode::Children(
              {operands_[0]->ToCalculationExpression(conversion_data),
               operands_[1]->ToCalculationExpression(conversion_data)}),
          CalculationOperator::kSubtract);
    case CSSMathOperator::kMultiply:
      DCHECK_EQ(operands_.size(), 2u);
      DCHECK_NE((operands_[0]->Category() == kCalcNumber),
                (operands_[1]->Category() == kCalcNumber));
      if (operands_[0]->Category() == kCalcNumber) {
        return CalculationExpressionOperationNode::CreateSimplified(
            CalculationExpressionOperationNode::Children(
                {operands_[1]->ToCalculationExpression(conversion_data),
                 base::MakeRefCounted<CalculationExpressionNumberNode>(
                     operands_[0]->DoubleValue())}),
            CalculationOperator::kMultiply);
      }
      return CalculationExpressionOperationNode::CreateSimplified(
          CalculationExpressionOperationNode::Children(
              {operands_[0]->ToCalculationExpression(conversion_data),
               base::MakeRefCounted<CalculationExpressionNumberNode>(
                   operands_[1]->DoubleValue())}),
          CalculationOperator::kMultiply);
    case CSSMathOperator::kDivide:
      DCHECK_EQ(operands_.size(), 2u);
      DCHECK_EQ(operands_[1]->Category(), kCalcNumber);
      return CalculationExpressionOperationNode::CreateSimplified(
          CalculationExpressionOperationNode::Children(
              {operands_[0]->ToCalculationExpression(conversion_data),
               base::MakeRefCounted<CalculationExpressionNumberNode>(
                   1.0 / operands_[1]->DoubleValue())}),
          CalculationOperator::kMultiply);
    case CSSMathOperator::kMin:
    case CSSMathOperator::kMax: {
      Vector<scoped_refptr<const CalculationExpressionNode>> operands;
      operands.ReserveCapacity(operands_.size());
      for (const CSSMathExpressionNode* operand : operands_)
        operands.push_back(operand->ToCalculationExpression(conversion_data));
      auto expression_operator = operator_ == CSSMathOperator::kMin
                                     ? CalculationOperator::kMin
                                     : CalculationOperator::kMax;
      return CalculationExpressionOperationNode::CreateSimplified(
          std::move(operands), expression_operator);
    }
    case CSSMathOperator::kClamp: {
      Vector<scoped_refptr<const CalculationExpressionNode>> operands;
      operands.ReserveCapacity(operands_.size());
      for (const CSSMathExpressionNode* operand : operands_)
        operands.push_back(operand->ToCalculationExpression(conversion_data));
      return CalculationExpressionOperationNode::CreateSimplified(
          std::move(operands), CalculationOperator::kClamp);
    }
    case CSSMathOperator::kInvalid:
      NOTREACHED();
      return nullptr;
  }
}

double CSSMathExpressionOperation::DoubleValue() const {
  DCHECK(HasDoubleValue(ResolvedUnitType())) << CustomCSSText();
  Vector<double> double_values;
  double_values.ReserveCapacity(operands_.size());
  for (const CSSMathExpressionNode* operand : operands_)
    double_values.push_back(operand->DoubleValue());
  return Evaluate(double_values);
}

static bool HasCanonicalUnit(CalculationCategory category) {
  return category == kCalcNumber || category == kCalcLength ||
         category == kCalcPercent || category == kCalcAngle ||
         category == kCalcTime || category == kCalcFrequency;
}

absl::optional<double> CSSMathExpressionOperation::ComputeValueInCanonicalUnit()
    const {
  if (!HasCanonicalUnit(category_))
    return absl::nullopt;

  Vector<double> double_values;
  double_values.ReserveCapacity(operands_.size());
  for (const CSSMathExpressionNode* operand : operands_) {
    absl::optional<double> maybe_value = operand->ComputeValueInCanonicalUnit();
    if (!maybe_value)
      return absl::nullopt;
    double_values.push_back(*maybe_value);
  }
  return Evaluate(double_values);
}

double CSSMathExpressionOperation::ComputeLengthPx(
    const CSSToLengthConversionData& data) const {
  DCHECK_EQ(kCalcLength, Category());
  Vector<double> double_values;
  double_values.ReserveCapacity(operands_.size());
  for (const CSSMathExpressionNode* operand : operands_) {
    if (operand->Category() == kCalcLength) {
      double_values.push_back(operand->ComputeLengthPx(data));
    } else {
      DCHECK_EQ(operand->Category(), kCalcNumber);
      double_values.push_back(operand->DoubleValue());
    }
  }
  return Evaluate(double_values);
}

bool CSSMathExpressionOperation::AccumulateLengthArray(
    CSSLengthArray& length_array,
    double multiplier) const {
  switch (operator_) {
    case CSSMathOperator::kAdd:
      DCHECK_EQ(operands_.size(), 2u);
      if (!operands_[0]->AccumulateLengthArray(length_array, multiplier))
        return false;
      if (!operands_[1]->AccumulateLengthArray(length_array, multiplier))
        return false;
      return true;
    case CSSMathOperator::kSubtract:
      DCHECK_EQ(operands_.size(), 2u);
      if (!operands_[0]->AccumulateLengthArray(length_array, multiplier))
        return false;
      if (!operands_[1]->AccumulateLengthArray(length_array, -multiplier))
        return false;
      return true;
    case CSSMathOperator::kMultiply:
      DCHECK_EQ(operands_.size(), 2u);
      DCHECK_NE((operands_[0]->Category() == kCalcNumber),
                (operands_[1]->Category() == kCalcNumber));
      if (operands_[0]->Category() == kCalcNumber) {
        return operands_[1]->AccumulateLengthArray(
            length_array, multiplier * operands_[0]->DoubleValue());
      } else {
        return operands_[0]->AccumulateLengthArray(
            length_array, multiplier * operands_[1]->DoubleValue());
      }
    case CSSMathOperator::kDivide:
      DCHECK_EQ(operands_.size(), 2u);
      DCHECK_EQ(operands_[1]->Category(), kCalcNumber);
      return operands_[0]->AccumulateLengthArray(
          length_array, multiplier / operands_[1]->DoubleValue());
    case CSSMathOperator::kMin:
    case CSSMathOperator::kMax:
    case CSSMathOperator::kClamp:
      // When comparison functions are involved, we can't resolve the expression
      // into a length array.
      return false;
    case CSSMathOperator::kInvalid:
      NOTREACHED();
      return false;
  }
}

void CSSMathExpressionOperation::AccumulateLengthUnitTypes(
    CSSPrimitiveValue::LengthTypeFlags& types) const {
  for (const CSSMathExpressionNode* operand : operands_)
    operand->AccumulateLengthUnitTypes(types);
}

bool CSSMathExpressionOperation::IsComputationallyIndependent() const {
  if (Category() != kCalcLength && Category() != kCalcPercentLength)
    return true;
  for (const CSSMathExpressionNode* operand : operands_) {
    if (!operand->IsComputationallyIndependent())
      return false;
  }
  return true;
}

String CSSMathExpressionOperation::CustomCSSText() const {
  switch (operator_) {
    case CSSMathOperator::kAdd:
    case CSSMathOperator::kSubtract:
    case CSSMathOperator::kMultiply:
    case CSSMathOperator::kDivide: {
      DCHECK_EQ(operands_.size(), 2u);
      StringBuilder result;

      const bool left_side_needs_parentheses =
          (operands_[0]->IsOperation() && !operands_[0]->IsMathFunction()) &&
          operator_ != CSSMathOperator::kAdd;
      if (left_side_needs_parentheses)
        result.Append('(');
      result.Append(operands_[0]->CustomCSSText());
      if (left_side_needs_parentheses)
        result.Append(')');

      result.Append(' ');
      result.Append(ToString(operator_));
      result.Append(' ');

      const bool right_side_needs_parentheses =
          (operands_[1]->IsOperation() && !operands_[1]->IsMathFunction()) &&
          operator_ != CSSMathOperator::kAdd;
      if (right_side_needs_parentheses)
        result.Append('(');
      result.Append(operands_[1]->CustomCSSText());
      if (right_side_needs_parentheses)
        result.Append(')');

      return result.ReleaseString();
    }
    case CSSMathOperator::kMin:
    case CSSMathOperator::kMax:
    case CSSMathOperator::kClamp: {
      StringBuilder result;
      result.Append(ToString(operator_));
      result.Append('(');
      result.Append(operands_.front()->CustomCSSText());
      for (const CSSMathExpressionNode* operand : SecondToLastOperands()) {
        result.Append(", ");
        result.Append(operand->CustomCSSText());
      }
      result.Append(')');

      return result.ReleaseString();
    }
    case CSSMathOperator::kInvalid:
      NOTREACHED();
      return String();
  }
}

bool CSSMathExpressionOperation::operator==(
    const CSSMathExpressionNode& exp) const {
  if (!exp.IsOperation())
    return false;

  const CSSMathExpressionOperation& other = To<CSSMathExpressionOperation>(exp);
  if (operator_ != other.operator_)
    return false;
  if (operands_.size() != other.operands_.size())
    return false;
  for (wtf_size_t i = 0; i < operands_.size(); ++i) {
    if (!base::ValuesEquivalent(operands_[i], other.operands_[i]))
      return false;
  }
  return true;
}

CSSPrimitiveValue::UnitType CSSMathExpressionOperation::ResolvedUnitType()
    const {
  switch (category_) {
    case kCalcNumber:
      return CSSPrimitiveValue::UnitType::kNumber;
    case kCalcAngle:
    case kCalcTime:
    case kCalcFrequency:
    case kCalcLength:
    case kCalcPercent:
      switch (operator_) {
        case CSSMathOperator::kMultiply:
        case CSSMathOperator::kDivide: {
          DCHECK_EQ(operands_.size(), 2u);
          if (operands_[0]->Category() == kCalcNumber)
            return operands_[1]->ResolvedUnitType();
          if (operands_[1]->Category() == kCalcNumber)
            return operands_[0]->ResolvedUnitType();
          NOTREACHED();
          return CSSPrimitiveValue::UnitType::kUnknown;
        }
        case CSSMathOperator::kAdd:
        case CSSMathOperator::kSubtract:
        case CSSMathOperator::kMin:
        case CSSMathOperator::kMax:
        case CSSMathOperator::kClamp: {
          CSSPrimitiveValue::UnitType first_type =
              operands_.front()->ResolvedUnitType();
          if (first_type == CSSPrimitiveValue::UnitType::kUnknown)
            return CSSPrimitiveValue::UnitType::kUnknown;
          for (const CSSMathExpressionNode* operand : SecondToLastOperands()) {
            CSSPrimitiveValue::UnitType next = operand->ResolvedUnitType();
            if (next == CSSPrimitiveValue::UnitType::kUnknown ||
                next != first_type)
              return CSSPrimitiveValue::UnitType::kUnknown;
          }
          return first_type;
        }
        case CSSMathOperator::kInvalid:
          NOTREACHED();
          return CSSPrimitiveValue::UnitType::kUnknown;
      }
    case kCalcPercentLength:
    case kCalcOther:
      return CSSPrimitiveValue::UnitType::kUnknown;
  }

  NOTREACHED();
  return CSSPrimitiveValue::UnitType::kUnknown;
}

void CSSMathExpressionOperation::Trace(Visitor* visitor) const {
  visitor->Trace(operands_);
  CSSMathExpressionNode::Trace(visitor);
}

// static
const CSSMathExpressionNode* CSSMathExpressionOperation::GetNumberSide(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side) {
  if (left_side->Category() == kCalcNumber)
    return left_side;
  if (right_side->Category() == kCalcNumber)
    return right_side;
  return nullptr;
}

// static
double CSSMathExpressionOperation::EvaluateOperator(
    const Vector<double>& operands,
    CSSMathOperator op) {
  // Design doc for infinity and NaN: https://bit.ly/349gXjq
  switch (op) {
    case CSSMathOperator::kAdd:
      DCHECK_EQ(operands.size(), 2u);
      if (RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled())
        return operands[0] + operands[1];
      return ClampTo<double>(operands[0] + operands[1]);
    case CSSMathOperator::kSubtract:
      DCHECK_EQ(operands.size(), 2u);
      if (RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled())
        return operands[0] - operands[1];
      return ClampTo<double>(operands[0] - operands[1]);
    case CSSMathOperator::kMultiply:
      DCHECK_EQ(operands.size(), 2u);
      if (RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled())
        return operands[0] * operands[1];
      return ClampTo<double>(operands[0] * operands[1]);
    case CSSMathOperator::kDivide:
      DCHECK(operands.size() == 1u || operands.size() == 2u);
      if (RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled())
        return operands[0] / operands[1];
      if (operands[1])
        return ClampTo<double>(operands[0] / operands[1]);
      return std::numeric_limits<double>::quiet_NaN();
    case CSSMathOperator::kMin: {
      if (operands.IsEmpty())
        return std::numeric_limits<double>::quiet_NaN();
      double minimum = operands[0];
      for (double operand : operands)
        minimum = std::min(minimum, operand);
      return minimum;
    }
    case CSSMathOperator::kMax: {
      if (operands.IsEmpty())
        return std::numeric_limits<double>::quiet_NaN();
      double maximum = operands[0];
      for (double operand : operands)
        maximum = std::max(maximum, operand);
      return maximum;
    }
    case CSSMathOperator::kClamp: {
      DCHECK_EQ(operands.size(), 3u);
      double min = operands[0];
      double val = operands[1];
      double max = operands[2];
      // clamp(MIN, VAL, MAX) is identical to max(MIN, min(VAL, MAX))
      // according to the spec,
      // https://drafts.csswg.org/css-values-4/#funcdef-clamp.
      return std::max(min, std::min(val, max));
    }
    case CSSMathOperator::kInvalid:
      NOTREACHED();
      break;
  }
  return 0;
}

#if DCHECK_IS_ON()
bool CSSMathExpressionOperation::InvolvesPercentageComparisons() const {
  if (IsMinOrMax() && Category() == kCalcPercent && operands_.size() > 1u)
    return true;
  for (const CSSMathExpressionNode* operand : operands_) {
    if (operand->InvolvesPercentageComparisons())
      return true;
  }
  return false;
}
#endif

// ------ End of CSSMathExpressionOperation member functions ------

class CSSMathExpressionNodeParser {
  STACK_ALLOCATED();

 public:
  CSSMathExpressionNodeParser() {}

  bool IsSupportedMathFunction(CSSValueID function_id) {
    switch (function_id) {
      case CSSValueID::kMin:
      case CSSValueID::kMax:
      case CSSValueID::kClamp:
        return true;
      // TODO(crbug.com/1284199): Support other math functions.
      default:
        return false;
    }
  }

  CSSMathExpressionNode* ParseMathFunction(CSSValueID function_id,
                                           CSSParserTokenRange& tokens,
                                           int depth) {
    // "arguments" refers to comma separated ones.
    wtf_size_t min_argument_count = 1;
    wtf_size_t max_argument_count = std::numeric_limits<wtf_size_t>::max();

    switch (function_id) {
      case CSSValueID::kCalc:
      case CSSValueID::kWebkitCalc:
        max_argument_count = 1;
        break;
      case CSSValueID::kMin:
      case CSSValueID::kMax:
        break;
      case CSSValueID::kClamp:
        min_argument_count = 3;
        max_argument_count = 3;
        break;
      // TODO(crbug.com/1284199): Support other math functions.
      default:
        break;
    }

    HeapVector<Member<const CSSMathExpressionNode>> nodes;

    while (!tokens.AtEnd() && nodes.size() < max_argument_count) {
      if (nodes.size()) {
        if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(tokens))
          return nullptr;
      }

      tokens.ConsumeWhitespace();
      CSSMathExpressionNode* node = ParseValueExpression(tokens, depth);
      if (!node)
        return nullptr;

      nodes.push_back(node);
    }

    if (!tokens.AtEnd() || nodes.size() < min_argument_count)
      return nullptr;

    switch (function_id) {
      case CSSValueID::kCalc:
      case CSSValueID::kWebkitCalc:
        return const_cast<CSSMathExpressionNode*>(nodes.front().Get());
      case CSSValueID::kMin:
        return CSSMathExpressionOperation::CreateComparisonFunction(
            std::move(nodes), CSSMathOperator::kMin);
      case CSSValueID::kMax:
        return CSSMathExpressionOperation::CreateComparisonFunction(
            std::move(nodes), CSSMathOperator::kMax);
      case CSSValueID::kClamp:
        return CSSMathExpressionOperation::CreateComparisonFunction(
            std::move(nodes), CSSMathOperator::kClamp);
      // TODO(crbug.com/1284199): Support other math functions.
      default:
        return nullptr;
    }
  }

 private:
  CSSMathExpressionNode* ParseValue(CSSParserTokenRange& tokens) {
    CSSParserToken token = tokens.ConsumeIncludingWhitespace();
    if (RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled()) {
      if (token.Id() == CSSValueID::kInfinity) {
        return CSSMathExpressionNumericLiteral::Create(
            std::numeric_limits<double>::infinity(),
            CSSPrimitiveValue::UnitType::kNumber);
      }
      if (token.Id() == CSSValueID::kNegativeInfinity) {
        return CSSMathExpressionNumericLiteral::Create(
            -std::numeric_limits<double>::infinity(),
            CSSPrimitiveValue::UnitType::kNumber);
      }
      if (token.Id() == CSSValueID::kNan) {
        return CSSMathExpressionNumericLiteral::Create(
            std::numeric_limits<double>::quiet_NaN(),
            CSSPrimitiveValue::UnitType::kNumber);
      }
    }
    if (!(token.GetType() == kNumberToken ||
          token.GetType() == kPercentageToken ||
          token.GetType() == kDimensionToken))
      return nullptr;

    CSSPrimitiveValue::UnitType type = token.GetUnitType();
    if (UnitCategory(type) == kCalcOther)
      return nullptr;

    return CSSMathExpressionNumericLiteral::Create(
        CSSNumericLiteralValue::Create(token.NumericValue(), type));
  }

  CSSMathExpressionNode* ParseValueTerm(CSSParserTokenRange& tokens,
                                        int depth) {
    if (tokens.AtEnd())
      return nullptr;

    if (tokens.Peek().GetType() == kLeftParenthesisToken ||
        tokens.Peek().FunctionId() == CSSValueID::kCalc) {
      CSSParserTokenRange inner_range = tokens.ConsumeBlock();
      tokens.ConsumeWhitespace();
      inner_range.ConsumeWhitespace();
      CSSMathExpressionNode* result = ParseValueExpression(inner_range, depth);
      if (!result)
        return nullptr;
      result->SetIsNestedCalc();
      return result;
    }

    if (tokens.Peek().GetType() == kFunctionToken) {
      CSSValueID function_id = tokens.Peek().FunctionId();
      CSSParserTokenRange inner_range = tokens.ConsumeBlock();
      tokens.ConsumeWhitespace();
      inner_range.ConsumeWhitespace();
      if (IsSupportedMathFunction(function_id))
        return ParseMathFunction(function_id, inner_range, depth);
    }

    return ParseValue(tokens);
  }

  CSSMathExpressionNode* ParseValueMultiplicativeExpression(
      CSSParserTokenRange& tokens,
      int depth) {
    if (tokens.AtEnd())
      return nullptr;

    CSSMathExpressionNode* result = ParseValueTerm(tokens, depth);
    if (!result)
      return nullptr;

    while (!tokens.AtEnd()) {
      CSSMathOperator math_operator = ParseCSSArithmeticOperator(tokens.Peek());
      if (math_operator != CSSMathOperator::kMultiply &&
          math_operator != CSSMathOperator::kDivide)
        break;
      tokens.ConsumeIncludingWhitespace();

      CSSMathExpressionNode* rhs = ParseValueTerm(tokens, depth);
      if (!rhs)
        return nullptr;

      result = CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
          result, rhs, math_operator);

      if (!result)
        return nullptr;
    }

    return result;
  }

  CSSMathExpressionNode* ParseAdditiveValueExpression(
      CSSParserTokenRange& tokens,
      int depth) {
    if (tokens.AtEnd())
      return nullptr;

    CSSMathExpressionNode* result =
        ParseValueMultiplicativeExpression(tokens, depth);
    if (!result)
      return nullptr;

    while (!tokens.AtEnd()) {
      CSSMathOperator math_operator = ParseCSSArithmeticOperator(tokens.Peek());
      if (math_operator != CSSMathOperator::kAdd &&
          math_operator != CSSMathOperator::kSubtract)
        break;
      if ((&tokens.Peek() - 1)->GetType() != kWhitespaceToken)
        return nullptr;  // calc(1px+ 2px) is invalid
      tokens.Consume();
      if (tokens.Peek().GetType() != kWhitespaceToken)
        return nullptr;  // calc(1px +2px) is invalid
      tokens.ConsumeIncludingWhitespace();

      CSSMathExpressionNode* rhs =
          ParseValueMultiplicativeExpression(tokens, depth);
      if (!rhs)
        return nullptr;

      result = CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
          result, rhs, math_operator);

      if (!result)
        return nullptr;
    }

    return result;
  }

  CSSMathExpressionNode* ParseValueExpression(CSSParserTokenRange& tokens,
                                              int depth) {
    if (++depth > kMaxExpressionDepth)
      return nullptr;
    return ParseAdditiveValueExpression(tokens, depth);
  }
};

scoped_refptr<const CalculationValue> CSSMathExpressionNode::ToCalcValue(
    const CSSToLengthConversionData& conversion_data,
    Length::ValueRange range,
    bool allows_negative_percentage_reference) const {
  if (auto maybe_pixels_and_percent = ToPixelsAndPercent(conversion_data)) {
    // Clamping if pixels + percent could result in NaN. In special case,
    // inf px + inf % could evaluate to nan when
    // allows_negative_percentage_reference is true.
    if (IsNaN(*maybe_pixels_and_percent,
              allows_negative_percentage_reference)) {
      maybe_pixels_and_percent = CreateClampedSamePixelsAndPercent(
          std::numeric_limits<float>::quiet_NaN());
    } else {
      maybe_pixels_and_percent->pixels =
          CSSValueClampingUtils::ClampLength(maybe_pixels_and_percent->pixels);
      maybe_pixels_and_percent->percent =
          CSSValueClampingUtils::ClampLength(maybe_pixels_and_percent->percent);
    }
    return CalculationValue::Create(*maybe_pixels_and_percent, range);
  }

  auto value = ToCalculationExpression(conversion_data);
  if (RuntimeEnabledFeatures::CSSCalcInfinityAndNaNEnabled()) {
    absl::optional<PixelsAndPercent> evaluated_value =
        EvaluateValueIfNaNorInfinity(value,
                                     allows_negative_percentage_reference);
    if (evaluated_value.has_value()) {
      return CalculationValue::Create(evaluated_value.value(), range);
    }
  }
  return CalculationValue::CreateSimplified(value, range);
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::Create(
    const CalculationValue& calc) {
  if (calc.IsExpression())
    return Create(*calc.GetOrCreateExpression());
  return Create(calc.GetPixelsAndPercent());
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::Create(PixelsAndPercent value) {
  double percent = value.percent;
  double pixels = value.pixels;
  CSSMathOperator op = CSSMathOperator::kAdd;
  if (pixels < 0) {
    pixels = -pixels;
    op = CSSMathOperator::kSubtract;
  }
  return CSSMathExpressionOperation::CreateArithmeticOperation(
      CSSMathExpressionNumericLiteral::Create(CSSNumericLiteralValue::Create(
          percent, CSSPrimitiveValue::UnitType::kPercentage)),
      CSSMathExpressionNumericLiteral::Create(CSSNumericLiteralValue::Create(
          pixels, CSSPrimitiveValue::UnitType::kPixels)),
      op);
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::Create(
    const CalculationExpressionNode& node) {
  if (node.IsPixelsAndPercent()) {
    const auto& pixels_and_percent =
        To<CalculationExpressionPixelsAndPercentNode>(node);
    return Create(pixels_and_percent.GetPixelsAndPercent());
  }

  DCHECK(node.IsOperation());

  const auto& operation = To<CalculationExpressionOperationNode>(node);
  const auto& children = operation.GetChildren();
  const auto calc_op = operation.GetOperator();
  switch (calc_op) {
    case CalculationOperator::kMultiply: {
      DCHECK_EQ(children.size(), 2u);
      auto& pixels_and_percent_node =
          children[0]->IsNumber() ? children[1] : children[0];
      auto& number_node = children[0]->IsNumber() ? children[0] : children[1];
      const auto& number = To<CalculationExpressionNumberNode>(*number_node);
      double number_value = number.Value();
      return CSSMathExpressionOperation::CreateArithmeticOperation(
          Create(*pixels_and_percent_node),
          CSSMathExpressionNumericLiteral::Create(
              CSSNumericLiteralValue::Create(
                  number_value, CSSPrimitiveValue::UnitType::kNumber)),
          CSSMathOperator::kMultiply);
    }
    case CalculationOperator::kAdd:
    case CalculationOperator::kSubtract: {
      DCHECK_EQ(children.size(), 2u);
      auto* lhs = Create(*children[0]);
      auto* rhs = Create(*children[1]);
      CSSMathOperator op = (calc_op == CalculationOperator::kAdd)
                               ? CSSMathOperator::kAdd
                               : CSSMathOperator::kSubtract;
      return CSSMathExpressionOperation::CreateArithmeticOperation(lhs, rhs,
                                                                   op);
    }
    case CalculationOperator::kMin:
    case CalculationOperator::kMax: {
      DCHECK(children.size());
      CSSMathExpressionOperation::Operands operands;
      for (const auto& child : children)
        operands.push_back(Create(*child));
      CSSMathOperator op = (calc_op == CalculationOperator::kMin)
                               ? CSSMathOperator::kMin
                               : CSSMathOperator::kMax;
      return CSSMathExpressionOperation::CreateComparisonFunction(
          std::move(operands), op);
    }
    case CalculationOperator::kClamp: {
      DCHECK_EQ(children.size(), 3u);
      CSSMathExpressionOperation::Operands operands;
      for (const auto& child : children)
        operands.push_back(Create(*child));
      return CSSMathExpressionOperation::CreateComparisonFunction(
          std::move(operands), CSSMathOperator::kClamp);
    }
    case CalculationOperator::kInvalid:
      NOTREACHED();
      return nullptr;
  }
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::ParseMathFunction(
    CSSValueID function_id,
    CSSParserTokenRange tokens) {
  CSSMathExpressionNodeParser parser;
  CSSMathExpressionNode* result =
      parser.ParseMathFunction(function_id, tokens, 0);

  // TODO(pjh0718): Do simplificiation for result above.
  return result;
}

}  // namespace blink
