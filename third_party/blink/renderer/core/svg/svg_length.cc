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
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

#define CAST_UNIT(unit) \
  (static_cast<uint8_t>(CSSPrimitiveValue::UnitType::unit))

// Table of initial values for SVGLength properties. Indexed by the
// SVGLength::Initial enumeration, hence these two need to be kept
// synchronized.
struct InitialLengthData {
  int8_t value;
  uint8_t unit;
};
constexpr auto g_initial_lengths_table = std::to_array<InitialLengthData>({
    {0, CAST_UNIT(kUserUnits)},
    {-10, CAST_UNIT(kPercentage)},
    {0, CAST_UNIT(kPercentage)},
    {50, CAST_UNIT(kPercentage)},
    {100, CAST_UNIT(kPercentage)},
    {120, CAST_UNIT(kPercentage)},
    {3, CAST_UNIT(kUserUnits)},
});
static_assert(static_cast<size_t>(SVGLength::Initial::kNumValues) ==
                  std::size(g_initial_lengths_table),
              "the enumeration is synchronized with the value table");
static_assert(static_cast<size_t>(SVGLength::Initial::kNumValues) <=
                  1u << SVGLength::kInitialValueBits,
              "the enumeration is synchronized with the value table");

#undef CAST_UNIT

const CSSPrimitiveValue& CreateInitialCSSValue(
    SVGLength::Initial initial_value) {
  size_t initial_value_index = static_cast<size_t>(initial_value);
  DCHECK_LT(initial_value_index, std::size(g_initial_lengths_table));
  const auto& entry = g_initial_lengths_table[initial_value_index];
  return *CSSNumericLiteralValue::Create(
      entry.value, static_cast<CSSPrimitiveValue::UnitType>(entry.unit));
}

bool IsSupportedCSSUnitType(CSSPrimitiveValue::UnitType type) {
  return (CSSPrimitiveValue::IsLength(type) ||
          type == CSSPrimitiveValue::UnitType::kNumber ||
          type == CSSPrimitiveValue::UnitType::kPercentage) &&
         type != CSSPrimitiveValue::UnitType::kQuirkyEms;
}

bool IsSupportedCalculationCategory(CalculationResultCategory category) {
  switch (category) {
    case kCalcLength:
    case kCalcNumber:
    case kCalcPercent:
    case kCalcLengthFunction:
      return true;
    default:
      return false;
  }
}

bool AllowedCSSValueForSVGLength(const CSSValue& value) {
  if (auto* numeric_value = DynamicTo<CSSNumericLiteralValue>(value)) {
    return IsSupportedCSSUnitType(numeric_value->GetType());
  }
  if (auto* math_value = DynamicTo<CSSMathFunctionValue>(value)) {
    return IsSupportedCalculationCategory(math_value->Category());
  }
  return value.IsUnparsedDeclaration();
}

}  // namespace

SVGLength::SVGLength(SVGLengthMode mode)
    : SVGLength(*CSSNumericLiteralValue::Create(
                    0,
                    CSSPrimitiveValue::UnitType::kUserUnits),
                mode) {}

SVGLength::SVGLength(Initial initial, SVGLengthMode mode)
    : SVGLength(CreateInitialCSSValue(initial), mode) {}

SVGLength::SVGLength(const CSSValue& value, SVGLengthMode mode)
    : value_(value), unit_mode_(static_cast<unsigned>(mode)) {
  DCHECK_EQ(UnitMode(), mode);
  DCHECK(AllowedCSSValueForSVGLength(value));
}

void SVGLength::Trace(Visitor* visitor) const {
  visitor->Trace(value_);
  SVGListablePropertyBase::Trace(visitor);
}

SVGLength* SVGLength::Clone() const {
  return MakeGarbageCollected<SVGLength>(*value_, UnitMode());
}

bool SVGLength::operator==(const SVGLength& other) const {
  return unit_mode_ == other.unit_mode_ && value_ == other.value_;
}

Length SVGLength::ConvertToLength(
    const SVGLengthConversionData& conversion_data) const {
  const CSSValue* resolved_value =
      conversion_data.MaybeResolveUnparsedValue(*value_);
  if (!resolved_value) {
    return Length::Fixed(0);
  }
  return To<CSSPrimitiveValue>(*resolved_value)
      .ConvertToLength(conversion_data);
}

float SVGLength::Value(const SVGLengthConversionData& conversion_data,
                       float dimension) const {
  const CSSValue* resolved_value =
      conversion_data.MaybeResolveUnparsedValue(*value_);
  if (!resolved_value) {
    return 0;
  }
  return FloatValueForLength(
      To<CSSPrimitiveValue>(*resolved_value).ConvertToLength(conversion_data),
      dimension);
}

float SVGLength::Value(const SVGLengthContext& context) const {
  const CSSValue* resolved_value = context.MaybeResolveUnparsedValue(*value_);
  if (!resolved_value) {
    return 0;
  }
  if (const auto* math_function =
          DynamicTo<CSSMathFunctionValue>(*resolved_value)) {
    return context.ResolveValue(*math_function, UnitMode());
  }
  const auto& numeric_literal = To<CSSNumericLiteralValue>(*resolved_value);
  return context.ConvertValueToUserUnits(numeric_literal.DoubleValue(),
                                         UnitMode(), numeric_literal.GetType());
}

void SVGLength::SetValueAsNumber(float value) {
  value_ = CSSNumericLiteralValue::Create(
      value, CSSPrimitiveValue::UnitType::kUserUnits);
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
  if (!IsNumericValue()) {
    return false;
  }

  return CSSPrimitiveValue::IsRelativeUnit(NumericLiteralType());
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
  if (!new_value) {
    if (RuntimeEnabledFeatures::SvgLengthResolveUnparsedValueEnabled()) {
      CSSParserTokenStream stream(string);
      stream.EnsureLookAhead();
      bool important = false;
      CSSVariableData* variable_data =
          CSSVariableParser::ConsumeUnparsedDeclaration(
              stream,
              /*allow_important_annotation=*/true,
              /*is_animation_tainted=*/false,
              /*must_contain_variable_reference=*/true,
              /*restricted_value=*/true, /*comma_ends_declaration=*/false,
              important, *GetSVGAttributeParserContext());
      if (!variable_data || important) {
        return SVGParseStatus::kExpectedLength;
      }

      auto* unparsed_value = MakeGarbageCollected<CSSUnparsedDeclarationValue>(
          variable_data, GetSVGAttributeParserContext());

      value_ = unparsed_value;
      return SVGParseStatus::kNoError;
    } else {
      return SVGParseStatus::kExpectedLength;
    }
  }

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
  return value_->CssText();
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

void SVGLength::Add(const SVGPropertyBase* other,
                    const SVGElement* context_element) {
  SVGLengthContext length_context(context_element);
  const float sum =
      Value(length_context) + To<SVGLength>(other)->Value(length_context);
  if (IsCalculated()) {
    SetValueAsNumber(sum);
    return;
  }
  SetValueInSpecifiedUnits(length_context.ConvertValueFromUserUnits(
      sum, UnitMode(), NumericLiteralType()));
}

void SVGLength::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from_value,
    const SVGPropertyBase* to_value,
    const SVGPropertyBase* to_at_end_of_duration_value,
    const SVGElement* context_element) {
  auto* from_length = To<SVGLength>(from_value);
  auto* to_length = To<SVGLength>(to_value);
  auto* to_at_end_of_duration_length =
      To<SVGLength>(to_at_end_of_duration_value);

  SVGLengthContext length_context(context_element);
  float result = ComputeAnimatedNumber(
      parameters, percentage, repeat_count, from_length->Value(length_context),
      to_length->Value(length_context),
      to_at_end_of_duration_length->Value(length_context));

  // TODO(shanmuga.m): Construct a calc() expression if the units fall in
  // different categories.
  const SVGLength* unit_determining_length =
      (percentage < 0.5) ? from_length : to_length;
  CSSPrimitiveValue::UnitType result_unit =
      unit_determining_length->IsNumericValue()
          ? unit_determining_length->NumericLiteralType()
          : CSSPrimitiveValue::UnitType::kUserUnits;

  if (parameters.is_additive)
    result += Value(length_context);

  value_ = CSSNumericLiteralValue::Create(
      length_context.ConvertValueFromUserUnits(result, UnitMode(), result_unit),
      result_unit);
}

float SVGLength::CalculateDistance(const SVGPropertyBase* to_value,
                                   const SVGElement* context_element) const {
  SVGLengthContext length_context(context_element);
  auto* to_length = To<SVGLength>(to_value);

  return fabsf(to_length->Value(length_context) - Value(length_context));
}

void SVGLength::SetInitial(unsigned initial_value) {
  value_ = CreateInitialCSSValue(static_cast<Initial>(initial_value));
}

bool SVGLength::IsNegativeNumericLiteral() const {
  if (!value_->IsPrimitiveValue()) {
    return false;
  }
  std::optional<double> value =
      To<CSSPrimitiveValue>(*value_).GetValueIfKnown();
  return value && *value < 0.0;
}

}  // namespace blink
