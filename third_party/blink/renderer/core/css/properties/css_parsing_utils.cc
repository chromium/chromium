// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"
#include "third_party/blink/renderer/core/css/css_border_image.h"
#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_feature_value.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_line_names_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_ray_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/css_timing_function_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_parsing_error.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using cssvalue::CSSFontFeatureValue;
using cssvalue::CSSGridLineNamesValue;

namespace css_parsing_utils {
namespace {

bool IsLeftOrRightKeyword(CSSValueID id) {
  return css_property_parser_helpers::IdentMatches<CSSValueID::kLeft,
                                                   CSSValueID::kRight>(id);
}

bool IsAuto(CSSValueID id) {
  return css_property_parser_helpers::IdentMatches<CSSValueID::kAuto>(id);
}

bool IsNormalOrStretch(CSSValueID id) {
  return css_property_parser_helpers::IdentMatches<CSSValueID::kNormal,
                                                   CSSValueID::kStretch>(id);
}

bool IsContentDistributionKeyword(CSSValueID id) {
  return css_property_parser_helpers::IdentMatches<
      CSSValueID::kSpaceBetween, CSSValueID::kSpaceAround,
      CSSValueID::kSpaceEvenly, CSSValueID::kStretch>(id);
}

bool IsOverflowKeyword(CSSValueID id) {
  return css_property_parser_helpers::IdentMatches<CSSValueID::kUnsafe,
                                                   CSSValueID::kSafe>(id);
}

CSSIdentifierValue* ConsumeOverflowPositionKeyword(CSSParserTokenRange& range) {
  return IsOverflowKeyword(range.Peek().Id())
             ? css_property_parser_helpers::ConsumeIdent(range)
             : nullptr;
}

CSSValueID GetBaselineKeyword(CSSValue& value) {
  auto* value_pair = DynamicTo<CSSValuePair>(value);
  if (!value_pair) {
    DCHECK(To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kBaseline);
    return CSSValueID::kBaseline;
  }

  DCHECK(To<CSSIdentifierValue>(value_pair->First()).GetValueID() ==
         CSSValueID::kLast);
  DCHECK(To<CSSIdentifierValue>(value_pair->Second()).GetValueID() ==
         CSSValueID::kBaseline);
  return CSSValueID::kLastBaseline;
}

CSSValue* ConsumeBaselineKeyword(CSSParserTokenRange& range) {
  CSSIdentifierValue* preference =
      css_property_parser_helpers::ConsumeIdent<CSSValueID::kFirst,
                                                CSSValueID::kLast>(range);
  CSSIdentifierValue* baseline =
      css_property_parser_helpers::ConsumeIdent<CSSValueID::kBaseline>(range);
  if (!baseline)
    return nullptr;
  if (preference && preference->GetValueID() == CSSValueID::kLast) {
    return MakeGarbageCollected<CSSValuePair>(
        preference, baseline, CSSValuePair::kDropIdenticalValues);
  }
  return baseline;
}

CSSValue* ConsumeSteps(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueID::kSteps);
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      css_property_parser_helpers::ConsumeFunction(range_copy);

  CSSPrimitiveValue* steps =
      css_property_parser_helpers::ConsumePositiveInteger(args);
  if (!steps)
    return nullptr;

  StepsTimingFunction::StepPosition position =
      StepsTimingFunction::StepPosition::END;
  if (css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args)) {
    switch (args.ConsumeIncludingWhitespace().Id()) {
      case CSSValueID::kStart:
        position = StepsTimingFunction::StepPosition::START;
        break;

      case CSSValueID::kEnd:
        position = StepsTimingFunction::StepPosition::END;
        break;

      case CSSValueID::kJumpBoth:
        position = StepsTimingFunction::StepPosition::JUMP_BOTH;
        break;

      case CSSValueID::kJumpEnd:
        position = StepsTimingFunction::StepPosition::JUMP_END;
        break;

      case CSSValueID::kJumpNone:
        position = StepsTimingFunction::StepPosition::JUMP_NONE;
        break;

      case CSSValueID::kJumpStart:
        position = StepsTimingFunction::StepPosition::JUMP_START;
        break;

      default:
        return nullptr;
    }
  }

  if (!args.AtEnd())
    return nullptr;

  // Steps(n, jump-none) requires n >= 2.
  if (position == StepsTimingFunction::StepPosition::JUMP_NONE &&
      steps->GetIntValue() < 2) {
    return nullptr;
  }

  range = range_copy;
  return cssvalue::CSSStepsTimingFunctionValue::Create(steps->GetIntValue(),
                                                       position);
}

CSSValue* ConsumeCubicBezier(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueID::kCubicBezier);
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      css_property_parser_helpers::ConsumeFunction(range_copy);

  double x1, y1, x2, y2;
  if (css_property_parser_helpers::ConsumeNumberRaw(args, x1) && x1 >= 0 &&
      x1 <= 1 &&
      css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args) &&
      css_property_parser_helpers::ConsumeNumberRaw(args, y1) &&
      css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args) &&
      css_property_parser_helpers::ConsumeNumberRaw(args, x2) && x2 >= 0 &&
      x2 <= 1 &&
      css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args) &&
      css_property_parser_helpers::ConsumeNumberRaw(args, y2) && args.AtEnd()) {
    range = range_copy;
    return MakeGarbageCollected<cssvalue::CSSCubicBezierTimingFunctionValue>(
        x1, y1, x2, y2);
  }

  return nullptr;
}

CSSIdentifierValue* ConsumeBorderImageRepeatKeyword(
    CSSParserTokenRange& range) {
  return css_property_parser_helpers::ConsumeIdent<
      CSSValueID::kStretch, CSSValueID::kRepeat, CSSValueID::kSpace,
      CSSValueID::kRound>(range);
}

bool ConsumeCSSValueId(CSSParserTokenRange& range, CSSValueID& value) {
  CSSIdentifierValue* keyword =
      css_property_parser_helpers::ConsumeIdent(range);
  if (!keyword || !range.AtEnd())
    return false;
  value = keyword->GetValueID();
  return true;
}

CSSValue* ConsumeShapeRadius(CSSParserTokenRange& args,
                             CSSParserMode css_parser_mode) {
  if (css_property_parser_helpers::IdentMatches<CSSValueID::kClosestSide,
                                                CSSValueID::kFarthestSide>(
          args.Peek().Id()))
    return css_property_parser_helpers::ConsumeIdent(args);
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      args, css_parser_mode, kValueRangeNonNegative);
}

cssvalue::CSSBasicShapeCircleValue* ConsumeBasicShapeCircle(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
  // circle( [<shape-radius>]? [at <position>]? )
  auto* shape = MakeGarbageCollected<cssvalue::CSSBasicShapeCircleValue>();
  if (CSSValue* radius = ConsumeShapeRadius(args, context.Mode()))
    shape->SetRadius(radius);
  if (css_property_parser_helpers::ConsumeIdent<CSSValueID::kAt>(args)) {
    CSSValue* center_x = nullptr;
    CSSValue* center_y = nullptr;
    if (!ConsumePosition(args, context,
                         css_property_parser_helpers::UnitlessQuirk::kForbid,
                         base::Optional<WebFeature>(), center_x, center_y))
      return nullptr;
    shape->SetCenterX(center_x);
    shape->SetCenterY(center_y);
  }
  return shape;
}

cssvalue::CSSBasicShapeEllipseValue* ConsumeBasicShapeEllipse(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
  // ellipse( [<shape-radius>{2}]? [at <position>]? )
  auto* shape = MakeGarbageCollected<cssvalue::CSSBasicShapeEllipseValue>();
  WebFeature feature = WebFeature::kBasicShapeEllipseNoRadius;
  if (CSSValue* radius_x = ConsumeShapeRadius(args, context.Mode())) {
    CSSValue* radius_y = ConsumeShapeRadius(args, context.Mode());
    if (!radius_y) {
      return nullptr;
    }
    shape->SetRadiusX(radius_x);
    shape->SetRadiusY(radius_y);
    feature = WebFeature::kBasicShapeEllipseTwoRadius;
  }
  if (css_property_parser_helpers::ConsumeIdent<CSSValueID::kAt>(args)) {
    CSSValue* center_x = nullptr;
    CSSValue* center_y = nullptr;
    if (!ConsumePosition(args, context,
                         css_property_parser_helpers::UnitlessQuirk::kForbid,
                         base::Optional<WebFeature>(), center_x, center_y))
      return nullptr;
    shape->SetCenterX(center_x);
    shape->SetCenterY(center_y);
  }
  context.Count(feature);
  return shape;
}

cssvalue::CSSBasicShapePolygonValue* ConsumeBasicShapePolygon(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  auto* shape = MakeGarbageCollected<cssvalue::CSSBasicShapePolygonValue>();
  if (css_property_parser_helpers::IdentMatches<CSSValueID::kEvenodd,
                                                CSSValueID::kNonzero>(
          args.Peek().Id())) {
    shape->SetWindRule(args.ConsumeIncludingWhitespace().Id() ==
                               CSSValueID::kEvenodd
                           ? RULE_EVENODD
                           : RULE_NONZERO);
    if (!css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args))
      return nullptr;
  }

  do {
    CSSPrimitiveValue* x_length =
        css_property_parser_helpers::ConsumeLengthOrPercent(
            args, context.Mode(), kValueRangeAll);
    if (!x_length)
      return nullptr;
    CSSPrimitiveValue* y_length =
        css_property_parser_helpers::ConsumeLengthOrPercent(
            args, context.Mode(), kValueRangeAll);
    if (!y_length)
      return nullptr;
    shape->AppendPoint(x_length, y_length);
  } while (css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args));
  return shape;
}

cssvalue::CSSBasicShapeInsetValue* ConsumeBasicShapeInset(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  auto* shape = MakeGarbageCollected<cssvalue::CSSBasicShapeInsetValue>();
  CSSPrimitiveValue* top = css_property_parser_helpers::ConsumeLengthOrPercent(
      args, context.Mode(), kValueRangeAll);
  if (!top)
    return nullptr;
  CSSPrimitiveValue* right =
      css_property_parser_helpers::ConsumeLengthOrPercent(args, context.Mode(),
                                                          kValueRangeAll);
  CSSPrimitiveValue* bottom = nullptr;
  CSSPrimitiveValue* left = nullptr;
  if (right) {
    bottom = css_property_parser_helpers::ConsumeLengthOrPercent(
        args, context.Mode(), kValueRangeAll);
    if (bottom) {
      left = css_property_parser_helpers::ConsumeLengthOrPercent(
          args, context.Mode(), kValueRangeAll);
    }
  }
  if (left)
    shape->UpdateShapeSize4Values(top, right, bottom, left);
  else if (bottom)
    shape->UpdateShapeSize3Values(top, right, bottom);
  else if (right)
    shape->UpdateShapeSize2Values(top, right);
  else
    shape->UpdateShapeSize1Value(top);

  if (css_property_parser_helpers::ConsumeIdent<CSSValueID::kRound>(args)) {
    CSSValue* horizontal_radii[4] = {nullptr};
    CSSValue* vertical_radii[4] = {nullptr};
    if (!ConsumeRadii(horizontal_radii, vertical_radii, args, context.Mode(),
                      false))
      return nullptr;
    shape->SetTopLeftRadius(MakeGarbageCollected<CSSValuePair>(
        horizontal_radii[0], vertical_radii[0],
        CSSValuePair::kDropIdenticalValues));
    shape->SetTopRightRadius(MakeGarbageCollected<CSSValuePair>(
        horizontal_radii[1], vertical_radii[1],
        CSSValuePair::kDropIdenticalValues));
    shape->SetBottomRightRadius(MakeGarbageCollected<CSSValuePair>(
        horizontal_radii[2], vertical_radii[2],
        CSSValuePair::kDropIdenticalValues));
    shape->SetBottomLeftRadius(MakeGarbageCollected<CSSValuePair>(
        horizontal_radii[3], vertical_radii[3],
        CSSValuePair::kDropIdenticalValues));
  }
  return shape;
}

bool ConsumeNumbers(CSSParserTokenRange& args,
                    CSSFunctionValue*& transform_value,
                    unsigned number_of_arguments) {
  do {
    CSSValue* parsed_value =
        css_property_parser_helpers::ConsumeNumber(args, kValueRangeAll);
    if (!parsed_value)
      return false;
    transform_value->Append(*parsed_value);
    if (--number_of_arguments &&
        !css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args)) {
      return false;
    }
  } while (number_of_arguments);
  return true;
}

bool ConsumePerspective(CSSParserTokenRange& args,
                        const CSSParserContext& context,
                        CSSFunctionValue*& transform_value,
                        bool use_legacy_parsing) {
  CSSPrimitiveValue* parsed_value = css_property_parser_helpers::ConsumeLength(
      args, context.Mode(), kValueRangeNonNegative);
  if (!parsed_value && use_legacy_parsing) {
    double perspective;
    if (!css_property_parser_helpers::ConsumeNumberRaw(args, perspective) ||
        perspective < 0) {
      return false;
    }
    context.Count(WebFeature::kUnitlessPerspectiveInTransformProperty);
    parsed_value = CSSNumericLiteralValue::Create(
        perspective, CSSPrimitiveValue::UnitType::kPixels);
  }
  if (!parsed_value)
    return false;
  transform_value->Append(*parsed_value);
  return true;
}

bool ConsumeTranslate3d(CSSParserTokenRange& args,
                        CSSParserMode css_parser_mode,
                        CSSFunctionValue*& transform_value) {
  unsigned number_of_arguments = 2;
  CSSValue* parsed_value = nullptr;
  do {
    parsed_value = css_property_parser_helpers::ConsumeLengthOrPercent(
        args, css_parser_mode, kValueRangeAll);
    if (!parsed_value)
      return false;
    transform_value->Append(*parsed_value);
    if (!css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args))
      return false;
  } while (--number_of_arguments);
  parsed_value = css_property_parser_helpers::ConsumeLength(
      args, css_parser_mode, kValueRangeAll);
  if (!parsed_value)
    return false;
  transform_value->Append(*parsed_value);
  return true;
}

}  // namespace

bool IsBaselineKeyword(CSSValueID id) {
  return css_property_parser_helpers::IdentMatches<
      CSSValueID::kFirst, CSSValueID::kLast, CSSValueID::kBaseline>(id);
}

bool IsSelfPositionKeyword(CSSValueID id) {
  return css_property_parser_helpers::IdentMatches<
      CSSValueID::kStart, CSSValueID::kEnd, CSSValueID::kCenter,
      CSSValueID::kSelfStart, CSSValueID::kSelfEnd, CSSValueID::kFlexStart,
      CSSValueID::kFlexEnd>(id);
}

bool IsSelfPositionOrLeftOrRightKeyword(CSSValueID id) {
  return IsSelfPositionKeyword(id) || IsLeftOrRightKeyword(id);
}

bool IsContentPositionKeyword(CSSValueID id) {
  return css_property_parser_helpers::IdentMatches<
      CSSValueID::kStart, CSSValueID::kEnd, CSSValueID::kCenter,
      CSSValueID::kFlexStart, CSSValueID::kFlexEnd>(id);
}

bool IsContentPositionOrLeftOrRightKeyword(CSSValueID id) {
  return IsContentPositionKeyword(id) || IsLeftOrRightKeyword(id);
}

CSSValue* ConsumeScrollOffset(CSSParserTokenRange& range) {
  range.ConsumeWhitespace();
  if (css_property_parser_helpers::IdentMatches<CSSValueID::kAuto>(
          range.Peek().Id()))
    return css_property_parser_helpers::ConsumeIdent(range);
  CSSValue* value = css_property_parser_helpers::ConsumeLengthOrPercent(
      range, kHTMLStandardMode, kValueRangeNonNegative);
  if (!range.AtEnd())
    return nullptr;
  return value;
}

CSSValue* ConsumeSelfPositionOverflowPosition(
    CSSParserTokenRange& range,
    IsPositionKeyword is_position_keyword) {
  DCHECK(is_position_keyword);
  CSSValueID id = range.Peek().Id();
  if (IsAuto(id) || IsNormalOrStretch(id))
    return css_property_parser_helpers::ConsumeIdent(range);

  if (IsBaselineKeyword(id))
    return ConsumeBaselineKeyword(range);

  CSSIdentifierValue* overflow_position = ConsumeOverflowPositionKeyword(range);
  if (!is_position_keyword(range.Peek().Id()))
    return nullptr;
  CSSIdentifierValue* self_position =
      css_property_parser_helpers::ConsumeIdent(range);
  if (overflow_position) {
    return MakeGarbageCollected<CSSValuePair>(
        overflow_position, self_position, CSSValuePair::kDropIdenticalValues);
  }
  return self_position;
}

CSSValue* ConsumeContentDistributionOverflowPosition(
    CSSParserTokenRange& range,
    IsPositionKeyword is_position_keyword) {
  DCHECK(is_position_keyword);
  CSSValueID id = range.Peek().Id();
  if (css_property_parser_helpers::IdentMatches<CSSValueID::kNormal>(id)) {
    return MakeGarbageCollected<cssvalue::CSSContentDistributionValue>(
        CSSValueID::kInvalid, range.ConsumeIncludingWhitespace().Id(),
        CSSValueID::kInvalid);
  }

  if (IsBaselineKeyword(id)) {
    CSSValue* baseline = ConsumeBaselineKeyword(range);
    if (!baseline)
      return nullptr;
    return MakeGarbageCollected<cssvalue::CSSContentDistributionValue>(
        CSSValueID::kInvalid, GetBaselineKeyword(*baseline),
        CSSValueID::kInvalid);
  }

  if (IsContentDistributionKeyword(id)) {
    return MakeGarbageCollected<cssvalue::CSSContentDistributionValue>(
        range.ConsumeIncludingWhitespace().Id(), CSSValueID::kInvalid,
        CSSValueID::kInvalid);
  }

  CSSValueID overflow = IsOverflowKeyword(id)
                            ? range.ConsumeIncludingWhitespace().Id()
                            : CSSValueID::kInvalid;
  if (is_position_keyword(range.Peek().Id())) {
    return MakeGarbageCollected<cssvalue::CSSContentDistributionValue>(
        CSSValueID::kInvalid, range.ConsumeIncludingWhitespace().Id(),
        overflow);
  }

  return nullptr;
}

CSSValue* ConsumeAnimationIterationCount(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueID::kInfinite)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeNumber(range,
                                                    kValueRangeNonNegative);
}

CSSValue* ConsumeAnimationName(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               bool allow_quoted_name) {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_property_parser_helpers::ConsumeIdent(range);

  if (allow_quoted_name && range.Peek().GetType() == kStringToken) {
    // Legacy support for strings in prefixed animations.
    context.Count(WebFeature::kQuotedAnimationName);

    const CSSParserToken& token = range.ConsumeIncludingWhitespace();
    if (EqualIgnoringASCIICase(token.Value(), "none"))
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    return MakeGarbageCollected<CSSCustomIdentValue>(
        token.Value().ToAtomicString());
  }

  return css_property_parser_helpers::ConsumeCustomIdent(range, context);
}

CSSValue* ConsumeAnimationTimingFunction(CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kEase || id == CSSValueID::kLinear ||
      id == CSSValueID::kEaseIn || id == CSSValueID::kEaseOut ||
      id == CSSValueID::kEaseInOut || id == CSSValueID::kStepStart ||
      id == CSSValueID::kStepEnd)
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSValueID function = range.Peek().FunctionId();
  if (function == CSSValueID::kSteps)
    return ConsumeSteps(range);
  if (function == CSSValueID::kCubicBezier)
    return ConsumeCubicBezier(range);
  return nullptr;
}

bool ConsumeAnimationShorthand(
    const StylePropertyShorthand& shorthand,
    HeapVector<Member<CSSValueList>, kMaxNumAnimationLonghands>& longhands,
    ConsumeAnimationItemValue consumeLonghandItem,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    bool use_legacy_parsing) {
  DCHECK(consumeLonghandItem);
  const unsigned longhand_count = shorthand.length();
  DCHECK_LE(longhand_count, kMaxNumAnimationLonghands);

  for (unsigned i = 0; i < longhand_count; ++i)
    longhands[i] = CSSValueList::CreateCommaSeparated();

  do {
    bool parsed_longhand[kMaxNumAnimationLonghands] = {false};
    do {
      bool found_property = false;
      for (unsigned i = 0; i < longhand_count; ++i) {
        if (parsed_longhand[i])
          continue;

        CSSValue* value =
            consumeLonghandItem(shorthand.properties()[i]->PropertyID(), range,
                                context, use_legacy_parsing);
        if (value) {
          parsed_longhand[i] = true;
          found_property = true;
          longhands[i]->Append(*value);
          break;
        }
      }
      if (!found_property)
        return false;
    } while (!range.AtEnd() && range.Peek().GetType() != kCommaToken);

    for (unsigned i = 0; i < longhand_count; ++i) {
      if (!parsed_longhand[i]) {
        longhands[i]->Append(
            *To<Longhand>(shorthand.properties()[i])->InitialValue());
      }
      parsed_longhand[i] = false;
    }
  } while (css_property_parser_helpers::ConsumeCommaIncludingWhitespace(range));

  return true;
}

void AddBackgroundValue(CSSValue*& list, CSSValue* value) {
  if (list) {
    if (!list->IsBaseValueList()) {
      CSSValue* first_value = list;
      list = CSSValueList::CreateCommaSeparated();
      To<CSSValueList>(list)->Append(*first_value);
    }
    To<CSSValueList>(list)->Append(*value);
  } else {
    // To conserve memory we don't actually wrap a single value in a list.
    list = value;
  }
}

CSSValue* ConsumeBackgroundAttachment(CSSParserTokenRange& range) {
  return css_property_parser_helpers::ConsumeIdent<
      CSSValueID::kScroll, CSSValueID::kFixed, CSSValueID::kLocal>(range);
}

CSSValue* ConsumeBackgroundBlendMode(CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNormal || id == CSSValueID::kOverlay ||
      (id >= CSSValueID::kMultiply && id <= CSSValueID::kLuminosity))
    return css_property_parser_helpers::ConsumeIdent(range);
  return nullptr;
}

CSSValue* ConsumeBackgroundBox(CSSParserTokenRange& range) {
  return css_property_parser_helpers::ConsumeIdent<
      CSSValueID::kBorderBox, CSSValueID::kPaddingBox, CSSValueID::kContentBox>(
      range);
}

CSSValue* ConsumeBackgroundComposite(CSSParserTokenRange& range) {
  return css_property_parser_helpers::ConsumeIdentRange(
      range, CSSValueID::kClear, CSSValueID::kPlusLighter);
}

CSSValue* ConsumeMaskSourceType(CSSParserTokenRange& range) {
  DCHECK(RuntimeEnabledFeatures::CSSMaskSourceTypeEnabled());
  return css_property_parser_helpers::ConsumeIdent<
      CSSValueID::kAuto, CSSValueID::kAlpha, CSSValueID::kLuminance>(range);
}

CSSPrimitiveValue* ConsumeLengthOrPercentCountNegative(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    base::Optional<WebFeature> negative_size) {
  CSSPrimitiveValue* result = ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative,
      css_property_parser_helpers::UnitlessQuirk::kForbid);
  if (!result && negative_size)
    context.Count(*negative_size);
  return result;
}

CSSValue* ConsumeBackgroundSize(CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                base::Optional<WebFeature> negative_size,
                                ParsingStyle parsing_style) {
  if (css_property_parser_helpers::IdentMatches<CSSValueID::kContain,
                                                CSSValueID::kCover>(
          range.Peek().Id())) {
    return css_property_parser_helpers::ConsumeIdent(range);
  }

  CSSValue* horizontal =
      css_property_parser_helpers::ConsumeIdent<CSSValueID::kAuto>(range);
  if (!horizontal) {
    horizontal =
        ConsumeLengthOrPercentCountNegative(range, context, negative_size);
  }
  if (!horizontal)
    return nullptr;

  CSSValue* vertical = nullptr;
  if (!range.AtEnd()) {
    if (range.Peek().Id() == CSSValueID::kAuto) {  // `auto' is the default
      range.ConsumeIncludingWhitespace();
    } else {
      vertical =
          ConsumeLengthOrPercentCountNegative(range, context, negative_size);
    }
  } else if (parsing_style == ParsingStyle::kLegacy) {
    // Legacy syntax: "-webkit-background-size: 10px" is equivalent to
    // "background-size: 10px 10px".
    vertical = horizontal;
  }
  if (!vertical)
    return horizontal;
  return MakeGarbageCollected<CSSValuePair>(horizontal, vertical,
                                            CSSValuePair::kKeepIdenticalValues);
}

static void SetAllowsNegativePercentageReference(CSSValue* value) {
  if (auto* math_value = DynamicTo<CSSMathFunctionValue>(value))
    math_value->SetAllowsNegativePercentageReference();
}

bool ConsumeBackgroundPosition(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    css_property_parser_helpers::UnitlessQuirk unitless,
    CSSValue*& result_x,
    CSSValue*& result_y) {
  do {
    CSSValue* position_x = nullptr;
    CSSValue* position_y = nullptr;
    if (!css_property_parser_helpers::ConsumePosition(
            range, context, unitless,
            WebFeature::kThreeValuedPositionBackground, position_x, position_y))
      return false;
    // TODO(crbug.com/825895): So far, 'background-position' is the only
    // property that allows resolving a percentage against a negative value. If
    // we have more of such properties, we should instead pass an additional
    // argument to ask the parser to set this flag.
    SetAllowsNegativePercentageReference(position_x);
    SetAllowsNegativePercentageReference(position_y);
    AddBackgroundValue(result_x, position_x);
    AddBackgroundValue(result_y, position_y);
  } while (css_property_parser_helpers::ConsumeCommaIncludingWhitespace(range));
  return true;
}

CSSValue* ConsumePrefixedBackgroundBox(CSSParserTokenRange& range,
                                       AllowTextValue allow_text_value) {
  // The values 'border', 'padding' and 'content' are deprecated and do not
  // apply to the version of the property that has the -webkit- prefix removed.
  if (CSSValue* value = css_property_parser_helpers::ConsumeIdentRange(
          range, CSSValueID::kBorder, CSSValueID::kPaddingBox))
    return value;
  if (allow_text_value == AllowTextValue::kAllow &&
      range.Peek().Id() == CSSValueID::kText)
    return css_property_parser_helpers::ConsumeIdent(range);
  return nullptr;
}

CSSValue* ParseBackgroundBox(CSSParserTokenRange& range,
                             const CSSParserLocalContext& local_context,
                             AllowTextValue alias_allow_text_value) {
  // This is legacy behavior that does not match spec, see crbug.com/604023
  if (local_context.UseAliasParsing()) {
    return css_property_parser_helpers::ConsumeCommaSeparatedList(
        ConsumePrefixedBackgroundBox, range, alias_allow_text_value);
  }
  return css_property_parser_helpers::ConsumeCommaSeparatedList(
      ConsumeBackgroundBox, range);
}

CSSValue* ParseBackgroundOrMaskSize(CSSParserTokenRange& range,
                                    const CSSParserContext& context,
                                    const CSSParserLocalContext& local_context,
                                    base::Optional<WebFeature> negative_size) {
  return css_property_parser_helpers::ConsumeCommaSeparatedList(
      ConsumeBackgroundSize, range, context, negative_size,
      local_context.UseAliasParsing() ? ParsingStyle::kLegacy
                                      : ParsingStyle::kNotLegacy);
}

namespace {

CSSValue* ConsumeBackgroundComponent(CSSPropertyID resolved_property,
                                     CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  switch (resolved_property) {
    case CSSPropertyID::kBackgroundClip:
      return ConsumeBackgroundBox(range);
    case CSSPropertyID::kBackgroundAttachment:
      return ConsumeBackgroundAttachment(range);
    case CSSPropertyID::kBackgroundOrigin:
      return ConsumeBackgroundBox(range);
    case CSSPropertyID::kBackgroundImage:
    case CSSPropertyID::kWebkitMaskImage:
      return css_property_parser_helpers::ConsumeImageOrNone(range, &context);
    case CSSPropertyID::kBackgroundPositionX:
    case CSSPropertyID::kWebkitMaskPositionX:
      return ConsumePositionLonghand<CSSValueID::kLeft, CSSValueID::kRight>(
          range, context.Mode());
    case CSSPropertyID::kBackgroundPositionY:
    case CSSPropertyID::kWebkitMaskPositionY:
      return ConsumePositionLonghand<CSSValueID::kTop, CSSValueID::kBottom>(
          range, context.Mode());
    case CSSPropertyID::kBackgroundSize:
      return ConsumeBackgroundSize(range, context,
                                   WebFeature::kNegativeBackgroundSize,
                                   ParsingStyle::kNotLegacy);
    case CSSPropertyID::kWebkitMaskSize:
      return ConsumeBackgroundSize(range, context,
                                   WebFeature::kNegativeMaskSize,
                                   ParsingStyle::kNotLegacy);
    case CSSPropertyID::kBackgroundColor:
      return css_property_parser_helpers::ConsumeColor(range, context.Mode());
    case CSSPropertyID::kWebkitMaskClip:
      return ConsumePrefixedBackgroundBox(range, AllowTextValue::kAllow);
    case CSSPropertyID::kWebkitMaskOrigin:
      return ConsumePrefixedBackgroundBox(range, AllowTextValue::kForbid);
    default:
      break;
  };
  return nullptr;
}

}  // namespace

// Note: this assumes y properties (e.g. background-position-y) follow the x
// properties in the shorthand array.
// TODO(jiameng): this is used by background and -webkit-mask, hence we
// need local_context as an input that contains shorthand id. We will consider
// remove local_context as an input after
//   (i). StylePropertyShorthand is refactored and
//   (ii). we split parsing logic of background and -webkit-mask into
//   different property classes.
bool ParseBackgroundOrMask(bool important,
                           CSSParserTokenRange& range,
                           const CSSParserContext& context,
                           const CSSParserLocalContext& local_context,
                           HeapVector<CSSPropertyValue, 256>& properties) {
  CSSPropertyID shorthand_id = local_context.CurrentShorthand();
  DCHECK(shorthand_id == CSSPropertyID::kBackground ||
         shorthand_id == CSSPropertyID::kWebkitMask);
  const StylePropertyShorthand& shorthand =
      shorthand_id == CSSPropertyID::kBackground ? backgroundShorthand()
                                                 : webkitMaskShorthand();

  const unsigned longhand_count = shorthand.length();
  CSSValue* longhands[10] = {nullptr};
  DCHECK_LE(longhand_count, 10u);

  bool implicit = false;
  do {
    bool parsed_longhand[10] = {false};
    CSSValue* origin_value = nullptr;
    do {
      bool found_property = false;
      bool bg_position_parsed_in_current_layer = false;
      for (unsigned i = 0; i < longhand_count; ++i) {
        if (parsed_longhand[i])
          continue;

        CSSValue* value = nullptr;
        CSSValue* value_y = nullptr;
        const CSSProperty& property = *shorthand.properties()[i];
        if (property.IDEquals(CSSPropertyID::kBackgroundRepeatX) ||
            property.IDEquals(CSSPropertyID::kWebkitMaskRepeatX)) {
          ConsumeRepeatStyleComponent(range, value, value_y, implicit);
        } else if (property.IDEquals(CSSPropertyID::kBackgroundPositionX) ||
                   property.IDEquals(CSSPropertyID::kWebkitMaskPositionX)) {
          if (!css_property_parser_helpers::ConsumePosition(
                  range, context,
                  css_property_parser_helpers::UnitlessQuirk::kForbid,
                  WebFeature::kThreeValuedPositionBackground, value, value_y))
            continue;
          if (value)
            bg_position_parsed_in_current_layer = true;
        } else if (property.IDEquals(CSSPropertyID::kBackgroundSize) ||
                   property.IDEquals(CSSPropertyID::kWebkitMaskSize)) {
          if (!css_property_parser_helpers::ConsumeSlashIncludingWhitespace(
                  range))
            continue;
          value = ConsumeBackgroundSize(
              range, context,
              property.IDEquals(CSSPropertyID::kBackgroundSize)
                  ? WebFeature::kNegativeBackgroundSize
                  : WebFeature::kNegativeMaskSize,
              ParsingStyle::kNotLegacy);
          if (!value || !bg_position_parsed_in_current_layer)
            return false;
        } else if (property.IDEquals(CSSPropertyID::kBackgroundPositionY) ||
                   property.IDEquals(CSSPropertyID::kBackgroundRepeatY) ||
                   property.IDEquals(CSSPropertyID::kWebkitMaskPositionY) ||
                   property.IDEquals(CSSPropertyID::kWebkitMaskRepeatY)) {
          continue;
        } else {
          value =
              ConsumeBackgroundComponent(property.PropertyID(), range, context);
        }
        if (value) {
          if (property.IDEquals(CSSPropertyID::kBackgroundOrigin) ||
              property.IDEquals(CSSPropertyID::kWebkitMaskOrigin)) {
            origin_value = value;
          }
          parsed_longhand[i] = true;
          found_property = true;
          AddBackgroundValue(longhands[i], value);
          if (value_y) {
            parsed_longhand[i + 1] = true;
            AddBackgroundValue(longhands[i + 1], value_y);
          }
        }
      }
      if (!found_property)
        return false;
    } while (!range.AtEnd() && range.Peek().GetType() != kCommaToken);

    // TODO(timloh): This will make invalid longhands, see crbug.com/386459
    for (unsigned i = 0; i < longhand_count; ++i) {
      const CSSProperty& property = *shorthand.properties()[i];
      if (property.IDEquals(CSSPropertyID::kBackgroundColor) &&
          !range.AtEnd()) {
        if (parsed_longhand[i])
          return false;  // Colors are only allowed in the last layer.
        continue;
      }
      if ((property.IDEquals(CSSPropertyID::kBackgroundClip) ||
           property.IDEquals(CSSPropertyID::kWebkitMaskClip)) &&
          !parsed_longhand[i] && origin_value) {
        AddBackgroundValue(longhands[i], origin_value);
        continue;
      }
      if (!parsed_longhand[i]) {
        AddBackgroundValue(longhands[i], CSSInitialValue::Create());
      }
    }
  } while (css_property_parser_helpers::ConsumeCommaIncludingWhitespace(range));
  if (!range.AtEnd())
    return false;

  for (unsigned i = 0; i < longhand_count; ++i) {
    const CSSProperty& property = *shorthand.properties()[i];
    if (property.IDEquals(CSSPropertyID::kBackgroundSize) && longhands[i] &&
        context.UseLegacyBackgroundSizeShorthandBehavior())
      continue;
    css_property_parser_helpers::AddProperty(
        property.PropertyID(), shorthand.id(), *longhands[i], important,
        implicit
            ? css_property_parser_helpers::IsImplicitProperty::kImplicit
            : css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
        properties);
  }
  return true;
}

bool ConsumeRepeatStyleComponent(CSSParserTokenRange& range,
                                 CSSValue*& value1,
                                 CSSValue*& value2,
                                 bool& implicit) {
  if (css_property_parser_helpers::ConsumeIdent<CSSValueID::kRepeatX>(range)) {
    value1 = CSSIdentifierValue::Create(CSSValueID::kRepeat);
    value2 = CSSIdentifierValue::Create(CSSValueID::kNoRepeat);
    implicit = true;
    return true;
  }
  if (css_property_parser_helpers::ConsumeIdent<CSSValueID::kRepeatY>(range)) {
    value1 = CSSIdentifierValue::Create(CSSValueID::kNoRepeat);
    value2 = CSSIdentifierValue::Create(CSSValueID::kRepeat);
    implicit = true;
    return true;
  }
  value1 = css_property_parser_helpers::ConsumeIdent<
      CSSValueID::kRepeat, CSSValueID::kNoRepeat, CSSValueID::kRound,
      CSSValueID::kSpace>(range);
  if (!value1)
    return false;

  value2 = css_property_parser_helpers::ConsumeIdent<
      CSSValueID::kRepeat, CSSValueID::kNoRepeat, CSSValueID::kRound,
      CSSValueID::kSpace>(range);
  if (!value2) {
    value2 = value1;
    implicit = true;
  }
  return true;
}

bool ConsumeRepeatStyle(CSSParserTokenRange& range,
                        CSSValue*& result_x,
                        CSSValue*& result_y,
                        bool& implicit) {
  do {
    CSSValue* repeat_x = nullptr;
    CSSValue* repeat_y = nullptr;
    if (!ConsumeRepeatStyleComponent(range, repeat_x, repeat_y, implicit))
      return false;
    AddBackgroundValue(result_x, repeat_x);
    AddBackgroundValue(result_y, repeat_y);
  } while (css_property_parser_helpers::ConsumeCommaIncludingWhitespace(range));
  return true;
}

CSSValue* ConsumeWebkitBorderImage(CSSParserTokenRange& range,
                                   const CSSParserContext& context) {
  CSSValue* source = nullptr;
  CSSValue* slice = nullptr;
  CSSValue* width = nullptr;
  CSSValue* outset = nullptr;
  CSSValue* repeat = nullptr;
  if (ConsumeBorderImageComponents(range, context, source, slice, width, outset,
                                   repeat, DefaultFill::kFill))
    return CreateBorderImageValue(source, slice, width, outset, repeat);
  return nullptr;
}

bool ConsumeBorderImageComponents(CSSParserTokenRange& range,
                                  const CSSParserContext& context,
                                  CSSValue*& source,
                                  CSSValue*& slice,
                                  CSSValue*& width,
                                  CSSValue*& outset,
                                  CSSValue*& repeat,
                                  DefaultFill default_fill) {
  do {
    if (!source) {
      source = css_property_parser_helpers::ConsumeImageOrNone(range, &context);
      if (source)
        continue;
    }
    if (!repeat) {
      repeat = ConsumeBorderImageRepeat(range);
      if (repeat)
        continue;
    }
    if (!slice) {
      slice = ConsumeBorderImageSlice(range, default_fill);
      if (slice) {
        DCHECK(!width);
        DCHECK(!outset);
        if (css_property_parser_helpers::ConsumeSlashIncludingWhitespace(
                range)) {
          width = ConsumeBorderImageWidth(range);
          if (css_property_parser_helpers::ConsumeSlashIncludingWhitespace(
                  range)) {
            outset = ConsumeBorderImageOutset(range);
            if (!outset)
              return false;
          } else if (!width) {
            return false;
          }
        }
      } else {
        return false;
      }
    } else {
      return false;
    }
  } while (!range.AtEnd());
  return true;
}

CSSValue* ConsumeBorderImageRepeat(CSSParserTokenRange& range) {
  CSSIdentifierValue* horizontal = ConsumeBorderImageRepeatKeyword(range);
  if (!horizontal)
    return nullptr;
  CSSIdentifierValue* vertical = ConsumeBorderImageRepeatKeyword(range);
  if (!vertical)
    vertical = horizontal;
  return MakeGarbageCollected<CSSValuePair>(horizontal, vertical,
                                            CSSValuePair::kDropIdenticalValues);
}

CSSValue* ConsumeBorderImageSlice(CSSParserTokenRange& range,
                                  DefaultFill default_fill) {
  bool fill =
      css_property_parser_helpers::ConsumeIdent<CSSValueID::kFill>(range);
  CSSValue* slices[4] = {nullptr};

  for (size_t index = 0; index < 4; ++index) {
    CSSPrimitiveValue* value = css_property_parser_helpers::ConsumePercent(
        range, kValueRangeNonNegative);
    if (!value) {
      value = css_property_parser_helpers::ConsumeNumber(
          range, kValueRangeNonNegative);
    }
    if (!value)
      break;
    slices[index] = value;
  }
  if (!slices[0])
    return nullptr;
  if (css_property_parser_helpers::ConsumeIdent<CSSValueID::kFill>(range)) {
    if (fill)
      return nullptr;
    fill = true;
  }
  css_property_parser_helpers::Complete4Sides(slices);
  if (default_fill == DefaultFill::kFill)
    fill = true;
  return MakeGarbageCollected<cssvalue::CSSBorderImageSliceValue>(
      MakeGarbageCollected<CSSQuadValue>(slices[0], slices[1], slices[2],
                                         slices[3],
                                         CSSQuadValue::kSerializeAsQuad),
      fill);
}

CSSValue* ConsumeBorderImageWidth(CSSParserTokenRange& range) {
  CSSValue* widths[4] = {nullptr};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value = css_property_parser_helpers::ConsumeNumber(range,
                                                       kValueRangeNonNegative);
    if (!value) {
      value = css_property_parser_helpers::ConsumeLengthOrPercent(
          range, kHTMLStandardMode, kValueRangeNonNegative,
          css_property_parser_helpers::UnitlessQuirk::kForbid);
    }
    if (!value) {
      value =
          css_property_parser_helpers::ConsumeIdent<CSSValueID::kAuto>(range);
    }
    if (!value)
      break;
    widths[index] = value;
  }
  if (!widths[0])
    return nullptr;
  css_property_parser_helpers::Complete4Sides(widths);
  return MakeGarbageCollected<CSSQuadValue>(widths[0], widths[1], widths[2],
                                            widths[3],
                                            CSSQuadValue::kSerializeAsQuad);
}

CSSValue* ConsumeBorderImageOutset(CSSParserTokenRange& range) {
  CSSValue* outsets[4] = {nullptr};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value = css_property_parser_helpers::ConsumeNumber(range,
                                                       kValueRangeNonNegative);
    if (!value) {
      value = css_property_parser_helpers::ConsumeLength(
          range, kHTMLStandardMode, kValueRangeNonNegative);
    }
    if (!value)
      break;
    outsets[index] = value;
  }
  if (!outsets[0])
    return nullptr;
  css_property_parser_helpers::Complete4Sides(outsets);
  return MakeGarbageCollected<CSSQuadValue>(outsets[0], outsets[1], outsets[2],
                                            outsets[3],
                                            CSSQuadValue::kSerializeAsQuad);
}

CSSValue* ParseBorderRadiusCorner(CSSParserTokenRange& range,
                                  const CSSParserContext& context) {
  CSSValue* parsed_value1 = css_property_parser_helpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
  if (!parsed_value1)
    return nullptr;
  CSSValue* parsed_value2 = css_property_parser_helpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
  if (!parsed_value2)
    parsed_value2 = parsed_value1;
  return MakeGarbageCollected<CSSValuePair>(parsed_value1, parsed_value2,
                                            CSSValuePair::kDropIdenticalValues);
}

CSSValue* ParseBorderWidthSide(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               const CSSParserLocalContext& local_context) {
  CSSPropertyID shorthand = local_context.CurrentShorthand();
  bool allow_quirky_lengths = IsQuirksModeBehavior(context.Mode()) &&
                              (shorthand == CSSPropertyID::kInvalid ||
                               shorthand == CSSPropertyID::kBorderWidth);
  css_property_parser_helpers::UnitlessQuirk unitless =
      allow_quirky_lengths
          ? css_property_parser_helpers::UnitlessQuirk::kAllow
          : css_property_parser_helpers::UnitlessQuirk::kForbid;
  return ConsumeBorderWidth(range, context.Mode(), unitless);
}

CSSValue* ConsumeShadow(CSSParserTokenRange& range,
                        CSSParserMode css_parser_mode,
                        AllowInsetAndSpread inset_and_spread) {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeCommaSeparatedList(
      ParseSingleShadow, range, css_parser_mode, inset_and_spread);
}

CSSShadowValue* ParseSingleShadow(CSSParserTokenRange& range,
                                  CSSParserMode css_parser_mode,
                                  AllowInsetAndSpread inset_and_spread) {
  CSSIdentifierValue* style = nullptr;
  CSSValue* color = nullptr;

  if (range.AtEnd())
    return nullptr;

  color = css_property_parser_helpers::ConsumeColor(range, css_parser_mode);
  if (range.Peek().Id() == CSSValueID::kInset) {
    if (inset_and_spread != AllowInsetAndSpread::kAllow)
      return nullptr;
    style = css_property_parser_helpers::ConsumeIdent(range);
    if (!color)
      color = css_property_parser_helpers::ConsumeColor(range, css_parser_mode);
  }

  CSSPrimitiveValue* horizontal_offset =
      css_property_parser_helpers::ConsumeLength(range, css_parser_mode,
                                                 kValueRangeAll);
  if (!horizontal_offset)
    return nullptr;

  CSSPrimitiveValue* vertical_offset =
      css_property_parser_helpers::ConsumeLength(range, css_parser_mode,
                                                 kValueRangeAll);
  if (!vertical_offset)
    return nullptr;

  CSSPrimitiveValue* blur_radius = css_property_parser_helpers::ConsumeLength(
      range, css_parser_mode, kValueRangeNonNegative);
  CSSPrimitiveValue* spread_distance = nullptr;
  if (blur_radius) {
    if (inset_and_spread == AllowInsetAndSpread::kAllow) {
      spread_distance = css_property_parser_helpers::ConsumeLength(
          range, css_parser_mode, kValueRangeAll);
    }
  }

  if (!range.AtEnd()) {
    if (!color)
      color = css_property_parser_helpers::ConsumeColor(range, css_parser_mode);
    if (range.Peek().Id() == CSSValueID::kInset) {
      if (inset_and_spread != AllowInsetAndSpread::kAllow || style)
        return nullptr;
      style = css_property_parser_helpers::ConsumeIdent(range);
      if (!color) {
        color =
            css_property_parser_helpers::ConsumeColor(range, css_parser_mode);
      }
    }
  }
  return MakeGarbageCollected<CSSShadowValue>(horizontal_offset,
                                              vertical_offset, blur_radius,
                                              spread_distance, style, color);
}

CSSValue* ConsumeColumnCount(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumePositiveInteger(range);
}

CSSValue* ConsumeColumnWidth(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_property_parser_helpers::ConsumeIdent(range);
  // Always parse lengths in strict mode here, since it would be ambiguous
  // otherwise when used in the 'columns' shorthand property.
  CSSPrimitiveValue* column_width = css_property_parser_helpers::ConsumeLength(
      range, kHTMLStandardMode, kValueRangeNonNegative);
  if (!column_width)
    return nullptr;
  return column_width;
}

bool ConsumeColumnWidthOrCount(CSSParserTokenRange& range,
                               CSSValue*& column_width,
                               CSSValue*& column_count) {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    css_property_parser_helpers::ConsumeIdent(range);
    return true;
  }
  if (!column_width) {
    column_width = ConsumeColumnWidth(range);
    if (column_width)
      return true;
  }
  if (!column_count)
    column_count = ConsumeColumnCount(range);
  return column_count;
}

CSSValue* ConsumeGapLength(CSSParserTokenRange& range,
                           const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNormal)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
}

CSSValue* ConsumeCounter(CSSParserTokenRange& range,
                         const CSSParserContext& context,
                         int default_value) {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSCustomIdentValue* counter_name =
        css_property_parser_helpers::ConsumeCustomIdent(range, context);
    if (!counter_name)
      return nullptr;
    int value = default_value;
    if (CSSPrimitiveValue* counter_value =
            css_property_parser_helpers::ConsumeInteger(range))
      value = clampTo<int>(counter_value->GetDoubleValue());
    list->Append(*MakeGarbageCollected<CSSValuePair>(
        counter_name,
        CSSNumericLiteralValue::Create(value,
                                       CSSPrimitiveValue::UnitType::kInteger),
        CSSValuePair::kDropIdenticalValues));
  } while (!range.AtEnd());
  return list;
}

CSSValue* ConsumeFontSize(CSSParserTokenRange& range,
                          const CSSParserContext& context,
                          css_property_parser_helpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueID::kWebkitXxxLarge)
    context.Count(WebFeature::kFontSizeWebkitXxxLarge);
  if (range.Peek().Id() >= CSSValueID::kXxSmall &&
      range.Peek().Id() <= CSSValueID::kWebkitXxxLarge)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative, unitless);
}

CSSValue* ConsumeLineHeight(CSSParserTokenRange& range,
                            CSSParserMode css_parser_mode) {
  if (range.Peek().Id() == CSSValueID::kNormal)
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSPrimitiveValue* line_height =
      css_property_parser_helpers::ConsumeNumber(range, kValueRangeNonNegative);
  if (line_height)
    return line_height;
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeNonNegative);
}

CSSValueList* ConsumeFontFamily(CSSParserTokenRange& range) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* parsed_value = ConsumeGenericFamily(range);
    if (parsed_value) {
      list->Append(*parsed_value);
    } else {
      parsed_value = ConsumeFamilyName(range);
      if (parsed_value) {
        list->Append(*parsed_value);
      } else {
        return nullptr;
      }
    }
  } while (css_property_parser_helpers::ConsumeCommaIncludingWhitespace(range));
  return list;
}

CSSValue* ConsumeGenericFamily(CSSParserTokenRange& range) {
  return css_property_parser_helpers::ConsumeIdentRange(
      range, CSSValueID::kSerif, CSSValueID::kWebkitBody);
}

CSSValue* ConsumeFamilyName(CSSParserTokenRange& range) {
  if (range.Peek().GetType() == kStringToken) {
    return CSSFontFamilyValue::Create(
        range.ConsumeIncludingWhitespace().Value().ToString());
  }
  if (range.Peek().GetType() != kIdentToken)
    return nullptr;
  String family_name = ConcatenateFamilyName(range);
  if (family_name.IsNull())
    return nullptr;
  return CSSFontFamilyValue::Create(family_name);
}

String ConcatenateFamilyName(CSSParserTokenRange& range) {
  StringBuilder builder;
  bool added_space = false;
  const CSSParserToken& first_token = range.Peek();
  while (range.Peek().GetType() == kIdentToken) {
    if (!builder.IsEmpty()) {
      builder.Append(' ');
      added_space = true;
    }
    builder.Append(range.ConsumeIncludingWhitespace().Value());
  }
  if (!added_space &&
      (css_property_parser_helpers::IsCSSWideKeyword(first_token.Value()) ||
       EqualIgnoringASCIICase(first_token.Value(), "default"))) {
    return String();
  }
  return builder.ToString();
}

CSSValueList* CombineToRangeListOrNull(const CSSPrimitiveValue* range_start,
                                       const CSSPrimitiveValue* range_end) {
  DCHECK(range_start);
  DCHECK(range_end);
  if (range_end->GetFloatValue() < range_start->GetFloatValue())
    return nullptr;
  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  value_list->Append(*range_start);
  value_list->Append(*range_end);
  return value_list;
}

bool IsAngleWithinLimits(CSSPrimitiveValue* angle) {
  constexpr float kMaxAngle = 90.0f;
  return angle->GetFloatValue() >= -kMaxAngle &&
         angle->GetFloatValue() <= kMaxAngle;
}

CSSValue* ConsumeFontStyle(CSSParserTokenRange& range,
                           const CSSParserMode& parser_mode) {
  if (range.Peek().Id() == CSSValueID::kNormal ||
      range.Peek().Id() == CSSValueID::kItalic)
    return css_property_parser_helpers::ConsumeIdent(range);

  if (range.Peek().Id() != CSSValueID::kOblique)
    return nullptr;

  CSSIdentifierValue* oblique_identifier =
      css_property_parser_helpers::ConsumeIdent<CSSValueID::kOblique>(range);

  CSSPrimitiveValue* start_angle = css_property_parser_helpers::ConsumeAngle(
      range, nullptr, base::nullopt, MinObliqueValue(), MaxObliqueValue());
  if (!start_angle)
    return oblique_identifier;
  if (!IsAngleWithinLimits(start_angle))
    return nullptr;

  if (parser_mode != kCSSFontFaceRuleMode || range.AtEnd()) {
    CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
    value_list->Append(*start_angle);
    return MakeGarbageCollected<cssvalue::CSSFontStyleRangeValue>(
        *oblique_identifier, *value_list);
  }

  CSSPrimitiveValue* end_angle = css_property_parser_helpers::ConsumeAngle(
      range, nullptr, base::nullopt, MinObliqueValue(), MaxObliqueValue());
  if (!end_angle || !IsAngleWithinLimits(end_angle))
    return nullptr;

  CSSValueList* range_list = CombineToRangeListOrNull(start_angle, end_angle);
  if (!range_list)
    return nullptr;
  return MakeGarbageCollected<cssvalue::CSSFontStyleRangeValue>(
      *oblique_identifier, *range_list);
}

CSSIdentifierValue* ConsumeFontStretchKeywordOnly(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  if (token.Id() == CSSValueID::kNormal ||
      (token.Id() >= CSSValueID::kUltraCondensed &&
       token.Id() <= CSSValueID::kUltraExpanded))
    return css_property_parser_helpers::ConsumeIdent(range);
  return nullptr;
}

CSSValue* ConsumeFontStretch(CSSParserTokenRange& range,
                             const CSSParserMode& parser_mode) {
  CSSIdentifierValue* parsed_keyword = ConsumeFontStretchKeywordOnly(range);
  if (parsed_keyword)
    return parsed_keyword;

  CSSPrimitiveValue* start_percent =
      css_property_parser_helpers::ConsumePercent(range,
                                                  kValueRangeNonNegative);
  if (!start_percent)
    return nullptr;

  // In a non-font-face context, more than one percentage is not allowed.
  if (parser_mode != kCSSFontFaceRuleMode || range.AtEnd())
    return start_percent;

  CSSPrimitiveValue* end_percent = css_property_parser_helpers::ConsumePercent(
      range, kValueRangeNonNegative);
  if (!end_percent)
    return nullptr;

  return CombineToRangeListOrNull(start_percent, end_percent);
}

CSSValue* ConsumeFontWeight(CSSParserTokenRange& range,
                            const CSSParserMode& parser_mode) {
  const CSSParserToken& token = range.Peek();
  if (token.Id() >= CSSValueID::kNormal && token.Id() <= CSSValueID::kLighter)
    return css_property_parser_helpers::ConsumeIdent(range);

  // Avoid consuming the first zero of font: 0/0; e.g. in the Acid3 test.  In
  // font:0/0; the first zero is the font size, the second is the line height.
  // In font: 100 0/0; we should parse the first 100 as font-weight, the 0
  // before the slash as font size. We need to peek and check the token in order
  // to avoid parsing a 0 font size as a font-weight. If we call ConsumeNumber
  // straight away without Peek, then the parsing cursor advances too far and we
  // parsed font-size as font-weight incorrectly.
  if (token.GetType() == kNumberToken &&
      (token.NumericValue() < 1 || token.NumericValue() > 1000))
    return nullptr;

  CSSPrimitiveValue* start_weight =
      css_property_parser_helpers::ConsumeNumber(range, kValueRangeNonNegative);
  if (!start_weight || start_weight->GetFloatValue() < 1 ||
      start_weight->GetFloatValue() > 1000)
    return nullptr;

  // In a non-font-face context, more than one number is not allowed. Return
  // what we have. If there is trailing garbage, the AtEnd() check in
  // CSSPropertyParser::ParseValueStart will catch that.
  if (parser_mode != kCSSFontFaceRuleMode || range.AtEnd())
    return start_weight;

  CSSPrimitiveValue* end_weight =
      css_property_parser_helpers::ConsumeNumber(range, kValueRangeNonNegative);
  if (!end_weight || end_weight->GetFloatValue() < 1 ||
      end_weight->GetFloatValue() > 1000)
    return nullptr;

  return CombineToRangeListOrNull(start_weight, end_weight);
}

CSSValue* ConsumeFontFeatureSettings(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueID::kNormal)
    return css_property_parser_helpers::ConsumeIdent(range);
  CSSValueList* settings = CSSValueList::CreateCommaSeparated();
  do {
    CSSFontFeatureValue* font_feature_value = ConsumeFontFeatureTag(range);
    if (!font_feature_value)
      return nullptr;
    settings->Append(*font_feature_value);
  } while (css_property_parser_helpers::ConsumeCommaIncludingWhitespace(range));
  return settings;
}

CSSFontFeatureValue* ConsumeFontFeatureTag(CSSParserTokenRange& range) {
  // Feature tag name consists of 4-letter characters.
  const unsigned kTagNameLength = 4;

  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  // Feature tag name comes first
  if (token.GetType() != kStringToken)
    return nullptr;
  if (token.Value().length() != kTagNameLength)
    return nullptr;
  AtomicString tag = token.Value().ToAtomicString();
  for (unsigned i = 0; i < kTagNameLength; ++i) {
    // Limits the range of characters to 0x20-0x7E, following the tag name rules
    // defined in the OpenType specification.
    UChar character = tag[i];
    if (character < 0x20 || character > 0x7E)
      return nullptr;
  }

  int tag_value = 1;
  // Feature tag values could follow: <integer> | on | off
  if (CSSPrimitiveValue* value =
          css_property_parser_helpers::ConsumeInteger(range, 0)) {
    tag_value = clampTo<int>(value->GetDoubleValue());
  } else if (range.Peek().Id() == CSSValueID::kOn ||
             range.Peek().Id() == CSSValueID::kOff) {
    tag_value = range.ConsumeIncludingWhitespace().Id() == CSSValueID::kOn;
  }
  return MakeGarbageCollected<CSSFontFeatureValue>(tag, tag_value);
}

CSSIdentifierValue* ConsumeFontVariantCSS21(CSSParserTokenRange& range) {
  return css_property_parser_helpers::ConsumeIdent<CSSValueID::kNormal,
                                                   CSSValueID::kSmallCaps>(
      range);
}

Vector<String> ParseGridTemplateAreasColumnNames(const String& grid_row_names) {
  DCHECK(!grid_row_names.IsEmpty());
  Vector<String> column_names;
  // Using StringImpl to avoid checks and indirection in every call to
  // String::operator[].
  StringImpl& text = *grid_row_names.Impl();

  StringBuilder area_name;
  for (unsigned i = 0; i < text.length(); ++i) {
    if (IsCSSSpace(text[i])) {
      if (!area_name.IsEmpty()) {
        column_names.push_back(area_name.ToString());
        area_name.Clear();
      }
      continue;
    }
    if (text[i] == '.') {
      if (area_name == ".")
        continue;
      if (!area_name.IsEmpty()) {
        column_names.push_back(area_name.ToString());
        area_name.Clear();
      }
    } else {
      if (!IsNameCodePoint(text[i]))
        return Vector<String>();
      if (area_name == ".") {
        column_names.push_back(area_name.ToString());
        area_name.Clear();
      }
    }

    area_name.Append(text[i]);
  }

  if (!area_name.IsEmpty())
    column_names.push_back(area_name.ToString());

  return column_names;
}

CSSValue* ConsumeGridBreadth(CSSParserTokenRange& range,
                             CSSParserMode css_parser_mode) {
  const CSSParserToken& token = range.Peek();
  if (css_property_parser_helpers::IdentMatches<
          CSSValueID::kMinContent, CSSValueID::kMaxContent, CSSValueID::kAuto>(
          token.Id()))
    return css_property_parser_helpers::ConsumeIdent(range);
  if (token.GetType() == kDimensionToken &&
      token.GetUnitType() == CSSPrimitiveValue::UnitType::kFraction) {
    if (range.Peek().NumericValue() < 0)
      return nullptr;
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kFraction);
  }
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeNonNegative,
      css_property_parser_helpers::UnitlessQuirk::kForbid);
}

CSSValue* ConsumeFitContent(CSSParserTokenRange& range,
                            CSSParserMode css_parser_mode) {
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      css_property_parser_helpers::ConsumeFunction(range_copy);
  CSSPrimitiveValue* length =
      css_property_parser_helpers::ConsumeLengthOrPercent(
          args, css_parser_mode, kValueRangeNonNegative,
          css_property_parser_helpers::UnitlessQuirk::kAllow);
  if (!length || !args.AtEnd())
    return nullptr;
  range = range_copy;
  auto* result =
      MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kFitContent);
  result->Append(*length);
  return result;
}

bool IsGridBreadthFixedSized(const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID value_id = identifier_value->GetValueID();
    return !(value_id == CSSValueID::kMinContent ||
             value_id == CSSValueID::kMaxContent ||
             value_id == CSSValueID::kAuto);
  }

  if (auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value))
    return !primitive_value->IsFlex();

  NOTREACHED();
  return true;
}

bool IsGridTrackFixedSized(const CSSValue& value) {
  if (value.IsPrimitiveValue() || value.IsIdentifierValue())
    return IsGridBreadthFixedSized(value);

  auto& function = To<CSSFunctionValue>(value);
  if (function.FunctionType() == CSSValueID::kFitContent)
    return false;

  const CSSValue& min_value = function.Item(0);
  const CSSValue& max_value = function.Item(1);
  return IsGridBreadthFixedSized(min_value) ||
         IsGridBreadthFixedSized(max_value);
}

CSSValue* ConsumeGridTrackSize(CSSParserTokenRange& range,
                               CSSParserMode css_parser_mode) {
  const CSSParserToken& token = range.Peek();
  if (css_property_parser_helpers::IdentMatches<CSSValueID::kAuto>(token.Id()))
    return css_property_parser_helpers::ConsumeIdent(range);

  if (token.FunctionId() == CSSValueID::kMinmax) {
    CSSParserTokenRange range_copy = range;
    CSSParserTokenRange args =
        css_property_parser_helpers::ConsumeFunction(range_copy);
    CSSValue* min_track_breadth = ConsumeGridBreadth(args, css_parser_mode);
    auto* min_track_breadth_primitive_value =
        DynamicTo<CSSPrimitiveValue>(min_track_breadth);
    if (!min_track_breadth ||
        (min_track_breadth_primitive_value &&
         min_track_breadth_primitive_value->IsFlex()) ||
        !css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args))
      return nullptr;
    CSSValue* max_track_breadth = ConsumeGridBreadth(args, css_parser_mode);
    if (!max_track_breadth || !args.AtEnd())
      return nullptr;
    range = range_copy;
    auto* result = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kMinmax);
    result->Append(*min_track_breadth);
    result->Append(*max_track_breadth);
    return result;
  }

  if (token.FunctionId() == CSSValueID::kFitContent)
    return ConsumeFitContent(range, css_parser_mode);

  return ConsumeGridBreadth(range, css_parser_mode);
}

CSSCustomIdentValue* ConsumeCustomIdentForGridLine(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kAuto ||
      range.Peek().Id() == CSSValueID::kSpan ||
      range.Peek().Id() == CSSValueID::kDefault)
    return nullptr;
  return css_property_parser_helpers::ConsumeCustomIdent(range, context);
}

// Appends to the passed in CSSGridLineNamesValue if any, otherwise creates a
// new one. Returns nullptr if an empty list is consumed.
CSSGridLineNamesValue* ConsumeGridLineNames(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSGridLineNamesValue* line_names = nullptr) {
  CSSParserTokenRange range_copy = range;
  if (range_copy.ConsumeIncludingWhitespace().GetType() != kLeftBracketToken)
    return nullptr;
  if (!line_names)
    line_names = MakeGarbageCollected<CSSGridLineNamesValue>();
  while (CSSCustomIdentValue* line_name =
             ConsumeCustomIdentForGridLine(range_copy, context))
    line_names->Append(*line_name);
  if (range_copy.ConsumeIncludingWhitespace().GetType() != kRightBracketToken)
    return nullptr;
  range = range_copy;
  if (line_names->length() == 0U)
    return nullptr;
  return line_names;
}

bool ConsumeGridTrackRepeatFunction(CSSParserTokenRange& range,
                                    const CSSParserContext& context,
                                    CSSParserMode css_parser_mode,
                                    CSSValueList& list,
                                    bool& is_auto_repeat,
                                    bool& all_tracks_are_fixed_sized) {
  CSSParserTokenRange args =
      css_property_parser_helpers::ConsumeFunction(range);
  // The number of repetitions for <auto-repeat> is not important at parsing
  // level because it will be computed later, let's set it to 1.
  size_t repetitions = 1;
  is_auto_repeat = css_property_parser_helpers::IdentMatches<
      CSSValueID::kAutoFill, CSSValueID::kAutoFit>(args.Peek().Id());
  CSSValueList* repeated_values;
  if (is_auto_repeat) {
    repeated_values = MakeGarbageCollected<cssvalue::CSSGridAutoRepeatValue>(
        args.ConsumeIncludingWhitespace().Id());
  } else {
    // TODO(rob.buis): a consumeIntegerRaw would be more efficient here.
    CSSPrimitiveValue* repetition =
        css_property_parser_helpers::ConsumePositiveInteger(args);
    if (!repetition)
      return false;
    repetitions =
        clampTo<size_t>(repetition->GetDoubleValue(), 0, kGridMaxTracks);
    repeated_values = CSSValueList::CreateSpaceSeparated();
  }
  if (!css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args))
    return false;
  CSSGridLineNamesValue* line_names = ConsumeGridLineNames(args, context);
  if (line_names)
    repeated_values->Append(*line_names);

  size_t number_of_tracks = 0;
  while (!args.AtEnd()) {
    CSSValue* track_size = ConsumeGridTrackSize(args, css_parser_mode);
    if (!track_size)
      return false;
    if (all_tracks_are_fixed_sized)
      all_tracks_are_fixed_sized = IsGridTrackFixedSized(*track_size);
    repeated_values->Append(*track_size);
    ++number_of_tracks;
    line_names = ConsumeGridLineNames(args, context);
    if (line_names)
      repeated_values->Append(*line_names);
  }
  // We should have found at least one <track-size> or else it is not a valid
  // <track-list>.
  if (!number_of_tracks)
    return false;

  if (is_auto_repeat) {
    list.Append(*repeated_values);
  } else {
    // We clamp the repetitions to a multiple of the repeat() track list's size,
    // while staying below the max grid size.
    repetitions = std::min(repetitions, kGridMaxTracks / number_of_tracks);
    auto* integer_repeated_values =
        MakeGarbageCollected<cssvalue::CSSGridIntegerRepeatValue>(repetitions);
    for (size_t i = 0; i < repeated_values->length(); ++i)
      integer_repeated_values->Append(repeated_values->Item(i));
    list.Append(*integer_repeated_values);
  }

  return true;
}

bool ConsumeGridTemplateRowsAndAreasAndColumns(bool important,
                                               CSSParserTokenRange& range,
                                               const CSSParserContext& context,
                                               CSSValue*& template_rows,
                                               CSSValue*& template_columns,
                                               CSSValue*& template_areas) {
  DCHECK(!template_rows);
  DCHECK(!template_columns);
  DCHECK(!template_areas);

  NamedGridAreaMap grid_area_map;
  size_t row_count = 0;
  size_t column_count = 0;
  CSSValueList* template_rows_value_list = CSSValueList::CreateSpaceSeparated();

  // Persists between loop iterations so we can use the same value for
  // consecutive <line-names> values
  CSSGridLineNamesValue* line_names = nullptr;

  do {
    // Handle leading <custom-ident>*.
    bool has_previous_line_names = line_names;
    line_names = ConsumeGridLineNames(range, context, line_names);
    if (line_names && !has_previous_line_names)
      template_rows_value_list->Append(*line_names);

    // Handle a template-area's row.
    if (range.Peek().GetType() != kStringToken ||
        !ParseGridTemplateAreasRow(
            range.ConsumeIncludingWhitespace().Value().ToString(),
            grid_area_map, row_count, column_count))
      return false;
    ++row_count;

    // Handle template-rows's track-size.
    CSSValue* value = ConsumeGridTrackSize(range, context.Mode());
    if (!value)
      value = CSSIdentifierValue::Create(CSSValueID::kAuto);
    template_rows_value_list->Append(*value);

    // This will handle the trailing/leading <custom-ident>* in the grammar.
    line_names = ConsumeGridLineNames(range, context);
    if (line_names)
      template_rows_value_list->Append(*line_names);
  } while (!range.AtEnd() && !(range.Peek().GetType() == kDelimiterToken &&
                               range.Peek().Delimiter() == '/'));

  if (!range.AtEnd()) {
    if (!css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range))
      return false;
    template_columns = ConsumeGridTrackList(
        range, context, context.Mode(), TrackListType::kGridTemplateNoRepeat);
    if (!template_columns || !range.AtEnd())
      return false;
  } else {
    template_columns = CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  template_rows = template_rows_value_list;
  template_areas = MakeGarbageCollected<cssvalue::CSSGridTemplateAreasValue>(
      grid_area_map, row_count, column_count);
  return true;
}

CSSValue* ConsumeGridLine(CSSParserTokenRange& range,
                          const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSIdentifierValue* span_value = nullptr;
  CSSCustomIdentValue* grid_line_name = nullptr;
  CSSPrimitiveValue* numeric_value =
      css_property_parser_helpers::ConsumeInteger(range);
  if (numeric_value) {
    grid_line_name = ConsumeCustomIdentForGridLine(range, context);
    span_value =
        css_property_parser_helpers::ConsumeIdent<CSSValueID::kSpan>(range);
  } else {
    span_value =
        css_property_parser_helpers::ConsumeIdent<CSSValueID::kSpan>(range);
    if (span_value) {
      numeric_value = css_property_parser_helpers::ConsumeInteger(range);
      grid_line_name = ConsumeCustomIdentForGridLine(range, context);
      if (!numeric_value)
        numeric_value = css_property_parser_helpers::ConsumeInteger(range);
    } else {
      grid_line_name = ConsumeCustomIdentForGridLine(range, context);
      if (grid_line_name) {
        numeric_value = css_property_parser_helpers::ConsumeInteger(range);
        span_value =
            css_property_parser_helpers::ConsumeIdent<CSSValueID::kSpan>(range);
        if (!span_value && !numeric_value)
          return grid_line_name;
      } else {
        return nullptr;
      }
    }
  }

  if (span_value && !numeric_value && !grid_line_name)
    return nullptr;  // "span" keyword alone is invalid.
  if (span_value && numeric_value && numeric_value->GetIntValue() < 0)
    return nullptr;  // Negative numbers are not allowed for span.
  if (numeric_value && numeric_value->GetIntValue() == 0)
    return nullptr;  // An <integer> value of zero makes the declaration
                     // invalid.

  if (numeric_value) {
    numeric_value = CSSNumericLiteralValue::Create(
        clampTo(numeric_value->GetIntValue(), -kGridMaxTracks, kGridMaxTracks),
        CSSPrimitiveValue::UnitType::kInteger);
  }

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  if (span_value)
    values->Append(*span_value);
  if (numeric_value)
    values->Append(*numeric_value);
  if (grid_line_name)
    values->Append(*grid_line_name);
  DCHECK(values->length());
  return values;
}

CSSValue* ConsumeGridTrackList(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               CSSParserMode css_parser_mode,
                               TrackListType track_list_type) {
  bool allow_grid_line_names = track_list_type != TrackListType::kGridAuto;
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  if (!allow_grid_line_names && range.Peek().GetType() == kLeftBracketToken)
    return nullptr;
  CSSGridLineNamesValue* line_names = ConsumeGridLineNames(range, context);
  if (line_names)
    values->Append(*line_names);

  bool allow_repeat = track_list_type == TrackListType::kGridTemplate;
  bool seen_auto_repeat = false;
  bool all_tracks_are_fixed_sized = true;
  do {
    bool is_auto_repeat;
    if (range.Peek().FunctionId() == CSSValueID::kRepeat) {
      if (!allow_repeat)
        return nullptr;
      if (!ConsumeGridTrackRepeatFunction(range, context, css_parser_mode,
                                          *values, is_auto_repeat,
                                          all_tracks_are_fixed_sized))
        return nullptr;
      if (is_auto_repeat && seen_auto_repeat)
        return nullptr;
      seen_auto_repeat = seen_auto_repeat || is_auto_repeat;
    } else if (CSSValue* value = ConsumeGridTrackSize(range, css_parser_mode)) {
      if (all_tracks_are_fixed_sized)
        all_tracks_are_fixed_sized = IsGridTrackFixedSized(*value);
      values->Append(*value);
    } else {
      return nullptr;
    }
    if (seen_auto_repeat && !all_tracks_are_fixed_sized)
      return nullptr;
    if (!allow_grid_line_names && range.Peek().GetType() == kLeftBracketToken)
      return nullptr;
    line_names = ConsumeGridLineNames(range, context);
    if (line_names)
      values->Append(*line_names);
  } while (!range.AtEnd() && range.Peek().GetType() != kDelimiterToken);
  return values;
}

bool ParseGridTemplateAreasRow(const String& grid_row_names,
                               NamedGridAreaMap& grid_area_map,
                               const size_t row_count,
                               size_t& column_count) {
  if (grid_row_names.ContainsOnlyWhitespaceOrEmpty())
    return false;

  Vector<String> column_names =
      ParseGridTemplateAreasColumnNames(grid_row_names);
  if (row_count == 0) {
    column_count = column_names.size();
    if (column_count == 0)
      return false;
  } else if (column_count != column_names.size()) {
    // The declaration is invalid if all the rows don't have the number of
    // columns.
    return false;
  }

  for (size_t current_column = 0; current_column < column_count;
       ++current_column) {
    const String& grid_area_name = column_names[current_column];

    // Unamed areas are always valid (we consider them to be 1x1).
    if (grid_area_name == ".")
      continue;

    size_t look_ahead_column = current_column + 1;
    while (look_ahead_column < column_count &&
           column_names[look_ahead_column] == grid_area_name)
      look_ahead_column++;

    NamedGridAreaMap::iterator grid_area_it =
        grid_area_map.find(grid_area_name);
    if (grid_area_it == grid_area_map.end()) {
      grid_area_map.insert(grid_area_name,
                           GridArea(GridSpan::TranslatedDefiniteGridSpan(
                                        row_count, row_count + 1),
                                    GridSpan::TranslatedDefiniteGridSpan(
                                        current_column, look_ahead_column)));
    } else {
      GridArea& grid_area = grid_area_it->value;

      // The following checks test that the grid area is a single filled-in
      // rectangle.
      // 1. The new row is adjacent to the previously parsed row.
      if (row_count != grid_area.rows.EndLine())
        return false;

      // 2. The new area starts at the same position as the previously parsed
      // area.
      if (current_column != grid_area.columns.StartLine())
        return false;

      // 3. The new area ends at the same position as the previously parsed
      // area.
      if (look_ahead_column != grid_area.columns.EndLine())
        return false;

      grid_area.rows = GridSpan::TranslatedDefiniteGridSpan(
          grid_area.rows.StartLine(), grid_area.rows.EndLine() + 1);
    }
    current_column = look_ahead_column - 1;
  }

  return true;
}

CSSValue* ConsumeGridTemplatesRowsOrColumns(CSSParserTokenRange& range,
                                            const CSSParserContext& context,
                                            CSSParserMode css_parser_mode) {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_property_parser_helpers::ConsumeIdent(range);
  return ConsumeGridTrackList(range, context, css_parser_mode,
                              TrackListType::kGridTemplate);
}

bool ConsumeGridItemPositionShorthand(bool important,
                                      CSSParserTokenRange& range,
                                      const CSSParserContext& context,
                                      CSSValue*& start_value,
                                      CSSValue*& end_value) {
  // Input should be nullptrs.
  DCHECK(!start_value);
  DCHECK(!end_value);

  start_value = ConsumeGridLine(range, context);
  if (!start_value)
    return false;

  if (css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range)) {
    end_value = ConsumeGridLine(range, context);
    if (!end_value)
      return false;
  } else {
    end_value = start_value->IsCustomIdentValue()
                    ? start_value
                    : CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  return range.AtEnd();
}

bool ConsumeGridTemplateShorthand(bool important,
                                  CSSParserTokenRange& range,
                                  const CSSParserContext& context,
                                  CSSValue*& template_rows,
                                  CSSValue*& template_columns,
                                  CSSValue*& template_areas) {
  DCHECK(!template_rows);
  DCHECK(!template_columns);
  DCHECK(!template_areas);

  DCHECK_EQ(gridTemplateShorthand().length(), 3u);

  CSSParserTokenRange range_copy = range;
  template_rows =
      css_property_parser_helpers::ConsumeIdent<CSSValueID::kNone>(range);

  // 1- 'none' case.
  if (template_rows && range.AtEnd()) {
    template_rows = CSSIdentifierValue::Create(CSSValueID::kNone);
    template_columns = CSSIdentifierValue::Create(CSSValueID::kNone);
    template_areas = CSSIdentifierValue::Create(CSSValueID::kNone);
    return true;
  }

  // 2- <grid-template-rows> / <grid-template-columns>
  if (!template_rows) {
    template_rows = ConsumeGridTrackList(range, context, context.Mode(),
                                         TrackListType::kGridTemplate);
  }

  if (template_rows) {
    if (!css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range))
      return false;
    template_columns =
        ConsumeGridTemplatesRowsOrColumns(range, context, context.Mode());
    if (!template_columns || !range.AtEnd())
      return false;

    template_areas = CSSIdentifierValue::Create(CSSValueID::kNone);
    return true;
  }

  // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+
  // [ / <track-list> ]?
  range = range_copy;
  return ConsumeGridTemplateRowsAndAreasAndColumns(
      important, range, context, template_rows, template_columns,
      template_areas);
}

bool ConsumeFromPageBreakBetween(CSSParserTokenRange& range,
                                 CSSValueID& value) {
  if (!ConsumeCSSValueId(range, value)) {
    return false;
  }

  if (value == CSSValueID::kAlways) {
    value = CSSValueID::kPage;
    return true;
  }
  return value == CSSValueID::kAuto || value == CSSValueID::kAvoid ||
         value == CSSValueID::kLeft || value == CSSValueID::kRight;
}

bool ConsumeFromColumnBreakBetween(CSSParserTokenRange& range,
                                   CSSValueID& value) {
  if (!ConsumeCSSValueId(range, value)) {
    return false;
  }

  if (value == CSSValueID::kAlways) {
    value = CSSValueID::kColumn;
    return true;
  }
  return value == CSSValueID::kAuto || value == CSSValueID::kAvoid;
}

bool ConsumeFromColumnOrPageBreakInside(CSSParserTokenRange& range,
                                        CSSValueID& value) {
  if (!ConsumeCSSValueId(range, value)) {
    return false;
  }
  return value == CSSValueID::kAuto || value == CSSValueID::kAvoid;
}

bool ValidWidthOrHeightKeyword(CSSValueID id, const CSSParserContext& context) {
  if (id == CSSValueID::kWebkitMinContent ||
      id == CSSValueID::kWebkitMaxContent ||
      id == CSSValueID::kWebkitFillAvailable ||
      id == CSSValueID::kWebkitFitContent || id == CSSValueID::kMinContent ||
      id == CSSValueID::kMaxContent || id == CSSValueID::kFitContent) {
    switch (id) {
      case CSSValueID::kWebkitMinContent:
        context.Count(WebFeature::kCSSValuePrefixedMinContent);
        break;
      case CSSValueID::kWebkitMaxContent:
        context.Count(WebFeature::kCSSValuePrefixedMaxContent);
        break;
      case CSSValueID::kWebkitFillAvailable:
        context.Count(WebFeature::kCSSValuePrefixedFillAvailable);
        break;
      case CSSValueID::kWebkitFitContent:
        context.Count(WebFeature::kCSSValuePrefixedFitContent);
        break;
      default:
        break;
    }
    return true;
  }
  return false;
}

CSSValue* ConsumePath(CSSParserTokenRange& range) {
  // FIXME: Add support for <url>, <basic-shape>, <geometry-box>.
  if (range.Peek().FunctionId() != CSSValueID::kPath)
    return nullptr;

  CSSParserTokenRange function_range = range;
  CSSParserTokenRange function_args =
      css_property_parser_helpers::ConsumeFunction(function_range);

  if (function_args.Peek().GetType() != kStringToken)
    return nullptr;
  StringView path_string = function_args.ConsumeIncludingWhitespace().Value();
  std::unique_ptr<SVGPathByteStream> byte_stream =
      std::make_unique<SVGPathByteStream>();
  if (BuildByteStreamFromString(path_string, *byte_stream) !=
          SVGParseStatus::kNoError ||
      !function_args.AtEnd()) {
    return nullptr;
  }

  range = function_range;
  if (byte_stream->IsEmpty())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  return MakeGarbageCollected<cssvalue::CSSPathValue>(std::move(byte_stream));
}

CSSValue* ConsumeRay(CSSParserTokenRange& range,
                     const CSSParserContext& context) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueID::kRay);
  CSSParserTokenRange function_range = range;
  CSSParserTokenRange function_args =
      css_property_parser_helpers::ConsumeFunction(function_range);

  CSSPrimitiveValue* angle = nullptr;
  CSSIdentifierValue* size = nullptr;
  CSSIdentifierValue* contain = nullptr;
  while (!function_args.AtEnd()) {
    if (!angle) {
      angle = css_property_parser_helpers::ConsumeAngle(
          function_args, &context, base::Optional<WebFeature>());
      if (angle)
        continue;
    }
    if (!size) {
      size = css_property_parser_helpers::ConsumeIdent<
          CSSValueID::kClosestSide, CSSValueID::kClosestCorner,
          CSSValueID::kFarthestSide, CSSValueID::kFarthestCorner,
          CSSValueID::kSides>(function_args);
      if (size)
        continue;
    }
    if (RuntimeEnabledFeatures::CSSOffsetPathRayContainEnabled() && !contain) {
      contain = css_property_parser_helpers::ConsumeIdent<CSSValueID::kContain>(
          function_args);
      if (contain)
        continue;
    }
    return nullptr;
  }
  if (!angle || !size)
    return nullptr;
  range = function_range;
  return MakeGarbageCollected<cssvalue::CSSRayValue>(*angle, *size, contain);
}

CSSValue* ConsumeMaxWidthOrHeight(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    css_property_parser_helpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueID::kNone ||
      ValidWidthOrHeightKeyword(range.Peek().Id(), context))
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative, unitless);
}

CSSValue* ConsumeWidthOrHeight(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    css_property_parser_helpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueID::kAuto ||
      ValidWidthOrHeightKeyword(range.Peek().Id(), context))
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative, unitless);
}

CSSValue* ConsumeMarginOrOffset(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode,
    css_property_parser_helpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeAll, unitless);
}

CSSValue* ConsumeScrollPadding(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      range, kHTMLStandardMode, kValueRangeNonNegative,
      css_property_parser_helpers::UnitlessQuirk::kForbid);
}

CSSValue* ConsumeOffsetPath(CSSParserTokenRange& range,
                            const CSSParserContext& context) {
  CSSValue* value = nullptr;
  if (RuntimeEnabledFeatures::CSSOffsetPathRayEnabled() &&
      range.Peek().FunctionId() == CSSValueID::kRay)
    value = ConsumeRay(range, context);
  else
    value = ConsumePathOrNone(range);

  // Count when we receive a valid path other than 'none'.
  if (value && !value->IsIdentifierValue())
    context.Count(WebFeature::kCSSOffsetInEffect);
  return value;
}

CSSValue* ConsumePathOrNone(CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone)
    return css_property_parser_helpers::ConsumeIdent(range);

  return ConsumePath(range);
}

CSSValue* ConsumeOffsetRotate(CSSParserTokenRange& range,
                              const CSSParserContext& context) {
  CSSValue* angle = css_property_parser_helpers::ConsumeAngle(
      range, &context, base::Optional<WebFeature>());
  CSSValue* keyword =
      css_property_parser_helpers::ConsumeIdent<CSSValueID::kAuto,
                                                CSSValueID::kReverse>(range);
  if (!angle && !keyword)
    return nullptr;

  if (!angle) {
    angle = css_property_parser_helpers::ConsumeAngle(
        range, &context, base::Optional<WebFeature>());
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (keyword)
    list->Append(*keyword);
  if (angle)
    list->Append(*angle);
  return list;
}

bool ConsumeRadii(CSSValue* horizontal_radii[4],
                  CSSValue* vertical_radii[4],
                  CSSParserTokenRange& range,
                  CSSParserMode css_parser_mode,
                  bool use_legacy_parsing) {
  unsigned horizontal_value_count = 0;
  for (; horizontal_value_count < 4 && !range.AtEnd() &&
         range.Peek().GetType() != kDelimiterToken;
       ++horizontal_value_count) {
    horizontal_radii[horizontal_value_count] =
        css_property_parser_helpers::ConsumeLengthOrPercent(
            range, css_parser_mode, kValueRangeNonNegative);
    if (!horizontal_radii[horizontal_value_count])
      return false;
  }
  if (!horizontal_radii[0])
    return false;
  if (range.AtEnd()) {
    // Legacy syntax: -webkit-border-radius: l1 l2; is equivalent to
    // border-radius: l1 / l2;
    if (use_legacy_parsing && horizontal_value_count == 2) {
      vertical_radii[0] = horizontal_radii[1];
      horizontal_radii[1] = nullptr;
    } else {
      css_property_parser_helpers::Complete4Sides(horizontal_radii);
      for (unsigned i = 0; i < 4; ++i)
        vertical_radii[i] = horizontal_radii[i];
      return true;
    }
  } else {
    if (!css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range))
      return false;
    for (unsigned i = 0; i < 4 && !range.AtEnd(); ++i) {
      vertical_radii[i] = css_property_parser_helpers::ConsumeLengthOrPercent(
          range, css_parser_mode, kValueRangeNonNegative);
      if (!vertical_radii[i])
        return false;
    }
    if (!vertical_radii[0] || !range.AtEnd())
      return false;
  }
  css_property_parser_helpers::Complete4Sides(horizontal_radii);
  css_property_parser_helpers::Complete4Sides(vertical_radii);
  return true;
}

CSSValue* ConsumeBasicShape(CSSParserTokenRange& range,
                            const CSSParserContext& context) {
  CSSValue* shape = nullptr;
  if (range.Peek().GetType() != kFunctionToken)
    return nullptr;
  CSSValueID id = range.Peek().FunctionId();
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      css_property_parser_helpers::ConsumeFunction(range_copy);
  if (id == CSSValueID::kCircle)
    shape = ConsumeBasicShapeCircle(args, context);
  else if (id == CSSValueID::kEllipse)
    shape = ConsumeBasicShapeEllipse(args, context);
  else if (id == CSSValueID::kPolygon)
    shape = ConsumeBasicShapePolygon(args, context);
  else if (id == CSSValueID::kInset)
    shape = ConsumeBasicShapeInset(args, context);
  if (!shape || !args.AtEnd())
    return nullptr;

  context.Count(WebFeature::kCSSBasicShape);
  range = range_copy;
  return shape;
}

// none | [ underline || overline || line-through || blink ]
CSSValue* ConsumeTextDecorationLine(CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone)
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSIdentifierValue* underline = nullptr;
  CSSIdentifierValue* overline = nullptr;
  CSSIdentifierValue* line_through = nullptr;
  CSSIdentifierValue* blink = nullptr;

  while (true) {
    id = range.Peek().Id();
    if (id == CSSValueID::kUnderline && !underline)
      underline = css_property_parser_helpers::ConsumeIdent(range);
    else if (id == CSSValueID::kOverline && !overline)
      overline = css_property_parser_helpers::ConsumeIdent(range);
    else if (id == CSSValueID::kLineThrough && !line_through)
      line_through = css_property_parser_helpers::ConsumeIdent(range);
    else if (id == CSSValueID::kBlink && !blink)
      blink = css_property_parser_helpers::ConsumeIdent(range);
    else
      break;
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (underline)
    list->Append(*underline);
  if (overline)
    list->Append(*overline);
  if (line_through)
    list->Append(*line_through);
  if (blink)
    list->Append(*blink);

  if (!list->length())
    return nullptr;
  return list;
}

CSSValue* ConsumeTransformValue(CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                bool use_legacy_parsing) {
  CSSValueID function_id = range.Peek().FunctionId();
  if (!IsValidCSSValueID(function_id))
    return nullptr;
  CSSParserTokenRange args =
      css_property_parser_helpers::ConsumeFunction(range);
  if (args.AtEnd())
    return nullptr;
  auto* transform_value = MakeGarbageCollected<CSSFunctionValue>(function_id);
  CSSValue* parsed_value = nullptr;
  switch (function_id) {
    case CSSValueID::kRotate:
    case CSSValueID::kRotateX:
    case CSSValueID::kRotateY:
    case CSSValueID::kRotateZ:
    case CSSValueID::kSkewX:
    case CSSValueID::kSkewY:
    case CSSValueID::kSkew:
      parsed_value = css_property_parser_helpers::ConsumeAngle(
          args, &context, WebFeature::kUnitlessZeroAngleTransform);
      if (!parsed_value)
        return nullptr;
      if (function_id == CSSValueID::kSkew &&
          css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value = css_property_parser_helpers::ConsumeAngle(
            args, &context, WebFeature::kUnitlessZeroAngleTransform);
        if (!parsed_value)
          return nullptr;
      }
      break;
    case CSSValueID::kScaleX:
    case CSSValueID::kScaleY:
    case CSSValueID::kScaleZ:
    case CSSValueID::kScale:
      parsed_value =
          css_property_parser_helpers::ConsumeNumber(args, kValueRangeAll);
      if (!parsed_value)
        return nullptr;
      if (function_id == CSSValueID::kScale &&
          css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value =
            css_property_parser_helpers::ConsumeNumber(args, kValueRangeAll);
        if (!parsed_value)
          return nullptr;
      }
      break;
    case CSSValueID::kPerspective:
      if (!ConsumePerspective(args, context, transform_value,
                              use_legacy_parsing)) {
        return nullptr;
      }
      break;
    case CSSValueID::kTranslateX:
    case CSSValueID::kTranslateY:
    case CSSValueID::kTranslate:
      parsed_value = css_property_parser_helpers::ConsumeLengthOrPercent(
          args, context.Mode(), kValueRangeAll);
      if (!parsed_value)
        return nullptr;
      if (function_id == CSSValueID::kTranslate &&
          css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value = css_property_parser_helpers::ConsumeLengthOrPercent(
            args, context.Mode(), kValueRangeAll);
        if (!parsed_value)
          return nullptr;
      }
      break;
    case CSSValueID::kTranslateZ:
      parsed_value = css_property_parser_helpers::ConsumeLength(
          args, context.Mode(), kValueRangeAll);
      break;
    case CSSValueID::kMatrix:
    case CSSValueID::kMatrix3d:
      if (!ConsumeNumbers(args, transform_value,
                          (function_id == CSSValueID::kMatrix3d) ? 16 : 6)) {
        return nullptr;
      }
      break;
    case CSSValueID::kScale3d:
      if (!ConsumeNumbers(args, transform_value, 3))
        return nullptr;
      break;
    case CSSValueID::kRotate3d:
      if (!ConsumeNumbers(args, transform_value, 3) ||
          !css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args)) {
        return nullptr;
      }
      parsed_value = css_property_parser_helpers::ConsumeAngle(
          args, &context, WebFeature::kUnitlessZeroAngleTransform);
      if (!parsed_value)
        return nullptr;
      break;
    case CSSValueID::kTranslate3d:
      if (!ConsumeTranslate3d(args, context.Mode(), transform_value))
        return nullptr;
      break;
    default:
      return nullptr;
  }
  if (parsed_value)
    transform_value->Append(*parsed_value);
  if (!args.AtEnd())
    return nullptr;
  return transform_value;
}

CSSValue* ConsumeTransformList(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               const CSSParserLocalContext& local_context) {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSValue* parsed_transform_value =
        ConsumeTransformValue(range, context, local_context.UseAliasParsing());
    if (!parsed_transform_value)
      return nullptr;
    list->Append(*parsed_transform_value);
  } while (!range.AtEnd());

  return list;
}

CSSValue* ConsumeTransitionProperty(CSSParserTokenRange& range,
                                    const CSSParserContext& context) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() != kIdentToken)
    return nullptr;
  if (token.Id() == CSSValueID::kNone)
    return css_property_parser_helpers::ConsumeIdent(range);
  CSSPropertyID unresolved_property = token.ParseAsUnresolvedCSSPropertyID();
  if (unresolved_property != CSSPropertyID::kInvalid &&
      unresolved_property != CSSPropertyID::kVariable) {
#if DCHECK_IS_ON()
    DCHECK(CSSProperty::Get(resolveCSSPropertyID(unresolved_property))
               .IsWebExposed());
#endif
    range.ConsumeIncludingWhitespace();
    return MakeGarbageCollected<CSSCustomIdentValue>(unresolved_property);
  }
  return css_property_parser_helpers::ConsumeCustomIdent(range, context);
}

bool IsValidPropertyList(const CSSValueList& value_list) {
  if (value_list.length() < 2)
    return true;
  for (auto& value : value_list) {
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value.Get());
    if (identifier_value && identifier_value->GetValueID() == CSSValueID::kNone)
      return false;
  }
  return true;
}

CSSValue* ConsumeBorderColorSide(CSSParserTokenRange& range,
                                 const CSSParserContext& context,
                                 const CSSParserLocalContext& local_context) {
  CSSPropertyID shorthand = local_context.CurrentShorthand();
  bool allow_quirky_colors = IsQuirksModeBehavior(context.Mode()) &&
                             (shorthand == CSSPropertyID::kInvalid ||
                              shorthand == CSSPropertyID::kBorderColor);
  return css_property_parser_helpers::ConsumeColor(range, context.Mode(),
                                                   allow_quirky_colors);
}

CSSValue* ConsumeBorderWidth(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode,
    css_property_parser_helpers::UnitlessQuirk unitless) {
  return css_property_parser_helpers::ConsumeLineWidth(range, css_parser_mode,
                                                       unitless);
}

CSSValue* ParseSpacing(CSSParserTokenRange& range,
                       const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNormal)
    return css_property_parser_helpers::ConsumeIdent(range);
  // TODO(timloh): allow <percentage>s in word-spacing.
  return css_property_parser_helpers::ConsumeLength(
      range, context.Mode(), kValueRangeAll,
      css_property_parser_helpers::UnitlessQuirk::kAllow);
}

CSSValue* ParsePaintStroke(CSSParserTokenRange& range,
                           const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_property_parser_helpers::ConsumeIdent(range);
  cssvalue::CSSURIValue* url =
      css_property_parser_helpers::ConsumeUrl(range, &context);
  if (url) {
    CSSValue* parsed_value = nullptr;
    if (range.Peek().Id() == CSSValueID::kNone) {
      parsed_value = css_property_parser_helpers::ConsumeIdent(range);
    } else {
      parsed_value =
          css_property_parser_helpers::ConsumeColor(range, context.Mode());
    }
    if (parsed_value) {
      CSSValueList* values = CSSValueList::CreateSpaceSeparated();
      values->Append(*url);
      values->Append(*parsed_value);
      return values;
    }
    return url;
  }
  return css_property_parser_helpers::ConsumeColor(range, context.Mode());
}

css_property_parser_helpers::UnitlessQuirk UnitlessUnlessShorthand(
    const CSSParserLocalContext& local_context) {
  return local_context.CurrentShorthand() == CSSPropertyID::kInvalid
             ? css_property_parser_helpers::UnitlessQuirk::kAllow
             : css_property_parser_helpers::UnitlessQuirk::kForbid;
}

CSSValue* ConsumeIntrinsicLength(CSSParserTokenRange& range,
                                 const CSSParserContext& context) {
  if (css_property_parser_helpers::IdentMatches<CSSValueID::kLegacy,
                                                CSSValueID::kAuto>(
          range.Peek().Id())) {
    return css_property_parser_helpers::ConsumeIdent(range);
  }
  return css_property_parser_helpers::ConsumeLength(range, context.Mode(),
                                                    kValueRangeNonNegative);
}

}  // namespace css_parsing_utils
}  // namespace blink
