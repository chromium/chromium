// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_numeric_value_type.h"

#include <algorithm>

namespace blink {

namespace {

CSSNumericValueType::BaseType UnitTypeToBaseType(
    CSSPrimitiveValue::UnitType unit) {
  using UnitType = CSSPrimitiveValue::UnitType;
  using BaseType = CSSNumericValueType::BaseType;

  DCHECK_NE(unit, UnitType::kNumber);
  switch (unit) {
    case UnitType::kEms:
    case UnitType::kExs:
    case UnitType::kPixels:
    case UnitType::kCentimeters:
    case UnitType::kMillimeters:
    case UnitType::kQuarterMillimeters:
    case UnitType::kInches:
    case UnitType::kPoints:
    case UnitType::kPicas:
    case UnitType::kUserUnits:
    case UnitType::kViewportWidth:
    case UnitType::kViewportHeight:
    case UnitType::kViewportMin:
    case UnitType::kViewportMax:
    case UnitType::kRems:
    case UnitType::kChs:
      return BaseType::kLength;
    case UnitType::kMilliseconds:
    case UnitType::kSeconds:
      return BaseType::kTime;
    case UnitType::kDegrees:
    case UnitType::kRadians:
    case UnitType::kGradians:
    case UnitType::kTurns:
      return BaseType::kAngle;
    case UnitType::kHertz:
    case UnitType::kKilohertz:
      return BaseType::kFrequency;
    case UnitType::kDotsPerPixel:
    case UnitType::kDotsPerInch:
    case UnitType::kDotsPerCentimeter:
      return BaseType::kResolution;
    case UnitType::kFraction:
      return BaseType::kFlex;
    case UnitType::kPercentage:
      return BaseType::kPercent;
    default:
      NOTREACHED();
      return BaseType::kLength;
  }
}

}  // namespace

AtomicString CSSNumericValueType::BaseTypeToString(BaseType base_type) {
  switch (base_type) {
    case BaseType::kLength:
      return "length";
    case BaseType::kAngle:
      return "angle";
    case BaseType::kTime:
      return "time";
    case BaseType::kFrequency:
      return "frequency";
    case BaseType::kResolution:
      return "resolution";
    case BaseType::kFlex:
      return "flex";
    case BaseType::kPercent:
      return "percent";
    default:
      break;
  }

  NOTREACHED();
  return "";
}

CSSNumericValueType::CSSNumericValueType(CSSPrimitiveValue::UnitType unit) {
  exponents_.Fill(0, kNumBaseTypes);
  if (unit != CSSPrimitiveValue::UnitType::kNumber)
    SetExponent(UnitTypeToBaseType(unit), 1);
}

CSSNumericValueType::CSSNumericValueType(int exponent,
                                         CSSPrimitiveValue::UnitType unit) {
  exponents_.Fill(0, kNumBaseTypes);
  if (unit != CSSPrimitiveValue::UnitType::kNumber)
    SetExponent(UnitTypeToBaseType(unit), exponent);
}

CSSNumericValueType CSSNumericValueType::NegateExponents(
    CSSNumericValueType type) {
  std::for_each(type.exponents_.begin(), type.exponents_.end(),
                [](int& v) { v *= -1; });
  return type;
}

CSSNumericValueType CSSNumericValueType::Add(CSSNumericValueType type1,
                                             CSSNumericValueType type2,
                                             bool& error) {
  if (type1.HasPercentHint() && type2.HasPercentHint() &&
      type1.PercentHint() != type2.PercentHint()) {
    error = true;
    return type1;
  }

  if (type1.HasPercentHint())
    type2.ApplyPercentHint(type1.PercentHint());
  else if (type2.HasPercentHint())
    type1.ApplyPercentHint(type2.PercentHint());

  DCHECK_EQ(type1.PercentHint(), type2.PercentHint());
  // Match up base types. Try to use the percent hint to match up any
  // differences.
  for (unsigned i = 0; i < kNumBaseTypes; ++i) {
    const BaseType base_type = static_cast<BaseType>(i);
    if (type1.exponents_[i] != type2.exponents_[i]) {
      if (base_type != BaseType::kPercent) {
        type1.ApplyPercentHint(base_type);
        type2.ApplyPercentHint(base_type);
      }

      if (type1.exponents_[i] != type2.exponents_[i]) {
        error = true;
        return type1;
      }
    }
  }

  error = false;
  return type1;
}

CSSNumericValueType CSSNumericValueType::Multiply(CSSNumericValueType type1,
                                                  CSSNumericValueType type2,
                                                  bool& error) {
  if (type1.HasPercentHint() && type2.HasPercentHint() &&
      type1.PercentHint() != type2.PercentHint()) {
    error = true;
    return type1;
  }

  if (type1.HasPercentHint())
    type2.ApplyPercentHint(type1.PercentHint());
  else if (type2.HasPercentHint())
    type1.ApplyPercentHint(type2.PercentHint());

  for (unsigned i = 0; i < kNumBaseTypes; ++i) {
    const auto base_type = static_cast<BaseType>(i);
    type1.SetExponent(base_type,
                      type1.Exponent(base_type) + type2.Exponent(base_type));
  }

  error = false;
  return type1;
}

void CSSNumericValueType::ApplyPercentHint(BaseType hint) {
  DCHECK_NE(hint, BaseType::kPercent);
  SetExponent(hint, Exponent(hint) + Exponent(BaseType::kPercent));
  SetExponent(BaseType::kPercent, 0);
  percent_hint_ = hint;
  has_percent_hint_ = true;
}

}  // namespace blink
