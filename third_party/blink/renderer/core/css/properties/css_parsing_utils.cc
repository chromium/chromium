// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

#include <cmath>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/css/counter_style_map.h"
#include "third_party/blink/renderer/core/css/css_axis_value.h"
#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"
#include "third_party/blink/renderer/core/css/css_border_image.h"
#include "third_party/blink/renderer/core/css/css_bracketed_value_list.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"
#include "third_party/blink/renderer/core/css/css_crossfade_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_feature_value.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_option_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_type_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_light_dark_value_pair.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/css/css_palette_mix_value.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/css_ratio_value.h"
#include "third_party/blink/renderer/core/css/css_ray_value.h"
#include "third_party/blink/renderer/core/css/css_revert_layer_value.h"
#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_scroll_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_timing_function_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/css_view_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_color_function_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_parsing_error.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/color_utils.h"

namespace blink {

using cssvalue::CSSBracketedValueList;
using cssvalue::CSSFontFeatureValue;

namespace css_parsing_utils {
namespace {

const char kTwoDashes[] = "--";

bool IsLeftOrRightKeyword(CSSValueID id) {
  return IdentMatches<CSSValueID::kLeft, CSSValueID::kRight>(id);
}

bool IsAuto(CSSValueID id) {
  return IdentMatches<CSSValueID::kAuto>(id);
}

bool IsNormalOrStretch(CSSValueID id) {
  return IdentMatches<CSSValueID::kNormal, CSSValueID::kStretch>(id);
}

bool IsContentDistributionKeyword(CSSValueID id) {
  return IdentMatches<CSSValueID::kSpaceBetween, CSSValueID::kSpaceAround,
                      CSSValueID::kSpaceEvenly, CSSValueID::kStretch>(id);
}

bool IsOverflowKeyword(CSSValueID id) {
  return IdentMatches<CSSValueID::kUnsafe, CSSValueID::kSafe>(id);
}

bool IsIdent(const CSSValue& value, CSSValueID id) {
  const auto* ident = DynamicTo<CSSIdentifierValue>(value);
  return ident && ident->GetValueID() == id;
}

CSSIdentifierValue* ConsumeOverflowPositionKeyword(CSSParserTokenRange& range) {
  return IsOverflowKeyword(range.Peek().Id()) ? ConsumeIdent(range) : nullptr;
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

CSSValue* ConsumeFirstBaseline(CSSParserTokenRange& range) {
  ConsumeIdent<CSSValueID::kFirst>(range);
  return ConsumeIdent<CSSValueID::kBaseline>(range);
}

CSSValue* ConsumeBaseline(CSSParserTokenRange& range) {
  CSSIdentifierValue* preference =
      ConsumeIdent<CSSValueID::kFirst, CSSValueID::kLast>(range);
  CSSIdentifierValue* baseline = ConsumeIdent<CSSValueID::kBaseline>(range);
  if (!baseline) {
    return nullptr;
  }
  if (preference && preference->GetValueID() == CSSValueID::kLast) {
    return MakeGarbageCollected<CSSValuePair>(
        preference, baseline, CSSValuePair::kDropIdenticalValues);
  }
  return baseline;
}

absl::optional<cssvalue::CSSLinearStop> ConsumeLinearStop(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  absl::optional<double> number;
  absl::optional<double> length_a;
  absl::optional<double> length_b;
  while (!range.AtEnd()) {
    if (range.Peek().GetType() == kCommaToken) {
      break;
    }
    CSSPrimitiveValue* value =
        ConsumeNumber(range, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!number.has_value() && value && value->IsNumber()) {
      number = value->GetDoubleValue();
      continue;
    }
    value = ConsumePercent(range, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!length_a.has_value() && value && value->IsPercentage()) {
      length_a = value->GetDoubleValue();
      value =
          ConsumePercent(range, context, CSSPrimitiveValue::ValueRange::kAll);
      if (value && value->IsPercentage()) {
        length_b = value->GetDoubleValue();
      }
      continue;
    }
    return {};
  }
  if (!number.has_value()) {
    return {};
  }
  return {{number.value(), length_a, length_b}};
}

CSSValue* ConsumeLinear(CSSParserTokenRange& range,
                        const CSSParserContext& context) {
  // https://w3c.github.io/csswg-drafts/css-easing/#linear-easing-function-parsing
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueID::kLinear);
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  Vector<cssvalue::CSSLinearStop> stop_list{};
  absl::optional<cssvalue::CSSLinearStop> linear_stop;
  do {
    linear_stop = ConsumeLinearStop(args, context);
    if (!linear_stop.has_value()) {
      return nullptr;
    }
    stop_list.emplace_back(linear_stop.value());
  } while (ConsumeCommaIncludingWhitespace(args));
  if (!args.AtEnd()) {
    return nullptr;
  }
  // 1. Let function be a new linear easing function.
  // 2. Let largestInput be negative infinity.
  // 3. If there are less than two items in stopList, then return failure.
  if (stop_list.size() < 2) {
    return nullptr;
  }
  // 4. For each stop in stopList:
  double largest_input = std::numeric_limits<double>::lowest();
  Vector<gfx::LinearEasingPoint> points{};
  for (wtf_size_t i = 0; i < stop_list.size(); ++i) {
    const auto& stop = stop_list[i];
    // 4.1. Let point be a new linear easing point with its output set
    // to stop’s <number> as a number.
    gfx::LinearEasingPoint point{std::numeric_limits<double>::quiet_NaN(),
                                 stop.number};
    // 4.2. Append point to function’s points.
    points.emplace_back(point);
    // 4.3. If stop has a <linear-stop-length>, then:
    if (stop.length_a.has_value()) {
      // 4.3.1. Set point’s input to whichever is greater:
      // stop’s <linear-stop-length>'s first <percentage> as a number,
      // or largestInput.
      points.back().input = std::max(largest_input, stop.length_a.value());
      // 4.3.2. Set largestInput to point’s input.
      largest_input = points.back().input;
      // 4.3.3. If stop’s <linear-stop-length> has a second <percentage>, then:
      if (stop.length_b.has_value()) {
        // 4.3.3.1. Let extraPoint be a new linear easing point with its output
        // set to stop’s <number> as a number.
        gfx::LinearEasingPoint extra_point{
            // 4.3.3.3. Set extraPoint’s input to whichever is greater:
            // stop’s <linear-stop-length>'s second <percentage>
            // as a number, or largestInput.
            std::max(largest_input, stop.length_b.value()), stop.number};
        // 4.3.3.2. Append extraPoint to function’s points.
        points.emplace_back(extra_point);
        // 4.3.3.4. Set largestInput to extraPoint’s input.
        largest_input = extra_point.input;
      }
      // 4.4. Otherwise, if stop is the first item in stopList, then:
    } else if (i == 0) {
      // 4.4.1. Set point’s input to 0.
      points.back().input = 0;
      // 4.4.2. Set largestInput to 0.
      largest_input = 0;
      // 4.5. Otherwise, if stop is the last item in stopList,
      // then set point’s input to whichever is greater: 1 or largestInput.
    } else if (i == stop_list.size() - 1) {
      points.back().input = std::max(100., largest_input);
    }
  }
  // 5. For runs of items in function’s points that have a null input, assign a
  // number to the input by linearly interpolating between the closest previous
  // and next points that have a non-null input.
  wtf_size_t upper_index = 0;
  for (wtf_size_t i = 1; i < points.size(); ++i) {
    if (std::isnan(points[i].input)) {
      if (i > upper_index) {
        const auto* it = std::find_if(
            std::next(points.begin(), i + 1), points.end(),
            [](const auto& point) { return !std::isnan(point.input); });
        upper_index = static_cast<wtf_size_t>(it - points.begin());
      }
      points[i].input = points[i - 1].input +
                        (points[upper_index].input - points[i - 1].input) /
                            (upper_index - (i - 1));
    }
  }
  range = range_copy;
  // 6. Return function.
  return MakeGarbageCollected<cssvalue::CSSLinearTimingFunctionValue>(
      std::move(points));
}

CSSValue* ConsumeSteps(CSSParserTokenRange& range,
                       const CSSParserContext& context) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueID::kSteps);
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);

  CSSPrimitiveValue* steps = ConsumePositiveInteger(args, context);
  if (!steps) {
    return nullptr;
  }

  StepsTimingFunction::StepPosition position =
      StepsTimingFunction::StepPosition::END;
  if (ConsumeCommaIncludingWhitespace(args)) {
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

  if (!args.AtEnd()) {
    return nullptr;
  }

  // Steps(n, jump-none) requires n >= 2.
  if (position == StepsTimingFunction::StepPosition::JUMP_NONE &&
      steps->GetIntValue() < 2) {
    return nullptr;
  }

  range = range_copy;
  return MakeGarbageCollected<cssvalue::CSSStepsTimingFunctionValue>(
      steps->GetIntValue(), position);
}

CSSValue* ConsumeCubicBezier(CSSParserTokenRange& range,
                             const CSSParserContext& context) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueID::kCubicBezier);
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);

  double x1, y1, x2, y2;
  if (ConsumeNumberRaw(args, context, x1) && x1 >= 0 && x1 <= 1 &&
      ConsumeCommaIncludingWhitespace(args) &&
      ConsumeNumberRaw(args, context, y1) &&
      ConsumeCommaIncludingWhitespace(args) &&
      ConsumeNumberRaw(args, context, x2) && x2 >= 0 && x2 <= 1 &&
      ConsumeCommaIncludingWhitespace(args) &&
      ConsumeNumberRaw(args, context, y2) && args.AtEnd()) {
    range = range_copy;
    return MakeGarbageCollected<cssvalue::CSSCubicBezierTimingFunctionValue>(
        x1, y1, x2, y2);
  }

  return nullptr;
}

CSSIdentifierValue* ConsumeBorderImageRepeatKeyword(
    CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kStretch, CSSValueID::kRepeat,
                      CSSValueID::kSpace, CSSValueID::kRound>(range);
}

bool ConsumeCSSValueId(CSSParserTokenRange& range, CSSValueID& value) {
  CSSIdentifierValue* keyword = ConsumeIdent(range);
  if (!keyword || !range.AtEnd()) {
    return false;
  }
  value = keyword->GetValueID();
  return true;
}

CSSValue* ConsumeShapeRadius(CSSParserTokenRange& args,
                             const CSSParserContext& context) {
  if (IdentMatches<CSSValueID::kClosestSide, CSSValueID::kFarthestSide>(
          args.Peek().Id())) {
    return ConsumeIdent(args);
  }
  return ConsumeLengthOrPercent(args, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative);
}

cssvalue::CSSBasicShapeCircleValue* ConsumeBasicShapeCircle(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
  // circle( [<shape-radius>]? [at <position>]? )
  auto* shape = MakeGarbageCollected<cssvalue::CSSBasicShapeCircleValue>();
  if (CSSValue* radius = ConsumeShapeRadius(args, context)) {
    shape->SetRadius(radius);
  }
  if (ConsumeIdent<CSSValueID::kAt>(args)) {
    CSSValue* center_x = nullptr;
    CSSValue* center_y = nullptr;
    if (!ConsumePosition(args, context, UnitlessQuirk::kForbid,
                         absl::optional<WebFeature>(), center_x, center_y)) {
      return nullptr;
    }
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
  if (CSSValue* radius_x = ConsumeShapeRadius(args, context)) {
    CSSValue* radius_y = ConsumeShapeRadius(args, context);
    if (!radius_y) {
      return nullptr;
    }
    shape->SetRadiusX(radius_x);
    shape->SetRadiusY(radius_y);
    feature = WebFeature::kBasicShapeEllipseTwoRadius;
  }
  if (ConsumeIdent<CSSValueID::kAt>(args)) {
    CSSValue* center_x = nullptr;
    CSSValue* center_y = nullptr;
    if (!ConsumePosition(args, context, UnitlessQuirk::kForbid,
                         absl::optional<WebFeature>(), center_x, center_y)) {
      return nullptr;
    }
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
  if (IdentMatches<CSSValueID::kEvenodd, CSSValueID::kNonzero>(
          args.Peek().Id())) {
    shape->SetWindRule(args.ConsumeIncludingWhitespace().Id() ==
                               CSSValueID::kEvenodd
                           ? RULE_EVENODD
                           : RULE_NONZERO);
    if (!ConsumeCommaIncludingWhitespace(args)) {
      return nullptr;
    }
  }

  do {
    CSSPrimitiveValue* x_length = ConsumeLengthOrPercent(
        args, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!x_length) {
      return nullptr;
    }
    CSSPrimitiveValue* y_length = ConsumeLengthOrPercent(
        args, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!y_length) {
      return nullptr;
    }
    shape->AppendPoint(x_length, y_length);
  } while (ConsumeCommaIncludingWhitespace(args));
  return shape;
}

template <typename T>
bool ConsumeBorderRadiusCommon(CSSParserTokenRange& args,
                               const CSSParserContext& context,
                               T* shape) {
  if (ConsumeIdent<CSSValueID::kRound>(args)) {
    CSSValue* horizontal_radii[4] = {nullptr};
    CSSValue* vertical_radii[4] = {nullptr};
    if (!ConsumeRadii(horizontal_radii, vertical_radii, args, context, false)) {
      return false;
    }
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
  return true;
}

cssvalue::CSSBasicShapeInsetValue* ConsumeBasicShapeInset(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  auto* shape = MakeGarbageCollected<cssvalue::CSSBasicShapeInsetValue>();
  CSSPrimitiveValue* top = ConsumeLengthOrPercent(
      args, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!top) {
    return nullptr;
  }
  CSSPrimitiveValue* right = ConsumeLengthOrPercent(
      args, context, CSSPrimitiveValue::ValueRange::kAll);
  CSSPrimitiveValue* bottom = nullptr;
  CSSPrimitiveValue* left = nullptr;
  if (right) {
    bottom = ConsumeLengthOrPercent(args, context,
                                    CSSPrimitiveValue::ValueRange::kAll);
    if (bottom) {
      left = ConsumeLengthOrPercent(args, context,
                                    CSSPrimitiveValue::ValueRange::kAll);
    }
  }
  if (left) {
    shape->UpdateShapeSize4Values(top, right, bottom, left);
  } else if (bottom) {
    shape->UpdateShapeSize3Values(top, right, bottom);
  } else if (right) {
    shape->UpdateShapeSize2Values(top, right);
  } else {
    shape->UpdateShapeSize1Value(top);
  }

  if (!ConsumeBorderRadiusCommon(args, context, shape)) {
    return nullptr;
  }

  return shape;
}

cssvalue::CSSBasicShapeRectValue* ConsumeBasicShapeRect(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  CSSValue* lengths[4];
  for (auto*& length : lengths) {
    length = ConsumeLengthOrPercent(args, context,
                                    CSSPrimitiveValue::ValueRange::kAll);
    if (length) {
      continue;
    }

    if (args.Peek().Id() == CSSValueID::kAuto) {
      length = css_parsing_utils::ConsumeIdent(args);
    }

    if (!length) {
      return nullptr;
    }
  }

  auto* shape = MakeGarbageCollected<cssvalue::CSSBasicShapeRectValue>(
      lengths[0], lengths[1], lengths[2], lengths[3]);

  if (!ConsumeBorderRadiusCommon(args, context, shape)) {
    return nullptr;
  }

  return shape;
}

cssvalue::CSSBasicShapeXYWHValue* ConsumeBasicShapeXYWH(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  CSSPrimitiveValue* lengths[4];
  for (size_t i = 0; i < 4; i++) {
    // The last 2 values are width/height which must be positive.
    auto value_range = i > 1 ? CSSPrimitiveValue::ValueRange::kNonNegative
                             : CSSPrimitiveValue::ValueRange::kAll;
    lengths[i] = ConsumeLengthOrPercent(args, context, value_range);
    if (!lengths[i]) {
      return nullptr;
    }
  }

  auto* shape = MakeGarbageCollected<cssvalue::CSSBasicShapeXYWHValue>(
      lengths[0], lengths[1], lengths[2], lengths[3]);

  if (!ConsumeBorderRadiusCommon(args, context, shape)) {
    return nullptr;
  }

  return shape;
}

bool ConsumeNumbers(CSSParserTokenRange& args,
                    const CSSParserContext& context,
                    CSSFunctionValue*& transform_value,
                    unsigned number_of_arguments) {
  do {
    CSSValue* parsed_value =
        ConsumeNumber(args, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!parsed_value) {
      return false;
    }
    transform_value->Append(*parsed_value);
    if (--number_of_arguments && !ConsumeCommaIncludingWhitespace(args)) {
      return false;
    }
  } while (number_of_arguments);
  return true;
}

bool ConsumeNumbersOrPercents(CSSParserTokenRange& args,
                              const CSSParserContext& context,
                              CSSFunctionValue*& transform_value,
                              unsigned number_of_arguments) {
  do {
    CSSValue* parsed_value = ConsumeNumberOrPercent(
        args, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!parsed_value) {
      return false;
    }
    transform_value->Append(*parsed_value);
    if (--number_of_arguments && !ConsumeCommaIncludingWhitespace(args)) {
      return false;
    }
  } while (number_of_arguments);
  return true;
}

bool ConsumePerspective(CSSParserTokenRange& args,
                        const CSSParserContext& context,
                        CSSFunctionValue*& transform_value,
                        bool use_legacy_parsing) {
  CSSValue* parsed_value =
      ConsumeLength(args, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!parsed_value) {
    parsed_value = ConsumeIdent<CSSValueID::kNone>(args);
  }
  if (!parsed_value && use_legacy_parsing) {
    double perspective;
    if (!ConsumeNumberRaw(args, context, perspective) || perspective < 0) {
      return false;
    }
    context.Count(WebFeature::kUnitlessPerspectiveInTransformProperty);
    parsed_value = CSSNumericLiteralValue::Create(
        perspective, CSSPrimitiveValue::UnitType::kPixels);
  }
  if (!parsed_value) {
    return false;
  }
  transform_value->Append(*parsed_value);
  return true;
}

bool ConsumeTranslate3d(CSSParserTokenRange& args,
                        const CSSParserContext& context,
                        CSSFunctionValue*& transform_value) {
  unsigned number_of_arguments = 2;
  CSSValue* parsed_value = nullptr;
  do {
    parsed_value = ConsumeLengthOrPercent(args, context,
                                          CSSPrimitiveValue::ValueRange::kAll);
    if (!parsed_value) {
      return false;
    }
    transform_value->Append(*parsed_value);
    if (!ConsumeCommaIncludingWhitespace(args)) {
      return false;
    }
  } while (--number_of_arguments);
  parsed_value =
      ConsumeLength(args, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!parsed_value) {
    return false;
  }
  transform_value->Append(*parsed_value);
  return true;
}

// Add CSSVariableData to variableData vector.
bool AddCSSPaintArgument(
    const Vector<CSSParserToken>& tokens,
    Vector<scoped_refptr<CSSVariableData>>* const variable_data) {
  CSSParserTokenRange token_range(tokens);
  if (CSSVariableParser::ContainsValidVariableReferences(token_range)) {
    return false;
  }
  if (!token_range.AtEnd()) {
    // CSSParserTokenRange doesn't store precise location information about
    // where each token started or ended, so we don't have the actual original
    // string. However, for CSS paint arguments, it's not a huge issue
    // if we get normalized whitespace etc., so we work around it by creating
    // a fake “original text” by serializing the tokens back.
    String text = token_range.Serialize();
    scoped_refptr<CSSVariableData> unparsed_css_variable_data =
        CSSVariableData::Create({token_range, text}, false, false);
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
    // A block of some type (maybe a function, maybe (), [], or {}).
    // Push the block start.
    //
    // For functions, we don't have any upfront knowledge about the input
    // argument types here, we should just leave the token as it is and
    // resolve it later in the variable parsing phase.
    argument_tokens.push_back(args.Peek());
    CSSParserTokenType closing_type =
        CSSParserToken::ClosingTokenType(args.Peek().GetType());

    CSSParserTokenRange contents = args.ConsumeBlock();
    while (!contents.AtEnd()) {
      argument_tokens.push_back(contents.Consume());
    }
    argument_tokens.push_back(
        CSSParserToken(closing_type, CSSParserToken::kBlockEnd));

  } else {
    argument_tokens.push_back(args.ConsumeIncludingWhitespace());
  }
  return argument_tokens;
}

CSSFunctionValue* ConsumeFilterFunction(CSSParserTokenRange& range,
                                        const CSSParserContext& context) {
  CSSValueID filter_type = range.Peek().FunctionId();
  if (filter_type < CSSValueID::kInvert ||
      filter_type > CSSValueID::kDropShadow) {
    return nullptr;
  }
  CSSParserTokenRange args = ConsumeFunction(range);
  CSSFunctionValue* filter_value =
      MakeGarbageCollected<CSSFunctionValue>(filter_type);
  CSSValue* parsed_value = nullptr;

  if (filter_type == CSSValueID::kDropShadow) {
    parsed_value =
        ParseSingleShadow(args, context, AllowInsetAndSpread::kForbid);
  } else {
    if (args.AtEnd()) {
      context.Count(WebFeature::kCSSFilterFunctionNoArguments);
      return filter_value;
    }
    if (filter_type == CSSValueID::kBrightness) {
      // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
      parsed_value =
          ConsumePercent(args, context, CSSPrimitiveValue::ValueRange::kAll);
      if (!parsed_value) {
        parsed_value = ConsumeNumber(
            args, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      }
    } else if (filter_type == CSSValueID::kHueRotate) {
      parsed_value =
          ConsumeAngle(args, context, WebFeature::kUnitlessZeroAngleFilter);
    } else if (filter_type == CSSValueID::kBlur) {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      parsed_value = ConsumeLength(args, context,
                                   CSSPrimitiveValue::ValueRange::kNonNegative);
    } else {
      // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
      parsed_value = ConsumePercent(
          args, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      if (!parsed_value) {
        parsed_value = ConsumeNumber(
            args, context, CSSPrimitiveValue::ValueRange::kNonNegative);
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
  if (!parsed_value || !args.AtEnd()) {
    return nullptr;
  }
  filter_value->Append(*parsed_value);
  return filter_value;
}

template <typename Func, typename... Args>
CSSLightDarkValuePair* ConsumeInternalLightDark(Func consume_value,
                                                CSSParserTokenRange& range,
                                                const CSSParserContext& context,
                                                Args&&... args) {
  if (range.Peek().FunctionId() != CSSValueID::kInternalLightDark) {
    return nullptr;
  }
  if (!isValueAllowedInMode(CSSValueID::kInternalLightDark, context.Mode())) {
    return nullptr;
  }
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange arg_range = ConsumeFunction(range_copy);
  CSSValue* light_value =
      consume_value(arg_range, context, std::forward<Args>(args)...);
  if (!light_value || !ConsumeCommaIncludingWhitespace(arg_range)) {
    return nullptr;
  }
  CSSValue* dark_value =
      consume_value(arg_range, context, std::forward<Args>(args)...);
  if (!dark_value || !arg_range.AtEnd()) {
    return nullptr;
  }
  range = range_copy;
  return MakeGarbageCollected<CSSLightDarkValuePair>(light_value, dark_value);
}

// https://drafts.csswg.org/css-syntax/#typedef-any-value
bool IsTokenAllowedForAnyValue(const CSSParserToken& token) {
  switch (token.GetType()) {
    case kBadStringToken:
    case kEOFToken:
    case kBadUrlToken:
      return false;
    case kRightParenthesisToken:
    case kRightBracketToken:
    case kRightBraceToken:
      return token.GetBlockType() == CSSParserToken::kBlockEnd;
    default:
      return true;
  }
}

bool IsGeneratedImage(const CSSValueID id) {
  switch (id) {
    case CSSValueID::kLinearGradient:
    case CSSValueID::kRadialGradient:
    case CSSValueID::kConicGradient:
    case CSSValueID::kRepeatingLinearGradient:
    case CSSValueID::kRepeatingRadialGradient:
    case CSSValueID::kRepeatingConicGradient:
    case CSSValueID::kWebkitLinearGradient:
    case CSSValueID::kWebkitRadialGradient:
    case CSSValueID::kWebkitRepeatingLinearGradient:
    case CSSValueID::kWebkitRepeatingRadialGradient:
    case CSSValueID::kWebkitGradient:
    case CSSValueID::kWebkitCrossFade:
    case CSSValueID::kPaint:
      return true;

    default:
      return false;
  }
}

bool IsImageSet(const CSSValueID id) {
  if (id == CSSValueID::kWebkitImageSet) {
    return true;
  }
  if (id == CSSValueID::kImageSet) {
    return RuntimeEnabledFeatures::CSSImageSetEnabled();
  }
  return false;
}

}  // namespace

void Complete4Sides(CSSValue* side[4]) {
  if (side[3]) {
    return;
  }
  if (!side[2]) {
    if (!side[1]) {
      side[1] = side[0];
    }
    side[2] = side[0];
  }
  side[3] = side[1];
}

bool ConsumeCommaIncludingWhitespace(CSSParserTokenRange& range) {
  CSSParserToken value = range.Peek();
  if (value.GetType() != kCommaToken) {
    return false;
  }
  range.ConsumeIncludingWhitespace();
  return true;
}

bool ConsumeSlashIncludingWhitespace(CSSParserTokenRange& range) {
  CSSParserToken value = range.Peek();
  if (value.GetType() != kDelimiterToken || value.Delimiter() != '/') {
    return false;
  }
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

bool ConsumeAnyValue(CSSParserTokenRange& range) {
  bool result = IsTokenAllowedForAnyValue(range.Peek());
  unsigned nesting_level = 0;

  while (nesting_level || result) {
    const CSSParserToken& token = range.Consume();
    if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      nesting_level++;
    } else if (token.GetBlockType() == CSSParserToken::kBlockEnd) {
      nesting_level--;
    }
    if (range.AtEnd()) {
      return result;
    }
    result = result && IsTokenAllowedForAnyValue(range.Peek());
  }

  return result;
}

// TODO(rwlbuis): consider pulling in the parsing logic from
// css_math_expression_node.cc.
class MathFunctionParser {
  STACK_ALLOCATED();

 public:
  MathFunctionParser(
      CSSParserTokenRange& range,
      const CSSParserContext& context,
      CSSPrimitiveValue::ValueRange value_range,
      const bool is_percentage_allowed = true,
      CSSAnchorQueryTypes allowed_anchor_queries = kCSSAnchorQueryTypesNone,
      HashMap<CSSValueID, double> color_channel_keyword_values = {})
      : source_range_(range), range_(range) {
    const CSSParserToken& token = range.Peek();
    if (token.GetType() == kFunctionToken) {
      calc_value_ = CSSMathFunctionValue::Create(
          CSSMathExpressionNode::ParseMathFunction(
              token.FunctionId(), ConsumeFunction(range_), context,
              is_percentage_allowed, allowed_anchor_queries),
          value_range);
    }
  }

  const CSSMathFunctionValue* Value() const { return calc_value_; }
  CSSMathFunctionValue* ConsumeValue() {
    if (!calc_value_) {
      return nullptr;
    }
    source_range_ = range_;
    CSSMathFunctionValue* result = calc_value_;
    calc_value_ = nullptr;
    return result;
  }

  bool ConsumeNumberRaw(double& result) {
    if (!calc_value_ || calc_value_->Category() != kCalcNumber) {
      return false;
    }
    source_range_ = range_;
    result = calc_value_->GetDoubleValue();
    return true;
  }

 private:
  CSSParserTokenRange& source_range_;
  CSSParserTokenRange range_;
  CSSMathFunctionValue* calc_value_ = nullptr;
};

CSSPrimitiveValue* ConsumeInteger(CSSParserTokenRange& range,
                                  const CSSParserContext& context,
                                  double minimum_value,
                                  const bool is_percentage_allowed) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kNumberToken) {
    if (token.GetNumericValueType() == kNumberValueType ||
        token.NumericValue() < minimum_value) {
      return nullptr;
    }
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kInteger);
  }

  DCHECK(minimum_value == -std::numeric_limits<double>::max() ||
         minimum_value == 0 || minimum_value == 1);

  CSSPrimitiveValue::ValueRange value_range =
      CSSPrimitiveValue::ValueRange::kInteger;
  if (minimum_value == 0) {
    value_range = CSSPrimitiveValue::ValueRange::kNonNegativeInteger;
  } else if (minimum_value == 1) {
    value_range = CSSPrimitiveValue::ValueRange::kPositiveInteger;
  }

  MathFunctionParser math_parser(range, context, value_range,
                                 is_percentage_allowed);
  if (const CSSMathFunctionValue* math_value = math_parser.Value()) {
    if (math_value->Category() != kCalcNumber) {
      return nullptr;
    }
    return math_parser.ConsumeValue();
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
CSSPrimitiveValue* ConsumeIntegerOrNumberCalc(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_range) {
  CSSParserTokenRange int_range(range);
  double minimum_value = -std::numeric_limits<double>::max();
  switch (value_range) {
    case CSSPrimitiveValue::ValueRange::kAll:
      NOTREACHED() << "unexpected value range for integer parsing";
      [[fallthrough]];
    case CSSPrimitiveValue::ValueRange::kInteger:
      minimum_value = -std::numeric_limits<double>::max();
      break;
    case CSSPrimitiveValue::ValueRange::kNonNegative:
      NOTREACHED() << "unexpected value range for integer parsing";
      [[fallthrough]];
    case CSSPrimitiveValue::ValueRange::kNonNegativeInteger:
      minimum_value = 0.0;
      break;
    case CSSPrimitiveValue::ValueRange::kPositiveInteger:
      minimum_value = 1.0;
      break;
  }
  if (CSSPrimitiveValue* value =
          ConsumeInteger(int_range, context, minimum_value)) {
    range = int_range;
    return value;
  }

  MathFunctionParser math_parser(range, context, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() != kCalcNumber) {
      return nullptr;
    }
    return math_parser.ConsumeValue();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumePositiveInteger(CSSParserTokenRange& range,
                                          const CSSParserContext& context) {
  return ConsumeInteger(range, context, 1);
}

bool ConsumeNumberRaw(CSSParserTokenRange& range,
                      const CSSParserContext& context,
                      double& result) {
  if (range.Peek().GetType() == kNumberToken) {
    result = range.ConsumeIncludingWhitespace().NumericValue();
    return true;
  }
  MathFunctionParser math_parser(range, context,
                                 CSSPrimitiveValue::ValueRange::kAll);
  return math_parser.ConsumeNumberRaw(result);
}

// TODO(timloh): Work out if this can just call consumeNumberRaw
CSSPrimitiveValue* ConsumeNumber(CSSParserTokenRange& range,
                                 const CSSParserContext& context,
                                 CSSPrimitiveValue::ValueRange value_range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kNumberToken) {
    if (value_range == CSSPrimitiveValue::ValueRange::kNonNegative &&
        token.NumericValue() < 0) {
      return nullptr;
    }
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), token.GetUnitType());
  }
  MathFunctionParser math_parser(range, context, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() != kCalcNumber) {
      return nullptr;
    }
    return math_parser.ConsumeValue();
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
                                 const CSSParserContext& context,
                                 CSSPrimitiveValue::ValueRange value_range,
                                 UnitlessQuirk unitless) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken) {
    switch (token.GetUnitType()) {
      case CSSPrimitiveValue::UnitType::kQuirkyEms:
        if (context.Mode() != kUASheetMode) {
          return nullptr;
        }
        [[fallthrough]];
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
      case CSSPrimitiveValue::UnitType::kIcs:
      case CSSPrimitiveValue::UnitType::kLhs:
      case CSSPrimitiveValue::UnitType::kRexs:
      case CSSPrimitiveValue::UnitType::kRchs:
      case CSSPrimitiveValue::UnitType::kRics:
      case CSSPrimitiveValue::UnitType::kRlhs:
        break;
      case CSSPrimitiveValue::UnitType::kCaps:
      case CSSPrimitiveValue::UnitType::kRcaps:
        if (!RuntimeEnabledFeatures::CSSCapFontUnitsEnabled()) {
          return nullptr;
        }
        break;
      case CSSPrimitiveValue::UnitType::kViewportInlineSize:
      case CSSPrimitiveValue::UnitType::kViewportBlockSize:
      case CSSPrimitiveValue::UnitType::kSmallViewportWidth:
      case CSSPrimitiveValue::UnitType::kSmallViewportHeight:
      case CSSPrimitiveValue::UnitType::kSmallViewportInlineSize:
      case CSSPrimitiveValue::UnitType::kSmallViewportBlockSize:
      case CSSPrimitiveValue::UnitType::kSmallViewportMin:
      case CSSPrimitiveValue::UnitType::kSmallViewportMax:
      case CSSPrimitiveValue::UnitType::kLargeViewportWidth:
      case CSSPrimitiveValue::UnitType::kLargeViewportHeight:
      case CSSPrimitiveValue::UnitType::kLargeViewportInlineSize:
      case CSSPrimitiveValue::UnitType::kLargeViewportBlockSize:
      case CSSPrimitiveValue::UnitType::kLargeViewportMin:
      case CSSPrimitiveValue::UnitType::kLargeViewportMax:
      case CSSPrimitiveValue::UnitType::kDynamicViewportWidth:
      case CSSPrimitiveValue::UnitType::kDynamicViewportHeight:
      case CSSPrimitiveValue::UnitType::kDynamicViewportInlineSize:
      case CSSPrimitiveValue::UnitType::kDynamicViewportBlockSize:
      case CSSPrimitiveValue::UnitType::kDynamicViewportMin:
      case CSSPrimitiveValue::UnitType::kDynamicViewportMax:
        if (!RuntimeEnabledFeatures::CSSViewportUnits4Enabled()) {
          return nullptr;
        }
        break;
      case CSSPrimitiveValue::UnitType::kContainerWidth:
      case CSSPrimitiveValue::UnitType::kContainerHeight:
      case CSSPrimitiveValue::UnitType::kContainerInlineSize:
      case CSSPrimitiveValue::UnitType::kContainerBlockSize:
      case CSSPrimitiveValue::UnitType::kContainerMin:
      case CSSPrimitiveValue::UnitType::kContainerMax:
        break;
      default:
        return nullptr;
    }
    if (value_range == CSSPrimitiveValue::ValueRange::kNonNegative &&
        token.NumericValue() < 0) {
      return nullptr;
    }
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), token.GetUnitType());
  }
  if (token.GetType() == kNumberToken) {
    if (!ShouldAcceptUnitlessLength(token.NumericValue(), context.Mode(),
                                    unitless) ||
        (value_range == CSSPrimitiveValue::ValueRange::kNonNegative &&
         token.NumericValue() < 0)) {
      return nullptr;
    }
    CSSPrimitiveValue::UnitType unit_type =
        CSSPrimitiveValue::UnitType::kPixels;
    if (context.Mode() == kSVGAttributeMode) {
      unit_type = CSSPrimitiveValue::UnitType::kUserUnits;
    }
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), unit_type);
  }
  if (context.Mode() == kSVGAttributeMode) {
    return nullptr;
  }
  MathFunctionParser math_parser(range, context, value_range);
  if (math_parser.Value() && math_parser.Value()->Category() == kCalcLength) {
    return math_parser.ConsumeValue();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumePercent(CSSParserTokenRange& range,
                                  const CSSParserContext& context,
                                  CSSPrimitiveValue::ValueRange value_range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kPercentageToken) {
    if (value_range == CSSPrimitiveValue::ValueRange::kNonNegative &&
        token.NumericValue() < 0) {
      return nullptr;
    }
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kPercentage);
  }
  MathFunctionParser math_parser(range, context, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() == kCalcPercent) {
      return math_parser.ConsumeValue();
    }
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeNumberOrPercent(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_range) {
  if (CSSPrimitiveValue* value = ConsumeNumber(range, context, value_range)) {
    return value;
  }
  if (CSSPrimitiveValue* value = ConsumePercent(range, context, value_range)) {
    return CSSNumericLiteralValue::Create(value->GetDoubleValue() / 100.0,
                                          CSSPrimitiveValue::UnitType::kNumber);
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeAlphaValue(CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  return ConsumeNumberOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kAll);
}

bool CanConsumeCalcValue(CalculationResultCategory category,
                         CSSParserMode css_parser_mode) {
  return category == kCalcLength || category == kCalcPercent ||
         category == kCalcPercentLength ||
         (css_parser_mode == kSVGAttributeMode && category == kCalcNumber);
}

CSSPrimitiveValue* ConsumeLengthOrPercent(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_range,
    UnitlessQuirk unitless,
    CSSAnchorQueryTypes allowed_anchor_queries) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken || token.GetType() == kNumberToken) {
    return ConsumeLength(range, context, value_range, unitless);
  }
  if (token.GetType() == kPercentageToken) {
    return ConsumePercent(range, context, value_range);
  }
  MathFunctionParser math_parser(range, context, value_range,
                                 true /* is_percentage_allowed */,
                                 allowed_anchor_queries);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (CanConsumeCalcValue(calculation->Category(), context.Mode())) {
      return math_parser.ConsumeValue();
    }
  }
  return nullptr;
}

namespace {

bool IsNonZeroUserUnitsValue(const CSSPrimitiveValue* value) {
  if (!value) {
    return false;
  }
  if (const auto* numeric_literal = DynamicTo<CSSNumericLiteralValue>(value)) {
    return numeric_literal->GetType() ==
               CSSPrimitiveValue::UnitType::kUserUnits &&
           value->GetDoubleValue() != 0;
  }
  const auto& math_value = To<CSSMathFunctionValue>(*value);
  return math_value.Category() == kCalcNumber && math_value.DoubleValue() != 0;
}

}  // namespace

CSSPrimitiveValue* ConsumeSVGGeometryPropertyLength(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_range) {
  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  CSSPrimitiveValue* value = ConsumeLengthOrPercent(range, context, value_range,
                                                    UnitlessQuirk::kForbid);
  if (IsNonZeroUserUnitsValue(value)) {
    context.Count(WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue);
  }
  return value;
}

CSSPrimitiveValue* ConsumeGradientLengthOrPercent(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_range,
    UnitlessQuirk unitless) {
  return ConsumeLengthOrPercent(range, context, value_range, unitless);
}

static CSSPrimitiveValue* ConsumeNumericLiteralAngle(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    absl::optional<WebFeature> unitless_zero_feature) {
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
    context.Count(*unitless_zero_feature);
    return CSSNumericLiteralValue::Create(
        0, CSSPrimitiveValue::UnitType::kDegrees);
  }
  return nullptr;
}

static CSSPrimitiveValue* ConsumeMathFunctionAngle(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    double minimum_value,
    double maximum_value) {
  MathFunctionParser math_parser(range, context,
                                 CSSPrimitiveValue::ValueRange::kAll);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() != kCalcAngle) {
      return nullptr;
    }
  }
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
  return nullptr;
}

static CSSPrimitiveValue* ConsumeMathFunctionAngle(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  MathFunctionParser math_parser(range, context,
                                 CSSPrimitiveValue::ValueRange::kAll);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() != kCalcAngle) {
      return nullptr;
    }
  }
  return math_parser.ConsumeValue();
}

CSSPrimitiveValue* ConsumeAngle(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    absl::optional<WebFeature> unitless_zero_feature,
    double minimum_value,
    double maximum_value) {
  if (auto* result =
          ConsumeNumericLiteralAngle(range, context, unitless_zero_feature)) {
    return result;
  }

  return ConsumeMathFunctionAngle(range, context, minimum_value, maximum_value);
}

CSSPrimitiveValue* ConsumeAngle(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    absl::optional<WebFeature> unitless_zero_feature) {
  if (auto* result =
          ConsumeNumericLiteralAngle(range, context, unitless_zero_feature)) {
    return result;
  }

  return ConsumeMathFunctionAngle(range, context);
}

CSSPrimitiveValue* ConsumeTime(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               CSSPrimitiveValue::ValueRange value_range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken) {
    if (value_range == CSSPrimitiveValue::ValueRange::kNonNegative &&
        token.NumericValue() < 0) {
      return nullptr;
    }
    CSSPrimitiveValue::UnitType unit = token.GetUnitType();
    if (unit == CSSPrimitiveValue::UnitType::kMilliseconds ||
        unit == CSSPrimitiveValue::UnitType::kSeconds) {
      return CSSNumericLiteralValue::Create(
          range.ConsumeIncludingWhitespace().NumericValue(),
          token.GetUnitType());
    }
    return nullptr;
  }
  MathFunctionParser math_parser(range, context, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() == kCalcTime) {
      return math_parser.ConsumeValue();
    }
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeResolution(CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  if (const CSSParserToken& token = range.Peek();
      token.GetType() == kDimensionToken) {
    CSSPrimitiveValue::UnitType unit = token.GetUnitType();
    if (!CSSPrimitiveValue::IsResolution(unit) || token.NumericValue() < 0.0) {
      // "The allowed range of <resolution> values always excludes negative
      // values"
      // https://www.w3.org/TR/css-values-4/#resolution-value

      return nullptr;
    }

    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), unit);
  }

  MathFunctionParser math_parser(range, context,
                                 CSSPrimitiveValue::ValueRange::kNonNegative);
  const CSSMathFunctionValue* math_value = math_parser.Value();
  if (math_value && math_value->IsResolution()) {
    return math_parser.ConsumeValue();
  }

  return nullptr;
}

// https://drafts.csswg.org/css-values-4/#ratio-value
//
// <ratio> = <number [0,+inf]> [ / <number [0,+inf]> ]?
CSSValue* ConsumeRatio(CSSParserTokenRange& range,
                       const CSSParserContext& context) {
  CSSPrimitiveValue* first = ConsumeNumber(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!first) {
    return nullptr;
  }

  CSSPrimitiveValue* second = nullptr;

  if (css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
    second = ConsumeNumber(range, context,
                           CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!second) {
      return nullptr;
    }
  } else {
    second = CSSNumericLiteralValue::Create(
        1, CSSPrimitiveValue::UnitType::kInteger);
  }

  return MakeGarbageCollected<cssvalue::CSSRatioValue>(*first, *second);
}

CSSIdentifierValue* ConsumeIdent(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken) {
    return nullptr;
  }
  return CSSIdentifierValue::Create(range.ConsumeIncludingWhitespace().Id());
}

CSSIdentifierValue* ConsumeIdentRange(CSSParserTokenRange& range,
                                      CSSValueID lower,
                                      CSSValueID upper) {
  if (range.Peek().Id() < lower || range.Peek().Id() > upper) {
    return nullptr;
  }
  return ConsumeIdent(range);
}

CSSCustomIdentValue* ConsumeCustomIdent(CSSParserTokenRange& range,
                                        const CSSParserContext& context) {
  if (range.Peek().GetType() != kIdentToken ||
      IsCSSWideKeyword(range.Peek().Id()) ||
      range.Peek().Id() == CSSValueID::kDefault) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(
      range.ConsumeIncludingWhitespace().Value().ToAtomicString());
}

CSSCustomIdentValue* ConsumeDashedIdent(CSSParserTokenRange& range,
                                        const CSSParserContext& context) {
  if (range.Peek().GetType() != kIdentToken) {
    return nullptr;
  }
  if (!range.Peek().Value().ToString().StartsWith(kTwoDashes)) {
    return nullptr;
  }

  return ConsumeCustomIdent(range, context);
}

CSSStringValue* ConsumeString(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kStringToken) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSStringValue>(
      range.ConsumeIncludingWhitespace().Value().ToString());
}

StringView ConsumeStringAsStringView(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != CSSParserTokenType::kStringToken) {
    return StringView();
  }

  return range.ConsumeIncludingWhitespace().Value();
}

namespace {

StringView ApplyFetchRestrictions(StringView url,
                                  const CSSParserContext& context) {
  // Invalidate the URL if only data URLs are allowed and the protocol is not
  // data.
  if (!url.IsNull() &&
      context.ResourceFetchRestriction() ==
          ResourceFetchRestriction::kOnlyDataUrls &&
      !ProtocolIs(url.ToString(), "data")) {
    // The StringView must be instantiated with an empty string otherwise the
    // URL will incorrectly be identified as null. The resource should behave as
    // if it failed to load.
    return StringView("");
  }
  return url;
}

}  // namespace

StringView ConsumeUrlAsStringView(CSSParserTokenRange& range,
                                  const CSSParserContext& context) {
  StringView url;
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kUrlToken) {
    range.ConsumeIncludingWhitespace();
    url = token.Value();
  } else if (token.FunctionId() == CSSValueID::kUrl) {
    CSSParserTokenRange url_range = range;
    CSSParserTokenRange url_args = url_range.ConsumeBlock();
    const CSSParserToken& next = url_args.ConsumeIncludingWhitespace();
    if (next.GetType() == kBadStringToken || !url_args.AtEnd()) {
      return StringView();
    }
    DCHECK_EQ(next.GetType(), kStringToken);
    range = url_range;
    range.ConsumeWhitespace();
    url = next.Value();
  }
  return ApplyFetchRestrictions(url, context);
}

cssvalue::CSSURIValue* ConsumeUrl(CSSParserTokenRange& range,
                                  const CSSParserContext& context) {
  StringView url = ConsumeUrlAsStringView(range, context);
  if (url.IsNull()) {
    return nullptr;
  }
  AtomicString url_string = url.ToAtomicString();
  return MakeGarbageCollected<cssvalue::CSSURIValue>(
      CSSUrlData(url_string, context.CompleteNonEmptyURL(url_string)));
}

static bool ConsumeColorInterpolationSpace(
    CSSParserTokenRange& args,
    Color::ColorSpace& color_space,
    Color::HueInterpolationMethod& hue_interpolation) {
  if (!ConsumeIdent<CSSValueID::kIn>(args)) {
    return false;
  }

  absl::optional<Color::ColorSpace> read_color_space;
  if (ConsumeIdent<CSSValueID::kXyz>(args)) {
    read_color_space = Color::ColorSpace::kXYZD65;
  } else if (ConsumeIdent<CSSValueID::kXyzD50>(args)) {
    read_color_space = Color::ColorSpace::kXYZD50;
  } else if (ConsumeIdent<CSSValueID::kXyzD65>(args)) {
    read_color_space = Color::ColorSpace::kXYZD65;
  } else if (ConsumeIdent<CSSValueID::kSRGBLinear>(args)) {
    read_color_space = Color::ColorSpace::kSRGBLinear;
  } else if (ConsumeIdent<CSSValueID::kLab>(args)) {
    read_color_space = Color::ColorSpace::kLab;
  } else if (ConsumeIdent<CSSValueID::kOklab>(args)) {
    read_color_space = Color::ColorSpace::kOklab;
  } else if (ConsumeIdent<CSSValueID::kLch>(args)) {
    read_color_space = Color::ColorSpace::kLch;
  } else if (ConsumeIdent<CSSValueID::kOklch>(args)) {
    read_color_space = Color::ColorSpace::kOklch;
  } else if (ConsumeIdent<CSSValueID::kSRGB>(args)) {
    read_color_space = Color::ColorSpace::kSRGB;
  } else if (ConsumeIdent<CSSValueID::kHsl>(args)) {
    read_color_space = Color::ColorSpace::kHSL;
  } else if (ConsumeIdent<CSSValueID::kHwb>(args)) {
    read_color_space = Color::ColorSpace::kHWB;
  }

  if (read_color_space) {
    color_space = read_color_space.value();
    absl::optional<Color::HueInterpolationMethod> read_hue;
    if (color_space == Color::ColorSpace::kHSL ||
        color_space == Color::ColorSpace::kHWB ||
        color_space == Color::ColorSpace::kLch ||
        color_space == Color::ColorSpace::kOklch) {
      if (ConsumeIdent<CSSValueID::kShorter>(args)) {
        read_hue = Color::HueInterpolationMethod::kShorter;
      } else if (ConsumeIdent<CSSValueID::kLonger>(args)) {
        read_hue = Color::HueInterpolationMethod::kLonger;
      } else if (ConsumeIdent<CSSValueID::kDecreasing>(args)) {
        read_hue = Color::HueInterpolationMethod::kDecreasing;
      } else if (ConsumeIdent<CSSValueID::kIncreasing>(args)) {
        read_hue = Color::HueInterpolationMethod::kIncreasing;
      }
      if (read_hue) {
        if (!ConsumeIdent<CSSValueID::kHue>(args)) {
          return false;
        }
        hue_interpolation = read_hue.value();
      } else {
        // Shorter is the default method for hue interpolation.
        hue_interpolation = Color::HueInterpolationMethod::kShorter;
      }
    }
    return true;
  }

  return false;
}

// https://www.w3.org/TR/css-color-5/#color-mix
static CSSValue* ConsumeColorMixFunction(CSSParserTokenRange& range,
                                         const CSSParserContext& context) {
  DCHECK(range.Peek().FunctionId() == CSSValueID::kColorMix);
  context.Count(WebFeature::kCSSColorMixFunction);

  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  // First argument is the colorspace
  Color::ColorSpace color_space;
  Color::HueInterpolationMethod hue_interpolation_method =
      Color::HueInterpolationMethod::kShorter;
  if (!ConsumeColorInterpolationSpace(args, color_space,
                                      hue_interpolation_method)) {
    return nullptr;
  }

  if (!ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  CSSValue* color1 =
      ConsumeColor(args, context, false /* Accept quirky colors */);
  CSSPrimitiveValue* p1 =
      ConsumePercent(args, context, CSSPrimitiveValue::ValueRange::kAll);
  // Color can come after the percentage
  if (!color1) {
    color1 = ConsumeColor(args, context, false /* Accept quirky colors */);
    if (!color1) {
      return nullptr;
    }
  }
  // Reject negative values and values > 100%, but not calc() values.
  if (p1 && p1->IsNumericLiteralValue() &&
      (p1->GetDoubleValue() < 0.0 || p1->GetDoubleValue() > 100.0)) {
    return nullptr;
  }

  if (!ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  CSSValue* color2 =
      ConsumeColor(args, context, false /* Accept quirky colors */);
  CSSPrimitiveValue* p2 =
      ConsumePercent(args, context, CSSPrimitiveValue::ValueRange::kAll);
  // Color can come after the percentage
  if (!color2) {
    color2 = ConsumeColor(args, context, false /* Accept quirky colors */);
    if (!color2) {
      return nullptr;
    }
  }
  // Reject negative values and values > 100%, but not calc() values.
  if (p2 && p2->IsNumericLiteralValue() &&
      (p2->GetDoubleValue() < 0.0 || p2->GetDoubleValue() > 100.0)) {
    return nullptr;
  }

  // If both values are literally zero (and not calc()) reject at parse time
  if (p1 && p2 && p1->IsNumericLiteralValue() && p1->GetDoubleValue() == 0.0f &&
      p2->IsNumericLiteralValue() && p2->GetDoubleValue() == 0.0) {
    return nullptr;
  }

  if (!args.AtEnd()) {
    return nullptr;
  }

  range = range_copy;

  cssvalue::CSSColorMixValue* result =
      MakeGarbageCollected<cssvalue::CSSColorMixValue>(
          color1, color2, p1, p2, color_space, hue_interpolation_method);
  return result;
}

static bool ParseHexColor(CSSParserTokenRange& range,
                          Color& result,
                          bool accept_quirky_colors) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kHashToken) {
    if (!Color::ParseHexColor(token.Value(), result)) {
      return false;
    }
  } else if (accept_quirky_colors) {
    String color;
    if (token.GetType() == kNumberToken || token.GetType() == kDimensionToken) {
      if (token.GetNumericValueType() != kIntegerValueType ||
          token.NumericValue() < 0. || token.NumericValue() >= 1000000.) {
        return false;
      }
      if (token.GetType() == kNumberToken) {  // e.g. 112233
        color = String::Format("%d", static_cast<int>(token.NumericValue()));
      } else {  // e.g. 0001FF
        color = String::Number(static_cast<int>(token.NumericValue())) +
                token.Value().ToString();
      }
      while (color.length() < 6) {
        color = "0" + color;
      }
    } else if (token.GetType() == kIdentToken) {  // e.g. FF0000
      color = token.Value().ToString();
    }
    unsigned length = color.length();
    if (length != 3 && length != 6) {
      return false;
    }
    if (!Color::ParseHexColor(color, result)) {
      return false;
    }
  } else {
    return false;
  }
  range.ConsumeIncludingWhitespace();
  return true;
}

namespace {

// TODO(crbug.com/1111385): Remove this when we move color-contrast()
// representation to ComputedStyle. This method does not handle currentColor
// correctly.
Color ResolveColor(CSSValue* value) {
  if (auto* color = DynamicTo<cssvalue::CSSColor>(value)) {
    return color->Value();
  }

  if (auto* color = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID color_id = color->GetValueID();
    DCHECK(StyleColor::IsColorKeyword(color_id));
    return StyleColor::ColorFromKeyword(color_id,
                                        mojom::blink::ColorScheme::kLight);
  }

  NOTREACHED();
  return Color();
}

}  // namespace

CSSValue* ConsumeColorContrast(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               bool accept_quirky_colors) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueID::kColorContrast);

  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);

  CSSValue* background_color =
      ConsumeColor(args, context, accept_quirky_colors);
  if (!background_color) {
    return nullptr;
  }

  if (!ConsumeIdent<CSSValueID::kVs>(args)) {
    return nullptr;
  }

  VectorOf<CSSValue> colors_to_compare_against;
  do {
    CSSValue* color = ConsumeColor(args, context, accept_quirky_colors);
    if (!color) {
      return nullptr;
    }
    colors_to_compare_against.push_back(color);
  } while (ConsumeCommaIncludingWhitespace(args));

  if (colors_to_compare_against.size() < 2) {
    return nullptr;
  }

  absl::optional<double> target_contrast;
  if (ConsumeIdent<CSSValueID::kTo>(args)) {
    double target_contrast_temp;
    if (ConsumeIdent<CSSValueID::kAA>(args)) {
      target_contrast = 4.5;
    } else if (ConsumeIdent<CSSValueID::kAALarge>(args)) {
      target_contrast = 3;
    } else if (ConsumeIdent<CSSValueID::kAAA>(args)) {
      target_contrast = 7;
    } else if (ConsumeIdent<CSSValueID::kAAALarge>(args)) {
      target_contrast = 4.5;
    } else if (ConsumeNumberRaw(args, context, target_contrast_temp)) {
      target_contrast = target_contrast_temp;
    } else {
      return nullptr;
    }
  }

  // Bail out if there is any trailing stuff after we parse everything
  if (!args.AtEnd()) {
    return nullptr;
  }

  // TODO(crbug.com/1111385): Represent |background_color| and
  // |colors_to_compare_against| in ComputedStyle and evaluate with currentColor
  // and other variables at used-value time instead of doing it at parse time
  // below.

  SkColor4f resolved_background_color =
      ResolveColor(background_color).toSkColor4f();
  int highest_contrast_index = -1;
  float highest_contrast_ratio = 0;
  for (unsigned i = 0; i < colors_to_compare_against.size(); i++) {
    float contrast_ratio = color_utils::GetContrastRatio(
        resolved_background_color,
        ResolveColor(colors_to_compare_against[i]).toSkColor4f());
    if (target_contrast.has_value()) {
      if (contrast_ratio >= target_contrast.value()) {
        highest_contrast_ratio = contrast_ratio;
        highest_contrast_index = i;
        break;
      }
    } else if (contrast_ratio > highest_contrast_ratio) {
      highest_contrast_ratio = contrast_ratio;
      highest_contrast_index = i;
    }
  }

  range = range_copy;

  if (highest_contrast_index < 0) {
    // If an explicit target contrast was set and no provided colors have enough
    // contrast, then return white or black depending on which has the most
    // contrast.
    return color_utils::GetContrastRatio(resolved_background_color,
                                         SkColors::kWhite) >
                   color_utils::GetContrastRatio(resolved_background_color,
                                                 SkColors::kBlack)
               ? MakeGarbageCollected<cssvalue::CSSColor>(Color::kWhite)
               : MakeGarbageCollected<cssvalue::CSSColor>(Color::kBlack);
  }

  return MakeGarbageCollected<cssvalue::CSSColor>(
      ResolveColor(colors_to_compare_against[highest_contrast_index]));
}

CSSValue* ConsumeColor(CSSParserTokenRange& range,
                       const CSSParserContext& context,
                       bool accept_quirky_colors,
                       AllowedColorKeywords allowed_keywords) {
  if (RuntimeEnabledFeatures::CSSColorContrastEnabled() &&
      range.Peek().FunctionId() == CSSValueID::kColorContrast) {
    return ConsumeColorContrast(range, context, accept_quirky_colors);
  }

  if (range.Peek().FunctionId() == CSSValueID::kColorMix) {
    CSSValue* color = ConsumeColorMixFunction(range, context);
    return color;
  }

  CSSValueID id = range.Peek().Id();
  if ((id == CSSValueID::kAccentcolor || id == CSSValueID::kAccentcolortext) &&
      !RuntimeEnabledFeatures::CSSSystemAccentColorEnabled()) {
    return nullptr;
  }
  if (StyleColor::IsColorKeyword(id)) {
    if (!isValueAllowedInMode(id, context.Mode())) {
      return nullptr;
    }
    if (allowed_keywords != AllowedColorKeywords::kAllowSystemColor &&
        (StyleColor::IsSystemColorIncludingDeprecated(id) ||
         StyleColor::IsSystemColor(id))) {
      return nullptr;
    }
    CSSIdentifierValue* color = ConsumeIdent(range);
    return color;
  }

  Color color = Color::kTransparent;
  if (ParseHexColor(range, color, accept_quirky_colors)) {
    return cssvalue::CSSColor::Create(color);
  }

  // Parses the color inputs rgb(), rgba(), hsl(), hsla(), hwb(), lab(),
  // oklab(), lch(), oklch() and color(). https://www.w3.org/TR/css-color-4/
  ColorFunctionParser parser;
  if (parser.ConsumeFunctionalSyntaxColor(range, context, color)) {
    return cssvalue::CSSColor::Create(color);
  }

  return ConsumeInternalLightDark(ConsumeColor, range, context,
                                  false /* accept_quirky_colors */,
                                  allowed_keywords);
}

CSSValue* ConsumeLineWidth(CSSParserTokenRange& range,
                           const CSSParserContext& context,
                           UnitlessQuirk unitless) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kThin || id == CSSValueID::kMedium ||
      id == CSSValueID::kThick) {
    return ConsumeIdent(range);
  }
  return ConsumeLength(range, context,
                       CSSPrimitiveValue::ValueRange::kNonNegative, unitless);
}

static CSSValue* ConsumePositionComponent(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          UnitlessQuirk unitless,
                                          bool& horizontal_edge,
                                          bool& vertical_edge) {
  if (range.Peek().GetType() != kIdentToken) {
    return ConsumeLengthOrPercent(
        range, context, CSSPrimitiveValue::ValueRange::kAll, unitless);
  }

  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kLeft || id == CSSValueID::kRight) {
    if (horizontal_edge) {
      return nullptr;
    }
    horizontal_edge = true;
  } else if (id == CSSValueID::kTop || id == CSSValueID::kBottom) {
    if (vertical_edge) {
      return nullptr;
    }
    vertical_edge = true;
  } else if (id != CSSValueID::kCenter) {
    return nullptr;
  }
  return ConsumeIdent(range);
}

static bool IsHorizontalPositionKeywordOnly(const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return false;
  }
  CSSValueID value_id = identifier_value->GetValueID();
  return value_id == CSSValueID::kLeft || value_id == CSSValueID::kRight;
}

static bool IsVerticalPositionKeywordOnly(const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return false;
  }
  CSSValueID value_id = identifier_value->GetValueID();
  return value_id == CSSValueID::kTop || value_id == CSSValueID::kBottom;
}

static void PositionFromOneValue(CSSValue* value,
                                 CSSValue*& result_x,
                                 CSSValue*& result_y) {
  bool value_applies_to_y_axis_only = IsVerticalPositionKeywordOnly(*value);
  result_x = value;
  result_y = CSSIdentifierValue::Create(CSSValueID::kCenter);
  if (value_applies_to_y_axis_only) {
    std::swap(result_x, result_y);
  }
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
  if (must_order_as_yx) {
    std::swap(result_x, result_y);
  }
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
    if (!result_x) {
      result_x = center;
    } else {
      result_y = center;
    }
  }

  DCHECK(result_x && result_y);
}

bool ConsumePosition(CSSParserTokenRange& range,
                     const CSSParserContext& context,
                     UnitlessQuirk unitless,
                     absl::optional<WebFeature> three_value_position,
                     CSSValue*& result_x,
                     CSSValue*& result_y) {
  bool horizontal_edge = false;
  bool vertical_edge = false;
  CSSValue* value1 = ConsumePositionComponent(range, context, unitless,
                                              horizontal_edge, vertical_edge);
  if (!value1) {
    return false;
  }
  if (!value1->IsIdentifierValue()) {
    horizontal_edge = true;
  }

  CSSParserTokenRange range_after_first_consume = range;
  CSSValue* value2 = ConsumePositionComponent(range, context, unitless,
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
           : identifier_value1->GetValueID()) != CSSValueID::kCenter) {
    value3 = ConsumePositionComponent(range, context, unitless, horizontal_edge,
                                      vertical_edge);
  }
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
      range.Peek().GetType() != kIdentToken) {
    value4 = ConsumePositionComponent(range, context, unitless, horizontal_edge,
                                      vertical_edge);
  }

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
                              absl::optional<WebFeature> three_value_position) {
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
                                   const CSSParserContext& context,
                                   UnitlessQuirk unitless,
                                   CSSValue*& result_x,
                                   CSSValue*& result_y) {
  bool horizontal_edge = false;
  bool vertical_edge = false;
  CSSValue* value1 = ConsumePositionComponent(range, context, unitless,
                                              horizontal_edge, vertical_edge);
  if (!value1) {
    return false;
  }
  if (!value1->IsIdentifierValue()) {
    horizontal_edge = true;
  }

  if (vertical_edge &&
      ConsumeLengthOrPercent(range, context,
                             CSSPrimitiveValue::ValueRange::kAll, unitless)) {
    // <length-percentage> is not permitted after top | bottom.
    return false;
  }
  CSSValue* value2 = ConsumePositionComponent(range, context, unitless,
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
      result_width = ConsumeLineWidth(range, context, UnitlessQuirk::kForbid);
      if (result_width) {
        continue;
      }
    }
    if (!result_style) {
      result_style = ParseLonghand(CSSPropertyID::kBorderLeftStyle,
                                   CSSPropertyID::kBorder, context, range);
      if (result_style) {
        continue;
      }
    }
    if (!result_color) {
      result_color = ConsumeColor(range, context);
      if (result_color) {
        continue;
      }
    }
    break;
  }

  if (!result_width && !result_style && !result_color) {
    return false;
  }

  if (!result_width) {
    result_width = CSSInitialValue::Create();
  }
  if (!result_style) {
    result_style = CSSInitialValue::Create();
  }
  if (!result_color) {
    result_color = CSSInitialValue::Create();
  }
  return true;
}

// This should go away once we drop support for -webkit-gradient
static CSSPrimitiveValue* ConsumeDeprecatedGradientPoint(
    CSSParserTokenRange& args,
    const CSSParserContext& context,
    bool horizontal) {
  if (args.Peek().GetType() == kIdentToken) {
    if ((horizontal && ConsumeIdent<CSSValueID::kLeft>(args)) ||
        (!horizontal && ConsumeIdent<CSSValueID::kTop>(args))) {
      return CSSNumericLiteralValue::Create(
          0., CSSPrimitiveValue::UnitType::kPercentage);
    }
    if ((horizontal && ConsumeIdent<CSSValueID::kRight>(args)) ||
        (!horizontal && ConsumeIdent<CSSValueID::kBottom>(args))) {
      return CSSNumericLiteralValue::Create(
          100., CSSPrimitiveValue::UnitType::kPercentage);
    }
    if (ConsumeIdent<CSSValueID::kCenter>(args)) {
      return CSSNumericLiteralValue::Create(
          50., CSSPrimitiveValue::UnitType::kPercentage);
    }
    return nullptr;
  }
  CSSPrimitiveValue* result =
      ConsumePercent(args, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!result) {
    result = ConsumeNumber(args, context, CSSPrimitiveValue::ValueRange::kAll);
  }
  return result;
}

// Used to parse colors for -webkit-gradient(...).
static CSSValue* ConsumeDeprecatedGradientStopColor(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  if (args.Peek().Id() == CSSValueID::kCurrentcolor) {
    return nullptr;
  }
  return ConsumeColor(args, context);
}

static bool ConsumeDeprecatedGradientColorStop(
    CSSParserTokenRange& range,
    cssvalue::CSSGradientColorStop& stop,
    const CSSParserContext& context) {
  CSSValueID id = range.Peek().FunctionId();
  if (id != CSSValueID::kFrom && id != CSSValueID::kTo &&
      id != CSSValueID::kColorStop) {
    return false;
  }

  CSSParserTokenRange args = ConsumeFunction(range);
  double position;
  if (id == CSSValueID::kFrom || id == CSSValueID::kTo) {
    position = (id == CSSValueID::kFrom) ? 0 : 1;
  } else {
    DCHECK(id == CSSValueID::kColorStop);
    if (CSSPrimitiveValue* percent_value = ConsumePercent(
            args, context, CSSPrimitiveValue::ValueRange::kAll)) {
      position = percent_value->GetDoubleValue() / 100.0;
    } else if (!ConsumeNumberRaw(args, context, position)) {
      return false;
    }

    if (!ConsumeCommaIncludingWhitespace(args)) {
      return false;
    }
  }

  stop.offset_ = CSSNumericLiteralValue::Create(
      position, CSSPrimitiveValue::UnitType::kNumber);
  stop.color_ = ConsumeDeprecatedGradientStopColor(args, context);
  return stop.color_ && args.AtEnd();
}

static CSSValue* ConsumeDeprecatedGradient(CSSParserTokenRange& args,
                                           const CSSParserContext& context) {
  CSSValueID id = args.ConsumeIncludingWhitespace().Id();
  if (id != CSSValueID::kRadial && id != CSSValueID::kLinear) {
    return nullptr;
  }

  if (!ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  const CSSPrimitiveValue* first_x =
      ConsumeDeprecatedGradientPoint(args, context, true);
  if (!first_x) {
    return nullptr;
  }
  const CSSPrimitiveValue* first_y =
      ConsumeDeprecatedGradientPoint(args, context, false);
  if (!first_y) {
    return nullptr;
  }
  if (!ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  // For radial gradients only, we now expect a numeric radius.
  const CSSPrimitiveValue* first_radius = nullptr;
  if (id == CSSValueID::kRadial) {
    first_radius = ConsumeNumber(args, context,
                                 CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!first_radius || !ConsumeCommaIncludingWhitespace(args)) {
      return nullptr;
    }
  }

  const CSSPrimitiveValue* second_x =
      ConsumeDeprecatedGradientPoint(args, context, true);
  if (!second_x) {
    return nullptr;
  }
  const CSSPrimitiveValue* second_y =
      ConsumeDeprecatedGradientPoint(args, context, false);
  if (!second_y) {
    return nullptr;
  }

  // For radial gradients only, we now expect the second radius.
  const CSSPrimitiveValue* second_radius = nullptr;
  if (id == CSSValueID::kRadial) {
    if (!ConsumeCommaIncludingWhitespace(args)) {
      return nullptr;
    }
    second_radius = ConsumeNumber(args, context,
                                  CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!second_radius) {
      return nullptr;
    }
  }

  cssvalue::CSSGradientValue* result;
  if (id == CSSValueID::kRadial) {
    result = MakeGarbageCollected<cssvalue::CSSRadialGradientValue>(
        first_x, first_y, first_radius, second_x, second_y, second_radius,
        cssvalue::kNonRepeating, cssvalue::kCSSDeprecatedRadialGradient);
  } else {
    result = MakeGarbageCollected<cssvalue::CSSLinearGradientValue>(
        first_x, first_y, second_x, second_y, nullptr, cssvalue::kNonRepeating,
        cssvalue::kCSSDeprecatedLinearGradient);
  }
  cssvalue::CSSGradientColorStop stop;
  while (ConsumeCommaIncludingWhitespace(args)) {
    if (!ConsumeDeprecatedGradientColorStop(args, stop, context)) {
      return nullptr;
    }
    result->AddStop(stop);
  }

  return result;
}

static CSSPrimitiveValue* ConsumeGradientAngleOrPercent(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_range,
    UnitlessQuirk) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken || token.GetType() == kNumberToken) {
    return ConsumeAngle(range, context, WebFeature::kUnitlessZeroAngleGradient);
  }
  if (token.GetType() == kPercentageToken) {
    return ConsumePercent(range, context, value_range);
  }
  MathFunctionParser math_parser(range, context, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    CalculationResultCategory category = calculation->Category();
    // TODO(fs): Add and support kCalcPercentAngle?
    if (category == kCalcAngle || category == kCalcPercent) {
      return math_parser.ConsumeValue();
    }
  }
  return nullptr;
}

using PositionFunctor = CSSPrimitiveValue* (*)(CSSParserTokenRange&,
                                               const CSSParserContext&,
                                               CSSPrimitiveValue::ValueRange,
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
    stop.color_ = ConsumeColor(range, context);
    // Two hints in a row are not allowed.
    if (!stop.color_ &&
        (!supports_color_hints || previous_stop_was_color_hint)) {
      return false;
    }
    previous_stop_was_color_hint = !stop.color_;
    stop.offset_ = consume_position_func(range, context,
                                         CSSPrimitiveValue::ValueRange::kAll,
                                         UnitlessQuirk::kForbid);
    if (!stop.color_ && !stop.offset_) {
      return false;
    }
    gradient->AddStop(stop);

    if (!stop.color_ || !stop.offset_) {
      continue;
    }

    // Optional second position.
    stop.offset_ = consume_position_func(range, context,
                                         CSSPrimitiveValue::ValueRange::kAll,
                                         UnitlessQuirk::kForbid);
    if (stop.offset_) {
      gradient->AddStop(stop);
    }
  } while (ConsumeCommaIncludingWhitespace(range));

  // The last color stop cannot be a color hint.
  if (previous_stop_was_color_hint) {
    return false;
  }

  // Must have 2 or more stops to be valid.
  return gradient->StopCount() >= 2;
}

static CSSValue* ConsumeDeprecatedRadialGradient(
    CSSParserTokenRange& args,
    const CSSParserContext& context,
    cssvalue::CSSGradientRepeat repeating) {
  CSSValue* center_x = nullptr;
  CSSValue* center_y = nullptr;
  ConsumeOneOrTwoValuedPosition(args, context, UnitlessQuirk::kForbid, center_x,
                                center_y);
  if ((center_x || center_y) && !ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  const CSSIdentifierValue* shape =
      ConsumeIdent<CSSValueID::kCircle, CSSValueID::kEllipse>(args);
  const CSSIdentifierValue* size_keyword =
      ConsumeIdent<CSSValueID::kClosestSide, CSSValueID::kClosestCorner,
                   CSSValueID::kFarthestSide, CSSValueID::kFarthestCorner,
                   CSSValueID::kContain, CSSValueID::kCover>(args);
  if (!shape) {
    shape = ConsumeIdent<CSSValueID::kCircle, CSSValueID::kEllipse>(args);
  }

  // Or, two lengths or percentages
  const CSSPrimitiveValue* horizontal_size = nullptr;
  const CSSPrimitiveValue* vertical_size = nullptr;
  if (!shape && !size_keyword) {
    horizontal_size = ConsumeLengthOrPercent(
        args, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (horizontal_size) {
      vertical_size = ConsumeLengthOrPercent(
          args, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      if (!vertical_size) {
        return nullptr;
      }
      ConsumeCommaIncludingWhitespace(args);
    }
  } else {
    ConsumeCommaIncludingWhitespace(args);
  }

  cssvalue::CSSGradientValue* result =
      MakeGarbageCollected<cssvalue::CSSRadialGradientValue>(
          center_x, center_y, shape, size_keyword, horizontal_size,
          vertical_size, repeating, cssvalue::kCSSPrefixedRadialGradient);
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

  // First part of grammar, the size/shape/color space clause:
  // [ in <color-space>? &&
  // [[ circle || <length> ] |
  // [ ellipse || [ <length> | <percentage> ]{2} ] |
  // [ [ circle | ellipse] || <size-keyword> ]] ]

  Color::ColorSpace color_space;
  Color::HueInterpolationMethod hue_interpolation_method =
      Color::HueInterpolationMethod::kShorter;
  bool has_color_space = ConsumeColorInterpolationSpace(
      args, color_space, hue_interpolation_method);

  for (int i = 0; i < 3; ++i) {
    if (args.Peek().GetType() == kIdentToken) {
      CSSValueID id = args.Peek().Id();
      if (id == CSSValueID::kCircle || id == CSSValueID::kEllipse) {
        if (shape) {
          return nullptr;
        }
        shape = ConsumeIdent(args);
      } else if (id == CSSValueID::kClosestSide ||
                 id == CSSValueID::kClosestCorner ||
                 id == CSSValueID::kFarthestSide ||
                 id == CSSValueID::kFarthestCorner) {
        if (size_keyword) {
          return nullptr;
        }
        size_keyword = ConsumeIdent(args);
      } else {
        break;
      }
    } else {
      CSSPrimitiveValue* center = ConsumeLengthOrPercent(
          args, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      if (!center) {
        break;
      }
      if (horizontal_size) {
        return nullptr;
      }
      horizontal_size = center;
      center = ConsumeLengthOrPercent(
          args, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      if (center) {
        vertical_size = center;
        ++i;
      }
    }
  }

  // You can specify size as a keyword or a length/percentage, not both.
  if (size_keyword && horizontal_size) {
    return nullptr;
  }
  // Circles must have 0 or 1 lengths.
  if (shape && shape->GetValueID() == CSSValueID::kCircle && vertical_size) {
    return nullptr;
  }
  // Ellipses must have 0 or 2 length/percentages.
  if (shape && shape->GetValueID() == CSSValueID::kEllipse && horizontal_size &&
      !vertical_size) {
    return nullptr;
  }
  // If there's only one size, it must be a length.
  if (!vertical_size && horizontal_size && horizontal_size->IsPercentage()) {
    return nullptr;
  }
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
                    absl::optional<WebFeature>(), center_x, center_y);
    if (!(center_x && center_y)) {
      return nullptr;
    }
    // Right now, CSS radial gradients have the same start and end centers.
  }

  if (!has_color_space) {
    has_color_space = ConsumeColorInterpolationSpace(args, color_space,
                                                     hue_interpolation_method);
  }

  if ((shape || size_keyword || horizontal_size || center_x || center_y ||
       has_color_space) &&
      !ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  cssvalue::CSSGradientValue* result =
      MakeGarbageCollected<cssvalue::CSSRadialGradientValue>(
          center_x, center_y, shape, size_keyword, horizontal_size,
          vertical_size, repeating, cssvalue::kCSSRadialGradient);

  if (has_color_space) {
    result->SetColorInterpolationSpace(color_space, hue_interpolation_method);
    context.Count(WebFeature::kCSSColorGradientColorSpace);
  }

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
  // First part of grammar, the size/shape/color space clause:
  // [ in <color-space>? || [ <angle> | to <side-or-corner> ]?]
  bool expect_comma = true;
  Color::ColorSpace color_space;
  Color::HueInterpolationMethod hue_interpolation_method =
      Color::HueInterpolationMethod::kShorter;
  bool has_color_space = ConsumeColorInterpolationSpace(
      args, color_space, hue_interpolation_method);

  const CSSPrimitiveValue* angle =
      ConsumeAngle(args, context, WebFeature::kUnitlessZeroAngleGradient);
  const CSSIdentifierValue* end_x = nullptr;
  const CSSIdentifierValue* end_y = nullptr;
  if (!angle) {
    // <side-or-corner> parsing
    if (gradient_type == cssvalue::kCSSPrefixedLinearGradient ||
        ConsumeIdent<CSSValueID::kTo>(args)) {
      end_x = ConsumeIdent<CSSValueID::kLeft, CSSValueID::kRight>(args);
      end_y = ConsumeIdent<CSSValueID::kBottom, CSSValueID::kTop>(args);
      if (!end_x && !end_y) {
        if (gradient_type == cssvalue::kCSSLinearGradient) {
          return nullptr;
        }
        end_y = CSSIdentifierValue::Create(CSSValueID::kTop);
        expect_comma = false;
      } else if (!end_x) {
        end_x = ConsumeIdent<CSSValueID::kLeft, CSSValueID::kRight>(args);
      }
    } else {
      // No <angle> or <side-to-corner>
      expect_comma = false;
    }
  }
  // It's possible that the <color-space> comes after the [ <angle> |
  // <side-or-corner> ]
  if (!has_color_space) {
    has_color_space = ConsumeColorInterpolationSpace(args, color_space,
                                                     hue_interpolation_method);
  }

  if (has_color_space) {
    expect_comma = true;
  }

  if (expect_comma && !ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  cssvalue::CSSGradientValue* result =
      MakeGarbageCollected<cssvalue::CSSLinearGradientValue>(
          end_x, end_y, nullptr, nullptr, angle, repeating, gradient_type);

  if (has_color_space) {
    result->SetColorInterpolationSpace(color_space, hue_interpolation_method);
    context.Count(WebFeature::kCSSColorGradientColorSpace);
  }

  return ConsumeGradientColorStops(args, context, result,
                                   ConsumeGradientLengthOrPercent)
             ? result
             : nullptr;
}

static CSSValue* ConsumeConicGradient(CSSParserTokenRange& args,
                                      const CSSParserContext& context,
                                      cssvalue::CSSGradientRepeat repeating) {
  Color::ColorSpace color_space;
  Color::HueInterpolationMethod hue_interpolation_method =
      Color::HueInterpolationMethod::kShorter;
  bool has_color_space = ConsumeColorInterpolationSpace(
      args, color_space, hue_interpolation_method);

  const CSSPrimitiveValue* from_angle = nullptr;
  if (ConsumeIdent<CSSValueID::kFrom>(args)) {
    if (!(from_angle = ConsumeAngle(args, context,
                                    WebFeature::kUnitlessZeroAngleGradient))) {
      return nullptr;
    }
  }

  CSSValue* center_x = nullptr;
  CSSValue* center_y = nullptr;
  if (ConsumeIdent<CSSValueID::kAt>(args)) {
    if (!ConsumePosition(args, context, UnitlessQuirk::kForbid,
                         absl::optional<WebFeature>(), center_x, center_y)) {
      return nullptr;
    }
  }

  if (!has_color_space) {
    has_color_space = ConsumeColorInterpolationSpace(args, color_space,
                                                     hue_interpolation_method);
  }

  // Comma separator required when fromAngle, position or color_space is
  // present.
  if ((from_angle || center_x || center_y || has_color_space) &&
      !ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  auto* result = MakeGarbageCollected<cssvalue::CSSConicGradientValue>(
      center_x, center_y, from_angle, repeating);

  if (has_color_space) {
    result->SetColorInterpolationSpace(color_space, hue_interpolation_method);
    context.Count(WebFeature::kCSSColorGradientColorSpace);
  }

  return ConsumeGradientColorStops(args, context, result,
                                   ConsumeGradientAngleOrPercent)
             ? result
             : nullptr;
}

CSSValue* ConsumeImageOrNone(CSSParserTokenRange& range,
                             const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(range);
  }
  return ConsumeImage(range, context);
}

CSSValue* ConsumeAxis(CSSParserTokenRange& range,
                      const CSSParserContext& context) {
  CSSValueID axis_id = range.Peek().Id();
  if (axis_id == CSSValueID::kX || axis_id == CSSValueID::kY ||
      axis_id == CSSValueID::kZ) {
    ConsumeIdent(range);
    return MakeGarbageCollected<cssvalue::CSSAxisValue>(axis_id);
  }

  CSSValue* x_dimension =
      ConsumeNumber(range, context, CSSPrimitiveValue::ValueRange::kAll);
  CSSValue* y_dimension =
      ConsumeNumber(range, context, CSSPrimitiveValue::ValueRange::kAll);
  CSSValue* z_dimension =
      ConsumeNumber(range, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!x_dimension || !y_dimension || !z_dimension) {
    return nullptr;
  }
  double x = To<CSSPrimitiveValue>(x_dimension)->GetDoubleValue();
  double y = To<CSSPrimitiveValue>(y_dimension)->GetDoubleValue();
  double z = To<CSSPrimitiveValue>(z_dimension)->GetDoubleValue();
  return MakeGarbageCollected<cssvalue::CSSAxisValue>(x, y, z);
}

CSSValue* ConsumeIntrinsicSizeLonghand(CSSParserTokenRange& range,
                                       const CSSParserContext& context) {
  if (css_parsing_utils::IdentMatches<CSSValueID::kNone>(range.Peek().Id())) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (css_parsing_utils::IdentMatches<CSSValueID::kAuto>(range.Peek().Id())) {
    list->Append(*css_parsing_utils::ConsumeIdent(range));
  }
  if (RuntimeEnabledFeatures::CSSContainIntrinsicSizeAutoNoneEnabled() &&
      css_parsing_utils::IdentMatches<CSSValueID::kNone>(range.Peek().Id())) {
    list->Append(*css_parsing_utils::ConsumeIdent(range));
  } else {
    CSSValue* length = css_parsing_utils::ConsumeLength(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!length) {
      return nullptr;
    }
    list->Append(*length);
  }
  return list;
}

static CSSValue* ConsumeCrossFade(CSSParserTokenRange& args,
                                  const CSSParserContext& context) {
  CSSValue* from_image_value = ConsumeImageOrNone(args, context);
  if (!from_image_value || !ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }
  CSSValue* to_image_value = ConsumeImageOrNone(args, context);
  if (!to_image_value || !ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  CSSPrimitiveValue* percentage = nullptr;
  if (CSSPrimitiveValue* percent_value =
          ConsumePercent(args, context, CSSPrimitiveValue::ValueRange::kAll)) {
    percentage = CSSNumericLiteralValue::Create(
        ClampTo<double>(percent_value->GetDoubleValue() / 100.0, 0, 1),
        CSSPrimitiveValue::UnitType::kNumber);
  } else if (CSSPrimitiveValue* number_value = ConsumeNumber(
                 args, context, CSSPrimitiveValue::ValueRange::kAll)) {
    percentage = CSSNumericLiteralValue::Create(
        ClampTo<double>(number_value->GetDoubleValue(), 0, 1),
        CSSPrimitiveValue::UnitType::kNumber);
  }

  if (!percentage) {
    return nullptr;
  }
  return MakeGarbageCollected<cssvalue::CSSCrossfadeValue>(
      from_image_value, to_image_value, percentage);
}

static CSSValue* ConsumePaint(CSSParserTokenRange& args,
                              const CSSParserContext& context) {
  CSSCustomIdentValue* name = ConsumeCustomIdent(args, context);
  if (!name) {
    return nullptr;
  }

  if (args.AtEnd()) {
    return MakeGarbageCollected<CSSPaintValue>(name);
  }

  if (!RuntimeEnabledFeatures::CSSPaintAPIArgumentsEnabled()) {
    // Arguments not enabled, but exists. Invalid.
    return nullptr;
  }

  // Begin parse paint arguments.
  if (!ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  // Consume arguments.
  // TODO(renjieliu): We may want to optimize the implementation by resolve
  // variables early if paint function is registered.
  Vector<CSSParserToken> argument_tokens;
  Vector<scoped_refptr<CSSVariableData>> variable_data;
  while (!args.AtEnd()) {
    if (args.Peek().GetType() != kCommaToken) {
      argument_tokens.AppendVector(ConsumeFunctionArgsOrNot(args));
    } else {
      if (!AddCSSPaintArgument(argument_tokens, &variable_data)) {
        return nullptr;
      }
      argument_tokens.clear();
      if (!ConsumeCommaIncludingWhitespace(args)) {
        return nullptr;
      }
    }
  }
  if (!AddCSSPaintArgument(argument_tokens, &variable_data)) {
    return nullptr;
  }

  return MakeGarbageCollected<CSSPaintValue>(name, variable_data);
}

static CSSValue* ConsumeGeneratedImage(CSSParserTokenRange& range,
                                       const CSSParserContext& context) {
  CSSValueID id = range.Peek().FunctionId();
  if (!IsGeneratedImage(id)) {
    return nullptr;
  }

  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  CSSValue* result = nullptr;
  if (id == CSSValueID::kRadialGradient) {
    result = ConsumeRadialGradient(args, context, cssvalue::kNonRepeating);
  } else if (id == CSSValueID::kRepeatingRadialGradient) {
    result = ConsumeRadialGradient(args, context, cssvalue::kRepeating);
  } else if (id == CSSValueID::kWebkitLinearGradient) {
    context.Count(WebFeature::kDeprecatedWebKitLinearGradient);
    result = ConsumeLinearGradient(args, context, cssvalue::kNonRepeating,
                                   cssvalue::kCSSPrefixedLinearGradient);
  } else if (id == CSSValueID::kWebkitRepeatingLinearGradient) {
    context.Count(WebFeature::kDeprecatedWebKitRepeatingLinearGradient);
    result = ConsumeLinearGradient(args, context, cssvalue::kRepeating,
                                   cssvalue::kCSSPrefixedLinearGradient);
  } else if (id == CSSValueID::kRepeatingLinearGradient) {
    result = ConsumeLinearGradient(args, context, cssvalue::kRepeating,
                                   cssvalue::kCSSLinearGradient);
  } else if (id == CSSValueID::kLinearGradient) {
    result = ConsumeLinearGradient(args, context, cssvalue::kNonRepeating,
                                   cssvalue::kCSSLinearGradient);
  } else if (id == CSSValueID::kWebkitGradient) {
    context.Count(WebFeature::kDeprecatedWebKitGradient);
    result = ConsumeDeprecatedGradient(args, context);
  } else if (id == CSSValueID::kWebkitRadialGradient) {
    context.Count(WebFeature::kDeprecatedWebKitRadialGradient);
    result =
        ConsumeDeprecatedRadialGradient(args, context, cssvalue::kNonRepeating);
  } else if (id == CSSValueID::kWebkitRepeatingRadialGradient) {
    context.Count(WebFeature::kDeprecatedWebKitRepeatingRadialGradient);
    result =
        ConsumeDeprecatedRadialGradient(args, context, cssvalue::kRepeating);
  } else if (id == CSSValueID::kConicGradient) {
    result = ConsumeConicGradient(args, context, cssvalue::kNonRepeating);
  } else if (id == CSSValueID::kRepeatingConicGradient) {
    result = ConsumeConicGradient(args, context, cssvalue::kRepeating);
  } else if (id == CSSValueID::kWebkitCrossFade) {
    result = ConsumeCrossFade(args, context);
  } else if (id == CSSValueID::kPaint) {
    result = context.IsSecureContext() ? ConsumePaint(args, context) : nullptr;
  }
  if (!result || !args.AtEnd()) {
    return nullptr;
  }

  WebFeature feature;
  if (id == CSSValueID::kWebkitCrossFade) {
    feature = WebFeature::kWebkitCrossFade;
  } else if (id == CSSValueID::kPaint) {
    feature = WebFeature::kCSSPaintFunction;
  } else {
    feature = WebFeature::kCSSGradient;
  }
  context.Count(feature);

  range = range_copy;
  return result;
}

static CSSImageValue* CreateCSSImageValueWithReferrer(
    const StringView& uri,
    const CSSParserContext& context) {
  AtomicString raw_value = uri.ToAtomicString();
  auto* image_value = MakeGarbageCollected<CSSImageValue>(
      CSSUrlData(raw_value, context.CompleteNonEmptyURL(raw_value)),
      context.GetReferrer(),
      context.IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse,
      context.IsAdRelated());
  if (context.Mode() == kUASheetMode) {
    image_value->SetInitiator(fetch_initiator_type_names::kUacss);
  }
  return image_value;
}

static CSSImageSetTypeValue* ConsumeImageSetType(CSSParserTokenRange& range) {
  if (!RuntimeEnabledFeatures::CSSImageSetEnabled() ||
      range.Peek().FunctionId() != CSSValueID::kType) {
    return nullptr;
  }

  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);

  auto type = ConsumeStringAsStringView(args);
  if (type.IsNull() || !args.AtEnd()) {
    return nullptr;
  }

  range = range_copy;
  return MakeGarbageCollected<CSSImageSetTypeValue>(type.ToString());
}

static CSSImageSetOptionValue* ConsumeImageSetOption(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    ConsumeGeneratedImagePolicy generated_image_policy) {
  const ConsumeStringUrlImagePolicy string_url_image_policy =
      RuntimeEnabledFeatures::CSSImageSetEnabled()
          ? ConsumeStringUrlImagePolicy::kAllow
          : ConsumeStringUrlImagePolicy::kForbid;
  const ConsumeGeneratedImagePolicy image_set_generated_image_policy =
      RuntimeEnabledFeatures::CSSImageSetEnabled()
          ? generated_image_policy
          : ConsumeGeneratedImagePolicy::kForbid;

  const CSSValue* image = ConsumeImage(
      range, context, image_set_generated_image_policy, string_url_image_policy,
      ConsumeImageSetImagePolicy::kForbid);
  if (!image) {
    return nullptr;
  }

  // Type could appear before or after resolution
  CSSImageSetTypeValue* type = ConsumeImageSetType(range);

  if (!RuntimeEnabledFeatures::CSSImageSetEnabled() &&
      range.Peek().GetType() != kDimensionToken &&
      range.Peek().GetUnitType() != CSSPrimitiveValue::UnitType::kX) {
    return nullptr;
  }

  CSSPrimitiveValue* resolution = ConsumeResolution(range, context);

  if (!type) {
    type = ConsumeImageSetType(range);
  }

  return MakeGarbageCollected<CSSImageSetOptionValue>(image, resolution, type);
}

static CSSValue* ConsumeImageSet(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    ConsumeGeneratedImagePolicy generated_image_policy =
        ConsumeGeneratedImagePolicy::kAllow) {
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);

  auto* image_set = MakeGarbageCollected<CSSImageSetValue>();
  do {
    auto* image_set_option =
        ConsumeImageSetOption(args, context, generated_image_policy);
    if (!image_set_option) {
      return nullptr;
    }

    image_set->Append(*image_set_option);
  } while (ConsumeCommaIncludingWhitespace(args));

  if (!args.AtEnd()) {
    return nullptr;
  }

  switch (range.Peek().FunctionId()) {
    case CSSValueID::kWebkitImageSet:
      context.Count(WebFeature::kWebkitImageSet);
      break;

    case CSSValueID::kImageSet:
      context.Count(WebFeature::kImageSet);
      break;

    default:
      NOTREACHED();
      break;
  }

  range = range_copy;

  return image_set;
}

CSSValue* ConsumeImage(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const ConsumeGeneratedImagePolicy generated_image_policy,
    const ConsumeStringUrlImagePolicy string_url_image_policy,
    const ConsumeImageSetImagePolicy image_set_image_policy) {
  StringView uri = ConsumeUrlAsStringView(range, context);
  if (!uri.IsNull()) {
    return CreateCSSImageValueWithReferrer(uri, context);
  }
  if (string_url_image_policy == ConsumeStringUrlImagePolicy::kAllow) {
    StringView uri_string = ConsumeStringAsStringView(range);
    if (!uri_string.IsNull()) {
      uri_string = ApplyFetchRestrictions(uri_string, context);
      return CreateCSSImageValueWithReferrer(uri_string, context);
    }
  }
  if (range.Peek().GetType() == kFunctionToken) {
    CSSValueID id = range.Peek().FunctionId();
    if (image_set_image_policy == ConsumeImageSetImagePolicy::kAllow &&
        IsImageSet(id)) {
      return ConsumeImageSet(range, context, generated_image_policy);
    }
    if (generated_image_policy == ConsumeGeneratedImagePolicy::kAllow &&
        IsGeneratedImage(id)) {
      return ConsumeGeneratedImage(range, context);
    }
    return ConsumeInternalLightDark(ConsumeImageOrNone, range, context);
  }
  return nullptr;
}

// https://drafts.csswg.org/css-shapes-1/#typedef-shape-box
CSSIdentifierValue* ConsumeShapeBox(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kContentBox, CSSValueID::kPaddingBox,
                      CSSValueID::kBorderBox, CSSValueID::kMarginBox>(range);
}

// https://drafts.csswg.org/css-box-4/#typedef-visual-box
CSSIdentifierValue* ConsumeVisualBox(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kContentBox, CSSValueID::kPaddingBox,
                      CSSValueID::kBorderBox>(range);
}

// https://drafts.csswg.org/css-box-4/#typedef-coord-box
CSSIdentifierValue* ConsumeCoordBox(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kContentBox, CSSValueID::kPaddingBox,
                      CSSValueID::kBorderBox, CSSValueID::kFillBox,
                      CSSValueID::kStrokeBox, CSSValueID::kViewBox>(range);
}

// https://drafts.fxtf.org/css-masking/#typedef-geometry-box
CSSIdentifierValue* ConsumeGeometryBox(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kBorderBox, CSSValueID::kPaddingBox,
                      CSSValueID::kContentBox, CSSValueID::kMarginBox,
                      CSSValueID::kFillBox, CSSValueID::kStrokeBox,
                      CSSValueID::kViewBox>(range);
}

void AddProperty(CSSPropertyID resolved_property,
                 CSSPropertyID current_shorthand,
                 const CSSValue& value,
                 bool important,
                 IsImplicitProperty implicit,
                 HeapVector<CSSPropertyValue, 64>& properties) {
  DCHECK(!IsPropertyAlias(resolved_property));
  DCHECK(implicit == IsImplicitProperty::kNotImplicit ||
         implicit == IsImplicitProperty::kImplicit);

  int shorthand_index = 0;
  bool set_from_shorthand = false;

  if (IsValidCSSPropertyID(current_shorthand)) {
    Vector<StylePropertyShorthand, 4> shorthands;
    getMatchingShorthandsForLonghand(resolved_property, &shorthands);
    set_from_shorthand = true;
    if (shorthands.size() > 1) {
      shorthand_index =
          indexOfShorthandForLonghand(current_shorthand, shorthands);
    }
  }

  properties.push_back(CSSPropertyValue(
      CSSPropertyName(resolved_property), value, important, set_from_shorthand,
      shorthand_index, implicit == IsImplicitProperty::kImplicit));
}

CSSValue* ConsumeTransformValue(CSSParserTokenRange& range,
                                const CSSParserContext& context) {
  bool use_legacy_parsing = false;
  return ConsumeTransformValue(range, context, use_legacy_parsing);
}

CSSValue* ConsumeTransformList(CSSParserTokenRange& range,
                               const CSSParserContext& context) {
  return ConsumeTransformList(range, context, CSSParserLocalContext());
}

CSSValue* ConsumeFilterFunctionList(CSSParserTokenRange& range,
                                    const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(range);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSValue* filter_value = ConsumeUrl(range, context);
    if (!filter_value) {
      filter_value = ConsumeFilterFunction(range, context);
      if (!filter_value) {
        return nullptr;
      }
    }
    list->Append(*filter_value);
  } while (!range.AtEnd());
  return list;
}

void CountKeywordOnlyPropertyUsage(CSSPropertyID property,
                                   const CSSParserContext& context,
                                   CSSValueID value_id) {
  if (!context.IsUseCounterRecordingEnabled()) {
    return;
  }
  switch (property) {
    case CSSPropertyID::kAppearance:
    case CSSPropertyID::kAliasWebkitAppearance: {
      // TODO(crbug.com/924486): Remove warnings after shipping.
      if ((RuntimeEnabledFeatures::
               NonStandardAppearanceValuesHighUsageEnabled() &&
           CSSParserFastPaths::IsNonStandardAppearanceValuesHighUsage(
               value_id)) ||
          (RuntimeEnabledFeatures::
               NonStandardAppearanceValuesLowUsageEnabled() &&
           CSSParserFastPaths::IsNonStandardAppearanceValuesLowUsage(
               value_id))) {
        if (const auto* document = context.GetDocument()) {
          document->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kDeprecation,
              mojom::blink::ConsoleMessageLevel::kWarning,
              String("The keyword '") + getValueName(value_id) +
                  "' specified to an 'appearance' property is not "
                  "standardized. It will be removed in the future."));
          Deprecation::CountDeprecation(
              document->GetExecutionContext(),
              WebFeature::kCSSValueAppearanceNonStandard);
        }
        // We make sure feature is counted even without document context.
        context.Count(WebFeature::kCSSValueAppearanceNonStandard);
      }
      // TODO(crbug.com/1426629): Remove warning after shipping.
      if (RuntimeEnabledFeatures::
              NonStandardAppearanceValueSliderVerticalEnabled() &&
          value_id == CSSValueID::kSliderVertical) {
        if (const auto* document = context.GetDocument()) {
          // TODO(crbug.com/681917): Remove "currently experimental" note when
          // feature FormControlsVerticalWritingModeSupport is in stable.
          document->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kDeprecation,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "The keyword 'slider-vertical' specified to an 'appearance' "
              "property is not standardized. It will be removed in the future "
              "and replaced by vertical writing-mode (currently "
              "experimental)."));
          Deprecation::CountDeprecation(
              document->GetExecutionContext(),
              WebFeature::kCSSValueAppearanceSliderVertical);
        }
        // We make double-sure the feature kCSSValueAppearanceSliderVertical is
        // counted here. It should also be counted below.
        context.Count(WebFeature::kCSSValueAppearanceSliderVertical);
      }
      WebFeature feature;
      if (value_id == CSSValueID::kNone) {
        feature = WebFeature::kCSSValueAppearanceNone;
      } else {
        feature = WebFeature::kCSSValueAppearanceNotNone;
        if (value_id == CSSValueID::kButton) {
          feature = WebFeature::kCSSValueAppearanceButton;
        } else if (value_id == CSSValueID::kCheckbox) {
          feature = WebFeature::kCSSValueAppearanceCheckbox;
        } else if (value_id == CSSValueID::kInnerSpinButton) {
          feature = WebFeature::kCSSValueAppearanceInnerSpinButton;
        } else if (value_id == CSSValueID::kMediaSlider) {
          feature = WebFeature::kCSSValueAppearanceMediaSlider;
        } else if (value_id == CSSValueID::kMediaSliderthumb) {
          feature = WebFeature::kCSSValueAppearanceMediaSliderthumb;
        } else if (value_id == CSSValueID::kMediaVolumeSlider) {
          feature = WebFeature::kCSSValueAppearanceMediaVolumeSlider;
        } else if (value_id == CSSValueID::kMediaVolumeSliderthumb) {
          feature = WebFeature::kCSSValueAppearanceMediaVolumeSliderthumb;
        } else if (value_id == CSSValueID::kMenulist) {
          feature = WebFeature::kCSSValueAppearanceMenulist;
        } else if (value_id == CSSValueID::kMenulistButton) {
          feature = WebFeature::kCSSValueAppearanceMenulistButton;
        } else if (value_id == CSSValueID::kMeter) {
          feature = WebFeature::kCSSValueAppearanceMeter;
        } else if (value_id == CSSValueID::kListbox) {
          feature = WebFeature::kCSSValueAppearanceListbox;
        } else if (value_id == CSSValueID::kProgressBar) {
          feature = WebFeature::kCSSValueAppearanceProgressBar;
        } else if (value_id == CSSValueID::kPushButton) {
          feature = WebFeature::kCSSValueAppearancePushButton;
        } else if (value_id == CSSValueID::kRadio) {
          feature = WebFeature::kCSSValueAppearanceRadio;
        } else if (value_id == CSSValueID::kSearchfieldCancelButton) {
          feature = WebFeature::kCSSValueAppearanceSearchCancel;
        } else if (value_id == CSSValueID::kSquareButton) {
          feature = WebFeature::kCSSValueAppearanceSquareButton;
        } else if (value_id == CSSValueID::kSearchfield) {
          feature = WebFeature::kCSSValueAppearanceSearchField;
        } else if (value_id == CSSValueID::kSliderHorizontal) {
          feature = WebFeature::kCSSValueAppearanceSliderHorizontal;
        } else if (value_id == CSSValueID::kSliderVertical) {
          feature = WebFeature::kCSSValueAppearanceSliderVertical;
        } else if (value_id == CSSValueID::kSliderthumbHorizontal) {
          feature = WebFeature::kCSSValueAppearanceSliderthumbHorizontal;
        } else if (value_id == CSSValueID::kSliderthumbVertical) {
          feature = WebFeature::kCSSValueAppearanceSliderthumbVertical;
        } else if (value_id == CSSValueID::kTextarea) {
          feature = WebFeature::kCSSValueAppearanceTextarea;
        } else if (value_id == CSSValueID::kTextfield) {
          feature = WebFeature::kCSSValueAppearanceTextField;
        } else {
          feature = WebFeature::kCSSValueAppearanceOthers;
        }
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
      if (value_id == CSSValueID::kContents) {
        context.Count(WebFeature::kCSSValueDisplayContents);
      }
      break;
    case CSSPropertyID::kOverflowX:
    case CSSPropertyID::kOverflowY:
      if (value_id == CSSValueID::kOverlay) {
        context.Count(WebFeature::kCSSValueOverflowOverlay);
      }
      break;
    default:
      break;
  }
}

void WarnInvalidKeywordPropertyUsage(CSSPropertyID property,
                                     const CSSParserContext& context,
                                     CSSValueID value_id) {
  if (!context.IsUseCounterRecordingEnabled()) {
    return;
  }
  switch (property) {
    case CSSPropertyID::kAppearance:
    case CSSPropertyID::kAliasWebkitAppearance: {
      // TODO(crbug.com/924486, crbug.com/1426629): Remove warnings after
      // shipping.
      if ((!RuntimeEnabledFeatures::
               NonStandardAppearanceValuesHighUsageEnabled() &&
           CSSParserFastPaths::IsNonStandardAppearanceValuesHighUsage(
               value_id)) ||
          (!RuntimeEnabledFeatures::
               NonStandardAppearanceValuesLowUsageEnabled() &&
           CSSParserFastPaths::IsNonStandardAppearanceValuesLowUsage(
               value_id)) ||
          (!RuntimeEnabledFeatures::
               NonStandardAppearanceValueSliderVerticalEnabled() &&
           value_id == CSSValueID::kSliderVertical)) {
        if (const auto* document = context.GetDocument()) {
          document->AddConsoleMessage(
              MakeGarbageCollected<ConsoleMessage>(
                  mojom::blink::ConsoleMessageSource::kOther,
                  mojom::blink::ConsoleMessageLevel::kWarning,
                  String("The keyword '") + getValueName(value_id) +
                      "' used on the 'appearance' property was deprecated and "
                      "has now been removed. It will no longer have any "
                      "effect."),
              true);
        }
      }
      break;
    }
    default:
      break;
  }
}

const CSSValue* ParseLonghand(CSSPropertyID unresolved_property,
                              CSSPropertyID current_shorthand,
                              const CSSParserContext& context,
                              CSSParserTokenRange& range) {
  CSSPropertyID property_id = ResolveCSSPropertyID(unresolved_property);
  CSSValueID value_id = range.Peek().Id();
  DCHECK(!CSSProperty::Get(property_id).IsShorthand());
  if (CSSParserFastPaths::IsHandledByKeywordFastPath(property_id)) {
    if (CSSParserFastPaths::IsValidKeywordPropertyAndValue(
            property_id, range.Peek().Id(), context.Mode())) {
      CountKeywordOnlyPropertyUsage(property_id, context, value_id);
      return ConsumeIdent(range);
    }
    WarnInvalidKeywordPropertyUsage(property_id, context, value_id);
    return nullptr;
  }

  const auto local_context =
      CSSParserLocalContext()
          .WithAliasParsing(IsPropertyAlias(unresolved_property))
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
    HeapVector<CSSPropertyValue, 64>& properties) {
  DCHECK_EQ(shorthand.length(), 2u);
  const CSSProperty** longhands = shorthand.properties();

  const CSSValue* start =
      ParseLonghand(longhands[0]->PropertyID(), shorthand.id(), context, range);

  if (!start) {
    return false;
  }

  const CSSValue* end =
      ParseLonghand(longhands[1]->PropertyID(), shorthand.id(), context, range);

  if (shorthand.id() == CSSPropertyID::kOverflow && start && end) {
    context.Count(WebFeature::kTwoValuedOverflow);
  }

  if (!end) {
    end = start;
  }
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
    HeapVector<CSSPropertyValue, 64>& properties) {
  DCHECK_EQ(shorthand.length(), 4u);
  const CSSProperty** longhands = shorthand.properties();
  const CSSValue* top =
      ParseLonghand(longhands[0]->PropertyID(), shorthand.id(), context, range);

  if (!top) {
    return false;
  }

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

  if (!right) {
    right = top;
  }
  if (!bottom) {
    bottom = top;
  }
  if (!left) {
    left = right;
  }

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
    HeapVector<CSSPropertyValue, 64>& properties,
    bool use_initial_value_function) {
  // Existing shorthands have at most 6 longhands.
  DCHECK_LE(shorthand.length(), 6u);
  const CSSValue* longhands[6] = {nullptr, nullptr, nullptr,
                                  nullptr, nullptr, nullptr};
  const CSSProperty** shorthand_properties = shorthand.properties();
  do {
    bool found_longhand = false;
    for (size_t i = 0; !found_longhand && i < shorthand.length(); ++i) {
      if (longhands[i]) {
        continue;
      }
      longhands[i] = ParseLonghand(shorthand_properties[i]->PropertyID(),
                                   shorthand.id(), context, range);

      if (longhands[i]) {
        found_longhand = true;
      }
    }
    if (!found_longhand) {
      return false;
    }
  } while (!range.AtEnd());

  for (size_t i = 0; i < shorthand.length(); ++i) {
    if (longhands[i]) {
      AddProperty(shorthand_properties[i]->PropertyID(), shorthand.id(),
                  *longhands[i], important, IsImplicitProperty::kNotImplicit,
                  properties);
    } else {
      const CSSValue* value =
          use_initial_value_function
              ? To<Longhand>(shorthand_properties[i])->InitialValue()
              : CSSInitialValue::Create();
      AddProperty(shorthand_properties[i]->PropertyID(), shorthand.id(), *value,
                  important, IsImplicitProperty::kNotImplicit, properties);
    }
  }
  return true;
}

void AddExpandedPropertyForValue(CSSPropertyID property,
                                 const CSSValue& value,
                                 bool important,
                                 HeapVector<CSSPropertyValue, 64>& properties) {
  const StylePropertyShorthand& shorthand = shorthandForProperty(property);
  unsigned shorthand_length = shorthand.length();
  DCHECK(shorthand_length);
  const CSSProperty** longhands = shorthand.properties();
  for (unsigned i = 0; i < shorthand_length; ++i) {
    AddProperty(longhands[i]->PropertyID(), property, value, important,
                IsImplicitProperty::kNotImplicit, properties);
  }
}

bool IsBaselineKeyword(CSSValueID id) {
  return IdentMatches<CSSValueID::kFirst, CSSValueID::kLast,
                      CSSValueID::kBaseline>(id);
}

bool IsSelfPositionKeyword(CSSValueID id) {
  if (IdentMatches<CSSValueID::kStart, CSSValueID::kEnd, CSSValueID::kCenter,
                   CSSValueID::kSelfStart, CSSValueID::kSelfEnd,
                   CSSValueID::kFlexStart, CSSValueID::kFlexEnd>(id)) {
    return true;
  }
  return RuntimeEnabledFeatures::CSSAnchorPositioningEnabled() &&
         id == CSSValueID::kAnchorCenter;
}

bool IsSelfPositionOrLeftOrRightKeyword(CSSValueID id) {
  return IsSelfPositionKeyword(id) || IsLeftOrRightKeyword(id);
}

bool IsContentPositionKeyword(CSSValueID id) {
  return IdentMatches<CSSValueID::kStart, CSSValueID::kEnd, CSSValueID::kCenter,
                      CSSValueID::kFlexStart, CSSValueID::kFlexEnd>(id);
}

bool IsContentPositionOrLeftOrRightKeyword(CSSValueID id) {
  return IsContentPositionKeyword(id) || IsLeftOrRightKeyword(id);
}

// https://drafts.csswg.org/css-values-4/#css-wide-keywords
bool IsCSSWideKeyword(CSSValueID id) {
  return id == CSSValueID::kInherit || id == CSSValueID::kInitial ||
         id == CSSValueID::kUnset || id == CSSValueID::kRevert ||
         id == CSSValueID::kRevertLayer;
  // This function should match the overload after it.
}

// https://drafts.csswg.org/css-values-4/#css-wide-keywords
bool IsCSSWideKeyword(StringView keyword) {
  return EqualIgnoringASCIICase(keyword, "initial") ||
         EqualIgnoringASCIICase(keyword, "inherit") ||
         EqualIgnoringASCIICase(keyword, "unset") ||
         EqualIgnoringASCIICase(keyword, "revert") ||
         EqualIgnoringASCIICase(keyword, "revert-layer");
  // This function should match the overload before it.
}

// https://drafts.csswg.org/css-cascade/#default
bool IsRevertKeyword(StringView keyword) {
  return EqualIgnoringASCIICase(keyword, "revert");
}

// https://drafts.csswg.org/css-values-4/#identifier-value
bool IsDefaultKeyword(StringView keyword) {
  return EqualIgnoringASCIICase(keyword, "default");
}

// https://drafts.csswg.org/css-syntax/#typedef-hash-token
bool IsHashIdentifier(const CSSParserToken& token) {
  return token.GetType() == kHashToken &&
         token.GetHashTokenType() == kHashTokenId;
}

bool IsDashedIdent(const CSSParserToken& token) {
  if (token.GetType() != kIdentToken) {
    return false;
  }
  DCHECK(!IsCSSWideKeyword(token.Value()));
  return token.Value().ToString().StartsWith(kTwoDashes);
}

CSSValue* ConsumeCSSWideKeyword(CSSParserTokenRange& range) {
  if (!IsCSSWideKeyword(range.Peek().Id())) {
    return nullptr;
  }
  switch (range.ConsumeIncludingWhitespace().Id()) {
    case CSSValueID::kInitial:
      return CSSInitialValue::Create();
    case CSSValueID::kInherit:
      return CSSInheritedValue::Create();
    case CSSValueID::kUnset:
      return cssvalue::CSSUnsetValue::Create();
    case CSSValueID::kRevert:
      return cssvalue::CSSRevertValue::Create();
    case CSSValueID::kRevertLayer:
      return cssvalue::CSSRevertLayerValue::Create();
    default:
      NOTREACHED();
      return nullptr;
  }
}

bool IsTimelineName(const CSSParserToken& token) {
  if (token.GetType() == kStringToken) {
    return true;
  }
  return token.GetType() == kIdentToken &&
         IsCustomIdent<CSSValueID::kNone>(token.Id());
}

CSSValue* ConsumeSelfPositionOverflowPosition(
    CSSParserTokenRange& range,
    IsPositionKeyword is_position_keyword) {
  DCHECK(is_position_keyword);
  CSSValueID id = range.Peek().Id();
  if (IsAuto(id) || IsNormalOrStretch(id)) {
    return ConsumeIdent(range);
  }

  if (CSSValue* baseline = ConsumeBaseline(range)) {
    return baseline;
  }

  CSSIdentifierValue* overflow_position = ConsumeOverflowPositionKeyword(range);
  if (!is_position_keyword(range.Peek().Id())) {
    return nullptr;
  }
  CSSIdentifierValue* self_position = ConsumeIdent(range);
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
  if (IdentMatches<CSSValueID::kNormal>(id)) {
    return MakeGarbageCollected<cssvalue::CSSContentDistributionValue>(
        CSSValueID::kInvalid, range.ConsumeIncludingWhitespace().Id(),
        CSSValueID::kInvalid);
  }

  if (CSSValue* baseline = ConsumeFirstBaseline(range)) {
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

CSSValue* ConsumeAnimationIterationCount(CSSParserTokenRange& range,
                                         const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kInfinite) {
    return ConsumeIdent(range);
  }
  return ConsumeNumber(range, context,
                       CSSPrimitiveValue::ValueRange::kNonNegative);
}

CSSValue* ConsumeAnimationName(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               bool allow_quoted_name) {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(range);
  }

  if (allow_quoted_name && range.Peek().GetType() == kStringToken) {
    // Legacy support for strings in prefixed animations.
    context.Count(WebFeature::kQuotedAnimationName);

    const CSSParserToken& token = range.ConsumeIncludingWhitespace();
    if (EqualIgnoringASCIICase(token.Value(), "none")) {
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    }
    return MakeGarbageCollected<CSSCustomIdentValue>(
        token.Value().ToAtomicString());
  }

  return ConsumeCustomIdent(range, context);
}

CSSValue* ConsumeScrollFunction(CSSParserTokenRange& range,
                                const CSSParserContext& context) {
  if (range.Peek().FunctionId() != CSSValueID::kScroll) {
    return nullptr;
  }
  CSSParserTokenRange block = ConsumeFunction(range);

  CSSValue* scroller = nullptr;
  CSSIdentifierValue* axis = nullptr;

  while (!scroller || !axis) {
    if (block.AtEnd()) {
      break;
    }
    if (!scroller) {
      if ((scroller = ConsumeIdent<CSSValueID::kRoot, CSSValueID::kNearest,
                                   CSSValueID::kSelf>(block))) {
        continue;
      }
    }
    if (!axis) {
      if ((axis = ConsumeIdent<CSSValueID::kBlock, CSSValueID::kInline,
                               CSSValueID::kX, CSSValueID::kY>(block))) {
        continue;
      }
    }
    return nullptr;
  }
  if (!block.AtEnd()) {
    return nullptr;
  }
  // Nullify default values.
  // https://drafts.csswg.org/scroll-animations-1/#valdef-scroll-nearest
  if (scroller && IsIdent(*scroller, CSSValueID::kNearest)) {
    scroller = nullptr;
  }
  // https://drafts.csswg.org/scroll-animations-1/#valdef-scroll-block
  if (axis && IsIdent(*axis, CSSValueID::kBlock)) {
    axis = nullptr;
  }
  return MakeGarbageCollected<cssvalue::CSSScrollValue>(scroller, axis);
}

CSSValue* ConsumeViewFunction(CSSParserTokenRange& range,
                              const CSSParserContext& context) {
  if (range.Peek().FunctionId() != CSSValueID::kView) {
    return nullptr;
  }

  CSSParserTokenRange block = ConsumeFunction(range);
  CSSIdentifierValue* axis = nullptr;
  CSSValue* inset = nullptr;

  while (!axis || !inset) {
    if (block.AtEnd()) {
      break;
    }
    if (!axis) {
      if ((axis = ConsumeIdent<CSSValueID::kBlock, CSSValueID::kInline,
                               CSSValueID::kX, CSSValueID::kY>(block))) {
        continue;
      }
    }
    if (!inset) {
      if ((inset = ConsumeSingleTimelineInset(block, context))) {
        continue;
      }
    }
    return nullptr;
  }

  if (!block.AtEnd()) {
    return nullptr;
  }

  // Nullify default values.
  // https://drafts.csswg.org/scroll-animations-1/#valdef-scroll-block
  if (axis && IsIdent(*axis, CSSValueID::kBlock)) {
    axis = nullptr;
  }
  if (inset) {
    auto* inset_pair = DynamicTo<CSSValuePair>(inset);
    if (IsIdent(inset_pair->First(), CSSValueID::kAuto) &&
        IsIdent(inset_pair->Second(), CSSValueID::kAuto)) {
      inset = nullptr;
    }
  }

  return MakeGarbageCollected<cssvalue::CSSViewValue>(axis, inset);
}

CSSValue* ConsumeAnimationTimeline(CSSParserTokenRange& range,
                                   const CSSParserContext& context) {
  if (auto* value = ConsumeIdent<CSSValueID::kNone, CSSValueID::kAuto>(range)) {
    return value;
  }
  if (auto* value = ConsumeDashedIdent(range, context)) {
    return value;
  }
  if (auto* value = ConsumeViewFunction(range, context)) {
    return value;
  }
  return ConsumeScrollFunction(range, context);
}

CSSValue* ConsumeAnimationTimingFunction(CSSParserTokenRange& range,
                                         const CSSParserContext& context) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kEase || id == CSSValueID::kLinear ||
      id == CSSValueID::kEaseIn || id == CSSValueID::kEaseOut ||
      id == CSSValueID::kEaseInOut || id == CSSValueID::kStepStart ||
      id == CSSValueID::kStepEnd) {
    return ConsumeIdent(range);
  }

  CSSValueID function = range.Peek().FunctionId();
  if (function == CSSValueID::kLinear &&
      RuntimeEnabledFeatures::CSSLinearTimingFunctionEnabled()) {
    return ConsumeLinear(range, context);
  }
  if (function == CSSValueID::kSteps) {
    return ConsumeSteps(range, context);
  }
  if (function == CSSValueID::kCubicBezier) {
    return ConsumeCubicBezier(range, context);
  }
  return nullptr;
}

CSSValue* ConsumeAnimationDuration(CSSParserTokenRange& range,
                                   const CSSParserContext& context) {
  if (RuntimeEnabledFeatures::ScrollTimelineEnabled()) {
    if (CSSValue* ident = ConsumeIdent<CSSValueID::kAuto>(range)) {
      return ident;
    }
  }
  return ConsumeTime(range, context,
                     CSSPrimitiveValue::ValueRange::kNonNegative);
}

CSSValue* ConsumeTimelineRangeName(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kContain, CSSValueID::kCover,
                      CSSValueID::kEntry, CSSValueID::kEntryCrossing,
                      CSSValueID::kExit, CSSValueID::kExitCrossing>(range);
}

CSSValue* ConsumeTimelineRangeNameAndPercent(CSSParserTokenRange& range,
                                             const CSSParserContext& context) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  CSSValue* range_name = ConsumeTimelineRangeName(range);
  if (!range_name) {
    return nullptr;
  }
  list->Append(*range_name);
  CSSValue* percentage =
      ConsumePercent(range, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!percentage) {
    return nullptr;
  }
  list->Append(*percentage);
  return list;
}

CSSValue* ConsumeAnimationDelay(CSSParserTokenRange& range,
                                const CSSParserContext& context) {
  DCHECK(RuntimeEnabledFeatures::CSSAnimationDelayStartEndEnabled());
  return ConsumeTime(range, context, CSSPrimitiveValue::ValueRange::kAll);
}

CSSValue* ConsumeAnimationRange(CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                double default_offset_percent) {
  DCHECK(RuntimeEnabledFeatures::ScrollTimelineEnabled());
  if (CSSValue* ident = ConsumeIdent<CSSValueID::kNormal>(range)) {
    return ident;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  CSSValue* range_name = ConsumeTimelineRangeName(range);
  if (range_name) {
    list->Append(*range_name);
  }
  CSSPrimitiveValue* percentage = ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
  if (percentage &&
      !(range_name && percentage->IsPercentage() &&
        percentage->GetValue<double>() == default_offset_percent)) {
    list->Append(*percentage);
  } else if (!range_name) {
    return nullptr;
  }
  return list;
}

bool ConsumeAnimationShorthand(
    const StylePropertyShorthand& shorthand,
    HeapVector<Member<CSSValueList>, kMaxNumAnimationLonghands>& longhands,
    ConsumeAnimationItemValue consumeLonghandItem,
    IsResetOnlyFunction is_reset_only,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    bool use_legacy_parsing) {
  DCHECK(consumeLonghandItem);
  const unsigned longhand_count = shorthand.length();
  DCHECK_LE(longhand_count, kMaxNumAnimationLonghands);

  for (unsigned i = 0; i < longhand_count; ++i) {
    longhands[i] = CSSValueList::CreateCommaSeparated();
  }

  do {
    bool parsed_longhand[kMaxNumAnimationLonghands] = {false};
    do {
      bool found_property = false;
      for (unsigned i = 0; i < longhand_count; ++i) {
        if (parsed_longhand[i]) {
          continue;
        }

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
      if (!found_property) {
        return false;
      }
    } while (!range.AtEnd() && range.Peek().GetType() != kCommaToken);

    for (unsigned i = 0; i < longhand_count; ++i) {
      const Longhand& longhand = *To<Longhand>(shorthand.properties()[i]);
      if (!parsed_longhand[i]) {
        // For each longhand that doesn't parse, add the initial (list-item)
        // value instead. However, we only do this *once* for reset-only
        // properties to end up with the initial value for the property as
        // a whole.
        //
        // Example:
        //
        //  animation: anim1, anim2;
        //
        // Should expand to (ignoring longhands other than name and timeline):
        //
        //   animation-name: anim1, anim2;
        //   animation-timeline: auto;
        //
        // It should *not* expand to:
        //
        //   animation-name: anim1, anim2;
        //   animation-timeline: auto, auto;
        //
        if (!is_reset_only(longhand.PropertyID()) || !longhands[i]->length()) {
          longhands[i]->Append(*longhand.InitialValue());
        }
      }
      parsed_longhand[i] = false;
    }
  } while (ConsumeCommaIncludingWhitespace(range));

  return true;
}

CSSValue* ConsumeSingleTimelineAxis(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kBlock, CSSValueID::kInline, CSSValueID::kX,
                      CSSValueID::kY>(range);
}

CSSValue* ConsumeSingleTimelineName(CSSParserTokenRange& range,
                                    const CSSParserContext& context) {
  if (CSSValue* value = ConsumeIdent<CSSValueID::kNone>(range)) {
    return value;
  }
  return ConsumeDashedIdent(range, context);
}

namespace {

CSSValue* ConsumeSingleTimelineInsetSide(CSSParserTokenRange& range,
                                         const CSSParserContext& context) {
  if (CSSValue* ident = ConsumeIdent<CSSValueID::kAuto>(range)) {
    return ident;
  }
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kAll);
}

}  // namespace

CSSValue* ConsumeSingleTimelineInset(CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  CSSValue* start = ConsumeSingleTimelineInsetSide(range, context);
  if (!start) {
    return nullptr;
  }
  CSSValue* end = ConsumeSingleTimelineInsetSide(range, context);
  if (!end) {
    end = start;
  }
  return MakeGarbageCollected<CSSValuePair>(start, end,
                                            CSSValuePair::kDropIdenticalValues);
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
  return ConsumeIdent<CSSValueID::kScroll, CSSValueID::kFixed,
                      CSSValueID::kLocal>(range);
}

CSSValue* ConsumeBackgroundBlendMode(CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNormal || id == CSSValueID::kOverlay ||
      (id >= CSSValueID::kMultiply && id <= CSSValueID::kLuminosity)) {
    return ConsumeIdent(range);
  }
  return nullptr;
}

CSSValue* ConsumeBackgroundBox(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kBorderBox, CSSValueID::kPaddingBox,
                      CSSValueID::kContentBox>(range);
}

CSSValue* ConsumeBackgroundBoxOrText(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kBorderBox, CSSValueID::kPaddingBox,
                      CSSValueID::kContentBox, CSSValueID::kText>(range);
}

CSSValue* ConsumeMaskComposite(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kAdd, CSSValueID::kSubtract,
                      CSSValueID::kIntersect, CSSValueID::kExclude>(range);
}

CSSValue* ConsumePrefixedMaskComposite(CSSParserTokenRange& range) {
  return ConsumeIdentRange(range, CSSValueID::kClear, CSSValueID::kPlusLighter);
}

CSSValue* ConsumeMaskMode(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kAlpha, CSSValueID::kLuminance,
                      CSSValueID::kMatchSource>(range);
}

CSSPrimitiveValue* ConsumeLengthOrPercentCountNegative(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    absl::optional<WebFeature> negative_size) {
  CSSPrimitiveValue* result = ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative,
      UnitlessQuirk::kForbid);
  if (!result && negative_size) {
    context.Count(*negative_size);
  }
  return result;
}

CSSValue* ConsumeBackgroundSize(CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                absl::optional<WebFeature> negative_size,
                                ParsingStyle parsing_style) {
  if (IdentMatches<CSSValueID::kContain, CSSValueID::kCover>(
          range.Peek().Id())) {
    return ConsumeIdent(range);
  }

  CSSValue* horizontal = ConsumeIdent<CSSValueID::kAuto>(range);
  if (!horizontal) {
    horizontal =
        ConsumeLengthOrPercentCountNegative(range, context, negative_size);
  }
  if (!horizontal) {
    return nullptr;
  }

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
  if (!vertical) {
    return horizontal;
  }
  return MakeGarbageCollected<CSSValuePair>(horizontal, vertical,
                                            CSSValuePair::kKeepIdenticalValues);
}

static void SetAllowsNegativePercentageReference(CSSValue* value) {
  if (auto* math_value = DynamicTo<CSSMathFunctionValue>(value)) {
    math_value->SetAllowsNegativePercentageReference();
  }
}

bool ConsumeBackgroundPosition(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               UnitlessQuirk unitless,
                               absl::optional<WebFeature> three_value_position,
                               CSSValue*& result_x,
                               CSSValue*& result_y) {
  do {
    CSSValue* position_x = nullptr;
    CSSValue* position_y = nullptr;
    if (!ConsumePosition(range, context, unitless, three_value_position,
                         position_x, position_y)) {
      return false;
    }
    // TODO(crbug.com/825895): So far, 'background-position' is the only
    // property that allows resolving a percentage against a negative value. If
    // we have more of such properties, we should instead pass an additional
    // argument to ask the parser to set this flag.
    SetAllowsNegativePercentageReference(position_x);
    SetAllowsNegativePercentageReference(position_y);
    AddBackgroundValue(result_x, position_x);
    AddBackgroundValue(result_y, position_y);
  } while (ConsumeCommaIncludingWhitespace(range));
  return true;
}

CSSValue* ConsumePrefixedBackgroundBox(CSSParserTokenRange& range,
                                       AllowTextValue allow_text_value) {
  // The values 'border', 'padding' and 'content' are deprecated and do not
  // apply to the version of the property that has the -webkit- prefix removed.
  if (CSSValue* value = ConsumeIdentRange(range, CSSValueID::kBorder,
                                          CSSValueID::kPaddingBox)) {
    return value;
  }
  if (allow_text_value == AllowTextValue::kAllow &&
      range.Peek().Id() == CSSValueID::kText) {
    return ConsumeIdent(range);
  }
  return nullptr;
}

CSSValue* ParseBackgroundBox(CSSParserTokenRange& range,
                             const CSSParserLocalContext& local_context,
                             AllowTextValue alias_allow_text_value) {
  // This is legacy behavior that does not match spec, see crbug.com/604023
  if (local_context.UseAliasParsing()) {
    return ConsumeCommaSeparatedList(ConsumePrefixedBackgroundBox, range,
                                     alias_allow_text_value);
  }
  return ConsumeCommaSeparatedList(ConsumeBackgroundBox, range);
}

CSSValue* ParseBackgroundSize(CSSParserTokenRange& range,
                              const CSSParserContext& context,
                              const CSSParserLocalContext& local_context,
                              absl::optional<WebFeature> negative_size) {
  return ConsumeCommaSeparatedList(
      ConsumeBackgroundSize, range, context, negative_size,
      local_context.UseAliasParsing() ? ParsingStyle::kLegacy
                                      : ParsingStyle::kNotLegacy);
}

CSSValue* ParseMaskSize(CSSParserTokenRange& range,
                        const CSSParserContext& context,
                        const CSSParserLocalContext& local_context,
                        absl::optional<WebFeature> negative_size) {
  return ConsumeCommaSeparatedList(ConsumeBackgroundSize, range, context,
                                   negative_size, ParsingStyle::kNotLegacy);
}

CSSValue* ConsumeCoordBoxOrNoClip(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueID::kNoClip) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeCoordBox(range);
}

namespace {

CSSValue* ConsumeBackgroundComponent(CSSPropertyID resolved_property,
                                     CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  switch (resolved_property) {
    case CSSPropertyID::kBackgroundClip:
      if (RuntimeEnabledFeatures::CSSBackgroundClipUnprefixEnabled()) {
        return ConsumeBackgroundBoxOrText(range);
      } else {
        return ConsumeBackgroundBox(range);
      }
    case CSSPropertyID::kBackgroundAttachment:
      return ConsumeBackgroundAttachment(range);
    case CSSPropertyID::kBackgroundOrigin:
      return ConsumeBackgroundBox(range);
    case CSSPropertyID::kBackgroundImage:
    case CSSPropertyID::kMaskImage:
    case CSSPropertyID::kWebkitMaskImage:
      return ConsumeImageOrNone(range, context);
    case CSSPropertyID::kBackgroundPositionX:
    case CSSPropertyID::kWebkitMaskPositionX:
      return ConsumePositionLonghand<CSSValueID::kLeft, CSSValueID::kRight>(
          range, context);
    case CSSPropertyID::kBackgroundPositionY:
    case CSSPropertyID::kWebkitMaskPositionY:
      return ConsumePositionLonghand<CSSValueID::kTop, CSSValueID::kBottom>(
          range, context);
    case CSSPropertyID::kBackgroundSize:
      return ConsumeBackgroundSize(range, context,
                                   WebFeature::kNegativeBackgroundSize,
                                   ParsingStyle::kNotLegacy);
    case CSSPropertyID::kWebkitMaskSize:
    case CSSPropertyID::kMaskSize:
      return ConsumeBackgroundSize(range, context,
                                   WebFeature::kNegativeMaskSize,
                                   ParsingStyle::kNotLegacy);
    case CSSPropertyID::kBackgroundColor:
      return ConsumeColor(range, context);
    case CSSPropertyID::kMaskClip:
      return ConsumeCoordBoxOrNoClip(range);
    case CSSPropertyID::kWebkitMaskClip:
      return ConsumePrefixedBackgroundBox(range, AllowTextValue::kAllow);
    case CSSPropertyID::kMaskOrigin:
      return ConsumeCoordBox(range);
    case CSSPropertyID::kWebkitMaskOrigin:
      return ConsumePrefixedBackgroundBox(range, AllowTextValue::kForbid);
    case CSSPropertyID::kBackgroundRepeat:
    case CSSPropertyID::kMaskRepeat:
    case CSSPropertyID::kWebkitMaskRepeat:
      return ConsumeRepeatStyleValue(range);
    case CSSPropertyID::kMaskComposite:
      return ConsumeMaskComposite(range);
    case CSSPropertyID::kMaskMode:
      return ConsumeMaskMode(range);
    default:
      return nullptr;
  };
}

const StylePropertyShorthand& WebkitMaskShorthand(CSSPropertyID shorthand_id) {
  if (shorthand_id == CSSPropertyID::kAlternativeMask) {
    return alternativeMaskShorthand();
  }
  return webkitMaskShorthand();
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
                           HeapVector<CSSPropertyValue, 64>& properties) {
  CSSPropertyID shorthand_id = local_context.CurrentShorthand();
  DCHECK(shorthand_id == CSSPropertyID::kBackground ||
         shorthand_id == CSSPropertyID::kWebkitMask ||
         shorthand_id == CSSPropertyID::kAlternativeMask);
  const StylePropertyShorthand& shorthand =
      shorthand_id == CSSPropertyID::kBackground
          ? backgroundShorthand()
          : WebkitMaskShorthand(shorthand_id);

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
        if (parsed_longhand[i]) {
          continue;
        }

        CSSValue* value = nullptr;
        CSSValue* value_y = nullptr;
        const CSSProperty& property = *shorthand.properties()[i];
        if (property.IDEquals(CSSPropertyID::kBackgroundPositionX) ||
            property.IDEquals(CSSPropertyID::kWebkitMaskPositionX)) {
          if (!ConsumePosition(range, context, UnitlessQuirk::kForbid,
                               WebFeature::kThreeValuedPositionBackground,
                               value, value_y)) {
            continue;
          }
          if (value) {
            bg_position_parsed_in_current_layer = true;
          }
        } else if (property.IDEquals(CSSPropertyID::kBackgroundSize) ||
                   property.IDEquals(CSSPropertyID::kWebkitMaskSize) ||
                   property.IDEquals(CSSPropertyID::kMaskSize)) {
          if (!ConsumeSlashIncludingWhitespace(range)) {
            continue;
          }
          value = ConsumeBackgroundSize(
              range, context,
              property.IDEquals(CSSPropertyID::kBackgroundSize)
                  ? WebFeature::kNegativeBackgroundSize
                  : WebFeature::kNegativeMaskSize,
              ParsingStyle::kNotLegacy);
          if (!value || !bg_position_parsed_in_current_layer) {
            return false;
          }
        } else if (property.IDEquals(CSSPropertyID::kBackgroundPositionY) ||
                   property.IDEquals(CSSPropertyID::kWebkitMaskPositionY)) {
          continue;
        } else {
          value =
              ConsumeBackgroundComponent(property.PropertyID(), range, context);
        }
        if (value) {
          if (property.IDEquals(CSSPropertyID::kBackgroundOrigin) ||
              property.IDEquals(CSSPropertyID::kMaskOrigin) ||
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
      if (!found_property) {
        return false;
      }
    } while (!range.AtEnd() && range.Peek().GetType() != kCommaToken);

    // TODO(timloh): This will make invalid longhands, see crbug.com/386459
    for (unsigned i = 0; i < longhand_count; ++i) {
      const CSSProperty& property = *shorthand.properties()[i];

      if (property.IDEquals(CSSPropertyID::kBackgroundColor) &&
          !range.AtEnd()) {
        if (parsed_longhand[i]) {
          return false;  // Colors are only allowed in the last layer.
        }
        continue;
      }

      if (!parsed_longhand[i]) {
        if ((property.IDEquals(CSSPropertyID::kBackgroundClip) ||
             property.IDEquals(CSSPropertyID::kMaskClip) ||
             property.IDEquals(CSSPropertyID::kWebkitMaskClip)) &&
            origin_value) {
          AddBackgroundValue(longhands[i], origin_value);
          continue;
        }

        AddBackgroundValue(longhands[i], CSSInitialValue::Create());
      }
    }
  } while (ConsumeCommaIncludingWhitespace(range));
  if (!range.AtEnd()) {
    return false;
  }

  for (unsigned i = 0; i < longhand_count; ++i) {
    const CSSProperty& property = *shorthand.properties()[i];
    if (property.IDEquals(CSSPropertyID::kBackgroundSize) && longhands[i] &&
        context.UseLegacyBackgroundSizeShorthandBehavior()) {
      continue;
    }
    AddProperty(property.PropertyID(), shorthand.id(), *longhands[i], important,
                implicit ? IsImplicitProperty::kImplicit
                         : IsImplicitProperty::kNotImplicit,
                properties);
  }
  return true;
}

CSSIdentifierValue* ConsumeRepeatStyleIdent(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kRepeat, CSSValueID::kNoRepeat,
                      CSSValueID::kRound, CSSValueID::kSpace>(range);
}

CSSRepeatStyleValue* ConsumeRepeatStyleValue(CSSParserTokenRange& range) {
  if (auto* id = ConsumeIdent<CSSValueID::kRepeatX>(range)) {
    return MakeGarbageCollected<CSSRepeatStyleValue>(id);
  }

  if (auto* id = ConsumeIdent<CSSValueID::kRepeatY>(range)) {
    return MakeGarbageCollected<CSSRepeatStyleValue>(id);
  }

  if (auto* id1 = ConsumeRepeatStyleIdent(range)) {
    if (auto* id2 = ConsumeRepeatStyleIdent(range)) {
      return MakeGarbageCollected<CSSRepeatStyleValue>(id1, id2);
    }

    return MakeGarbageCollected<CSSRepeatStyleValue>(id1);
  }

  return nullptr;
}

CSSValueList* ParseRepeatStyle(CSSParserTokenRange& range) {
  return ConsumeCommaSeparatedList(ConsumeRepeatStyleValue, range);
}

CSSValue* ConsumeWebkitBorderImage(CSSParserTokenRange& range,
                                   const CSSParserContext& context) {
  CSSValue* source = nullptr;
  CSSValue* slice = nullptr;
  CSSValue* width = nullptr;
  CSSValue* outset = nullptr;
  CSSValue* repeat = nullptr;
  if (ConsumeBorderImageComponents(range, context, source, slice, width, outset,
                                   repeat, DefaultFill::kFill)) {
    return CreateBorderImageValue(source, slice, width, outset, repeat);
  }
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
      source = ConsumeImageOrNone(range, context);
      if (source) {
        continue;
      }
    }
    if (!repeat) {
      repeat = ConsumeBorderImageRepeat(range);
      if (repeat) {
        continue;
      }
    }
    if (!slice) {
      slice = ConsumeBorderImageSlice(range, context, default_fill);
      if (slice) {
        DCHECK(!width);
        DCHECK(!outset);
        if (ConsumeSlashIncludingWhitespace(range)) {
          width = ConsumeBorderImageWidth(range, context);
          if (ConsumeSlashIncludingWhitespace(range)) {
            outset = ConsumeBorderImageOutset(range, context);
            if (!outset) {
              return false;
            }
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
  if (!horizontal) {
    return nullptr;
  }
  CSSIdentifierValue* vertical = ConsumeBorderImageRepeatKeyword(range);
  if (!vertical) {
    vertical = horizontal;
  }
  return MakeGarbageCollected<CSSValuePair>(horizontal, vertical,
                                            CSSValuePair::kDropIdenticalValues);
}

CSSValue* ConsumeBorderImageSlice(CSSParserTokenRange& range,
                                  const CSSParserContext& context,
                                  DefaultFill default_fill) {
  bool fill = ConsumeIdent<CSSValueID::kFill>(range);
  CSSValue* slices[4] = {nullptr};

  for (size_t index = 0; index < 4; ++index) {
    CSSPrimitiveValue* value = ConsumePercent(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!value) {
      value = ConsumeNumber(range, context,
                            CSSPrimitiveValue::ValueRange::kNonNegative);
    }
    if (!value) {
      break;
    }
    slices[index] = value;
  }
  if (!slices[0]) {
    return nullptr;
  }
  if (ConsumeIdent<CSSValueID::kFill>(range)) {
    if (fill) {
      return nullptr;
    }
    fill = true;
  }
  Complete4Sides(slices);
  if (default_fill == DefaultFill::kFill) {
    fill = true;
  }
  return MakeGarbageCollected<cssvalue::CSSBorderImageSliceValue>(
      MakeGarbageCollected<CSSQuadValue>(slices[0], slices[1], slices[2],
                                         slices[3],
                                         CSSQuadValue::kSerializeAsQuad),
      fill);
}

CSSValue* ConsumeBorderImageWidth(CSSParserTokenRange& range,
                                  const CSSParserContext& context) {
  CSSValue* widths[4] = {nullptr};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value = ConsumeNumber(range, context,
                          CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!value) {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      value = ConsumeLengthOrPercent(
          range, context, CSSPrimitiveValue::ValueRange::kNonNegative,
          UnitlessQuirk::kForbid);
    }
    if (!value) {
      value = ConsumeIdent<CSSValueID::kAuto>(range);
    }
    if (!value) {
      break;
    }
    widths[index] = value;
  }
  if (!widths[0]) {
    return nullptr;
  }
  Complete4Sides(widths);
  return MakeGarbageCollected<CSSQuadValue>(widths[0], widths[1], widths[2],
                                            widths[3],
                                            CSSQuadValue::kSerializeAsQuad);
}

CSSValue* ConsumeBorderImageOutset(CSSParserTokenRange& range,
                                   const CSSParserContext& context) {
  CSSValue* outsets[4] = {nullptr};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value = ConsumeNumber(range, context,
                          CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!value) {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      value = ConsumeLength(range, context,
                            CSSPrimitiveValue::ValueRange::kNonNegative);
    }
    if (!value) {
      break;
    }
    outsets[index] = value;
  }
  if (!outsets[0]) {
    return nullptr;
  }
  Complete4Sides(outsets);
  return MakeGarbageCollected<CSSQuadValue>(outsets[0], outsets[1], outsets[2],
                                            outsets[3],
                                            CSSQuadValue::kSerializeAsQuad);
}

CSSValue* ParseBorderRadiusCorner(CSSParserTokenRange& range,
                                  const CSSParserContext& context) {
  CSSValue* parsed_value1 = ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!parsed_value1) {
    return nullptr;
  }
  CSSValue* parsed_value2 = ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!parsed_value2) {
    parsed_value2 = parsed_value1;
  }
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
  UnitlessQuirk unitless =
      allow_quirky_lengths ? UnitlessQuirk::kAllow : UnitlessQuirk::kForbid;
  return ConsumeBorderWidth(range, context, unitless);
}

CSSValue* ConsumeShadow(CSSParserTokenRange& range,
                        const CSSParserContext& context,
                        AllowInsetAndSpread inset_and_spread) {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(range);
  }
  return ConsumeCommaSeparatedList(ParseSingleShadow, range, context,
                                   inset_and_spread);
}

CSSShadowValue* ParseSingleShadow(CSSParserTokenRange& range,
                                  const CSSParserContext& context,
                                  AllowInsetAndSpread inset_and_spread) {
  CSSIdentifierValue* style = nullptr;
  CSSValue* color = nullptr;

  if (range.AtEnd()) {
    return nullptr;
  }

  color = ConsumeColor(range, context);
  if (range.Peek().Id() == CSSValueID::kInset) {
    if (inset_and_spread != AllowInsetAndSpread::kAllow) {
      return nullptr;
    }
    style = ConsumeIdent(range);
    if (!color) {
      color = ConsumeColor(range, context);
    }
  }

  CSSPrimitiveValue* horizontal_offset =
      ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!horizontal_offset) {
    return nullptr;
  }

  CSSPrimitiveValue* vertical_offset =
      ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!vertical_offset) {
    return nullptr;
  }

  CSSPrimitiveValue* blur_radius = ConsumeLength(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  CSSPrimitiveValue* spread_distance = nullptr;
  if (blur_radius) {
    if (inset_and_spread == AllowInsetAndSpread::kAllow) {
      spread_distance =
          ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll);
    }
  }

  if (!range.AtEnd()) {
    if (!color) {
      color = ConsumeColor(range, context);
    }
    if (range.Peek().Id() == CSSValueID::kInset) {
      if (inset_and_spread != AllowInsetAndSpread::kAllow || style) {
        return nullptr;
      }
      style = ConsumeIdent(range);
      if (!color) {
        color = ConsumeColor(range, context);
      }
    }
  }
  return MakeGarbageCollected<CSSShadowValue>(horizontal_offset,
                                              vertical_offset, blur_radius,
                                              spread_distance, style, color);
}

CSSValue* ConsumeColumnCount(CSSParserTokenRange& range,
                             const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return ConsumeIdent(range);
  }
  return ConsumePositiveInteger(range, context);
}

CSSValue* ConsumeColumnWidth(CSSParserTokenRange& range,
                             const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return ConsumeIdent(range);
  }
  // Always parse lengths in strict mode here, since it would be ambiguous
  // otherwise when used in the 'columns' shorthand property.
  CSSParserContext::ParserModeOverridingScope scope(context, kHTMLStandardMode);
  CSSPrimitiveValue* column_width = ConsumeLength(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!column_width) {
    return nullptr;
  }
  return column_width;
}

bool ConsumeColumnWidthOrCount(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               CSSValue*& column_width,
                               CSSValue*& column_count) {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    ConsumeIdent(range);
    return true;
  }
  if (!column_width) {
    column_width = ConsumeColumnWidth(range, context);
    if (column_width) {
      return true;
    }
  }
  if (!column_count) {
    column_count = ConsumeColumnCount(range, context);
  }
  return column_count;
}

CSSValue* ConsumeGapLength(CSSParserTokenRange& range,
                           const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNormal) {
    return ConsumeIdent(range);
  }
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative);
}

CSSValue* ConsumeCounter(CSSParserTokenRange& range,
                         const CSSParserContext& context,
                         int default_value) {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(range);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSCustomIdentValue* counter_name = ConsumeCustomIdent(range, context);
    if (!counter_name) {
      return nullptr;
    }
    int value = default_value;
    if (CSSPrimitiveValue* counter_value = ConsumeInteger(range, context)) {
      value = ClampTo<int>(counter_value->GetDoubleValue());
    }
    list->Append(*MakeGarbageCollected<CSSValuePair>(
        counter_name,
        CSSNumericLiteralValue::Create(value,
                                       CSSPrimitiveValue::UnitType::kInteger),
        CSSValuePair::kDropIdenticalValues));
  } while (!range.AtEnd());
  return list;
}

CSSValue* ConsumeMathDepth(CSSParserTokenRange& range,
                           const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kAutoAdd) {
    return ConsumeIdent(range);
  }

  if (CSSPrimitiveValue* integer_value = ConsumeInteger(range, context)) {
    return integer_value;
  }

  CSSValueID function_id = range.Peek().FunctionId();
  if (function_id == CSSValueID::kAdd) {
    CSSParserTokenRange add_args = ConsumeFunction(range);
    CSSValue* value = ConsumeInteger(add_args, context);
    if (value && add_args.AtEnd()) {
      auto* add_value = MakeGarbageCollected<CSSFunctionValue>(function_id);
      add_value->Append(*value);
      return add_value;
    }
  }

  return nullptr;
}

CSSValue* ConsumeFontSize(CSSParserTokenRange& range,
                          const CSSParserContext& context,
                          UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueID::kWebkitXxxLarge) {
    context.Count(WebFeature::kFontSizeWebkitXxxLarge);
  }
  if ((range.Peek().Id() >= CSSValueID::kXxSmall &&
       range.Peek().Id() <= CSSValueID::kWebkitXxxLarge) ||
      range.Peek().Id() == CSSValueID::kMath) {
    return ConsumeIdent(range);
  }
  return ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative, unitless);
}

CSSValue* ConsumeLineHeight(CSSParserTokenRange& range,
                            const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNormal) {
    return ConsumeIdent(range);
  }

  CSSPrimitiveValue* line_height = ConsumeNumber(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (line_height) {
    return line_height;
  }
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative);
}

CSSValue* ConsumePaletteMixFunction(CSSParserTokenRange& range,
                                    const CSSParserContext& context) {
  // Grammar proposal in: https://github.com/w3c/csswg-drafts/issues/8922
  //
  // palette-mix() = palette-mix(<color-interpolation-method> , [ [normal |
  // light | dark | <palette-identifier> | <palette-mix()>] && <percentage
  // [0,100]>? ]#{2})
  DCHECK(RuntimeEnabledFeatures::FontPaletteAnimationEnabled());

  if (range.Peek().FunctionId() != CSSValueID::kPaletteMix) {
    return nullptr;
  }

  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  Color::ColorSpace color_space;
  Color::HueInterpolationMethod hue_interpolation_method =
      Color::HueInterpolationMethod::kShorter;
  if (!ConsumeColorInterpolationSpace(args, color_space,
                                      hue_interpolation_method)) {
    return nullptr;
  }

  auto consume_endpoint_palette_with_percentage =
      [](CSSParserTokenRange& args, const CSSParserContext& context)
      -> std::pair<CSSValue*, CSSPrimitiveValue*> {
    if (!ConsumeCommaIncludingWhitespace(args)) {
      return std::make_pair(nullptr, nullptr);
    }

    CSSValue* palette = ConsumeFontPalette(args, context);
    CSSPrimitiveValue* percentage =
        ConsumePercent(args, context, CSSPrimitiveValue::ValueRange::kAll);
    // Percentage can be followed by a palette.
    if (!palette) {
      palette = ConsumeFontPalette(args, context);
      if (!palette) {
        return std::make_pair(nullptr, nullptr);
      }
    }
    // Reject negative values and values > 100%, but not calc() values.
    if (percentage && percentage->IsNumericLiteralValue() &&
        (percentage->GetDoubleValue() < 0.0 ||
         percentage->GetDoubleValue() > 100.0)) {
      return std::make_pair(nullptr, nullptr);
    }
    return std::make_pair(palette, percentage);
  };

  auto palette_with_percentage_1 =
      consume_endpoint_palette_with_percentage(args, context);
  auto palette_with_percentage_2 =
      consume_endpoint_palette_with_percentage(args, context);
  CSSValue* palette1 = palette_with_percentage_1.first;
  CSSValue* palette2 = palette_with_percentage_2.first;
  CSSPrimitiveValue* percentage1 = palette_with_percentage_1.second;
  CSSPrimitiveValue* percentage2 = palette_with_percentage_2.second;

  if (!palette1 || !palette2) {
    return nullptr;
  }
  // If both values are literally zero (and not calc()) reject at parse time.
  if (percentage1 && percentage2 && percentage1->IsNumericLiteralValue() &&
      percentage1->GetDoubleValue() == 0.0f &&
      percentage2->IsNumericLiteralValue() &&
      percentage2->GetDoubleValue() == 0.0) {
    return nullptr;
  }

  if (!args.AtEnd()) {
    return nullptr;
  }

  range = range_copy;

  return MakeGarbageCollected<cssvalue::CSSPaletteMixValue>(
      palette1, palette2, percentage1, percentage2, color_space,
      hue_interpolation_method);
}

CSSValue* ConsumeFontPalette(CSSParserTokenRange& range,
                             const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNormal ||
      range.Peek().Id() == CSSValueID::kLight ||
      range.Peek().Id() == CSSValueID::kDark) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  if (RuntimeEnabledFeatures::FontPaletteAnimationEnabled() &&
      range.Peek().FunctionId() == CSSValueID::kPaletteMix) {
    return ConsumePaletteMixFunction(range, context);
  }

  return ConsumeDashedIdent(range, context);
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
  } while (ConsumeCommaIncludingWhitespace(range));
  return list;
}

CSSValueList* ConsumeNonGenericFamilyNameList(CSSParserTokenRange& range) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* parsed_value = ConsumeGenericFamily(range);
    // Consume only if all families in the list are regular family names and
    // none of them are generic ones.
    if (parsed_value) {
      return nullptr;
    }
    parsed_value = ConsumeFamilyName(range);
    if (parsed_value) {
      list->Append(*parsed_value);
    } else {
      return nullptr;
    }
  } while (ConsumeCommaIncludingWhitespace(range));
  return list;
}

CSSValue* ConsumeGenericFamily(CSSParserTokenRange& range) {
  return ConsumeIdentRange(range, CSSValueID::kSerif, CSSValueID::kMath);
}

CSSValue* ConsumeFamilyName(CSSParserTokenRange& range) {
  if (range.Peek().GetType() == kStringToken) {
    return CSSFontFamilyValue::Create(
        range.ConsumeIncludingWhitespace().Value().ToAtomicString());
  }
  if (range.Peek().GetType() != kIdentToken) {
    return nullptr;
  }
  String family_name = ConcatenateFamilyName(range);
  if (family_name.IsNull()) {
    return nullptr;
  }
  return CSSFontFamilyValue::Create(AtomicString(family_name));
}

String ConcatenateFamilyName(CSSParserTokenRange& range) {
  StringBuilder builder;
  bool added_space = false;
  const CSSParserToken& first_token = range.Peek();
  while (range.Peek().GetType() == kIdentToken) {
    if (!builder.empty()) {
      builder.Append(' ');
      added_space = true;
    }
    builder.Append(range.ConsumeIncludingWhitespace().Value());
  }
  if (!added_space && (IsCSSWideKeyword(first_token.Value()) ||
                       IsDefaultKeyword(first_token.Value()))) {
    return String();
  }
  return builder.ReleaseString();
}

CSSValueList* CombineToRangeList(const CSSPrimitiveValue* range_start,
                                 const CSSPrimitiveValue* range_end) {
  DCHECK(range_start);
  DCHECK(range_end);
  // Reversed ranges are valid, let them pass through here and swap them in
  // FontFace to keep serialisation of the value as specified.
  // https://drafts.csswg.org/css-fonts/#font-prop-desc
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
                           const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNormal ||
      range.Peek().Id() == CSSValueID::kItalic) {
    return ConsumeIdent(range);
  }

  if (RuntimeEnabledFeatures::CSSFontFaceAutoVariableRangeEnabled() &&
      range.Peek().Id() == CSSValueID::kAuto &&
      context.Mode() == kCSSFontFaceRuleMode) {
    return ConsumeIdent(range);
  }

  if (range.Peek().Id() != CSSValueID::kOblique) {
    return nullptr;
  }

  CSSIdentifierValue* oblique_identifier =
      ConsumeIdent<CSSValueID::kOblique>(range);

  CSSPrimitiveValue* start_angle = ConsumeAngle(
      range, context, absl::nullopt, kMinObliqueValue, kMaxObliqueValue);
  if (!start_angle) {
    return oblique_identifier;
  }
  if (!IsAngleWithinLimits(start_angle)) {
    return nullptr;
  }

  if (context.Mode() != kCSSFontFaceRuleMode || range.AtEnd()) {
    CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
    value_list->Append(*start_angle);
    return MakeGarbageCollected<cssvalue::CSSFontStyleRangeValue>(
        *oblique_identifier, *value_list);
  }

  CSSPrimitiveValue* end_angle = ConsumeAngle(
      range, context, absl::nullopt, kMinObliqueValue, kMaxObliqueValue);
  if (!end_angle || !IsAngleWithinLimits(end_angle)) {
    return nullptr;
  }

  CSSValueList* range_list = CombineToRangeList(start_angle, end_angle);
  if (!range_list) {
    return nullptr;
  }
  return MakeGarbageCollected<cssvalue::CSSFontStyleRangeValue>(
      *oblique_identifier, *range_list);
}

CSSIdentifierValue* ConsumeFontStretchKeywordOnly(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  const CSSParserToken& token = range.Peek();
  if (token.Id() == CSSValueID::kNormal ||
      (token.Id() >= CSSValueID::kUltraCondensed &&
       token.Id() <= CSSValueID::kUltraExpanded)) {
    return ConsumeIdent(range);
  }
  if (RuntimeEnabledFeatures::CSSFontFaceAutoVariableRangeEnabled() &&
      token.Id() == CSSValueID::kAuto &&
      context.Mode() == kCSSFontFaceRuleMode) {
    return ConsumeIdent(range);
  }
  return nullptr;
}

CSSValue* ConsumeFontStretch(CSSParserTokenRange& range,
                             const CSSParserContext& context) {
  CSSIdentifierValue* parsed_keyword =
      ConsumeFontStretchKeywordOnly(range, context);
  if (parsed_keyword) {
    return parsed_keyword;
  }

  CSSPrimitiveValue* start_percent = ConsumePercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!start_percent) {
    return nullptr;
  }

  // In a non-font-face context, more than one percentage is not allowed.
  if (context.Mode() != kCSSFontFaceRuleMode || range.AtEnd()) {
    return start_percent;
  }

  CSSPrimitiveValue* end_percent = ConsumePercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!end_percent) {
    return nullptr;
  }

  return CombineToRangeList(start_percent, end_percent);
}

CSSValue* ConsumeFontWeight(CSSParserTokenRange& range,
                            const CSSParserContext& context) {
  const CSSParserToken& token = range.Peek();
  if (context.Mode() != kCSSFontFaceRuleMode) {
    if (token.Id() >= CSSValueID::kNormal &&
        token.Id() <= CSSValueID::kLighter) {
      return ConsumeIdent(range);
    }
  } else {
    if ((token.Id() == CSSValueID::kNormal ||
         token.Id() == CSSValueID::kBold) ||
        (RuntimeEnabledFeatures::CSSFontFaceAutoVariableRangeEnabled() &&
         token.Id() == CSSValueID::kAuto)) {
      return ConsumeIdent(range);
    }
  }

  // Avoid consuming the first zero of font: 0/0; e.g. in the Acid3 test.  In
  // font:0/0; the first zero is the font size, the second is the line height.
  // In font: 100 0/0; we should parse the first 100 as font-weight, the 0
  // before the slash as font size. We need to peek and check the token in order
  // to avoid parsing a 0 font size as a font-weight. If we call ConsumeNumber
  // straight away without Peek, then the parsing cursor advances too far and we
  // parsed font-size as font-weight incorrectly.
  if (token.GetType() == kNumberToken &&
      (token.NumericValue() < 1 || token.NumericValue() > 1000)) {
    return nullptr;
  }

  CSSPrimitiveValue* start_weight = ConsumeNumber(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!start_weight || start_weight->GetFloatValue() < 1 ||
      start_weight->GetFloatValue() > 1000) {
    return nullptr;
  }

  // In a non-font-face context, more than one number is not allowed. Return
  // what we have. If there is trailing garbage, the AtEnd() check in
  // CSSPropertyParser::ParseValueStart will catch that.
  if (context.Mode() != kCSSFontFaceRuleMode || range.AtEnd()) {
    return start_weight;
  }

  CSSPrimitiveValue* end_weight = ConsumeNumber(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!end_weight || end_weight->GetFloatValue() < 1 ||
      end_weight->GetFloatValue() > 1000) {
    return nullptr;
  }

  return CombineToRangeList(start_weight, end_weight);
}

CSSValue* ConsumeFontFeatureSettings(CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNormal) {
    return ConsumeIdent(range);
  }
  CSSValueList* settings = CSSValueList::CreateCommaSeparated();
  do {
    CSSFontFeatureValue* font_feature_value =
        ConsumeFontFeatureTag(range, context);
    if (!font_feature_value) {
      return nullptr;
    }
    settings->Append(*font_feature_value);
  } while (ConsumeCommaIncludingWhitespace(range));
  return settings;
}

CSSFontFeatureValue* ConsumeFontFeatureTag(CSSParserTokenRange& range,
                                           const CSSParserContext& context) {
  // Feature tag name consists of 4-letter characters.
  const unsigned kTagNameLength = 4;

  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  // Feature tag name comes first
  if (token.GetType() != kStringToken) {
    return nullptr;
  }
  if (token.Value().length() != kTagNameLength) {
    return nullptr;
  }
  AtomicString tag = token.Value().ToAtomicString();
  for (unsigned i = 0; i < kTagNameLength; ++i) {
    // Limits the range of characters to 0x20-0x7E, following the tag name rules
    // defined in the OpenType specification.
    UChar character = tag[i];
    if (character < 0x20 || character > 0x7E) {
      return nullptr;
    }
  }

  int tag_value = 1;
  // Feature tag values could follow: <integer> | on | off
  if (CSSPrimitiveValue* value = ConsumeInteger(range, context, 0)) {
    tag_value = ClampTo<int>(value->GetDoubleValue());
  } else if (range.Peek().Id() == CSSValueID::kOn ||
             range.Peek().Id() == CSSValueID::kOff) {
    tag_value = range.ConsumeIncludingWhitespace().Id() == CSSValueID::kOn;
  }
  return MakeGarbageCollected<CSSFontFeatureValue>(tag, tag_value);
}

CSSIdentifierValue* ConsumeFontVariantCSS21(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kNormal, CSSValueID::kSmallCaps>(range);
}

CSSIdentifierValue* ConsumeFontTechIdent(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kFeaturesOpentype, CSSValueID::kFeaturesAat,
                      CSSValueID::kFeaturesGraphite, CSSValueID::kColorCOLRv0,
                      CSSValueID::kColorCOLRv1, CSSValueID::kColorSVG,
                      CSSValueID::kColorSbix, CSSValueID::kColorCBDT,
                      CSSValueID::kVariations, CSSValueID::kPalettes,
                      CSSValueID::kIncremental>(range);
}

CSSIdentifierValue* ConsumeFontFormatIdent(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kCollection, CSSValueID::kEmbeddedOpentype,
                      CSSValueID::kOpentype, CSSValueID::kTruetype,
                      CSSValueID::kSVG, CSSValueID::kWoff, CSSValueID::kWoff2>(
      range);
}

CSSValueID FontFormatToId(String font_format) {
  CSSValueID converted_id = CssValueKeywordID(font_format);
  if (converted_id == CSSValueID::kCollection ||
      converted_id == CSSValueID::kEmbeddedOpentype ||
      converted_id == CSSValueID::kOpentype ||
      converted_id == CSSValueID::kTruetype ||
      converted_id == CSSValueID::kSVG || converted_id == CSSValueID::kWoff ||
      converted_id == CSSValueID::kWoff2) {
    return converted_id;
  }
  return CSSValueID::kInvalid;
}

bool IsSupportedKeywordTech(CSSValueID keyword) {
  switch (keyword) {
    case CSSValueID::kFeaturesOpentype:
    case CSSValueID::kFeaturesAat:
    case CSSValueID::kColorCOLRv0:
    case CSSValueID::kColorCOLRv1:
    case CSSValueID::kColorSbix:
    case CSSValueID::kColorCBDT:
    case CSSValueID::kVariations:
    case CSSValueID::kPalettes:
      return true;
    case CSSValueID::kFeaturesGraphite:
    case CSSValueID::kColorSVG:
    case CSSValueID::kIncremental:
      return false;
    default:
      return false;
  }
  NOTREACHED();
  return false;
}

bool IsSupportedKeywordFormat(CSSValueID keyword) {
  switch (keyword) {
    case CSSValueID::kCollection:
    case CSSValueID::kOpentype:
    case CSSValueID::kTruetype:
    case CSSValueID::kWoff:
    case CSSValueID::kWoff2:
      return true;
    case CSSValueID::kEmbeddedOpentype:
    case CSSValueID::kSVG:
      return false;
    default:
      return false;
  }
}

Vector<String> ParseGridTemplateAreasColumnNames(const String& grid_row_names) {
  DCHECK(!grid_row_names.empty());

  // Using StringImpl to avoid checks and indirection in every call to
  // String::operator[].
  StringImpl& text = *grid_row_names.Impl();
  StringBuilder area_name;
  Vector<String> column_names;
  for (unsigned i = 0; i < text.length(); ++i) {
    if (IsCSSSpace(text[i])) {
      if (!area_name.empty()) {
        column_names.push_back(area_name.ReleaseString());
      }
      continue;
    }
    if (text[i] == '.') {
      if (area_name == ".") {
        continue;
      }
      if (!area_name.empty()) {
        column_names.push_back(area_name.ReleaseString());
      }
    } else {
      if (!IsNameCodePoint(text[i])) {
        return Vector<String>();
      }
      if (area_name == ".") {
        column_names.push_back(area_name.ReleaseString());
      }
    }
    area_name.Append(text[i]);
  }

  if (!area_name.empty()) {
    column_names.push_back(area_name.ReleaseString());
  }

  return column_names;
}

CSSValue* ConsumeGridBreadth(CSSParserTokenRange& range,
                             const CSSParserContext& context) {
  const CSSParserToken& token = range.Peek();
  if (IdentMatches<CSSValueID::kAuto, CSSValueID::kMinContent,
                   CSSValueID::kMaxContent>(token.Id())) {
    return ConsumeIdent(range);
  }
  if (token.GetType() == kDimensionToken &&
      token.GetUnitType() == CSSPrimitiveValue::UnitType::kFlex) {
    if (token.NumericValue() < 0) {
      return nullptr;
    }
    return CSSNumericLiteralValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kFlex);
  }
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                UnitlessQuirk::kForbid);
}

CSSValue* ConsumeFitContent(CSSParserTokenRange& range,
                            const CSSParserContext& context) {
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  CSSPrimitiveValue* length = ConsumeLengthOrPercent(
      args, context, CSSPrimitiveValue::ValueRange::kNonNegative,
      UnitlessQuirk::kAllow);
  if (!length || !args.AtEnd()) {
    return nullptr;
  }
  range = range_copy;
  auto* result =
      MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kFitContent);
  result->Append(*length);
  return result;
}

bool IsGridBreadthFixedSized(const CSSValue& value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID value_id = identifier_value->GetValueID();
    return value_id != CSSValueID::kAuto &&
           value_id != CSSValueID::kMinContent &&
           value_id != CSSValueID::kMaxContent;
  }

  if (auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    return !primitive_value->IsFlex();
  }

  NOTREACHED();
  return true;
}

bool IsGridTrackFixedSized(const CSSValue& value) {
  if (value.IsPrimitiveValue() || value.IsIdentifierValue()) {
    return IsGridBreadthFixedSized(value);
  }

  auto& function = To<CSSFunctionValue>(value);
  if (function.FunctionType() == CSSValueID::kFitContent) {
    return false;
  }

  const CSSValue& min_value = function.Item(0);
  const CSSValue& max_value = function.Item(1);
  return IsGridBreadthFixedSized(min_value) ||
         IsGridBreadthFixedSized(max_value);
}

CSSValue* ConsumeGridTrackSize(CSSParserTokenRange& range,
                               const CSSParserContext& context) {
  const auto& token_id = range.Peek().FunctionId();

  if (token_id == CSSValueID::kMinmax) {
    CSSParserTokenRange range_copy = range;
    CSSParserTokenRange args = ConsumeFunction(range_copy);
    CSSValue* min_track_breadth = ConsumeGridBreadth(args, context);
    auto* min_track_breadth_primitive_value =
        DynamicTo<CSSPrimitiveValue>(min_track_breadth);
    if (!min_track_breadth ||
        (min_track_breadth_primitive_value &&
         min_track_breadth_primitive_value->IsFlex()) ||
        !ConsumeCommaIncludingWhitespace(args)) {
      return nullptr;
    }
    CSSValue* max_track_breadth = ConsumeGridBreadth(args, context);
    if (!max_track_breadth || !args.AtEnd()) {
      return nullptr;
    }
    range = range_copy;
    auto* result = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kMinmax);
    result->Append(*min_track_breadth);
    result->Append(*max_track_breadth);
    return result;
  }

  return (token_id == CSSValueID::kFitContent)
             ? ConsumeFitContent(range, context)
             : ConsumeGridBreadth(range, context);
}

CSSCustomIdentValue* ConsumeCustomIdentForGridLine(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kAuto ||
      range.Peek().Id() == CSSValueID::kSpan) {
    return nullptr;
  }
  return ConsumeCustomIdent(range, context);
}

// Appends to the passed in CSSBracketedValueList if any, otherwise creates a
// new one. Returns nullptr if an empty list is consumed.
CSSBracketedValueList* ConsumeGridLineNames(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    bool is_subgrid_track_list,
    CSSBracketedValueList* line_names = nullptr) {
  CSSParserTokenRange range_copy = range;
  if (range_copy.ConsumeIncludingWhitespace().GetType() != kLeftBracketToken) {
    return nullptr;
  }

  if (!line_names) {
    line_names = MakeGarbageCollected<CSSBracketedValueList>();
  }

  while (CSSCustomIdentValue* line_name =
             ConsumeCustomIdentForGridLine(range_copy, context)) {
    line_names->Append(*line_name);
  }

  if (range_copy.ConsumeIncludingWhitespace().GetType() != kRightBracketToken) {
    return nullptr;
  }
  range = range_copy;

  if (!is_subgrid_track_list && line_names->length() == 0U) {
    return nullptr;
  }

  return line_names;
}

bool AppendLineNames(CSSParserTokenRange& range,
                     const CSSParserContext& context,
                     bool is_subgrid_track_list,
                     CSSValueList* values) {
  if (CSSBracketedValueList* line_names =
          ConsumeGridLineNames(range, context, is_subgrid_track_list)) {
    values->Append(*line_names);
    return true;
  }
  return false;
}

bool ConsumeGridTrackRepeatFunction(CSSParserTokenRange& range,
                                    const CSSParserContext& context,
                                    bool is_subgrid_track_list,
                                    CSSValueList& list,
                                    bool& is_auto_repeat,
                                    bool& all_tracks_are_fixed_sized) {
  CSSParserTokenRange args = ConsumeFunction(range);
  is_auto_repeat = IdentMatches<CSSValueID::kAutoFill, CSSValueID::kAutoFit>(
      args.Peek().Id());
  CSSValueList* repeated_values;
  // The number of repetitions for <auto-repeat> is not important at parsing
  // level because it will be computed later, let's set it to 1.
  wtf_size_t repetitions = 1;

  if (is_auto_repeat) {
    repeated_values = MakeGarbageCollected<cssvalue::CSSGridAutoRepeatValue>(
        args.ConsumeIncludingWhitespace().Id());
  } else {
    // TODO(rob.buis): a consumeIntegerRaw would be more efficient here.
    CSSPrimitiveValue* repetition = ConsumePositiveInteger(args, context);
    if (!repetition) {
      return false;
    }
    repetitions =
        ClampTo<wtf_size_t>(repetition->GetDoubleValue(), 0, kGridMaxTracks);
    repeated_values = CSSValueList::CreateSpaceSeparated();
  }

  if (!ConsumeCommaIncludingWhitespace(args)) {
    return false;
  }

  wtf_size_t number_of_line_name_sets =
      AppendLineNames(args, context, is_subgrid_track_list, repeated_values);
  wtf_size_t number_of_tracks = 0;
  while (!args.AtEnd()) {
    if (is_subgrid_track_list) {
      if (!number_of_line_name_sets ||
          !AppendLineNames(args, context, is_subgrid_track_list,
                           repeated_values)) {
        return false;
      }
      ++number_of_line_name_sets;
    } else {
      CSSValue* track_size = ConsumeGridTrackSize(args, context);
      if (!track_size) {
        return false;
      }
      if (all_tracks_are_fixed_sized) {
        all_tracks_are_fixed_sized = IsGridTrackFixedSized(*track_size);
      }
      repeated_values->Append(*track_size);
      ++number_of_tracks;
      AppendLineNames(args, context, is_subgrid_track_list, repeated_values);
    }
  }

  // We should have found at least one <track-size> or else it is not a valid
  // <track-list>. If it's a subgrid <line-name-list>, then we should have found
  // at least one named grid line.
  if ((is_subgrid_track_list && !number_of_line_name_sets) ||
      (!is_subgrid_track_list && !number_of_tracks)) {
    return false;
  }

  if (is_auto_repeat) {
    list.Append(*repeated_values);
  } else {
    // We clamp the repetitions to a multiple of the repeat() track list's size,
    // while staying below the max grid size.
    repetitions =
        std::min(repetitions, kGridMaxTracks / (is_subgrid_track_list
                                                    ? number_of_line_name_sets
                                                    : number_of_tracks));
    auto* integer_repeated_values =
        MakeGarbageCollected<cssvalue::CSSGridIntegerRepeatValue>(repetitions);
    for (wtf_size_t i = 0; i < repeated_values->length(); ++i) {
      integer_repeated_values->Append(repeated_values->Item(i));
    }
    list.Append(*integer_repeated_values);
  }

  return true;
}

bool ConsumeGridTemplateRowsAndAreasAndColumns(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSValue*& template_rows,
    const CSSValue*& template_columns,
    const CSSValue*& template_areas) {
  DCHECK(!template_rows);
  DCHECK(!template_columns);
  DCHECK(!template_areas);

  NamedGridAreaMap grid_area_map;
  wtf_size_t row_count = 0;
  wtf_size_t column_count = 0;
  CSSValueList* template_rows_value_list = CSSValueList::CreateSpaceSeparated();

  // Persists between loop iterations so we can use the same value for
  // consecutive <line-names> values
  CSSBracketedValueList* line_names = nullptr;

  do {
    // Handle leading <custom-ident>*.
    bool has_previous_line_names = line_names;
    line_names = ConsumeGridLineNames(
        range, context, /* is_subgrid_track_list */ false, line_names);
    if (line_names && !has_previous_line_names) {
      template_rows_value_list->Append(*line_names);
    }

    // Handle a template-area's row.
    if (range.Peek().GetType() != kStringToken ||
        !ParseGridTemplateAreasRow(
            range.ConsumeIncludingWhitespace().Value().ToString(),
            grid_area_map, row_count, column_count)) {
      return false;
    }
    ++row_count;

    // Handle template-rows's track-size.
    CSSValue* value = ConsumeGridTrackSize(range, context);
    if (!value) {
      value = CSSIdentifierValue::Create(CSSValueID::kAuto);
    }
    template_rows_value_list->Append(*value);

    // This will handle the trailing/leading <custom-ident>* in the grammar.
    line_names =
        ConsumeGridLineNames(range, context, /* is_subgrid_track_list */ false);
    if (line_names) {
      template_rows_value_list->Append(*line_names);
    }
  } while (!range.AtEnd() && !(range.Peek().GetType() == kDelimiterToken &&
                               range.Peek().Delimiter() == '/'));

  if (!range.AtEnd()) {
    if (!ConsumeSlashIncludingWhitespace(range)) {
      return false;
    }
    template_columns = ConsumeGridTrackList(
        range, context, TrackListType::kGridTemplateNoRepeat);
    if (!template_columns || !range.AtEnd()) {
      return false;
    }
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
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return ConsumeIdent(range);
  }

  CSSIdentifierValue* span_value = nullptr;
  CSSCustomIdentValue* grid_line_name = nullptr;
  CSSPrimitiveValue* numeric_value = ConsumeInteger(range, context);
  if (numeric_value) {
    grid_line_name = ConsumeCustomIdentForGridLine(range, context);
    span_value = ConsumeIdent<CSSValueID::kSpan>(range);
  } else {
    span_value = ConsumeIdent<CSSValueID::kSpan>(range);
    if (span_value) {
      numeric_value = ConsumeInteger(range, context);
      grid_line_name = ConsumeCustomIdentForGridLine(range, context);
      if (!numeric_value) {
        numeric_value = ConsumeInteger(range, context);
      }
    } else {
      grid_line_name = ConsumeCustomIdentForGridLine(range, context);
      if (grid_line_name) {
        numeric_value = ConsumeInteger(range, context);
        span_value = ConsumeIdent<CSSValueID::kSpan>(range);
        if (!span_value && !numeric_value) {
          return grid_line_name;
        }
      } else {
        return nullptr;
      }
    }
  }

  if (span_value && !numeric_value && !grid_line_name) {
    return nullptr;  // "span" keyword alone is invalid.
  }
  if (span_value && numeric_value && numeric_value->GetIntValue() < 0) {
    return nullptr;  // Negative numbers are not allowed for span.
  }
  if (numeric_value && numeric_value->GetIntValue() == 0) {
    return nullptr;  // An <integer> value of zero makes the declaration
                     // invalid.
  }

  if (numeric_value) {
    numeric_value = CSSNumericLiteralValue::Create(
        ClampTo(numeric_value->GetIntValue(), -kGridMaxTracks, kGridMaxTracks),
        CSSPrimitiveValue::UnitType::kInteger);
  }

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  if (span_value) {
    values->Append(*span_value);
  }
  if (numeric_value) {
    values->Append(*numeric_value);
  }
  if (grid_line_name) {
    values->Append(*grid_line_name);
  }
  DCHECK(values->length());
  return values;
}

CSSValue* ConsumeGridTrackList(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               TrackListType track_list_type) {
  bool allow_grid_line_names = track_list_type != TrackListType::kGridAuto;
  if (!allow_grid_line_names && range.Peek().GetType() == kLeftBracketToken) {
    return nullptr;
  }

  bool is_subgrid_track_list =
      RuntimeEnabledFeatures::LayoutNGSubgridEnabled() &&
      track_list_type == TrackListType::kGridTemplateSubgrid;

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  if (is_subgrid_track_list) {
    if (IdentMatches<CSSValueID::kSubgrid>(range.Peek().Id())) {
      values->Append(*ConsumeIdent(range));
    } else {
      return nullptr;
    }
  }

  AppendLineNames(range, context, is_subgrid_track_list, values);

  bool allow_repeat =
      is_subgrid_track_list || track_list_type == TrackListType::kGridTemplate;
  bool seen_auto_repeat = false;
  bool all_tracks_are_fixed_sized = true;
  auto IsRangeAtEnd = [](CSSParserTokenRange& range) -> bool {
    return range.AtEnd() || range.Peek().GetType() == kDelimiterToken;
  };

  do {
    bool is_auto_repeat;
    if (range.Peek().FunctionId() == CSSValueID::kRepeat) {
      if (!allow_repeat) {
        return nullptr;
      }
      if (!ConsumeGridTrackRepeatFunction(range, context, is_subgrid_track_list,
                                          *values, is_auto_repeat,
                                          all_tracks_are_fixed_sized)) {
        return nullptr;
      }
      if (is_auto_repeat && seen_auto_repeat) {
        return nullptr;
      }

      seen_auto_repeat = seen_auto_repeat || is_auto_repeat;
    } else if (CSSValue* value = ConsumeGridTrackSize(range, context)) {
      // If we find a <track-size> in a subgrid track list, then it isn't a
      // valid <line-name-list>.
      if (is_subgrid_track_list) {
        return nullptr;
      }
      if (all_tracks_are_fixed_sized) {
        all_tracks_are_fixed_sized = IsGridTrackFixedSized(*value);
      }

      values->Append(*value);
    } else if (!is_subgrid_track_list) {
      return nullptr;
    }

    if (seen_auto_repeat && !all_tracks_are_fixed_sized) {
      return nullptr;
    }
    if (!allow_grid_line_names && range.Peek().GetType() == kLeftBracketToken) {
      return nullptr;
    }

    bool did_append_line_names =
        AppendLineNames(range, context, is_subgrid_track_list, values);
    if (is_subgrid_track_list && !did_append_line_names &&
        range.Peek().FunctionId() != CSSValueID::kRepeat) {
      return IsRangeAtEnd(range) ? values : nullptr;
    }
  } while (!IsRangeAtEnd(range));

  return values;
}

bool ParseGridTemplateAreasRow(const String& grid_row_names,
                               NamedGridAreaMap& grid_area_map,
                               const wtf_size_t row_count,
                               wtf_size_t& column_count) {
  if (grid_row_names.ContainsOnlyWhitespaceOrEmpty()) {
    return false;
  }

  Vector<String> column_names =
      ParseGridTemplateAreasColumnNames(grid_row_names);
  if (row_count == 0) {
    column_count = column_names.size();
    if (column_count == 0) {
      return false;
    }
  } else if (column_count != column_names.size()) {
    // The declaration is invalid if all the rows don't have the number of
    // columns.
    return false;
  }

  for (wtf_size_t current_column = 0; current_column < column_count;
       ++current_column) {
    const String& grid_area_name = column_names[current_column];

    // Unamed areas are always valid (we consider them to be 1x1).
    if (grid_area_name == ".") {
      continue;
    }

    wtf_size_t look_ahead_column = current_column + 1;
    while (look_ahead_column < column_count &&
           column_names[look_ahead_column] == grid_area_name) {
      look_ahead_column++;
    }

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
      if (row_count != grid_area.rows.EndLine()) {
        return false;
      }

      // 2. The new area starts at the same position as the previously parsed
      // area.
      if (current_column != grid_area.columns.StartLine()) {
        return false;
      }

      // 3. The new area ends at the same position as the previously parsed
      // area.
      if (look_ahead_column != grid_area.columns.EndLine()) {
        return false;
      }

      grid_area.rows = GridSpan::TranslatedDefiniteGridSpan(
          grid_area.rows.StartLine(), grid_area.rows.EndLine() + 1);
    }
    current_column = look_ahead_column - 1;
  }

  return true;
}

CSSValue* ConsumeGridTemplatesRowsOrColumns(CSSParserTokenRange& range,
                                            const CSSParserContext& context) {
  switch (range.Peek().Id()) {
    case CSSValueID::kNone:
      return ConsumeIdent(range);
    case CSSValueID::kSubgrid:
      return ConsumeGridTrackList(range, context,
                                  TrackListType::kGridTemplateSubgrid);
    default:
      return ConsumeGridTrackList(range, context, TrackListType::kGridTemplate);
  }
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
  if (!start_value) {
    return false;
  }

  if (ConsumeSlashIncludingWhitespace(range)) {
    end_value = ConsumeGridLine(range, context);
    if (!end_value) {
      return false;
    }
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
                                  const CSSValue*& template_rows,
                                  const CSSValue*& template_columns,
                                  const CSSValue*& template_areas) {
  DCHECK(!template_rows);
  DCHECK(!template_columns);
  DCHECK(!template_areas);

  DCHECK_EQ(gridTemplateShorthand().length(), 3u);

  CSSParserTokenRange range_copy = range;
  template_rows = ConsumeIdent<CSSValueID::kNone>(range);

  // 1- 'none' case.
  if (template_rows && range.AtEnd()) {
    template_rows = CSSIdentifierValue::Create(CSSValueID::kNone);
    template_columns = CSSIdentifierValue::Create(CSSValueID::kNone);
    template_areas = CSSIdentifierValue::Create(CSSValueID::kNone);
    return true;
  }

  // 2- <grid-template-rows> / <grid-template-columns>
  if (!template_rows) {
    template_rows = ConsumeGridTemplatesRowsOrColumns(range, context);
  }

  if (template_rows) {
    if (!ConsumeSlashIncludingWhitespace(range)) {
      return false;
    }
    template_columns = ConsumeGridTemplatesRowsOrColumns(range, context);
    if (!template_columns || !range.AtEnd()) {
      return false;
    }

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

CSSValue* ConsumeHyphenateLimitChars(CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  CSSValueList* const list = CSSValueList::CreateSpaceSeparated();
  while (!range.AtEnd() && list->length() < 3) {
    if (const CSSPrimitiveValue* value = ConsumeIntegerOrNumberCalc(
            range, context, CSSPrimitiveValue::ValueRange::kPositiveInteger)) {
      list->Append(*value);
      continue;
    }
    if (const CSSIdentifierValue* ident =
            ConsumeIdent<CSSValueID::kAuto>(range)) {
      list->Append(*ident);
      continue;
    }
    return nullptr;
  }
  if (list->length()) {
    return list;
  }
  return nullptr;
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

std::unique_ptr<SVGPathByteStream> ConsumePathStringArg(
    CSSParserTokenRange& args) {
  if (args.Peek().GetType() != kStringToken) {
    return nullptr;
  }

  StringView path_string = args.ConsumeIncludingWhitespace().Value();
  std::unique_ptr<SVGPathByteStream> byte_stream =
      std::make_unique<SVGPathByteStream>();
  if (BuildByteStreamFromString(path_string, *byte_stream) !=
      SVGParseStatus::kNoError) {
    return nullptr;
  }

  return byte_stream;
}

cssvalue::CSSPathValue* ConsumeBasicShapePath(CSSParserTokenRange& args) {
  auto wind_rule = RULE_NONZERO;

  if (IdentMatches<CSSValueID::kEvenodd, CSSValueID::kNonzero>(
          args.Peek().Id())) {
    wind_rule = args.ConsumeIncludingWhitespace().Id() == CSSValueID::kEvenodd
                    ? RULE_EVENODD
                    : RULE_NONZERO;
    if (!ConsumeCommaIncludingWhitespace(args)) {
      return nullptr;
    }
  }

  auto byte_stream = ConsumePathStringArg(args);
  // https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-path
  // A path data string that does not conform to the to the grammar
  // and parsing rules of SVG 1.1, or that does conform but defines
  // an empty path, is invalid and causes the entire path() to be invalid.
  if (!byte_stream || !args.AtEnd() ||
      (RuntimeEnabledFeatures::ClipPathRejectEmptyPathsEnabled() &&
       byte_stream->IsEmpty())) {
    return nullptr;
  }

  return MakeGarbageCollected<cssvalue::CSSPathValue>(std::move(byte_stream),
                                                      wind_rule);
}

CSSValue* ConsumePathFunction(CSSParserTokenRange& range,
                              EmptyPathStringHandling empty_handling) {
  // FIXME: Add support for <url>, <basic-shape>, <geometry-box>.
  if (range.Peek().FunctionId() != CSSValueID::kPath) {
    return nullptr;
  }

  CSSParserTokenRange function_range = range;
  CSSParserTokenRange function_args = ConsumeFunction(function_range);

  auto byte_stream = ConsumePathStringArg(function_args);
  if (!byte_stream || !function_args.AtEnd()) {
    return nullptr;
  }

  // https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-path
  // A path data string that does not conform to the to the grammar
  // and parsing rules of SVG 1.1, or that does conform but defines
  // an empty path, is invalid and causes the entire path() to be invalid.
  if (byte_stream->IsEmpty()) {
    if (empty_handling == EmptyPathStringHandling::kTreatAsNone) {
      range = function_range;
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    }
    return nullptr;
  }
  range = function_range;

  return MakeGarbageCollected<cssvalue::CSSPathValue>(std::move(byte_stream));
}

CSSValue* ConsumeRay(CSSParserTokenRange& range,
                     const CSSParserContext& context) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueID::kRay);
  CSSParserTokenRange function_range = range;
  CSSParserTokenRange function_args = ConsumeFunction(function_range);

  CSSPrimitiveValue* angle = nullptr;
  CSSIdentifierValue* size = nullptr;
  CSSIdentifierValue* contain = nullptr;
  bool position = false;
  CSSValue* x = nullptr;
  CSSValue* y = nullptr;
  while (!function_args.AtEnd()) {
    if (!angle) {
      angle =
          ConsumeAngle(function_args, context, absl::optional<WebFeature>());
      if (angle) {
        continue;
      }
    }
    if (!size) {
      size =
          ConsumeIdent<CSSValueID::kClosestSide, CSSValueID::kClosestCorner,
                       CSSValueID::kFarthestSide, CSSValueID::kFarthestCorner,
                       CSSValueID::kSides>(function_args);
      if (size) {
        continue;
      }
    }
    if (RuntimeEnabledFeatures::CSSOffsetPathRayContainEnabled() && !contain) {
      contain = ConsumeIdent<CSSValueID::kContain>(function_args);
      if (contain) {
        continue;
      }
    }
    if (!position && ConsumeIdent<CSSValueID::kAt>(function_args)) {
      position = ConsumePosition(function_args, context, UnitlessQuirk::kForbid,
                                 absl::optional<WebFeature>(), x, y);
      if (position) {
        continue;
      }
    }
    return nullptr;
  }
  if (!angle) {
    return nullptr;
  }
  if (!size) {
    size = CSSIdentifierValue::Create(CSSValueID::kClosestSide);
  }
  range = function_range;
  return MakeGarbageCollected<cssvalue::CSSRayValue>(*angle, *size, contain, x,
                                                     y);
}

CSSValue* ConsumeMaxWidthOrHeight(CSSParserTokenRange& range,
                                  const CSSParserContext& context,
                                  UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueID::kNone ||
      ValidWidthOrHeightKeyword(range.Peek().Id(), context)) {
    return ConsumeIdent(range);
  }
  return ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative, unitless,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize));
}

CSSValue* ConsumeWidthOrHeight(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueID::kAuto ||
      ValidWidthOrHeightKeyword(range.Peek().Id(), context)) {
    return ConsumeIdent(range);
  }
  return ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative, unitless,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize));
}

CSSValue* ConsumeMarginOrOffset(CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                UnitlessQuirk unitless,
                                CSSAnchorQueryTypes allowed_anchor_queries) {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return ConsumeIdent(range);
  }
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kAll, unitless,
                                allowed_anchor_queries);
}

CSSValue* ConsumeScrollPadding(CSSParserTokenRange& range,
                               const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return ConsumeIdent(range);
  }
  CSSParserContext::ParserModeOverridingScope scope(context, kHTMLStandardMode);
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                UnitlessQuirk::kForbid);
}

namespace {

// https://drafts.csswg.org/css-shapes-1/#supported-basic-shapes
bool IsBasicShapeSupportedByOffsetPath(const CSSValueID& id) {
  switch (id) {
    case CSSValueID::kCircle:
    case CSSValueID::kEllipse:
      return RuntimeEnabledFeatures::
          CSSOffsetPathBasicShapesCircleAndEllipseEnabled();
    case CSSValueID::kInset:
    case CSSValueID::kXywh:
    case CSSValueID::kRect:
    case CSSValueID::kPolygon:
      return RuntimeEnabledFeatures::
          CSSOffsetPathBasicShapesRectanglesAndPolygonEnabled();
    default:
      return false;
  }
}

}  // namespace

CSSValue* ConsumeScrollStart(CSSParserTokenRange& range,
                             const CSSParserContext& context) {
  if (CSSIdentifierValue* ident =
          ConsumeIdent<CSSValueID::kAuto, CSSValueID::kStart,
                       CSSValueID::kCenter, CSSValueID::kEnd, CSSValueID::kTop,
                       CSSValueID::kBottom, CSSValueID::kLeft,
                       CSSValueID::kRight>(range)) {
    return ident;
  }
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative);
}

CSSValue* ConsumeScrollStartTarget(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueID::kAuto, CSSValueID::kNone>(range);
}

CSSValue* ConsumeOffsetPath(CSSParserTokenRange& range,
                            const CSSParserContext& context) {
  CSSValue* offset_path = nullptr;
  CSSValue* coord_box = nullptr;

  if (CSSValue* none = ConsumeIdent<CSSValueID::kNone>(range)) {
    return none;
  }
  if (RuntimeEnabledFeatures::CSSOffsetPathCoordBoxEnabled()) {
    coord_box = ConsumeCoordBox(range);
  }

  const CSSValueID function_id = range.Peek().FunctionId();
  if (RuntimeEnabledFeatures::CSSOffsetPathRayEnabled() &&
      function_id == CSSValueID::kRay) {
    offset_path = ConsumeRay(range, context);
  }
  if (!offset_path && IsBasicShapeSupportedByOffsetPath(function_id)) {
    offset_path = ConsumeBasicShape(range, context, AllowPathValue::kAllow,
                                    AllowBasicShapeRectValue::kAllow,
                                    AllowBasicShapeXYWHValue::kAllow);
  }
  if (!offset_path && RuntimeEnabledFeatures::CSSOffsetPathUrlEnabled()) {
    offset_path = ConsumeUrl(range, context);
  }
  if (!offset_path) {
    offset_path = ConsumePathFunction(range, EmptyPathStringHandling::kFailure);
  }

  if (!coord_box && RuntimeEnabledFeatures::CSSOffsetPathCoordBoxEnabled()) {
    coord_box = ConsumeCoordBox(range);
  }

  if (!offset_path && !coord_box) {
    return nullptr;
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (offset_path) {
    list->Append(*offset_path);
  }
  if (!offset_path ||
      (coord_box && To<CSSIdentifierValue>(coord_box)->GetValueID() !=
                        CSSValueID::kBorderBox)) {
    list->Append(*coord_box);
  }

  // Count when we receive a valid path other than 'none'.
  context.Count(WebFeature::kCSSOffsetInEffect);

  return list;
}

CSSValue* ConsumePathOrNone(CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone) {
    return ConsumeIdent(range);
  }

  return ConsumePathFunction(range, EmptyPathStringHandling::kTreatAsNone);
}

CSSValue* ConsumeOffsetRotate(CSSParserTokenRange& range,
                              const CSSParserContext& context) {
  CSSValue* angle = ConsumeAngle(range, context, absl::optional<WebFeature>());
  CSSValue* keyword =
      ConsumeIdent<CSSValueID::kAuto, CSSValueID::kReverse>(range);
  if (!angle && !keyword) {
    return nullptr;
  }

  if (!angle) {
    angle = ConsumeAngle(range, context, absl::optional<WebFeature>());
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (keyword) {
    list->Append(*keyword);
  }
  if (angle) {
    list->Append(*angle);
  }
  return list;
}

CSSValue* ConsumeInitialLetter(CSSParserTokenRange& range,
                               const CSSParserContext& context) {
  if (ConsumeIdent<CSSValueID::kNormal>(range)) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  CSSValueList* const list = CSSValueList::CreateSpaceSeparated();
  // ["drop" | "raise"] number[1,Inf]
  if (auto* sink_type =
          ConsumeIdent<CSSValueID::kDrop, CSSValueID::kRaise>(range)) {
    if (auto* size = ConsumeNumber(
            range, context, CSSPrimitiveValue::ValueRange::kNonNegative)) {
      if (size->GetFloatValue() < 1) {
        return nullptr;
      }
      list->Append(*size);
      list->Append(*sink_type);
      return list;
    }
    return nullptr;
  }

  // number[1, Inf]
  // number[1, Inf] ["drop" | "raise"]
  // number[1, Inf] integer[1, Inf]
  if (auto* size = ConsumeNumber(range, context,
                                 CSSPrimitiveValue::ValueRange::kNonNegative)) {
    if (size->GetFloatValue() < 1) {
      return nullptr;
    }
    list->Append(*size);
    if (range.AtEnd()) {
      return list;
    }
    if (auto* sink_type =
            ConsumeIdent<CSSValueID::kDrop, CSSValueID::kRaise>(range)) {
      list->Append(*sink_type);
      return list;
    }
    if (auto* sink = ConsumeIntegerOrNumberCalc(
            range, context, CSSPrimitiveValue::ValueRange::kPositiveInteger)) {
      list->Append(*sink);
      return list;
    }
  }

  return nullptr;
}

bool ConsumeRadii(CSSValue* horizontal_radii[4],
                  CSSValue* vertical_radii[4],
                  CSSParserTokenRange& range,
                  const CSSParserContext& context,
                  bool use_legacy_parsing) {
  unsigned horizontal_value_count = 0;
  for (; horizontal_value_count < 4 && !range.AtEnd() &&
         range.Peek().GetType() != kDelimiterToken;
       ++horizontal_value_count) {
    horizontal_radii[horizontal_value_count] = ConsumeLengthOrPercent(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!horizontal_radii[horizontal_value_count]) {
      return false;
    }
  }
  if (!horizontal_radii[0]) {
    return false;
  }
  if (range.AtEnd()) {
    // Legacy syntax: -webkit-border-radius: l1 l2; is equivalent to
    // border-radius: l1 / l2;
    if (use_legacy_parsing && horizontal_value_count == 2) {
      vertical_radii[0] = horizontal_radii[1];
      horizontal_radii[1] = nullptr;
    } else {
      Complete4Sides(horizontal_radii);
      for (unsigned i = 0; i < 4; ++i) {
        vertical_radii[i] = horizontal_radii[i];
      }
      return true;
    }
  } else {
    if (!ConsumeSlashIncludingWhitespace(range)) {
      return false;
    }
    for (unsigned i = 0; i < 4 && !range.AtEnd(); ++i) {
      vertical_radii[i] = ConsumeLengthOrPercent(
          range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      if (!vertical_radii[i]) {
        return false;
      }
    }
    if (!vertical_radii[0] || !range.AtEnd()) {
      return false;
    }
  }
  Complete4Sides(horizontal_radii);
  Complete4Sides(vertical_radii);
  return true;
}

CSSValue* ConsumeBasicShape(CSSParserTokenRange& range,
                            const CSSParserContext& context,
                            AllowPathValue allow_path,
                            AllowBasicShapeRectValue allow_rect,
                            AllowBasicShapeXYWHValue allow_xywh) {
  CSSValue* shape = nullptr;
  if (range.Peek().GetType() != kFunctionToken) {
    return nullptr;
  }
  CSSValueID id = range.Peek().FunctionId();
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  if (id == CSSValueID::kCircle) {
    shape = ConsumeBasicShapeCircle(args, context);
  } else if (id == CSSValueID::kEllipse) {
    shape = ConsumeBasicShapeEllipse(args, context);
  } else if (id == CSSValueID::kPolygon) {
    shape = ConsumeBasicShapePolygon(args, context);
  } else if (id == CSSValueID::kInset) {
    shape = ConsumeBasicShapeInset(args, context);
  } else if (id == CSSValueID::kPath && allow_path == AllowPathValue::kAllow) {
    shape = ConsumeBasicShapePath(args);
  } else if (id == CSSValueID::kRect &&
             allow_rect == AllowBasicShapeRectValue::kAllow) {
    shape = ConsumeBasicShapeRect(args, context);
  } else if (id == CSSValueID::kXywh &&
             allow_xywh == AllowBasicShapeXYWHValue::kAllow) {
    shape = ConsumeBasicShapeXYWH(args, context);
  }
  if (!shape || !args.AtEnd()) {
    return nullptr;
  }

  context.Count(WebFeature::kCSSBasicShape);
  range = range_copy;
  return shape;
}

// none | [ underline || overline || line-through || blink ] | spelling-error |
// grammar-error
CSSValue* ConsumeTextDecorationLine(CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone) {
    return ConsumeIdent(range);
  }

  if (RuntimeEnabledFeatures::CSSSpellingGrammarErrorsEnabled() &&
      (id == CSSValueID::kSpellingError || id == CSSValueID::kGrammarError)) {
    // Note that StyleBuilderConverter::ConvertFlags() requires that values
    // other than 'none' appear in a CSSValueList.
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    list->Append(*ConsumeIdent(range));
    return list;
  }

  CSSIdentifierValue* underline = nullptr;
  CSSIdentifierValue* overline = nullptr;
  CSSIdentifierValue* line_through = nullptr;
  CSSIdentifierValue* blink = nullptr;

  while (true) {
    id = range.Peek().Id();
    if (id == CSSValueID::kUnderline && !underline) {
      underline = ConsumeIdent(range);
    } else if (id == CSSValueID::kOverline && !overline) {
      overline = ConsumeIdent(range);
    } else if (id == CSSValueID::kLineThrough && !line_through) {
      line_through = ConsumeIdent(range);
    } else if (id == CSSValueID::kBlink && !blink) {
      blink = ConsumeIdent(range);
    } else {
      break;
    }
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (underline) {
    list->Append(*underline);
  }
  if (overline) {
    list->Append(*overline);
  }
  if (line_through) {
    list->Append(*line_through);
  }
  if (blink) {
    list->Append(*blink);
  }

  if (!list->length()) {
    return nullptr;
  }
  return list;
}

// Consume the `autospace` production.
// https://drafts.csswg.org/css-text-4/#typedef-autospace
CSSValue* ConsumeAutospace(CSSParserTokenRange& range) {
  // Currently, only `no-autospace` is supported.
  return ConsumeIdent<CSSValueID::kNoAutospace>(range);
}

// Consume the `spacing-trim` production.
// https://drafts.csswg.org/css-text-4/#typedef-spacing-trim
CSSValue* ConsumeSpacingTrim(CSSParserTokenRange& range) {
  // Currently, only `space-first` and `space-all` are supported.
  return ConsumeIdent<CSSValueID::kSpaceFirst, CSSValueID::kSpaceAll>(range);
}

CSSValue* ConsumeTransformValue(CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                bool use_legacy_parsing) {
  CSSValueID function_id = range.Peek().FunctionId();
  if (!IsValidCSSValueID(function_id)) {
    return nullptr;
  }
  CSSParserTokenRange args = ConsumeFunction(range);
  if (args.AtEnd()) {
    return nullptr;
  }
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
      parsed_value =
          ConsumeAngle(args, context, WebFeature::kUnitlessZeroAngleTransform);
      if (!parsed_value) {
        return nullptr;
      }
      if (function_id == CSSValueID::kSkew &&
          ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value = ConsumeAngle(args, context,
                                    WebFeature::kUnitlessZeroAngleTransform);
        if (!parsed_value) {
          return nullptr;
        }
      }
      break;
    case CSSValueID::kScaleX:
    case CSSValueID::kScaleY:
    case CSSValueID::kScaleZ:
    case CSSValueID::kScale:
      parsed_value = ConsumeNumberOrPercent(
          args, context, CSSPrimitiveValue::ValueRange::kAll);
      if (!parsed_value) {
        return nullptr;
      }
      if (function_id == CSSValueID::kScale &&
          ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value = ConsumeNumberOrPercent(
            args, context, CSSPrimitiveValue::ValueRange::kAll);
        if (!parsed_value) {
          return nullptr;
        }
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
      parsed_value = ConsumeLengthOrPercent(
          args, context, CSSPrimitiveValue::ValueRange::kAll);
      if (!parsed_value) {
        return nullptr;
      }
      if (function_id == CSSValueID::kTranslate &&
          ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value = ConsumeLengthOrPercent(
            args, context, CSSPrimitiveValue::ValueRange::kAll);
        if (!parsed_value) {
          return nullptr;
        }
      }
      break;
    case CSSValueID::kTranslateZ:
      parsed_value =
          ConsumeLength(args, context, CSSPrimitiveValue::ValueRange::kAll);
      break;
    case CSSValueID::kMatrix:
    case CSSValueID::kMatrix3d:
      if (!ConsumeNumbers(args, context, transform_value,
                          (function_id == CSSValueID::kMatrix3d) ? 16 : 6)) {
        return nullptr;
      }
      break;
    case CSSValueID::kScale3d:
      if (!ConsumeNumbersOrPercents(args, context, transform_value, 3)) {
        return nullptr;
      }
      break;
    case CSSValueID::kRotate3d:
      if (!ConsumeNumbers(args, context, transform_value, 3) ||
          !ConsumeCommaIncludingWhitespace(args)) {
        return nullptr;
      }
      parsed_value =
          ConsumeAngle(args, context, WebFeature::kUnitlessZeroAngleTransform);
      if (!parsed_value) {
        return nullptr;
      }
      break;
    case CSSValueID::kTranslate3d:
      if (!ConsumeTranslate3d(args, context, transform_value)) {
        return nullptr;
      }
      break;
    default:
      return nullptr;
  }
  if (parsed_value) {
    transform_value->Append(*parsed_value);
  }
  if (!args.AtEnd()) {
    return nullptr;
  }
  return transform_value;
}

CSSValue* ConsumeTransformList(CSSParserTokenRange& range,
                               const CSSParserContext& context,
                               const CSSParserLocalContext& local_context) {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(range);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSValue* parsed_transform_value =
        ConsumeTransformValue(range, context, local_context.UseAliasParsing());
    if (!parsed_transform_value) {
      return nullptr;
    }
    list->Append(*parsed_transform_value);
  } while (!range.AtEnd());

  return list;
}

CSSValue* ConsumeTransitionProperty(CSSParserTokenRange& range,
                                    const CSSParserContext& context) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() != kIdentToken) {
    return nullptr;
  }
  if (token.Id() == CSSValueID::kNone) {
    return ConsumeIdent(range);
  }
  const auto* execution_context = context.GetExecutionContext();
  CSSPropertyID unresolved_property =
      token.ParseAsUnresolvedCSSPropertyID(execution_context);
  if (unresolved_property != CSSPropertyID::kInvalid &&
      unresolved_property != CSSPropertyID::kVariable) {
#if DCHECK_IS_ON()
    DCHECK(CSSProperty::Get(ResolveCSSPropertyID(unresolved_property))
               .IsWebExposed(execution_context));
#endif
    range.ConsumeIncludingWhitespace();
    return MakeGarbageCollected<CSSCustomIdentValue>(unresolved_property);
  }
  return ConsumeCustomIdent(range, context);
}

bool IsValidPropertyList(const CSSValueList& value_list) {
  if (value_list.length() < 2) {
    return true;
  }
  for (auto& value : value_list) {
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value.Get());
    if (identifier_value &&
        identifier_value->GetValueID() == CSSValueID::kNone) {
      return false;
    }
  }
  return true;
}

bool IsValidTransitionBehavior(const CSSValueID& value) {
  switch (value) {
    case CSSValueID::kNormal:
    case CSSValueID::kAllowDiscrete:
      return true;
    default:
      return false;
  }
}

bool IsValidTransitionBehaviorList(const CSSValueList& value_list) {
  for (auto& value : value_list) {
    auto* ident_value = DynamicTo<CSSIdentifierValue>(value.Get());
    if (!ident_value) {
      return false;
    }
    if (!IsValidTransitionBehavior(ident_value->GetValueID())) {
      return false;
    }
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
  return ConsumeColor(range, context, allow_quirky_colors);
}

CSSValue* ConsumeBorderWidth(CSSParserTokenRange& range,
                             const CSSParserContext& context,
                             UnitlessQuirk unitless) {
  return ConsumeLineWidth(range, context, unitless);
}

CSSValue* ParseSpacing(CSSParserTokenRange& range,
                       const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNormal) {
    return ConsumeIdent(range);
  }
  // TODO(timloh): allow <percentage>s in word-spacing.
  return ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll,
                       UnitlessQuirk::kAllow);
}

CSSValue* ConsumeSingleContainerName(CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  if (range.Peek().GetType() != kIdentToken) {
    return nullptr;
  }
  if (range.Peek().Id() == CSSValueID::kNone) {
    return nullptr;
  }
  if (EqualIgnoringASCIICase(range.Peek().Value(), "not")) {
    return nullptr;
  }
  if (EqualIgnoringASCIICase(range.Peek().Value(), "and")) {
    return nullptr;
  }
  if (EqualIgnoringASCIICase(range.Peek().Value(), "or")) {
    return nullptr;
  }
  return ConsumeCustomIdent(range, context);
}

CSSValue* ConsumeContainerName(CSSParserTokenRange& range,
                               const CSSParserContext& context) {
  if (CSSValue* value = ConsumeIdent<CSSValueID::kNone>(range)) {
    return value;
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  while (CSSValue* value = ConsumeSingleContainerName(range, context)) {
    list->Append(*value);
  }

  return list->length() ? list : nullptr;
}

CSSValue* ConsumeContainerType(CSSParserTokenRange& range) {
  // container-type: normal | [ [ size | inline-size ] || sticky || snap ]
  if (CSSValue* value = ConsumeIdent<CSSValueID::kNormal>(range)) {
    return value;
  }

  CSSValue* size_value = nullptr;
  CSSValue* sticky_value = nullptr;
  CSSValue* snap_value = nullptr;

  do {
    if (!size_value) {
      size_value =
          ConsumeIdent<CSSValueID::kSize, CSSValueID::kInlineSize>(range);
      if (size_value) {
        continue;
      }
    }
    if (!sticky_value &&
        RuntimeEnabledFeatures::CSSStickyContainerQueriesEnabled()) {
      sticky_value = ConsumeIdent<CSSValueID::kSticky>(range);
      if (sticky_value) {
        continue;
      }
    }
    if (!snap_value &&
        RuntimeEnabledFeatures::CSSSnapContainerQueriesEnabled()) {
      snap_value = ConsumeIdent<CSSValueID::kSnap>(range);
      if (snap_value) {
        continue;
      }
    }
    return nullptr;
  } while (!range.AtEnd());

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (size_value) {
    list->Append(*size_value);
  }
  if (sticky_value) {
    list->Append(*sticky_value);
  }
  if (snap_value) {
    list->Append(*snap_value);
  }
  return list;
}

CSSValue* ConsumeSVGPaint(CSSParserTokenRange& range,
                          const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(range);
  }
  cssvalue::CSSURIValue* url = ConsumeUrl(range, context);
  if (url) {
    CSSValue* parsed_value = nullptr;
    if (range.Peek().Id() == CSSValueID::kNone) {
      parsed_value = ConsumeIdent(range);
    } else {
      parsed_value = ConsumeColor(range, context);
    }
    if (parsed_value) {
      CSSValueList* values = CSSValueList::CreateSpaceSeparated();
      values->Append(*url);
      values->Append(*parsed_value);
      return values;
    }
    return url;
  }
  return ConsumeColor(range, context);
}

UnitlessQuirk UnitlessUnlessShorthand(
    const CSSParserLocalContext& local_context) {
  return local_context.CurrentShorthand() == CSSPropertyID::kInvalid
             ? UnitlessQuirk::kAllow
             : UnitlessQuirk::kForbid;
}

bool ShouldLowerCaseCounterStyleNameOnParse(const AtomicString& name,
                                            const CSSParserContext& context) {
  if (context.Mode() == kUASheetMode) {
    // Names in UA sheet should be already in lower case.
    DCHECK_EQ(name, name.LowerASCII());
    return false;
  }
  return CounterStyleMap::GetUACounterStyleMap()->FindCounterStyleAcrossScopes(
      name.LowerASCII());
}

CSSCustomIdentValue* ConsumeCounterStyleName(CSSParserTokenRange& range,
                                             const CSSParserContext& context) {
  CSSParserTokenRange original_range = range;

  // <counter-style-name> is a <custom-ident> that is not an ASCII
  // case-insensitive match for "none".
  const CSSParserToken& name_token = range.ConsumeIncludingWhitespace();
  if (name_token.GetType() != kIdentToken ||
      !css_parsing_utils::IsCustomIdent<CSSValueID::kNone>(name_token.Id())) {
    range = original_range;
    return nullptr;
  }

  AtomicString name(name_token.Value().ToString());
  if (ShouldLowerCaseCounterStyleNameOnParse(name, context)) {
    name = name.LowerASCII();
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(name);
}

AtomicString ConsumeCounterStyleNameInPrelude(CSSParserTokenRange& prelude,
                                              const CSSParserContext& context) {
  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd()) {
    return g_null_atom;
  }

  if (name_token.GetType() != kIdentToken ||
      !IsCustomIdent<CSSValueID::kNone>(name_token.Id())) {
    return g_null_atom;
  }

  if (context.Mode() != kUASheetMode) {
    if (name_token.Id() == CSSValueID::kDecimal ||
        name_token.Id() == CSSValueID::kDisc ||
        name_token.Id() == CSSValueID::kCircle ||
        name_token.Id() == CSSValueID::kSquare ||
        name_token.Id() == CSSValueID::kDisclosureOpen ||
        name_token.Id() == CSSValueID::kDisclosureClosed) {
      return g_null_atom;
    }
  }

  AtomicString name(name_token.Value().ToString());
  if (ShouldLowerCaseCounterStyleNameOnParse(name, context)) {
    name = name.LowerASCII();
  }
  return name;
}

CSSValue* ConsumeFontSizeAdjust(CSSParserTokenRange& range,
                                const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  CSSIdentifierValue* font_metric =
      ConsumeIdent<CSSValueID::kExHeight, CSSValueID::kCapHeight,
                   CSSValueID::kChWidth, CSSValueID::kIcWidth>(range);

  CSSValue* value = css_parsing_utils::ConsumeNumber(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!value) {
    value = ConsumeIdent<CSSValueID::kFromFont>(range);
  }

  if (!value || !font_metric ||
      font_metric->GetValueID() == CSSValueID::kExHeight) {
    return value;
  }

  return MakeGarbageCollected<CSSValuePair>(font_metric, value,
                                            CSSValuePair::kKeepIdenticalValues);
}

}  // namespace css_parsing_utils
}  // namespace blink
