// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"

#include "third_party/blink/renderer/core/animation/length_property_functions.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_invert.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_max.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_min.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_product.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_sum.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_sum_value.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

CSSPrimitiveValue::UnitType ToCanonicalUnit(CSSPrimitiveValue::UnitType unit) {
  return CSSPrimitiveValue::CanonicalUnitTypeForCategory(
      CSSPrimitiveValue::UnitTypeToUnitCategory(unit));
}

CSSPrimitiveValue::UnitType ToCanonicalUnitIfPossible(
    CSSPrimitiveValue::UnitType unit) {
  const auto canonical_unit = ToCanonicalUnit(unit);
  if (canonical_unit == CSSPrimitiveValue::UnitType::kUnknown) {
    return unit;
  }
  return canonical_unit;
}

bool IsValueOutOfRangeForProperty(CSSPropertyID property_id,
                                  double value,
                                  CSSPrimitiveValue::UnitType unit) {
  // FIXME: Avoid this CSSProperty::Get call as it can be costly.
  // The caller often has a CSSProperty already, so we can just pass it here.
  if (LengthPropertyFunctions::GetValueRange(CSSProperty::Get(property_id)) ==
          Length::ValueRange::kNonNegative &&
      value < 0) {
    return true;
  }

  // For non-length properties and special cases.
  switch (property_id) {
    case CSSPropertyID::kOrder:
    case CSSPropertyID::kZIndex:
    case CSSPropertyID::kMathDepth:
      return round(value) != value;
    case CSSPropertyID::kTabSize:
      return value < 0 || (unit == CSSPrimitiveValue::UnitType::kNumber &&
                           round(value) != value);
    case CSSPropertyID::kOrphans:
    case CSSPropertyID::kWidows:
    case CSSPropertyID::kColumnCount:
      return round(value) != value || value < 1;
    case CSSPropertyID::kBlockSize:
    case CSSPropertyID::kColumnRuleWidth:
    case CSSPropertyID::kFlexGrow:
    case CSSPropertyID::kFlexShrink:
    case CSSPropertyID::kFontSize:
    case CSSPropertyID::kFontSizeAdjust:
    case CSSPropertyID::kFontStretch:
    case CSSPropertyID::kInlineSize:
    case CSSPropertyID::kMaxBlockSize:
    case CSSPropertyID::kMaxInlineSize:
    case CSSPropertyID::kMinBlockSize:
    case CSSPropertyID::kMinInlineSize:
    case CSSPropertyID::kR:
    case CSSPropertyID::kRx:
    case CSSPropertyID::kRy:
      return value < 0;
    case CSSPropertyID::kFontWeight:
      return value < 0 || value > 1000;
    default:
      return false;
  }
}

}  // namespace

CSSUnitValue* CSSUnitValue::Create(double value,
                                   const String& unit_name,
                                   ExceptionState& exception_state) {
  CSSPrimitiveValue::UnitType unit = UnitFromName(unit_name);
  if (!IsValidUnit(unit)) {
    exception_state.ThrowTypeError("Invalid unit: " + unit_name);
    return nullptr;
  }
  return MakeGarbageCollected<CSSUnitValue>(value, unit);
}

CSSUnitValue* CSSUnitValue::Create(double value,
                                   CSSPrimitiveValue::UnitType unit) {
  DCHECK(IsValidUnit(unit));
  return MakeGarbageCollected<CSSUnitValue>(value, unit);
}

CSSUnitValue* CSSUnitValue::FromCSSValue(const CSSNumericLiteralValue& value) {
  CSSPrimitiveValue::UnitType unit = value.GetType();
  if (unit == CSSPrimitiveValue::UnitType::kInteger) {
    unit = CSSPrimitiveValue::UnitType::kNumber;
  }

  if (!IsValidUnit(unit)) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSUnitValue>(value.GetDoubleValue(), unit);
}

String CSSUnitValue::unit() const {
  if (unit_ == CSSPrimitiveValue::UnitType::kNumber) {
    return "number";
  }
  if (unit_ == CSSPrimitiveValue::UnitType::kPercentage) {
    return "percent";
  }
  return CSSPrimitiveValue::UnitTypeToString(unit_);
}

CSSStyleValue::StyleValueType CSSUnitValue::GetType() const {
  return StyleValueType::kUnitType;
}

CSSUnitValue* CSSUnitValue::ConvertTo(
    CSSPrimitiveValue::UnitType target_unit) const {
  if (unit_ == target_unit) {
    return Create(value_, unit_);
  }

  // Instead of defining the scale factors for every unit to every other unit,
  // we simply convert to the canonical unit and back since we already have
  // the scale factors for canonical units.
  const auto canonical_unit = ToCanonicalUnit(unit_);
  if (canonical_unit != ToCanonicalUnit(target_unit) ||
      canonical_unit == CSSPrimitiveValue::UnitType::kUnknown) {
    return nullptr;
  }

  const double scale_factor =
      CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(unit_) /
      CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(target_unit);

  return CSSUnitValue::Create(value_ * scale_factor, target_unit);
}

std::optional<CSSNumericSumValue> CSSUnitValue::SumValue() const {
  CSSNumericSumValue sum;
  CSSNumericSumValue::UnitMap unit_map;
  if (unit_ != CSSPrimitiveValue::UnitType::kNumber) {
    unit_map.insert(ToCanonicalUnitIfPossible(unit_), 1);
  }

  sum.terms.emplace_back(
      value_ * CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(unit_),
      std::move(unit_map));
  return sum;
}

bool CSSUnitValue::Equals(const CSSNumericValue& other) const {
  auto* other_unit_value = DynamicTo<CSSUnitValue>(other);
  if (!other_unit_value) {
    return false;
  }

  return value_ == other_unit_value->value_ && unit_ == other_unit_value->unit_;
}

const CSSNumericLiteralValue* CSSUnitValue::ToCSSValue() const {
  return CSSNumericLiteralValue::Create(value_, unit_);
}

const CSSPrimitiveValue* CSSUnitValue::ToCSSValueWithProperty(
    CSSPropertyID property_id) const {
  if (IsValueOutOfRangeForProperty(property_id, value_, unit_)) {
    // Wrap out of range values with a calc.
    CSSMathExpressionNode* node = ToCalcExpressionNode();
    node->SetIsNestedCalc();
    return CSSMathFunctionValue::Create(node);
  }

  return CSSNumericLiteralValue::Create(value_, unit_);
}

CSSMathExpressionNode* CSSUnitValue::ToCalcExpressionNode() const {
  return CSSMathExpressionNumericLiteral::Create(
      CSSNumericLiteralValue::Create(value_, unit_));
}

CSSNumericValue* CSSUnitValue::Negate() {
  return CSSUnitValue::Create(-value_, unit_);
}

CSSNumericValue* CSSUnitValue::Invert() {
  if (unit_ == CSSPrimitiveValue::UnitType::kNumber) {
    if (value_ == 0) {
      return nullptr;
    }
    return CSSUnitValue::Create(1.0 / value_, unit_);
  }
  return CSSMathInvert::Create(this);
}

void CSSUnitValue::BuildCSSText(Nested,
                                ParenLess,
                                StringBuilder& result) const {
  result.Append(ToCSSValue()->CssText());
}

}  // namespace blink
