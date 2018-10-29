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

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_calculation_value.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

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
  double num;
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

CSSPrimitiveValue* CSSPrimitiveValue::Create(double value, UnitType type) {
  // TODO(timloh): This looks wrong.
  if (std::isinf(value))
    value = 0;

  if (value < 0 || value > CSSValuePool::kMaximumCacheableIntegerValue)
    return new CSSPrimitiveValue(value, type);

  int int_value = static_cast<int>(value);
  if (value != int_value)
    return new CSSPrimitiveValue(value, type);

  CSSValuePool& pool = CssValuePool();
  CSSPrimitiveValue* result = nullptr;
  switch (type) {
    case CSSPrimitiveValue::UnitType::kPixels:
      result = pool.PixelCacheValue(int_value);
      if (!result)
        result = pool.SetPixelCacheValue(int_value,
                                         new CSSPrimitiveValue(value, type));
      return result;
    case CSSPrimitiveValue::UnitType::kPercentage:
      result = pool.PercentCacheValue(int_value);
      if (!result)
        result = pool.SetPercentCacheValue(int_value,
                                           new CSSPrimitiveValue(value, type));
      return result;
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kInteger:
      result = pool.NumberCacheValue(int_value);
      if (!result)
        result = pool.SetNumberCacheValue(
            int_value, new CSSPrimitiveValue(
                           value, CSSPrimitiveValue::UnitType::kInteger));
      return result;
    default:
      return new CSSPrimitiveValue(value, type);
  }
}

CSSPrimitiveValue::UnitType CSSPrimitiveValue::TypeWithCalcResolved() const {
  if (GetType() != UnitType::kCalc)
    return GetType();

  switch (value_.calc->Category()) {
    case kCalcAngle:
      return UnitType::kDegrees;
    case kCalcFrequency:
      return UnitType::kHertz;
    case kCalcNumber:
      return UnitType::kNumber;
    case kCalcPercent:
      return UnitType::kPercentage;
    case kCalcLength:
      return UnitType::kPixels;
    case kCalcPercentNumber:
      return UnitType::kCalcPercentageWithNumber;
    case kCalcPercentLength:
      return UnitType::kCalcPercentageWithLength;
    case kCalcLengthNumber:
      return UnitType::kCalcLengthWithNumber;
    case kCalcPercentLengthNumber:
      return UnitType::kCalcPercentageWithLengthAndNumber;
    case kCalcTime:
      return UnitType::kMilliseconds;
    case kCalcOther:
      return UnitType::kUnknown;
  }
  return UnitType::kUnknown;
}

CSSPrimitiveValue::CSSPrimitiveValue(double num, UnitType type)
    : CSSValue(kPrimitiveClass) {
  Init(type);
  DCHECK(std::isfinite(num));
  value_.num = num;
}

CSSPrimitiveValue::CSSPrimitiveValue(const Length& length, float zoom)
    : CSSValue(kPrimitiveClass) {
  switch (length.GetType()) {
    case kPercent:
      Init(UnitType::kPercentage);
      DCHECK(std::isfinite(length.Percent()));
      value_.num = length.Percent();
      break;
    case kFixed:
      Init(UnitType::kPixels);
      value_.num = length.Value() / zoom;
      break;
    case kCalculated: {
      const CalculationValue& calc = length.GetCalculationValue();
      if (calc.Pixels() && calc.Percent()) {
        Init(CSSCalcValue::Create(CSSCalcValue::CreateExpressionNode(
                                      calc.Pixels() / zoom, calc.Percent()),
                                  calc.GetValueRange()));
        break;
      }
      if (calc.Percent()) {
        Init(UnitType::kPercentage);
        value_.num = calc.Percent();
      } else {
        Init(UnitType::kPixels);
        value_.num = calc.Pixels() / zoom;
      }
      if (value_.num < 0 && calc.IsNonNegative())
        value_.num = 0;
      break;
    }
    case kAuto:
    case kMinContent:
    case kMaxContent:
    case kFillAvailable:
    case kFitContent:
    case kExtendToZoom:
    case kDeviceWidth:
    case kDeviceHeight:
    case kMaxSizeNone:
      NOTREACHED();
      break;
  }
}

void CSSPrimitiveValue::Init(UnitType type) {
  primitive_unit_type_ = static_cast<unsigned>(type);
}

void CSSPrimitiveValue::Init(CSSCalcValue* c) {
  Init(UnitType::kCalc);
  value_.calc = c;
}

CSSPrimitiveValue::~CSSPrimitiveValue() = default;

double CSSPrimitiveValue::ComputeSeconds() const {
  DCHECK(IsTime() ||
         (IsCalculated() && CssCalcValue()->Category() == kCalcTime));
  UnitType current_type =
      IsCalculated() ? CssCalcValue()->ExpressionNode()->TypeWithCalcResolved()
                     : GetType();
  if (current_type == UnitType::kSeconds)
    return GetDoubleValue();
  if (current_type == UnitType::kMilliseconds)
    return GetDoubleValue() / 1000;
  NOTREACHED();
  return 0;
}

double CSSPrimitiveValue::ComputeDegrees() const {
  DCHECK(IsAngle() ||
         (IsCalculated() && CssCalcValue()->Category() == kCalcAngle));
  UnitType current_type =
      IsCalculated() ? CssCalcValue()->ExpressionNode()->TypeWithCalcResolved()
                     : GetType();
  switch (current_type) {
    case UnitType::kDegrees:
      return GetDoubleValue();
    case UnitType::kRadians:
      return rad2deg(GetDoubleValue());
    case UnitType::kGradians:
      return grad2deg(GetDoubleValue());
    case UnitType::kTurns:
      return turn2deg(GetDoubleValue());
    default:
      NOTREACHED();
      return 0;
  }
}

double CSSPrimitiveValue::ComputeDotsPerPixel() const {
  UnitType current_type = TypeWithCalcResolved();
  DCHECK(IsResolution(current_type));
  return GetDoubleValue() * ConversionToCanonicalUnitsScaleFactor(current_type);
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
  return Length(ClampToCSSLengthRange(ComputeLengthDouble(conversion_data)),
                kFixed);
}

template <>
short CSSPrimitiveValue::ComputeLength(
    const CSSToLengthConversionData& conversion_data) const {
  return RoundForImpreciseConversion<short>(
      ComputeLengthDouble(conversion_data));
}

template <>
unsigned short CSSPrimitiveValue::ComputeLength(
    const CSSToLengthConversionData& conversion_data) const {
  return RoundForImpreciseConversion<unsigned short>(
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
  if (GetType() == UnitType::kCalc)
    return value_.calc->ComputeLengthPx(conversion_data);
  return conversion_data.ZoomedComputedPixels(GetDoubleValue(), GetType());
}

void CSSPrimitiveValue::AccumulateLengthArray(CSSLengthArray& length_array,
                                              double multiplier) const {
  DCHECK_EQ(length_array.values.size(),
            static_cast<unsigned>(kLengthUnitTypeCount));

  if (GetType() == UnitType::kCalc) {
    CssCalcValue()->AccumulateLengthArray(length_array, multiplier);
    return;
  }

  LengthUnitType length_type;
  bool conversion_success = UnitTypeToLengthUnitType(GetType(), length_type);
  DCHECK(conversion_success);
  length_array.values[length_type] +=
      value_.num * ConversionToCanonicalUnitsScaleFactor(GetType()) *
      multiplier;
  length_array.type_flags.Set(length_type);
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
  if (IsPercentage())
    return Length(GetDoubleValue(), kPercent);
  DCHECK(IsCalculated());
  return Length(CssCalcValue()->ToCalcValue(conversion_data));
}

double CSSPrimitiveValue::GetDoubleValue() const {
  return GetType() != UnitType::kCalc ? value_.num : value_.calc->DoubleValue();
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

static String FormatNumber(double number, const char* suffix) {
#if defined(OS_WIN) && _MSC_VER < 1900
  unsigned oldFormat = _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
  String result = String::Format("%.6g%s", number, suffix);
#if defined(OS_WIN) && _MSC_VER < 1900
  _set_output_format(oldFormat);
#endif
  return result;
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
    case UnitType::kUnknown:
    case UnitType::kCalc:
    case UnitType::kCalcPercentageWithNumber:
    case UnitType::kCalcPercentageWithLength:
    case UnitType::kCalcLengthWithNumber:
    case UnitType::kCalcPercentageWithLengthAndNumber:
      break;
  };
  NOTREACHED();
  return "";
}

String CSSPrimitiveValue::CustomCSSText() const {
  String text;
  switch (GetType()) {
    case UnitType::kUnknown:
      // FIXME
      break;
    case UnitType::kInteger:
      text = String::Format("%d", GetIntValue());
      break;
    case UnitType::kNumber:
    case UnitType::kPercentage:
    case UnitType::kEms:
    case UnitType::kQuirkyEms:
    case UnitType::kExs:
    case UnitType::kRems:
    case UnitType::kChs:
    case UnitType::kPixels:
    case UnitType::kCentimeters:
    case UnitType::kDotsPerPixel:
    case UnitType::kDotsPerInch:
    case UnitType::kDotsPerCentimeter:
    case UnitType::kMillimeters:
    case UnitType::kQuarterMillimeters:
    case UnitType::kInches:
    case UnitType::kPoints:
    case UnitType::kPicas:
    case UnitType::kUserUnits:
    case UnitType::kDegrees:
    case UnitType::kRadians:
    case UnitType::kGradians:
    case UnitType::kMilliseconds:
    case UnitType::kSeconds:
    case UnitType::kHertz:
    case UnitType::kKilohertz:
    case UnitType::kTurns:
    case UnitType::kFraction:
    case UnitType::kViewportWidth:
    case UnitType::kViewportHeight:
    case UnitType::kViewportMin:
    case UnitType::kViewportMax:
      text = FormatNumber(value_.num, UnitTypeToString(GetType()));
      break;
    case UnitType::kCalc:
      text = value_.calc->CustomCSSText();
      break;
    case UnitType::kCalcPercentageWithNumber:
    case UnitType::kCalcPercentageWithLength:
    case UnitType::kCalcLengthWithNumber:
    case UnitType::kCalcPercentageWithLengthAndNumber:
      NOTREACHED();
      break;
  }

  return text;
}

bool CSSPrimitiveValue::Equals(const CSSPrimitiveValue& other) const {
  if (GetType() != other.GetType())
    return false;

  switch (GetType()) {
    case UnitType::kUnknown:
      return false;
    case UnitType::kNumber:
    case UnitType::kInteger:
    case UnitType::kPercentage:
    case UnitType::kEms:
    case UnitType::kExs:
    case UnitType::kRems:
    case UnitType::kPixels:
    case UnitType::kCentimeters:
    case UnitType::kDotsPerPixel:
    case UnitType::kDotsPerInch:
    case UnitType::kDotsPerCentimeter:
    case UnitType::kMillimeters:
    case UnitType::kQuarterMillimeters:
    case UnitType::kInches:
    case UnitType::kPoints:
    case UnitType::kPicas:
    case UnitType::kUserUnits:
    case UnitType::kDegrees:
    case UnitType::kRadians:
    case UnitType::kGradians:
    case UnitType::kMilliseconds:
    case UnitType::kSeconds:
    case UnitType::kHertz:
    case UnitType::kKilohertz:
    case UnitType::kTurns:
    case UnitType::kViewportWidth:
    case UnitType::kViewportHeight:
    case UnitType::kViewportMin:
    case UnitType::kViewportMax:
    case UnitType::kFraction:
      return value_.num == other.value_.num;
    case UnitType::kCalc:
      return value_.calc && other.value_.calc &&
             value_.calc->Equals(*other.value_.calc);
    case UnitType::kChs:
    case UnitType::kCalcPercentageWithNumber:
    case UnitType::kCalcPercentageWithLength:
    case UnitType::kCalcLengthWithNumber:
    case UnitType::kCalcPercentageWithLengthAndNumber:
    case UnitType::kQuirkyEms:
      return false;
  }
  return false;
}

void CSSPrimitiveValue::TraceAfterDispatch(blink::Visitor* visitor) {
  switch (GetType()) {
    case UnitType::kCalc:
      visitor->Trace(value_.calc);
      break;
    default:
      break;
  }
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
