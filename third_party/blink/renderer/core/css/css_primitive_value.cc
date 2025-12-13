/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2012 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/css/css_primitive_value.h"

#include <cmath>

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_length_resolver.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_value_clamping_utils.h"
#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// Max/min values for CSS, needs to slightly smaller/larger than the true
// max/min values to allow for rounding without overflowing.
// Subtract two (rather than one) to allow for values to be converted to float
// and back without exceeding the LayoutUnit::Max.
const int kMaxValueForCssLength =
    INT_MAX / LayoutUnit::kFixedPointDenominator - 2;
const int kMinValueForCssLength =
    INT_MIN / LayoutUnit::kFixedPointDenominator + 2;

}  // namespace

struct SameSizeAsCSSPrimitiveValue : CSSValue {};
ASSERT_SIZE(CSSPrimitiveValue, SameSizeAsCSSPrimitiveValue);

float CSSPrimitiveValue::ClampToCSSLengthRange(double value) {
  // TODO(crbug.com/1133390): ClampTo function could occur the DECHECK failure
  // for NaN value. Therefore, infinity and NaN values should not be clamped
  // here.
  return ClampTo<float>(CSSValueClampingUtils::ClampLength(value),
                        kMinValueForCssLength, kMaxValueForCssLength);
}

Length::ValueRange CSSPrimitiveValue::ConversionToLengthValueRange(
    ValueRange range) {
  switch (range) {
    case ValueRange::kNonNegative:
      return Length::ValueRange::kNonNegative;
    case ValueRange::kAll:
      return Length::ValueRange::kAll;
    default:
      NOTREACHED();
  }
}

CSSPrimitiveValue::ValueRange CSSPrimitiveValue::ValueRangeForLengthValueRange(
    Length::ValueRange range) {
  switch (range) {
    case Length::ValueRange::kNonNegative:
      return ValueRange::kNonNegative;
    case Length::ValueRange::kAll:
      return ValueRange::kAll;
  }
}

CSSPrimitiveValue::UnitCategory CSSPrimitiveValue::UnitTypeToUnitCategory(
    UnitType type) {
  switch (type) {
    case UnitType::kNumber:
    case UnitType::kInteger:
      return CSSPrimitiveValue::kUNumber;
    case UnitType::kPercentage:
      return CSSPrimitiveValue::kUPercent;
    case UnitType::kPixels:
    case UnitType::kCentimeters:
    case UnitType::kMillimeters:
    case UnitType::kQuarterMillimeters:
    case UnitType::kInches:
    case UnitType::kPoints:
    case UnitType::kPicas:
    case UnitType::kUserUnits:
      return CSSPrimitiveValue::kULength;
    case UnitType::kMilliseconds:
    case UnitType::kSeconds:
      return CSSPrimitiveValue::kUTime;
    case UnitType::kDegrees:
    case UnitType::kRadians:
    case UnitType::kGradians:
    case UnitType::kTurns:
      return CSSPrimitiveValue::kUAngle;
    case UnitType::kHertz:
    case UnitType::kKilohertz:
      return CSSPrimitiveValue::kUFrequency;
    case UnitType::kDotsPerPixel:
    case UnitType::kX:
    case UnitType::kDotsPerInch:
    case UnitType::kDotsPerCentimeter:
      return CSSPrimitiveValue::kUResolution;
    default:
      return CSSPrimitiveValue::kUOther;
  }
}

bool CSSPrimitiveValue::IsResolvableBeforeLayout() const {
  // TODO(crbug.com/979895): Move this function to |CSSMathFunctionValue|.
  if (!IsCalculated()) {
    return true;
  }
  CalculationResultCategory category =
      To<CSSMathFunctionValue>(this)->Category();
  return category != kCalcLengthFunction;
}

bool CSSPrimitiveValue::IsResolution() const {
  return (IsNumericLiteralValue() &&
          To<CSSNumericLiteralValue>(this)->IsResolution()) ||
         (IsMathFunctionValue() &&
          To<CSSMathFunctionValue>(this)->IsResolution());
}

bool CSSPrimitiveValue::IsFlex() const {
  // TODO(crbug.com/993136): Either support math functions on flexible lengths;
  // or provide a justification for not supporting it, and move this function to
  // |CSSNumericLiteralValue|.
  return IsNumericLiteralValue() && To<CSSNumericLiteralValue>(this)->IsFlex();
}

bool CSSPrimitiveValue::IsAngle() const {
  if (IsNumericLiteralValue()) {
    return To<CSSNumericLiteralValue>(this)->IsAngle();
  }
  return To<CSSMathFunctionValue>(this)->IsAngle();
}

bool CSSPrimitiveValue::IsLength() const {
  if (IsNumericLiteralValue()) {
    return To<CSSNumericLiteralValue>(this)->IsLength();
  }
  return To<CSSMathFunctionValue>(this)->IsLength();
}

bool CSSPrimitiveValue::IsPx() const {
  if (IsNumericLiteralValue()) {
    return To<CSSNumericLiteralValue>(this)->IsPx();
  }
  return To<CSSMathFunctionValue>(this)->IsPx();
}

bool CSSPrimitiveValue::IsNumber() const {
  if (IsNumericLiteralValue()) {
    return To<CSSNumericLiteralValue>(this)->IsNumber();
  }
  return To<CSSMathFunctionValue>(this)->IsNumber();
}

bool CSSPrimitiveValue::IsInteger() const {
  // Integer target context can take calc() function
  // which resolves to number type.
  // So we don't have to track whether cals type is integer,
  // and we can answer to IsInteger() question asked from a context
  // in which requires integer type
  // (e.g. CSSPrimitiveValue::IsInteger() check in MediaQueryExp::Create)
  // here.
  if (IsNumericLiteralValue()) {
    return To<CSSNumericLiteralValue>(this)->IsInteger();
  }
  return To<CSSMathFunctionValue>(this)->IsNumber();
}

bool CSSPrimitiveValue::IsPercentage() const {
  if (IsNumericLiteralValue()) {
    return To<CSSNumericLiteralValue>(this)->IsPercentage();
  }
  return To<CSSMathFunctionValue>(this)->IsPercentage();
}

bool CSSPrimitiveValue::IsResolvableLength() const {
  return IsLength() && !InvolvesLayout();
}

bool CSSPrimitiveValue::HasPercentage() const {
  if (IsNumericLiteralValue()) {
    return To<CSSNumericLiteralValue>(this)->IsPercentage();
  }
  return To<CSSMathFunctionValue>(this)->ExpressionNode()->HasPercentage();
}

bool CSSPrimitiveValue::InvolvesLayout() const {
  if (IsNumericLiteralValue()) {
    return To<CSSNumericLiteralValue>(this)->IsPercentage();
  }
  return To<CSSMathFunctionValue>(this)->ExpressionNode()->InvolvesLayout();
}

bool CSSPrimitiveValue::IsTime() const {
  if (IsNumericLiteralValue()) {
    return To<CSSNumericLiteralValue>(this)->IsTime();
  }
  return To<CSSMathFunctionValue>(this)->IsTime();
}

bool CSSPrimitiveValue::IsComputationallyIndependent() const {
  if (IsNumericLiteralValue()) {
    return To<CSSNumericLiteralValue>(this)->IsComputationallyIndependent();
  }
  return To<CSSMathFunctionValue>(this)->IsComputationallyIndependent();
}

bool CSSPrimitiveValue::IsElementDependent() const {
  return IsMathFunctionValue() &&
         To<CSSMathFunctionValue>(this)->IsElementDependent();
}

bool CSSPrimitiveValue::HasContainerRelativeUnits() const {
  CSSPrimitiveValue::LengthTypeFlags units;
  AccumulateLengthUnitTypes(units);
  const CSSPrimitiveValue::LengthTypeFlags container_units(
      (1ull << CSSPrimitiveValue::kUnitTypeContainerWidth) |
      (1ull << CSSPrimitiveValue::kUnitTypeContainerHeight) |
      (1ull << CSSPrimitiveValue::kUnitTypeContainerInlineSize) |
      (1ull << CSSPrimitiveValue::kUnitTypeContainerBlockSize) |
      (1ull << CSSPrimitiveValue::kUnitTypeContainerMin) |
      (1ull << CSSPrimitiveValue::kUnitTypeContainerMax));
  return (units & container_units).any();
}

// static
CSSPrimitiveValue* CSSPrimitiveValue::CreateFromLength(const Length& length,
                                                       float zoom) {
  switch (length.GetType()) {
    case Length::kPercent:
      return CSSNumericLiteralValue::Create(length.Percent(),
                                            UnitType::kPercentage);
    case Length::kFixed:
      return CSSNumericLiteralValue::Create(length.Pixels() / zoom,
                                            UnitType::kPixels);
    case Length::kCalculated: {
      const CalculationValue& calc = length.GetCalculationValue();
      if (calc.IsExpression() || calc.Pixels()) {
        return CSSMathFunctionValue::Create(length, zoom);
      }
      double num = calc.Percent();
      if (num < 0 && calc.IsNonNegative()) {
        num = 0;
      }
      return CSSNumericLiteralValue::Create(num, UnitType::kPercentage);
    }
    case Length::kFlex:
      return CSSNumericLiteralValue::Create(length.Flex(), UnitType::kFlex);
    default:
      break;
  }
  NOTREACHED();
}

double CSSPrimitiveValue::ComputeDegrees(
    const CSSLengthResolver& length_resolver) const {
  double result =
      IsCalculated()
          ? To<CSSMathFunctionValue>(this)->ComputeDegrees(length_resolver)
          : To<CSSNumericLiteralValue>(this)->ComputeDegrees();
  return CSSValueClampingUtils::ClampAngle(result);
}

double CSSPrimitiveValue::ComputeSeconds(
    const CSSLengthResolver& length_resolver) const {
  double result =
      IsCalculated()
          ? To<CSSMathFunctionValue>(this)->ComputeSeconds(length_resolver)
          : To<CSSNumericLiteralValue>(this)->ComputeSeconds();
  return CSSValueClampingUtils::ClampTime(result);
}

double CSSPrimitiveValue::ComputeDotsPerPixel(
    const CSSLengthResolver& length_resolver) const {
  DCHECK(IsResolution());
  double result =
      IsCalculated()
          ? To<CSSMathFunctionValue>(this)->ComputeDotsPerPixel(length_resolver)
          : To<CSSNumericLiteralValue>(this)->ComputeDotsPerPixel();
  return CSSValueClampingUtils::ClampDouble(result);
}

template <>
int CSSPrimitiveValue::ComputeLength(
    const CSSLengthResolver& length_resolver) const {
  return RoundForImpreciseConversion<int>(ComputeLengthDouble(length_resolver));
}

template <>
unsigned CSSPrimitiveValue::ComputeLength(
    const CSSLengthResolver& length_resolver) const {
  return RoundForImpreciseConversion<unsigned>(
      ComputeLengthDouble(length_resolver));
}

template <>
Length CSSPrimitiveValue::ComputeLength(
    const CSSLengthResolver& length_resolver) const {
  return Length::Fixed(
      ClampToCSSLengthRange(ComputeLengthDouble(length_resolver)));
}

template <>
int16_t CSSPrimitiveValue::ComputeLength(
    const CSSLengthResolver& length_resolver) const {
  return RoundForImpreciseConversion<int16_t>(
      ComputeLengthDouble(length_resolver));
}

template <>
uint16_t CSSPrimitiveValue::ComputeLength(
    const CSSLengthResolver& length_resolver) const {
  return RoundForImpreciseConversion<uint16_t>(
      ComputeLengthDouble(length_resolver));
}

template <>
uint8_t CSSPrimitiveValue::ComputeLength(
    const CSSLengthResolver& length_resolver) const {
  return RoundForImpreciseConversion<uint8_t>(
      ComputeLengthDouble(length_resolver));
}

template <>
float CSSPrimitiveValue::ComputeLength(
    const CSSLengthResolver& length_resolver) const {
  return ClampTo<float>(
      CSSValueClampingUtils::ClampLength(ComputeLengthDouble(length_resolver)));
}

template <>
double CSSPrimitiveValue::ComputeLength(
    const CSSLengthResolver& length_resolver) const {
  return CSSValueClampingUtils::ClampLength(
      ComputeLengthDouble(length_resolver));
}

int CSSPrimitiveValue::ComputeInteger(
    const CSSLengthResolver& length_resolver) const {
  DCHECK(IsNumber());
  return IsCalculated()
             ? To<CSSMathFunctionValue>(this)->ComputeInteger(length_resolver)
             : To<CSSNumericLiteralValue>(this)->ComputeInteger();
}

double CSSPrimitiveValue::ComputeNumber(
    const CSSLengthResolver& length_resolver) const {
  DCHECK(IsNumber() || IsPercentage());
  // NOTE: Division by 100 will be done by ComputeNumber() if needed.
  return IsCalculated()
             ? To<CSSMathFunctionValue>(this)->ComputeNumber(length_resolver)
             : To<CSSNumericLiteralValue>(this)->ComputeNumber();
}

template <>
double CSSPrimitiveValue::ComputePercentage(
    const CSSLengthResolver& length_resolver) const {
  DCHECK(IsPercentage());
  return IsCalculated() ? To<CSSMathFunctionValue>(this)->ComputePercentage(
                              length_resolver)
                        : To<CSSNumericLiteralValue>(this)->ComputePercentage();
}

template <>
float CSSPrimitiveValue::ComputePercentage(
    const CSSLengthResolver& length_resolver) const {
  return ClampTo<float>(ComputePercentage<double>(length_resolver));
}

double CSSPrimitiveValue::ComputeValueInCanonicalUnit(
    const CSSLengthResolver& length_resolver) const {
  // Don't use it for mix of length and percentage or similar,
  // as it would compute 10px + 10% to 20.
  DCHECK(IsResolvableBeforeLayout());
  return IsCalculated()
             ? To<CSSMathFunctionValue>(this)->ComputeValueInCanonicalUnit(
                   length_resolver)
             : To<CSSNumericLiteralValue>(this)->ComputeInCanonicalUnit(
                   length_resolver);
}

std::optional<double> CSSPrimitiveValue::GetValueIfKnown() const {
  return IsCalculated() ? To<CSSMathFunctionValue>(this)->GetValueIfKnown()
                        : To<CSSNumericLiteralValue>(this)->GetValueIfKnown();
}

double CSSPrimitiveValue::ComputeLengthDouble(
    const CSSLengthResolver& length_resolver) const {
  if (IsCalculated()) {
    return To<CSSMathFunctionValue>(this)->ComputeLengthPx(length_resolver);
  }
  return To<CSSNumericLiteralValue>(this)->ComputeLengthPx(length_resolver);
}

bool CSSPrimitiveValue::AccumulateLengthArray(CSSLengthArray& length_array,
                                              double multiplier) const {
  DCHECK_EQ(length_array.values.size(), CSSLengthArray::kSize);
  if (IsCalculated()) {
    return To<CSSMathFunctionValue>(this)->AccumulateLengthArray(length_array,
                                                                 multiplier);
  }
  return To<CSSNumericLiteralValue>(this)->AccumulateLengthArray(length_array,
                                                                 multiplier);
}

void CSSPrimitiveValue::AccumulateLengthUnitTypes(
    LengthTypeFlags& types) const {
  if (IsCalculated()) {
    return To<CSSMathFunctionValue>(this)->AccumulateLengthUnitTypes(types);
  }
  To<CSSNumericLiteralValue>(this)->AccumulateLengthUnitTypes(types);
}

bool CSSPrimitiveValue::HasStaticViewportUnits(
    const LengthTypeFlags& length_type_flags) {
  return length_type_flags.test(CSSPrimitiveValue::kUnitTypeViewportWidth) ||
         length_type_flags.test(CSSPrimitiveValue::kUnitTypeViewportHeight) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeViewportInlineSize) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeViewportBlockSize) ||
         length_type_flags.test(CSSPrimitiveValue::kUnitTypeViewportMin) ||
         length_type_flags.test(CSSPrimitiveValue::kUnitTypeViewportMax) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeSmallViewportWidth) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeSmallViewportHeight) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeSmallViewportInlineSize) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeSmallViewportBlockSize) ||
         length_type_flags.test(CSSPrimitiveValue::kUnitTypeSmallViewportMin) ||
         length_type_flags.test(CSSPrimitiveValue::kUnitTypeSmallViewportMax) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeLargeViewportWidth) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeLargeViewportHeight) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeLargeViewportInlineSize) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeLargeViewportBlockSize) ||
         length_type_flags.test(CSSPrimitiveValue::kUnitTypeLargeViewportMin) ||
         length_type_flags.test(CSSPrimitiveValue::kUnitTypeLargeViewportMax);
}

bool CSSPrimitiveValue::HasDynamicViewportUnits(
    const LengthTypeFlags& length_type_flags) {
  return length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeDynamicViewportWidth) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeDynamicViewportHeight) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeDynamicViewportInlineSize) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeDynamicViewportBlockSize) ||
         length_type_flags.test(
             CSSPrimitiveValue::kUnitTypeDynamicViewportMin) ||
         length_type_flags.test(CSSPrimitiveValue::kUnitTypeDynamicViewportMax);
}

double CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
    UnitType unit_type) {
  double factor = 1.0;
  // FIXME: the switch can be replaced by an array of scale factors.
  switch (unit_type) {
    // These are "canonical" units in their respective categories.
    case UnitType::kPixels:
    case UnitType::kUserUnits:
    case UnitType::kDegrees:
    case UnitType::kSeconds:
    case UnitType::kHertz:
      break;
    case UnitType::kMilliseconds:
      factor = 0.001;
      break;
    case UnitType::kCentimeters:
      factor = kCssPixelsPerCentimeter;
      break;
    case UnitType::kDotsPerCentimeter:
      factor = 1 / kCssPixelsPerCentimeter;
      break;
    case UnitType::kMillimeters:
      factor = kCssPixelsPerMillimeter;
      break;
    case UnitType::kQuarterMillimeters:
      factor = kCssPixelsPerQuarterMillimeter;
      break;
    case UnitType::kInches:
      factor = kCssPixelsPerInch;
      break;
    case UnitType::kDotsPerInch:
      factor = 1 / kCssPixelsPerInch;
      break;
    case UnitType::kPoints:
      factor = kCssPixelsPerPoint;
      break;
    case UnitType::kPicas:
      factor = kCssPixelsPerPica;
      break;
    case UnitType::kRadians:
      factor = 180 / kPiDouble;
      break;
    case UnitType::kGradians:
      factor = 0.9;
      break;
    case UnitType::kTurns:
      factor = 360;
      break;
    case UnitType::kKilohertz:
      factor = 1000;
      break;
    default:
      break;
  }

  return factor;
}

Length CSSPrimitiveValue::ConvertToLength(
    const CSSLengthResolver& length_resolver) const {
  if (IsResolvableLength()) {
    return ComputeLength<Length>(length_resolver);
  }
  if (IsPercentage()) {
    if (IsNumericLiteralValue() ||
        !To<CSSMathFunctionValue>(this)->AllowsNegativePercentageReference()) {
      return Length::Percent(CSSValueClampingUtils::ClampLength(
          ComputePercentage(length_resolver)));
    }
  }
  DCHECK(IsCalculated());
  return To<CSSMathFunctionValue>(this)->ConvertToLength(length_resolver);
}

CSSPrimitiveValue::UnitType CSSPrimitiveValue::CanonicalUnitTypeForCategory(
    UnitCategory category) {
  // The canonical unit type is chosen according to the way
  // CSSPropertyParser::ValidUnit() chooses the default unit in each category
  // (based on unitflags).
  switch (category) {
    case kUNumber:
      return UnitType::kNumber;
    case kULength:
      return UnitType::kPixels;
    case kUPercent:
      return UnitType::kUnknown;  // Cannot convert between numbers and percent.
    case kUTime:
      return UnitType::kSeconds;
    case kUAngle:
      return UnitType::kDegrees;
    case kUFrequency:
      return UnitType::kHertz;
    case kUResolution:
      return UnitType::kDotsPerPixel;
    default:
      return UnitType::kUnknown;
  }
}

// static
CSSPrimitiveValue::UnitType CSSPrimitiveValue::CanonicalUnit(
    CSSPrimitiveValue::UnitType unit_type) {
  return CanonicalUnitTypeForCategory(UnitTypeToUnitCategory(unit_type));
}

bool CSSPrimitiveValue::UnitTypeToLengthUnitType(UnitType unit_type,
                                                 LengthUnitType& length_type) {
  switch (unit_type) {
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kCentimeters:
    case CSSPrimitiveValue::UnitType::kMillimeters:
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
    case CSSPrimitiveValue::UnitType::kInches:
    case CSSPrimitiveValue::UnitType::kPoints:
    case CSSPrimitiveValue::UnitType::kPicas:
    case CSSPrimitiveValue::UnitType::kUserUnits:
      length_type = kUnitTypePixels;
      return true;
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kQuirkyEms:
      length_type = kUnitTypeFontSize;
      return true;
    case CSSPrimitiveValue::UnitType::kExs:
      length_type = kUnitTypeFontXSize;
      return true;
    case CSSPrimitiveValue::UnitType::kRems:
      length_type = kUnitTypeRootFontSize;
      return true;
    case CSSPrimitiveValue::UnitType::kRexs:
      length_type = kUnitTypeRootFontXSize;
      return true;
    case CSSPrimitiveValue::UnitType::kRchs:
      length_type = kUnitTypeRootFontZeroCharacterWidth;
      return true;
    case CSSPrimitiveValue::UnitType::kRics:
      length_type = kUnitTypeRootFontIdeographicFullWidth;
      return true;
    case CSSPrimitiveValue::UnitType::kChs:
      length_type = kUnitTypeZeroCharacterWidth;
      return true;
    case CSSPrimitiveValue::UnitType::kIcs:
      length_type = kUnitTypeIdeographicFullWidth;
      return true;
    case CSSPrimitiveValue::UnitType::kCaps:
      length_type = kUnitTypeFontCapitalHeight;
      return true;
    case CSSPrimitiveValue::UnitType::kRcaps:
      length_type = kUnitTypeRootFontCapitalHeight;
      return true;
    case CSSPrimitiveValue::UnitType::kLhs:
      length_type = kUnitTypeLineHeight;
      return true;
    case CSSPrimitiveValue::UnitType::kRlhs:
      length_type = kUnitTypeRootLineHeight;
      return true;
    case CSSPrimitiveValue::UnitType::kPercentage:
      length_type = kUnitTypePercentage;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportWidth:
      length_type = kUnitTypeViewportWidth;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportHeight:
      length_type = kUnitTypeViewportHeight;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportInlineSize:
      length_type = kUnitTypeViewportInlineSize;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportBlockSize:
      length_type = kUnitTypeViewportBlockSize;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportMin:
      length_type = kUnitTypeViewportMin;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportMax:
      length_type = kUnitTypeViewportMax;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportWidth:
      length_type = kUnitTypeSmallViewportWidth;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportHeight:
      length_type = kUnitTypeSmallViewportHeight;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportInlineSize:
      length_type = kUnitTypeSmallViewportInlineSize;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportBlockSize:
      length_type = kUnitTypeSmallViewportBlockSize;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportMin:
      length_type = kUnitTypeSmallViewportMin;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportMax:
      length_type = kUnitTypeSmallViewportMax;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportWidth:
      length_type = kUnitTypeLargeViewportWidth;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportHeight:
      length_type = kUnitTypeLargeViewportHeight;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportInlineSize:
      length_type = kUnitTypeLargeViewportInlineSize;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportBlockSize:
      length_type = kUnitTypeLargeViewportBlockSize;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportMin:
      length_type = kUnitTypeLargeViewportMin;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportMax:
      length_type = kUnitTypeLargeViewportMax;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportWidth:
      length_type = kUnitTypeDynamicViewportWidth;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportHeight:
      length_type = kUnitTypeDynamicViewportHeight;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportInlineSize:
      length_type = kUnitTypeDynamicViewportInlineSize;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportBlockSize:
      length_type = kUnitTypeDynamicViewportBlockSize;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportMin:
      length_type = kUnitTypeDynamicViewportMin;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportMax:
      length_type = kUnitTypeDynamicViewportMax;
      return true;
    case CSSPrimitiveValue::UnitType::kContainerWidth:
      length_type = kUnitTypeContainerWidth;
      return true;
    case CSSPrimitiveValue::UnitType::kContainerHeight:
      length_type = kUnitTypeContainerHeight;
      return true;
    case CSSPrimitiveValue::UnitType::kContainerInlineSize:
      length_type = kUnitTypeContainerInlineSize;
      return true;
    case CSSPrimitiveValue::UnitType::kContainerBlockSize:
      length_type = kUnitTypeContainerBlockSize;
      return true;
    case CSSPrimitiveValue::UnitType::kContainerMin:
      length_type = kUnitTypeContainerMin;
      return true;
    case CSSPrimitiveValue::UnitType::kContainerMax:
      length_type = kUnitTypeContainerMax;
      return true;
    default:
      return false;
  }
}

CSSPrimitiveValue::UnitType CSSPrimitiveValue::LengthUnitTypeToUnitType(
    LengthUnitType type) {
  switch (type) {
    case kUnitTypePixels:
      return CSSPrimitiveValue::UnitType::kPixels;
    case kUnitTypeFontSize:
      return CSSPrimitiveValue::UnitType::kEms;
    case kUnitTypeFontXSize:
      return CSSPrimitiveValue::UnitType::kExs;
    case kUnitTypeRootFontSize:
      return CSSPrimitiveValue::UnitType::kRems;
    case kUnitTypeRootFontXSize:
      return CSSPrimitiveValue::UnitType::kRexs;
    case kUnitTypeRootFontZeroCharacterWidth:
      return CSSPrimitiveValue::UnitType::kRchs;
    case kUnitTypeRootFontIdeographicFullWidth:
      return CSSPrimitiveValue::UnitType::kRics;
    case kUnitTypeZeroCharacterWidth:
      return CSSPrimitiveValue::UnitType::kChs;
    case kUnitTypeIdeographicFullWidth:
      return CSSPrimitiveValue::UnitType::kIcs;
    case kUnitTypeFontCapitalHeight:
      return CSSPrimitiveValue::UnitType::kCaps;
    case kUnitTypeRootFontCapitalHeight:
      return CSSPrimitiveValue::UnitType::kRcaps;
    case kUnitTypeLineHeight:
      return CSSPrimitiveValue::UnitType::kLhs;
    case kUnitTypeRootLineHeight:
      return CSSPrimitiveValue::UnitType::kRlhs;
    case kUnitTypePercentage:
      return CSSPrimitiveValue::UnitType::kPercentage;
    case kUnitTypeViewportWidth:
      return CSSPrimitiveValue::UnitType::kViewportWidth;
    case kUnitTypeViewportHeight:
      return CSSPrimitiveValue::UnitType::kViewportHeight;
    case kUnitTypeViewportInlineSize:
      return CSSPrimitiveValue::UnitType::kViewportInlineSize;
    case kUnitTypeViewportBlockSize:
      return CSSPrimitiveValue::UnitType::kViewportBlockSize;
    case kUnitTypeViewportMin:
      return CSSPrimitiveValue::UnitType::kViewportMin;
    case kUnitTypeViewportMax:
      return CSSPrimitiveValue::UnitType::kViewportMax;
    case kUnitTypeSmallViewportWidth:
      return CSSPrimitiveValue::UnitType::kSmallViewportWidth;
    case kUnitTypeSmallViewportHeight:
      return CSSPrimitiveValue::UnitType::kSmallViewportHeight;
    case kUnitTypeSmallViewportInlineSize:
      return CSSPrimitiveValue::UnitType::kSmallViewportInlineSize;
    case kUnitTypeSmallViewportBlockSize:
      return CSSPrimitiveValue::UnitType::kSmallViewportBlockSize;
    case kUnitTypeSmallViewportMin:
      return CSSPrimitiveValue::UnitType::kSmallViewportMin;
    case kUnitTypeSmallViewportMax:
      return CSSPrimitiveValue::UnitType::kSmallViewportMax;
    case kUnitTypeLargeViewportWidth:
      return CSSPrimitiveValue::UnitType::kLargeViewportWidth;
    case kUnitTypeLargeViewportHeight:
      return CSSPrimitiveValue::UnitType::kLargeViewportHeight;
    case kUnitTypeLargeViewportInlineSize:
      return CSSPrimitiveValue::UnitType::kLargeViewportInlineSize;
    case kUnitTypeLargeViewportBlockSize:
      return CSSPrimitiveValue::UnitType::kLargeViewportBlockSize;
    case kUnitTypeLargeViewportMin:
      return CSSPrimitiveValue::UnitType::kLargeViewportMin;
    case kUnitTypeLargeViewportMax:
      return CSSPrimitiveValue::UnitType::kLargeViewportMax;
    case kUnitTypeDynamicViewportWidth:
      return CSSPrimitiveValue::UnitType::kDynamicViewportWidth;
    case kUnitTypeDynamicViewportHeight:
      return CSSPrimitiveValue::UnitType::kDynamicViewportHeight;
    case kUnitTypeDynamicViewportInlineSize:
      return CSSPrimitiveValue::UnitType::kDynamicViewportInlineSize;
    case kUnitTypeDynamicViewportBlockSize:
      return CSSPrimitiveValue::UnitType::kDynamicViewportBlockSize;
    case kUnitTypeDynamicViewportMin:
      return CSSPrimitiveValue::UnitType::kDynamicViewportMin;
    case kUnitTypeDynamicViewportMax:
      return CSSPrimitiveValue::UnitType::kDynamicViewportMax;
    case kUnitTypeContainerWidth:
      return CSSPrimitiveValue::UnitType::kContainerWidth;
    case kUnitTypeContainerHeight:
      return CSSPrimitiveValue::UnitType::kContainerHeight;
    case kUnitTypeContainerInlineSize:
      return CSSPrimitiveValue::UnitType::kContainerInlineSize;
    case kUnitTypeContainerBlockSize:
      return CSSPrimitiveValue::UnitType::kContainerBlockSize;
    case kUnitTypeContainerMin:
      return CSSPrimitiveValue::UnitType::kContainerMin;
    case kUnitTypeContainerMax:
      return CSSPrimitiveValue::UnitType::kContainerMax;
    case kLengthUnitTypeCount:
      break;
  }
  NOTREACHED();
}

StringView CSSPrimitiveValue::UnitTypeToString(UnitType type) {
  switch (type) {
    case UnitType::kNumber:
    case UnitType::kInteger:
    case UnitType::kUserUnits:
      return StringView("");
    case UnitType::kPercentage:
      return StringView("%");
    case UnitType::kEms:
    case UnitType::kQuirkyEms:
      return StringView("em");
    case UnitType::kExs:
      return StringView("ex");
    case UnitType::kRexs:
      return StringView("rex");
    case UnitType::kRems:
      return StringView("rem");
    case UnitType::kChs:
      return StringView("ch");
    case UnitType::kRchs:
      return StringView("rch");
    case UnitType::kIcs:
      return StringView("ic");
    case UnitType::kRics:
      return StringView("ric");
    case UnitType::kLhs:
      return StringView("lh");
    case UnitType::kRlhs:
      return StringView("rlh");
    case UnitType::kCaps:
      return StringView("cap");
    case UnitType::kRcaps:
      return StringView("rcap");
    case UnitType::kPixels:
      return StringView("px");
    case UnitType::kCentimeters:
      return StringView("cm");
    case UnitType::kDotsPerPixel:
      return StringView("dppx");
    case UnitType::kX:
      return StringView("x");
    case UnitType::kDotsPerInch:
      return StringView("dpi");
    case UnitType::kDotsPerCentimeter:
      return StringView("dpcm");
    case UnitType::kMillimeters:
      return StringView("mm");
    case UnitType::kQuarterMillimeters:
      return StringView("q");
    case UnitType::kInches:
      return StringView("in");
    case UnitType::kPoints:
      return StringView("pt");
    case UnitType::kPicas:
      return StringView("pc");
    case UnitType::kDegrees:
      return StringView("deg");
    case UnitType::kRadians:
      return StringView("rad");
    case UnitType::kGradians:
      return StringView("grad");
    case UnitType::kMilliseconds:
      return StringView("ms");
    case UnitType::kSeconds:
      return StringView("s");
    case UnitType::kHertz:
      return StringView("hz");
    case UnitType::kKilohertz:
      return StringView("khz");
    case UnitType::kTurns:
      return StringView("turn");
    case UnitType::kFlex:
      return StringView("fr");
    case UnitType::kViewportWidth:
      return StringView("vw");
    case UnitType::kViewportHeight:
      return StringView("vh");
    case UnitType::kViewportInlineSize:
      return StringView("vi");
    case UnitType::kViewportBlockSize:
      return StringView("vb");
    case UnitType::kViewportMin:
      return StringView("vmin");
    case UnitType::kViewportMax:
      return StringView("vmax");
    case UnitType::kSmallViewportWidth:
      return StringView("svw");
    case UnitType::kSmallViewportHeight:
      return StringView("svh");
    case UnitType::kSmallViewportInlineSize:
      return StringView("svi");
    case UnitType::kSmallViewportBlockSize:
      return StringView("svb");
    case UnitType::kSmallViewportMin:
      return StringView("svmin");
    case UnitType::kSmallViewportMax:
      return StringView("svmax");
    case UnitType::kLargeViewportWidth:
      return StringView("lvw");
    case UnitType::kLargeViewportHeight:
      return StringView("lvh");
    case UnitType::kLargeViewportInlineSize:
      return StringView("lvi");
    case UnitType::kLargeViewportBlockSize:
      return StringView("lvb");
    case UnitType::kLargeViewportMin:
      return StringView("lvmin");
    case UnitType::kLargeViewportMax:
      return StringView("lvmax");
    case UnitType::kDynamicViewportWidth:
      return StringView("dvw");
    case UnitType::kDynamicViewportHeight:
      return StringView("dvh");
    case UnitType::kDynamicViewportInlineSize:
      return StringView("dvi");
    case UnitType::kDynamicViewportBlockSize:
      return StringView("dvb");
    case UnitType::kDynamicViewportMin:
      return StringView("dvmin");
    case UnitType::kDynamicViewportMax:
      return StringView("dvmax");
    case UnitType::kContainerWidth:
      return StringView("cqw");
    case UnitType::kContainerHeight:
      return StringView("cqh");
    case UnitType::kContainerInlineSize:
      return StringView("cqi");
    case UnitType::kContainerBlockSize:
      return StringView("cqb");
    case UnitType::kContainerMin:
      return StringView("cqmin");
    case UnitType::kContainerMax:
      return StringView("cqmax");
    default:
      break;
  }
  NOTREACHED();
}

String CSSPrimitiveValue::CustomCSSText() const {
  if (IsCalculated()) {
    return To<CSSMathFunctionValue>(this)->CustomCSSText();
  }
  return To<CSSNumericLiteralValue>(this)->CustomCSSText();
}

void CSSPrimitiveValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
}

namespace {

const CSSMathExpressionNode* CreateExpressionNodeFromDouble(
    double value,
    CSSPrimitiveValue::UnitType unit_type) {
  return CSSMathExpressionNumericLiteral::Create(value, unit_type);
}

CSSPrimitiveValue* CreateValueFromOperation(const CSSMathExpressionNode* left,
                                            const CSSMathExpressionNode* right,
                                            CSSMathOperator op) {
  const CSSMathExpressionNode* operation =
      CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
          left, right, op);
  if (!operation) {
    return nullptr;
  }
  if (auto* numeric = DynamicTo<CSSMathExpressionNumericLiteral>(operation)) {
    return MakeGarbageCollected<CSSNumericLiteralValue>(
        numeric->DoubleValue(), numeric->ResolvedUnitType());
  }
  return MakeGarbageCollected<CSSMathFunctionValue>(
      operation, CSSPrimitiveValue::ValueRange::kAll);
}

}  // namespace

const CSSMathExpressionNode* CSSPrimitiveValue::ToMathExpressionNode() const {
  if (IsMathFunctionValue()) {
    return To<CSSMathFunctionValue>(this)->ExpressionNode();
  } else {
    DCHECK(IsNumericLiteralValue());
    auto* numeric = To<CSSNumericLiteralValue>(this);
    return CreateExpressionNodeFromDouble(numeric->DoubleValue(),
                                          numeric->GetType());
  }
}

CSSPrimitiveValue* CSSPrimitiveValue::Add(double value,
                                          UnitType unit_type) const {
  return CreateValueFromOperation(
      ToMathExpressionNode(), CreateExpressionNodeFromDouble(value, unit_type),
      CSSMathOperator::kAdd);
}

CSSPrimitiveValue* CSSPrimitiveValue::AddTo(double value,
                                            UnitType unit_type) const {
  return CreateValueFromOperation(
      CreateExpressionNodeFromDouble(value, unit_type), ToMathExpressionNode(),
      CSSMathOperator::kAdd);
}

CSSPrimitiveValue* CSSPrimitiveValue::Add(
    const CSSPrimitiveValue& other) const {
  return CreateValueFromOperation(ToMathExpressionNode(),
                                  other.ToMathExpressionNode(),
                                  CSSMathOperator::kAdd);
}

CSSPrimitiveValue* CSSPrimitiveValue::AddTo(
    const CSSPrimitiveValue& other) const {
  return CreateValueFromOperation(other.ToMathExpressionNode(),
                                  ToMathExpressionNode(),
                                  CSSMathOperator::kAdd);
}

CSSPrimitiveValue* CSSPrimitiveValue::Subtract(double value,
                                               UnitType unit_type) const {
  return CreateValueFromOperation(
      ToMathExpressionNode(), CreateExpressionNodeFromDouble(value, unit_type),
      CSSMathOperator::kSubtract);
}

CSSPrimitiveValue* CSSPrimitiveValue::SubtractFrom(double value,
                                                   UnitType unit_type) const {
  return CreateValueFromOperation(
      CreateExpressionNodeFromDouble(value, unit_type), ToMathExpressionNode(),
      CSSMathOperator::kSubtract);
}

CSSPrimitiveValue* CSSPrimitiveValue::Subtract(
    const CSSPrimitiveValue& other) const {
  return CreateValueFromOperation(ToMathExpressionNode(),
                                  other.ToMathExpressionNode(),
                                  CSSMathOperator::kSubtract);
}

CSSPrimitiveValue* CSSPrimitiveValue::SubtractFrom(
    const CSSPrimitiveValue& other) const {
  return CreateValueFromOperation(other.ToMathExpressionNode(),
                                  ToMathExpressionNode(),
                                  CSSMathOperator::kSubtract);
}

CSSPrimitiveValue* CSSPrimitiveValue::Multiply(double value,
                                               UnitType unit_type) const {
  return CreateValueFromOperation(
      ToMathExpressionNode(), CreateExpressionNodeFromDouble(value, unit_type),
      CSSMathOperator::kMultiply);
}

CSSPrimitiveValue* CSSPrimitiveValue::MultiplyBy(double value,
                                                 UnitType unit_type) const {
  return CreateValueFromOperation(
      CreateExpressionNodeFromDouble(value, unit_type), ToMathExpressionNode(),
      CSSMathOperator::kMultiply);
}

CSSPrimitiveValue* CSSPrimitiveValue::Multiply(
    const CSSPrimitiveValue& other) const {
  return CreateValueFromOperation(ToMathExpressionNode(),
                                  other.ToMathExpressionNode(),
                                  CSSMathOperator::kMultiply);
}

CSSPrimitiveValue* CSSPrimitiveValue::MultiplyBy(
    const CSSPrimitiveValue& other) const {
  return CreateValueFromOperation(other.ToMathExpressionNode(),
                                  ToMathExpressionNode(),
                                  CSSMathOperator::kMultiply);
}

CSSPrimitiveValue* CSSPrimitiveValue::Divide(double value,
                                             UnitType unit_type) const {
  return CreateValueFromOperation(
      ToMathExpressionNode(), CreateExpressionNodeFromDouble(value, unit_type),
      CSSMathOperator::kDivide);
}

CSSPrimitiveValue* CSSPrimitiveValue::ConvertLiteralsFromPercentageToNumber()
    const {
  if (const auto* numeric = DynamicTo<CSSNumericLiteralValue>(this)) {
    return MakeGarbageCollected<CSSNumericLiteralValue>(
        numeric->DoubleValue() / 100, UnitType::kNumber);
  }
  CHECK(IsMathFunctionValue());
  const CSSMathExpressionNode* math_node =
      To<CSSMathFunctionValue>(this)->ExpressionNode();
  return MakeGarbageCollected<CSSMathFunctionValue>(
      math_node->ConvertLiteralsFromPercentageToNumber(),
      CSSPrimitiveValue::ValueRange::kAll);
}

}  // namespace blink
