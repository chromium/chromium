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
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// Max/min values for CSS, needs to slightly smaller/larger than the true
// max/min values to allow for rounding without overflowing.
// Subtract two (rather than one) to allow for values to be converted to float
// and back without exceeding the LayoutUnit::Max.
const int kMaxValueForCssLength = INT_MAX / kFixedPointDenominator - 2;
const int kMinValueForCssLength = INT_MIN / kFixedPointDenominator + 2;

}  // namespace

struct SameSizeAsCSSPrimitiveValue : CSSValue {
};
ASSERT_SIZE(CSSPrimitiveValue, SameSizeAsCSSPrimitiveValue);

float CSSPrimitiveValue::ClampToCSSLengthRange(double value) {
  return clampTo<float>(value, kMinValueForCssLength, kMaxValueForCssLength);
}

CSSPrimitiveValue::UnitCategory CSSPrimitiveValue::UnitTypeToUnitCategory(
    UnitType type) {
  switch (type) {
    case UnitType::kNumber:
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
    case UnitType::kDotsPerInch:
    case UnitType::kDotsPerCentimeter:
      return CSSPrimitiveValue::kUResolution;
    default:
      return CSSPrimitiveValue::kUOther;
  }
}

bool CSSPrimitiveValue::IsCalculatedPercentageWithLength() const {
  // TODO(crbug.com/979895): Move this function to |CSSMathFunctionValue|.
  return IsCalculated() &&
         To<CSSMathFunctionValue>(this)->Category() == kCalcPercentLength;
}

bool CSSPrimitiveValue::IsResolution() const {
  // TODO(crbug.com/983613): Either support math functions on resolutions; or
  // provide a justification for not supporting it, and move this function to
  // |CSSNumericLiteralValue|.
  return IsNumericLiteralValue() &&
         To<CSSNumericLiteralValue>(this)->IsResolution();
}

bool CSSPrimitiveValue::IsFlex() const {
  // TODO(crbug.com/993136): Either support math functions on flexible lengths;
  // or provide a justification for not supporting it, and move this function to
  // |CSSNumericLiteralValue|.
  return IsNumericLiteralValue() && To<CSSNumericLiteralValue>(this)->IsFlex();
}

bool CSSPrimitiveValue::IsAngle() const {
  if (IsNumericLiteralValue())
    return To<CSSNumericLiteralValue>(this)->IsAngle();
  return To<CSSMathFunctionValue>(this)->IsAngle();
}

bool CSSPrimitiveValue::IsLength() const {
  if (IsNumericLiteralValue())
    return To<CSSNumericLiteralValue>(this)->IsLength();
  return To<CSSMathFunctionValue>(this)->IsLength();
}

bool CSSPrimitiveValue::IsPx() const {
  if (IsNumericLiteralValue())
    return To<CSSNumericLiteralValue>(this)->IsPx();
  return To<CSSMathFunctionValue>(this)->IsPx();
}

bool CSSPrimitiveValue::IsNumber() const {
  if (IsNumericLiteralValue())
    return To<CSSNumericLiteralValue>(this)->IsNumber();
  return To<CSSMathFunctionValue>(this)->IsNumber();
}

bool CSSPrimitiveValue::IsInteger() const {
  // TODO(crbug.com/931216): Support integer math functions properly.
  return IsNumericLiteralValue() &&
         To<CSSNumericLiteralValue>(this)->IsInteger();
}

bool CSSPrimitiveValue::IsPercentage() const {
  if (IsNumericLiteralValue())
    return To<CSSNumericLiteralValue>(this)->IsPercentage();
  return To<CSSMathFunctionValue>(this)->IsPercentage();
}

bool CSSPrimitiveValue::IsTime() const {
  if (IsNumericLiteralValue())
    return To<CSSNumericLiteralValue>(this)->IsTime();
  return To<CSSMathFunctionValue>(this)->IsTime();
}

bool CSSPrimitiveValue::IsComputationallyIndependent() const {
  if (IsNumericLiteralValue())
    return To<CSSNumericLiteralValue>(this)->IsComputationallyIndependent();
  return To<CSSMathFunctionValue>(this)->IsComputationallyIndependent();
}

CSSPrimitiveValue::CSSPrimitiveValue(ClassType class_type)
    : CSSValue(class_type) {}

// static
CSSPrimitiveValue* CSSPrimitiveValue::CreateFromLength(const Length& length,
                                                       float zoom) {
  switch (length.GetType()) {
    case Length::kPercent:
      return CSSNumericLiteralValue::Create(length.Percent(),
                                            UnitType::kPercentage);
    case Length::kFixed:
      return CSSNumericLiteralValue::Create(length.Value() / zoom,
                                            UnitType::kPixels);
    case Length::kCalculated: {
      const CalculationValue& calc = length.GetCalculationValue();
      if (calc.IsExpression() || (calc.Pixels() && calc.Percent()))
        return CSSMathFunctionValue::Create(length, zoom);
      if (!calc.Pixels()) {
        double num = calc.Percent();
        if (num < 0 && calc.IsNonNegative())
          num = 0;
        return CSSNumericLiteralValue::Create(num, UnitType::kPercentage);
      }
      double num = calc.Pixels() / zoom;
      if (num < 0 && calc.IsNonNegative())
        num = 0;
      return CSSNumericLiteralValue::Create(num, UnitType::kPixels);
    }
    default:
      break;
  }
  NOTREACHED();
  return nullptr;
}

double CSSPrimitiveValue::ComputeSeconds() const {
  if (IsCalculated())
    return To<CSSMathFunctionValue>(this)->ComputeSeconds();
  return To<CSSNumericLiteralValue>(this)->ComputeSeconds();
}

double CSSPrimitiveValue::ComputeDegrees() const {
  if (IsCalculated())
    return To<CSSMathFunctionValue>(this)->ComputeDegrees();
  return To<CSSNumericLiteralValue>(this)->ComputeDegrees();
}

double CSSPrimitiveValue::ComputeDotsPerPixel() const {
  // TODO(crbug.com/983613): Either support math functions on resolutions; or
  // provide a justification for not supporting it.
  DCHECK(IsNumericLiteralValue());
  return To<CSSNumericLiteralValue>(this)->ComputeDotsPerPixel();
}

template <>
int CSSPrimitiveValue::ComputeLength(
    const CSSToLengthConversionData& conversion_data) const {
  return RoundForImpreciseConversion<int>(ComputeLengthDouble(conversion_data));
}

template <>
unsigned CSSPrimitiveValue::ComputeLength(
    const CSSToLengthConversionData& conversion_data) const {
  return RoundForImpreciseConversion<unsigned>(
      ComputeLengthDouble(conversion_data));
}

template <>
Length CSSPrimitiveValue::ComputeLength(
    const CSSToLengthConversionData& conversion_data) const {
  return Length::Fixed(
      ClampToCSSLengthRange(ComputeLengthDouble(conversion_data)));
}

template <>
int16_t CSSPrimitiveValue::ComputeLength(
    const CSSToLengthConversionData& conversion_data) const {
  return RoundForImpreciseConversion<int16_t>(
      ComputeLengthDouble(conversion_data));
}

template <>
uint16_t CSSPrimitiveValue::ComputeLength(
    const CSSToLengthConversionData& conversion_data) const {
  return RoundForImpreciseConversion<uint16_t>(
      ComputeLengthDouble(conversion_data));
}

template <>
uint8_t CSSPrimitiveValue::ComputeLength(
    const CSSToLengthConversionData& conversion_data) const {
  return RoundForImpreciseConversion<uint8_t>(
      ComputeLengthDouble(conversion_data));
}

template <>
float CSSPrimitiveValue::ComputeLength(
    const CSSToLengthConversionData& conversion_data) const {
  return clampTo<float>(ComputeLengthDouble(conversion_data));
}

template <>
double CSSPrimitiveValue::ComputeLength(
    const CSSToLengthConversionData& conversion_data) const {
  return ComputeLengthDouble(conversion_data);
}

double CSSPrimitiveValue::ComputeLengthDouble(
    const CSSToLengthConversionData& conversion_data) const {
  if (IsCalculated())
    return To<CSSMathFunctionValue>(this)->ComputeLengthPx(conversion_data);
  return To<CSSNumericLiteralValue>(this)->ComputeLengthPx(conversion_data);
}

bool CSSPrimitiveValue::AccumulateLengthArray(CSSLengthArray& length_array,
                                              double multiplier) const {
  DCHECK_EQ(length_array.values.size(),
            static_cast<unsigned>(kLengthUnitTypeCount));
  if (IsCalculated()) {
    return To<CSSMathFunctionValue>(this)->AccumulateLengthArray(length_array,
                                                                 multiplier);
  }
  return To<CSSNumericLiteralValue>(this)->AccumulateLengthArray(length_array,
                                                                 multiplier);
}

void CSSPrimitiveValue::AccumulateLengthUnitTypes(
    LengthTypeFlags& types) const {
  if (IsCalculated())
    return To<CSSMathFunctionValue>(this)->AccumulateLengthUnitTypes(types);
  To<CSSNumericLiteralValue>(this)->AccumulateLengthUnitTypes(types);
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
    case UnitType::kMilliseconds:
    case UnitType::kHertz:
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
    case UnitType::kSeconds:
    case UnitType::kKilohertz:
      factor = 1000;
      break;
    default:
      break;
  }

  return factor;
}

Length CSSPrimitiveValue::ConvertToLength(
    const CSSToLengthConversionData& conversion_data) const {
  if (IsLength())
    return ComputeLength<Length>(conversion_data);
  if (IsPercentage()) {
    if (IsNumericLiteralValue() ||
        !To<CSSMathFunctionValue>(this)->AllowsNegativePercentageReference()) {
      return Length::Percent(GetDoubleValue());
    }
  }
  DCHECK(IsCalculated());
  return To<CSSMathFunctionValue>(this)->ConvertToLength(conversion_data);
}

double CSSPrimitiveValue::GetDoubleValue() const {
  return IsCalculated() ? To<CSSMathFunctionValue>(this)->DoubleValue()
                        : To<CSSNumericLiteralValue>(this)->DoubleValue();
}

bool CSSPrimitiveValue::IsZero() const {
  return IsCalculated() ? To<CSSMathFunctionValue>(this)->IsZero()
                        : To<CSSNumericLiteralValue>(this)->IsZero();
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
      return UnitType::kMilliseconds;
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
    case CSSPrimitiveValue::UnitType::kChs:
      length_type = kUnitTypeZeroCharacterWidth;
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
    case CSSPrimitiveValue::UnitType::kViewportMin:
      length_type = kUnitTypeViewportMin;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportMax:
      length_type = kUnitTypeViewportMax;
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
    case kUnitTypeZeroCharacterWidth:
      return CSSPrimitiveValue::UnitType::kChs;
    case kUnitTypePercentage:
      return CSSPrimitiveValue::UnitType::kPercentage;
    case kUnitTypeViewportWidth:
      return CSSPrimitiveValue::UnitType::kViewportWidth;
    case kUnitTypeViewportHeight:
      return CSSPrimitiveValue::UnitType::kViewportHeight;
    case kUnitTypeViewportMin:
      return CSSPrimitiveValue::UnitType::kViewportMin;
    case kUnitTypeViewportMax:
      return CSSPrimitiveValue::UnitType::kViewportMax;
    case kLengthUnitTypeCount:
      break;
  }
  NOTREACHED();
  return CSSPrimitiveValue::UnitType::kUnknown;
}

const char* CSSPrimitiveValue::UnitTypeToString(UnitType type) {
  switch (type) {
    case UnitType::kNumber:
    case UnitType::kInteger:
    case UnitType::kUserUnits:
      return "";
    case UnitType::kPercentage:
      return "%";
    case UnitType::kEms:
    case UnitType::kQuirkyEms:
      return "em";
    case UnitType::kExs:
      return "ex";
    case UnitType::kRems:
      return "rem";
    case UnitType::kChs:
      return "ch";
    case UnitType::kPixels:
      return "px";
    case UnitType::kCentimeters:
      return "cm";
    case UnitType::kDotsPerPixel:
      return "dppx";
    case UnitType::kDotsPerInch:
      return "dpi";
    case UnitType::kDotsPerCentimeter:
      return "dpcm";
    case UnitType::kMillimeters:
      return "mm";
    case UnitType::kQuarterMillimeters:
      return "q";
    case UnitType::kInches:
      return "in";
    case UnitType::kPoints:
      return "pt";
    case UnitType::kPicas:
      return "pc";
    case UnitType::kDegrees:
      return "deg";
    case UnitType::kRadians:
      return "rad";
    case UnitType::kGradians:
      return "grad";
    case UnitType::kMilliseconds:
      return "ms";
    case UnitType::kSeconds:
      return "s";
    case UnitType::kHertz:
      return "hz";
    case UnitType::kKilohertz:
      return "khz";
    case UnitType::kTurns:
      return "turn";
    case UnitType::kFraction:
      return "fr";
    case UnitType::kViewportWidth:
      return "vw";
    case UnitType::kViewportHeight:
      return "vh";
    case UnitType::kViewportMin:
      return "vmin";
    case UnitType::kViewportMax:
      return "vmax";
    default:
      break;
  }
  NOTREACHED();
  return "";
}

String CSSPrimitiveValue::CustomCSSText() const {
  if (IsCalculated())
    return To<CSSMathFunctionValue>(this)->CustomCSSText();
  return To<CSSNumericLiteralValue>(this)->CustomCSSText();
}

void CSSPrimitiveValue::TraceAfterDispatch(blink::Visitor* visitor) {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
