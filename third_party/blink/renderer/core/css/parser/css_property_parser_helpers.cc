// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"

#include "third_party/blink/renderer/core/css/css_axis_value.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_crossfade_value.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_light_dark_color_pair.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace css_property_parser_helpers {

namespace {

// Add CSSVariableData to variableData vector.
bool AddCSSPaintArgument(
    const Vector<CSSParserToken>& tokens,
    Vector<scoped_refptr<CSSVariableData>>* const variable_data,
    const CSSParserContext* context) {
  CSSParserTokenRange token_range(tokens);
  if (!token_range.AtEnd()) {
    scoped_refptr<CSSVariableData> unparsed_css_variable_data =
        CSSVariableData::Create(token_range, false, false, context->BaseURL(),
                                context->Charset());
    if (unparsed_css_variable_data.get()) {
      variable_data->push_back(std::move(unparsed_css_variable_data));
      return true;
    }
  }
  return false;
}

// Consume input arguments, if encounter function, will return the function
// block as a Vector of CSSParserToken, otherwise, will just return a Vector of
// a single CSSParserToken.
Vector<CSSParserToken> ConsumeFunctionArgsOrNot(CSSParserTokenRange& args) {
  Vector<CSSParserToken> argument_tokens;
  if (args.Peek().GetBlockType() == CSSParserToken::kBlockStart) {
    // Function block.
    // Push the function name and initial right parenthesis.
    // Since we don't have any upfront knowledge about the input argument types
    // here, we should just leave the token as it is and resolve it later in
    // the variable parsing phase.
    argument_tokens.push_back(args.Peek());
    CSSParserTokenRange contents = args.ConsumeBlock();
    while (!contents.AtEnd()) {
      argument_tokens.push_back(contents.Consume());
    }
    argument_tokens.push_back(
        CSSParserToken(kRightParenthesisToken, CSSParserToken::kBlockEnd));

  } else {
    argument_tokens.push_back(args.ConsumeIncludingWhitespace());
  }
  return argument_tokens;
}

CSSFunctionValue* ConsumeFilterFunction(CSSParserTokenRange& range,
                                        const CSSParserContext& context) {
  CSSValueID filter_type = range.Peek().FunctionId();
  if (filter_type < CSSValueID::kInvert ||
      filter_type > CSSValueID::kDropShadow)
    return nullptr;
  CSSParserTokenRange args =
      css_property_parser_helpers::ConsumeFunction(range);
  CSSFunctionValue* filter_value =
      MakeGarbageCollected<CSSFunctionValue>(filter_type);
  CSSValue* parsed_value = nullptr;

  if (filter_type == CSSValueID::kDropShadow) {
    parsed_value = css_parsing_utils::ParseSingleShadow(
        args, context.Mode(), css_parsing_utils::AllowInsetAndSpread::kForbid);
  } else {
    if (args.AtEnd()) {
      context.Count(WebFeature::kCSSFilterFunctionNoArguments);
      return filter_value;
    }
    if (filter_type == CSSValueID::kBrightness) {
      // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
      parsed_value =
          css_property_parser_helpers::ConsumePercent(args, kValueRangeAll);
      if (!parsed_value) {
        parsed_value = css_property_parser_helpers::ConsumeNumber(
            args, kValueRangeNonNegative);
      }
    } else if (filter_type == CSSValueID::kHueRotate) {
      parsed_value = css_property_parser_helpers::ConsumeAngle(
          args, &context, WebFeature::kUnitlessZeroAngleFilter);
    } else if (filter_type == CSSValueID::kBlur) {
      parsed_value = css_property_parser_helpers::ConsumeLength(
          args, kHTMLStandardMode, kValueRangeNonNegative);
    } else {
      // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
      parsed_value = css_property_parser_helpers::ConsumePercent(
          args, kValueRangeNonNegative);
      if (!parsed_value) {
        parsed_value = css_property_parser_helpers::ConsumeNumber(
            args, kValueRangeNonNegative);
      }
      if (parsed_value && filter_type != CSSValueID::kSaturate &&
          filter_type != CSSValueID::kContrast) {
        bool is_percentage =
            To<CSSPrimitiveValue>(parsed_value)->IsPercentage();
        double max_allowed = is_percentage ? 100.0 : 1.0;
        if (To<CSSPrimitiveValue>(parsed_value)->GetDoubleValue() >
            max_allowed) {
          parsed_value = CSSNumericLiteralValue::Create(
              max_allowed, is_percentage
                               ? CSSPrimitiveValue::UnitType::kPercentage
                               : CSSPrimitiveValue::UnitType::kNumber);
        }
      }
    }
  }
  if (!parsed_value || !args.AtEnd())
    return nullptr;
  filter_value->Append(*parsed_value);
  return filter_value;
}

}  // namespace

void Complete4Sides(CSSValue* side[4]) {
  if (side[3])
    return;
  if (!side[2]) {
    if (!side[1])
      side[1] = side[0];
    side[2] = side[0];
  }
  side[3] = side[1];
}

bool ConsumeCommaIncludingWhitespace(CSSParserTokenRange& range) {
  CSSParserToken value = range.Peek();
  if (value.GetType() != kCommaToken)
    return false;
  range.ConsumeIncludingWhitespace();
  return true;
}

bool ConsumeSlashIncludingWhitespace(CSSParserTokenRange& range) {
  CSSParserToken value = range.Peek();
  if (value.GetType() != kDelimiterToken || value.Delimiter() != '/')
    return false;
  range.ConsumeIncludingWhitespace();
  return true;
}

CSSParserTokenRange ConsumeFunction(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kFunctionToken);
  CSSParserTokenRange contents = range.ConsumeBlock();
  range.ConsumeWhitespace();
  contents.ConsumeWhitespace();
  return contents;
}

// TODO(rwlbuis): consider pulling in the parsing logic from
// css_math_expression_node.cc.
class MathFunctionParser {
  STACK_ALLOCATED();

 public:
  explicit MathFunctionParser(CSSParserTokenRange& range,
                              ValueRange value_range = kValueRangeAll)
      : source_range_(range), range_(range) {
    const CSSParserToken& token = range.Peek();
    if (token.FunctionId() == CSSValueID::kCalc ||
        token.FunctionId() == CSSValueID::kWebkitCalc) {
      calc_value_ = CSSMathFunctionValue::Create(
          CSSMathExpressionNode::ParseCalc(ConsumeFunction(range_)),
          value_range);
      return;
    }

    if (RuntimeEnabledFeatures::CSSComparisonFunctionsEnabled()) {
      switch (token.FunctionId()) {
        case CSSValueID::kMin:
          calc_value_ = CSSMathFunctionValue::Create(
              CSSMathExpressionNode::ParseMin(ConsumeFunction(range_)),
              value_range);
          return;
        case CSSValueID::kMax:
          calc_value_ = CSSMathFunctionValue::Create(
              CSSMathExpressionNode::ParseMax(ConsumeFunction(range_)),
              value_range);
          return;
        case CSSValueID::kClamp:
          calc_value_ = CSSMathFunctionValue::Create(
              CSSMathExpressionNode::ParseClamp(ConsumeFunction(range_)),
              value_range);
          return;
        default:
          break;
      }
    }
  }

  const CSSMathFunctionValue* Value() const { return calc_value_; }
  CSSMathFunctionValue* ConsumeValue() {
    if (!calc_value_)
      return nullptr;
    source_range_ = range_;
    return calc_value_.Release();
  }

  CSSPrimitiveValue* ConsumeRoundedInt() {
    if (!calc_value_)
      return nullptr;
    source_range_ = range_;
    CSSPrimitiveValue::UnitType unit_type =
        CSSPrimitiveValue::UnitType::kInteger;
    double rounded_value = floor(calc_value_->GetDoubleValue() + 0.5);
    return CSSNumericLiteralValue::Create(rounded_value, unit_type);
  }

  CSSPrimitiveValue* ConsumeNumber() {
    if (!calc_value_)
      return nullptr;
    source_range_ = range_;
    CSSPrimitiveValue::UnitType unit_type =
        calc_value_->IsInt() ? CSSPrimitiveValue::UnitType::kInteger
                             : CSSPrimitiveValue::UnitType::kNumber;
    return CSSNumericLiteralValue::Create(calc_value_->GetDoubleValue(),
                                          unit_type);
  }

  bool ConsumeNumberRaw(double& result) {
    if (!calc_value_ || calc_value_->Category() != kCalcNumber)
      return false;
    source_range_ = range_;
    result = calc_value_->GetDoubleValue();
    return true;
  }

 private:
  CSSParserTokenRange& source_range_;
  CSSParserTokenRange range_;
  Member<CSSMathFunctionValue> calc_value_;
};

CSSPrimitiveValue* ConsumeInteger(CSSParserTokenRange& range,
                                  double minimum_value) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kNumberToken) {
    if (token.GetNumericValueType() == kNumberValueType ||
        token.NumericValue() < minimum_value)
      return nullptr;
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kInteger);
  }
  MathFunctionParser math_parser(range);
  if (const CSSMathFunctionValue* math_value = math_parser.Value()) {
    if (!RuntimeEnabledFeatures::CSSCalcAsIntEnabled() && !math_value->IsInt())
      return nullptr;
    if (math_value->Category() != kCalcNumber)
      return nullptr;
    double double_value = math_value->GetDoubleValue();
    if (double_value < minimum_value)
      return nullptr;
    if (!RuntimeEnabledFeatures::CSSCalcAsIntEnabled())
      return math_parser.ConsumeNumber();
    if (math_value->IsInt())
      return math_parser.ConsumeNumber();
    return math_parser.ConsumeRoundedInt();
  }
  return nullptr;
}

// This implements the behavior defined in [1], where calc() expressions
// are valid when <integer> is expected, even if the calc()-expression does
// not result in an integral value.
//
// TODO(andruud): Eventually this behavior should just be part of
// ConsumeInteger, and this function can be removed. For now, having a separate
// function with this behavior allows us to implement [1] gradually.
//
// [1] https://drafts.csswg.org/css-values-4/#calc-type-checking
CSSPrimitiveValue* ConsumeIntegerOrNumberCalc(CSSParserTokenRange& range) {
  CSSParserTokenRange int_range(range);
  if (CSSPrimitiveValue* value = ConsumeInteger(int_range)) {
    range = int_range;
    return value;
  }
  MathFunctionParser math_parser(range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() != kCalcNumber)
      return nullptr;
    return math_parser.ConsumeValue();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumePositiveInteger(CSSParserTokenRange& range) {
  return ConsumeInteger(range, 1);
}

bool ConsumeNumberRaw(CSSParserTokenRange& range, double& result) {
  if (range.Peek().GetType() == kNumberToken) {
    result = range.ConsumeIncludingWhitespace().NumericValue();
    return true;
  }
  MathFunctionParser math_parser(range, kValueRangeAll);
  return math_parser.ConsumeNumberRaw(result);
}

// TODO(timloh): Work out if this can just call consumeNumberRaw
CSSPrimitiveValue* ConsumeNumber(CSSParserTokenRange& range,
                                 ValueRange value_range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kNumberToken) {
    if (value_range == kValueRangeNonNegative && token.NumericValue() < 0)
      return nullptr;
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), token.GetUnitType());
  }
  MathFunctionParser math_parser(range, kValueRangeAll);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    // TODO(rwlbuis) Calcs should not be subject to parse time range checks.
    // spec: https://drafts.csswg.org/css-values-3/#calc-range
    if (calculation->Category() != kCalcNumber ||
        (value_range == kValueRangeNonNegative && calculation->IsNegative()))
      return nullptr;
    return math_parser.ConsumeNumber();
  }
  return nullptr;
}

inline bool ShouldAcceptUnitlessLength(double value,
                                       CSSParserMode css_parser_mode,
                                       UnitlessQuirk unitless) {
  return value == 0 || css_parser_mode == kSVGAttributeMode ||
         (css_parser_mode == kHTMLQuirksMode &&
          unitless == UnitlessQuirk::kAllow);
}

CSSPrimitiveValue* ConsumeLength(CSSParserTokenRange& range,
                                 CSSParserMode css_parser_mode,
                                 ValueRange value_range,
                                 UnitlessQuirk unitless) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken) {
    switch (token.GetUnitType()) {
      case CSSPrimitiveValue::UnitType::kQuirkyEms:
        if (css_parser_mode != kUASheetMode)
          return nullptr;
        FALLTHROUGH;
      case CSSPrimitiveValue::UnitType::kEms:
      case CSSPrimitiveValue::UnitType::kRems:
      case CSSPrimitiveValue::UnitType::kChs:
      case CSSPrimitiveValue::UnitType::kExs:
      case CSSPrimitiveValue::UnitType::kPixels:
      case CSSPrimitiveValue::UnitType::kCentimeters:
      case CSSPrimitiveValue::UnitType::kMillimeters:
      case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
      case CSSPrimitiveValue::UnitType::kInches:
      case CSSPrimitiveValue::UnitType::kPoints:
      case CSSPrimitiveValue::UnitType::kPicas:
      case CSSPrimitiveValue::UnitType::kUserUnits:
      case CSSPrimitiveValue::UnitType::kViewportWidth:
      case CSSPrimitiveValue::UnitType::kViewportHeight:
      case CSSPrimitiveValue::UnitType::kViewportMin:
      case CSSPrimitiveValue::UnitType::kViewportMax:
        break;
      default:
        return nullptr;
    }
    if (value_range == kValueRangeNonNegative && token.NumericValue() < 0)
      return nullptr;
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), token.GetUnitType());
  }
  if (token.GetType() == kNumberToken) {
    if (!ShouldAcceptUnitlessLength(token.NumericValue(), css_parser_mode,
                                    unitless) ||
        (value_range == kValueRangeNonNegative && token.NumericValue() < 0))
      return nullptr;
    CSSPrimitiveValue::UnitType unit_type =
        CSSPrimitiveValue::UnitType::kPixels;
    if (css_parser_mode == kSVGAttributeMode)
      unit_type = CSSPrimitiveValue::UnitType::kUserUnits;
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), unit_type);
  }
  if (css_parser_mode == kSVGAttributeMode)
    return nullptr;
  MathFunctionParser math_parser(range, value_range);
  if (math_parser.Value() && math_parser.Value()->Category() == kCalcLength)
    return math_parser.ConsumeValue();
  return nullptr;
}

CSSPrimitiveValue* ConsumePercent(CSSParserTokenRange& range,
                                  ValueRange value_range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kPercentageToken) {
    if (value_range == kValueRangeNonNegative && token.NumericValue() < 0)
      return nullptr;
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kPercentage);
  }
  MathFunctionParser math_parser(range, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() == kCalcPercent)
      return math_parser.ConsumeValue();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeAlphaValue(CSSParserTokenRange& range) {
  if (CSSPrimitiveValue* value = ConsumeNumber(range, kValueRangeAll)) {
    return value;
  }
  if (CSSPrimitiveValue* value = ConsumePercent(range, kValueRangeAll)) {
    return CSSNumericLiteralValue::Create(value->GetDoubleValue() / 100.0,
                                          CSSPrimitiveValue::UnitType::kNumber);
  }
  return nullptr;
}

bool CanConsumeCalcValue(CalculationCategory category,
                         CSSParserMode css_parser_mode) {
  if (category == kCalcLength || category == kCalcPercent ||
      category == kCalcPercentLength)
    return true;

  if (css_parser_mode != kSVGAttributeMode)
    return false;

  if (category == kCalcNumber || category == kCalcPercentNumber ||
      category == kCalcLengthNumber || category == kCalcPercentLengthNumber)
    return true;

  return false;
}

CSSPrimitiveValue* ConsumeLengthOrPercent(CSSParserTokenRange& range,
                                          CSSParserMode css_parser_mode,
                                          ValueRange value_range,
                                          UnitlessQuirk unitless) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken || token.GetType() == kNumberToken)
    return ConsumeLength(range, css_parser_mode, value_range, unitless);
  if (token.GetType() == kPercentageToken)
    return ConsumePercent(range, value_range);
  MathFunctionParser math_parser(range, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (CanConsumeCalcValue(calculation->Category(), css_parser_mode))
      return math_parser.ConsumeValue();
  }
  return nullptr;
}

namespace {

bool IsNonZeroUserUnitsValue(const CSSPrimitiveValue* value) {
  if (!value)
    return false;
  if (const auto* numeric_literal = DynamicTo<CSSNumericLiteralValue>(value)) {
    return numeric_literal->GetType() ==
               CSSPrimitiveValue::UnitType::kUserUnits &&
           value->GetDoubleValue() != 0;
  }
  const auto& math_value = To<CSSMathFunctionValue>(*value);
  switch (math_value.Category()) {
    case kCalcNumber:
      return math_value.DoubleValue() != 0;
    case kCalcPercentNumber:
    case kCalcLengthNumber:
    case kCalcPercentLengthNumber:
      return true;
    default:
      return false;
  }
}

}  // namespace

CSSPrimitiveValue* ConsumeSVGGeometryPropertyLength(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    ValueRange value_range) {
  CSSPrimitiveValue* value = ConsumeLengthOrPercent(
      range, kSVGAttributeMode, value_range, UnitlessQuirk::kForbid);
  if (IsNonZeroUserUnitsValue(value))
    context.Count(WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue);
  return value;
}

CSSPrimitiveValue* ConsumeGradientLengthOrPercent(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    ValueRange value_range,
    UnitlessQuirk unitless) {
  return ConsumeLengthOrPercent(range, context.Mode(), value_range, unitless);
}

CSSPrimitiveValue* ConsumeAngle(
    CSSParserTokenRange& range,
    const CSSParserContext* context,
    base::Optional<WebFeature> unitless_zero_feature,
    double minimum_value,
    double maximum_value) {
  // Ensure that we have a context for counting the
  // unitless_zero_feature if it is requested.
  DCHECK(context || !unitless_zero_feature);
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken) {
    switch (token.GetUnitType()) {
      case CSSPrimitiveValue::UnitType::kDegrees:
      case CSSPrimitiveValue::UnitType::kRadians:
      case CSSPrimitiveValue::UnitType::kGradians:
      case CSSPrimitiveValue::UnitType::kTurns:
        return CSSNumericLiteralValue::Create(
            range.ConsumeIncludingWhitespace().NumericValue(),
            token.GetUnitType());
      default:
        return nullptr;
    }
  }
  if (token.GetType() == kNumberToken && token.NumericValue() == 0 &&
      unitless_zero_feature) {
    range.ConsumeIncludingWhitespace();
    context->Count(*unitless_zero_feature);
    return CSSNumericLiteralValue::Create(
        0, CSSPrimitiveValue::UnitType::kDegrees);
  }
  MathFunctionParser math_parser(range, kValueRangeAll);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() != kCalcAngle)
      return nullptr;
    if (CSSMathFunctionValue* result = math_parser.ConsumeValue()) {
      if (result->ComputeDegrees() < minimum_value) {
        return CSSNumericLiteralValue::Create(
            minimum_value, CSSPrimitiveValue::UnitType::kDegrees);
      }
      if (result->ComputeDegrees() > maximum_value) {
        return CSSNumericLiteralValue::Create(
            maximum_value, CSSPrimitiveValue::UnitType::kDegrees);
      }
      return result;
    }
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeAngle(
    CSSParserTokenRange& range,
    const CSSParserContext* context,
    base::Optional<WebFeature> unitless_zero_feature) {
  return ConsumeAngle(range, context, std::move(unitless_zero_feature),
                      std::numeric_limits<double>::lowest(),
                      std::numeric_limits<double>::max());
}

CSSPrimitiveValue* ConsumeTime(CSSParserTokenRange& range,
                               ValueRange value_range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken) {
    if (value_range == kValueRangeNonNegative && token.NumericValue() < 0)
      return nullptr;
    CSSPrimitiveValue::UnitType unit = token.GetUnitType();
    if (unit == CSSPrimitiveValue::UnitType::kMilliseconds ||
        unit == CSSPrimitiveValue::UnitType::kSeconds)
      return CSSNumericLiteralValue::Create(
          range.ConsumeIncludingWhitespace().NumericValue(),
          token.GetUnitType());
    return nullptr;
  }
  MathFunctionParser math_parser(range, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() == kCalcTime)
      return math_parser.ConsumeValue();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeResolution(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  // Unlike the other types, calc() does not work with <resolution>.
  if (token.GetType() != kDimensionToken)
    return nullptr;
  CSSPrimitiveValue::UnitType unit = token.GetUnitType();
  if (unit == CSSPrimitiveValue::UnitType::kDotsPerPixel ||
      unit == CSSPrimitiveValue::UnitType::kDotsPerInch ||
      unit == CSSPrimitiveValue::UnitType::kDotsPerCentimeter) {
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), unit);
  }
  return nullptr;
}

CSSIdentifierValue* ConsumeIdent(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken)
    return nullptr;
  return CSSIdentifierValue::Create(range.ConsumeIncludingWhitespace().Id());
}

CSSIdentifierValue* ConsumeIdentRange(CSSParserTokenRange& range,
                                      CSSValueID lower,
                                      CSSValueID upper) {
  if (range.Peek().Id() < lower || range.Peek().Id() > upper)
    return nullptr;
  return ConsumeIdent(range);
}

CSSCustomIdentValue* ConsumeCustomIdentWithToken(
    const CSSParserToken& token,
    const CSSParserContext& context) {
  if (token.GetType() != kIdentToken || IsCSSWideKeyword(token.Value()))
    return nullptr;

  if (EqualIgnoringASCIICase(token.Value(), "default"))
    context.Count(WebFeature::kDefaultInCustomIdent);
  if (EqualIgnoringASCIICase(token.Value(), "revert"))
    context.Count(WebFeature::kRevertInCustomIdent);

  return MakeGarbageCollected<CSSCustomIdentValue>(
      token.Value().ToAtomicString());
}

CSSCustomIdentValue* ConsumeCustomIdent(CSSParserTokenRange& range,
                                        const CSSParserContext& context) {
  if (range.Peek().GetType() != kIdentToken ||
      IsCSSWideKeyword(range.Peek().Value()))
    return nullptr;

  return ConsumeCustomIdentWithToken(range.ConsumeIncludingWhitespace(),
                                     context);
}

CSSStringValue* ConsumeString(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kStringToken)
    return nullptr;
  return MakeGarbageCollected<CSSStringValue>(
      range.ConsumeIncludingWhitespace().Value().ToString());
}

StringView ConsumeUrlAsStringView(CSSParserTokenRange& range,
                                  const CSSParserContext* context) {
  StringView url;
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kUrlToken) {
    range.ConsumeIncludingWhitespace();
    url = token.Value();
  } else if (token.FunctionId() == CSSValueID::kUrl) {
    CSSParserTokenRange url_range = range;
    CSSParserTokenRange url_args = url_range.ConsumeBlock();
    const CSSParserToken& next = url_args.ConsumeIncludingWhitespace();
    if (next.GetType() == kBadStringToken || !url_args.AtEnd())
      return StringView();
    DCHECK_EQ(next.GetType(), kStringToken);
    range = url_range;
    range.ConsumeWhitespace();
    url = next.Value();
  }

  // Invalidate the URL if only data URLs are allowed and the protocol is not
  // data.
  if (!url.IsNull() &&
      context->ResourceFetchRestriction() ==
          ResourceFetchRestriction::kOnlyDataUrls &&
      !ProtocolIs(url.ToString(), "data")) {
    // The StringView must be instantiated with an empty string otherwise the
    // URL will incorrectly be identified as null. The resource should behave as
    // if it failed to load.
    url = StringView("");
  }

  return url;
}

cssvalue::CSSURIValue* ConsumeUrl(CSSParserTokenRange& range,
                                  const CSSParserContext* context) {
  StringView url = ConsumeUrlAsStringView(range, context);
  if (url.IsNull())
    return nullptr;
  String url_string = url.ToString();
  return cssvalue::CSSURIValue::Create(url_string,
                                       context->CompleteURL(url_string));
}

static int ClampRGBComponent(const CSSPrimitiveValue& value) {
  double result = value.GetDoubleValue();
  if (value.IsPercentage()) {
    // 2.55 cannot be precisely represented as a double
    result = (result / 100.0) * 255.0;
  }
  return clampTo<int>(round(result), 0, 255);
}

static bool ParseRGBParameters(CSSParserTokenRange& range, RGBA32& result) {
  DCHECK(range.Peek().FunctionId() == CSSValueID::kRgb ||
         range.Peek().FunctionId() == CSSValueID::kRgba);
  CSSParserTokenRange args = ConsumeFunction(range);
  CSSPrimitiveValue* color_parameter = ConsumeNumber(args, kValueRangeAll);
  if (!color_parameter)
    color_parameter = ConsumePercent(args, kValueRangeAll);
  if (!color_parameter)
    return false;
  const bool is_percent = color_parameter->IsPercentage();
  int color_array[3];
  color_array[0] = ClampRGBComponent(*color_parameter);
  bool requires_commas = false;
  for (int i = 1; i < 3; i++) {
    if (ConsumeCommaIncludingWhitespace(args)) {
      if (i != 1 && !requires_commas)
        return false;
      requires_commas = true;
    } else if (requires_commas || args.AtEnd()) {
      return false;
    }
    color_parameter = is_percent ? ConsumePercent(args, kValueRangeAll)
                                 : ConsumeNumber(args, kValueRangeAll);
    if (!color_parameter)
      return false;
    color_array[i] = ClampRGBComponent(*color_parameter);
  }

  bool comma_consumed = ConsumeCommaIncludingWhitespace(args);
  bool slash_consumed = ConsumeSlashIncludingWhitespace(args);
  if ((comma_consumed && !requires_commas) ||
      (slash_consumed && requires_commas))
    return false;
  if (comma_consumed || slash_consumed) {
    double alpha;
    if (!ConsumeNumberRaw(args, alpha)) {
      CSSPrimitiveValue* alpha_percent = ConsumePercent(args, kValueRangeAll);
      if (!alpha_percent)
        return false;
      else
        alpha = alpha_percent->GetDoubleValue() / 100.0;
    }
    // W3 standard stipulates a 2.55 alpha value multiplication factor.
    int alpha_component =
        static_cast<int>(lround(clampTo<double>(alpha, 0.0, 1.0) * 255.0));
    result = MakeRGBA(color_array[0], color_array[1], color_array[2],
                      alpha_component);
  } else {
    result = MakeRGB(color_array[0], color_array[1], color_array[2]);
  }
  return args.AtEnd();
}

static bool ParseHSLParameters(CSSParserTokenRange& range, RGBA32& result) {
  DCHECK(range.Peek().FunctionId() == CSSValueID::kHsl ||
         range.Peek().FunctionId() == CSSValueID::kHsla);
  CSSParserTokenRange args = ConsumeFunction(range);
  CSSPrimitiveValue* hsl_value = ConsumeAngle(args, nullptr, base::nullopt);
  double angle_value;
  if (!hsl_value) {
    hsl_value = ConsumeNumber(args, kValueRangeAll);
    if (!hsl_value)
      return false;
    angle_value = hsl_value->GetDoubleValue();
  } else {
    angle_value = hsl_value->ComputeDegrees();
  }
  double color_array[3];
  color_array[0] = fmod(fmod(angle_value, 360.0) + 360.0, 360.0) / 60.0;
  bool requires_commas = false;
  for (int i = 1; i < 3; i++) {
    if (ConsumeCommaIncludingWhitespace(args)) {
      if (i != 1 && !requires_commas)
        return false;
      requires_commas = true;
    } else if (requires_commas || args.AtEnd()) {
      return false;
    }
    hsl_value = ConsumePercent(args, kValueRangeAll);
    if (!hsl_value)
      return false;
    double double_value = hsl_value->GetDoubleValue();
    color_array[i] = clampTo<double>(double_value, 0.0, 100.0) /
                     100.0;  // Needs to be value between 0 and 1.0.
  }

  double alpha = 1.0;
  bool comma_consumed = ConsumeCommaIncludingWhitespace(args);
  bool slash_consumed = ConsumeSlashIncludingWhitespace(args);
  if ((comma_consumed && !requires_commas) ||
      (slash_consumed && requires_commas))
    return false;
  if (comma_consumed || slash_consumed) {
    if (!ConsumeNumberRaw(args, alpha)) {
      CSSPrimitiveValue* alpha_percent = ConsumePercent(args, kValueRangeAll);
      if (!alpha_percent)
        return false;
      else
        alpha = alpha_percent->GetDoubleValue() / 100.0;
    }
    alpha = clampTo<double>(alpha, 0.0, 1.0);
  }
  result =
      MakeRGBAFromHSLA(color_array[0], color_array[1], color_array[2], alpha);
  return args.AtEnd();
}

static bool ParseHexColor(CSSParserTokenRange& range,
                          RGBA32& result,
                          bool accept_quirky_colors) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kHashToken) {
    if (!Color::ParseHexColor(token.Value(), result))
      return false;
  } else if (accept_quirky_colors) {
    String color;
    if (token.GetType() == kNumberToken || token.GetType() == kDimensionToken) {
      if (token.GetNumericValueType() != kIntegerValueType ||
          token.NumericValue() < 0. || token.NumericValue() >= 1000000.)
        return false;
      if (token.GetType() == kNumberToken)  // e.g. 112233
        color = String::Format("%d", static_cast<int>(token.NumericValue()));
      else  // e.g. 0001FF
        color = String::Number(static_cast<int>(token.NumericValue())) +
                token.Value().ToString();
      while (color.length() < 6)
        color = "0" + color;
    } else if (token.GetType() == kIdentToken) {  // e.g. FF0000
      color = token.Value().ToString();
    }
    unsigned length = color.length();
    if (length != 3 && length != 6)
      return false;
    if (!Color::ParseHexColor(color, result))
      return false;
  } else {
    return false;
  }
  range.ConsumeIncludingWhitespace();
  return true;
}

static bool ParseColorFunction(CSSParserTokenRange& range, RGBA32& result) {
  CSSValueID function_id = range.Peek().FunctionId();
  if (function_id < CSSValueID::kRgb || function_id > CSSValueID::kHsla)
    return false;
  CSSParserTokenRange color_range = range;
  if ((function_id <= CSSValueID::kRgba &&
       !ParseRGBParameters(color_range, result)) ||
      (function_id >= CSSValueID::kHsl &&
       !ParseHSLParameters(color_range, result)))
    return false;
  range = color_range;
  return true;
}

static CSSLightDarkColorPair* ParseLightDarkColor(CSSParserTokenRange& range,
                                                  CSSParserMode mode) {
  if (range.Peek().FunctionId() != CSSValueID::kInternalLightDarkColor)
    return nullptr;
  if (!isValueAllowedInMode(CSSValueID::kInternalLightDarkColor, mode))
    return nullptr;
  CSSParserTokenRange args = ConsumeFunction(range);
  CSSValue* light_color = ConsumeColor(args, kUASheetMode);
  if (!light_color || !ConsumeCommaIncludingWhitespace(args))
    return nullptr;
  CSSValue* dark_color = ConsumeColor(args, kUASheetMode);
  if (!dark_color || !args.AtEnd())
    return nullptr;
  return MakeGarbageCollected<CSSLightDarkColorPair>(light_color, dark_color);
}

CSSValue* ConsumeColor(CSSParserTokenRange& range,
                       CSSParserMode css_parser_mode,
                       bool accept_quirky_colors) {
  CSSValueID id = range.Peek().Id();
  if (StyleColor::IsColorKeyword(id)) {
    if (!isValueAllowedInMode(id, css_parser_mode))
      return nullptr;
    CSSIdentifierValue* color = ConsumeIdent(range);
    if (!RuntimeEnabledFeatures::LinkSystemColorsEnabled() &&
        (color->GetValueID() == CSSValueID::kLinktext ||
         color->GetValueID() == CSSValueID::kVisitedtext)) {
      return nullptr;
    } else if (!RuntimeEnabledFeatures::NewSystemColorsEnabled() &&
               (id == CSSValueID::kActivetext || id == CSSValueID::kField ||
                id == CSSValueID::kFieldtext)) {
      return nullptr;
    }
    return color;
  }
  RGBA32 color = Color::kTransparent;
  if (!ParseHexColor(range, color, accept_quirky_colors) &&
      !ParseColorFunction(range, color)) {
    return ParseLightDarkColor(range, css_parser_mode);
  }
  return cssvalue::CSSColorValue::Create(color);
}

CSSValue* ConsumeLineWidth(CSSParserTokenRange& range,
                           CSSParserMode css_parser_mode,
                           UnitlessQuirk unitless) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kThin || id == CSSValueID::kMedium ||
      id == CSSValueID::kThick)
    return ConsumeIdent(range);
  return ConsumeLength(range, css_parser_mode, kValueRangeNonNegative,
                       unitless);
}

static CSSValue* ConsumePositionComponent(CSSParserTokenRange& range,
                                          CSSParserMode css_parser_mode,
                                          UnitlessQuirk unitless,
                                          bool& horizontal_edge,
                                          bool& vertical_edge) {
  if (range.Peek().GetType() != kIdentToken)
    return ConsumeLengthOrPercent(range, css_parser_mode, kValueRangeAll,
                                  unitless);

  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kLeft || id == CSSValueID::kRight) {
    if (horizontal_edge)
      return nullptr;
    horizontal_edge = true;
  } else if (id == CSSValueID::kTop || id == CSSValueID::kBottom) {
    if (vertical_edge)
      return nullptr;
    vertical_edge = true;
  } else if (id != CSSValueID::kCenter) {
    return nullptr;
  }
  return ConsumeIdent(range);
}

static bool IsHorizontalPositionKeywordOnly(const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return false;
  CSSValueID value_id = identifier_value->GetValueID();
  return value_id == CSSValueID::kLeft || value_id == CSSValueID::kRight;
}

static bool IsVerticalPositionKeywordOnly(const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return false;
  CSSValueID value_id = identifier_value->GetValueID();
  return value_id == CSSValueID::kTop || value_id == CSSValueID::kBottom;
}

static void PositionFromOneValue(CSSValue* value,
                                 CSSValue*& result_x,
                                 CSSValue*& result_y) {
  bool value_applies_to_y_axis_only = IsVerticalPositionKeywordOnly(*value);
  result_x = value;
  result_y = CSSIdentifierValue::Create(CSSValueID::kCenter);
  if (value_applies_to_y_axis_only)
    std::swap(result_x, result_y);
}

static void PositionFromTwoValues(CSSValue* value1,
                                  CSSValue* value2,
                                  CSSValue*& result_x,
                                  CSSValue*& result_y) {
  bool must_order_as_xy = IsHorizontalPositionKeywordOnly(*value1) ||
                          IsVerticalPositionKeywordOnly(*value2) ||
                          !value1->IsIdentifierValue() ||
                          !value2->IsIdentifierValue();
  bool must_order_as_yx = IsVerticalPositionKeywordOnly(*value1) ||
                          IsHorizontalPositionKeywordOnly(*value2);
  DCHECK(!must_order_as_xy || !must_order_as_yx);
  result_x = value1;
  result_y = value2;
  if (must_order_as_yx)
    std::swap(result_x, result_y);
}

static void PositionFromThreeOrFourValues(CSSValue** values,
                                          CSSValue*& result_x,
                                          CSSValue*& result_y) {
  CSSIdentifierValue* center = nullptr;
  for (int i = 0; values[i]; i++) {
    auto* current_value = To<CSSIdentifierValue>(values[i]);
    CSSValueID id = current_value->GetValueID();

    if (id == CSSValueID::kCenter) {
      DCHECK(!center);
      center = current_value;
      continue;
    }

    CSSValue* result = nullptr;
    if (values[i + 1] && !values[i + 1]->IsIdentifierValue()) {
      result = MakeGarbageCollected<CSSValuePair>(
          current_value, values[++i], CSSValuePair::kKeepIdenticalValues);
    } else {
      result = current_value;
    }

    if (id == CSSValueID::kLeft || id == CSSValueID::kRight) {
      DCHECK(!result_x);
      result_x = result;
    } else {
      DCHECK(id == CSSValueID::kTop || id == CSSValueID::kBottom);
      DCHECK(!result_y);
      result_y = result;
    }
  }

  if (center) {
    DCHECK(!!result_x != !!result_y);
    if (!result_x)
      result_x = center;
    else
      result_y = center;
  }

  DCHECK(result_x && result_y);
}

bool ConsumePosition(CSSParserTokenRange& range,
                     const CSSParserContext& context,
                     UnitlessQuirk unitless,
                     base::Optional<WebFeature> three_value_position,
                     CSSValue*& result_x,
                     CSSValue*& result_y) {
  bool horizontal_edge = false;
  bool vertical_edge = false;
  CSSValue* value1 = ConsumePositionComponent(range, context.Mode(), unitless,
                                              horizontal_edge, vertical_edge);
  if (!value1)
    return false;
  if (!value1->IsIdentifierValue())
    horizontal_edge = true;

  CSSParserTokenRange range_after_first_consume = range;
  CSSValue* value2 = ConsumePositionComponent(range, context.Mode(), unitless,
                                              horizontal_edge, vertical_edge);
  if (!value2) {
    PositionFromOneValue(value1, result_x, result_y);
    return true;
  }

  CSSParserTokenRange range_after_second_consume = range;
  CSSValue* value3 = nullptr;
  auto* identifier_value1 = DynamicTo<CSSIdentifierValue>(value1);
  auto* identifier_value2 = DynamicTo<CSSIdentifierValue>(value2);
  // TODO(crbug.com/940442): Fix the strange comparison of a
  // CSSIdentifierValue instance against a specific "range peek" type check.
  if (identifier_value1 &&
      !!identifier_value2 != (range.Peek().GetType() == kIdentToken) &&
      (identifier_value2
           ? identifier_value2->GetValueID()
           : identifier_value1->GetValueID()) != CSSValueID::kCenter)
    value3 = ConsumePositionComponent(range, context.Mode(), unitless,
                                      horizontal_edge, vertical_edge);
  if (!value3) {
    if (vertical_edge && !value2->IsIdentifierValue()) {
      range = range_after_first_consume;
      PositionFromOneValue(value1, result_x, result_y);
      return true;
    }
    PositionFromTwoValues(value1, value2, result_x, result_y);
    return true;
  }

  CSSValue* value4 = nullptr;
  auto* identifier_value3 = DynamicTo<CSSIdentifierValue>(value3);
  if (identifier_value3 &&
      identifier_value3->GetValueID() != CSSValueID::kCenter &&
      range.Peek().GetType() != kIdentToken)
    value4 = ConsumePositionComponent(range, context.Mode(), unitless,
                                      horizontal_edge, vertical_edge);

  if (!value4) {
    if (!three_value_position) {
      // [top | bottom] <length-percentage> is not permitted
      if (vertical_edge && !value2->IsIdentifierValue()) {
        range = range_after_first_consume;
        PositionFromOneValue(value1, result_x, result_y);
        return true;
      }
      range = range_after_second_consume;
      PositionFromTwoValues(value1, value2, result_x, result_y);
      return true;
    }
    DCHECK_EQ(*three_value_position,
              WebFeature::kThreeValuedPositionBackground);
    context.Count(*three_value_position);
  }

  CSSValue* values[5];
  values[0] = value1;
  values[1] = value2;
  values[2] = value3;
  values[3] = value4;
  values[4] = nullptr;
  PositionFromThreeOrFourValues(values, result_x, result_y);
  return true;
}

CSSValuePair* ConsumePosition(CSSParserTokenRange& range,
                              const CSSParserContext& context,
                              UnitlessQuirk unitless,
                              base::Optional<WebFeature> three_value_position) {
  CSSValue* result_x = nullptr;
  CSSValue* result_y = nullptr;
  if (ConsumePosition(range, context, unitless, three_value_position, result_x,
                      result_y)) {
    return MakeGarbageCollected<CSSValuePair>(
        result_x, result_y, CSSValuePair::kKeepIdenticalValues);
  }
  return nullptr;
}

bool ConsumeOneOrTwoValuedPosition(CSSParserTokenRange& range,
                                   CSSParserMode css_parser_mode,
                                   UnitlessQuirk unitless,
                                   CSSValue*& result_x,
                                   CSSValue*& result_y) {
  bool horizontal_edge = false;
  bool vertical_edge = false;
  CSSValue* value1 = ConsumePositionComponent(range, css_parser_mode, unitless,
                                              horizontal_edge, vertical_edge);
  if (!value1)
    return false;
  if (!value1->IsIdentifierValue())
    horizontal_edge = true;

  if (vertical_edge && ConsumeLengthOrPercent(range, css_parser_mode,
                                              kValueRangeAll, unitless)) {
    // <length-percentage> is not permitted after top | bottom.
    return false;
  }
  CSSValue* value2 = ConsumePositionComponent(range, css_parser_mode, unitless,
                                              horizontal_edge, vertical_edge);
  if (!value2) {
    PositionFromOneValue(value1, result_x, result_y);
    return true;
  }
  PositionFromTwoValues(value1, value2, result_x, result_y);
  return true;
}

bool ConsumeBorderShorthand(CSSParserTokenRange& range,
                            const CSSParserContext& context,
                            const CSSValue*& result_width,
                            const CSSValue*& result_style,
                            const CSSValue*& result_color) {
  while (!result_width || !result_style || !result_color) {
    if (!result_width) {
      result_width = css_property_parser_helpers::ConsumeLineWidth(
          range, context.Mode(),
          css_property_parser_helpers::UnitlessQuirk::kForbid);
      if (result_width)
        continue;
    }
    if (!result_style) {
      result_style = css_property_parser_helpers::ParseLonghand(
          CSSPropertyID::kBorderLeftStyle, CSSPropertyID::kBorder, context,
          range);
      if (result_style)
        continue;
    }
    if (!result_color) {
      result_color =
          css_property_parser_helpers::ConsumeColor(range, context.Mode());
      if (result_color)
        continue;
    }
    break;
  }

  if (!result_width && !result_style && !result_color)
    return false;

  if (!result_width)
    result_width = CSSInitialValue::Create();
  if (!result_style)
    result_style = CSSInitialValue::Create();
  if (!result_color)
    result_color = CSSInitialValue::Create();
  return true;
}

// This should go away once we drop support for -webkit-gradient
static CSSPrimitiveValue* ConsumeDeprecatedGradientPoint(
    CSSParserTokenRange& args,
    bool horizontal) {
  if (args.Peek().GetType() == kIdentToken) {
    if ((horizontal && ConsumeIdent<CSSValueID::kLeft>(args)) ||
        (!horizontal && ConsumeIdent<CSSValueID::kTop>(args)))
      return CSSNumericLiteralValue::Create(
          0., CSSPrimitiveValue::UnitType::kPercentage);
    if ((horizontal && ConsumeIdent<CSSValueID::kRight>(args)) ||
        (!horizontal && ConsumeIdent<CSSValueID::kBottom>(args)))
      return CSSNumericLiteralValue::Create(
          100., CSSPrimitiveValue::UnitType::kPercentage);
    if (ConsumeIdent<CSSValueID::kCenter>(args))
      return CSSNumericLiteralValue::Create(
          50., CSSPrimitiveValue::UnitType::kPercentage);
    return nullptr;
  }
  CSSPrimitiveValue* result = ConsumePercent(args, kValueRangeAll);
  if (!result)
    result = ConsumeNumber(args, kValueRangeAll);
  return result;
}

// Used to parse colors for -webkit-gradient(...).
static CSSValue* ConsumeDeprecatedGradientStopColor(
    CSSParserTokenRange& args,
    CSSParserMode css_parser_mode) {
  if (args.Peek().Id() == CSSValueID::kCurrentcolor)
    return nullptr;
  return ConsumeColor(args, css_parser_mode);
}

static bool ConsumeDeprecatedGradientColorStop(
    CSSParserTokenRange& range,
    cssvalue::CSSGradientColorStop& stop,
    CSSParserMode css_parser_mode) {
  CSSValueID id = range.Peek().FunctionId();
  if (id != CSSValueID::kFrom && id != CSSValueID::kTo &&
      id != CSSValueID::kColorStop)
    return false;

  CSSParserTokenRange args = ConsumeFunction(range);
  double position;
  if (id == CSSValueID::kFrom || id == CSSValueID::kTo) {
    position = (id == CSSValueID::kFrom) ? 0 : 1;
  } else {
    DCHECK(id == CSSValueID::kColorStop);
    if (CSSPrimitiveValue* percent_value = ConsumePercent(args, kValueRangeAll))
      position = percent_value->GetDoubleValue() / 100.0;
    else if (!ConsumeNumberRaw(args, position))
      return false;

    if (!ConsumeCommaIncludingWhitespace(args))
      return false;
  }

  stop.offset_ = CSSNumericLiteralValue::Create(
      position, CSSPrimitiveValue::UnitType::kNumber);
  stop.color_ = ConsumeDeprecatedGradientStopColor(args, css_parser_mode);
  return stop.color_ && args.AtEnd();
}

static CSSValue* ConsumeDeprecatedGradient(CSSParserTokenRange& args,
                                           CSSParserMode css_parser_mode) {
  CSSValueID id = args.ConsumeIncludingWhitespace().Id();
  if (id != CSSValueID::kRadial && id != CSSValueID::kLinear)
    return nullptr;

  if (!ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  const CSSPrimitiveValue* first_x = ConsumeDeprecatedGradientPoint(args, true);
  if (!first_x)
    return nullptr;
  const CSSPrimitiveValue* first_y =
      ConsumeDeprecatedGradientPoint(args, false);
  if (!first_y)
    return nullptr;
  if (!ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  // For radial gradients only, we now expect a numeric radius.
  const CSSPrimitiveValue* first_radius = nullptr;
  if (id == CSSValueID::kRadial) {
    first_radius = ConsumeNumber(args, kValueRangeNonNegative);
    if (!first_radius || !ConsumeCommaIncludingWhitespace(args))
      return nullptr;
  }

  const CSSPrimitiveValue* second_x =
      ConsumeDeprecatedGradientPoint(args, true);
  if (!second_x)
    return nullptr;
  const CSSPrimitiveValue* second_y =
      ConsumeDeprecatedGradientPoint(args, false);
  if (!second_y)
    return nullptr;

  // For radial gradients only, we now expect the second radius.
  const CSSPrimitiveValue* second_radius = nullptr;
  if (id == CSSValueID::kRadial) {
    if (!ConsumeCommaIncludingWhitespace(args))
      return nullptr;
    second_radius = ConsumeNumber(args, kValueRangeNonNegative);
    if (!second_radius)
      return nullptr;
  }

  cssvalue::CSSGradientValue* result =
      (id == CSSValueID::kRadial)
          ? cssvalue::CSSRadialGradientValue::Create(
                first_x, first_y, first_radius, second_x, second_y,
                second_radius, cssvalue::kNonRepeating,
                cssvalue::kCSSDeprecatedRadialGradient)
          : MakeGarbageCollected<cssvalue::CSSLinearGradientValue>(
                first_x, first_y, second_x, second_y, nullptr,
                cssvalue::kNonRepeating,
                cssvalue::kCSSDeprecatedLinearGradient);
  cssvalue::CSSGradientColorStop stop;
  while (ConsumeCommaIncludingWhitespace(args)) {
    if (!ConsumeDeprecatedGradientColorStop(args, stop, css_parser_mode))
      return nullptr;
    result->AddStop(stop);
  }

  return result;
}

static CSSPrimitiveValue* ConsumeGradientAngleOrPercent(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    ValueRange value_range,
    UnitlessQuirk) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken || token.GetType() == kNumberToken) {
    return ConsumeAngle(range, &context,
                        WebFeature::kUnitlessZeroAngleGradient);
  }
  if (token.GetType() == kPercentageToken)
    return ConsumePercent(range, value_range);
  MathFunctionParser math_parser(range, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    CalculationCategory category = calculation->Category();
    // TODO(fs): Add and support kCalcPercentAngle?
    if (category == kCalcAngle || category == kCalcPercent)
      return math_parser.ConsumeValue();
  }
  return nullptr;
}

using PositionFunctor = CSSPrimitiveValue* (*)(CSSParserTokenRange&,
                                               const CSSParserContext&,
                                               ValueRange,
                                               UnitlessQuirk);

static bool ConsumeGradientColorStops(CSSParserTokenRange& range,
                                      const CSSParserContext& context,
                                      cssvalue::CSSGradientValue* gradient,
                                      PositionFunctor consume_position_func) {
  bool supports_color_hints =
      gradient->GradientType() == cssvalue::kCSSLinearGradient ||
      gradient->GradientType() == cssvalue::kCSSRadialGradient ||
      gradient->GradientType() == cssvalue::kCSSConicGradient;

  // The first color stop cannot be a color hint.
  bool previous_stop_was_color_hint = true;
  do {
    cssvalue::CSSGradientColorStop stop;
    stop.color_ = ConsumeColor(range, context.Mode());
    // Two hints in a row are not allowed.
    if (!stop.color_ && (!supports_color_hints || previous_stop_was_color_hint))
      return false;
    previous_stop_was_color_hint = !stop.color_;
    stop.offset_ = consume_position_func(range, context, kValueRangeAll,
                                         UnitlessQuirk::kForbid);
    if (!stop.color_ && !stop.offset_)
      return false;
    gradient->AddStop(stop);

    if (!stop.color_ || !stop.offset_)
      continue;

    // Optional second position.
    stop.offset_ = consume_position_func(range, context, kValueRangeAll,
                                         UnitlessQuirk::kForbid);
    if (stop.offset_)
      gradient->AddStop(stop);
  } while (ConsumeCommaIncludingWhitespace(range));

  // The last color stop cannot be a color hint.
  if (previous_stop_was_color_hint)
    return false;

  // Must have 2 or more stops to be valid.
  return gradient->StopCount() >= 2;
}

static CSSValue* ConsumeDeprecatedRadialGradient(
    CSSParserTokenRange& args,
    const CSSParserContext& context,
    cssvalue::CSSGradientRepeat repeating) {
  CSSValue* center_x = nullptr;
  CSSValue* center_y = nullptr;
  ConsumeOneOrTwoValuedPosition(args, context.Mode(), UnitlessQuirk::kForbid,
                                center_x, center_y);
  if ((center_x || center_y) && !ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  const CSSIdentifierValue* shape =
      ConsumeIdent<CSSValueID::kCircle, CSSValueID::kEllipse>(args);
  const CSSIdentifierValue* size_keyword =
      ConsumeIdent<CSSValueID::kClosestSide, CSSValueID::kClosestCorner,
                   CSSValueID::kFarthestSide, CSSValueID::kFarthestCorner,
                   CSSValueID::kContain, CSSValueID::kCover>(args);
  if (!shape)
    shape = ConsumeIdent<CSSValueID::kCircle, CSSValueID::kEllipse>(args);

  // Or, two lengths or percentages
  const CSSPrimitiveValue* horizontal_size = nullptr;
  const CSSPrimitiveValue* vertical_size = nullptr;
  if (!shape && !size_keyword) {
    horizontal_size =
        ConsumeLengthOrPercent(args, context.Mode(), kValueRangeNonNegative);
    if (horizontal_size) {
      vertical_size =
          ConsumeLengthOrPercent(args, context.Mode(), kValueRangeNonNegative);
      if (!vertical_size)
        return nullptr;
      ConsumeCommaIncludingWhitespace(args);
    }
  } else {
    ConsumeCommaIncludingWhitespace(args);
  }

  cssvalue::CSSGradientValue* result = cssvalue::CSSRadialGradientValue::Create(
      center_x, center_y, shape, size_keyword, horizontal_size, vertical_size,
      repeating, cssvalue::kCSSPrefixedRadialGradient);
  return ConsumeGradientColorStops(args, context, result,
                                   ConsumeGradientLengthOrPercent)
             ? result
             : nullptr;
}

static CSSValue* ConsumeRadialGradient(CSSParserTokenRange& args,
                                       const CSSParserContext& context,
                                       cssvalue::CSSGradientRepeat repeating) {
  const CSSIdentifierValue* shape = nullptr;
  const CSSIdentifierValue* size_keyword = nullptr;
  const CSSPrimitiveValue* horizontal_size = nullptr;
  const CSSPrimitiveValue* vertical_size = nullptr;

  // First part of grammar, the size/shape clause:
  // [ circle || <length> ] |
  // [ ellipse || [ <length> | <percentage> ]{2} ] |
  // [ [ circle | ellipse] || <size-keyword> ]
  for (int i = 0; i < 3; ++i) {
    if (args.Peek().GetType() == kIdentToken) {
      CSSValueID id = args.Peek().Id();
      if (id == CSSValueID::kCircle || id == CSSValueID::kEllipse) {
        if (shape)
          return nullptr;
        shape = ConsumeIdent(args);
      } else if (id == CSSValueID::kClosestSide ||
                 id == CSSValueID::kClosestCorner ||
                 id == CSSValueID::kFarthestSide ||
                 id == CSSValueID::kFarthestCorner) {
        if (size_keyword)
          return nullptr;
        size_keyword = ConsumeIdent(args);
      } else {
        break;
      }
    } else {
      CSSPrimitiveValue* center =
          ConsumeLengthOrPercent(args, context.Mode(), kValueRangeNonNegative);
      if (!center)
        break;
      if (horizontal_size)
        return nullptr;
      horizontal_size = center;
      center =
          ConsumeLengthOrPercent(args, context.Mode(), kValueRangeNonNegative);
      if (center) {
        vertical_size = center;
        ++i;
      }
    }
  }

  // You can specify size as a keyword or a length/percentage, not both.
  if (size_keyword && horizontal_size)
    return nullptr;
  // Circles must have 0 or 1 lengths.
  if (shape && shape->GetValueID() == CSSValueID::kCircle && vertical_size)
    return nullptr;
  // Ellipses must have 0 or 2 length/percentages.
  if (shape && shape->GetValueID() == CSSValueID::kEllipse && horizontal_size &&
      !vertical_size) {
    return nullptr;
  }
  // If there's only one size, it must be a length.
  if (!vertical_size && horizontal_size && horizontal_size->IsPercentage())
    return nullptr;
  if ((horizontal_size &&
       horizontal_size->IsCalculatedPercentageWithLength()) ||
      (vertical_size && vertical_size->IsCalculatedPercentageWithLength())) {
    return nullptr;
  }

  CSSValue* center_x = nullptr;
  CSSValue* center_y = nullptr;
  if (args.Peek().Id() == CSSValueID::kAt) {
    args.ConsumeIncludingWhitespace();
    ConsumePosition(args, context, UnitlessQuirk::kForbid,
                    base::Optional<WebFeature>(), center_x, center_y);
    if (!(center_x && center_y))
      return nullptr;
    // Right now, CSS radial gradients have the same start and end centers.
  }

  if ((shape || size_keyword || horizontal_size || center_x || center_y) &&
      !ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  cssvalue::CSSGradientValue* result = cssvalue::CSSRadialGradientValue::Create(
      center_x, center_y, shape, size_keyword, horizontal_size, vertical_size,
      repeating, cssvalue::kCSSRadialGradient);
  return ConsumeGradientColorStops(args, context, result,
                                   ConsumeGradientLengthOrPercent)
             ? result
             : nullptr;
}

static CSSValue* ConsumeLinearGradient(
    CSSParserTokenRange& args,
    const CSSParserContext& context,
    cssvalue::CSSGradientRepeat repeating,
    cssvalue::CSSGradientType gradient_type) {
  bool expect_comma = true;
  const CSSPrimitiveValue* angle =
      ConsumeAngle(args, &context, WebFeature::kUnitlessZeroAngleGradient);
  const CSSIdentifierValue* end_x = nullptr;
  const CSSIdentifierValue* end_y = nullptr;
  if (!angle) {
    if (gradient_type == cssvalue::kCSSPrefixedLinearGradient ||
        ConsumeIdent<CSSValueID::kTo>(args)) {
      end_x = ConsumeIdent<CSSValueID::kLeft, CSSValueID::kRight>(args);
      end_y = ConsumeIdent<CSSValueID::kBottom, CSSValueID::kTop>(args);
      if (!end_x && !end_y) {
        if (gradient_type == cssvalue::kCSSLinearGradient)
          return nullptr;
        end_y = CSSIdentifierValue::Create(CSSValueID::kTop);
        expect_comma = false;
      } else if (!end_x) {
        end_x = ConsumeIdent<CSSValueID::kLeft, CSSValueID::kRight>(args);
      }
    } else {
      expect_comma = false;
    }
  }

  if (expect_comma && !ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  cssvalue::CSSGradientValue* result =
      MakeGarbageCollected<cssvalue::CSSLinearGradientValue>(
          end_x, end_y, nullptr, nullptr, angle, repeating, gradient_type);
  return ConsumeGradientColorStops(args, context, result,
                                   ConsumeGradientLengthOrPercent)
             ? result
             : nullptr;
}

static CSSValue* ConsumeConicGradient(CSSParserTokenRange& args,
                                      const CSSParserContext& context,
                                      cssvalue::CSSGradientRepeat repeating) {
  const CSSPrimitiveValue* from_angle = nullptr;
  if (ConsumeIdent<CSSValueID::kFrom>(args)) {
    if (!(from_angle = ConsumeAngle(args, &context,
                                    WebFeature::kUnitlessZeroAngleGradient)))
      return nullptr;
  }

  CSSValue* center_x = nullptr;
  CSSValue* center_y = nullptr;
  if (ConsumeIdent<CSSValueID::kAt>(args)) {
    if (!ConsumePosition(args, context, UnitlessQuirk::kForbid,
                         base::Optional<WebFeature>(), center_x, center_y))
      return nullptr;
  }

  // Comma separator required when fromAngle or position is present.
  if ((from_angle || center_x || center_y) &&
      !ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  cssvalue::CSSGradientValue* result = cssvalue::CSSConicGradientValue::Create(
      center_x, center_y, from_angle, repeating);
  return ConsumeGradientColorStops(args, context, result,
                                   ConsumeGradientAngleOrPercent)
             ? result
             : nullptr;
}

CSSValue* ConsumeImageOrNone(CSSParserTokenRange& range,
                             const CSSParserContext* context) {
  if (range.Peek().Id() == CSSValueID::kNone)
    return ConsumeIdent(range);
  return ConsumeImage(range, context);
}

CSSValue* ConsumeAxis(CSSParserTokenRange& range) {
  CSSValueID axis_id = range.Peek().Id();
  if (axis_id == CSSValueID::kX || axis_id == CSSValueID::kY ||
      axis_id == CSSValueID::kZ) {
    ConsumeIdent(range);
    return MakeGarbageCollected<cssvalue::CSSAxisValue>(axis_id);
  }

  CSSValue* x_dimension =
      css_property_parser_helpers::ConsumeNumber(range, kValueRangeAll);
  CSSValue* y_dimension =
      css_property_parser_helpers::ConsumeNumber(range, kValueRangeAll);
  CSSValue* z_dimension =
      css_property_parser_helpers::ConsumeNumber(range, kValueRangeAll);
  if (!x_dimension || !y_dimension || !z_dimension)
    return nullptr;
  double x = To<CSSPrimitiveValue>(x_dimension)->GetDoubleValue();
  double y = To<CSSPrimitiveValue>(y_dimension)->GetDoubleValue();
  double z = To<CSSPrimitiveValue>(z_dimension)->GetDoubleValue();
  return MakeGarbageCollected<cssvalue::CSSAxisValue>(x, y, z);
}

static CSSValue* ConsumeCrossFade(CSSParserTokenRange& args,
                                  const CSSParserContext* context) {
  CSSValue* from_image_value = ConsumeImageOrNone(args, context);
  if (!from_image_value || !ConsumeCommaIncludingWhitespace(args))
    return nullptr;
  CSSValue* to_image_value = ConsumeImageOrNone(args, context);
  if (!to_image_value || !ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  CSSPrimitiveValue* percentage = nullptr;
  if (CSSPrimitiveValue* percent_value = ConsumePercent(args, kValueRangeAll))
    percentage = CSSNumericLiteralValue::Create(
        clampTo<double>(percent_value->GetDoubleValue() / 100.0, 0, 1),
        CSSPrimitiveValue::UnitType::kNumber);
  else if (CSSPrimitiveValue* number_value =
               ConsumeNumber(args, kValueRangeAll))
    percentage = CSSNumericLiteralValue::Create(
        clampTo<double>(number_value->GetDoubleValue(), 0, 1),
        CSSPrimitiveValue::UnitType::kNumber);

  if (!percentage)
    return nullptr;
  return MakeGarbageCollected<cssvalue::CSSCrossfadeValue>(
      from_image_value, to_image_value, percentage);
}

static CSSValue* ConsumePaint(CSSParserTokenRange& args,
                              const CSSParserContext* context) {
  const CSSParserToken& name_token = args.ConsumeIncludingWhitespace();
  CSSCustomIdentValue* name = ConsumeCustomIdentWithToken(name_token, *context);
  if (!name)
    return nullptr;

  if (args.AtEnd())
    return MakeGarbageCollected<CSSPaintValue>(name);

  if (!RuntimeEnabledFeatures::CSSPaintAPIArgumentsEnabled()) {
    // Arguments not enabled, but exists. Invalid.
    return nullptr;
  }

  // Begin parse paint arguments.
  if (!ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  // Consume arguments.
  // TODO(renjieliu): We may want to optimize the implementation by resolve
  // variables early if paint function is registered.
  Vector<CSSParserToken> argument_tokens;
  Vector<scoped_refptr<CSSVariableData>> variable_data;
  while (!args.AtEnd()) {
    if (args.Peek().GetType() != kCommaToken) {
      argument_tokens.AppendVector(ConsumeFunctionArgsOrNot(args));
    } else {
      if (!AddCSSPaintArgument(argument_tokens, &variable_data, context))
        return nullptr;
      argument_tokens.clear();
      if (!ConsumeCommaIncludingWhitespace(args))
        return nullptr;
    }
  }
  if (!AddCSSPaintArgument(argument_tokens, &variable_data, context))
    return nullptr;

  return MakeGarbageCollected<CSSPaintValue>(name, variable_data);
}

static CSSValue* ConsumeGeneratedImage(CSSParserTokenRange& range,
                                       const CSSParserContext* context) {
  CSSValueID id = range.Peek().FunctionId();
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  CSSValue* result = nullptr;
  if (id == CSSValueID::kRadialGradient) {
    result = ConsumeRadialGradient(args, *context, cssvalue::kNonRepeating);
  } else if (id == CSSValueID::kRepeatingRadialGradient) {
    result = ConsumeRadialGradient(args, *context, cssvalue::kRepeating);
  } else if (id == CSSValueID::kWebkitLinearGradient) {
    context->Count(WebFeature::kDeprecatedWebKitLinearGradient);
    result = ConsumeLinearGradient(args, *context, cssvalue::kNonRepeating,
                                   cssvalue::kCSSPrefixedLinearGradient);
  } else if (id == CSSValueID::kWebkitRepeatingLinearGradient) {
    context->Count(WebFeature::kDeprecatedWebKitRepeatingLinearGradient);
    result = ConsumeLinearGradient(args, *context, cssvalue::kRepeating,
                                   cssvalue::kCSSPrefixedLinearGradient);
  } else if (id == CSSValueID::kRepeatingLinearGradient) {
    result = ConsumeLinearGradient(args, *context, cssvalue::kRepeating,
                                   cssvalue::kCSSLinearGradient);
  } else if (id == CSSValueID::kLinearGradient) {
    result = ConsumeLinearGradient(args, *context, cssvalue::kNonRepeating,
                                   cssvalue::kCSSLinearGradient);
  } else if (id == CSSValueID::kWebkitGradient) {
    context->Count(WebFeature::kDeprecatedWebKitGradient);
    result = ConsumeDeprecatedGradient(args, context->Mode());
  } else if (id == CSSValueID::kWebkitRadialGradient) {
    context->Count(WebFeature::kDeprecatedWebKitRadialGradient);
    result = ConsumeDeprecatedRadialGradient(args, *context,
                                             cssvalue::kNonRepeating);
  } else if (id == CSSValueID::kWebkitRepeatingRadialGradient) {
    context->Count(WebFeature::kDeprecatedWebKitRepeatingRadialGradient);
    result =
        ConsumeDeprecatedRadialGradient(args, *context, cssvalue::kRepeating);
  } else if (id == CSSValueID::kConicGradient) {
    result = ConsumeConicGradient(args, *context, cssvalue::kNonRepeating);
  } else if (id == CSSValueID::kRepeatingConicGradient) {
    result = ConsumeConicGradient(args, *context, cssvalue::kRepeating);
  } else if (id == CSSValueID::kWebkitCrossFade) {
    result = ConsumeCrossFade(args, context);
  } else if (id == CSSValueID::kPaint) {
    result = context->IsSecureContext() ? ConsumePaint(args, context) : nullptr;
  }
  if (!result || !args.AtEnd())
    return nullptr;

  WebFeature feature;
  if (id == CSSValueID::kWebkitCrossFade)
    feature = WebFeature::kWebkitCrossFade;
  else if (id == CSSValueID::kPaint)
    feature = WebFeature::kCSSPaintFunction;
  else
    feature = WebFeature::kCSSGradient;
  context->Count(feature);

  range = range_copy;
  return result;
}

static CSSValue* CreateCSSImageValueWithReferrer(
    const AtomicString& raw_value,
    const CSSParserContext* context) {
  CSSValue* image_value = CSSImageValue::Create(
      raw_value, context->CompleteURL(raw_value), context->GetReferrer(),
      context->IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse);
  return image_value;
}

static CSSValue* ConsumeImageSet(CSSParserTokenRange& range,
                                 const CSSParserContext* context) {
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  auto* image_set = MakeGarbageCollected<CSSImageSetValue>(context->Mode());
  do {
    AtomicString url_value =
        ConsumeUrlAsStringView(args, context).ToAtomicString();
    if (url_value.IsNull())
      return nullptr;

    CSSValue* image = CreateCSSImageValueWithReferrer(url_value, context);
    image_set->Append(*image);

    const CSSParserToken& token = args.ConsumeIncludingWhitespace();
    if (token.GetType() != kDimensionToken)
      return nullptr;
    if (token.Value() != "x")
      return nullptr;
    DCHECK(token.GetUnitType() == CSSPrimitiveValue::UnitType::kDotsPerPixel);
    double image_scale_factor = token.NumericValue();
    if (image_scale_factor <= 0)
      return nullptr;
    image_set->Append(*CSSNumericLiteralValue::Create(
        image_scale_factor, CSSPrimitiveValue::UnitType::kNumber));
  } while (ConsumeCommaIncludingWhitespace(args));
  if (!args.AtEnd())
    return nullptr;
  range = range_copy;
  return image_set;
}

static bool IsGeneratedImage(CSSValueID id) {
  return id == CSSValueID::kLinearGradient ||
         id == CSSValueID::kRadialGradient ||
         id == CSSValueID::kConicGradient ||
         id == CSSValueID::kRepeatingLinearGradient ||
         id == CSSValueID::kRepeatingRadialGradient ||
         id == CSSValueID::kRepeatingConicGradient ||
         id == CSSValueID::kWebkitLinearGradient ||
         id == CSSValueID::kWebkitRadialGradient ||
         id == CSSValueID::kWebkitRepeatingLinearGradient ||
         id == CSSValueID::kWebkitRepeatingRadialGradient ||
         id == CSSValueID::kWebkitGradient ||
         id == CSSValueID::kWebkitCrossFade || id == CSSValueID::kPaint;
}

CSSValue* ConsumeImage(CSSParserTokenRange& range,
                       const CSSParserContext* context,
                       ConsumeGeneratedImagePolicy generated_image) {
  AtomicString uri = ConsumeUrlAsStringView(range, context).ToAtomicString();
  if (!uri.IsNull())
    return CreateCSSImageValueWithReferrer(uri, context);
  if (range.Peek().GetType() == kFunctionToken) {
    CSSValueID id = range.Peek().FunctionId();
    if (id == CSSValueID::kWebkitImageSet)
      return ConsumeImageSet(range, context);
    if (generated_image == ConsumeGeneratedImagePolicy::kAllow &&
        IsGeneratedImage(id)) {
      return ConsumeGeneratedImage(range, context);
    }
  }
  return nullptr;
}

// https://drafts.csswg.org/css-values-4/#css-wide-keywords
bool IsCSSWideKeyword(StringView keyword) {
  return EqualIgnoringASCIICase(keyword, "initial") ||
         EqualIgnoringASCIICase(keyword, "inherit") ||
         EqualIgnoringASCIICase(keyword, "unset");
}

// https://drafts.csswg.org/css-cascade/#default
bool IsRevertKeyword(StringView keyword) {
  return EqualIgnoringASCIICase(keyword, "revert");
}

// https://drafts.csswg.org/css-values-4/#identifier-value
bool IsDefaultKeyword(StringView keyword) {
  return EqualIgnoringASCIICase(keyword, "default");
}

// https://drafts.csswg.org/css-shapes-1/#typedef-shape-box
CSSIdentifierValue* ConsumeShapeBox(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kContentBox, CSSValueID::kPaddingBox,
                      CSSValueID::kBorderBox, CSSValueID::kMarginBox>(range);
}

void AddProperty(CSSPropertyID resolved_property,
                 CSSPropertyID current_shorthand,
                 const CSSValue& value,
                 bool important,
                 IsImplicitProperty implicit,
                 HeapVector<CSSPropertyValue, 256>& properties) {
  DCHECK(!isPropertyAlias(resolved_property));
  DCHECK(implicit == IsImplicitProperty::kNotImplicit ||
         implicit == IsImplicitProperty::kImplicit);

  int shorthand_index = 0;
  bool set_from_shorthand = false;

  if (isValidCSSPropertyID(current_shorthand)) {
    Vector<StylePropertyShorthand, 4> shorthands;
    getMatchingShorthandsForLonghand(resolved_property, &shorthands);
    set_from_shorthand = true;
    if (shorthands.size() > 1) {
      shorthand_index =
          indexOfShorthandForLonghand(current_shorthand, shorthands);
    }
  }

  properties.push_back(CSSPropertyValue(
      CSSProperty::Get(resolved_property), value, important, set_from_shorthand,
      shorthand_index, implicit == IsImplicitProperty::kImplicit));
}

CSSValue* ConsumeTransformValue(CSSParserTokenRange& range,
                                const CSSParserContext& context) {
  bool use_legacy_parsing = false;
  return css_parsing_utils::ConsumeTransformValue(range, context,
                                                  use_legacy_parsing);
}

CSSValue* ConsumeTransformList(CSSParserTokenRange& range,
                               const CSSParserContext& context) {
  return css_parsing_utils::ConsumeTransformList(range, context,
                                                 CSSParserLocalContext());
}

CSSValue* ConsumeFilterFunctionList(CSSParserTokenRange& range,
                                    const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNone)
    return ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSValue* filter_value = ConsumeUrl(range, &context);
    if (!filter_value) {
      filter_value = ConsumeFilterFunction(range, context);
      if (!filter_value)
        return nullptr;
    }
    list->Append(*filter_value);
  } while (!range.AtEnd());
  return list;
}

void CountKeywordOnlyPropertyUsage(CSSPropertyID property,
                                   const CSSParserContext& context,
                                   CSSValueID value_id) {
  if (!context.IsUseCounterRecordingEnabled())
    return;
  switch (property) {
    case CSSPropertyID::kWebkitAppearance: {
      WebFeature feature;
      if (value_id == CSSValueID::kNone) {
        feature = WebFeature::kCSSValueAppearanceNone;
      } else {
        feature = WebFeature::kCSSValueAppearanceNotNone;
        if (value_id == CSSValueID::kButton)
          feature = WebFeature::kCSSValueAppearanceButton;
        else if (value_id == CSSValueID::kCheckbox)
          feature = WebFeature::kCSSValueAppearanceCheckbox;
        else if (value_id == CSSValueID::kInnerSpinButton)
          feature = WebFeature::kCSSValueAppearanceInnerSpinButton;
        else if (value_id == CSSValueID::kMenulist)
          feature = WebFeature::kCSSValueAppearanceMenulist;
        else if (value_id == CSSValueID::kMenulistButton)
          feature = WebFeature::kCSSValueAppearanceMenulistButton;
        else if (value_id == CSSValueID::kMeter)
          feature = WebFeature::kCSSValueAppearanceMeter;
        else if (value_id == CSSValueID::kListbox)
          feature = WebFeature::kCSSValueAppearanceListbox;
        else if (value_id == CSSValueID::kProgressBar)
          feature = WebFeature::kCSSValueAppearanceProgressBar;
        else if (value_id == CSSValueID::kPushButton)
          feature = WebFeature::kCSSValueAppearancePushButton;
        else if (value_id == CSSValueID::kRadio)
          feature = WebFeature::kCSSValueAppearanceRadio;
        else if (value_id == CSSValueID::kSearchfieldCancelButton)
          feature = WebFeature::kCSSValueAppearanceSearchCancel;
        else if (value_id == CSSValueID::kSquareButton)
          feature = WebFeature::kCSSValueAppearanceSquareButton;
        else if (value_id == CSSValueID::kSearchfield)
          feature = WebFeature::kCSSValueAppearanceSearchField;
        else if (value_id == CSSValueID::kTextarea)
          feature = WebFeature::kCSSValueAppearanceTextarea;
        else if (value_id == CSSValueID::kTextfield)
          feature = WebFeature::kCSSValueAppearanceTextField;
        else
          feature = WebFeature::kCSSValueAppearanceOthers;
      }
      context.Count(feature);
      break;
    }

    case CSSPropertyID::kWebkitUserModify: {
      switch (value_id) {
        case CSSValueID::kReadOnly:
          context.Count(WebFeature::kCSSValueUserModifyReadOnly);
          break;
        case CSSValueID::kReadWrite:
          context.Count(WebFeature::kCSSValueUserModifyReadWrite);
          break;
        case CSSValueID::kReadWritePlaintextOnly:
          context.Count(WebFeature::kCSSValueUserModifyReadWritePlaintextOnly);
          break;
        default:
          NOTREACHED();
      }
      break;
    }
    case CSSPropertyID::kDisplay:
      if (value_id == CSSValueID::kContents)
        context.Count(WebFeature::kCSSValueDisplayContents);
      break;
    case CSSPropertyID::kOverflowX:
    case CSSPropertyID::kOverflowY:
      if (value_id == CSSValueID::kOverlay)
        context.Count(WebFeature::kCSSValueOverflowOverlay);
      break;
    default:
      break;
  }
}

const CSSValue* ParseLonghand(CSSPropertyID unresolved_property,
                              CSSPropertyID current_shorthand,
                              const CSSParserContext& context,
                              CSSParserTokenRange& range) {
  CSSPropertyID property_id = resolveCSSPropertyID(unresolved_property);
  DCHECK(!CSSProperty::Get(property_id).IsShorthand());
  if (CSSParserFastPaths::IsKeywordPropertyID(property_id)) {
    if (CSSParserFastPaths::IsValidKeywordPropertyAndValue(
            property_id, range.Peek().Id(), context.Mode())) {
      CountKeywordOnlyPropertyUsage(property_id, context, range.Peek().Id());
      return ConsumeIdent(range);
    }

    // Some properties need to fallback onto the regular parser.
    if (!CSSParserFastPaths::IsPartialKeywordPropertyID(property_id))
      return nullptr;
  }

  const auto local_context =
      CSSParserLocalContext()
          .WithAliasParsing(isPropertyAlias(unresolved_property))
          .WithCurrentShorthand(current_shorthand);

  const CSSValue* result = To<Longhand>(CSSProperty::Get(property_id))
                               .ParseSingleValue(range, context, local_context);
  return result;
}

bool ConsumeShorthandVia2Longhands(
    const StylePropertyShorthand& shorthand,
    bool important,
    const CSSParserContext& context,
    CSSParserTokenRange& range,
    HeapVector<CSSPropertyValue, 256>& properties) {
  DCHECK_EQ(shorthand.length(), 2u);
  const CSSProperty** longhands = shorthand.properties();

  const CSSValue* start =
      ParseLonghand(longhands[0]->PropertyID(), shorthand.id(), context, range);

  if (!start)
    return false;

  const CSSValue* end =
      ParseLonghand(longhands[1]->PropertyID(), shorthand.id(), context, range);

  if (shorthand.id() == CSSPropertyID::kOverflow && start && end) {
    context.Count(WebFeature::kTwoValuedOverflow);
  }

  if (!end)
    end = start;
  AddProperty(longhands[0]->PropertyID(), shorthand.id(), *start, important,
              IsImplicitProperty::kNotImplicit, properties);
  AddProperty(longhands[1]->PropertyID(), shorthand.id(), *end, important,
              IsImplicitProperty::kNotImplicit, properties);

  return range.AtEnd();
}

bool ConsumeShorthandVia4Longhands(
    const StylePropertyShorthand& shorthand,
    bool important,
    const CSSParserContext& context,
    CSSParserTokenRange& range,
    HeapVector<CSSPropertyValue, 256>& properties) {
  DCHECK_EQ(shorthand.length(), 4u);
  const CSSProperty** longhands = shorthand.properties();
  const CSSValue* top =
      ParseLonghand(longhands[0]->PropertyID(), shorthand.id(), context, range);

  if (!top)
    return false;

  const CSSValue* right =
      ParseLonghand(longhands[1]->PropertyID(), shorthand.id(), context, range);

  const CSSValue* bottom = nullptr;
  const CSSValue* left = nullptr;
  if (right) {
    bottom = ParseLonghand(longhands[2]->PropertyID(), shorthand.id(), context,
                           range);
    if (bottom) {
      left = ParseLonghand(longhands[3]->PropertyID(), shorthand.id(), context,
                           range);
    }
  }

  if (!right)
    right = top;
  if (!bottom)
    bottom = top;
  if (!left)
    left = right;

  AddProperty(longhands[0]->PropertyID(), shorthand.id(), *top, important,
              IsImplicitProperty::kNotImplicit, properties);
  AddProperty(longhands[1]->PropertyID(), shorthand.id(), *right, important,
              IsImplicitProperty::kNotImplicit, properties);
  AddProperty(longhands[2]->PropertyID(), shorthand.id(), *bottom, important,
              IsImplicitProperty::kNotImplicit, properties);
  AddProperty(longhands[3]->PropertyID(), shorthand.id(), *left, important,
              IsImplicitProperty::kNotImplicit, properties);

  return range.AtEnd();
}

bool ConsumeShorthandGreedilyViaLonghands(
    const StylePropertyShorthand& shorthand,
    bool important,
    const CSSParserContext& context,
    CSSParserTokenRange& range,
    HeapVector<CSSPropertyValue, 256>& properties) {
  // Existing shorthands have at most 6 longhands.
  DCHECK_LE(shorthand.length(), 6u);
  const CSSValue* longhands[6] = {nullptr, nullptr, nullptr,
                                  nullptr, nullptr, nullptr};
  const CSSProperty** shorthand_properties = shorthand.properties();
  do {
    bool found_longhand = false;
    for (size_t i = 0; !found_longhand && i < shorthand.length(); ++i) {
      if (longhands[i])
        continue;
      longhands[i] = ParseLonghand(shorthand_properties[i]->PropertyID(),
                                   shorthand.id(), context, range);

      if (longhands[i])
        found_longhand = true;
    }
    if (!found_longhand)
      return false;
  } while (!range.AtEnd());

  for (size_t i = 0; i < shorthand.length(); ++i) {
    if (longhands[i]) {
      AddProperty(shorthand_properties[i]->PropertyID(), shorthand.id(),
                  *longhands[i], important, IsImplicitProperty::kNotImplicit,
                  properties);
    } else {
      AddProperty(shorthand_properties[i]->PropertyID(), shorthand.id(),
                  *CSSInitialValue::Create(), important,
                  IsImplicitProperty::kNotImplicit, properties);
    }
  }
  return true;
}

void AddExpandedPropertyForValue(
    CSSPropertyID property,
    const CSSValue& value,
    bool important,
    HeapVector<CSSPropertyValue, 256>& properties) {
  const StylePropertyShorthand& shorthand = shorthandForProperty(property);
  unsigned shorthand_length = shorthand.length();
  DCHECK(shorthand_length);
  const CSSProperty** longhands = shorthand.properties();
  for (unsigned i = 0; i < shorthand_length; ++i) {
    AddProperty(longhands[i]->PropertyID(), property, value, important,
                IsImplicitProperty::kNotImplicit, properties);
  }
}

}  // namespace css_property_parser_helpers

}  // namespace blink
