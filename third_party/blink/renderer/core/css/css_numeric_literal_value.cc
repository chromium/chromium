// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

struct SameSizeAsCSSNumericLiteralValue : CSSPrimitiveValue {
  double num;
};
ASSERT_SIZE(CSSNumericLiteralValue, SameSizeAsCSSNumericLiteralValue);

void CSSNumericLiteralValue::TraceAfterDispatch(blink::Visitor* visitor) {
  CSSPrimitiveValue::TraceAfterDispatch(visitor);
}

CSSNumericLiteralValue::CSSNumericLiteralValue(double num, UnitType type)
    : CSSPrimitiveValue(kNumericLiteralClass), num_(num) {
  DCHECK(std::isfinite(num));
  DCHECK_NE(UnitType::kUnknown, type);
  numeric_literal_unit_type_ = static_cast<unsigned>(type);
}

// static
CSSNumericLiteralValue* CSSNumericLiteralValue::Create(double value,
                                                       UnitType type) {
  // TODO(timloh): This looks wrong.
  if (std::isinf(value))
    value = 0;

  if (value < 0 || value > CSSValuePool::kMaximumCacheableIntegerValue)
    return MakeGarbageCollected<CSSNumericLiteralValue>(value, type);

  int int_value = clampTo<int>(value);
  if (value != int_value)
    return MakeGarbageCollected<CSSNumericLiteralValue>(value, type);

  CSSValuePool& pool = CssValuePool();
  CSSNumericLiteralValue* result = nullptr;
  switch (type) {
    case CSSPrimitiveValue::UnitType::kPixels:
      result = pool.PixelCacheValue(int_value);
      if (!result) {
        result = pool.SetPixelCacheValue(
            int_value,
            MakeGarbageCollected<CSSNumericLiteralValue>(value, type));
      }
      return result;
    case CSSPrimitiveValue::UnitType::kPercentage:
      result = pool.PercentCacheValue(int_value);
      if (!result) {
        result = pool.SetPercentCacheValue(
            int_value,
            MakeGarbageCollected<CSSNumericLiteralValue>(value, type));
      }
      return result;
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kInteger:
      result = pool.NumberCacheValue(int_value);
      if (!result) {
        result = pool.SetNumberCacheValue(
            int_value, MakeGarbageCollected<CSSNumericLiteralValue>(
                           value, CSSPrimitiveValue::UnitType::kInteger));
      }
      return result;
    default:
      return MakeGarbageCollected<CSSNumericLiteralValue>(value, type);
  }
}

double CSSNumericLiteralValue::ComputeSeconds() const {
  DCHECK(IsTime());
  UnitType current_type = GetType();
  if (current_type == UnitType::kSeconds)
    return num_;
  if (current_type == UnitType::kMilliseconds)
    return num_ / 1000;
  NOTREACHED();
  return 0;
}

double CSSNumericLiteralValue::ComputeDegrees() const {
  DCHECK(IsAngle());
  UnitType current_type = GetType();
  switch (current_type) {
    case UnitType::kDegrees:
      return num_;
    case UnitType::kRadians:
      return rad2deg(num_);
    case UnitType::kGradians:
      return grad2deg(num_);
    case UnitType::kTurns:
      return turn2deg(num_);
    default:
      NOTREACHED();
      return 0;
  }
}

double CSSNumericLiteralValue::ComputeDotsPerPixel() const {
  DCHECK(IsResolution());
  return DoubleValue() * ConversionToCanonicalUnitsScaleFactor(GetType());
}

double CSSNumericLiteralValue::ComputeLengthPx(
    const CSSToLengthConversionData& conversion_data) const {
  DCHECK(IsLength());
  return conversion_data.ZoomedComputedPixels(num_, GetType());
}

bool CSSNumericLiteralValue::AccumulateLengthArray(CSSLengthArray& length_array,
                                                   double multiplier) const {
  LengthUnitType length_type;
  bool conversion_success = UnitTypeToLengthUnitType(GetType(), length_type);
  DCHECK(conversion_success);
  length_array.values[length_type] +=
      num_ * ConversionToCanonicalUnitsScaleFactor(GetType()) * multiplier;
  length_array.type_flags.set(length_type);
  return true;
}

void CSSNumericLiteralValue::AccumulateLengthUnitTypes(
    LengthTypeFlags& types) const {
  if (!IsLength())
    return;
  LengthUnitType length_type;
  bool conversion_success = UnitTypeToLengthUnitType(GetType(), length_type);
  DCHECK(conversion_success);
  types.set(length_type);
}

bool CSSNumericLiteralValue::IsComputationallyIndependent() const {
  if (!IsLength())
    return true;
  if (IsViewportPercentageLength())
    return true;
  return !IsRelativeUnit(GetType());
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

String CSSNumericLiteralValue::CustomCSSText() const {
  String text;
  switch (GetType()) {
    case UnitType::kUnknown:
      // FIXME
      break;
    case UnitType::kInteger:
      text = String::Number(GetIntValue());
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
    case UnitType::kViewportMax: {
      // The following integers are minimal and maximum integers which can
      // be represented in non-exponential format with 6 digit precision.
      constexpr int kMinInteger = -999999;
      constexpr int kMaxInteger = 999999;
      double value = To<CSSNumericLiteralValue>(this)->DoubleValue();
      // If the value is small integer, go the fast path.
      if (value < kMinInteger || value > kMaxInteger ||
          std::trunc(value) != value) {
        text = FormatNumber(value, UnitTypeToString(GetType()));
      } else {
        StringBuilder builder;
        int int_value = value;
        const char* unit_type = UnitTypeToString(GetType());
        builder.AppendNumber(int_value);
        builder.Append(unit_type, strlen(unit_type));
        text = builder.ToString();
      }
    } break;
    default:
      NOTREACHED();
      break;
  }
  return text;
}

bool CSSNumericLiteralValue::Equals(const CSSNumericLiteralValue& other) const {
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
      return num_ == other.num_;
    case UnitType::kQuirkyEms:
      return false;
    default:
      return false;
  }
}

}  // namespace blink
