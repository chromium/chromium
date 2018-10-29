// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"
#include "third_party/blink/renderer/core/css/css_border_image.h"
#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_feature_value.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_line_names_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
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
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_parsing_error.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using namespace cssvalue;

namespace CSSParsingUtils {
namespace {

bool IsLeftOrRightKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<CSSValueLeft, CSSValueRight>(
      id);
}

bool IsAuto(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<CSSValueAuto>(id);
}

bool IsNormalOrStretch(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<CSSValueNormal,
                                                CSSValueStretch>(id);
}

bool IsContentDistributionKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<
      CSSValueSpaceBetween, CSSValueSpaceAround, CSSValueSpaceEvenly,
      CSSValueStretch>(id);
}

bool IsOverflowKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<CSSValueUnsafe, CSSValueSafe>(
      id);
}

CSSIdentifierValue* ConsumeOverflowPositionKeyword(CSSParserTokenRange& range) {
  return IsOverflowKeyword(range.Peek().Id())
             ? CSSPropertyParserHelpers::ConsumeIdent(range)
             : nullptr;
}

bool IsBaselineKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<CSSValueFirst, CSSValueLast,
                                                CSSValueBaseline>(id);
}

CSSValueID GetBaselineKeyword(CSSValue& value) {
  if (!value.IsValuePair()) {
    DCHECK(ToCSSIdentifierValue(value).GetValueID() == CSSValueBaseline);
    return CSSValueBaseline;
  }

  DCHECK(ToCSSIdentifierValue(ToCSSValuePair(value).First()).GetValueID() ==
         CSSValueLast);
  DCHECK(ToCSSIdentifierValue(ToCSSValuePair(value).Second()).GetValueID() ==
         CSSValueBaseline);
  return CSSValueLastBaseline;
}

CSSValue* ConsumeBaselineKeyword(CSSParserTokenRange& range) {
  CSSIdentifierValue* preference =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueFirst, CSSValueLast>(
          range);
  CSSIdentifierValue* baseline =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueBaseline>(range);
  if (!baseline)
    return nullptr;
  if (preference && preference->GetValueID() == CSSValueLast) {
    return CSSValuePair::Create(preference, baseline,
                                CSSValuePair::kDropIdenticalValues);
  }
  return baseline;
}

CSSValue* ConsumeSteps(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueSteps);
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      CSSPropertyParserHelpers::ConsumeFunction(range_copy);

  CSSPrimitiveValue* steps =
      CSSPropertyParserHelpers::ConsumePositiveInteger(args);
  if (!steps)
    return nullptr;

  StepsTimingFunction::StepPosition position =
      StepsTimingFunction::StepPosition::END;
  if (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
    switch (args.ConsumeIncludingWhitespace().Id()) {
      case CSSValueMiddle:
        if (!RuntimeEnabledFeatures::WebAnimationsAPIEnabled())
          return nullptr;
        position = StepsTimingFunction::StepPosition::MIDDLE;
        break;
      case CSSValueStart:
        position = StepsTimingFunction::StepPosition::START;
        break;
      case CSSValueEnd:
        position = StepsTimingFunction::StepPosition::END;
        break;
      default:
        return nullptr;
    }
  }

  if (!args.AtEnd())
    return nullptr;

  range = range_copy;
  return CSSStepsTimingFunctionValue::Create(steps->GetIntValue(), position);
}

CSSValue* ConsumeFrames(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueFrames);
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      CSSPropertyParserHelpers::ConsumeFunction(range_copy);

  CSSPrimitiveValue* frames =
      CSSPropertyParserHelpers::ConsumePositiveInteger(args);
  if (!frames)
    return nullptr;

  int frames_int = frames->GetIntValue();
  if (frames_int <= 1)
    return nullptr;

  if (!args.AtEnd())
    return nullptr;

  range = range_copy;
  return CSSFramesTimingFunctionValue::Create(frames_int);
}

CSSValue* ConsumeCubicBezier(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueCubicBezier);
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      CSSPropertyParserHelpers::ConsumeFunction(range_copy);

  double x1, y1, x2, y2;
  if (CSSPropertyParserHelpers::ConsumeNumberRaw(args, x1) && x1 >= 0 &&
      x1 <= 1 &&
      CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args) &&
      CSSPropertyParserHelpers::ConsumeNumberRaw(args, y1) &&
      CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args) &&
      CSSPropertyParserHelpers::ConsumeNumberRaw(args, x2) && x2 >= 0 &&
      x2 <= 1 &&
      CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args) &&
      CSSPropertyParserHelpers::ConsumeNumberRaw(args, y2) && args.AtEnd()) {
    range = range_copy;
    return CSSCubicBezierTimingFunctionValue::Create(x1, y1, x2, y2);
  }

  return nullptr;
}

CSSIdentifierValue* ConsumeBorderImageRepeatKeyword(
    CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdent<CSSValueStretch, CSSValueRepeat,
                                                CSSValueSpace, CSSValueRound>(
      range);
}

bool ConsumeCSSValueId(CSSParserTokenRange& range, CSSValueID& value) {
  CSSIdentifierValue* keyword = CSSPropertyParserHelpers::ConsumeIdent(range);
  if (!keyword || !range.AtEnd())
    return false;
  value = keyword->GetValueID();
  return true;
}

CSSValue* ConsumeShapeRadius(CSSParserTokenRange& args,
                             CSSParserMode css_parser_mode) {
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueClosestSide,
                                             CSSValueFarthestSide>(
          args.Peek().Id()))
    return CSSPropertyParserHelpers::ConsumeIdent(args);
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      args, css_parser_mode, kValueRangeNonNegative);
}

CSSBasicShapeCircleValue* ConsumeBasicShapeCircle(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
  // circle( [<shape-radius>]? [at <position>]? )
  CSSBasicShapeCircleValue* shape = CSSBasicShapeCircleValue::Create();
  if (CSSValue* radius = ConsumeShapeRadius(args, context.Mode()))
    shape->SetRadius(radius);
  if (CSSPropertyParserHelpers::ConsumeIdent<CSSValueAt>(args)) {
    CSSValue* center_x = nullptr;
    CSSValue* center_y = nullptr;
    if (!ConsumePosition(args, context,
                         CSSPropertyParserHelpers::UnitlessQuirk::kForbid,
                         base::Optional<WebFeature>(), center_x, center_y))
      return nullptr;
    shape->SetCenterX(center_x);
    shape->SetCenterY(center_y);
  }
  return shape;
}

CSSBasicShapeEllipseValue* ConsumeBasicShapeEllipse(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
  // ellipse( [<shape-radius>{2}]? [at <position>]? )
  CSSBasicShapeEllipseValue* shape = CSSBasicShapeEllipseValue::Create();
  WebFeature feature = WebFeature::kBasicShapeEllipseNoRadius;
  if (CSSValue* radius_x = ConsumeShapeRadius(args, context.Mode())) {
    shape->SetRadiusX(radius_x);
    feature = WebFeature::kBasicShapeEllipseOneRadius;
    if (CSSValue* radius_y = ConsumeShapeRadius(args, context.Mode())) {
      shape->SetRadiusY(radius_y);
      feature = WebFeature::kBasicShapeEllipseTwoRadius;
    }
  }
  if (CSSPropertyParserHelpers::ConsumeIdent<CSSValueAt>(args)) {
    CSSValue* center_x = nullptr;
    CSSValue* center_y = nullptr;
    if (!ConsumePosition(args, context,
                         CSSPropertyParserHelpers::UnitlessQuirk::kForbid,
                         base::Optional<WebFeature>(), center_x, center_y))
      return nullptr;
    shape->SetCenterX(center_x);
    shape->SetCenterY(center_y);
  }
  context.Count(feature);
  return shape;
}

CSSBasicShapePolygonValue* ConsumeBasicShapePolygon(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  CSSBasicShapePolygonValue* shape = CSSBasicShapePolygonValue::Create();
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueEvenodd, CSSValueNonzero>(
          args.Peek().Id())) {
    shape->SetWindRule(args.ConsumeIncludingWhitespace().Id() == CSSValueEvenodd
                           ? RULE_EVENODD
                           : RULE_NONZERO);
    if (!CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args))
      return nullptr;
  }

  do {
    CSSPrimitiveValue* x_length =
        CSSPropertyParserHelpers::ConsumeLengthOrPercent(args, context.Mode(),
                                                         kValueRangeAll);
    if (!x_length)
      return nullptr;
    CSSPrimitiveValue* y_length =
        CSSPropertyParserHelpers::ConsumeLengthOrPercent(args, context.Mode(),
                                                         kValueRangeAll);
    if (!y_length)
      return nullptr;
    shape->AppendPoint(x_length, y_length);
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args));
  return shape;
}

CSSBasicShapeInsetValue* ConsumeBasicShapeInset(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  CSSBasicShapeInsetValue* shape = CSSBasicShapeInsetValue::Create();
  CSSPrimitiveValue* top = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      args, context.Mode(), kValueRangeAll);
  if (!top)
    return nullptr;
  CSSPrimitiveValue* right = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      args, context.Mode(), kValueRangeAll);
  CSSPrimitiveValue* bottom = nullptr;
  CSSPrimitiveValue* left = nullptr;
  if (right) {
    bottom = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
        args, context.Mode(), kValueRangeAll);
    if (bottom) {
      left = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
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

  if (CSSPropertyParserHelpers::ConsumeIdent<CSSValueRound>(args)) {
    CSSValue* horizontal_radii[4] = {nullptr};
    CSSValue* vertical_radii[4] = {nullptr};
    if (!ConsumeRadii(horizontal_radii, vertical_radii, args, context.Mode(),
                      false))
      return nullptr;
    shape->SetTopLeftRadius(
        CSSValuePair::Create(horizontal_radii[0], vertical_radii[0],
                             CSSValuePair::kDropIdenticalValues));
    shape->SetTopRightRadius(
        CSSValuePair::Create(horizontal_radii[1], vertical_radii[1],
                             CSSValuePair::kDropIdenticalValues));
    shape->SetBottomRightRadius(
        CSSValuePair::Create(horizontal_radii[2], vertical_radii[2],
                             CSSValuePair::kDropIdenticalValues));
    shape->SetBottomLeftRadius(
        CSSValuePair::Create(horizontal_radii[3], vertical_radii[3],
                             CSSValuePair::kDropIdenticalValues));
  }
  return shape;
}

bool ConsumeNumbers(CSSParserTokenRange& args,
                    CSSFunctionValue*& transform_value,
                    unsigned number_of_arguments) {
  do {
    CSSValue* parsed_value =
        CSSPropertyParserHelpers::ConsumeNumber(args, kValueRangeAll);
    if (!parsed_value)
      return false;
    transform_value->Append(*parsed_value);
    if (--number_of_arguments &&
        !CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
      return false;
    }
  } while (number_of_arguments);
  return true;
}

bool ConsumePerspective(CSSParserTokenRange& args,
                        const CSSParserContext& context,
                        CSSFunctionValue*& transform_value,
                        bool use_legacy_parsing) {
  CSSPrimitiveValue* parsed_value = CSSPropertyParserHelpers::ConsumeLength(
      args, context.Mode(), kValueRangeNonNegative);
  if (!parsed_value && use_legacy_parsing) {
    double perspective;
    if (!CSSPropertyParserHelpers::ConsumeNumberRaw(args, perspective) ||
        perspective < 0) {
      return false;
    }
    context.Count(WebFeature::kUnitlessPerspectiveInTransformProperty);
    parsed_value = CSSPrimitiveValue::Create(
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
    parsed_value = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
        args, css_parser_mode, kValueRangeAll);
    if (!parsed_value)
      return false;
    transform_value->Append(*parsed_value);
    if (!CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args))
      return false;
  } while (--number_of_arguments);
  parsed_value = CSSPropertyParserHelpers::ConsumeLength(args, css_parser_mode,
                                                         kValueRangeAll);
  if (!parsed_value)
    return false;
  transform_value->Append(*parsed_value);
  return true;
}

}  // namespace

bool IsSelfPositionKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<
      CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueSelfStart,
      CSSValueSelfEnd, CSSValueFlexStart, CSSValueFlexEnd>(id);
}

bool IsSelfPositionOrLeftOrRightKeyword(CSSValueID id) {
  return IsSelfPositionKeyword(id) || IsLeftOrRightKeyword(id);
}

bool IsContentPositionKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<
      CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueFlexStart,
      CSSValueFlexEnd>(id);
}

bool IsContentPositionOrLeftOrRightKeyword(CSSValueID id) {
  return IsContentPositionKeyword(id) || IsLeftOrRightKeyword(id);
}

CSSValue* ConsumeScrollOffset(CSSParserTokenRange& range) {
  range.ConsumeWhitespace();
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueAuto>(range.Peek().Id()))
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSValue* value = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
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
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (IsBaselineKeyword(id))
    return ConsumeBaselineKeyword(range);

  CSSIdentifierValue* overflow_position = ConsumeOverflowPositionKeyword(range);
  if (!is_position_keyword(range.Peek().Id()))
    return nullptr;
  CSSIdentifierValue* self_position =
      CSSPropertyParserHelpers::ConsumeIdent(range);
  if (overflow_position) {
    return CSSValuePair::Create(overflow_position, self_position,
                                CSSValuePair::kDropIdenticalValues);
  }
  return self_position;
}

CSSValue* ConsumeContentDistributionOverflowPosition(
    CSSParserTokenRange& range,
    IsPositionKeyword is_position_keyword) {
  DCHECK(is_position_keyword);
  CSSValueID id = range.Peek().Id();
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueNormal>(id)) {
    return CSSContentDistributionValue::Create(
        CSSValueInvalid, range.ConsumeIncludingWhitespace().Id(),
        CSSValueInvalid);
  }

  if (IsBaselineKeyword(id)) {
    CSSValue* baseline = ConsumeBaselineKeyword(range);
    if (!baseline)
      return nullptr;
    return CSSContentDistributionValue::Create(
        CSSValueInvalid, GetBaselineKeyword(*baseline), CSSValueInvalid);
  }

  if (IsContentDistributionKeyword(id)) {
    return CSSContentDistributionValue::Create(
        range.ConsumeIncludingWhitespace().Id(), CSSValueInvalid,
        CSSValueInvalid);
  }

  CSSValueID overflow = IsOverflowKeyword(id)
                            ? range.ConsumeIncludingWhitespace().Id()
                            : CSSValueInvalid;
  if (is_position_keyword(range.Peek().Id())) {
    return CSSContentDistributionValue::Create(
        CSSValueInvalid, range.ConsumeIncludingWhitespace().Id(), overflow);
  }

  return nullptr;
}

CSSValue* ConsumeAnimationIterationCount(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueInfinite)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
}

CSSValue* ConsumeAnimationName(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               bool allow_quoted_name) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (allow_quoted_name && range.Peek().GetType() == kStringToken) {
    // Legacy support for strings in prefixed animations.
    context.Count(WebFeature::kQuotedAnimationName);

    const CSSParserToken& token = range.ConsumeIncludingWhitespace();
    if (EqualIgnoringASCIICase(token.Value(), "none"))
      return CSSIdentifierValue::Create(CSSValueNone);
    return CSSCustomIdentValue::Create(token.Value().ToAtomicString());
  }

  return CSSPropertyParserHelpers::ConsumeCustomIdent(range);
}

CSSValue* ConsumeAnimationTimingFunction(CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueEase || id == CSSValueLinear || id == CSSValueEaseIn ||
      id == CSSValueEaseOut || id == CSSValueEaseInOut ||
      id == CSSValueStepStart || id == CSSValueStepEnd ||
      id == CSSValueStepMiddle)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueID function = range.Peek().FunctionId();
  if (function == CSSValueSteps)
    return ConsumeSteps(range);
  if (RuntimeEnabledFeatures::FramesTimingFunctionEnabled() &&
      function == CSSValueFrames) {
    return ConsumeFrames(range);
  }
  if (function == CSSValueCubicBezier)
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
            *ToLonghand(shorthand.properties()[i])->InitialValue());
      }
      parsed_longhand[i] = false;
    }
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));

  return true;
}

void AddBackgroundValue(CSSValue*& list, CSSValue* value) {
  if (list) {
    if (!list->IsBaseValueList()) {
      CSSValue* first_value = list;
      list = CSSValueList::CreateCommaSeparated();
      ToCSSValueList(list)->Append(*first_value);
    }
    ToCSSValueList(list)->Append(*value);
  } else {
    // To conserve memory we don't actually wrap a single value in a list.
    list = value;
  }
}

CSSValue* ConsumeBackgroundAttachment(CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdent<CSSValueScroll, CSSValueFixed,
                                                CSSValueLocal>(range);
}

CSSValue* ConsumeBackgroundBlendMode(CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNormal || id == CSSValueOverlay ||
      (id >= CSSValueMultiply && id <= CSSValueLuminosity))
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return nullptr;
}

CSSValue* ConsumeBackgroundBox(CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueBorderBox, CSSValuePaddingBox, CSSValueContentBox>(range);
}

CSSValue* ConsumeBackgroundComposite(CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdentRange(range, CSSValueClear,
                                                     CSSValuePlusLighter);
}

CSSValue* ConsumeMaskSourceType(CSSParserTokenRange& range) {
  DCHECK(RuntimeEnabledFeatures::CSSMaskSourceTypeEnabled());
  return CSSPropertyParserHelpers::ConsumeIdent<CSSValueAuto, CSSValueAlpha,
                                                CSSValueLuminance>(range);
}

CSSPrimitiveValue* ConsumeLengthOrPercentCountNegative(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    base::Optional<WebFeature> negative_size) {
  CSSPrimitiveValue* result =
      ConsumeLengthOrPercent(range, context.Mode(), kValueRangeNonNegative,
                             CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
  if (!result && negative_size)
    context.Count(*negative_size);
  return result;
}

CSSValue* ConsumeBackgroundSize(CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                base::Optional<WebFeature> negative_size,
                                ParsingStyle parsing_style) {
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueContain, CSSValueCover>(
          range.Peek().Id())) {
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  }

  CSSValue* horizontal =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueAuto>(range);
  if (!horizontal) {
    horizontal =
        ConsumeLengthOrPercentCountNegative(range, context, negative_size);
  }
  if (!horizontal)
    return nullptr;

  CSSValue* vertical = nullptr;
  if (!range.AtEnd()) {
    if (range.Peek().Id() == CSSValueAuto) {  // `auto' is the default
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
  return CSSValuePair::Create(horizontal, vertical,
                              CSSValuePair::kKeepIdenticalValues);
}

bool ConsumeBackgroundPosition(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               CSSPropertyParserHelpers::UnitlessQuirk unitless,
                               CSSValue*& result_x,
                               CSSValue*& result_y) {
  do {
    CSSValue* position_x = nullptr;
    CSSValue* position_y = nullptr;
    if (!CSSPropertyParserHelpers::ConsumePosition(
            range, context, unitless,
            WebFeature::kThreeValuedPositionBackground, position_x, position_y))
      return false;
    AddBackgroundValue(result_x, position_x);
    AddBackgroundValue(result_y, position_y);
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
  return true;
}

CSSValue* ConsumePrefixedBackgroundBox(CSSParserTokenRange& range,
                                       AllowTextValue allow_text_value) {
  // The values 'border', 'padding' and 'content' are deprecated and do not
  // apply to the version of the property that has the -webkit- prefix removed.
  if (CSSValue* value = CSSPropertyParserHelpers::ConsumeIdentRange(
          range, CSSValueBorder, CSSValuePaddingBox))
    return value;
  if (allow_text_value == AllowTextValue::kAllow &&
      range.Peek().Id() == CSSValueText)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return nullptr;
}

CSSValue* ParseBackgroundBox(CSSParserTokenRange& range,
                             const CSSParserLocalContext& local_context,
                             AllowTextValue alias_allow_text_value) {
  // This is legacy behavior that does not match spec, see crbug.com/604023
  if (local_context.UseAliasParsing()) {
    return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
        ConsumePrefixedBackgroundBox, range, alias_allow_text_value);
  }
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      ConsumeBackgroundBox, range);
}

CSSValue* ParseBackgroundOrMaskSize(CSSParserTokenRange& range,
                                    const CSSParserContext& context,
                                    const CSSParserLocalContext& local_context,
                                    base::Optional<WebFeature> negative_size) {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      ConsumeBackgroundSize, range, context, negative_size,
      local_context.UseAliasParsing() ? ParsingStyle::kLegacy
                                      : ParsingStyle::kNotLegacy);
}

namespace {

CSSValue* ConsumeBackgroundComponent(CSSPropertyID resolved_property,
                                     CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  switch (resolved_property) {
    case CSSPropertyBackgroundClip:
      return ConsumeBackgroundBox(range);
    case CSSPropertyBackgroundAttachment:
      return ConsumeBackgroundAttachment(range);
    case CSSPropertyBackgroundOrigin:
      return ConsumeBackgroundBox(range);
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitMaskImage:
      return CSSPropertyParserHelpers::ConsumeImageOrNone(range, &context);
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyWebkitMaskPositionX:
      return ConsumePositionLonghand<CSSValueLeft, CSSValueRight>(
          range, context.Mode());
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionY:
      return ConsumePositionLonghand<CSSValueTop, CSSValueBottom>(
          range, context.Mode());
    case CSSPropertyBackgroundSize:
      return ConsumeBackgroundSize(range, context,
                                   WebFeature::kNegativeBackgroundSize,
                                   ParsingStyle::kNotLegacy);
    case CSSPropertyWebkitMaskSize:
      return ConsumeBackgroundSize(range, context,
                                   WebFeature::kNegativeMaskSize,
                                   ParsingStyle::kNotLegacy);
    case CSSPropertyBackgroundColor:
      return CSSPropertyParserHelpers::ConsumeColor(range, context.Mode());
    case CSSPropertyWebkitMaskClip:
      return ConsumePrefixedBackgroundBox(range, AllowTextValue::kAllow);
    case CSSPropertyWebkitMaskOrigin:
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
  DCHECK(shorthand_id == CSSPropertyBackground ||
         shorthand_id == CSSPropertyWebkitMask);
  const StylePropertyShorthand& shorthand =
      shorthand_id == CSSPropertyBackground ? backgroundShorthand()
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
      for (unsigned i = 0; i < longhand_count; ++i) {
        if (parsed_longhand[i])
          continue;

        CSSValue* value = nullptr;
        CSSValue* value_y = nullptr;
        const CSSProperty& property = *shorthand.properties()[i];
        if (property.IDEquals(CSSPropertyBackgroundRepeatX) ||
            property.IDEquals(CSSPropertyWebkitMaskRepeatX)) {
          ConsumeRepeatStyleComponent(range, value, value_y, implicit);
        } else if (property.IDEquals(CSSPropertyBackgroundPositionX) ||
                   property.IDEquals(CSSPropertyWebkitMaskPositionX)) {
          if (!CSSPropertyParserHelpers::ConsumePosition(
                  range, context,
                  CSSPropertyParserHelpers::UnitlessQuirk::kForbid,
                  WebFeature::kThreeValuedPositionBackground, value, value_y))
            continue;
        } else if (property.IDEquals(CSSPropertyBackgroundSize) ||
                   property.IDEquals(CSSPropertyWebkitMaskSize)) {
          if (!CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range))
            continue;
          value =
              ConsumeBackgroundSize(range, context,
                                    property.IDEquals(CSSPropertyBackgroundSize)
                                        ? WebFeature::kNegativeBackgroundSize
                                        : WebFeature::kNegativeMaskSize,
                                    ParsingStyle::kNotLegacy);
          if (!value ||
              !parsed_longhand[i - 1])  // Position must have been
                                        // parsed in the current layer.
          {
            return false;
          }
        } else if (property.IDEquals(CSSPropertyBackgroundPositionY) ||
                   property.IDEquals(CSSPropertyBackgroundRepeatY) ||
                   property.IDEquals(CSSPropertyWebkitMaskPositionY) ||
                   property.IDEquals(CSSPropertyWebkitMaskRepeatY)) {
          continue;
        } else {
          value =
              ConsumeBackgroundComponent(property.PropertyID(), range, context);
        }
        if (value) {
          if (property.IDEquals(CSSPropertyBackgroundOrigin) ||
              property.IDEquals(CSSPropertyWebkitMaskOrigin)) {
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
      if (property.IDEquals(CSSPropertyBackgroundColor) && !range.AtEnd()) {
        if (parsed_longhand[i])
          return false;  // Colors are only allowed in the last layer.
        continue;
      }
      if ((property.IDEquals(CSSPropertyBackgroundClip) ||
           property.IDEquals(CSSPropertyWebkitMaskClip)) &&
          !parsed_longhand[i] && origin_value) {
        AddBackgroundValue(longhands[i], origin_value);
        continue;
      }
      if (!parsed_longhand[i]) {
        AddBackgroundValue(longhands[i], CSSInitialValue::Create());
      }
    }
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
  if (!range.AtEnd())
    return false;

  for (unsigned i = 0; i < longhand_count; ++i) {
    const CSSProperty& property = *shorthand.properties()[i];
    if (property.IDEquals(CSSPropertyBackgroundSize) && longhands[i] &&
        context.UseLegacyBackgroundSizeShorthandBehavior())
      continue;
    CSSPropertyParserHelpers::AddProperty(
        property.PropertyID(), shorthand.id(), *longhands[i], important,
        implicit ? CSSPropertyParserHelpers::IsImplicitProperty::kImplicit
                 : CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
  }
  return true;
}

bool ConsumeRepeatStyleComponent(CSSParserTokenRange& range,
                                 CSSValue*& value1,
                                 CSSValue*& value2,
                                 bool& implicit) {
  if (CSSPropertyParserHelpers::ConsumeIdent<CSSValueRepeatX>(range)) {
    value1 = CSSIdentifierValue::Create(CSSValueRepeat);
    value2 = CSSIdentifierValue::Create(CSSValueNoRepeat);
    implicit = true;
    return true;
  }
  if (CSSPropertyParserHelpers::ConsumeIdent<CSSValueRepeatY>(range)) {
    value1 = CSSIdentifierValue::Create(CSSValueNoRepeat);
    value2 = CSSIdentifierValue::Create(CSSValueRepeat);
    implicit = true;
    return true;
  }
  value1 = CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
  if (!value1)
    return false;

  value2 = CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
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
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
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
      source = CSSPropertyParserHelpers::ConsumeImageOrNone(range, &context);
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
        if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
          width = ConsumeBorderImageWidth(range);
          if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(
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
  return CSSValuePair::Create(horizontal, vertical,
                              CSSValuePair::kDropIdenticalValues);
}

CSSValue* ConsumeBorderImageSlice(CSSParserTokenRange& range,
                                  DefaultFill default_fill) {
  bool fill = CSSPropertyParserHelpers::ConsumeIdent<CSSValueFill>(range);
  CSSValue* slices[4] = {nullptr};

  for (size_t index = 0; index < 4; ++index) {
    CSSPrimitiveValue* value =
        CSSPropertyParserHelpers::ConsumePercent(range, kValueRangeNonNegative);
    if (!value) {
      value = CSSPropertyParserHelpers::ConsumeNumber(range,
                                                      kValueRangeNonNegative);
    }
    if (!value)
      break;
    slices[index] = value;
  }
  if (!slices[0])
    return nullptr;
  if (CSSPropertyParserHelpers::ConsumeIdent<CSSValueFill>(range)) {
    if (fill)
      return nullptr;
    fill = true;
  }
  CSSPropertyParserHelpers::Complete4Sides(slices);
  if (default_fill == DefaultFill::kFill)
    fill = true;
  return CSSBorderImageSliceValue::Create(
      CSSQuadValue::Create(slices[0], slices[1], slices[2], slices[3],
                           CSSQuadValue::kSerializeAsQuad),
      fill);
}

CSSValue* ConsumeBorderImageWidth(CSSParserTokenRange& range) {
  CSSValue* widths[4] = {nullptr};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value =
        CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
    if (!value) {
      value = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
          range, kHTMLStandardMode, kValueRangeNonNegative,
          CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
    }
    if (!value)
      value = CSSPropertyParserHelpers::ConsumeIdent<CSSValueAuto>(range);
    if (!value)
      break;
    widths[index] = value;
  }
  if (!widths[0])
    return nullptr;
  CSSPropertyParserHelpers::Complete4Sides(widths);
  return CSSQuadValue::Create(widths[0], widths[1], widths[2], widths[3],
                              CSSQuadValue::kSerializeAsQuad);
}

CSSValue* ConsumeBorderImageOutset(CSSParserTokenRange& range) {
  CSSValue* outsets[4] = {nullptr};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value =
        CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
    if (!value) {
      value = CSSPropertyParserHelpers::ConsumeLength(range, kHTMLStandardMode,
                                                      kValueRangeNonNegative);
    }
    if (!value)
      break;
    outsets[index] = value;
  }
  if (!outsets[0])
    return nullptr;
  CSSPropertyParserHelpers::Complete4Sides(outsets);
  return CSSQuadValue::Create(outsets[0], outsets[1], outsets[2], outsets[3],
                              CSSQuadValue::kSerializeAsQuad);
}

CSSValue* ParseBorderRadiusCorner(CSSParserTokenRange& range,
                                  const CSSParserContext& context) {
  CSSValue* parsed_value1 = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
  if (!parsed_value1)
    return nullptr;
  CSSValue* parsed_value2 = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
  if (!parsed_value2)
    parsed_value2 = parsed_value1;
  return CSSValuePair::Create(parsed_value1, parsed_value2,
                              CSSValuePair::kDropIdenticalValues);
}

CSSValue* ParseBorderWidthSide(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               const CSSParserLocalContext& local_context) {
  CSSPropertyID shorthand = local_context.CurrentShorthand();
  bool allow_quirky_lengths =
      IsQuirksModeBehavior(context.Mode()) &&
      (shorthand == CSSPropertyInvalid || shorthand == CSSPropertyBorderWidth);
  CSSPropertyParserHelpers::UnitlessQuirk unitless =
      allow_quirky_lengths ? CSSPropertyParserHelpers::UnitlessQuirk::kAllow
                           : CSSPropertyParserHelpers::UnitlessQuirk::kForbid;
  return ConsumeBorderWidth(range, context.Mode(), unitless);
}

CSSValue* ConsumeShadow(CSSParserTokenRange& range,
                        CSSParserMode css_parser_mode,
                        AllowInsetAndSpread inset_and_spread) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      ParseSingleShadow, range, css_parser_mode, inset_and_spread);
}

CSSShadowValue* ParseSingleShadow(CSSParserTokenRange& range,
                                  CSSParserMode css_parser_mode,
                                  AllowInsetAndSpread inset_and_spread) {
  CSSIdentifierValue* style = nullptr;
  CSSValue* color = nullptr;

  if (range.AtEnd())
    return nullptr;

  color = CSSPropertyParserHelpers::ConsumeColor(range, css_parser_mode);
  if (range.Peek().Id() == CSSValueInset) {
    if (inset_and_spread != AllowInsetAndSpread::kAllow)
      return nullptr;
    style = CSSPropertyParserHelpers::ConsumeIdent(range);
    if (!color)
      color = CSSPropertyParserHelpers::ConsumeColor(range, css_parser_mode);
  }

  CSSPrimitiveValue* horizontal_offset =
      CSSPropertyParserHelpers::ConsumeLength(range, css_parser_mode,
                                              kValueRangeAll);
  if (!horizontal_offset)
    return nullptr;

  CSSPrimitiveValue* vertical_offset = CSSPropertyParserHelpers::ConsumeLength(
      range, css_parser_mode, kValueRangeAll);
  if (!vertical_offset)
    return nullptr;

  CSSPrimitiveValue* blur_radius = CSSPropertyParserHelpers::ConsumeLength(
      range, css_parser_mode, kValueRangeAll);
  CSSPrimitiveValue* spread_distance = nullptr;
  if (blur_radius) {
    // Blur radius must be non-negative.
    if (blur_radius->GetDoubleValue() < 0)
      return nullptr;
    if (inset_and_spread == AllowInsetAndSpread::kAllow) {
      spread_distance = CSSPropertyParserHelpers::ConsumeLength(
          range, css_parser_mode, kValueRangeAll);
    }
  }

  if (!range.AtEnd()) {
    if (!color)
      color = CSSPropertyParserHelpers::ConsumeColor(range, css_parser_mode);
    if (range.Peek().Id() == CSSValueInset) {
      if (inset_and_spread != AllowInsetAndSpread::kAllow || style)
        return nullptr;
      style = CSSPropertyParserHelpers::ConsumeIdent(range);
      if (!color)
        color = CSSPropertyParserHelpers::ConsumeColor(range, css_parser_mode);
    }
  }
  return CSSShadowValue::Create(horizontal_offset, vertical_offset, blur_radius,
                                spread_distance, style, color);
}

CSSValue* ConsumeColumnCount(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumePositiveInteger(range);
}

CSSValue* ConsumeColumnWidth(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  // Always parse lengths in strict mode here, since it would be ambiguous
  // otherwise when used in the 'columns' shorthand property.
  CSSPrimitiveValue* column_width = CSSPropertyParserHelpers::ConsumeLength(
      range, kHTMLStandardMode, kValueRangeNonNegative);
  if (!column_width)
    return nullptr;
  return column_width;
}

bool ConsumeColumnWidthOrCount(CSSParserTokenRange& range,
                               CSSValue*& column_width,
                               CSSValue*& column_count) {
  if (range.Peek().Id() == CSSValueAuto) {
    CSSPropertyParserHelpers::ConsumeIdent(range);
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
  if (range.Peek().Id() == CSSValueNormal)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
}

CSSValue* ConsumeCounter(CSSParserTokenRange& range, int default_value) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSCustomIdentValue* counter_name =
        CSSPropertyParserHelpers::ConsumeCustomIdent(range);
    if (!counter_name)
      return nullptr;
    int value = default_value;
    if (CSSPrimitiveValue* counter_value =
            CSSPropertyParserHelpers::ConsumeInteger(range))
      value = clampTo<int>(counter_value->GetDoubleValue());
    list->Append(*CSSValuePair::Create(
        counter_name,
        CSSPrimitiveValue::Create(value, CSSPrimitiveValue::UnitType::kInteger),
        CSSValuePair::kDropIdenticalValues));
  } while (!range.AtEnd());
  return list;
}

CSSValue* ConsumeFontSize(CSSParserTokenRange& range,
                          CSSParserMode css_parser_mode,
                          CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() >= CSSValueXxSmall &&
      range.Peek().Id() <= CSSValueLarger)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeNonNegative, unitless);
}

CSSValue* ConsumeLineHeight(CSSParserTokenRange& range,
                            CSSParserMode css_parser_mode) {
  if (range.Peek().Id() == CSSValueNormal)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSPrimitiveValue* line_height =
      CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
  if (line_height)
    return line_height;
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
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
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
  return list;
}

CSSValue* ConsumeGenericFamily(CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdentRange(range, CSSValueSerif,
                                                     CSSValueWebkitBody);
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
      (CSSPropertyParserHelpers::IsCSSWideKeyword(first_token.Value()) ||
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
  if (range.Peek().Id() == CSSValueNormal ||
      range.Peek().Id() == CSSValueItalic)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (range.Peek().Id() != CSSValueOblique)
    return nullptr;

  CSSIdentifierValue* oblique_identifier =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueOblique>(range);

  CSSPrimitiveValue* start_angle =
      CSSPropertyParserHelpers::ConsumeAngle(range, nullptr, base::nullopt);
  if (!start_angle)
    return oblique_identifier;
  if (!IsAngleWithinLimits(start_angle))
    return nullptr;

  if (parser_mode != kCSSFontFaceRuleMode || range.AtEnd()) {
    CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
    value_list->Append(*start_angle);
    return CSSFontStyleRangeValue::Create(*oblique_identifier, *value_list);
  }

  CSSPrimitiveValue* end_angle =
      CSSPropertyParserHelpers::ConsumeAngle(range, nullptr, base::nullopt);
  if (!end_angle || !IsAngleWithinLimits(end_angle))
    return nullptr;

  CSSValueList* range_list = CombineToRangeListOrNull(start_angle, end_angle);
  if (!range_list)
    return nullptr;
  return CSSFontStyleRangeValue::Create(*oblique_identifier, *range_list);
}

CSSIdentifierValue* ConsumeFontStretchKeywordOnly(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  if (token.Id() == CSSValueNormal || (token.Id() >= CSSValueUltraCondensed &&
                                       token.Id() <= CSSValueUltraExpanded))
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return nullptr;
}

CSSValue* ConsumeFontStretch(CSSParserTokenRange& range,
                             const CSSParserMode& parser_mode) {
  CSSIdentifierValue* parsed_keyword = ConsumeFontStretchKeywordOnly(range);
  if (parsed_keyword)
    return parsed_keyword;

  CSSPrimitiveValue* start_percent =
      CSSPropertyParserHelpers::ConsumePercent(range, kValueRangeNonNegative);
  if (!start_percent)
    return nullptr;

  // In a non-font-face context, more than one percentage is not allowed.
  if (parser_mode != kCSSFontFaceRuleMode || range.AtEnd())
    return start_percent;

  CSSPrimitiveValue* end_percent =
      CSSPropertyParserHelpers::ConsumePercent(range, kValueRangeNonNegative);
  if (!end_percent)
    return nullptr;

  return CombineToRangeListOrNull(start_percent, end_percent);
}

CSSValue* ConsumeFontWeight(CSSParserTokenRange& range,
                            const CSSParserMode& parser_mode) {
  const CSSParserToken& token = range.Peek();
  if (token.Id() >= CSSValueNormal && token.Id() <= CSSValueLighter)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

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
      CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
  if (!start_weight || start_weight->GetFloatValue() < 1 ||
      start_weight->GetFloatValue() > 1000)
    return nullptr;

  // In a non-font-face context, more than one number is not allowed. Return
  // what we have. If there is trailing garbage, the AtEnd() check in
  // CSSPropertyParser::ParseValueStart will catch that.
  if (parser_mode != kCSSFontFaceRuleMode || range.AtEnd())
    return start_weight;

  CSSPrimitiveValue* end_weight =
      CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
  if (!end_weight || end_weight->GetFloatValue() < 1 ||
      end_weight->GetFloatValue() > 1000)
    return nullptr;

  return CombineToRangeListOrNull(start_weight, end_weight);
}

CSSValue* ConsumeFontFeatureSettings(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueNormal)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSValueList* settings = CSSValueList::CreateCommaSeparated();
  do {
    CSSFontFeatureValue* font_feature_value = ConsumeFontFeatureTag(range);
    if (!font_feature_value)
      return nullptr;
    settings->Append(*font_feature_value);
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
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
          CSSPropertyParserHelpers::ConsumeInteger(range, 0)) {
    tag_value = clampTo<int>(value->GetDoubleValue());
  } else if (range.Peek().Id() == CSSValueOn ||
             range.Peek().Id() == CSSValueOff) {
    tag_value = range.ConsumeIncludingWhitespace().Id() == CSSValueOn;
  }
  return CSSFontFeatureValue::Create(tag, tag_value);
}

CSSIdentifierValue* ConsumeFontVariantCSS21(CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdent<CSSValueNormal,
                                                CSSValueSmallCaps>(range);
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
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueMinContent,
                                             CSSValueMaxContent, CSSValueAuto>(
          token.Id()))
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  if (token.GetType() == kDimensionToken &&
      token.GetUnitType() == CSSPrimitiveValue::UnitType::kFraction) {
    if (range.Peek().NumericValue() < 0)
      return nullptr;
    return CSSPrimitiveValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kFraction);
  }
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeNonNegative,
      CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
}

CSSValue* ConsumeFitContent(CSSParserTokenRange& range,
                            CSSParserMode css_parser_mode) {
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      CSSPropertyParserHelpers::ConsumeFunction(range_copy);
  CSSPrimitiveValue* length = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      args, css_parser_mode, kValueRangeNonNegative,
      CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
  if (!length || !args.AtEnd())
    return nullptr;
  range = range_copy;
  CSSFunctionValue* result = CSSFunctionValue::Create(CSSValueFitContent);
  result->Append(*length);
  return result;
}

bool IsGridBreadthFixedSized(const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    CSSValueID value_id = ToCSSIdentifierValue(value).GetValueID();
    return !(value_id == CSSValueMinContent || value_id == CSSValueMaxContent ||
             value_id == CSSValueAuto);
  }

  if (value.IsPrimitiveValue()) {
    return !ToCSSPrimitiveValue(value).IsFlex();
  }

  NOTREACHED();
  return true;
}

bool IsGridTrackFixedSized(const CSSValue& value) {
  if (value.IsPrimitiveValue() || value.IsIdentifierValue())
    return IsGridBreadthFixedSized(value);

  DCHECK(value.IsFunctionValue());
  auto& function = ToCSSFunctionValue(value);
  if (function.FunctionType() == CSSValueFitContent)
    return false;

  const CSSValue& min_value = function.Item(0);
  const CSSValue& max_value = function.Item(1);
  return IsGridBreadthFixedSized(min_value) ||
         IsGridBreadthFixedSized(max_value);
}

CSSValue* ConsumeGridTrackSize(CSSParserTokenRange& range,
                               CSSParserMode css_parser_mode) {
  const CSSParserToken& token = range.Peek();
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueAuto>(token.Id()))
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (token.FunctionId() == CSSValueMinmax) {
    CSSParserTokenRange range_copy = range;
    CSSParserTokenRange args =
        CSSPropertyParserHelpers::ConsumeFunction(range_copy);
    CSSValue* min_track_breadth = ConsumeGridBreadth(args, css_parser_mode);
    if (!min_track_breadth ||
        (min_track_breadth->IsPrimitiveValue() &&
         ToCSSPrimitiveValue(min_track_breadth)->IsFlex()) ||
        !CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args))
      return nullptr;
    CSSValue* max_track_breadth = ConsumeGridBreadth(args, css_parser_mode);
    if (!max_track_breadth || !args.AtEnd())
      return nullptr;
    range = range_copy;
    CSSFunctionValue* result = CSSFunctionValue::Create(CSSValueMinmax);
    result->Append(*min_track_breadth);
    result->Append(*max_track_breadth);
    return result;
  }

  if (token.FunctionId() == CSSValueFitContent)
    return ConsumeFitContent(range, css_parser_mode);

  return ConsumeGridBreadth(range, css_parser_mode);
}

CSSCustomIdentValue* ConsumeCustomIdentForGridLine(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueAuto || range.Peek().Id() == CSSValueSpan ||
      range.Peek().Id() == CSSValueDefault)
    return nullptr;
  return CSSPropertyParserHelpers::ConsumeCustomIdent(range);
}

// Appends to the passed in CSSGridLineNamesValue if any, otherwise creates a
// new one.
CSSGridLineNamesValue* ConsumeGridLineNames(
    CSSParserTokenRange& range,
    CSSGridLineNamesValue* line_names = nullptr) {
  CSSParserTokenRange range_copy = range;
  if (range_copy.ConsumeIncludingWhitespace().GetType() != kLeftBracketToken)
    return nullptr;
  if (!line_names)
    line_names = CSSGridLineNamesValue::Create();
  while (CSSCustomIdentValue* line_name =
             ConsumeCustomIdentForGridLine(range_copy))
    line_names->Append(*line_name);
  if (range_copy.ConsumeIncludingWhitespace().GetType() != kRightBracketToken)
    return nullptr;
  range = range_copy;
  return line_names;
}

bool ConsumeGridTrackRepeatFunction(CSSParserTokenRange& range,
                                    CSSParserMode css_parser_mode,
                                    CSSValueList& list,
                                    bool& is_auto_repeat,
                                    bool& all_tracks_are_fixed_sized) {
  CSSParserTokenRange args = CSSPropertyParserHelpers::ConsumeFunction(range);
  // The number of repetitions for <auto-repeat> is not important at parsing
  // level because it will be computed later, let's set it to 1.
  size_t repetitions = 1;
  is_auto_repeat =
      CSSPropertyParserHelpers::IdentMatches<CSSValueAutoFill, CSSValueAutoFit>(
          args.Peek().Id());
  CSSValueList* repeated_values;
  if (is_auto_repeat) {
    repeated_values =
        CSSGridAutoRepeatValue::Create(args.ConsumeIncludingWhitespace().Id());
  } else {
    // TODO(rob.buis): a consumeIntegerRaw would be more efficient here.
    CSSPrimitiveValue* repetition =
        CSSPropertyParserHelpers::ConsumePositiveInteger(args);
    if (!repetition)
      return false;
    repetitions =
        clampTo<size_t>(repetition->GetDoubleValue(), 0, kGridMaxTracks);
    repeated_values = CSSValueList::CreateSpaceSeparated();
  }
  if (!CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args))
    return false;
  CSSGridLineNamesValue* line_names = ConsumeGridLineNames(args);
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
    line_names = ConsumeGridLineNames(args);
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
    for (size_t i = 0; i < repetitions; ++i) {
      for (size_t j = 0; j < repeated_values->length(); ++j)
        list.Append(repeated_values->Item(j));
    }
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
    line_names = ConsumeGridLineNames(range, line_names);
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
      value = CSSIdentifierValue::Create(CSSValueAuto);
    template_rows_value_list->Append(*value);

    // This will handle the trailing/leading <custom-ident>* in the grammar.
    line_names = ConsumeGridLineNames(range);
    if (line_names)
      template_rows_value_list->Append(*line_names);
  } while (!range.AtEnd() && !(range.Peek().GetType() == kDelimiterToken &&
                               range.Peek().Delimiter() == '/'));

  if (!range.AtEnd()) {
    if (!CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range))
      return false;
    template_columns = ConsumeGridTrackList(
        range, context.Mode(), TrackListType::kGridTemplateNoRepeat);
    if (!template_columns || !range.AtEnd())
      return false;
  } else {
    template_columns = CSSIdentifierValue::Create(CSSValueNone);
  }

  template_rows = template_rows_value_list;
  template_areas =
      CSSGridTemplateAreasValue::Create(grid_area_map, row_count, column_count);
  return true;
}

CSSValue* ConsumeGridLine(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSIdentifierValue* span_value = nullptr;
  CSSCustomIdentValue* grid_line_name = nullptr;
  CSSPrimitiveValue* numeric_value =
      CSSPropertyParserHelpers::ConsumeInteger(range);
  if (numeric_value) {
    grid_line_name = ConsumeCustomIdentForGridLine(range);
    span_value = CSSPropertyParserHelpers::ConsumeIdent<CSSValueSpan>(range);
  } else {
    span_value = CSSPropertyParserHelpers::ConsumeIdent<CSSValueSpan>(range);
    if (span_value) {
      numeric_value = CSSPropertyParserHelpers::ConsumeInteger(range);
      grid_line_name = ConsumeCustomIdentForGridLine(range);
      if (!numeric_value)
        numeric_value = CSSPropertyParserHelpers::ConsumeInteger(range);
    } else {
      grid_line_name = ConsumeCustomIdentForGridLine(range);
      if (grid_line_name) {
        numeric_value = CSSPropertyParserHelpers::ConsumeInteger(range);
        span_value =
            CSSPropertyParserHelpers::ConsumeIdent<CSSValueSpan>(range);
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
    numeric_value = CSSPrimitiveValue::Create(
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
                               CSSParserMode css_parser_mode,
                               TrackListType track_list_type) {
  bool allow_grid_line_names = track_list_type != TrackListType::kGridAuto;
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  CSSGridLineNamesValue* line_names = ConsumeGridLineNames(range);
  if (line_names) {
    if (!allow_grid_line_names)
      return nullptr;
    values->Append(*line_names);
  }

  bool allow_repeat = track_list_type == TrackListType::kGridTemplate;
  bool seen_auto_repeat = false;
  bool all_tracks_are_fixed_sized = true;
  do {
    bool is_auto_repeat;
    if (range.Peek().FunctionId() == CSSValueRepeat) {
      if (!allow_repeat)
        return nullptr;
      if (!ConsumeGridTrackRepeatFunction(range, css_parser_mode, *values,
                                          is_auto_repeat,
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
    line_names = ConsumeGridLineNames(range);
    if (line_names) {
      if (!allow_grid_line_names)
        return nullptr;
      values->Append(*line_names);
    }
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
                                            CSSParserMode css_parser_mode) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return ConsumeGridTrackList(range, css_parser_mode,
                              TrackListType::kGridTemplate);
}

bool ConsumeGridItemPositionShorthand(bool important,
                                      CSSParserTokenRange& range,
                                      CSSValue*& start_value,
                                      CSSValue*& end_value) {
  // Input should be nullptrs.
  DCHECK(!start_value);
  DCHECK(!end_value);

  start_value = ConsumeGridLine(range);
  if (!start_value)
    return false;

  if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
    end_value = ConsumeGridLine(range);
    if (!end_value)
      return false;
  } else {
    end_value = start_value->IsCustomIdentValue()
                    ? start_value
                    : CSSIdentifierValue::Create(CSSValueAuto);
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
  template_rows = CSSPropertyParserHelpers::ConsumeIdent<CSSValueNone>(range);

  // 1- 'none' case.
  if (template_rows && range.AtEnd()) {
    template_rows = CSSIdentifierValue::Create(CSSValueNone);
    template_columns = CSSIdentifierValue::Create(CSSValueNone);
    template_areas = CSSIdentifierValue::Create(CSSValueNone);
    return true;
  }

  // 2- <grid-template-rows> / <grid-template-columns>
  if (!template_rows) {
    template_rows = ConsumeGridTrackList(range, context.Mode(),
                                         TrackListType::kGridTemplate);
  }

  if (template_rows) {
    if (!CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range))
      return false;
    template_columns = ConsumeGridTemplatesRowsOrColumns(range, context.Mode());
    if (!template_columns || !range.AtEnd())
      return false;

    template_areas = CSSIdentifierValue::Create(CSSValueNone);
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

  if (value == CSSValueAlways) {
    value = CSSValuePage;
    return true;
  }
  return value == CSSValueAuto || value == CSSValueAvoid ||
         value == CSSValueLeft || value == CSSValueRight;
}

bool ConsumeFromColumnBreakBetween(CSSParserTokenRange& range,
                                   CSSValueID& value) {
  if (!ConsumeCSSValueId(range, value)) {
    return false;
  }

  if (value == CSSValueAlways) {
    value = CSSValueColumn;
    return true;
  }
  return value == CSSValueAuto || value == CSSValueAvoid;
}

bool ConsumeFromColumnOrPageBreakInside(CSSParserTokenRange& range,
                                        CSSValueID& value) {
  if (!ConsumeCSSValueId(range, value)) {
    return false;
  }
  return value == CSSValueAuto || value == CSSValueAvoid;
}

bool ValidWidthOrHeightKeyword(CSSValueID id, const CSSParserContext& context) {
  if (id == CSSValueWebkitMinContent || id == CSSValueWebkitMaxContent ||
      id == CSSValueWebkitFillAvailable || id == CSSValueWebkitFitContent ||
      id == CSSValueMinContent || id == CSSValueMaxContent ||
      id == CSSValueFitContent) {
    switch (id) {
      case CSSValueWebkitMinContent:
        context.Count(WebFeature::kCSSValuePrefixedMinContent);
        break;
      case CSSValueWebkitMaxContent:
        context.Count(WebFeature::kCSSValuePrefixedMaxContent);
        break;
      case CSSValueWebkitFillAvailable:
        context.Count(WebFeature::kCSSValuePrefixedFillAvailable);
        break;
      case CSSValueWebkitFitContent:
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
  if (range.Peek().FunctionId() != CSSValuePath)
    return nullptr;

  CSSParserTokenRange function_range = range;
  CSSParserTokenRange function_args =
      CSSPropertyParserHelpers::ConsumeFunction(function_range);

  if (function_args.Peek().GetType() != kStringToken)
    return nullptr;
  String path_string =
      function_args.ConsumeIncludingWhitespace().Value().ToString();

  std::unique_ptr<SVGPathByteStream> byte_stream = SVGPathByteStream::Create();
  if (BuildByteStreamFromString(path_string, *byte_stream) !=
          SVGParseStatus::kNoError ||
      !function_args.AtEnd()) {
    return nullptr;
  }

  range = function_range;
  if (byte_stream->IsEmpty())
    return CSSIdentifierValue::Create(CSSValueNone);
  return CSSPathValue::Create(std::move(byte_stream));
}

CSSValue* ConsumeRay(CSSParserTokenRange& range,
                     const CSSParserContext& context) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueRay);
  CSSParserTokenRange function_range = range;
  CSSParserTokenRange function_args =
      CSSPropertyParserHelpers::ConsumeFunction(function_range);

  CSSPrimitiveValue* angle = nullptr;
  CSSIdentifierValue* size = nullptr;
  CSSIdentifierValue* contain = nullptr;
  while (!function_args.AtEnd()) {
    if (!angle) {
      angle = CSSPropertyParserHelpers::ConsumeAngle(
          function_args, &context, base::Optional<WebFeature>());
      if (angle)
        continue;
    }
    if (!size) {
      size = CSSPropertyParserHelpers::ConsumeIdent<
          CSSValueClosestSide, CSSValueClosestCorner, CSSValueFarthestSide,
          CSSValueFarthestCorner, CSSValueSides>(function_args);
      if (size)
        continue;
    }
    if (RuntimeEnabledFeatures::CSSOffsetPathRayContainEnabled() && !contain) {
      contain = CSSPropertyParserHelpers::ConsumeIdent<CSSValueContain>(
          function_args);
      if (contain)
        continue;
    }
    return nullptr;
  }
  if (!angle || !size)
    return nullptr;
  range = function_range;
  return CSSRayValue::Create(*angle, *size, contain);
}

CSSValue* ConsumeMaxWidthOrHeight(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueNone ||
      ValidWidthOrHeightKeyword(range.Peek().Id(), context))
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative, unitless);
}

CSSValue* ConsumeWidthOrHeight(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueAuto ||
      ValidWidthOrHeightKeyword(range.Peek().Id(), context))
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative, unitless);
}

CSSValue* ConsumeMarginOrOffset(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeAll, unitless);
}

CSSValue* ConsumeOffsetPath(CSSParserTokenRange& range,
                            const CSSParserContext& context) {
  CSSValue* value = nullptr;
  if (RuntimeEnabledFeatures::CSSOffsetPathRayEnabled() &&
      range.Peek().FunctionId() == CSSValueRay)
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
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  return ConsumePath(range);
}

CSSValue* ConsumeOffsetRotate(CSSParserTokenRange& range,
                              const CSSParserContext& context) {
  CSSValue* angle = CSSPropertyParserHelpers::ConsumeAngle(
      range, &context, base::Optional<WebFeature>());
  CSSValue* keyword =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueAuto, CSSValueReverse>(
          range);
  if (!angle && !keyword)
    return nullptr;

  if (!angle) {
    angle = CSSPropertyParserHelpers::ConsumeAngle(
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
  unsigned i = 0;
  for (; i < 4 && !range.AtEnd() && range.Peek().GetType() != kDelimiterToken;
       ++i) {
    horizontal_radii[i] = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
        range, css_parser_mode, kValueRangeNonNegative);
    if (!horizontal_radii[i])
      return false;
  }
  if (!horizontal_radii[0])
    return false;
  if (range.AtEnd()) {
    // Legacy syntax: -webkit-border-radius: l1 l2; is equivalent to
    // border-radius: l1 / l2;
    if (use_legacy_parsing && i == 2) {
      vertical_radii[0] = horizontal_radii[1];
      horizontal_radii[1] = nullptr;
    } else {
      CSSPropertyParserHelpers::Complete4Sides(horizontal_radii);
      for (unsigned i = 0; i < 4; ++i)
        vertical_radii[i] = horizontal_radii[i];
      return true;
    }
  } else {
    if (!CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range))
      return false;
    for (i = 0; i < 4 && !range.AtEnd(); ++i) {
      vertical_radii[i] = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
          range, css_parser_mode, kValueRangeNonNegative);
      if (!vertical_radii[i])
        return false;
    }
    if (!vertical_radii[0] || !range.AtEnd())
      return false;
  }
  CSSPropertyParserHelpers::Complete4Sides(horizontal_radii);
  CSSPropertyParserHelpers::Complete4Sides(vertical_radii);
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
      CSSPropertyParserHelpers::ConsumeFunction(range_copy);
  if (id == CSSValueCircle)
    shape = ConsumeBasicShapeCircle(args, context);
  else if (id == CSSValueEllipse)
    shape = ConsumeBasicShapeEllipse(args, context);
  else if (id == CSSValuePolygon)
    shape = ConsumeBasicShapePolygon(args, context);
  else if (id == CSSValueInset)
    shape = ConsumeBasicShapeInset(args, context);
  if (!shape || !args.AtEnd())
    return nullptr;

  context.Count(WebFeature::kCSSBasicShape);
  range = range_copy;
  return shape;
}

CSSValue* ConsumeTextDecorationLine(CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  while (true) {
    CSSIdentifierValue* ident =
        CSSPropertyParserHelpers::ConsumeIdent<CSSValueBlink, CSSValueUnderline,
                                               CSSValueOverline,
                                               CSSValueLineThrough>(range);
    if (!ident)
      break;
    if (list->HasValue(*ident))
      return nullptr;
    list->Append(*ident);
  }

  if (!list->length())
    return nullptr;
  return list;
}

CSSValue* ConsumeTransformValue(CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                bool use_legacy_parsing) {
  CSSValueID function_id = range.Peek().FunctionId();
  if (function_id == CSSValueInvalid)
    return nullptr;
  CSSParserTokenRange args = CSSPropertyParserHelpers::ConsumeFunction(range);
  if (args.AtEnd())
    return nullptr;
  CSSFunctionValue* transform_value = CSSFunctionValue::Create(function_id);
  CSSValue* parsed_value = nullptr;
  switch (function_id) {
    case CSSValueRotate:
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
    case CSSValueSkewX:
    case CSSValueSkewY:
    case CSSValueSkew:
      parsed_value = CSSPropertyParserHelpers::ConsumeAngle(
          args, &context, WebFeature::kUnitlessZeroAngleTransform);
      if (!parsed_value)
        return nullptr;
      if (function_id == CSSValueSkew &&
          CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value = CSSPropertyParserHelpers::ConsumeAngle(
            args, &context, WebFeature::kUnitlessZeroAngleTransform);
        if (!parsed_value)
          return nullptr;
      }
      break;
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
    case CSSValueScale:
      parsed_value =
          CSSPropertyParserHelpers::ConsumeNumber(args, kValueRangeAll);
      if (!parsed_value)
        return nullptr;
      if (function_id == CSSValueScale &&
          CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value =
            CSSPropertyParserHelpers::ConsumeNumber(args, kValueRangeAll);
        if (!parsed_value)
          return nullptr;
      }
      break;
    case CSSValuePerspective:
      if (!ConsumePerspective(args, context, transform_value,
                              use_legacy_parsing)) {
        return nullptr;
      }
      break;
    case CSSValueTranslateX:
    case CSSValueTranslateY:
    case CSSValueTranslate:
      parsed_value = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
          args, context.Mode(), kValueRangeAll);
      if (!parsed_value)
        return nullptr;
      if (function_id == CSSValueTranslate &&
          CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
            args, context.Mode(), kValueRangeAll);
        if (!parsed_value)
          return nullptr;
      }
      break;
    case CSSValueTranslateZ:
      parsed_value = CSSPropertyParserHelpers::ConsumeLength(
          args, context.Mode(), kValueRangeAll);
      break;
    case CSSValueMatrix:
    case CSSValueMatrix3d:
      if (!ConsumeNumbers(args, transform_value,
                          (function_id == CSSValueMatrix3d) ? 16 : 6)) {
        return nullptr;
      }
      break;
    case CSSValueScale3d:
      if (!ConsumeNumbers(args, transform_value, 3))
        return nullptr;
      break;
    case CSSValueRotate3d:
      if (!ConsumeNumbers(args, transform_value, 3) ||
          !CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
        return nullptr;
      }
      parsed_value = CSSPropertyParserHelpers::ConsumeAngle(
          args, &context, WebFeature::kUnitlessZeroAngleTransform);
      if (!parsed_value)
        return nullptr;
      break;
    case CSSValueTranslate3d:
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
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

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

CSSValue* ConsumeTransitionProperty(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() != kIdentToken)
    return nullptr;
  if (token.Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSPropertyID unresolved_property = token.ParseAsUnresolvedCSSPropertyID();
  if (unresolved_property != CSSPropertyInvalid &&
      unresolved_property != CSSPropertyVariable) {
#if DCHECK_IS_ON()
    DCHECK(CSSProperty::Get(resolveCSSPropertyID(unresolved_property))
               .IsEnabled());
#endif
    range.ConsumeIncludingWhitespace();
    return CSSCustomIdentValue::Create(unresolved_property);
  }
  return CSSPropertyParserHelpers::ConsumeCustomIdent(range);
}

bool IsValidPropertyList(const CSSValueList& value_list) {
  if (value_list.length() < 2)
    return true;
  for (auto& value : value_list) {
    if (value->IsIdentifierValue() &&
        ToCSSIdentifierValue(*value).GetValueID() == CSSValueNone)
      return false;
  }
  return true;
}

CSSValue* ConsumeBorderColorSide(CSSParserTokenRange& range,
                                 const CSSParserContext& context,
                                 const CSSParserLocalContext& local_context) {
  CSSPropertyID shorthand = local_context.CurrentShorthand();
  bool allow_quirky_colors =
      IsQuirksModeBehavior(context.Mode()) &&
      (shorthand == CSSPropertyInvalid || shorthand == CSSPropertyBorderColor);
  return CSSPropertyParserHelpers::ConsumeColor(range, context.Mode(),
                                                allow_quirky_colors);
}

CSSValue* ConsumeBorderWidth(CSSParserTokenRange& range,
                             CSSParserMode css_parser_mode,
                             CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  return CSSPropertyParserHelpers::ConsumeLineWidth(range, css_parser_mode,
                                                    unitless);
}

CSSValue* ParseSpacing(CSSParserTokenRange& range,
                       const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueNormal)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  // TODO(timloh): allow <percentage>s in word-spacing.
  return CSSPropertyParserHelpers::ConsumeLength(
      range, context.Mode(), kValueRangeAll,
      CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

CSSValue* ParsePaintStroke(CSSParserTokenRange& range,
                           const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSURIValue* url = CSSPropertyParserHelpers::ConsumeUrl(range, &context);
  if (url) {
    CSSValue* parsed_value = nullptr;
    if (range.Peek().Id() == CSSValueNone) {
      parsed_value = CSSPropertyParserHelpers::ConsumeIdent(range);
    } else {
      parsed_value =
          CSSPropertyParserHelpers::ConsumeColor(range, context.Mode());
    }
    if (parsed_value) {
      CSSValueList* values = CSSValueList::CreateSpaceSeparated();
      values->Append(*url);
      values->Append(*parsed_value);
      return values;
    }
    return url;
  }
  return CSSPropertyParserHelpers::ConsumeColor(range, context.Mode());
}

}  // namespace CSSParsingUtils
}  // namespace blink
