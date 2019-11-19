/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_length.h"

#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/svg/svg_animate_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

#define CAST_UNIT(unit) \
  (static_cast<uint8_t>(CSSPrimitiveValue::UnitType::unit))

// Table of initial values for SVGLength properties. Indexed by the
// SVGLength::Initial enumeration, hence these two need to be kept
// synchronized.
const struct {
  int8_t value;
  uint8_t unit;
} g_initial_lengths_table[] = {
    {0, CAST_UNIT(kUserUnits)},    {-10, CAST_UNIT(kPercentage)},
    {0, CAST_UNIT(kPercentage)},   {50, CAST_UNIT(kPercentage)},
    {100, CAST_UNIT(kPercentage)}, {120, CAST_UNIT(kPercentage)},
    {3, CAST_UNIT(kUserUnits)},
};
static_assert(static_cast<size_t>(SVGLength::Initial::kNumValues) ==
                  base::size(g_initial_lengths_table),
              "the enumeration is synchronized with the value table");
static_assert(static_cast<size_t>(SVGLength::Initial::kNumValues) <=
                  1u << SVGLength::kInitialValueBits,
              "the enumeration is synchronized with the value table");

#undef CAST_UNIT

const CSSPrimitiveValue& CreateInitialCSSValue(
    SVGLength::Initial initial_value) {
  size_t initial_value_index = static_cast<size_t>(initial_value);
  DCHECK_LT(initial_value_index, base::size(g_initial_lengths_table));
  const auto& entry = g_initial_lengths_table[initial_value_index];
  return *CSSNumericLiteralValue::Create(
      entry.value, static_cast<CSSPrimitiveValue::UnitType>(entry.unit));
}

}  // namespace

SVGLength::SVGLength(SVGLengthMode mode)
    : SVGLength(*CSSNumericLiteralValue::Create(
                    0,
                    CSSPrimitiveValue::UnitType::kUserUnits),
                mode) {}

SVGLength::SVGLength(Initial initial, SVGLengthMode mode)
    : SVGLength(CreateInitialCSSValue(initial), mode) {}

SVGLength::SVGLength(const CSSPrimitiveValue& value, SVGLengthMode mode)
    : value_(value), unit_mode_(static_cast<unsigned>(mode)) {
  DCHECK_EQ(UnitMode(), mode);
}

SVGLength::SVGLength(const SVGLength& o)
    : value_(o.value_), unit_mode_(o.unit_mode_) {}

void SVGLength::Trace(blink::Visitor* visitor) {
  visitor->Trace(value_);
  SVGPropertyBase::Trace(visitor);
}

SVGLength* SVGLength::Clone() const {
  return MakeGarbageCollected<SVGLength>(*this);
}

SVGPropertyBase* SVGLength::CloneForAnimation(const String& value) const {
  auto* length = MakeGarbageCollected<SVGLength>();
  length->unit_mode_ = unit_mode_;

  if (length->SetValueAsString(value) != SVGParseStatus::kNoError) {
    length->value_ = CSSNumericLiteralValue::Create(
        0, CSSPrimitiveValue::UnitType::kUserUnits);
  }

  return length;
}

bool SVGLength::operator==(const SVGLength& other) const {
  return unit_mode_ == other.unit_mode_ && value_ == other.value_;
}

float SVGLength::Value(const SVGLengthContext& context) const {
  if (IsCalculated())
    return context.ResolveValue(AsCSSPrimitiveValue(), UnitMode());

  return context.ConvertValueToUserUnits(value_->GetFloatValue(), UnitMode(),
                                         NumericLiteralType());
}

void SVGLength::SetValueAsNumber(float value) {
  value_ = CSSNumericLiteralValue::Create(
      value, CSSPrimitiveValue::UnitType::kUserUnits);
}

void SVGLength::SetValue(float value, const SVGLengthContext& context) {
  // |value| is in user units.
  if (IsCalculated()) {
    value_ = CSSNumericLiteralValue::Create(
        value, CSSPrimitiveValue::UnitType::kUserUnits);
    return;
  }
  value_ = CSSNumericLiteralValue::Create(
      context.ConvertValueFromUserUnits(value, UnitMode(),
                                        NumericLiteralType()),
      NumericLiteralType());
}

void SVGLength::SetValueInSpecifiedUnits(float value) {
  DCHECK(!IsCalculated());
  value_ = CSSNumericLiteralValue::Create(value, NumericLiteralType());
}

bool SVGLength::IsRelative() const {
  if (IsPercentage())
    return true;
  // TODO(crbug.com/979895): This is the result of a refactoring, which might
  // have revealed an existing bug with relative units in math functions.
  return !IsCalculated() &&
         CSSPrimitiveValue::IsRelativeUnit(NumericLiteralType());
}

static bool IsSupportedCSSUnitType(CSSPrimitiveValue::UnitType type) {
  return (CSSPrimitiveValue::IsLength(type) ||
          type == CSSPrimitiveValue::UnitType::kNumber ||
          type == CSSPrimitiveValue::UnitType::kPercentage) &&
         type != CSSPrimitiveValue::UnitType::kQuirkyEms;
}

static bool IsSupportedCalculationCategory(CalculationCategory category) {
  switch (category) {
    case kCalcLength:
    case kCalcNumber:
    case kCalcPercent:
    case kCalcPercentNumber:
    case kCalcPercentLength:
    case kCalcLengthNumber:
    case kCalcPercentLengthNumber:
      return true;
    default:
      return false;
  }
}

void SVGLength::SetUnitType(CSSPrimitiveValue::UnitType type) {
  DCHECK(IsSupportedCSSUnitType(type));
  value_ = CSSNumericLiteralValue::Create(value_->GetFloatValue(), type);
}

float SVGLength::ValueAsPercentage() const {
  // LengthTypePercentage is represented with 100% = 100.0. Good for accuracy
  // but could eventually be changed.
  if (value_->IsPercentage()) {
    // Note: This division is a source of floating point inaccuracy.
    return value_->GetFloatValue() / 100;
  }

  return value_->GetFloatValue();
}

float SVGLength::ValueAsPercentage100() const {
  // LengthTypePercentage is represented with 100% = 100.0. Good for accuracy
  // but could eventually be changed.
  if (value_->IsPercentage())
    return value_->GetFloatValue();

  return value_->GetFloatValue() * 100;
}

float SVGLength::ScaleByPercentage(float input) const {
  float result = input * value_->GetFloatValue();
  if (value_->IsPercentage()) {
    // Delaying division by 100 as long as possible since it introduces floating
    // point errors.
    result = result / 100;
  }
  return result;
}

namespace {

const CSSParserContext* GetSVGAttributeParserContext() {
  // NOTE(ikilpatrick): We will always parse SVG lengths in the insecure
  // context mode. If a function/unit/etc will require a secure context check
  // in the future, plumbing will need to be added.
  DEFINE_STATIC_LOCAL(
      const Persistent<CSSParserContext>, svg_parser_context,
      (MakeGarbageCollected<CSSParserContext>(
          kSVGAttributeMode, SecureContextMode::kInsecureContext)));
  return svg_parser_context;
}

}  // namespace

SVGParsingError SVGLength::SetValueAsString(const String& string) {
  // TODO(fs): Preferably we wouldn't need to special-case the null
  // string (which we'll get for example for removeAttribute.)
  // Hopefully work on crbug.com/225807 can help here.
  if (string.IsNull()) {
    value_ = CSSNumericLiteralValue::Create(
        0, CSSPrimitiveValue::UnitType::kUserUnits);
    return SVGParseStatus::kNoError;
  }

  const CSSValue* parsed = CSSParser::ParseSingleValue(
      CSSPropertyID::kX, string, GetSVGAttributeParserContext());
  const auto* new_value = DynamicTo<CSSPrimitiveValue>(parsed);
  if (!new_value)
    return SVGParseStatus::kExpectedLength;

  if (const auto* math_value = DynamicTo<CSSMathFunctionValue>(new_value)) {
    if (!IsSupportedCalculationCategory(math_value->Category()))
      return SVGParseStatus::kExpectedLength;
  } else {
    const auto* numeric_literal_value = To<CSSNumericLiteralValue>(new_value);
    if (!IsSupportedCSSUnitType(numeric_literal_value->GetType()))
      return SVGParseStatus::kExpectedLength;
  }

  value_ = new_value;
  return SVGParseStatus::kNoError;
}

String SVGLength::ValueAsString() const {
  return value_->CustomCSSText();
}

void SVGLength::NewValueSpecifiedUnits(CSSPrimitiveValue::UnitType type,
                                       float value) {
  value_ = CSSNumericLiteralValue::Create(value, type);
}

void SVGLength::ConvertToSpecifiedUnits(CSSPrimitiveValue::UnitType type,
                                        const SVGLengthContext& context) {
  DCHECK(IsSupportedCSSUnitType(type));

  float value_in_user_units = Value(context);
  value_ = CSSNumericLiteralValue::Create(
      context.ConvertValueFromUserUnits(value_in_user_units, UnitMode(), type),
      type);
}

SVGLengthMode SVGLength::LengthModeForAnimatedLengthAttribute(
    const QualifiedName& attr_name) {
  typedef HashMap<QualifiedName, SVGLengthMode> LengthModeForLengthAttributeMap;
  DEFINE_STATIC_LOCAL(LengthModeForLengthAttributeMap, length_mode_map, ());

  if (length_mode_map.IsEmpty()) {
    length_mode_map.Set(svg_names::kXAttr, SVGLengthMode::kWidth);
    length_mode_map.Set(svg_names::kYAttr, SVGLengthMode::kHeight);
    length_mode_map.Set(svg_names::kCxAttr, SVGLengthMode::kWidth);
    length_mode_map.Set(svg_names::kCyAttr, SVGLengthMode::kHeight);
    length_mode_map.Set(svg_names::kDxAttr, SVGLengthMode::kWidth);
    length_mode_map.Set(svg_names::kDyAttr, SVGLengthMode::kHeight);
    length_mode_map.Set(svg_names::kFrAttr, SVGLengthMode::kOther);
    length_mode_map.Set(svg_names::kFxAttr, SVGLengthMode::kWidth);
    length_mode_map.Set(svg_names::kFyAttr, SVGLengthMode::kHeight);
    length_mode_map.Set(svg_names::kRAttr, SVGLengthMode::kOther);
    length_mode_map.Set(svg_names::kRxAttr, SVGLengthMode::kWidth);
    length_mode_map.Set(svg_names::kRyAttr, SVGLengthMode::kHeight);
    length_mode_map.Set(svg_names::kWidthAttr, SVGLengthMode::kWidth);
    length_mode_map.Set(svg_names::kHeightAttr, SVGLengthMode::kHeight);
    length_mode_map.Set(svg_names::kX1Attr, SVGLengthMode::kWidth);
    length_mode_map.Set(svg_names::kX2Attr, SVGLengthMode::kWidth);
    length_mode_map.Set(svg_names::kY1Attr, SVGLengthMode::kHeight);
    length_mode_map.Set(svg_names::kY2Attr, SVGLengthMode::kHeight);
    length_mode_map.Set(svg_names::kRefXAttr, SVGLengthMode::kWidth);
    length_mode_map.Set(svg_names::kRefYAttr, SVGLengthMode::kHeight);
    length_mode_map.Set(svg_names::kMarkerWidthAttr, SVGLengthMode::kWidth);
    length_mode_map.Set(svg_names::kMarkerHeightAttr, SVGLengthMode::kHeight);
    length_mode_map.Set(svg_names::kTextLengthAttr, SVGLengthMode::kWidth);
    length_mode_map.Set(svg_names::kStartOffsetAttr, SVGLengthMode::kWidth);
  }

  if (length_mode_map.Contains(attr_name))
    return length_mode_map.at(attr_name);

  return SVGLengthMode::kOther;
}

bool SVGLength::NegativeValuesForbiddenForAnimatedLengthAttribute(
    const QualifiedName& attr_name) {
  DEFINE_STATIC_LOCAL(
      HashSet<QualifiedName>, no_negative_values_set,
      ({
          svg_names::kFrAttr, svg_names::kRAttr, svg_names::kRxAttr,
          svg_names::kRyAttr, svg_names::kWidthAttr, svg_names::kHeightAttr,
          svg_names::kMarkerWidthAttr, svg_names::kMarkerHeightAttr,
          svg_names::kTextLengthAttr,
      }));
  return no_negative_values_set.Contains(attr_name);
}

void SVGLength::Add(SVGPropertyBase* other, SVGElement* context_element) {
  SVGLengthContext length_context(context_element);
  SetValue(Value(length_context) + ToSVGLength(other)->Value(length_context),
           length_context);
}

void SVGLength::CalculateAnimatedValue(
    const SVGAnimateElement& animation_element,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from_value,
    SVGPropertyBase* to_value,
    SVGPropertyBase* to_at_end_of_duration_value,
    SVGElement* context_element) {
  SVGLength* from_length = ToSVGLength(from_value);
  SVGLength* to_length = ToSVGLength(to_value);
  SVGLength* to_at_end_of_duration_length =
      ToSVGLength(to_at_end_of_duration_value);

  SVGLengthContext length_context(context_element);
  float animated_number = Value(length_context);
  animation_element.AnimateAdditiveNumber(
      percentage, repeat_count, from_length->Value(length_context),
      to_length->Value(length_context),
      to_at_end_of_duration_length->Value(length_context), animated_number);

  DCHECK_EQ(UnitMode(), LengthModeForAnimatedLengthAttribute(
                            animation_element.AttributeName()));

  // TODO(shanmuga.m): Construct a calc() expression if the units fall in
  // different categories.
  CSSPrimitiveValue::UnitType new_unit =
      CSSPrimitiveValue::UnitType::kUserUnits;
  if (percentage < 0.5) {
    if (!from_length->IsCalculated()) {
      new_unit = from_length->NumericLiteralType();
    }
  } else {
    if (!to_length->IsCalculated()) {
      new_unit = to_length->NumericLiteralType();
    }
  }
  animated_number = length_context.ConvertValueFromUserUnits(
      animated_number, UnitMode(), new_unit);
  value_ = CSSNumericLiteralValue::Create(animated_number, new_unit);
}

float SVGLength::CalculateDistance(SVGPropertyBase* to_value,
                                   SVGElement* context_element) {
  SVGLengthContext length_context(context_element);
  SVGLength* to_length = ToSVGLength(to_value);

  return fabsf(to_length->Value(length_context) - Value(length_context));
}

void SVGLength::SetInitial(unsigned initial_value) {
  value_ = CreateInitialCSSValue(static_cast<Initial>(initial_value));
}

bool SVGLength::IsNegativeNumericLiteral() const {
  if (!value_->IsNumericLiteralValue())
    return false;
  return value_->GetDoubleValue() < 0;
}

}  // namespace blink
