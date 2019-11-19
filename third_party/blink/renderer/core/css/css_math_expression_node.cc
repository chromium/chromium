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

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

static const int maxExpressionDepth = 100;

enum ParseState { OK, TooDeep, NoMoreTokens };

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

// ------ Start of CSSMathExpressionNumericLiteral member functions ------

// static
CSSMathExpressionNumericLiteral* CSSMathExpressionNumericLiteral::Create(
    const CSSNumericLiteralValue* value,
    bool is_integer) {
  return MakeGarbageCollected<CSSMathExpressionNumericLiteral>(value,
                                                               is_integer);
}

// static
CSSMathExpressionNumericLiteral* CSSMathExpressionNumericLiteral::Create(
    double value,
    CSSPrimitiveValue::UnitType type,
    bool is_integer) {
  if (std::isnan(value) || std::isinf(value))
    return nullptr;
  return MakeGarbageCollected<CSSMathExpressionNumericLiteral>(
      CSSNumericLiteralValue::Create(value, type), is_integer);
}

CSSMathExpressionNumericLiteral::CSSMathExpressionNumericLiteral(
    const CSSNumericLiteralValue* value,
    bool is_integer)
    : CSSMathExpressionNode(UnitCategory(value->GetType()), is_integer),
      value_(value) {}

bool CSSMathExpressionNumericLiteral::IsZero() const {
  return !value_->GetDoubleValue();
}

String CSSMathExpressionNumericLiteral::CustomCSSText() const {
  return value_->CssText();
}

base::Optional<PixelsAndPercent>
CSSMathExpressionNumericLiteral::ToPixelsAndPercent(
    const CSSToLengthConversionData& conversion_data) const {
  PixelsAndPercent value(0, 0);
  switch (category_) {
    case kCalcLength:
      value.pixels = value_->ComputeLength<float>(conversion_data);
      break;
    case kCalcPercent:
      DCHECK(value_->IsPercentage());
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
  return base::MakeRefCounted<CalculationExpressionLeafNode>(
      *ToPixelsAndPercent(conversion_data));
}

double CSSMathExpressionNumericLiteral::DoubleValue() const {
  if (HasDoubleValue(ResolvedUnitType()))
    return value_->GetDoubleValue();
  NOTREACHED();
  return 0;
}

base::Optional<double>
CSSMathExpressionNumericLiteral::ComputeValueInCanonicalUnit() const {
  switch (category_) {
    case kCalcNumber:
    case kCalcPercent:
      return value_->DoubleValue();
    case kCalcLength:
      if (CSSPrimitiveValue::IsRelativeUnit(value_->GetType()))
        return base::nullopt;
      U_FALLTHROUGH;
    case kCalcAngle:
    case kCalcTime:
    case kCalcFrequency:
      return value_->DoubleValue() *
             CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                 value_->GetType());
    default:
      return base::nullopt;
  }
}

double CSSMathExpressionNumericLiteral::ComputeLengthPx(
    const CSSToLengthConversionData& conversion_data) const {
  switch (category_) {
    case kCalcLength:
      return value_->ComputeLength<double>(conversion_data);
    case kCalcNumber:
    case kCalcPercent:
    case kCalcAngle:
    case kCalcFrequency:
    case kCalcPercentLength:
    case kCalcPercentNumber:
    case kCalcTime:
    case kCalcLengthNumber:
    case kCalcPercentLengthNumber:
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

  return DataEquivalent(value_,
                        To<CSSMathExpressionNumericLiteral>(other).value_);
}

CSSPrimitiveValue::UnitType CSSMathExpressionNumericLiteral::ResolvedUnitType()
    const {
  return value_->GetType();
}

bool CSSMathExpressionNumericLiteral::IsComputationallyIndependent() const {
  return value_->IsComputationallyIndependent();
}

void CSSMathExpressionNumericLiteral::Trace(blink::Visitor* visitor) {
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
    /* CalcNumber */ {kCalcNumber, kCalcLengthNumber, kCalcPercentNumber,
                      kCalcPercentNumber, kCalcOther, kCalcOther, kCalcOther,
                      kCalcOther, kCalcLengthNumber, kCalcPercentLengthNumber},
    /* CalcLength */
    {kCalcLengthNumber, kCalcLength, kCalcPercentLength, kCalcOther,
     kCalcPercentLength, kCalcOther, kCalcOther, kCalcOther, kCalcLengthNumber,
     kCalcPercentLengthNumber},
    /* CalcPercent */
    {kCalcPercentNumber, kCalcPercentLength, kCalcPercent, kCalcPercentNumber,
     kCalcPercentLength, kCalcOther, kCalcOther, kCalcOther,
     kCalcPercentLengthNumber, kCalcPercentLengthNumber},
    /* CalcPercentNumber */
    {kCalcPercentNumber, kCalcPercentLengthNumber, kCalcPercentNumber,
     kCalcPercentNumber, kCalcPercentLengthNumber, kCalcOther, kCalcOther,
     kCalcOther, kCalcOther, kCalcPercentLengthNumber},
    /* CalcPercentLength */
    {kCalcPercentLengthNumber, kCalcPercentLength, kCalcPercentLength,
     kCalcPercentLengthNumber, kCalcPercentLength, kCalcOther, kCalcOther,
     kCalcOther, kCalcOther, kCalcPercentLengthNumber},
    /* CalcAngle  */
    {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcAngle,
     kCalcOther, kCalcOther, kCalcOther, kCalcOther},
    /* CalcTime */
    {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
     kCalcTime, kCalcOther, kCalcOther, kCalcOther},
    /* CalcFrequency */
    {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
     kCalcOther, kCalcFrequency, kCalcOther, kCalcOther},
    /* CalcLengthNumber */
    {kCalcLengthNumber, kCalcLengthNumber, kCalcPercentLengthNumber,
     kCalcPercentLengthNumber, kCalcPercentLengthNumber, kCalcOther, kCalcOther,
     kCalcOther, kCalcLengthNumber, kCalcPercentLengthNumber},
    /* CalcPercentLengthNumber */
    {kCalcPercentLengthNumber, kCalcPercentLengthNumber,
     kCalcPercentLengthNumber, kCalcPercentLengthNumber,
     kCalcPercentLengthNumber, kCalcOther, kCalcOther, kCalcOther,
     kCalcPercentLengthNumber, kCalcPercentLengthNumber}};

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
      if (right_category != kCalcNumber || right_side.IsZero())
        return kCalcOther;
      return left_category;
    default:
      break;
  }

  NOTREACHED();
  return kCalcOther;
}

static bool IsIntegerResult(const CSSMathExpressionNode* left_side,
                            const CSSMathExpressionNode* right_side,
                            CSSMathOperator op) {
  // Not testing for actual integer values.
  // Performs W3C spec's type checking for calc integers.
  // http://www.w3.org/TR/css3-values/#calc-type-checking
  return op != CSSMathOperator::kDivide && left_side->IsInteger() &&
         right_side->IsInteger();
}

// ------ Start of CSSMathExpressionBinaryOperation member functions ------

// static
CSSMathExpressionNode* CSSMathExpressionBinaryOperation::Create(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  DCHECK_NE(left_side->Category(), kCalcOther);
  DCHECK_NE(right_side->Category(), kCalcOther);

  CalculationCategory new_category =
      DetermineCategory(*left_side, *right_side, op);
  if (new_category == kCalcOther)
    return nullptr;

  return MakeGarbageCollected<CSSMathExpressionBinaryOperation>(
      left_side, right_side, op, new_category);
}

// static
CSSMathExpressionNode* CSSMathExpressionBinaryOperation::CreateSimplified(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  if (left_side->IsMathFunction() || right_side->IsMathFunction())
    return Create(left_side, right_side, op);

  CalculationCategory left_category = left_side->Category();
  CalculationCategory right_category = right_side->Category();
  DCHECK_NE(left_category, kCalcOther);
  DCHECK_NE(right_category, kCalcOther);

  bool is_integer = IsIntegerResult(left_side, right_side, op);

  // Simplify numbers.
  if (left_category == kCalcNumber && right_category == kCalcNumber) {
    return CSSMathExpressionNumericLiteral::Create(
        EvaluateOperator(left_side->DoubleValue(), right_side->DoubleValue(),
                         op),
        CSSPrimitiveValue::UnitType::kNumber, is_integer);
  }

  // Simplify addition and subtraction between same types.
  if (op == CSSMathOperator::kAdd || op == CSSMathOperator::kSubtract) {
    if (left_category == right_side->Category()) {
      CSSPrimitiveValue::UnitType left_type = left_side->ResolvedUnitType();
      if (HasDoubleValue(left_type)) {
        CSSPrimitiveValue::UnitType right_type = right_side->ResolvedUnitType();
        if (left_type == right_type) {
          return CSSMathExpressionNumericLiteral::Create(
              EvaluateOperator(left_side->DoubleValue(),
                               right_side->DoubleValue(), op),
              left_type, is_integer);
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
            double left_value = clampTo<double>(
                left_side->DoubleValue() *
                CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                    left_type));
            double right_value = clampTo<double>(
                right_side->DoubleValue() *
                CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                    right_type));
            return CSSMathExpressionNumericLiteral::Create(
                EvaluateOperator(left_value, right_value, op), canonical_type,
                is_integer);
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
      return Create(left_side, right_side, op);
    if (number_side == left_side && op == CSSMathOperator::kDivide)
      return nullptr;
    const CSSMathExpressionNode* other_side =
        left_side == number_side ? right_side : left_side;

    double number = number_side->DoubleValue();
    if (std::isnan(number) || std::isinf(number))
      return nullptr;
    if (op == CSSMathOperator::kDivide && !number)
      return nullptr;

    CSSPrimitiveValue::UnitType other_type = other_side->ResolvedUnitType();
    if (HasDoubleValue(other_type)) {
      return CSSMathExpressionNumericLiteral::Create(
          EvaluateOperator(other_side->DoubleValue(), number, op), other_type,
          is_integer);
    }
  }

  return Create(left_side, right_side, op);
}

CSSMathExpressionBinaryOperation::CSSMathExpressionBinaryOperation(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op,
    CalculationCategory category)
    : CSSMathExpressionNode(category,
                            IsIntegerResult(left_side, right_side, op)),
      left_side_(left_side),
      right_side_(right_side),
      operator_(op) {}

bool CSSMathExpressionBinaryOperation::IsZero() const {
  return !DoubleValue();
}

base::Optional<PixelsAndPercent>
CSSMathExpressionBinaryOperation::ToPixelsAndPercent(
    const CSSToLengthConversionData& conversion_data) const {
  base::Optional<PixelsAndPercent> result;
  switch (operator_) {
    case CSSMathOperator::kAdd:
    case CSSMathOperator::kSubtract: {
      result = left_side_->ToPixelsAndPercent(conversion_data);
      if (!result)
        return base::nullopt;

      base::Optional<PixelsAndPercent> other_side =
          right_side_->ToPixelsAndPercent(conversion_data);
      if (!other_side)
        return base::nullopt;
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
      const CSSMathExpressionNode* number_side =
          GetNumberSide(left_side_, right_side_);
      const CSSMathExpressionNode* other_side =
          left_side_ == number_side ? right_side_ : left_side_;
      result = other_side->ToPixelsAndPercent(conversion_data);
      if (!result)
        return base::nullopt;
      float number = number_side->DoubleValue();
      if (operator_ == CSSMathOperator::kDivide)
        number = 1.0 / number;
      result->pixels *= number;
      result->percent *= number;
      break;
    }
    default:
      NOTREACHED();
  }
  return result;
}

scoped_refptr<const CalculationExpressionNode>
CSSMathExpressionBinaryOperation::ToCalculationExpression(
    const CSSToLengthConversionData& conversion_data) const {
  switch (operator_) {
    case CSSMathOperator::kAdd:
      return CalculationExpressionAdditiveNode::CreateSimplified(
          left_side_->ToCalculationExpression(conversion_data),
          right_side_->ToCalculationExpression(conversion_data),
          CalculationExpressionAdditiveNode::Type::kAdd);
    case CSSMathOperator::kSubtract:
      return CalculationExpressionAdditiveNode::CreateSimplified(
          left_side_->ToCalculationExpression(conversion_data),
          right_side_->ToCalculationExpression(conversion_data),
          CalculationExpressionAdditiveNode::Type::kSubtract);
    case CSSMathOperator::kMultiply:
      DCHECK_NE((left_side_->Category() == kCalcNumber),
                (right_side_->Category() == kCalcNumber));
      if (left_side_->Category() == kCalcNumber) {
        return CalculationExpressionMultiplicationNode::CreateSimplified(
            right_side_->ToCalculationExpression(conversion_data),
            left_side_->DoubleValue());
      }
      return CalculationExpressionMultiplicationNode::CreateSimplified(
          left_side_->ToCalculationExpression(conversion_data),
          right_side_->DoubleValue());
    case CSSMathOperator::kDivide:
      DCHECK_EQ(right_side_->Category(), kCalcNumber);
      return CalculationExpressionMultiplicationNode::CreateSimplified(
          left_side_->ToCalculationExpression(conversion_data),
          1.0 / right_side_->DoubleValue());
    default:
      NOTREACHED();
      return nullptr;
  }
}

double CSSMathExpressionBinaryOperation::DoubleValue() const {
  DCHECK(HasDoubleValue(ResolvedUnitType())) << CustomCSSText();
  return Evaluate(left_side_->DoubleValue(), right_side_->DoubleValue());
}

static bool HasCanonicalUnit(CalculationCategory category) {
  return category == kCalcNumber || category == kCalcLength ||
         category == kCalcPercent || category == kCalcAngle ||
         category == kCalcTime || category == kCalcFrequency;
}

base::Optional<double>
CSSMathExpressionBinaryOperation::ComputeValueInCanonicalUnit() const {
  if (!HasCanonicalUnit(category_))
    return base::nullopt;

  base::Optional<double> left_value = left_side_->ComputeValueInCanonicalUnit();
  if (!left_value)
    return base::nullopt;

  base::Optional<double> right_value =
      right_side_->ComputeValueInCanonicalUnit();
  if (!right_value)
    return base::nullopt;

  return Evaluate(*left_value, *right_value);
}

double CSSMathExpressionBinaryOperation::ComputeLengthPx(
    const CSSToLengthConversionData& conversion_data) const {
  DCHECK_EQ(kCalcLength, Category());
  double left_value;
  if (left_side_->Category() == kCalcLength) {
    left_value = left_side_->ComputeLengthPx(conversion_data);
  } else {
    DCHECK_EQ(kCalcNumber, left_side_->Category());
    left_value = left_side_->DoubleValue();
  }
  double right_value;
  if (right_side_->Category() == kCalcLength) {
    right_value = right_side_->ComputeLengthPx(conversion_data);
  } else {
    DCHECK_EQ(kCalcNumber, right_side_->Category());
    right_value = right_side_->DoubleValue();
  }
  return Evaluate(left_value, right_value);
}

bool CSSMathExpressionBinaryOperation::AccumulateLengthArray(
    CSSLengthArray& length_array,
    double multiplier) const {
  switch (operator_) {
    case CSSMathOperator::kAdd:
      if (!left_side_->AccumulateLengthArray(length_array, multiplier))
        return false;
      if (!right_side_->AccumulateLengthArray(length_array, multiplier))
        return false;
      return true;
    case CSSMathOperator::kSubtract:
      if (!left_side_->AccumulateLengthArray(length_array, multiplier))
        return false;
      if (!right_side_->AccumulateLengthArray(length_array, -multiplier))
        return false;
      return true;
    case CSSMathOperator::kMultiply:
      DCHECK_NE((left_side_->Category() == kCalcNumber),
                (right_side_->Category() == kCalcNumber));
      if (left_side_->Category() == kCalcNumber) {
        return right_side_->AccumulateLengthArray(
            length_array, multiplier * left_side_->DoubleValue());
      } else {
        return left_side_->AccumulateLengthArray(
            length_array, multiplier * right_side_->DoubleValue());
      }
    case CSSMathOperator::kDivide:
      DCHECK_EQ(right_side_->Category(), kCalcNumber);
      return left_side_->AccumulateLengthArray(
          length_array, multiplier / right_side_->DoubleValue());
    default:
      NOTREACHED();
      return false;
  }
}

void CSSMathExpressionBinaryOperation::AccumulateLengthUnitTypes(
    CSSPrimitiveValue::LengthTypeFlags& types) const {
  left_side_->AccumulateLengthUnitTypes(types);
  right_side_->AccumulateLengthUnitTypes(types);
}

bool CSSMathExpressionBinaryOperation::IsComputationallyIndependent() const {
  if (Category() != kCalcLength && Category() != kCalcPercentLength)
    return true;
  return left_side_->IsComputationallyIndependent() &&
         right_side_->IsComputationallyIndependent();
}

String CSSMathExpressionBinaryOperation::CustomCSSText() const {
  StringBuilder result;

  const bool left_side_needs_parentheses =
      left_side_->IsBinaryOperation() && operator_ != CSSMathOperator::kAdd;
  if (left_side_needs_parentheses)
    result.Append('(');
  result.Append(left_side_->CustomCSSText());
  if (left_side_needs_parentheses)
    result.Append(')');

  result.Append(' ');
  result.Append(ToString(operator_));
  result.Append(' ');

  const bool right_side_needs_parentheses =
      right_side_->IsBinaryOperation() && operator_ != CSSMathOperator::kAdd;
  if (right_side_needs_parentheses)
    result.Append('(');
  result.Append(right_side_->CustomCSSText());
  if (right_side_needs_parentheses)
    result.Append(')');

  return result.ToString();
}

bool CSSMathExpressionBinaryOperation::operator==(
    const CSSMathExpressionNode& exp) const {
  if (!exp.IsBinaryOperation())
    return false;

  const CSSMathExpressionBinaryOperation& other =
      To<CSSMathExpressionBinaryOperation>(exp);
  return DataEquivalent(left_side_, other.left_side_) &&
         DataEquivalent(right_side_, other.right_side_) &&
         operator_ == other.operator_;
}

CSSPrimitiveValue::UnitType CSSMathExpressionBinaryOperation::ResolvedUnitType()
    const {
  switch (category_) {
    case kCalcNumber:
      DCHECK_EQ(left_side_->Category(), kCalcNumber);
      DCHECK_EQ(right_side_->Category(), kCalcNumber);
      return CSSPrimitiveValue::UnitType::kNumber;
    case kCalcLength:
    case kCalcPercent: {
      if (left_side_->Category() == kCalcNumber)
        return right_side_->ResolvedUnitType();
      if (right_side_->Category() == kCalcNumber)
        return left_side_->ResolvedUnitType();
      CSSPrimitiveValue::UnitType left_type = left_side_->ResolvedUnitType();
      if (left_type == right_side_->ResolvedUnitType())
        return left_type;
      return CSSPrimitiveValue::UnitType::kUnknown;
    }
    case kCalcAngle:
      return CSSPrimitiveValue::UnitType::kDegrees;
    case kCalcTime:
      return CSSPrimitiveValue::UnitType::kMilliseconds;
    case kCalcFrequency:
      return CSSPrimitiveValue::UnitType::kHertz;
    case kCalcPercentLength:
    case kCalcPercentNumber:
    case kCalcLengthNumber:
    case kCalcPercentLengthNumber:
    case kCalcOther:
      return CSSPrimitiveValue::UnitType::kUnknown;
  }
  NOTREACHED();
  return CSSPrimitiveValue::UnitType::kUnknown;
}

void CSSMathExpressionBinaryOperation::Trace(blink::Visitor* visitor) {
  visitor->Trace(left_side_);
  visitor->Trace(right_side_);
  CSSMathExpressionNode::Trace(visitor);
}

// static
const CSSMathExpressionNode* CSSMathExpressionBinaryOperation::GetNumberSide(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side) {
  if (left_side->Category() == kCalcNumber)
    return left_side;
  if (right_side->Category() == kCalcNumber)
    return right_side;
  return nullptr;
}

// static
double CSSMathExpressionBinaryOperation::EvaluateOperator(double left_value,
                                                          double right_value,
                                                          CSSMathOperator op) {
  switch (op) {
    case CSSMathOperator::kAdd:
      return clampTo<double>(left_value + right_value);
    case CSSMathOperator::kSubtract:
      return clampTo<double>(left_value - right_value);
    case CSSMathOperator::kMultiply:
      return clampTo<double>(left_value * right_value);
    case CSSMathOperator::kDivide:
      if (right_value)
        return clampTo<double>(left_value / right_value);
      return std::numeric_limits<double>::quiet_NaN();
    default:
      NOTREACHED();
      break;
  }
  return 0;
}

#if DCHECK_IS_ON()
bool CSSMathExpressionBinaryOperation::InvolvesPercentageComparisons() const {
  return left_side_->InvolvesPercentageComparisons() ||
         right_side_->InvolvesPercentageComparisons();
}
#endif

// ------ End of CSSMathExpressionBinaryOperation member functions ------

// ------ Start of CSSMathExpressionVariadicOperation member functions ------

// static
CSSMathExpressionVariadicOperation* CSSMathExpressionVariadicOperation::Create(
    Operands&& operands,
    CSSMathOperator op) {
  DCHECK(op == CSSMathOperator::kMin || op == CSSMathOperator::kMax);
  DCHECK(operands.size());
  bool is_first = true;
  CalculationCategory category;
  bool is_integer;
  for (const auto& operand : operands) {
    if (is_first) {
      category = operand->Category();
      is_integer = operand->IsInteger();
    } else {
      category = kAddSubtractResult[category][operand->Category()];
      if (!operand->IsInteger())
        is_integer = false;
    }
    is_first = false;
    if (category == kCalcOther)
      return nullptr;
  }
  return MakeGarbageCollected<CSSMathExpressionVariadicOperation>(
      category, is_integer, std::move(operands), op);
}

CSSMathExpressionVariadicOperation::CSSMathExpressionVariadicOperation(
    CalculationCategory category,
    bool is_integer_result,
    Operands&& operands,
    CSSMathOperator op)
    : CSSMathExpressionNode(category, is_integer_result),
      operands_(std::move(operands)),
      operator_(op) {}

void CSSMathExpressionVariadicOperation::Trace(blink::Visitor* visitor) {
  visitor->Trace(operands_);
  CSSMathExpressionNode::Trace(visitor);
}

bool CSSMathExpressionVariadicOperation::IsZero() const {
  base::Optional<double> maybe_value = ComputeValueInCanonicalUnit();
  return maybe_value && !*maybe_value;
}

double CSSMathExpressionVariadicOperation::EvaluateBinary(double lhs,
                                                          double rhs) const {
  switch (operator_) {
    case CSSMathOperator::kMin:
      return std::min(lhs, rhs);
    case CSSMathOperator::kMax:
      return std::max(lhs, rhs);
    default:
      NOTREACHED();
      return 0;
  }
}

base::Optional<double>
CSSMathExpressionVariadicOperation::ComputeValueInCanonicalUnit() const {
  base::Optional<double> first_value =
      operands_.front()->ComputeValueInCanonicalUnit();
  if (!first_value)
    return base::nullopt;

  double result = *first_value;
  for (const auto& operand : SecondToLastOperands()) {
    base::Optional<double> maybe_value = operand->ComputeValueInCanonicalUnit();
    if (!maybe_value)
      return base::nullopt;
    result = EvaluateBinary(result, *maybe_value);
  }
  return result;
}

double CSSMathExpressionVariadicOperation::DoubleValue() const {
  DCHECK(HasDoubleValue(ResolvedUnitType()));
  double result = operands_.front()->DoubleValue();
  for (const auto& operand : SecondToLastOperands())
    result = EvaluateBinary(result, operand->DoubleValue());
  return result;
}

double CSSMathExpressionVariadicOperation::ComputeLengthPx(
    const CSSToLengthConversionData& data) const {
  DCHECK_EQ(kCalcLength, Category());
  double result = operands_.front()->ComputeLengthPx(data);
  for (const auto& operand : SecondToLastOperands())
    result = EvaluateBinary(result, operand->ComputeLengthPx(data));
  return result;
}

String CSSMathExpressionVariadicOperation::CSSTextAsClamp() const {
  DCHECK(is_clamp_);
  DCHECK_EQ(CSSMathOperator::kMax, operator_);
  DCHECK_EQ(2u, operands_.size());
  DCHECK(operands_[1]->IsVariadicOperation());
  const auto& nested = To<CSSMathExpressionVariadicOperation>(*operands_[1]);
  DCHECK(!nested.is_clamp_);
  DCHECK_EQ(CSSMathOperator::kMin, nested.operator_);
  DCHECK_EQ(2u, nested.operands_.size());

  StringBuilder result;
  result.Append("clamp(");
  result.Append(operands_[0]->CustomCSSText());
  result.Append(", ");
  result.Append(nested.operands_[0]->CustomCSSText());
  result.Append(", ");
  result.Append(nested.operands_[1]->CustomCSSText());
  result.Append(")");
  return result.ToString();
}

String CSSMathExpressionVariadicOperation::CustomCSSText() const {
  if (is_clamp_)
    return CSSTextAsClamp();

  StringBuilder result;
  result.Append(ToString(operator_));
  result.Append('(');
  result.Append(operands_.front()->CustomCSSText());
  for (const auto& operand : SecondToLastOperands()) {
    result.Append(", ");
    result.Append(operand->CustomCSSText());
  }
  result.Append(')');

  return result.ToString();
}

base::Optional<PixelsAndPercent>
CSSMathExpressionVariadicOperation::ToPixelsAndPercent(
    const CSSToLengthConversionData& conversion_data) const {
  return base::nullopt;
}

scoped_refptr<const CalculationExpressionNode>
CSSMathExpressionVariadicOperation::ToCalculationExpression(
    const CSSToLengthConversionData& data) const {
  Vector<scoped_refptr<const CalculationExpressionNode>> operands;
  operands.ReserveCapacity(operands_.size());
  for (const auto operand : operands_)
    operands.push_back(operand->ToCalculationExpression(data));
  auto expression_type = operator_ == CSSMathOperator::kMin
                             ? CalculationExpressionComparisonNode::Type::kMin
                             : CalculationExpressionComparisonNode::Type::kMax;
  return CalculationExpressionComparisonNode::CreateSimplified(
      std::move(operands), expression_type);
}

bool CSSMathExpressionVariadicOperation::AccumulateLengthArray(CSSLengthArray&,
                                                               double) const {
  // When comparison function are involved, we can't resolve the expression into
  // a length array.
  // TODO(crbug.com/991672): We need a more general length interpolation
  // implemetation that doesn't rely on CSSLengthArray.
  return false;
}

void CSSMathExpressionVariadicOperation::AccumulateLengthUnitTypes(
    CSSPrimitiveValue::LengthTypeFlags& types) const {
  for (const auto& operand : operands_)
    operand->AccumulateLengthUnitTypes(types);
}

bool CSSMathExpressionVariadicOperation::IsComputationallyIndependent() const {
  for (const auto& operand : operands_) {
    if (!operand->IsComputationallyIndependent())
      return false;
  }
  return true;
}

bool CSSMathExpressionVariadicOperation::operator==(
    const CSSMathExpressionNode& exp) const {
  if (!exp.IsVariadicOperation())
    return false;
  const CSSMathExpressionVariadicOperation& other =
      To<CSSMathExpressionVariadicOperation>(exp);
  if (operator_ != other.operator_)
    return false;
  if (operands_.size() != other.operands_.size())
    return false;
  for (wtf_size_t i = 0; i < operands_.size(); ++i) {
    if (!DataEquivalent(operands_[i], other.operands_[i]))
      return false;
  }
  return true;
}

CSSPrimitiveValue::UnitType
CSSMathExpressionVariadicOperation::ResolvedUnitType() const {
  CSSPrimitiveValue::UnitType result = operands_.front()->ResolvedUnitType();
  if (result == CSSPrimitiveValue::UnitType::kUnknown)
    return CSSPrimitiveValue::UnitType::kUnknown;
  for (const auto& operand : SecondToLastOperands()) {
    CSSPrimitiveValue::UnitType next = operand->ResolvedUnitType();
    if (next == CSSPrimitiveValue::UnitType::kUnknown || next != result)
      return CSSPrimitiveValue::UnitType::kUnknown;
  }
  return result;
}

#if DCHECK_IS_ON()
bool CSSMathExpressionVariadicOperation::InvolvesPercentageComparisons() const {
  if (Category() == kCalcPercent && operands_.size() > 1u)
    return true;
  for (const auto& operand : operands_) {
    if (operand->InvolvesPercentageComparisons())
      return true;
  }
  return false;
}
#endif

// ------ End of CSSMathExpressionVariadicOperation member functions

static ParseState CheckDepthAndIndex(int* depth, CSSParserTokenRange tokens) {
  (*depth)++;
  if (tokens.AtEnd())
    return NoMoreTokens;
  if (*depth > maxExpressionDepth)
    return TooDeep;
  return OK;
}

class CSSMathExpressionNodeParser {
  STACK_ALLOCATED();

 public:
  CSSMathExpressionNodeParser() {}

  CSSMathExpressionNode* ParseCalc(CSSParserTokenRange tokens) {
    tokens.ConsumeWhitespace();
    CSSMathExpressionNode* result = ParseValueExpression(tokens, 0);
    if (!result || !tokens.AtEnd())
      return nullptr;
    return result;
  }

  CSSMathExpressionNode* ParseMinOrMax(CSSParserTokenRange tokens,
                                       CSSMathOperator op,
                                       int depth) {
    DCHECK(op == CSSMathOperator::kMin || op == CSSMathOperator::kMax);
    if (CheckDepthAndIndex(&depth, tokens) != OK)
      return nullptr;

    CSSMathExpressionVariadicOperation::Operands operands;
    bool last_token_is_comma = false;
    while (!tokens.AtEnd()) {
      tokens.ConsumeWhitespace();
      CSSMathExpressionNode* operand = ParseValueExpression(tokens, depth);
      if (!operand)
        return nullptr;

      last_token_is_comma = false;
      operands.push_back(operand);

      if (!css_property_parser_helpers::ConsumeCommaIncludingWhitespace(tokens))
        break;
      last_token_is_comma = true;
    }

    if (operands.IsEmpty() || !tokens.AtEnd() || last_token_is_comma)
      return nullptr;

    return CSSMathExpressionVariadicOperation::Create(std::move(operands), op);
  }

  CSSMathExpressionNode* ParseClamp(CSSParserTokenRange tokens, int depth) {
    if (CheckDepthAndIndex(&depth, tokens) != OK)
      return nullptr;

    CSSMathExpressionNode* min_operand = ParseValueExpression(tokens, depth);
    if (!min_operand)
      return nullptr;

    if (!css_property_parser_helpers::ConsumeCommaIncludingWhitespace(tokens))
      return nullptr;

    CSSMathExpressionNode* val_operand = ParseValueExpression(tokens, depth);
    if (!val_operand)
      return nullptr;

    if (!css_property_parser_helpers::ConsumeCommaIncludingWhitespace(tokens))
      return nullptr;

    CSSMathExpressionNode* max_operand = ParseValueExpression(tokens, depth);
    if (!max_operand)
      return nullptr;

    if (!tokens.AtEnd())
      return nullptr;

    // clamp(MIN, VAL, MAX) is identical to max(MIN, min(VAL, MAX))

    auto* nested = CSSMathExpressionVariadicOperation::Create(
        {val_operand, max_operand}, CSSMathOperator::kMin);
    auto* result = CSSMathExpressionVariadicOperation::Create(
        {min_operand, nested}, CSSMathOperator::kMax);
    result->SetIsClamp();
    return result;
  }

 private:
  CSSMathExpressionNode* ParseValue(CSSParserTokenRange& tokens) {
    CSSParserToken token = tokens.ConsumeIncludingWhitespace();
    if (!(token.GetType() == kNumberToken ||
          token.GetType() == kPercentageToken ||
          token.GetType() == kDimensionToken))
      return nullptr;

    CSSPrimitiveValue::UnitType type = token.GetUnitType();
    if (UnitCategory(type) == kCalcOther)
      return nullptr;

    return CSSMathExpressionNumericLiteral::Create(
        CSSNumericLiteralValue::Create(token.NumericValue(), type),
        token.GetNumericValueType() == kIntegerValueType);
  }

  CSSMathExpressionNode* ParseValueTerm(CSSParserTokenRange& tokens,
                                        int depth) {
    if (CheckDepthAndIndex(&depth, tokens) != OK)
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

    if (RuntimeEnabledFeatures::CSSComparisonFunctionsEnabled()) {
      if (tokens.Peek().GetType() == kFunctionToken) {
        CSSValueID function_id = tokens.Peek().FunctionId();
        CSSParserTokenRange inner_range = tokens.ConsumeBlock();
        tokens.ConsumeWhitespace();
        inner_range.ConsumeWhitespace();
        switch (function_id) {
          case CSSValueID::kMin:
            return ParseMinOrMax(inner_range, CSSMathOperator::kMin, depth);
          case CSSValueID::kMax:
            return ParseMinOrMax(inner_range, CSSMathOperator::kMax, depth);
          case CSSValueID::kClamp:
            return ParseClamp(inner_range, depth);
          default:
            break;
        }
      }
    }

    return ParseValue(tokens);
  }

  CSSMathExpressionNode* ParseValueMultiplicativeExpression(
      CSSParserTokenRange& tokens,
      int depth) {
    if (CheckDepthAndIndex(&depth, tokens) != OK)
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

      result = CSSMathExpressionBinaryOperation::CreateSimplified(
          result, rhs, math_operator);

      if (!result)
        return nullptr;
    }

    return result;
  }

  CSSMathExpressionNode* ParseAdditiveValueExpression(
      CSSParserTokenRange& tokens,
      int depth) {
    if (CheckDepthAndIndex(&depth, tokens) != OK)
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

      result = CSSMathExpressionBinaryOperation::CreateSimplified(
          result, rhs, math_operator);

      if (!result)
        return nullptr;
    }

    return result;
  }

  CSSMathExpressionNode* ParseValueExpression(CSSParserTokenRange& tokens,
                                              int depth) {
    return ParseAdditiveValueExpression(tokens, depth);
  }
};

scoped_refptr<CalculationValue> CSSMathExpressionNode::ToCalcValue(
    const CSSToLengthConversionData& conversion_data,
    ValueRange range) const {
  if (auto maybe_pixels_and_percent = ToPixelsAndPercent(conversion_data))
    return CalculationValue::Create(*maybe_pixels_and_percent, range);
  return CalculationValue::CreateSimplified(
      ToCalculationExpression(conversion_data), range);
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
  return CSSMathExpressionBinaryOperation::Create(
      CSSMathExpressionNumericLiteral::Create(
          CSSNumericLiteralValue::Create(
              percent, CSSPrimitiveValue::UnitType::kPercentage),
          percent == trunc(percent)),
      CSSMathExpressionNumericLiteral::Create(
          CSSNumericLiteralValue::Create(pixels,
                                         CSSPrimitiveValue::UnitType::kPixels),
          pixels == trunc(pixels)),
      op);
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::Create(
    const CalculationExpressionNode& node) {
  if (node.IsLeaf()) {
    const auto& leaf = To<CalculationExpressionLeafNode>(node);
    return Create(leaf.GetPixelsAndPercent());
  }

  if (node.IsMultiplication()) {
    const auto& multiplication =
        To<CalculationExpressionMultiplicationNode>(node);
    double factor = multiplication.GetFactor();
    return CSSMathExpressionBinaryOperation::Create(
        Create(multiplication.GetChild()),
        CSSMathExpressionNumericLiteral::Create(
            CSSNumericLiteralValue::Create(
                factor, CSSPrimitiveValue::UnitType::kNumber),
            factor == trunc(factor)),
        CSSMathOperator::kMultiply);
  }

  if (node.IsAdditive()) {
    const auto& add_or_subtract = To<CalculationExpressionAdditiveNode>(node);
    auto* lhs = Create(add_or_subtract.GetLeftSide());
    auto* rhs = Create(add_or_subtract.GetRightSide());
    CSSMathOperator op = add_or_subtract.IsAdd() ? CSSMathOperator::kAdd
                                                 : CSSMathOperator::kSubtract;
    return CSSMathExpressionBinaryOperation::Create(lhs, rhs, op);
  }

  DCHECK(node.IsComparison());
  const auto& comparison = To<CalculationExpressionComparisonNode>(node);
  CSSMathExpressionVariadicOperation::Operands operands;
  for (const auto& operand : comparison.GetOperands())
    operands.push_back(Create(*operand));
  CSSMathOperator op =
      comparison.IsMin() ? CSSMathOperator::kMin : CSSMathOperator::kMax;
  return CSSMathExpressionVariadicOperation::Create(std::move(operands), op);
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::ParseCalc(
    const CSSParserTokenRange& tokens) {
  CSSMathExpressionNodeParser parser;
  return parser.ParseCalc(tokens);
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::ParseMin(
    const CSSParserTokenRange& tokens) {
  CSSMathExpressionNodeParser parser;
  return parser.ParseMinOrMax(tokens, CSSMathOperator::kMin, 0);
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::ParseMax(
    const CSSParserTokenRange& tokens) {
  CSSMathExpressionNodeParser parser;
  return parser.ParseMinOrMax(tokens, CSSMathOperator::kMax, 0);
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::ParseClamp(
    const CSSParserTokenRange& tokens) {
  CSSMathExpressionNodeParser parser;
  return parser.ParseClamp(tokens, 0);
}

}  // namespace blink
