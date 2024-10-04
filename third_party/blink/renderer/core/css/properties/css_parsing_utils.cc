// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

#include <cmath>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/css/counter_style_map.h"
#include "third_party/blink/renderer/core/css/css_appearance_auto_base_select_value_pair.h"
#include "third_party/blink/renderer/core/css/css_attr_value_tainting.h"
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
#include "third_party/blink/renderer/core/css/css_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_revert_layer_value.h"
#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_scoped_keyword_value.h"
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
#include "third_party/blink/renderer/core/css/parser/css_parser_save_point.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
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
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_parsing_error.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_builder.h"
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

CSSIdentifierValue* ConsumeOverflowPositionKeyword(
    CSSParserTokenStream& stream) {
  return IsOverflowKeyword(stream.Peek().Id()) ? ConsumeIdent(stream) : nullptr;
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

CSSValue* ConsumeFirstBaseline(CSSParserTokenStream& stream) {
  ConsumeIdent<CSSValueID::kFirst>(stream);
  return ConsumeIdent<CSSValueID::kBaseline>(stream);
}

CSSValue* ConsumeBaseline(CSSParserTokenStream& stream) {
  CSSIdentifierValue* preference =
      ConsumeIdent<CSSValueID::kFirst, CSSValueID::kLast>(stream);
  CSSIdentifierValue* baseline = ConsumeIdent<CSSValueID::kBaseline>(stream);
  if (!baseline) {
    return nullptr;
  }
  if (preference && preference->GetValueID() == CSSValueID::kLast) {
    return MakeGarbageCollected<CSSValuePair>(
        preference, baseline, CSSValuePair::kDropIdenticalValues);
  }
  return baseline;
}

std::optional<cssvalue::CSSLinearStop> ConsumeLinearStop(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  std::optional<double> number;
  std::optional<double> length_a;
  std::optional<double> length_b;
  while (!stream.AtEnd()) {
    if (stream.Peek().GetType() == kCommaToken) {
      break;
    }
    CSSPrimitiveValue* value =
        ConsumeNumber(stream, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!number.has_value() && value && value->IsNumber()) {
      number = value->GetDoubleValue();
      continue;
    }
    value =
        ConsumePercent(stream, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!length_a.has_value() && value && value->IsPercentage()) {
      length_a = value->GetDoubleValue();
      value =
          ConsumePercent(stream, context, CSSPrimitiveValue::ValueRange::kAll);
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

CSSValue* ConsumeLinear(CSSParserTokenStream& stream,
                        const CSSParserContext& context) {
  CSSValue* result;

  // https://w3c.github.io/csswg-drafts/css-easing/#linear-easing-function-parsing
  DCHECK_EQ(stream.Peek().FunctionId(), CSSValueID::kLinear);
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    Vector<cssvalue::CSSLinearStop> stop_list{};
    std::optional<cssvalue::CSSLinearStop> linear_stop;
    do {
      linear_stop = ConsumeLinearStop(stream, context);
      if (!linear_stop.has_value()) {
        return nullptr;
      }
      stop_list.emplace_back(linear_stop.value());
    } while (ConsumeCommaIncludingWhitespace(stream));
    if (!stream.AtEnd()) {
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
        // 4.3.3. If stop’s <linear-stop-length> has a second <percentage>,
        // then:
        if (stop.length_b.has_value()) {
          // 4.3.3.1. Let extraPoint be a new linear easing point with its
          // output set to stop’s <number> as a number.
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
    // 5. For runs of items in function’s points that have a null input, assign
    // a number to the input by linearly interpolating between the closest
    // previous and next points that have a non-null input.
    wtf_size_t upper_index = 0;
    for (wtf_size_t i = 1; i < points.size(); ++i) {
      if (std::isnan(points[i].input)) {
        if (i > upper_index) {
          const auto it = std::find_if(
              std::next(points.begin(), i + 1), points.end(),
              [](const auto& point) { return !std::isnan(point.input); });
          upper_index = static_cast<wtf_size_t>(it - points.begin());
        }
        points[i].input = points[i - 1].input +
                          (points[upper_index].input - points[i - 1].input) /
                              (upper_index - (i - 1));
      }
    }
    guard.Release();
    result = MakeGarbageCollected<cssvalue::CSSLinearTimingFunctionValue>(
        std::move(points));
  }
  stream.ConsumeWhitespace();

  // 6. Return function.
  return result;
}

CSSValue* ConsumeSteps(CSSParserTokenStream& stream,
                       const CSSParserContext& context) {
  CSSValue* result;

  DCHECK_EQ(stream.Peek().FunctionId(), CSSValueID::kSteps);
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    CSSPrimitiveValue* steps = ConsumePositiveInteger(stream, context);
    if (!steps) {
      return nullptr;
    }

    StepsTimingFunction::StepPosition position =
        StepsTimingFunction::StepPosition::END;
    if (ConsumeCommaIncludingWhitespace(stream)) {
      switch (stream.Peek().Id()) {
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
      stream.ConsumeIncludingWhitespace();  // kIdentToken
    }

    if (!stream.AtEnd()) {
      return nullptr;
    }

    // Steps(n, jump-none) requires n >= 2.
    if (position == StepsTimingFunction::StepPosition::JUMP_NONE &&
        steps->GetIntValue() < 2) {
      return nullptr;
    }

    guard.Release();
    result = MakeGarbageCollected<cssvalue::CSSStepsTimingFunctionValue>(
        steps->GetIntValue(), position);
  }
  stream.ConsumeWhitespace();
  return result;
}

CSSValue* ConsumeCubicBezier(CSSParserTokenStream& stream,
                             const CSSParserContext& context) {
  DCHECK_EQ(stream.Peek().FunctionId(), CSSValueID::kCubicBezier);
  CSSValue* result = nullptr;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    double x1, y1, x2, y2;
    if (ConsumeNumberRaw(stream, context, x1) && x1 >= 0 && x1 <= 1 &&
        ConsumeCommaIncludingWhitespace(stream) &&
        ConsumeNumberRaw(stream, context, y1) &&
        ConsumeCommaIncludingWhitespace(stream) &&
        ConsumeNumberRaw(stream, context, x2) && x2 >= 0 && x2 <= 1 &&
        ConsumeCommaIncludingWhitespace(stream) &&
        ConsumeNumberRaw(stream, context, y2) && stream.AtEnd()) {
      guard.Release();
      result =
          MakeGarbageCollected<cssvalue::CSSCubicBezierTimingFunctionValue>(
              x1, y1, x2, y2);
    }
  }
  if (result) {
    stream.ConsumeWhitespace();
  }

  return result;
}

CSSIdentifierValue* ConsumeBorderImageRepeatKeyword(
    CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kStretch, CSSValueID::kRepeat,
                      CSSValueID::kSpace, CSSValueID::kRound>(stream);
}

bool ConsumeCSSValueId(CSSParserTokenStream& stream, CSSValueID& value) {
  CSSIdentifierValue* keyword = ConsumeIdent(stream);
  if (!keyword) {
    return false;
  }
  value = keyword->GetValueID();
  return true;
}

CSSValue* ConsumeShapeRadius(CSSParserTokenStream& args,
                             const CSSParserContext& context) {
  if (IdentMatches<CSSValueID::kClosestSide, CSSValueID::kFarthestSide>(
          args.Peek().Id())) {
    return ConsumeIdent(args);
  }
  return ConsumeLengthOrPercent(args, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative);
}

cssvalue::CSSBasicShapeCircleValue* ConsumeBasicShapeCircle(
    CSSParserTokenStream& args,
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
                         std::optional<WebFeature>(), center_x, center_y)) {
      return nullptr;
    }
    shape->SetCenterX(center_x);
    shape->SetCenterY(center_y);
  }
  return shape;
}

cssvalue::CSSBasicShapeEllipseValue* ConsumeBasicShapeEllipse(
    CSSParserTokenStream& args,
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
                         std::optional<WebFeature>(), center_x, center_y)) {
      return nullptr;
    }
    shape->SetCenterX(center_x);
    shape->SetCenterY(center_y);
  }
  context.Count(feature);
  return shape;
}

cssvalue::CSSBasicShapePolygonValue* ConsumeBasicShapePolygon(
    CSSParserTokenStream& args,
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

template <class U>
bool ConsumeBorderRadiusCommon(CSSParserTokenStream& args,
                               const CSSParserContext& context,
                               U* shape) {
  if (ConsumeIdent<CSSValueID::kRound>(args)) {
    std::array<CSSValue*, 4> horizontal_radii = {nullptr};
    std::array<CSSValue*, 4> vertical_radii = {nullptr};
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
    CSSParserTokenStream& args,
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
    CSSParserTokenStream& args,
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
    CSSParserTokenStream& args,
    const CSSParserContext& context) {
  std::array<CSSPrimitiveValue*, 4> lengths;
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

bool ConsumeNumbers(CSSParserTokenStream& stream,
                    const CSSParserContext& context,
                    CSSFunctionValue*& transform_value,
                    unsigned number_of_arguments) {
  do {
    CSSValue* parsed_value =
        ConsumeNumber(stream, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!parsed_value) {
      return false;
    }
    transform_value->Append(*parsed_value);
    if (--number_of_arguments && !ConsumeCommaIncludingWhitespace(stream)) {
      return false;
    }
  } while (number_of_arguments);
  return true;
}

bool ConsumeNumbersOrPercents(CSSParserTokenStream& stream,
                              const CSSParserContext& context,
                              CSSFunctionValue*& transform_value,
                              unsigned number_of_arguments) {
  do {
    CSSValue* parsed_value = ConsumeNumberOrPercent(
        stream, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!parsed_value) {
      return false;
    }
    transform_value->Append(*parsed_value);
    if (--number_of_arguments && !ConsumeCommaIncludingWhitespace(stream)) {
      return false;
    }
  } while (number_of_arguments);
  return true;
}

bool ConsumePerspective(CSSParserTokenStream& stream,
                        const CSSParserContext& context,
                        CSSFunctionValue*& transform_value,
                        bool use_legacy_parsing) {
  CSSValue* parsed_value = ConsumeLength(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!parsed_value) {
    parsed_value = ConsumeIdent<CSSValueID::kNone>(stream);
  }
  if (!parsed_value && use_legacy_parsing) {
    double perspective;
    if (!ConsumeNumberRaw(stream, context, perspective) || perspective < 0) {
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

bool ConsumeTranslate3d(CSSParserTokenStream& stream,
                        const CSSParserContext& context,
                        CSSFunctionValue*& transform_value) {
  unsigned number_of_arguments = 2;
  CSSValue* parsed_value = nullptr;
  do {
    parsed_value = ConsumeLengthOrPercent(stream, context,
                                          CSSPrimitiveValue::ValueRange::kAll);
    if (!parsed_value) {
      return false;
    }
    transform_value->Append(*parsed_value);
    if (!ConsumeCommaIncludingWhitespace(stream)) {
      return false;
    }
  } while (--number_of_arguments);
  parsed_value =
      ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!parsed_value) {
    return false;
  }
  transform_value->Append(*parsed_value);
  return true;
}

CSSFunctionValue* ConsumeFilterFunction(CSSParserTokenStream& stream,
                                        const CSSParserContext& context) {
  CSSValueID filter_type = stream.Peek().FunctionId();
  if (filter_type < CSSValueID::kInvert ||
      filter_type > CSSValueID::kDropShadow) {
    return nullptr;
  }

  CSSFunctionValue* filter_value;
  CSSValue* parsed_value = nullptr;
  bool no_arguments = false;
  {
    CSSParserTokenStream::BlockGuard guard(stream);
    stream.ConsumeWhitespace();
    filter_value = MakeGarbageCollected<CSSFunctionValue>(filter_type);

    if (filter_type == CSSValueID::kDropShadow) {
      parsed_value =
          ParseSingleShadow(stream, context, AllowInsetAndSpread::kForbid);
    } else {
      if (stream.AtEnd()) {
        context.Count(WebFeature::kCSSFilterFunctionNoArguments);
        no_arguments = true;
      } else if (filter_type == CSSValueID::kBrightness) {
        // FIXME (crbug.com/397061): Support calc expressions like
        // calc(10% + 0.5)
        parsed_value = ConsumePercent(stream, context,
                                      CSSPrimitiveValue::ValueRange::kAll);
        if (!parsed_value) {
          parsed_value = ConsumeNumber(
              stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
        }
      } else if (filter_type == CSSValueID::kHueRotate) {
        parsed_value =
            ConsumeAngle(stream, context, WebFeature::kUnitlessZeroAngleFilter);
      } else if (filter_type == CSSValueID::kBlur) {
        CSSParserContext::ParserModeOverridingScope scope(context,
                                                          kHTMLStandardMode);
        parsed_value = ConsumeLength(
            stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      } else {
        // FIXME (crbug.com/397061): Support calc expressions like
        // calc(10% + 0.5)
        parsed_value = ConsumePercent(
            stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
        if (!parsed_value) {
          parsed_value = ConsumeNumber(
              stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
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
    if (!no_arguments && (!parsed_value || !stream.AtEnd())) {
      return nullptr;
    }
  }
  stream.ConsumeWhitespace();
  if (parsed_value) {
    filter_value->Append(*parsed_value);
  }
  return filter_value;
}

template <typename Func>
CSSLightDarkValuePair* ConsumeLightDark(Func consume_value,
                                        CSSParserTokenStream& stream,
                                        const CSSParserContext& context) {
  if (stream.Peek().FunctionId() != CSSValueID::kLightDark) {
    return nullptr;
  }
  if (!IsUASheetBehavior(context.Mode())) {
    context.Count(WebFeature::kCSSLightDark);
  }

  CSSValue* light_value;
  CSSValue* dark_value;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    light_value = consume_value(stream, context);
    if (!light_value || !ConsumeCommaIncludingWhitespace(stream)) {
      return nullptr;
    }
    dark_value = consume_value(stream, context);
    if (!dark_value || !stream.AtEnd()) {
      return nullptr;
    }
    guard.Release();
  }
  stream.ConsumeWhitespace();
  return MakeGarbageCollected<CSSLightDarkValuePair>(light_value, dark_value);
}

CSSAppearanceAutoBaseSelectValuePair* ConsumeAppearanceAutoBaseSelectColor(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  if (stream.Peek().FunctionId() !=
      CSSValueID::kInternalAppearanceAutoBaseSelect) {
    return nullptr;
  }

  CSSValue* auto_value;
  CSSValue* base_select_value;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    auto_value = ConsumeColor(stream, context);
    if (!auto_value || !ConsumeCommaIncludingWhitespace(stream)) {
      return nullptr;
    }
    base_select_value = ConsumeColor(stream, context);
    if (!base_select_value || !stream.AtEnd()) {
      return nullptr;
    }
    guard.Release();
  }
  stream.ConsumeWhitespace();
  return MakeGarbageCollected<CSSAppearanceAutoBaseSelectValuePair>(
      auto_value, base_select_value);
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
    case CSSValueID::kCrossFade:
      return true;

    default:
      return false;
  }
}

bool IsImageSet(const CSSValueID id) {
  return id == CSSValueID::kWebkitImageSet || id == CSSValueID::kImageSet;
}

}  // namespace

void Complete4Sides(std::array<CSSValue*, 4>& side) {
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

bool ConsumeCommaIncludingWhitespace(CSSParserTokenStream& stream) {
  CSSParserToken value = stream.Peek();
  if (value.GetType() != kCommaToken) {
    return false;
  }
  stream.ConsumeIncludingWhitespace();
  return true;
}

bool ConsumeSlashIncludingWhitespace(CSSParserTokenStream& stream) {
  CSSParserToken value = stream.Peek();
  if (value.GetType() != kDelimiterToken || value.Delimiter() != '/') {
    return false;
  }
  stream.ConsumeIncludingWhitespace();
  return true;
}

namespace {

bool ConsumeAnyComponentValue(CSSParserTokenStream& stream) {
  if (stream.Peek().GetBlockType() == CSSParserToken::kBlockStart) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    ConsumeAnyValue(stream);
    if (guard.Release()) {
      return true;
    }
  } else if (IsTokenAllowedForAnyValue(stream.Peek())) {
    stream.Consume();
    return true;
  }
  return false;
}

}  // namespace

void ConsumeAnyValue(CSSParserTokenStream& stream) {
  while (!stream.AtEnd()) {
    if (!ConsumeAnyComponentValue(stream)) {
      return;
    }
  }
}

// MathFunctionParser is a helper for parsing something that _might_ be a
// function. In particular, it helps rewinding the parser to the point where it
// started if what was to be parsed was not a function (or an invalid function).
// This rewinding happens in the destructor, unless Consume*() was called _and_
// returned success. In effect, this gives us a multi-token peek for functions.
//
// TODO(rwlbuis): consider pulling in the parsing logic from
// css_math_expression_node.cc.
class MathFunctionParser {
  STACK_ALLOCATED();

 public:
  using Flag = CSSMathExpressionNode::Flag;
  using Flags = CSSMathExpressionNode::Flags;

  MathFunctionParser(
      CSSParserTokenStream& stream,
      const CSSParserContext& context,
      CSSPrimitiveValue::ValueRange value_range,
      const Flags parsing_flags = Flags({Flag::AllowPercent}),
      CSSAnchorQueryTypes allowed_anchor_queries = kCSSAnchorQueryTypesNone,
      const CSSColorChannelMap& color_channel_map = {})
      : stream_(&stream), savepoint_(stream.Save()) {
    const CSSParserToken token = stream.Peek();
    if (token.GetType() == kFunctionToken) {
      {
        CSSParserTokenStream::BlockGuard guard(*stream_);
        stream_->ConsumeWhitespace();
        calc_value_ = CSSMathFunctionValue::Create(
            CSSMathExpressionNode::ParseMathFunction(
                token.FunctionId(), *stream_, context, parsing_flags,
                allowed_anchor_queries),
            value_range);
      }
      stream_->ConsumeWhitespace();
    }
  }

  ~MathFunctionParser() {
    if (!has_consumed_) {
      // Rewind the parser.
      stream_->Restore(savepoint_);
    }
  }

  const CSSMathFunctionValue* Value() const { return calc_value_; }
  CSSMathFunctionValue* ConsumeValue() {
    if (!calc_value_) {
      return nullptr;
    }
    DCHECK(!has_consumed_);  // Cannot consume twice.
    has_consumed_ = true;
    CSSMathFunctionValue* result = calc_value_;
    calc_value_ = nullptr;
    return result;
  }

  bool ConsumeNumberRaw(double& result) {
    if (!calc_value_ || calc_value_->Category() != kCalcNumber) {
      return false;
    }
    DCHECK(!has_consumed_);  // Cannot consume twice.
    has_consumed_ = true;
    result = calc_value_->GetDoubleValue();
    return true;
  }

 private:
  bool has_consumed_ = false;
  CSSParserTokenStream* stream_;
  // For rewinding.
  CSSParserTokenStream::State savepoint_;
  CSSMathFunctionValue* calc_value_ = nullptr;
};

CSSPrimitiveValue* ConsumeIntegerInternal(CSSParserTokenStream& stream,
                                          const CSSParserContext& context,
                                          double minimum_value,
                                          const bool is_percentage_allowed) {
  const CSSParserToken token = stream.Peek();
  if (token.GetType() == kNumberToken) {
    if (token.GetNumericValueType() == kNumberValueType ||
        token.NumericValue() < minimum_value) {
      return nullptr;
    }
    return CSSNumericLiteralValue::Create(
        stream.ConsumeIncludingWhitespace().NumericValue(),
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

  using enum CSSMathExpressionNode::Flag;
  using Flags = CSSMathExpressionNode::Flags;

  Flags parsing_flags;
  if (is_percentage_allowed) {
    parsing_flags.Put(AllowPercent);
  }

  MathFunctionParser math_parser(stream, context, value_range, parsing_flags);
  if (const CSSMathFunctionValue* math_value = math_parser.Value()) {
    if (math_value->Category() != kCalcNumber) {
      return nullptr;
    }
    return math_parser.ConsumeValue();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeInteger(CSSParserTokenStream& stream,
                                  const CSSParserContext& context,
                                  double minimum_value,
                                  const bool is_percentage_allowed) {
  return ConsumeIntegerInternal(stream, context, minimum_value,
                                is_percentage_allowed);
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_range) {
  double minimum_value = -std::numeric_limits<double>::max();
  switch (value_range) {
    case CSSPrimitiveValue::ValueRange::kAll:
      NOTREACHED_IN_MIGRATION() << "unexpected value range for integer parsing";
      [[fallthrough]];
    case CSSPrimitiveValue::ValueRange::kInteger:
      minimum_value = -std::numeric_limits<double>::max();
      break;
    case CSSPrimitiveValue::ValueRange::kNonNegative:
      NOTREACHED_IN_MIGRATION() << "unexpected value range for integer parsing";
      [[fallthrough]];
    case CSSPrimitiveValue::ValueRange::kNonNegativeInteger:
      minimum_value = 0.0;
      break;
    case CSSPrimitiveValue::ValueRange::kPositiveInteger:
      minimum_value = 1.0;
      break;
  }
  if (CSSPrimitiveValue* value =
          ConsumeInteger(stream, context, minimum_value)) {
    return value;
  }

  MathFunctionParser math_parser(stream, context, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() != kCalcNumber) {
      return nullptr;
    }
    return math_parser.ConsumeValue();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumePositiveInteger(CSSParserTokenStream& stream,
                                          const CSSParserContext& context) {
  return ConsumeInteger(stream, context, 1);
}

bool ConsumeNumberRaw(CSSParserTokenStream& stream,
                      const CSSParserContext& context,
                      double& result) {
  if (stream.Peek().GetType() == kNumberToken) {
    result = stream.ConsumeIncludingWhitespace().NumericValue();
    return true;
  }
  MathFunctionParser math_parser(stream, context,
                                 CSSPrimitiveValue::ValueRange::kAll);
  return math_parser.ConsumeNumberRaw(result);
}

CSSPrimitiveValue* ConsumeNumber(CSSParserTokenStream& stream,
                                 const CSSParserContext& context,
                                 CSSPrimitiveValue::ValueRange value_range) {
  const CSSParserToken token = stream.Peek();
  if (token.GetType() == kNumberToken) {
    if (value_range == CSSPrimitiveValue::ValueRange::kNonNegative &&
        token.NumericValue() < 0) {
      return nullptr;
    }
    return CSSNumericLiteralValue::Create(
        stream.ConsumeIncludingWhitespace().NumericValue(),
        token.GetUnitType());
  }
  MathFunctionParser math_parser(stream, context, value_range);
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

CSSPrimitiveValue* ConsumeLength(CSSParserTokenStream& stream,
                                 const CSSParserContext& context,
                                 CSSPrimitiveValue::ValueRange value_range,
                                 UnitlessQuirk unitless) {
  const CSSParserToken token = stream.Peek();
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
      case CSSPrimitiveValue::UnitType::kCaps:
      case CSSPrimitiveValue::UnitType::kRcaps:
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
        stream.ConsumeIncludingWhitespace().NumericValue(),
        token.GetUnitType());
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
        stream.ConsumeIncludingWhitespace().NumericValue(), unit_type);
  }
  if (context.Mode() == kSVGAttributeMode) {
    return nullptr;
  }
  MathFunctionParser math_parser(stream, context, value_range);
  if (math_parser.Value() && math_parser.Value()->Category() == kCalcLength) {
    return math_parser.ConsumeValue();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumePercent(CSSParserTokenStream& stream,
                                  const CSSParserContext& context,
                                  CSSPrimitiveValue::ValueRange value_range) {
  const CSSParserToken token = stream.Peek();
  if (token.GetType() == kPercentageToken) {
    if (value_range == CSSPrimitiveValue::ValueRange::kNonNegative &&
        token.NumericValue() < 0) {
      return nullptr;
    }
    return CSSNumericLiteralValue::Create(
        stream.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kPercentage);
  }
  MathFunctionParser math_parser(stream, context, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() == kCalcPercent) {
      return math_parser.ConsumeValue();
    }
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeNumberOrPercent(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_stream) {
  if (CSSPrimitiveValue* value = ConsumeNumber(stream, context, value_stream)) {
    return value;
  }
  if (CSSPrimitiveValue* value =
          ConsumePercent(stream, context, value_stream)) {
    return CSSNumericLiteralValue::Create(value->GetDoubleValue() / 100.0,
                                          CSSPrimitiveValue::UnitType::kNumber);
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeAlphaValue(CSSParserTokenStream& stream,
                                     const CSSParserContext& context) {
  return ConsumeNumberOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kAll);
}

bool CanConsumeCalcValue(CalculationResultCategory category,
                         CSSParserMode css_parser_mode) {
  return category == kCalcLength || category == kCalcPercent ||
         category == kCalcLengthFunction || category == kCalcIntrinsicSize ||
         (css_parser_mode == kSVGAttributeMode && category == kCalcNumber);
}

CSSPrimitiveValue* ConsumeLengthOrPercent(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_range,
    UnitlessQuirk unitless,
    CSSAnchorQueryTypes allowed_anchor_queries,
    AllowCalcSize allow_calc_size) {
  using enum CSSMathExpressionNode::Flag;
  using Flags = CSSMathExpressionNode::Flags;

  const CSSParserToken& token = stream.Peek();
  if (token.GetType() == kDimensionToken || token.GetType() == kNumberToken) {
    return ConsumeLength(stream, context, value_range, unitless);
  }
  if (token.GetType() == kPercentageToken) {
    return ConsumePercent(stream, context, value_range);
  }
  Flags parsing_flags({AllowPercent});
  switch (allow_calc_size) {
    case AllowCalcSize::kAllowWithAutoAndContent:
      parsing_flags.Put(AllowContentInCalcSize);
      [[fallthrough]];
    case AllowCalcSize::kAllowWithAuto:
      parsing_flags.Put(AllowAutoInCalcSize);
      [[fallthrough]];
    case AllowCalcSize::kAllowWithoutAuto:
      parsing_flags.Put(AllowCalcSize);
      [[fallthrough]];
    case AllowCalcSize::kForbid:
      break;
  }
  MathFunctionParser math_parser(stream, context, value_range, parsing_flags,
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_range) {
  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  CSSPrimitiveValue* value = ConsumeLengthOrPercent(
      stream, context, value_range, UnitlessQuirk::kForbid);
  if (IsNonZeroUserUnitsValue(value)) {
    context.Count(WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue);
  }
  return value;
}

CSSPrimitiveValue* ConsumeGradientLengthOrPercent(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_range,
    UnitlessQuirk unitless) {
  return ConsumeLengthOrPercent(stream, context, value_range, unitless);
}

static CSSPrimitiveValue* ConsumeNumericLiteralAngle(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    std::optional<WebFeature> unitless_zero_feature) {
  const CSSParserToken token = stream.Peek();
  if (token.GetType() == kDimensionToken) {
    switch (token.GetUnitType()) {
      case CSSPrimitiveValue::UnitType::kDegrees:
      case CSSPrimitiveValue::UnitType::kRadians:
      case CSSPrimitiveValue::UnitType::kGradians:
      case CSSPrimitiveValue::UnitType::kTurns:
        return CSSNumericLiteralValue::Create(
            stream.ConsumeIncludingWhitespace().NumericValue(),
            token.GetUnitType());
      default:
        return nullptr;
    }
  }
  if (token.GetType() == kNumberToken && token.NumericValue() == 0 &&
      unitless_zero_feature) {
    stream.ConsumeIncludingWhitespace();
    context.Count(*unitless_zero_feature);
    return CSSNumericLiteralValue::Create(
        0, CSSPrimitiveValue::UnitType::kDegrees);
  }
  return nullptr;
}

template <class T>
  requires std::is_same_v<T, CSSParserTokenStream>
static CSSPrimitiveValue* ConsumeMathFunctionAngle(
    T& stream,
    const CSSParserContext& context,
    double minimum_value,
    double maximum_value) {
  MathFunctionParser math_parser(stream, context,
                                 CSSPrimitiveValue::ValueRange::kAll);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() != kCalcAngle) {
      return nullptr;
    }
  }
  if (CSSMathFunctionValue* result = math_parser.ConsumeValue()) {
    auto* numeric_result =
        DynamicTo<CSSMathExpressionNumericLiteral>(result->ExpressionNode());
    if (numeric_result && numeric_result->DoubleValue() < minimum_value) {
      return CSSNumericLiteralValue::Create(
          minimum_value, CSSPrimitiveValue::UnitType::kDegrees);
    }
    if (numeric_result && numeric_result->DoubleValue() > maximum_value) {
      return CSSNumericLiteralValue::Create(
          maximum_value, CSSPrimitiveValue::UnitType::kDegrees);
    }
    return result;
  }
  return nullptr;
}

static CSSPrimitiveValue* ConsumeMathFunctionAngle(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  MathFunctionParser math_parser(stream, context,
                                 CSSPrimitiveValue::ValueRange::kAll);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() != kCalcAngle) {
      return nullptr;
    }
  }
  return math_parser.ConsumeValue();
}

CSSPrimitiveValue* ConsumeAngle(CSSParserTokenStream& stream,
                                const CSSParserContext& context,
                                std::optional<WebFeature> unitless_zero_feature,
                                double minimum_value,
                                double maximum_value) {
  if (auto* result =
          ConsumeNumericLiteralAngle(stream, context, unitless_zero_feature)) {
    return result;
  }

  return ConsumeMathFunctionAngle(stream, context, minimum_value,
                                  maximum_value);
}

CSSPrimitiveValue* ConsumeAngle(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    std::optional<WebFeature> unitless_zero_feature) {
  if (auto* result =
          ConsumeNumericLiteralAngle(stream, context, unitless_zero_feature)) {
    return result;
  }

  return ConsumeMathFunctionAngle(stream, context);
}

CSSPrimitiveValue* ConsumeTime(CSSParserTokenStream& stream,
                               const CSSParserContext& context,
                               CSSPrimitiveValue::ValueRange value_range) {
  const CSSParserToken token = stream.Peek();
  if (token.GetType() == kDimensionToken) {
    if (value_range == CSSPrimitiveValue::ValueRange::kNonNegative &&
        token.NumericValue() < 0) {
      return nullptr;
    }
    CSSPrimitiveValue::UnitType unit = token.GetUnitType();
    if (unit == CSSPrimitiveValue::UnitType::kMilliseconds ||
        unit == CSSPrimitiveValue::UnitType::kSeconds) {
      return CSSNumericLiteralValue::Create(
          stream.ConsumeIncludingWhitespace().NumericValue(),
          token.GetUnitType());
    }
    return nullptr;
  }
  MathFunctionParser math_parser(stream, context, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    if (calculation->Category() == kCalcTime) {
      return math_parser.ConsumeValue();
    }
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeResolution(CSSParserTokenStream& stream,
                                     const CSSParserContext& context) {
  if (const CSSParserToken& token = stream.Peek();
      token.GetType() == kDimensionToken) {
    CSSPrimitiveValue::UnitType unit = token.GetUnitType();
    if (!CSSPrimitiveValue::IsResolution(unit) || token.NumericValue() < 0.0) {
      // "The allowed range of <resolution> values always excludes negative
      // values"
      // https://www.w3.org/TR/css-values-4/#resolution-value

      return nullptr;
    }

    return CSSNumericLiteralValue::Create(
        stream.ConsumeIncludingWhitespace().NumericValue(), unit);
  }

  MathFunctionParser math_parser(stream, context,
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
CSSValue* ConsumeRatio(CSSParserTokenStream& stream,
                       const CSSParserContext& context) {
  CSSParserSavePoint savepoint(stream);

  CSSPrimitiveValue* first = ConsumeNumber(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!first) {
    return nullptr;
  }

  CSSPrimitiveValue* second = nullptr;

  if (css_parsing_utils::ConsumeSlashIncludingWhitespace(stream)) {
    second = ConsumeNumber(stream, context,
                           CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!second) {
      return nullptr;
    }
  } else {
    second = CSSNumericLiteralValue::Create(
        1, CSSPrimitiveValue::UnitType::kInteger);
  }

  savepoint.Release();
  return MakeGarbageCollected<cssvalue::CSSRatioValue>(*first, *second);
}

CSSIdentifierValue* ConsumeIdent(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kIdentToken) {
    return nullptr;
  }
  return CSSIdentifierValue::Create(stream.ConsumeIncludingWhitespace().Id());
}

CSSIdentifierValue* ConsumeIdentRange(CSSParserTokenStream& stream,
                                      CSSValueID lower,
                                      CSSValueID upper) {
  if (stream.Peek().Id() < lower || stream.Peek().Id() > upper) {
    return nullptr;
  }
  return ConsumeIdent(stream);
}

CSSCustomIdentValue* ConsumeCustomIdent(CSSParserTokenStream& stream,
                                        const CSSParserContext& context) {
  if (stream.Peek().GetType() != kIdentToken ||
      IsCSSWideKeyword(stream.Peek().Id()) ||
      stream.Peek().Id() == CSSValueID::kDefault) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(
      stream.ConsumeIncludingWhitespace().Value().ToAtomicString());
}

CSSCustomIdentValue* ConsumeDashedIdent(CSSParserTokenStream& stream,
                                        const CSSParserContext& context) {
  if (stream.Peek().GetType() != kIdentToken) {
    return nullptr;
  }
  if (!stream.Peek().Value().ToString().StartsWith(kTwoDashes)) {
    return nullptr;
  }

  return ConsumeCustomIdent(stream, context);
}

cssvalue::CSSScopedKeywordValue* ConsumeScopedKeywordValue(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kIdentToken) {
    return nullptr;
  }
  return MakeGarbageCollected<cssvalue::CSSScopedKeywordValue>(
      stream.ConsumeIncludingWhitespace().Id());
}

CSSStringValue* ConsumeString(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kStringToken) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSStringValue>(
      stream.ConsumeIncludingWhitespace().Value().ToString());
}

String ConsumeStringAsString(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != CSSParserTokenType::kStringToken) {
    return String();
  }

  return stream.ConsumeIncludingWhitespace().Value().ToString();
}

namespace {

// Invalidate the URL if only data URLs are allowed and the protocol is not
// data.
//
// NOTE: The StringView must be instantiated with an empty string; otherwise the
// URL will incorrectly be identified as null. The resource should behave as
// if it failed to load.
bool IsFetchRestricted(StringView url, const CSSParserContext& context) {
  return !url.IsNull() &&
         context.ResourceFetchRestriction() ==
             ResourceFetchRestriction::kOnlyDataUrls &&
         !ProtocolIs(url.ToString(), "data");
}

CSSUrlData CollectUrlData(const StringView& url,
                          const CSSParserContext& context) {
  AtomicString url_string = url.ToAtomicString();
  return CSSUrlData(
      url_string, context.CompleteNonEmptyURL(url_string),
      context.GetReferrer(),
      context.IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse,
      context.IsAdRelated());
}

}  // namespace

// Returns a token whose token.Value() will contain the URL,
// or the empty string if there are fetch restrictions,
// or an EOF token if we failed to parse.
//
// NOTE: We are careful not to return a reference, since the token
// will be overwritten once we move to the next one.
CSSParserToken ConsumeUrlAsToken(CSSParserTokenStream& stream,
                                 const CSSParserContext& context) {
  wtf_size_t value_start_offset = stream.LookAheadOffset();
  stream.EnsureLookAhead();

  CSSParserToken token = stream.Peek();
  if (token.GetType() == kUrlToken) {
    stream.ConsumeIncludingWhitespace();
  } else if (token.FunctionId() == CSSValueID::kUrl) {
    {
      CSSParserTokenStream::RestoringBlockGuard guard(stream);
      stream.ConsumeWhitespace();
      // If the block doesn't start with a quote, then the tokenizer
      // would return a kUrlToken or kBadUrlToken instead of a
      // kFunctionToken. Note also that this Peek() placates the
      // DCHECK that we Peek() before Consume().
      DCHECK(stream.Peek().GetType() == kStringToken ||
             stream.Peek().GetType() == kBadStringToken)
          << "Got unexpected token " << stream.Peek();
      token = stream.ConsumeIncludingWhitespace();
      if (token.GetType() == kBadStringToken || !stream.AtEnd()) {
        return CSSParserToken(kEOFToken);
      }
      guard.Release();
    }
    DCHECK_EQ(token.GetType(), kStringToken);
    stream.ConsumeWhitespace();
  } else {
    return CSSParserToken(kEOFToken);
  }
  wtf_size_t value_end_offset = stream.LookAheadOffset();
  if (IsAttrTainted(stream, value_start_offset, value_end_offset)) {
    return CSSParserToken(kEOFToken);
  }
  return IsFetchRestricted(token.Value(), context)
             ? CSSParserToken(kUrlToken, StringView(""))
             : token;
}

cssvalue::CSSURIValue* ConsumeUrl(CSSParserTokenStream& stream,
                                  const CSSParserContext& context) {
  CSSParserToken url = ConsumeUrlAsToken(stream, context);
  if (url.GetType() == kEOFToken) {
    return nullptr;
  }
  return MakeGarbageCollected<cssvalue::CSSURIValue>(
      CollectUrlData(url.Value(), context));
}

static bool ConsumeColorInterpolationSpace(
    CSSParserTokenStream& stream,
    Color::ColorSpace& color_space,
    Color::HueInterpolationMethod& hue_interpolation) {
  if (!ConsumeIdent<CSSValueID::kIn>(stream)) {
    return false;
  }

  std::optional<Color::ColorSpace> read_color_space;
  if (ConsumeIdent<CSSValueID::kXyz>(stream)) {
    read_color_space = Color::ColorSpace::kXYZD65;
  } else if (ConsumeIdent<CSSValueID::kXyzD50>(stream)) {
    read_color_space = Color::ColorSpace::kXYZD50;
  } else if (ConsumeIdent<CSSValueID::kXyzD65>(stream)) {
    read_color_space = Color::ColorSpace::kXYZD65;
  } else if (ConsumeIdent<CSSValueID::kSRGBLinear>(stream)) {
    read_color_space = Color::ColorSpace::kSRGBLinear;
  } else if (ConsumeIdent<CSSValueID::kDisplayP3>(stream)) {
    read_color_space = Color::ColorSpace::kDisplayP3;
  } else if (ConsumeIdent<CSSValueID::kA98Rgb>(stream)) {
    read_color_space = Color::ColorSpace::kA98RGB;
  } else if (ConsumeIdent<CSSValueID::kProphotoRgb>(stream)) {
    read_color_space = Color::ColorSpace::kProPhotoRGB;
  } else if (ConsumeIdent<CSSValueID::kRec2020>(stream)) {
    read_color_space = Color::ColorSpace::kRec2020;
  } else if (ConsumeIdent<CSSValueID::kLab>(stream)) {
    read_color_space = Color::ColorSpace::kLab;
  } else if (ConsumeIdent<CSSValueID::kOklab>(stream)) {
    read_color_space = Color::ColorSpace::kOklab;
  } else if (ConsumeIdent<CSSValueID::kLch>(stream)) {
    read_color_space = Color::ColorSpace::kLch;
  } else if (ConsumeIdent<CSSValueID::kOklch>(stream)) {
    read_color_space = Color::ColorSpace::kOklch;
  } else if (ConsumeIdent<CSSValueID::kSRGB>(stream)) {
    read_color_space = Color::ColorSpace::kSRGB;
  } else if (ConsumeIdent<CSSValueID::kHsl>(stream)) {
    read_color_space = Color::ColorSpace::kHSL;
  } else if (ConsumeIdent<CSSValueID::kHwb>(stream)) {
    read_color_space = Color::ColorSpace::kHWB;
  }

  if (read_color_space) {
    color_space = read_color_space.value();
    std::optional<Color::HueInterpolationMethod> read_hue;
    if (color_space == Color::ColorSpace::kHSL ||
        color_space == Color::ColorSpace::kHWB ||
        color_space == Color::ColorSpace::kLch ||
        color_space == Color::ColorSpace::kOklch) {
      if (ConsumeIdent<CSSValueID::kShorter>(stream)) {
        read_hue = Color::HueInterpolationMethod::kShorter;
      } else if (ConsumeIdent<CSSValueID::kLonger>(stream)) {
        read_hue = Color::HueInterpolationMethod::kLonger;
      } else if (ConsumeIdent<CSSValueID::kDecreasing>(stream)) {
        read_hue = Color::HueInterpolationMethod::kDecreasing;
      } else if (ConsumeIdent<CSSValueID::kIncreasing>(stream)) {
        read_hue = Color::HueInterpolationMethod::kIncreasing;
      }
      if (read_hue) {
        if (!ConsumeIdent<CSSValueID::kHue>(stream)) {
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

namespace {

CSSValue* ConsumeColorInternal(CSSParserTokenStream&,
                               const CSSParserContext&,
                               bool accept_quirky_colors,
                               AllowedColors);

}  // namespace

// https://www.w3.org/TR/css-color-5/#color-mix
static CSSValue* ConsumeColorMixFunction(CSSParserTokenStream& stream,
                                         const CSSParserContext& context,
                                         AllowedColors allowed_colors) {
  DCHECK(stream.Peek().FunctionId() == CSSValueID::kColorMix);
  context.Count(WebFeature::kCSSColorMixFunction);

  cssvalue::CSSColorMixValue* result;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    // First argument is the colorspace
    Color::ColorSpace color_space;
    Color::HueInterpolationMethod hue_interpolation_method =
        Color::HueInterpolationMethod::kShorter;
    if (!ConsumeColorInterpolationSpace(stream, color_space,
                                        hue_interpolation_method)) {
      return nullptr;
    }

    if (!ConsumeCommaIncludingWhitespace(stream)) {
      return nullptr;
    }

    const bool no_quirky_colors = false;

    CSSValue* color1 =
        ConsumeColorInternal(stream, context, no_quirky_colors, allowed_colors);
    CSSPrimitiveValue* p1 =
        ConsumePercent(stream, context, CSSPrimitiveValue::ValueRange::kAll);
    // Color can come after the percentage
    if (!color1) {
      color1 = ConsumeColorInternal(stream, context, no_quirky_colors,
                                    allowed_colors);
      if (!color1) {
        return nullptr;
      }
    }
    // Reject negative values and values > 100%, but not calc() values.
    if (auto* p1_numeric = DynamicTo<CSSNumericLiteralValue>(p1);
        p1_numeric && (p1_numeric->ComputePercentage() < 0.0 ||
                       p1_numeric->ComputePercentage() > 100.0)) {
      return nullptr;
    }

    if (!ConsumeCommaIncludingWhitespace(stream)) {
      return nullptr;
    }

    CSSValue* color2 =
        ConsumeColorInternal(stream, context, no_quirky_colors, allowed_colors);
    CSSPrimitiveValue* p2 =
        ConsumePercent(stream, context, CSSPrimitiveValue::ValueRange::kAll);
    // Color can come after the percentage
    if (!color2) {
      color2 = ConsumeColorInternal(stream, context, no_quirky_colors,
                                    allowed_colors);
      if (!color2) {
        return nullptr;
      }
    }
    // Reject negative values and values > 100%, but not calc() values.
    if (auto* p2_numeric = DynamicTo<CSSNumericLiteralValue>(p2);
        p2_numeric && (p2_numeric->ComputePercentage() < 0.0 ||
                       p2_numeric->ComputePercentage() > 100.0)) {
      return nullptr;
    }

    // If both values are literally zero (and not calc()) reject at parse time
    if (p1 && p2 && p1->IsNumericLiteralValue() &&
        To<CSSNumericLiteralValue>(p1)->ComputePercentage() == 0.0f &&
        p2->IsNumericLiteralValue() &&
        To<CSSNumericLiteralValue>(p2)->ComputePercentage() == 0.0) {
      return nullptr;
    }

    if (!stream.AtEnd()) {
      return nullptr;
    }

    guard.Release();
    result = MakeGarbageCollected<cssvalue::CSSColorMixValue>(
        color1, color2, p1, p2, color_space, hue_interpolation_method);
  }
  stream.ConsumeWhitespace();

  return result;
}

static bool ParseHexColor(CSSParserTokenStream& stream,
                          Color& result,
                          bool accept_quirky_colors) {
  const CSSParserToken& token = stream.Peek();
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
  stream.ConsumeIncludingWhitespace();
  return true;
}

namespace {

// TODO(crbug.com/1111385): Remove this when we move color-contrast()
// representation to ComputedStyle. This method does not handle currentColor
// correctly.
Color ResolveColor(CSSValue* value,
                   const ui::ColorProvider* color_provider,
                   bool is_in_web_app_scope) {
  if (auto* color = DynamicTo<cssvalue::CSSColor>(value)) {
    return color->Value();
  }

  if (auto* color = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID color_id = color->GetValueID();
    DCHECK(StyleColor::IsColorKeyword(color_id));
    return StyleColor::ColorFromKeyword(color_id,
                                        mojom::blink::ColorScheme::kLight,
                                        color_provider, is_in_web_app_scope);
  }

  NOTREACHED_IN_MIGRATION();
  return Color();
}

}  // namespace

CSSValue* ConsumeColorContrast(CSSParserTokenStream& stream,
                               const CSSParserContext& context,
                               AllowedColors allowed_colors) {
  DCHECK_EQ(stream.Peek().FunctionId(), CSSValueID::kColorContrast);

  VectorOf<CSSValue> colors_to_compare_against;
  int highest_contrast_index = -1;
  SkColor4f resolved_background_color;
  const ui::ColorProvider* color_provider = nullptr;
  const auto* document = context.GetDocument();
  bool is_in_web_app_scope = document && document->IsInWebAppScope();
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    const bool no_quirky_colors = false;

    CSSValue* background_color =
        ConsumeColorInternal(stream, context, no_quirky_colors, allowed_colors);
    if (!background_color) {
      return nullptr;
    }

    if (!ConsumeIdent<CSSValueID::kVs>(stream)) {
      return nullptr;
    }

    do {
      CSSValue* color = ConsumeColorInternal(stream, context, no_quirky_colors,
                                             allowed_colors);
      if (!color) {
        return nullptr;
      }
      colors_to_compare_against.push_back(color);
    } while (ConsumeCommaIncludingWhitespace(stream));

    if (colors_to_compare_against.size() < 2) {
      return nullptr;
    }

    std::optional<double> target_contrast;
    if (ConsumeIdent<CSSValueID::kTo>(stream)) {
      double target_contrast_temp;
      if (ConsumeIdent<CSSValueID::kAA>(stream)) {
        target_contrast = 4.5;
      } else if (ConsumeIdent<CSSValueID::kAALarge>(stream)) {
        target_contrast = 3;
      } else if (ConsumeIdent<CSSValueID::kAAA>(stream)) {
        target_contrast = 7;
      } else if (ConsumeIdent<CSSValueID::kAAALarge>(stream)) {
        target_contrast = 4.5;
      } else if (ConsumeNumberRaw(stream, context, target_contrast_temp)) {
        target_contrast = target_contrast_temp;
      } else {
        return nullptr;
      }
    }

    // Bail out if there is any trailing stuff after we parse everything
    if (!stream.AtEnd()) {
      return nullptr;
    }
    if (document) {
      // TODO(crbug.com/929098) Need to pass an appropriate color scheme here.
      color_provider = document->GetColorProviderForPainting(
          mojom::blink::ColorScheme::kLight);
    }
    // TODO(crbug.com/1111385): Represent |background_color| and
    // |colors_to_compare_against| in ComputedStyle and evaluate with
    // currentColor and other variables at used-value time instead of doing it
    // at parse time below.
    resolved_background_color =
        ResolveColor(background_color, color_provider, is_in_web_app_scope)
            .toSkColor4f();
    float highest_contrast_ratio = 0;
    for (unsigned i = 0; i < colors_to_compare_against.size(); i++) {
      float contrast_ratio = color_utils::GetContrastRatio(
          resolved_background_color,
          ResolveColor(colors_to_compare_against[i], color_provider,
                       is_in_web_app_scope)
              .toSkColor4f());
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

    guard.Release();
  }
  stream.ConsumeWhitespace();

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
      ResolveColor(colors_to_compare_against[highest_contrast_index],
                   color_provider, is_in_web_app_scope));
}

namespace {

bool SystemAccentColorAllowed(const CSSParserContext& context) {
  if (!RuntimeEnabledFeatures::CSSSystemAccentColorEnabled()) {
    return false;
  }

  // We should not allow the system accent color to be rendered in image
  // contexts because it could be read back by the page and used for
  // fingerprinting.
  if (const auto* document = context.GetDocument()) {
    if (document->GetPage()->GetChromeClient().IsIsolatedSVGChromeClient()) {
      return false;
    }
  }

  return true;
}

CSSValue* ConsumeColorInternal(CSSParserTokenStream& stream,
                               const CSSParserContext& context,
                               bool accept_quirky_colors,
                               AllowedColors allowed_colors) {
  if (RuntimeEnabledFeatures::CSSColorContrastEnabled() &&
      stream.Peek().FunctionId() == CSSValueID::kColorContrast) {
    return ConsumeColorContrast(stream, context, allowed_colors);
  }

  if (stream.Peek().FunctionId() == CSSValueID::kColorMix) {
    CSSValue* color = ConsumeColorMixFunction(stream, context, allowed_colors);
    return color;
  }

  CSSValueID id = stream.Peek().Id();
  if ((id == CSSValueID::kAccentcolor || id == CSSValueID::kAccentcolortext) &&
      !SystemAccentColorAllowed(context)) {
    return nullptr;
  }
  if (StyleColor::IsColorKeyword(id)) {
    if (!isValueAllowedInMode(id, context.Mode())) {
      return nullptr;
    }
    if (allowed_colors == AllowedColors::kAbsolute &&
        (id == CSSValueID::kCurrentcolor ||
         StyleColor::IsSystemColorIncludingDeprecated(id) ||
         StyleColor::IsSystemColor(id))) {
      return nullptr;
    }
    CSSIdentifierValue* color = ConsumeIdent(stream);
    return color;
  }

  Color color = Color::kTransparent;
  if (ParseHexColor(stream, color, accept_quirky_colors)) {
    return cssvalue::CSSColor::Create(color);
  }

  // Parses the color inputs rgb(), rgba(), hsl(), hsla(), hwb(), lab(),
  // oklab(), lch(), oklch() and color(). https://www.w3.org/TR/css-color-4/
  ColorFunctionParser parser;
  if (CSSValue* functional_syntax_color =
          parser.ConsumeFunctionalSyntaxColor(stream, context)) {
    return functional_syntax_color;
  }

  if (RuntimeEnabledFeatures::CustomizableSelectEnabled() &&
      IsUASheetBehavior(context.Mode())) {
    if (CSSAppearanceAutoBaseSelectValuePair* auto_base_select_pair =
            ConsumeAppearanceAutoBaseSelectColor(stream, context)) {
      return auto_base_select_pair;
    }
  }

  if (allowed_colors == AllowedColors::kAll) {
    return ConsumeLightDark(ConsumeColor, stream, context);
  }
  return nullptr;
}

}  // namespace

CSSValue* ConsumeColorMaybeQuirky(CSSParserTokenStream& stream,
                                  const CSSParserContext& context) {
  return ConsumeColorInternal(stream, context,
                              IsQuirksModeBehavior(context.Mode()),
                              AllowedColors::kAll);
}

CSSValue* ConsumeColor(CSSParserTokenStream& stream,
                       const CSSParserContext& context) {
  return ConsumeColorInternal(stream, context, false /* accept_quirky_colors */,
                              AllowedColors::kAll);
}

CSSValue* ConsumeAbsoluteColor(CSSParserTokenStream& stream,
                               const CSSParserContext& context) {
  return ConsumeColorInternal(stream, context, false /* accept_quirky_colors */,
                              AllowedColors::kAbsolute);
}

CSSValue* ConsumeLineWidth(CSSParserTokenStream& stream,
                           const CSSParserContext& context,
                           UnitlessQuirk unitless) {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kThin || id == CSSValueID::kMedium ||
      id == CSSValueID::kThick) {
    return ConsumeIdent(stream);
  }
  return ConsumeLength(stream, context,
                       CSSPrimitiveValue::ValueRange::kNonNegative, unitless);
}

static CSSValue* ConsumePositionComponent(CSSParserTokenStream& stream,
                                          const CSSParserContext& context,
                                          UnitlessQuirk unitless,
                                          bool& horizontal_edge,
                                          bool& vertical_edge) {
  if (stream.Peek().GetType() != kIdentToken) {
    return ConsumeLengthOrPercent(
        stream, context, CSSPrimitiveValue::ValueRange::kAll, unitless);
  }

  CSSValueID id = stream.Peek().Id();
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
  return ConsumeIdent(stream);
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

static void PositionFromThreeOrFourValues(
    const std::array<CSSValue*, 5>& values,
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

bool ConsumePosition(CSSParserTokenStream& stream,
                     const CSSParserContext& context,
                     UnitlessQuirk unitless,
                     std::optional<WebFeature> three_value_position,
                     CSSValue*& result_x,
                     CSSValue*& result_y) {
  bool horizontal_edge = false;
  bool vertical_edge = false;
  CSSValue* value1 = ConsumePositionComponent(stream, context, unitless,
                                              horizontal_edge, vertical_edge);
  if (!value1) {
    return false;
  }
  if (!value1->IsIdentifierValue()) {
    horizontal_edge = true;
  }

  CSSParserTokenStream::State savepoint_after_first_consume = stream.Save();
  CSSValue* value2 = ConsumePositionComponent(stream, context, unitless,
                                              horizontal_edge, vertical_edge);
  if (!value2) {
    PositionFromOneValue(value1, result_x, result_y);
    return true;
  }

  CSSParserTokenStream::State savepoint_after_second_consume = stream.Save();
  CSSValue* value3 = nullptr;
  auto* identifier_value1 = DynamicTo<CSSIdentifierValue>(value1);
  auto* identifier_value2 = DynamicTo<CSSIdentifierValue>(value2);
  // TODO(crbug.com/940442): Fix the strange comparison of a
  // CSSIdentifierValue instance against a specific "stream peek" type check.
  if (identifier_value1 &&
      !!identifier_value2 != (stream.Peek().GetType() == kIdentToken) &&
      (identifier_value2
           ? identifier_value2->GetValueID()
           : identifier_value1->GetValueID()) != CSSValueID::kCenter) {
    value3 = ConsumePositionComponent(stream, context, unitless,
                                      horizontal_edge, vertical_edge);
  }
  if (!value3) {
    if (vertical_edge && !value2->IsIdentifierValue()) {
      stream.Restore(savepoint_after_first_consume);
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
      stream.Peek().GetType() != kIdentToken) {
    value4 = ConsumePositionComponent(stream, context, unitless,
                                      horizontal_edge, vertical_edge);
  }

  if (!value4) {
    if (!three_value_position) {
      // [top | bottom] <length-percentage> is not permitted
      if (vertical_edge && !value2->IsIdentifierValue()) {
        stream.Restore(savepoint_after_first_consume);
        PositionFromOneValue(value1, result_x, result_y);
        return true;
      }
      stream.Restore(savepoint_after_second_consume);
      PositionFromTwoValues(value1, value2, result_x, result_y);
      return true;
    }
    DCHECK_EQ(*three_value_position,
              WebFeature::kThreeValuedPositionBackground);
    context.Count(*three_value_position);
  }

  std::array<CSSValue*, 5> values = {value1, value2, value3, value4, nullptr};
  PositionFromThreeOrFourValues(values, result_x, result_y);
  return true;
}

CSSValuePair* ConsumePosition(CSSParserTokenStream& stream,
                              const CSSParserContext& context,
                              UnitlessQuirk unitless,
                              std::optional<WebFeature> three_value_position) {
  CSSValue* result_x = nullptr;
  CSSValue* result_y = nullptr;
  if (ConsumePosition(stream, context, unitless, three_value_position, result_x,
                      result_y)) {
    return MakeGarbageCollected<CSSValuePair>(
        result_x, result_y, CSSValuePair::kKeepIdenticalValues);
  }
  return nullptr;
}

bool ConsumeOneOrTwoValuedPosition(CSSParserTokenStream& stream,
                                   const CSSParserContext& context,
                                   UnitlessQuirk unitless,
                                   CSSValue*& result_x,
                                   CSSValue*& result_y) {
  bool horizontal_edge = false;
  bool vertical_edge = false;
  CSSValue* value1 = ConsumePositionComponent(stream, context, unitless,
                                              horizontal_edge, vertical_edge);
  if (!value1) {
    return false;
  }
  if (!value1->IsIdentifierValue()) {
    horizontal_edge = true;
  }

  if (vertical_edge &&
      ConsumeLengthOrPercent(stream, context,
                             CSSPrimitiveValue::ValueRange::kAll, unitless)) {
    // <length-percentage> is not permitted after top | bottom.
    return false;
  }
  CSSValue* value2 = ConsumePositionComponent(stream, context, unitless,
                                              horizontal_edge, vertical_edge);
  if (!value2) {
    PositionFromOneValue(value1, result_x, result_y);
    return true;
  }
  PositionFromTwoValues(value1, value2, result_x, result_y);
  return true;
}

bool ConsumeBorderShorthand(CSSParserTokenStream& stream,
                            const CSSParserContext& context,
                            const CSSParserLocalContext& local_context,
                            const CSSValue*& result_width,
                            const CSSValue*& result_style,
                            const CSSValue*& result_color) {
  while (!result_width || !result_style || !result_color) {
    if (!result_width) {
      result_width = ParseBorderWidthSide(stream, context, local_context);
      if (result_width) {
        ConsumeCommaIncludingWhitespace(stream);
        continue;
      }
    }
    if (!result_style) {
      result_style = ParseBorderStyleSide(stream, context);
      if (result_style) {
        ConsumeCommaIncludingWhitespace(stream);
        continue;
      }
    }
    if (!result_color) {
      result_color = ConsumeBorderColorSide(stream, context, local_context);
      if (result_color) {
        ConsumeCommaIncludingWhitespace(stream);
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    bool horizontal) {
  if (stream.Peek().GetType() == kIdentToken) {
    if ((horizontal && ConsumeIdent<CSSValueID::kLeft>(stream)) ||
        (!horizontal && ConsumeIdent<CSSValueID::kTop>(stream))) {
      return CSSNumericLiteralValue::Create(
          0., CSSPrimitiveValue::UnitType::kPercentage);
    }
    if ((horizontal && ConsumeIdent<CSSValueID::kRight>(stream)) ||
        (!horizontal && ConsumeIdent<CSSValueID::kBottom>(stream))) {
      return CSSNumericLiteralValue::Create(
          100., CSSPrimitiveValue::UnitType::kPercentage);
    }
    if (ConsumeIdent<CSSValueID::kCenter>(stream)) {
      return CSSNumericLiteralValue::Create(
          50., CSSPrimitiveValue::UnitType::kPercentage);
    }
    return nullptr;
  }
  CSSPrimitiveValue* result =
      ConsumePercent(stream, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!result) {
    result =
        ConsumeNumber(stream, context, CSSPrimitiveValue::ValueRange::kAll);
  }
  return result;
}

// Used to parse colors for -webkit-gradient(...).
static CSSValue* ConsumeDeprecatedGradientStopColor(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kCurrentcolor) {
    return nullptr;
  }
  return ConsumeColor(stream, context);
}

static bool ConsumeDeprecatedGradientColorStop(
    CSSParserTokenStream& stream,
    cssvalue::CSSGradientColorStop& stop,
    const CSSParserContext& context) {
  CSSValueID id = stream.Peek().FunctionId();
  if (id != CSSValueID::kFrom && id != CSSValueID::kTo &&
      id != CSSValueID::kColorStop) {
    return false;
  }

  {
    CSSParserTokenStream::BlockGuard guard(stream);
    stream.ConsumeWhitespace();
    double position;
    if (id == CSSValueID::kFrom || id == CSSValueID::kTo) {
      position = (id == CSSValueID::kFrom) ? 0 : 1;
    } else {
      DCHECK(id == CSSValueID::kColorStop);
      if (CSSPrimitiveValue* percent_value = ConsumePercent(
              stream, context, CSSPrimitiveValue::ValueRange::kAll)) {
        position = percent_value->GetDoubleValue() / 100.0;
      } else if (!ConsumeNumberRaw(stream, context, position)) {
        return false;
      }

      if (!ConsumeCommaIncludingWhitespace(stream)) {
        return false;
      }
    }

    stop.offset_ = CSSNumericLiteralValue::Create(
        position, CSSPrimitiveValue::UnitType::kNumber);
    stop.color_ = ConsumeDeprecatedGradientStopColor(stream, context);
    if (!stream.AtEnd()) {
      return false;
    }
  }
  stream.ConsumeWhitespace();
  return stop.color_;
}

static CSSValue* ConsumeDeprecatedGradient(CSSParserTokenStream& stream,
                                           const CSSParserContext& context) {
  CSSValueID id = stream.Peek().Id();
  if (id != CSSValueID::kRadial && id != CSSValueID::kLinear) {
    return nullptr;
  }
  stream.ConsumeIncludingWhitespace();  // id

  if (!ConsumeCommaIncludingWhitespace(stream)) {
    return nullptr;
  }

  const CSSPrimitiveValue* first_x =
      ConsumeDeprecatedGradientPoint(stream, context, true);
  if (!first_x) {
    return nullptr;
  }
  const CSSPrimitiveValue* first_y =
      ConsumeDeprecatedGradientPoint(stream, context, false);
  if (!first_y) {
    return nullptr;
  }
  if (!ConsumeCommaIncludingWhitespace(stream)) {
    return nullptr;
  }

  // For radial gradients only, we now expect a numeric radius.
  const CSSPrimitiveValue* first_radius = nullptr;
  if (id == CSSValueID::kRadial) {
    first_radius = ConsumeNumber(stream, context,
                                 CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!first_radius || !ConsumeCommaIncludingWhitespace(stream)) {
      return nullptr;
    }
  }

  const CSSPrimitiveValue* second_x =
      ConsumeDeprecatedGradientPoint(stream, context, true);
  if (!second_x) {
    return nullptr;
  }
  const CSSPrimitiveValue* second_y =
      ConsumeDeprecatedGradientPoint(stream, context, false);
  if (!second_y) {
    return nullptr;
  }

  // For radial gradients only, we now expect the second radius.
  const CSSPrimitiveValue* second_radius = nullptr;
  if (id == CSSValueID::kRadial) {
    if (!ConsumeCommaIncludingWhitespace(stream)) {
      return nullptr;
    }
    second_radius = ConsumeNumber(stream, context,
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
  while (ConsumeCommaIncludingWhitespace(stream)) {
    if (!ConsumeDeprecatedGradientColorStop(stream, stop, context)) {
      return nullptr;
    }
    result->AddStop(stop);
  }

  return result;
}

static CSSPrimitiveValue* ConsumeGradientAngleOrPercent(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    CSSPrimitiveValue::ValueRange value_range,
    UnitlessQuirk) {
  const CSSParserToken& token = stream.Peek();
  if (token.GetType() == kDimensionToken || token.GetType() == kNumberToken) {
    return ConsumeAngle(stream, context,
                        WebFeature::kUnitlessZeroAngleGradient);
  }
  if (token.GetType() == kPercentageToken) {
    return ConsumePercent(stream, context, value_range);
  }
  MathFunctionParser math_parser(stream, context, value_range);
  if (const CSSMathFunctionValue* calculation = math_parser.Value()) {
    CalculationResultCategory category = calculation->Category();
    // TODO(fs): Add and support kCalcPercentAngle?
    if (category == kCalcAngle || category == kCalcPercent) {
      return math_parser.ConsumeValue();
    }
  }
  return nullptr;
}

using PositionFunctor = CSSPrimitiveValue* (*)(CSSParserTokenStream&,
                                               const CSSParserContext&,
                                               CSSPrimitiveValue::ValueRange,
                                               UnitlessQuirk);

static bool ConsumeGradientColorStops(CSSParserTokenStream& stream,
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
    stop.color_ = ConsumeColor(stream, context);
    // Two hints in a row are not allowed.
    if (!stop.color_ &&
        (!supports_color_hints || previous_stop_was_color_hint)) {
      return false;
    }
    previous_stop_was_color_hint = !stop.color_;
    stop.offset_ = consume_position_func(stream, context,
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
    stop.offset_ = consume_position_func(stream, context,
                                         CSSPrimitiveValue::ValueRange::kAll,
                                         UnitlessQuirk::kForbid);
    if (stop.offset_) {
      gradient->AddStop(stop);
    }
  } while (ConsumeCommaIncludingWhitespace(stream));

  // The last color stop cannot be a color hint.
  if (previous_stop_was_color_hint) {
    return false;
  }

  // Must have 2 or more stops to be valid.
  return gradient->StopCount() >= 2;
}

static CSSValue* ConsumeDeprecatedRadialGradient(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    cssvalue::CSSGradientRepeat repeating) {
  CSSValue* center_x = nullptr;
  CSSValue* center_y = nullptr;
  ConsumeOneOrTwoValuedPosition(stream, context, UnitlessQuirk::kForbid,
                                center_x, center_y);
  if ((center_x || center_y) && !ConsumeCommaIncludingWhitespace(stream)) {
    return nullptr;
  }

  const CSSIdentifierValue* shape =
      ConsumeIdent<CSSValueID::kCircle, CSSValueID::kEllipse>(stream);
  const CSSIdentifierValue* size_keyword =
      ConsumeIdent<CSSValueID::kClosestSide, CSSValueID::kClosestCorner,
                   CSSValueID::kFarthestSide, CSSValueID::kFarthestCorner,
                   CSSValueID::kContain, CSSValueID::kCover>(stream);
  if (!shape) {
    shape = ConsumeIdent<CSSValueID::kCircle, CSSValueID::kEllipse>(stream);
  }

  // Or, two lengths or percentages
  const CSSPrimitiveValue* horizontal_size = nullptr;
  const CSSPrimitiveValue* vertical_size = nullptr;
  if (!shape && !size_keyword) {
    horizontal_size = ConsumeLengthOrPercent(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (horizontal_size) {
      vertical_size = ConsumeLengthOrPercent(
          stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      if (!vertical_size) {
        return nullptr;
      }
      ConsumeCommaIncludingWhitespace(stream);
    }
  } else {
    ConsumeCommaIncludingWhitespace(stream);
  }

  cssvalue::CSSGradientValue* result =
      MakeGarbageCollected<cssvalue::CSSRadialGradientValue>(
          center_x, center_y, shape, size_keyword, horizontal_size,
          vertical_size, repeating, cssvalue::kCSSPrefixedRadialGradient);
  return ConsumeGradientColorStops(stream, context, result,
                                   ConsumeGradientLengthOrPercent)
             ? result
             : nullptr;
}

static CSSValue* ConsumeRadialGradient(CSSParserTokenStream& stream,
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
      stream, color_space, hue_interpolation_method);

  for (int i = 0; i < 3; ++i) {
    if (stream.Peek().GetType() == kIdentToken) {
      CSSValueID id = stream.Peek().Id();
      if (id == CSSValueID::kCircle || id == CSSValueID::kEllipse) {
        if (shape) {
          return nullptr;
        }
        shape = ConsumeIdent(stream);
      } else if (id == CSSValueID::kClosestSide ||
                 id == CSSValueID::kClosestCorner ||
                 id == CSSValueID::kFarthestSide ||
                 id == CSSValueID::kFarthestCorner) {
        if (size_keyword) {
          return nullptr;
        }
        size_keyword = ConsumeIdent(stream);
      } else {
        break;
      }
    } else {
      CSSPrimitiveValue* center = ConsumeLengthOrPercent(
          stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      if (!center) {
        break;
      }
      if (horizontal_size) {
        return nullptr;
      }
      horizontal_size = center;
      center = ConsumeLengthOrPercent(
          stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
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
  if (stream.Peek().Id() == CSSValueID::kAt) {
    stream.ConsumeIncludingWhitespace();
    ConsumePosition(stream, context, UnitlessQuirk::kForbid,
                    std::optional<WebFeature>(), center_x, center_y);
    if (!(center_x && center_y)) {
      return nullptr;
    }
    // Right now, CSS radial gradients have the same start and end centers.
  }

  if (!has_color_space) {
    has_color_space = ConsumeColorInterpolationSpace(stream, color_space,
                                                     hue_interpolation_method);
  }

  if ((shape || size_keyword || horizontal_size || center_x || center_y ||
       has_color_space) &&
      !ConsumeCommaIncludingWhitespace(stream)) {
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

  return ConsumeGradientColorStops(stream, context, result,
                                   ConsumeGradientLengthOrPercent)
             ? result
             : nullptr;
}

static CSSValue* ConsumeLinearGradient(
    CSSParserTokenStream& stream,
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
      stream, color_space, hue_interpolation_method);

  const CSSPrimitiveValue* angle =
      ConsumeAngle(stream, context, WebFeature::kUnitlessZeroAngleGradient);
  const CSSIdentifierValue* end_x = nullptr;
  const CSSIdentifierValue* end_y = nullptr;
  if (!angle) {
    // <side-or-corner> parsing
    if (gradient_type == cssvalue::kCSSPrefixedLinearGradient ||
        ConsumeIdent<CSSValueID::kTo>(stream)) {
      end_x = ConsumeIdent<CSSValueID::kLeft, CSSValueID::kRight>(stream);
      end_y = ConsumeIdent<CSSValueID::kBottom, CSSValueID::kTop>(stream);
      if (!end_x && !end_y) {
        if (gradient_type == cssvalue::kCSSLinearGradient) {
          return nullptr;
        }
        end_y = CSSIdentifierValue::Create(CSSValueID::kTop);
        expect_comma = false;
      } else if (!end_x) {
        end_x = ConsumeIdent<CSSValueID::kLeft, CSSValueID::kRight>(stream);
      }
    } else {
      // No <angle> or <side-to-corner>
      expect_comma = false;
    }
  }
  // It's possible that the <color-space> comes after the [ <angle> |
  // <side-or-corner> ]
  if (!has_color_space) {
    has_color_space = ConsumeColorInterpolationSpace(stream, color_space,
                                                     hue_interpolation_method);
  }

  if (has_color_space) {
    expect_comma = true;
  }

  if (expect_comma && !ConsumeCommaIncludingWhitespace(stream)) {
    return nullptr;
  }

  cssvalue::CSSGradientValue* result =
      MakeGarbageCollected<cssvalue::CSSLinearGradientValue>(
          end_x, end_y, nullptr, nullptr, angle, repeating, gradient_type);

  if (has_color_space) {
    result->SetColorInterpolationSpace(color_space, hue_interpolation_method);
    context.Count(WebFeature::kCSSColorGradientColorSpace);
  }

  return ConsumeGradientColorStops(stream, context, result,
                                   ConsumeGradientLengthOrPercent)
             ? result
             : nullptr;
}

static CSSValue* ConsumeConicGradient(CSSParserTokenStream& stream,
                                      const CSSParserContext& context,
                                      cssvalue::CSSGradientRepeat repeating) {
  Color::ColorSpace color_space;
  Color::HueInterpolationMethod hue_interpolation_method =
      Color::HueInterpolationMethod::kShorter;
  bool has_color_space = ConsumeColorInterpolationSpace(
      stream, color_space, hue_interpolation_method);

  const CSSPrimitiveValue* from_angle = nullptr;
  if (ConsumeIdent<CSSValueID::kFrom>(stream)) {
    if (!(from_angle = ConsumeAngle(stream, context,
                                    WebFeature::kUnitlessZeroAngleGradient))) {
      return nullptr;
    }
  }

  CSSValue* center_x = nullptr;
  CSSValue* center_y = nullptr;
  if (ConsumeIdent<CSSValueID::kAt>(stream)) {
    if (!ConsumePosition(stream, context, UnitlessQuirk::kForbid,
                         std::optional<WebFeature>(), center_x, center_y)) {
      return nullptr;
    }
  }

  if (!has_color_space) {
    has_color_space = ConsumeColorInterpolationSpace(stream, color_space,
                                                     hue_interpolation_method);
  }

  // Comma separator required when fromAngle, position or color_space is
  // present.
  if ((from_angle || center_x || center_y || has_color_space) &&
      !ConsumeCommaIncludingWhitespace(stream)) {
    return nullptr;
  }

  auto* result = MakeGarbageCollected<cssvalue::CSSConicGradientValue>(
      center_x, center_y, from_angle, repeating);

  if (has_color_space) {
    result->SetColorInterpolationSpace(color_space, hue_interpolation_method);
    context.Count(WebFeature::kCSSColorGradientColorSpace);
  }

  return ConsumeGradientColorStops(stream, context, result,
                                   ConsumeGradientAngleOrPercent)
             ? result
             : nullptr;
}

CSSValue* ConsumeImageOrNone(CSSParserTokenStream& stream,
                             const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(stream);
  }
  return ConsumeImage(stream, context);
}

CSSValue* ConsumeAxis(CSSParserTokenStream& stream,
                      const CSSParserContext& context) {
  CSSValueID axis_id = stream.Peek().Id();
  if (axis_id == CSSValueID::kX || axis_id == CSSValueID::kY ||
      axis_id == CSSValueID::kZ) {
    ConsumeIdent(stream);
    return MakeGarbageCollected<cssvalue::CSSAxisValue>(axis_id);
  }

  CSSValue* x_dimension =
      ConsumeNumber(stream, context, CSSPrimitiveValue::ValueRange::kAll);
  CSSValue* y_dimension =
      ConsumeNumber(stream, context, CSSPrimitiveValue::ValueRange::kAll);
  CSSValue* z_dimension =
      ConsumeNumber(stream, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!x_dimension || !y_dimension || !z_dimension) {
    return nullptr;
  }
  return MakeGarbageCollected<cssvalue::CSSAxisValue>(
      To<CSSPrimitiveValue>(x_dimension), To<CSSPrimitiveValue>(y_dimension),
      To<CSSPrimitiveValue>(z_dimension));
}

CSSValue* ConsumeIntrinsicSizeLonghand(CSSParserTokenStream& stream,
                                       const CSSParserContext& context) {
  if (css_parsing_utils::IdentMatches<CSSValueID::kNone>(stream.Peek().Id())) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (css_parsing_utils::IdentMatches<CSSValueID::kAuto>(stream.Peek().Id())) {
    list->Append(*css_parsing_utils::ConsumeIdent(stream));
  }
  if (css_parsing_utils::IdentMatches<CSSValueID::kNone>(stream.Peek().Id())) {
    list->Append(*css_parsing_utils::ConsumeIdent(stream));
  } else {
    CSSValue* length = css_parsing_utils::ConsumeLength(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!length) {
      return nullptr;
    }
    list->Append(*length);
  }
  return list;
}

static CSSValue* ConsumeDeprecatedWebkitCrossFade(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  CSSValue* from_image_value = ConsumeImageOrNone(stream, context);
  if (!from_image_value || !ConsumeCommaIncludingWhitespace(stream)) {
    return nullptr;
  }
  CSSValue* to_image_value = ConsumeImageOrNone(stream, context);
  if (!to_image_value || !ConsumeCommaIncludingWhitespace(stream)) {
    return nullptr;
  }

  CSSPrimitiveValue* percentage = nullptr;
  if (CSSPrimitiveValue* percent_value = ConsumePercent(
          stream, context, CSSPrimitiveValue::ValueRange::kAll)) {
    percentage = CSSNumericLiteralValue::Create(
        ClampTo<double>(percent_value->GetDoubleValue() / 100.0, 0, 1),
        CSSPrimitiveValue::UnitType::kNumber);
  } else if (CSSPrimitiveValue* number_value = ConsumeNumber(
                 stream, context, CSSPrimitiveValue::ValueRange::kAll)) {
    percentage = CSSNumericLiteralValue::Create(
        ClampTo<double>(number_value->GetDoubleValue(), 0, 1),
        CSSPrimitiveValue::UnitType::kNumber);
  }

  if (!percentage) {
    return nullptr;
  }
  return MakeGarbageCollected<cssvalue::CSSCrossfadeValue>(
      /*is_legacy_variant=*/true,
      HeapVector<std::pair<Member<CSSValue>, Member<CSSPrimitiveValue>>>{
          {from_image_value, nullptr}, {to_image_value, percentage}});
}

// https://drafts.csswg.org/css-images-4/#cross-fade-function
static CSSValue* ConsumeCrossFade(CSSParserTokenStream& stream,
                                  const CSSParserContext& context) {
  // Parse an arbitrary comma-separated image|color values,
  // where each image may have a percentage before or after it.
  HeapVector<std::pair<Member<CSSValue>, Member<CSSPrimitiveValue>>>
      image_and_percentages;
  CSSValue* image = nullptr;
  CSSPrimitiveValue* percentage = nullptr;
  for (;;) {
    if (CSSPrimitiveValue* percent_value = ConsumePercent(
            stream, context, CSSPrimitiveValue::ValueRange::kAll)) {
      if (percentage) {
        return nullptr;
      }
      if (percent_value->IsNumericLiteralValue()) {
        double val = percent_value->GetDoubleValue();
        if (!(val >= 0.0 &&
              val <= 100.0)) {  // Includes checks for NaN and infinities.
          return nullptr;
        }
      }
      percentage = percent_value;
      continue;
    } else if (CSSValue* image_value = ConsumeImage(stream, context)) {
      if (image) {
        return nullptr;
      }
      image = image_value;
    } else if (CSSValue* color_value = ConsumeColor(stream, context)) {
      if (image) {
        return nullptr;
      }

      // Wrap the color in a constant gradient, so that we can treat it as a
      // gradient in nearly all the remaining code.
      image =
          MakeGarbageCollected<cssvalue::CSSConstantGradientValue>(color_value);
    } else {
      if (!image) {
        return nullptr;
      }
      image_and_percentages.emplace_back(image, percentage);
      image = nullptr;
      percentage = nullptr;
      if (!ConsumeCommaIncludingWhitespace(stream)) {
        break;
      }
    }
  }
  if (image_and_percentages.empty()) {
    return nullptr;
  }

  return MakeGarbageCollected<cssvalue::CSSCrossfadeValue>(
      /*is_legacy_variant=*/false, image_and_percentages);
}

static CSSValue* ConsumePaint(CSSParserTokenStream& stream,
                              const CSSParserContext& context) {
  CSSCustomIdentValue* name = ConsumeCustomIdent(stream, context);
  if (!name) {
    return nullptr;
  }

  if (stream.AtEnd()) {
    return MakeGarbageCollected<CSSPaintValue>(name);
  }

  if (!RuntimeEnabledFeatures::CSSPaintAPIArgumentsEnabled()) {
    // Arguments not enabled, but exists. Invalid.
    return nullptr;
  }

  // Begin parse paint arguments.
  if (!ConsumeCommaIncludingWhitespace(stream)) {
    return nullptr;
  }

  // Consume arguments.
  // TODO(renjieliu): We may want to optimize the implementation by resolve
  // variables early if paint function is registered.
  Vector<CSSParserToken> argument_tokens;
  HeapVector<Member<CSSVariableData>> variable_data;
  bool first_argument = true;
  while (!stream.AtEnd()) {
    stream.ConsumeWhitespace();
    if (!first_argument) {
      if (stream.Peek().GetType() != kCommaToken) {
        return nullptr;
      }
      ConsumeCommaIncludingWhitespace(stream);
      if (stream.AtEnd()) {
        return nullptr;
      }
    }
    bool important_ignored;
    CSSVariableData* argument = CSSVariableParser::ConsumeUnparsedDeclaration(
        stream, /*allow_important_annotation=*/false,
        /*is_animation_tainted=*/false,
        /*must_contain_variable_reference=*/false,
        /*restricted_value=*/false, /*comma_ends_declaration=*/true,
        important_ignored, context.GetExecutionContext());
    if (!argument) {
      return nullptr;
    }
    if (argument->NeedsVariableResolution()) {
      // If we see an un-substituted var() or similar, it is a sign that
      // we are in parsing (as opposed to resolving, where it would be
      // substituted). We need to return an error so that the value as a whole
      // becomes an unparsed value; we will be called back during resolving
      // with all substitutions done.
      //
      // This is something most properties do implicitly, since var() would
      // be a parse error. But since we accept pretty much any token sequence
      // as arguments to paint(), we need to make this check explicitly here.
      return nullptr;
    }
    variable_data.push_back(argument);
    first_argument = false;
  }

  return MakeGarbageCollected<CSSPaintValue>(name, std::move(variable_data));
}

static CSSValue* ConsumeGeneratedImage(CSSParserTokenStream& stream,
                                       const CSSParserContext& context) {
  CSSValueID id = stream.Peek().FunctionId();
  if (!IsGeneratedImage(id)) {
    return nullptr;
  }

  CSSValue* result = nullptr;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    if (id == CSSValueID::kRadialGradient) {
      result = ConsumeRadialGradient(stream, context, cssvalue::kNonRepeating);
    } else if (id == CSSValueID::kRepeatingRadialGradient) {
      result = ConsumeRadialGradient(stream, context, cssvalue::kRepeating);
    } else if (id == CSSValueID::kWebkitLinearGradient) {
      context.Count(WebFeature::kDeprecatedWebKitLinearGradient);
      result = ConsumeLinearGradient(stream, context, cssvalue::kNonRepeating,
                                     cssvalue::kCSSPrefixedLinearGradient);
    } else if (id == CSSValueID::kWebkitRepeatingLinearGradient) {
      context.Count(WebFeature::kDeprecatedWebKitRepeatingLinearGradient);
      result = ConsumeLinearGradient(stream, context, cssvalue::kRepeating,
                                     cssvalue::kCSSPrefixedLinearGradient);
    } else if (id == CSSValueID::kRepeatingLinearGradient) {
      result = ConsumeLinearGradient(stream, context, cssvalue::kRepeating,
                                     cssvalue::kCSSLinearGradient);
    } else if (id == CSSValueID::kLinearGradient) {
      result = ConsumeLinearGradient(stream, context, cssvalue::kNonRepeating,
                                     cssvalue::kCSSLinearGradient);
    } else if (id == CSSValueID::kWebkitGradient) {
      context.Count(WebFeature::kDeprecatedWebKitGradient);
      result = ConsumeDeprecatedGradient(stream, context);
    } else if (id == CSSValueID::kWebkitRadialGradient) {
      context.Count(WebFeature::kDeprecatedWebKitRadialGradient);
      result = ConsumeDeprecatedRadialGradient(stream, context,
                                               cssvalue::kNonRepeating);
    } else if (id == CSSValueID::kWebkitRepeatingRadialGradient) {
      context.Count(WebFeature::kDeprecatedWebKitRepeatingRadialGradient);
      result = ConsumeDeprecatedRadialGradient(stream, context,
                                               cssvalue::kRepeating);
    } else if (id == CSSValueID::kConicGradient) {
      result = ConsumeConicGradient(stream, context, cssvalue::kNonRepeating);
    } else if (id == CSSValueID::kRepeatingConicGradient) {
      result = ConsumeConicGradient(stream, context, cssvalue::kRepeating);
    } else if (id == CSSValueID::kWebkitCrossFade) {
      result = ConsumeDeprecatedWebkitCrossFade(stream, context);
    } else if (RuntimeEnabledFeatures::CSSCrossFadeEnabled() &&
               id == CSSValueID::kCrossFade) {
      result = ConsumeCrossFade(stream, context);
    } else if (id == CSSValueID::kPaint) {
      result =
          context.IsSecureContext() ? ConsumePaint(stream, context) : nullptr;
    }
    if (!result || !stream.AtEnd()) {
      return nullptr;
    }
    guard.Release();
  }
  stream.ConsumeWhitespace();

  WebFeature feature;
  if (id == CSSValueID::kWebkitCrossFade) {
    feature = WebFeature::kWebkitCrossFade;
  } else if (id == CSSValueID::kPaint) {
    feature = WebFeature::kCSSPaintFunction;
  } else {
    feature = WebFeature::kCSSGradient;
  }
  context.Count(feature);

  return result;
}

static CSSImageValue* CreateCSSImageValueWithReferrer(
    const StringView& uri,
    const CSSParserContext& context) {
  auto* image_value =
      MakeGarbageCollected<CSSImageValue>(CollectUrlData(uri, context));
  if (context.Mode() == kUASheetMode) {
    image_value->SetInitiator(fetch_initiator_type_names::kUacss);
  }
  return image_value;
}

static CSSImageSetTypeValue* ConsumeImageSetType(CSSParserTokenStream& stream) {
  if (stream.Peek().FunctionId() != CSSValueID::kType) {
    return nullptr;
  }

  String type;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    type = ConsumeStringAsString(stream);
    if (type.IsNull() || !stream.AtEnd()) {
      return nullptr;
    }

    guard.Release();
  }
  stream.ConsumeWhitespace();
  return MakeGarbageCollected<CSSImageSetTypeValue>(type);
}

static CSSImageSetOptionValue* ConsumeImageSetOption(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    ConsumeGeneratedImagePolicy generated_image_policy) {
  const CSSValue* image = ConsumeImage(stream, context, generated_image_policy,
                                       ConsumeStringUrlImagePolicy::kAllow,
                                       ConsumeImageSetImagePolicy::kForbid);
  if (!image) {
    return nullptr;
  }

  // Type could appear before or after resolution
  CSSImageSetTypeValue* type = ConsumeImageSetType(stream);
  CSSPrimitiveValue* resolution = ConsumeResolution(stream, context);
  if (!type) {
    type = ConsumeImageSetType(stream);
  }

  return MakeGarbageCollected<CSSImageSetOptionValue>(image, resolution, type);
}

static CSSValue* ConsumeImageSet(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    ConsumeGeneratedImagePolicy generated_image_policy =
        ConsumeGeneratedImagePolicy::kAllow) {
  auto* image_set = MakeGarbageCollected<CSSImageSetValue>();
  CSSValueID function_id = stream.Peek().FunctionId();
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    do {
      auto* image_set_option =
          ConsumeImageSetOption(stream, context, generated_image_policy);
      if (!image_set_option) {
        return nullptr;
      }

      image_set->Append(*image_set_option);
    } while (ConsumeCommaIncludingWhitespace(stream));

    if (!stream.AtEnd()) {
      return nullptr;
    }

    switch (function_id) {
      case CSSValueID::kWebkitImageSet:
        context.Count(WebFeature::kWebkitImageSet);
        break;

      case CSSValueID::kImageSet:
        context.Count(WebFeature::kImageSet);
        break;

      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    guard.Release();
  }
  stream.ConsumeWhitespace();

  return image_set;
}

CSSValue* ConsumeImage(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const ConsumeGeneratedImagePolicy generated_image_policy,
    const ConsumeStringUrlImagePolicy string_url_image_policy,
    const ConsumeImageSetImagePolicy image_set_image_policy) {
  CSSParserToken uri = ConsumeUrlAsToken(stream, context);
  if (uri.GetType() != kEOFToken) {
    return CreateCSSImageValueWithReferrer(uri.Value(), context);
  }
  if (string_url_image_policy == ConsumeStringUrlImagePolicy::kAllow) {
    wtf_size_t value_start_offset = stream.LookAheadOffset();
    String uri_string = ConsumeStringAsString(stream);
    if (!uri_string.IsNull()) {
      wtf_size_t value_end_offset = stream.LookAheadOffset();
      if (IsAttrTainted(stream, value_start_offset, value_end_offset)) {
        // https://drafts.csswg.org/css-values-5/#attr-security
        // “Additionally, attr() is not allowed to be used in any <url> value,
        // whether directly or indirectly. Doing so makes the property it’s used
        // in invalid.”
        return nullptr;
      }
      if (IsFetchRestricted(uri_string, context)) {
        uri_string = "";
      }
      return CreateCSSImageValueWithReferrer(uri_string, context);
    }
  }
  if (stream.Peek().GetType() == kFunctionToken) {
    CSSValueID id = stream.Peek().FunctionId();
    if (image_set_image_policy == ConsumeImageSetImagePolicy::kAllow &&
        IsImageSet(id)) {
      return ConsumeImageSet(stream, context, generated_image_policy);
    }
    if (generated_image_policy == ConsumeGeneratedImagePolicy::kAllow &&
        IsGeneratedImage(id)) {
      return ConsumeGeneratedImage(stream, context);
    }
    if (IsUASheetBehavior(context.Mode())) {
      return ConsumeLightDark(
          static_cast<CSSValue* (*)(CSSParserTokenStream&,
                                    const CSSParserContext&)>(
              ConsumeImageOrNone),
          stream, context);
    }
  }
  return nullptr;
}

// https://drafts.csswg.org/css-shapes-1/#typedef-shape-box
CSSIdentifierValue* ConsumeShapeBox(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kContentBox, CSSValueID::kPaddingBox,
                      CSSValueID::kBorderBox, CSSValueID::kMarginBox>(stream);
}

// https://drafts.csswg.org/css-box-4/#typedef-visual-box
CSSIdentifierValue* ConsumeVisualBox(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kContentBox, CSSValueID::kPaddingBox,
                      CSSValueID::kBorderBox>(stream);
}

// https://drafts.csswg.org/css-box-4/#typedef-coord-box
CSSIdentifierValue* ConsumeCoordBox(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kContentBox, CSSValueID::kPaddingBox,
                      CSSValueID::kBorderBox, CSSValueID::kFillBox,
                      CSSValueID::kStrokeBox, CSSValueID::kViewBox>(stream);
}

// https://drafts.fxtf.org/css-masking/#typedef-geometry-box
CSSIdentifierValue* ConsumeGeometryBox(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kBorderBox, CSSValueID::kPaddingBox,
                      CSSValueID::kContentBox, CSSValueID::kMarginBox,
                      CSSValueID::kFillBox, CSSValueID::kStrokeBox,
                      CSSValueID::kViewBox>(stream);
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

CSSValue* ConsumeTransformValue(CSSParserTokenStream& stream,
                                const CSSParserContext& context) {
  bool use_legacy_parsing = false;
  return ConsumeTransformValue(stream, context, use_legacy_parsing);
}

CSSValue* ConsumeTransformList(CSSParserTokenStream& stream,
                               const CSSParserContext& context) {
  return ConsumeTransformList(stream, context, CSSParserLocalContext());
}

CSSValue* ConsumeFilterFunctionList(CSSParserTokenStream& stream,
                                    const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(stream);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSParserSavePoint savepoint(stream);
    CSSValue* filter_value = ConsumeUrl(stream, context);
    if (!filter_value) {
      filter_value = ConsumeFilterFunction(stream, context);
      if (!filter_value) {
        break;
      }
    }
    savepoint.Release();
    list->Append(*filter_value);
  } while (!stream.AtEnd());
  if (list->length() == 0) {
    return nullptr;
  }
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
      // TODO(crbug.com/1426629): Remove warning after shipping.
      if (RuntimeEnabledFeatures::
              NonStandardAppearanceValueSliderVerticalEnabled() &&
          value_id == CSSValueID::kSliderVertical) {
        if (const auto* document = context.GetDocument()) {
          document->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kDeprecation,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "The keyword 'slider-vertical' specified to an 'appearance' "
              "property is not standardized. It will be removed in the future. "
              "Use <input type=range style=\"writing-mode: vertical-lr; "
              "direction: rtl\"> instead."));
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
        } else if (value_id == CSSValueID::kRadio) {
          feature = WebFeature::kCSSValueAppearanceRadio;
        } else if (value_id == CSSValueID::kSearchfield) {
          feature = WebFeature::kCSSValueAppearanceSearchField;
        } else if (value_id == CSSValueID::kSliderVertical) {
          feature = WebFeature::kCSSValueAppearanceSliderVertical;
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
          NOTREACHED_IN_MIGRATION();
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
               NonStandardAppearanceValueSliderVerticalEnabled() &&
           value_id == CSSValueID::kSliderVertical)) {
        if (const auto* document = context.GetDocument()) {
          document->AddConsoleMessage(
              MakeGarbageCollected<ConsoleMessage>(
                  mojom::blink::ConsoleMessageSource::kOther,
                  mojom::blink::ConsoleMessageLevel::kWarning,
                  "The keyword 'slider-vertical' used on the 'appearance' "
                  "property was deprecated and has now been removed. It will "
                  "no longer have any effect."),
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
                              CSSParserTokenStream& stream) {
  CSSPropertyID property_id = ResolveCSSPropertyID(unresolved_property);
  CSSValueID value_id = stream.Peek().Id();
  DCHECK(!CSSProperty::Get(property_id).IsShorthand());
  if (CSSParserFastPaths::IsHandledByKeywordFastPath(property_id)) {
    if (CSSParserFastPaths::IsValidKeywordPropertyAndValue(
            property_id, stream.Peek().Id(), context.Mode())) {
      CountKeywordOnlyPropertyUsage(property_id, context, value_id);
      return ConsumeIdent(stream);
    }
    WarnInvalidKeywordPropertyUsage(property_id, context, value_id);
    return nullptr;
  }

  const auto local_context =
      CSSParserLocalContext()
          .WithAliasParsing(IsPropertyAlias(unresolved_property))
          .WithCurrentShorthand(current_shorthand);

  const CSSValue* result =
      To<Longhand>(CSSProperty::Get(property_id))
          .ParseSingleValue(stream, context, local_context);
  return result;
}

bool ConsumeShorthandVia2Longhands(
    const StylePropertyShorthand& shorthand,
    bool important,
    const CSSParserContext& context,
    CSSParserTokenStream& stream,
    HeapVector<CSSPropertyValue, 64>& properties) {
  const StylePropertyShorthand::Properties& longhands = shorthand.properties();
  DCHECK_EQ(longhands.size(), 2u);

  const CSSValue* start = ParseLonghand(longhands[0]->PropertyID(),
                                        shorthand.id(), context, stream);

  if (!start) {
    return false;
  }

  const CSSValue* end = ParseLonghand(longhands[1]->PropertyID(),
                                      shorthand.id(), context, stream);

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

  return true;
}

bool ConsumeShorthandVia4Longhands(
    const StylePropertyShorthand& shorthand,
    bool important,
    const CSSParserContext& context,
    CSSParserTokenStream& stream,
    HeapVector<CSSPropertyValue, 64>& properties) {
  const StylePropertyShorthand::Properties& longhands = shorthand.properties();
  DCHECK_EQ(longhands.size(), 4u);
  const CSSValue* top = ParseLonghand(longhands[0]->PropertyID(),
                                      shorthand.id(), context, stream);

  if (!top) {
    return false;
  }

  const CSSValue* right = ParseLonghand(longhands[1]->PropertyID(),
                                        shorthand.id(), context, stream);

  const CSSValue* bottom = nullptr;
  const CSSValue* left = nullptr;
  if (right) {
    bottom = ParseLonghand(longhands[2]->PropertyID(), shorthand.id(), context,
                           stream);
    if (bottom) {
      left = ParseLonghand(longhands[3]->PropertyID(), shorthand.id(), context,
                           stream);
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

  return true;
}

bool ConsumeShorthandGreedilyViaLonghands(
    const StylePropertyShorthand& shorthand,
    bool important,
    const CSSParserContext& context,
    CSSParserTokenStream& stream,
    HeapVector<CSSPropertyValue, 64>& properties,
    bool use_initial_value_function) {
  // Existing shorthands have at most 6 longhands.
  DCHECK_LE(shorthand.length(), 6u);
  std::array<const CSSValue*, 6> longhands = {nullptr};
  const StylePropertyShorthand::Properties& shorthand_properties =
      shorthand.properties();
  bool found_any = false;
  bool found_longhand;
  do {
    found_longhand = false;
    for (size_t i = 0; i < shorthand.length(); ++i) {
      if (longhands[i]) {
        continue;
      }
      longhands[i] = ParseLonghand(shorthand_properties[i]->PropertyID(),
                                   shorthand.id(), context, stream);

      if (longhands[i]) {
        found_longhand = true;
        found_any = true;
        break;
      }
    }
  } while (found_longhand && !stream.AtEnd());

  if (!found_any) {
    return false;
  }

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
  const StylePropertyShorthand::Properties& longhands = shorthand.properties();
  DCHECK(longhands.size());
  for (const CSSProperty* const longhand : longhands) {
    AddProperty(longhand->PropertyID(), property, value, important,
                IsImplicitProperty::kNotImplicit, properties);
  }
}

bool IsBaselineKeyword(CSSValueID id) {
  return IdentMatches<CSSValueID::kFirst, CSSValueID::kLast,
                      CSSValueID::kBaseline>(id);
}

bool IsSelfPositionKeyword(CSSValueID id) {
  return IdentMatches<CSSValueID::kStart, CSSValueID::kEnd, CSSValueID::kCenter,
                      CSSValueID::kSelfStart, CSSValueID::kSelfEnd,
                      CSSValueID::kFlexStart, CSSValueID::kFlexEnd,
                      CSSValueID::kAnchorCenter>(id);
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

CSSValue* ConsumeCSSWideKeyword(CSSParserTokenStream& stream) {
  if (!IsCSSWideKeyword(stream.Peek().Id())) {
    return nullptr;
  }
  switch (stream.ConsumeIncludingWhitespace().Id()) {
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
      NOTREACHED_IN_MIGRATION();
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
    CSSParserTokenStream& stream,
    IsPositionKeyword is_position_keyword) {
  DCHECK(is_position_keyword);
  CSSValueID id = stream.Peek().Id();
  if (IsAuto(id) || IsNormalOrStretch(id)) {
    return ConsumeIdent(stream);
  }

  if (CSSValue* baseline = ConsumeBaseline(stream)) {
    return baseline;
  }

  CSSIdentifierValue* overflow_position =
      ConsumeOverflowPositionKeyword(stream);
  if (!is_position_keyword(stream.Peek().Id())) {
    return nullptr;
  }
  CSSIdentifierValue* self_position = ConsumeIdent(stream);
  if (overflow_position) {
    return MakeGarbageCollected<CSSValuePair>(
        overflow_position, self_position, CSSValuePair::kDropIdenticalValues);
  }
  return self_position;
}

CSSValue* ConsumeContentDistributionOverflowPosition(
    CSSParserTokenStream& stream,
    IsPositionKeyword is_position_keyword) {
  DCHECK(is_position_keyword);
  CSSValueID id = stream.Peek().Id();
  if (IdentMatches<CSSValueID::kNormal>(id)) {
    return MakeGarbageCollected<cssvalue::CSSContentDistributionValue>(
        CSSValueID::kInvalid, stream.ConsumeIncludingWhitespace().Id(),
        CSSValueID::kInvalid);
  }

  if (CSSValue* baseline = ConsumeFirstBaseline(stream)) {
    return MakeGarbageCollected<cssvalue::CSSContentDistributionValue>(
        CSSValueID::kInvalid, GetBaselineKeyword(*baseline),
        CSSValueID::kInvalid);
  }

  if (IsContentDistributionKeyword(id)) {
    return MakeGarbageCollected<cssvalue::CSSContentDistributionValue>(
        stream.ConsumeIncludingWhitespace().Id(), CSSValueID::kInvalid,
        CSSValueID::kInvalid);
  }

  CSSParserSavePoint savepoint(stream);
  CSSValueID overflow = IsOverflowKeyword(id)
                            ? stream.ConsumeIncludingWhitespace().Id()
                            : CSSValueID::kInvalid;
  if (is_position_keyword(stream.Peek().Id())) {
    savepoint.Release();
    return MakeGarbageCollected<cssvalue::CSSContentDistributionValue>(
        CSSValueID::kInvalid, stream.ConsumeIncludingWhitespace().Id(),
        overflow);
  }

  return nullptr;
}

CSSValue* ConsumeAnimationIterationCount(CSSParserTokenStream& stream,
                                         const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kInfinite) {
    return ConsumeIdent(stream);
  }
  return ConsumeNumber(stream, context,
                       CSSPrimitiveValue::ValueRange::kNonNegative);
}

CSSValue* ConsumeAnimationName(CSSParserTokenStream& stream,
                               const CSSParserContext& context,
                               bool allow_quoted_name) {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(stream);
  }

  if (allow_quoted_name && stream.Peek().GetType() == kStringToken) {
    // Legacy support for strings in prefixed animations.
    context.Count(WebFeature::kQuotedAnimationName);

    const CSSParserToken& token = stream.ConsumeIncludingWhitespace();
    if (EqualIgnoringASCIICase(token.Value(), "none")) {
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    }
    return MakeGarbageCollected<CSSCustomIdentValue>(
        token.Value().ToAtomicString());
  }

  return ConsumeCustomIdent(stream, context);
}

CSSValue* ConsumeScrollFunction(CSSParserTokenStream& stream,
                                const CSSParserContext& context) {
  if (stream.Peek().FunctionId() != CSSValueID::kScroll) {
    return nullptr;
  }

  CSSValue* scroller = nullptr;
  CSSIdentifierValue* axis = nullptr;

  {
    CSSParserTokenStream::BlockGuard guard(stream);
    stream.ConsumeWhitespace();
    while (!scroller || !axis) {
      if (stream.AtEnd()) {
        break;
      }
      if (!scroller) {
        if ((scroller = ConsumeIdent<CSSValueID::kRoot, CSSValueID::kNearest,
                                     CSSValueID::kSelf>(stream))) {
          continue;
        }
      }
      if (!axis) {
        if ((axis = ConsumeIdent<CSSValueID::kBlock, CSSValueID::kInline,
                                 CSSValueID::kX, CSSValueID::kY>(stream))) {
          continue;
        }
      }
      return nullptr;
    }
    if (!stream.AtEnd()) {
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
  }
  stream.ConsumeWhitespace();
  return MakeGarbageCollected<cssvalue::CSSScrollValue>(scroller, axis);
}

CSSValue* ConsumeViewFunction(CSSParserTokenStream& stream,
                              const CSSParserContext& context) {
  if (stream.Peek().FunctionId() != CSSValueID::kView) {
    return nullptr;
  }

  CSSIdentifierValue* axis = nullptr;
  CSSValue* inset = nullptr;

  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    while (!axis || !inset) {
      if (stream.AtEnd()) {
        break;
      }
      if (!axis) {
        if ((axis = ConsumeIdent<CSSValueID::kBlock, CSSValueID::kInline,
                                 CSSValueID::kX, CSSValueID::kY>(stream))) {
          continue;
        }
      }
      if (!inset) {
        if ((inset = ConsumeSingleTimelineInset(stream, context))) {
          continue;
        }
      }
      return nullptr;
    }

    if (!stream.AtEnd()) {
      return nullptr;
    }
    guard.Release();
  }
  stream.ConsumeWhitespace();

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

CSSValue* ConsumeAnimationTimeline(CSSParserTokenStream& stream,
                                   const CSSParserContext& context) {
  if (auto* value =
          ConsumeIdent<CSSValueID::kNone, CSSValueID::kAuto>(stream)) {
    return value;
  }
  if (auto* value = ConsumeDashedIdent(stream, context)) {
    return value;
  }
  if (auto* value = ConsumeViewFunction(stream, context)) {
    return value;
  }
  return ConsumeScrollFunction(stream, context);
}

CSSValue* ConsumeAnimationTimingFunction(CSSParserTokenStream& stream,
                                         const CSSParserContext& context) {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kEase || id == CSSValueID::kLinear ||
      id == CSSValueID::kEaseIn || id == CSSValueID::kEaseOut ||
      id == CSSValueID::kEaseInOut || id == CSSValueID::kStepStart ||
      id == CSSValueID::kStepEnd) {
    return ConsumeIdent(stream);
  }

  CSSValueID function = stream.Peek().FunctionId();
  if (function == CSSValueID::kLinear) {
    return ConsumeLinear(stream, context);
  }
  if (function == CSSValueID::kSteps) {
    return ConsumeSteps(stream, context);
  }
  if (function == CSSValueID::kCubicBezier) {
    return ConsumeCubicBezier(stream, context);
  }
  return nullptr;
}

CSSValue* ConsumeAnimationDuration(CSSParserTokenStream& stream,
                                   const CSSParserContext& context) {
  if (RuntimeEnabledFeatures::ScrollTimelineEnabled()) {
    if (CSSValue* ident = ConsumeIdent<CSSValueID::kAuto>(stream)) {
      return ident;
    }
  }
  return ConsumeTime(stream, context,
                     CSSPrimitiveValue::ValueRange::kNonNegative);
}

CSSValue* ConsumeTimelineRangeName(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kContain, CSSValueID::kCover,
                      CSSValueID::kEntry, CSSValueID::kEntryCrossing,
                      CSSValueID::kExit, CSSValueID::kExitCrossing>(stream);
}

CSSValue* ConsumeTimelineRangeNameAndPercent(CSSParserTokenStream& stream,
                                             const CSSParserContext& context) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  CSSValue* range_name = ConsumeTimelineRangeName(stream);
  if (!range_name) {
    return nullptr;
  }
  list->Append(*range_name);
  CSSValue* percentage =
      ConsumePercent(stream, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!percentage) {
    return nullptr;
  }
  list->Append(*percentage);
  return list;
}

CSSValue* ConsumeAnimationDelay(CSSParserTokenStream& stream,
                                const CSSParserContext& context) {
  return ConsumeTime(stream, context, CSSPrimitiveValue::ValueRange::kAll);
}

CSSValue* ConsumeAnimationRange(CSSParserTokenStream& stream,
                                const CSSParserContext& context,
                                double default_offset_percent) {
  DCHECK(RuntimeEnabledFeatures::ScrollTimelineEnabled());
  if (CSSValue* ident = ConsumeIdent<CSSValueID::kNormal>(stream)) {
    return ident;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  CSSValue* range_name = ConsumeTimelineRangeName(stream);
  if (range_name) {
    list->Append(*range_name);
  }
  CSSPrimitiveValue* percentage = ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    bool use_legacy_parsing) {
  DCHECK(consumeLonghandItem);
  const unsigned longhand_count = shorthand.length();
  DCHECK_LE(longhand_count, kMaxNumAnimationLonghands);

  for (unsigned i = 0; i < longhand_count; ++i) {
    longhands[i] = CSSValueList::CreateCommaSeparated();
  }

  do {
    std::array<bool, kMaxNumAnimationLonghands> parsed_longhand = {false};
    bool found_any = false;
    do {
      bool found_property = false;
      for (unsigned i = 0; i < longhand_count; ++i) {
        if (parsed_longhand[i]) {
          continue;
        }

        CSSValue* value =
            consumeLonghandItem(shorthand.properties()[i]->PropertyID(), stream,
                                context, use_legacy_parsing);
        if (value) {
          parsed_longhand[i] = true;
          found_property = true;
          found_any = true;
          longhands[i]->Append(*value);
          break;
        }
      }
      if (!found_property) {
        break;
      }
    } while (!stream.AtEnd() && stream.Peek().GetType() != kCommaToken);

    if (!found_any) {
      return false;
    }

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
  } while (ConsumeCommaIncludingWhitespace(stream));

  return true;
}

CSSValue* ConsumeSingleTimelineAxis(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kBlock, CSSValueID::kInline, CSSValueID::kX,
                      CSSValueID::kY>(stream);
}

CSSValue* ConsumeSingleTimelineName(CSSParserTokenStream& stream,
                                    const CSSParserContext& context) {
  if (CSSValue* value = ConsumeIdent<CSSValueID::kNone>(stream)) {
    return value;
  }
  return ConsumeDashedIdent(stream, context);
}

namespace {

CSSValue* ConsumeSingleTimelineInsetSide(CSSParserTokenStream& stream,
                                         const CSSParserContext& context) {
  if (CSSValue* ident = ConsumeIdent<CSSValueID::kAuto>(stream)) {
    return ident;
  }
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kAll);
}

}  // namespace

CSSValue* ConsumeSingleTimelineInset(CSSParserTokenStream& stream,
                                     const CSSParserContext& context) {
  CSSValue* start = ConsumeSingleTimelineInsetSide(stream, context);
  if (!start) {
    return nullptr;
  }
  CSSValue* end = ConsumeSingleTimelineInsetSide(stream, context);
  if (!end) {
    end = start;
  }
  return MakeGarbageCollected<CSSValuePair>(start, end,
                                            CSSValuePair::kDropIdenticalValues);
}

const CSSValue* GetSingleValueOrMakeList(
    CSSValue::ValueListSeparator list_separator,
    HeapVector<Member<const CSSValue>, 4> values) {
  if (values.size() == 1u) {
    return values.front().Get();
  }
  return MakeGarbageCollected<CSSValueList>(list_separator, std::move(values));
}

CSSValue* ConsumeBackgroundAttachment(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kScroll, CSSValueID::kFixed,
                      CSSValueID::kLocal>(stream);
}

CSSValue* ConsumeBackgroundBlendMode(CSSParserTokenStream& stream) {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kNormal || id == CSSValueID::kOverlay ||
      (id >= CSSValueID::kMultiply && id <= CSSValueID::kLuminosity)) {
    return ConsumeIdent(stream);
  }
  return nullptr;
}

CSSValue* ConsumeBackgroundBox(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kBorderBox, CSSValueID::kPaddingBox,
                      CSSValueID::kContentBox>(stream);
}

CSSValue* ConsumeBackgroundBoxOrText(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kBorderBox, CSSValueID::kPaddingBox,
                      CSSValueID::kContentBox, CSSValueID::kText>(stream);
}

CSSValue* ConsumeMaskComposite(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kAdd, CSSValueID::kSubtract,
                      CSSValueID::kIntersect, CSSValueID::kExclude>(stream);
}

CSSValue* ConsumePrefixedMaskComposite(CSSParserTokenStream& stream) {
  return ConsumeIdentRange(stream, CSSValueID::kClear,
                           CSSValueID::kPlusLighter);
}

CSSValue* ConsumeMaskMode(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kAlpha, CSSValueID::kLuminance,
                      CSSValueID::kMatchSource>(stream);
}

CSSPrimitiveValue* ConsumeLengthOrPercentCountNegative(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    std::optional<WebFeature> negative_size) {
  CSSPrimitiveValue* result = ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative,
      UnitlessQuirk::kForbid);
  if (!result && negative_size) {
    context.Count(*negative_size);
  }
  return result;
}

CSSValue* ConsumeBackgroundSize(CSSParserTokenStream& stream,
                                const CSSParserContext& context,
                                std::optional<WebFeature> negative_size,
                                ParsingStyle parsing_style) {
  if (IdentMatches<CSSValueID::kContain, CSSValueID::kCover>(
          stream.Peek().Id())) {
    return ConsumeIdent(stream);
  }

  CSSValue* horizontal = ConsumeIdent<CSSValueID::kAuto>(stream);
  if (!horizontal) {
    horizontal =
        ConsumeLengthOrPercentCountNegative(stream, context, negative_size);
  }
  if (!horizontal) {
    return nullptr;
  }

  CSSValue* vertical = nullptr;
  if (!stream.AtEnd()) {
    if (stream.Peek().Id() == CSSValueID::kAuto) {  // `auto' is the default
      stream.ConsumeIncludingWhitespace();
    } else {
      vertical =
          ConsumeLengthOrPercentCountNegative(stream, context, negative_size);
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

bool ConsumeBackgroundPosition(CSSParserTokenStream& stream,
                               const CSSParserContext& context,
                               UnitlessQuirk unitless,
                               std::optional<WebFeature> three_value_position,
                               const CSSValue*& result_x,
                               const CSSValue*& result_y) {
  HeapVector<Member<const CSSValue>, 4> values_x;
  HeapVector<Member<const CSSValue>, 4> values_y;

  do {
    CSSValue* position_x = nullptr;
    CSSValue* position_y = nullptr;
    if (!ConsumePosition(stream, context, unitless, three_value_position,
                         position_x, position_y)) {
      return false;
    }
    // TODO(crbug.com/825895): So far, 'background-position' is the only
    // property that allows resolving a percentage against a negative value. If
    // we have more of such properties, we should instead pass an additional
    // argument to ask the parser to set this flag.
    SetAllowsNegativePercentageReference(position_x);
    SetAllowsNegativePercentageReference(position_y);
    values_x.push_back(position_x);
    values_y.push_back(position_y);
  } while (ConsumeCommaIncludingWhitespace(stream));

  // To conserve memory we don't wrap single values in lists.
  result_x =
      GetSingleValueOrMakeList(CSSValue::kCommaSeparator, std::move(values_x));
  result_y =
      GetSingleValueOrMakeList(CSSValue::kCommaSeparator, std::move(values_y));

  return true;
}

CSSValue* ConsumePrefixedBackgroundBox(CSSParserTokenStream& stream,
                                       AllowTextValue allow_text_value) {
  // The values 'border', 'padding' and 'content' are deprecated and do not
  // apply to the version of the property that has the -webkit- prefix removed.
  if (CSSValue* value = ConsumeIdentRange(stream, CSSValueID::kBorder,
                                          CSSValueID::kPaddingBox)) {
    return value;
  }
  if (allow_text_value == AllowTextValue::kAllow &&
      stream.Peek().Id() == CSSValueID::kText) {
    return ConsumeIdent(stream);
  }
  return nullptr;
}

CSSValue* ParseBackgroundBox(CSSParserTokenStream& stream,
                             const CSSParserLocalContext& local_context,
                             AllowTextValue alias_allow_text_value) {
  // This is legacy behavior that does not match spec, see crbug.com/604023
  if (local_context.UseAliasParsing()) {
    return ConsumeCommaSeparatedList(ConsumePrefixedBackgroundBox, stream,
                                     alias_allow_text_value);
  }
  return ConsumeCommaSeparatedList(ConsumeBackgroundBox, stream);
}

CSSValue* ParseBackgroundSize(CSSParserTokenStream& stream,
                              const CSSParserContext& context,
                              const CSSParserLocalContext& local_context,
                              std::optional<WebFeature> negative_size) {
  return ConsumeCommaSeparatedList(
      static_cast<CSSValue* (*)(CSSParserTokenStream&, const CSSParserContext&,
                                std::optional<WebFeature>, ParsingStyle)>(
          ConsumeBackgroundSize),
      stream, context, negative_size,
      local_context.UseAliasParsing() ? ParsingStyle::kLegacy
                                      : ParsingStyle::kNotLegacy);
}

CSSValue* ParseMaskSize(CSSParserTokenStream& stream,
                        const CSSParserContext& context,
                        const CSSParserLocalContext& local_context,
                        std::optional<WebFeature> negative_size) {
  return ConsumeCommaSeparatedList(
      static_cast<CSSValue* (*)(CSSParserTokenStream&, const CSSParserContext&,
                                std::optional<WebFeature>, ParsingStyle)>(
          ConsumeBackgroundSize),
      stream, context, negative_size, ParsingStyle::kNotLegacy);
}

CSSValue* ConsumeCoordBoxOrNoClip(CSSParserTokenStream& stream) {
  if (stream.Peek().Id() == CSSValueID::kNoClip) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeCoordBox(stream);
}

namespace {

CSSValue* ConsumeBackgroundComponent(CSSPropertyID resolved_property,
                                     CSSParserTokenStream& stream,
                                     const CSSParserContext& context,
                                     bool use_alias_parsing) {
  switch (resolved_property) {
    case CSSPropertyID::kBackgroundClip:
      if (RuntimeEnabledFeatures::CSSBackgroundClipUnprefixEnabled()) {
        return ConsumeBackgroundBoxOrText(stream);
      } else {
        return ConsumeBackgroundBox(stream);
      }
    case CSSPropertyID::kBackgroundAttachment:
      return ConsumeBackgroundAttachment(stream);
    case CSSPropertyID::kBackgroundOrigin:
      return ConsumeBackgroundBox(stream);
    case CSSPropertyID::kBackgroundImage:
    case CSSPropertyID::kMaskImage:
      return ConsumeImageOrNone(stream, context);
    case CSSPropertyID::kBackgroundPositionX:
    case CSSPropertyID::kWebkitMaskPositionX:
      return ConsumePositionLonghand<CSSValueID::kLeft, CSSValueID::kRight>(
          stream, context);
    case CSSPropertyID::kBackgroundPositionY:
    case CSSPropertyID::kWebkitMaskPositionY:
      return ConsumePositionLonghand<CSSValueID::kTop, CSSValueID::kBottom>(
          stream, context);
    case CSSPropertyID::kBackgroundSize:
      return ConsumeBackgroundSize(stream, context,
                                   WebFeature::kNegativeBackgroundSize,
                                   ParsingStyle::kNotLegacy);
    case CSSPropertyID::kMaskSize:
      return ConsumeBackgroundSize(stream, context,
                                   WebFeature::kNegativeMaskSize,
                                   ParsingStyle::kNotLegacy);
    case CSSPropertyID::kBackgroundColor:
      return ConsumeColor(stream, context);
    case CSSPropertyID::kMaskClip:
      return use_alias_parsing
                 ? ConsumePrefixedBackgroundBox(stream, AllowTextValue::kAllow)
                 : ConsumeCoordBoxOrNoClip(stream);
    case CSSPropertyID::kMaskOrigin:
      return use_alias_parsing
                 ? ConsumePrefixedBackgroundBox(stream, AllowTextValue::kForbid)
                 : ConsumeCoordBox(stream);
    case CSSPropertyID::kBackgroundRepeat:
    case CSSPropertyID::kMaskRepeat:
      return ConsumeRepeatStyleValue(stream);
    case CSSPropertyID::kMaskComposite:
      return ConsumeMaskComposite(stream);
    case CSSPropertyID::kMaskMode:
      return ConsumeMaskMode(stream);
    default:
      return nullptr;
  };
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
                           CSSParserTokenStream& stream,
                           const CSSParserContext& context,
                           const CSSParserLocalContext& local_context,
                           HeapVector<CSSPropertyValue, 64>& properties) {
  CSSPropertyID shorthand_id = local_context.CurrentShorthand();
  DCHECK(shorthand_id == CSSPropertyID::kBackground ||
         shorthand_id == CSSPropertyID::kMask);
  const StylePropertyShorthand& shorthand =
      shorthand_id == CSSPropertyID::kBackground ? backgroundShorthand()
                                                 : maskShorthand();

  const unsigned longhand_count = shorthand.length();
  std::array<HeapVector<Member<const CSSValue>, 4>, 10> longhands;
  CHECK_LE(longhand_count, 10u);

  bool implicit = false;
  bool previous_layer_had_background_color = false;
  do {
    std::array<bool, 10> parsed_longhand = {false};
    CSSValue* origin_value = nullptr;
    bool found_property;
    bool found_any = false;
    do {
      found_property = false;
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
          if (!ConsumePosition(stream, context, UnitlessQuirk::kForbid,
                               WebFeature::kThreeValuedPositionBackground,
                               value, value_y)) {
            continue;
          }
          if (value) {
            bg_position_parsed_in_current_layer = true;
          }
        } else if (property.IDEquals(CSSPropertyID::kBackgroundSize) ||
                   property.IDEquals(CSSPropertyID::kMaskSize)) {
          if (!ConsumeSlashIncludingWhitespace(stream)) {
            continue;
          }
          value = ConsumeBackgroundSize(
              stream, context,
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
              ConsumeBackgroundComponent(property.PropertyID(), stream, context,
                                         local_context.UseAliasParsing());
        }
        if (value) {
          if (property.IDEquals(CSSPropertyID::kBackgroundOrigin) ||
              property.IDEquals(CSSPropertyID::kMaskOrigin)) {
            origin_value = value;
          }
          parsed_longhand[i] = true;
          found_property = true;
          found_any = true;
          longhands[i].push_back(value);
          if (value_y) {
            parsed_longhand[i + 1] = true;
            longhands[i + 1].push_back(value_y);
          }
        }
      }
    } while (found_property && !stream.AtEnd() &&
             stream.Peek().GetType() != kCommaToken);

    if (!found_any) {
      return false;
    }
    if (previous_layer_had_background_color) {
      // Colors are only allowed in the last layer; previous layer had
      // a background color and we now know for sure it was not the last one,
      // so return parse failure.
      return false;
    }

    // TODO(timloh): This will make invalid longhands, see crbug.com/386459
    for (unsigned i = 0; i < longhand_count; ++i) {
      const CSSProperty& property = *shorthand.properties()[i];

      if (property.IDEquals(CSSPropertyID::kBackgroundColor)) {
        if (parsed_longhand[i]) {
          previous_layer_had_background_color = true;
        }
      }
      if (!parsed_longhand[i]) {
        if ((property.IDEquals(CSSPropertyID::kBackgroundClip) ||
             property.IDEquals(CSSPropertyID::kMaskClip)) &&
            origin_value) {
          longhands[i].push_back(origin_value);
          continue;
        }

        if (shorthand_id == CSSPropertyID::kMask) {
          longhands[i].push_back(To<Longhand>(property).InitialValue());
        } else {
          longhands[i].push_back(CSSInitialValue::Create());
        }
      }
    }
  } while (ConsumeCommaIncludingWhitespace(stream));

  for (unsigned i = 0; i < longhand_count; ++i) {
    const CSSProperty& property = *shorthand.properties()[i];

    const CSSValue* longhand;
    if (property.IDEquals(CSSPropertyID::kBackgroundColor)) {
      // There can only be one background-color (we've verified this earlier,
      // by means of previous_layer_had_background_color), so pick out only
      // the last one (any others will just be “initial” over and over again).
      longhand = longhands[i].back().Get();
    } else {
      // To conserve memory we don't wrap a single value in a list.
      longhand = GetSingleValueOrMakeList(CSSValue::kCommaSeparator,
                                          std::move(longhands[i]));
    }

    AddProperty(property.PropertyID(), shorthand.id(), *longhand, important,
                implicit ? IsImplicitProperty::kImplicit
                         : IsImplicitProperty::kNotImplicit,
                properties);
  }
  return true;
}

CSSIdentifierValue* ConsumeRepeatStyleIdent(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kRepeat, CSSValueID::kNoRepeat,
                      CSSValueID::kRound, CSSValueID::kSpace>(stream);
}

CSSRepeatStyleValue* ConsumeRepeatStyleValue(CSSParserTokenStream& stream) {
  if (auto* id = ConsumeIdent<CSSValueID::kRepeatX>(stream)) {
    return MakeGarbageCollected<CSSRepeatStyleValue>(id);
  }

  if (auto* id = ConsumeIdent<CSSValueID::kRepeatY>(stream)) {
    return MakeGarbageCollected<CSSRepeatStyleValue>(id);
  }

  if (auto* id1 = ConsumeRepeatStyleIdent(stream)) {
    if (auto* id2 = ConsumeRepeatStyleIdent(stream)) {
      return MakeGarbageCollected<CSSRepeatStyleValue>(id1, id2);
    }

    return MakeGarbageCollected<CSSRepeatStyleValue>(id1);
  }

  return nullptr;
}

CSSValueList* ParseRepeatStyle(CSSParserTokenStream& stream) {
  return ConsumeCommaSeparatedList(ConsumeRepeatStyleValue, stream);
}

CSSValue* ConsumeWebkitBorderImage(CSSParserTokenStream& stream,
                                   const CSSParserContext& context) {
  CSSValue* source = nullptr;
  CSSValue* slice = nullptr;
  CSSValue* width = nullptr;
  CSSValue* outset = nullptr;
  CSSValue* repeat = nullptr;
  if (ConsumeBorderImageComponents(stream, context, source, slice, width,
                                   outset, repeat, DefaultFill::kFill)) {
    return CreateBorderImageValue(source, slice, width, outset, repeat);
  }
  return nullptr;
}

bool ConsumeBorderImageComponents(CSSParserTokenStream& stream,
                                  const CSSParserContext& context,
                                  CSSValue*& source,
                                  CSSValue*& slice,
                                  CSSValue*& width,
                                  CSSValue*& outset,
                                  CSSValue*& repeat,
                                  DefaultFill default_fill) {
  do {
    if (!source) {
      source = ConsumeImageOrNone(stream, context);
      if (source) {
        continue;
      }
    }
    if (!repeat) {
      repeat = ConsumeBorderImageRepeat(stream);
      if (repeat) {
        continue;
      }
    }
    if (!slice) {
      CSSParserSavePoint savepoint(stream);
      slice = ConsumeBorderImageSlice(stream, context, default_fill);
      if (slice) {
        DCHECK(!width);
        DCHECK(!outset);
        if (ConsumeSlashIncludingWhitespace(stream)) {
          width = ConsumeBorderImageWidth(stream, context);
          if (ConsumeSlashIncludingWhitespace(stream)) {
            outset = ConsumeBorderImageOutset(stream, context);
            if (!outset) {
              break;
            }
          } else if (!width) {
            break;
          }
        }
      } else {
        break;
      }
      savepoint.Release();
    } else {
      break;
    }
  } while (!stream.AtEnd());
  if (!source && !repeat && !slice) {
    return false;
  }
  return true;
}

CSSValue* ConsumeBorderImageRepeat(CSSParserTokenStream& stream) {
  CSSIdentifierValue* horizontal = ConsumeBorderImageRepeatKeyword(stream);
  if (!horizontal) {
    return nullptr;
  }
  CSSIdentifierValue* vertical = ConsumeBorderImageRepeatKeyword(stream);
  if (!vertical) {
    vertical = horizontal;
  }
  return MakeGarbageCollected<CSSValuePair>(horizontal, vertical,
                                            CSSValuePair::kDropIdenticalValues);
}

CSSValue* ConsumeBorderImageSlice(CSSParserTokenStream& stream,
                                  const CSSParserContext& context,
                                  DefaultFill default_fill) {
  bool fill = ConsumeIdent<CSSValueID::kFill>(stream);
  std::array<CSSValue*, 4> slices = {nullptr};

  for (size_t index = 0; index < 4; ++index) {
    CSSPrimitiveValue* value = ConsumePercent(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!value) {
      value = ConsumeNumber(stream, context,
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
  if (ConsumeIdent<CSSValueID::kFill>(stream)) {
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

CSSValue* ConsumeBorderImageWidth(CSSParserTokenStream& stream,
                                  const CSSParserContext& context) {
  std::array<CSSValue*, 4> widths = {nullptr};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value = ConsumeNumber(stream, context,
                          CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!value) {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      value = ConsumeLengthOrPercent(
          stream, context, CSSPrimitiveValue::ValueRange::kNonNegative,
          UnitlessQuirk::kForbid);
    }
    if (!value) {
      value = ConsumeIdent<CSSValueID::kAuto>(stream);
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

CSSValue* ConsumeBorderImageOutset(CSSParserTokenStream& stream,
                                   const CSSParserContext& context) {
  std::array<CSSValue*, 4> outsets = {nullptr};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value = ConsumeNumber(stream, context,
                          CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!value) {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      value = ConsumeLength(stream, context,
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

CSSValue* ParseBorderRadiusCorner(CSSParserTokenStream& stream,
                                  const CSSParserContext& context) {
  CSSValue* parsed_value1 = ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!parsed_value1) {
    return nullptr;
  }
  CSSValue* parsed_value2 = ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!parsed_value2) {
    parsed_value2 = parsed_value1;
  }
  return MakeGarbageCollected<CSSValuePair>(parsed_value1, parsed_value2,
                                            CSSValuePair::kDropIdenticalValues);
}

CSSValue* ParseBorderWidthSide(CSSParserTokenStream& stream,
                               const CSSParserContext& context,
                               const CSSParserLocalContext& local_context) {
  CSSPropertyID shorthand = local_context.CurrentShorthand();
  bool allow_quirky_lengths = IsQuirksModeBehavior(context.Mode()) &&
                              (shorthand == CSSPropertyID::kInvalid ||
                               shorthand == CSSPropertyID::kBorderWidth);
  UnitlessQuirk unitless =
      allow_quirky_lengths ? UnitlessQuirk::kAllow : UnitlessQuirk::kForbid;
  return ConsumeBorderWidth(stream, context, unitless);
}

const CSSValue* ParseBorderStyleSide(CSSParserTokenStream& stream,
                                     const CSSParserContext& context) {
  if (RuntimeEnabledFeatures::CustomizableSelectEnabled() &&
      IsUASheetBehavior(context.Mode()) &&
      stream.Peek().FunctionId() ==
          CSSValueID::kInternalAppearanceAutoBaseSelect) {
    CSSValue* auto_value;
    CSSValue* base_select_value;
    {
      CSSParserTokenStream::RestoringBlockGuard guard(stream);
      stream.ConsumeWhitespace();
      auto_value = ConsumeIdent(stream);
      if (!auto_value || !ConsumeCommaIncludingWhitespace(stream)) {
        return nullptr;
      }
      base_select_value = ConsumeIdent(stream);
      if (!base_select_value || !stream.AtEnd()) {
        return nullptr;
      }
      guard.Release();
    }
    stream.ConsumeWhitespace();
    return MakeGarbageCollected<CSSAppearanceAutoBaseSelectValuePair>(
        auto_value, base_select_value);
  }
  return ParseLonghand(CSSPropertyID::kBorderLeftStyle, CSSPropertyID::kBorder,
                       context, stream);
}

cssvalue::CSSRepeatValue* ConsumeGapColorRepeatFunction(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  DCHECK_EQ(stream.Peek().GetType(), kFunctionToken);
  CSSParserTokenStream::RestoringBlockGuard guard(stream);

  stream.ConsumeWhitespace();

  CSSPrimitiveValue* repetition_value = nullptr;
  CSSValueList* repeated_colors = CSSValueList::CreateSpaceSeparated();

  if (IdentMatches<CSSValueID::kAuto>(stream.Peek().Id())) {
    CHECK(stream.ConsumeIncludingWhitespace().Id() == CSSValueID::kAuto);
  } else {
    // Means it's an integer repetition.
    repetition_value = ConsumeIntegerOrNumberCalc(
        stream, context, CSSPrimitiveValue::ValueRange::kPositiveInteger);
    if (!repetition_value) {
      return nullptr;
    }
  }

  // After auto/integer we expect a comma.
  if (!ConsumeCommaIncludingWhitespace(stream)) {
    return nullptr;
  }

  while (!stream.AtEnd()) {
    CSSValue* color = ConsumeColor(stream, context);
    if (!color) {
      return nullptr;
    }
    repeated_colors->Append(*color);
  }

  if (repeated_colors->length() == 0) {
    return nullptr;
  }

  guard.Release();
  stream.ConsumeWhitespace();

  return MakeGarbageCollected<cssvalue::CSSRepeatValue>(repetition_value,
                                                        *repeated_colors);
}

CSSValue* ConsumeGapDecorationColorList(CSSParserTokenStream& stream,
                                        const CSSParserContext& context) {
  // Consume single color if the Gap decoration feature flag is not
  // enabled.
  if (!RuntimeEnabledFeatures::CSSGapDecorationEnabled()) {
    return ConsumeColor(stream, context);
  }

  // List to holds all the color values.
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();

  if (stream.AtEnd()) {
    return nullptr;
  }

  // Flag to limit to one auto-repeat.
  bool seen_auto_repeat = false;

  // Consume the color values(s) in the stream.
  do {
    if (stream.Peek().FunctionId() == CSSValueID::kRepeat) {
      auto* repeat_value = ConsumeGapColorRepeatFunction(stream, context);
      if (!repeat_value) {
        return nullptr;
      }
      if (repeat_value->IsAutoRepeatValue()) {
        if (seen_auto_repeat) {
          return nullptr;
        }
        seen_auto_repeat = true;
      }
      values->Append(*repeat_value);
    } else {
      CSSValue* color = ConsumeColor(stream, context);
      if (!color) {
        break;
      }
      values->Append(*color);
    }
  } while (!stream.AtEnd());

  return values;
}

CSSValue* ConsumeShadow(CSSParserTokenStream& stream,
                        const CSSParserContext& context,
                        AllowInsetAndSpread inset_and_spread) {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(stream);
  }
  return ConsumeCommaSeparatedList(ParseSingleShadow, stream, context,
                                   inset_and_spread);
}

CSSShadowValue* ParseSingleShadow(CSSParserTokenStream& stream,
                                  const CSSParserContext& context,
                                  AllowInsetAndSpread inset_and_spread) {
  CSSIdentifierValue* style = nullptr;
  CSSValue* color = nullptr;

  if (stream.AtEnd()) {
    return nullptr;
  }

  color = ConsumeColor(stream, context);
  if (stream.Peek().Id() == CSSValueID::kInset) {
    if (inset_and_spread != AllowInsetAndSpread::kAllow) {
      return nullptr;
    }
    style = ConsumeIdent(stream);
    if (!color) {
      color = ConsumeColor(stream, context);
    }
  }

  CSSPrimitiveValue* horizontal_offset =
      ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!horizontal_offset) {
    return nullptr;
  }

  CSSPrimitiveValue* vertical_offset =
      ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!vertical_offset) {
    return nullptr;
  }

  CSSPrimitiveValue* blur_radius = ConsumeLength(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  CSSPrimitiveValue* spread_distance = nullptr;
  if (blur_radius) {
    if (inset_and_spread == AllowInsetAndSpread::kAllow) {
      spread_distance =
          ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll);
    }
  }

  if (!stream.AtEnd()) {
    if (!color) {
      color = ConsumeColor(stream, context);
    }
    if (stream.Peek().Id() == CSSValueID::kInset) {
      if (inset_and_spread != AllowInsetAndSpread::kAllow || style) {
        return nullptr;
      }
      style = ConsumeIdent(stream);
      if (!color) {
        color = ConsumeColor(stream, context);
      }
    }
  }
  return MakeGarbageCollected<CSSShadowValue>(horizontal_offset,
                                              vertical_offset, blur_radius,
                                              spread_distance, style, color);
}

CSSValue* ConsumeColumnCount(CSSParserTokenStream& stream,
                             const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return ConsumeIdent(stream);
  }
  return ConsumePositiveInteger(stream, context);
}

CSSValue* ConsumeColumnWidth(CSSParserTokenStream& stream,
                             const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return ConsumeIdent(stream);
  }
  // Always parse lengths in strict mode here, since it would be ambiguous
  // otherwise when used in the 'columns' shorthand property.
  CSSParserContext::ParserModeOverridingScope scope(context, kHTMLStandardMode);
  CSSPrimitiveValue* column_width = ConsumeLength(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!column_width) {
    return nullptr;
  }
  return column_width;
}

bool ConsumeColumnWidthOrCount(CSSParserTokenStream& stream,
                               const CSSParserContext& context,
                               CSSValue*& column_width,
                               CSSValue*& column_count) {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    ConsumeIdent(stream);
    return true;
  }
  if (!column_width) {
    column_width = ConsumeColumnWidth(stream, context);
    if (column_width) {
      return true;
    }
  }
  if (!column_count) {
    column_count = ConsumeColumnCount(stream, context);
  }
  return column_count;
}

CSSValue* ConsumeGapLength(CSSParserTokenStream& stream,
                           const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kNormal) {
    return ConsumeIdent(stream);
  }
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative);
}

CSSValue* ConsumeCounter(CSSParserTokenStream& stream,
                         const CSSParserContext& context,
                         int default_value) {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(stream);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSCustomIdentValue* counter_name = ConsumeCustomIdent(stream, context);
    if (!counter_name) {
      break;
    }
    CSSPrimitiveValue* value = CSSNumericLiteralValue::Create(
        default_value, CSSPrimitiveValue::UnitType::kInteger);
    if (CSSPrimitiveValue* counter_value = ConsumeInteger(stream, context)) {
      value = counter_value;
    }
    list->Append(*MakeGarbageCollected<CSSValuePair>(
        counter_name, value, CSSValuePair::kDropIdenticalValues));
  } while (!stream.AtEnd());
  if (list->length() == 0) {
    return nullptr;
  }
  return list;
}

CSSValue* ConsumeMathDepth(CSSParserTokenStream& stream,
                           const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kAutoAdd) {
    return ConsumeIdent(stream);
  }

  if (CSSPrimitiveValue* integer_value = ConsumeInteger(stream, context)) {
    return integer_value;
  }

  CSSValueID function_id = stream.Peek().FunctionId();
  if (function_id == CSSValueID::kAdd) {
    CSSValue* value;
    bool at_end;
    {
      CSSParserTokenStream::BlockGuard guard(stream);
      stream.ConsumeWhitespace();
      value = ConsumeInteger(stream, context);
      at_end = stream.AtEnd();
    }
    stream.ConsumeWhitespace();
    if (value && at_end) {
      auto* add_value = MakeGarbageCollected<CSSFunctionValue>(function_id);
      add_value->Append(*value);
      return add_value;
    }
  }

  return nullptr;
}

CSSValue* ConsumeFontSize(CSSParserTokenStream& stream,
                          const CSSParserContext& context,
                          UnitlessQuirk unitless) {
  if (stream.Peek().Id() == CSSValueID::kWebkitXxxLarge) {
    context.Count(WebFeature::kFontSizeWebkitXxxLarge);
  }
  if ((stream.Peek().Id() >= CSSValueID::kXxSmall &&
       stream.Peek().Id() <= CSSValueID::kWebkitXxxLarge) ||
      stream.Peek().Id() == CSSValueID::kMath) {
    return ConsumeIdent(stream);
  }
  return ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative, unitless);
}

CSSValue* ConsumeLineHeight(CSSParserTokenStream& stream,
                            const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kNormal) {
    return ConsumeIdent(stream);
  }

  CSSPrimitiveValue* line_height = ConsumeNumber(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (line_height) {
    return line_height;
  }
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative);
}

CSSValue* ConsumePaletteMixFunction(CSSParserTokenStream& stream,
                                    const CSSParserContext& context) {
  // Grammar proposal in: https://github.com/w3c/csswg-drafts/issues/8922
  //
  // palette-mix() = palette-mix(<color-interpolation-method> , [ [normal |
  // light | dark | <palette-identifier> | <palette-mix()>] && <percentage
  // [0,100]>? ]#{2})
  if (stream.Peek().FunctionId() != CSSValueID::kPaletteMix) {
    return nullptr;
  }

  Color::HueInterpolationMethod hue_interpolation_method =
      Color::HueInterpolationMethod::kShorter;
  Color::ColorSpace color_space;
  CSSValue* palette1;
  CSSValue* palette2;
  CSSPrimitiveValue* percentage1;
  CSSPrimitiveValue* percentage2;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    if (!ConsumeColorInterpolationSpace(stream, color_space,
                                        hue_interpolation_method)) {
      return nullptr;
    }

    auto consume_endpoint_palette_with_percentage =
        [](CSSParserTokenStream& stream, const CSSParserContext& context)
        -> std::pair<CSSValue*, CSSPrimitiveValue*> {
      if (!ConsumeCommaIncludingWhitespace(stream)) {
        return std::make_pair(nullptr, nullptr);
      }

      CSSValue* palette = ConsumeFontPalette(stream, context);
      CSSPrimitiveValue* percentage =
          ConsumePercent(stream, context, CSSPrimitiveValue::ValueRange::kAll);
      // Percentage can be followed by a palette.
      if (!palette) {
        palette = ConsumeFontPalette(stream, context);
        if (!palette) {
          return std::make_pair(nullptr, nullptr);
        }
      }
      // Reject negative values and values > 100%, but not calc() values.
      if (percentage && percentage->IsNumericLiteralValue() &&
          (To<CSSNumericLiteralValue>(percentage)->ComputePercentage() < 0.0 ||
           To<CSSNumericLiteralValue>(percentage)->ComputePercentage() >
               100.0)) {
        return std::make_pair(nullptr, nullptr);
      }
      return std::make_pair(palette, percentage);
    };

    auto palette_with_percentage_1 =
        consume_endpoint_palette_with_percentage(stream, context);
    auto palette_with_percentage_2 =
        consume_endpoint_palette_with_percentage(stream, context);
    palette1 = palette_with_percentage_1.first;
    palette2 = palette_with_percentage_2.first;
    percentage1 = palette_with_percentage_1.second;
    percentage2 = palette_with_percentage_2.second;

    if (!palette1 || !palette2) {
      return nullptr;
    }
    // If both values are literally zero (and not calc()) reject at parse time.
    if (percentage1 && percentage2 && percentage1->IsNumericLiteralValue() &&
        To<CSSNumericLiteralValue>(percentage1)->ComputePercentage() == 0.0f &&
        percentage2->IsNumericLiteralValue() &&
        To<CSSNumericLiteralValue>(percentage2)->ComputePercentage() == 0.0) {
      return nullptr;
    }

    if (!stream.AtEnd()) {
      return nullptr;
    }

    guard.Release();
  }
  stream.ConsumeWhitespace();

  return MakeGarbageCollected<cssvalue::CSSPaletteMixValue>(
      palette1, palette2, percentage1, percentage2, color_space,
      hue_interpolation_method);
}

CSSValue* ConsumeFontPalette(CSSParserTokenStream& stream,
                             const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kNormal ||
      stream.Peek().Id() == CSSValueID::kLight ||
      stream.Peek().Id() == CSSValueID::kDark) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  if (stream.Peek().FunctionId() == CSSValueID::kPaletteMix) {
    return ConsumePaletteMixFunction(stream, context);
  }

  return ConsumeDashedIdent(stream, context);
}

CSSValueList* ConsumeFontFamily(CSSParserTokenStream& stream) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* parsed_value = ConsumeGenericFamily(stream);
    if (parsed_value) {
      list->Append(*parsed_value);
    } else {
      parsed_value = ConsumeFamilyName(stream);
      if (parsed_value) {
        list->Append(*parsed_value);
      } else {
        return nullptr;
      }
    }
  } while (ConsumeCommaIncludingWhitespace(stream));
  return list;
}

CSSValueList* ConsumeNonGenericFamilyNameList(CSSParserTokenStream& stream) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* parsed_value = ConsumeGenericFamily(stream);
    // Consume only if all families in the list are regular family names and
    // none of them are generic ones.
    if (parsed_value) {
      return nullptr;
    }
    parsed_value = ConsumeFamilyName(stream);
    if (parsed_value) {
      list->Append(*parsed_value);
    } else {
      return nullptr;
    }
  } while (ConsumeCommaIncludingWhitespace(stream));
  return list;
}

CSSValue* ConsumeGenericFamily(CSSParserTokenStream& stream) {
  return ConsumeIdentRange(stream, CSSValueID::kSerif, CSSValueID::kMath);
}

CSSValue* ConsumeFamilyName(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kStringToken) {
    return CSSFontFamilyValue::Create(
        stream.ConsumeIncludingWhitespace().Value().ToAtomicString());
  }
  if (stream.Peek().GetType() != kIdentToken) {
    return nullptr;
  }
  String family_name = ConcatenateFamilyName(stream);
  if (family_name.IsNull()) {
    return nullptr;
  }
  return CSSFontFamilyValue::Create(AtomicString(family_name));
}

String ConcatenateFamilyName(CSSParserTokenStream& stream) {
  StringBuilder builder;
  bool added_space = false;
  const CSSParserToken first_token = stream.Peek();
  while (stream.Peek().GetType() == kIdentToken) {
    if (!builder.empty()) {
      builder.Append(' ');
      added_space = true;
    }
    builder.Append(stream.ConsumeIncludingWhitespace().Value());
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
  auto* numeric_angle = DynamicTo<CSSNumericLiteralValue>(angle);
  if (!numeric_angle) {
    // Can't resolve math function here without length resolver.
    return true;
  }
  return numeric_angle->DoubleValue() >= -kMaxAngle &&
         numeric_angle->DoubleValue() <= kMaxAngle;
}

CSSValue* ConsumeFontStyle(CSSParserTokenStream& stream,
                           const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kNormal ||
      stream.Peek().Id() == CSSValueID::kItalic) {
    return ConsumeIdent(stream);
  }

  if (stream.Peek().Id() == CSSValueID::kAuto &&
      context.Mode() == kCSSFontFaceRuleMode) {
    return ConsumeIdent(stream);
  }

  if (stream.Peek().Id() != CSSValueID::kOblique) {
    return nullptr;
  }

  CSSIdentifierValue* oblique_identifier =
      ConsumeIdent<CSSValueID::kOblique>(stream);

  CSSPrimitiveValue* start_angle = ConsumeAngle(
      stream, context, std::nullopt, kMinObliqueValue, kMaxObliqueValue);
  if (!start_angle) {
    return oblique_identifier;
  }
  if (!IsAngleWithinLimits(start_angle)) {
    return nullptr;
  }

  if (context.Mode() != kCSSFontFaceRuleMode || stream.AtEnd()) {
    CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
    value_list->Append(*start_angle);
    return MakeGarbageCollected<cssvalue::CSSFontStyleRangeValue>(
        *oblique_identifier, *value_list);
  }

  CSSPrimitiveValue* end_angle = ConsumeAngle(
      stream, context, std::nullopt, kMinObliqueValue, kMaxObliqueValue);
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  const CSSParserToken& token = stream.Peek();
  if (token.Id() == CSSValueID::kNormal ||
      (token.Id() >= CSSValueID::kUltraCondensed &&
       token.Id() <= CSSValueID::kUltraExpanded)) {
    return ConsumeIdent(stream);
  }
  if (token.Id() == CSSValueID::kAuto &&
      context.Mode() == kCSSFontFaceRuleMode) {
    return ConsumeIdent(stream);
  }
  return nullptr;
}

CSSValue* ConsumeFontStretch(CSSParserTokenStream& stream,
                             const CSSParserContext& context) {
  CSSIdentifierValue* parsed_keyword =
      ConsumeFontStretchKeywordOnly(stream, context);
  if (parsed_keyword) {
    return parsed_keyword;
  }

  CSSPrimitiveValue* start_percent = ConsumePercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!start_percent) {
    return nullptr;
  }

  // In a non-font-face context, more than one percentage is not allowed.
  if (context.Mode() != kCSSFontFaceRuleMode || stream.AtEnd()) {
    return start_percent;
  }

  CSSPrimitiveValue* end_percent = ConsumePercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!end_percent) {
    return nullptr;
  }

  return CombineToRangeList(start_percent, end_percent);
}

CSSValue* ConsumeFontWeight(CSSParserTokenStream& stream,
                            const CSSParserContext& context) {
  const CSSParserToken& token = stream.Peek();
  if (context.Mode() != kCSSFontFaceRuleMode) {
    if (token.Id() >= CSSValueID::kNormal &&
        token.Id() <= CSSValueID::kLighter) {
      return ConsumeIdent(stream);
    }
  } else {
    if (token.Id() == CSSValueID::kNormal || token.Id() == CSSValueID::kBold ||
        token.Id() == CSSValueID::kAuto) {
      return ConsumeIdent(stream);
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
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  auto* numeric_start_weight = DynamicTo<CSSNumericLiteralValue>(start_weight);
  if (!start_weight ||
      (numeric_start_weight && (numeric_start_weight->DoubleValue() < 1 ||
                                numeric_start_weight->DoubleValue() > 1000))) {
    return nullptr;
  }

  // In a non-font-face context, more than one number is not allowed. Return
  // what we have. If there is trailing garbage, the AtEnd() check in
  // CSSPropertyParser::ParseValueStart will catch that.
  if (context.Mode() != kCSSFontFaceRuleMode || stream.AtEnd()) {
    return start_weight;
  }

  CSSPrimitiveValue* end_weight = ConsumeNumber(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  auto* numeric_end_weight = DynamicTo<CSSNumericLiteralValue>(end_weight);
  if (!end_weight ||
      (numeric_end_weight && (numeric_end_weight->DoubleValue() < 1 ||
                              numeric_end_weight->DoubleValue() > 1000))) {
    return nullptr;
  }

  return CombineToRangeList(start_weight, end_weight);
}

CSSValue* ConsumeFontFeatureSettings(CSSParserTokenStream& stream,
                                     const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kNormal) {
    return ConsumeIdent(stream);
  }
  CSSValueList* settings = CSSValueList::CreateCommaSeparated();
  do {
    CSSFontFeatureValue* font_feature_value =
        ConsumeFontFeatureTag(stream, context);
    if (!font_feature_value) {
      return nullptr;
    }
    settings->Append(*font_feature_value);
  } while (ConsumeCommaIncludingWhitespace(stream));
  return settings;
}

CSSFontFeatureValue* ConsumeFontFeatureTag(CSSParserTokenStream& stream,
                                           const CSSParserContext& context) {
  // Feature tag name consists of 4-letter characters.
  const unsigned kTagNameLength = 4;

  const CSSParserToken& token = stream.Peek();
  // Feature tag name comes first
  if (token.GetType() != kStringToken) {
    return nullptr;
  }
  if (token.Value().length() != kTagNameLength) {
    return nullptr;
  }
  AtomicString tag = token.Value().ToAtomicString();
  stream.ConsumeIncludingWhitespace();
  for (unsigned i = 0; i < kTagNameLength; ++i) {
    // Limits the stream of characters to 0x20-0x7E, following the tag name
    // rules defined in the OpenType specification.
    UChar character = tag[i];
    if (character < 0x20 || character > 0x7E) {
      return nullptr;
    }
  }

  int tag_value = 1;
  // Feature tag values could follow: <integer> | on | off
  if (CSSPrimitiveValue* value = ConsumeInteger(stream, context, 0)) {
    tag_value = ClampTo<int>(value->GetDoubleValue());
  } else if (stream.Peek().Id() == CSSValueID::kOn ||
             stream.Peek().Id() == CSSValueID::kOff) {
    tag_value = stream.ConsumeIncludingWhitespace().Id() == CSSValueID::kOn;
  }
  return MakeGarbageCollected<CSSFontFeatureValue>(tag, tag_value);
}

CSSIdentifierValue* ConsumeFontVariantCSS21(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kNormal, CSSValueID::kSmallCaps>(stream);
}

CSSIdentifierValue* ConsumeFontTechIdent(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kFeaturesOpentype, CSSValueID::kFeaturesAat,
                      CSSValueID::kFeaturesGraphite, CSSValueID::kColorCOLRv0,
                      CSSValueID::kColorCOLRv1, CSSValueID::kColorSVG,
                      CSSValueID::kColorSbix, CSSValueID::kColorCBDT,
                      CSSValueID::kVariations, CSSValueID::kPalettes,
                      CSSValueID::kIncremental>(stream);
}

CSSIdentifierValue* ConsumeFontFormatIdent(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kCollection, CSSValueID::kEmbeddedOpentype,
                      CSSValueID::kOpentype, CSSValueID::kTruetype,
                      CSSValueID::kSVG, CSSValueID::kWoff, CSSValueID::kWoff2>(
      stream);
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
  NOTREACHED_IN_MIGRATION();
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

CSSValue* ConsumeGridBreadth(CSSParserTokenStream& stream,
                             const CSSParserContext& context) {
  const CSSParserToken& token = stream.Peek();
  if (IdentMatches<CSSValueID::kAuto, CSSValueID::kMinContent,
                   CSSValueID::kMaxContent>(token.Id())) {
    return ConsumeIdent(stream);
  }
  if (token.GetType() == kDimensionToken &&
      token.GetUnitType() == CSSPrimitiveValue::UnitType::kFlex) {
    if (token.NumericValue() < 0) {
      return nullptr;
    }
    return CSSNumericLiteralValue::Create(
        stream.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kFlex);
  }
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                UnitlessQuirk::kForbid);
}

CSSValue* ConsumeFitContent(CSSParserTokenStream& stream,
                            const CSSParserContext& context) {
  CSSFunctionValue* result;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    CSSPrimitiveValue* length = ConsumeLengthOrPercent(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative,
        UnitlessQuirk::kAllow);
    if (!length || !stream.AtEnd()) {
      return nullptr;
    }
    guard.Release();
    result = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kFitContent);
    result->Append(*length);
  }
  stream.ConsumeWhitespace();
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

  NOTREACHED_IN_MIGRATION();
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

CSSValue* ConsumeGridTrackSize(CSSParserTokenStream& stream,
                               const CSSParserContext& context) {
  const auto& token_id = stream.Peek().FunctionId();

  if (token_id == CSSValueID::kMinmax) {
    CSSFunctionValue* result;
    DCHECK_EQ(stream.Peek().GetType(), kFunctionToken);
    {
      CSSParserTokenStream::RestoringBlockGuard guard(stream);
      stream.ConsumeWhitespace();
      CSSValue* min_track_breadth = ConsumeGridBreadth(stream, context);
      auto* min_track_breadth_primitive_value =
          DynamicTo<CSSPrimitiveValue>(min_track_breadth);
      if (!min_track_breadth ||
          (min_track_breadth_primitive_value &&
           min_track_breadth_primitive_value->IsFlex()) ||
          !ConsumeCommaIncludingWhitespace(stream)) {
        return nullptr;
      }
      CSSValue* max_track_breadth = ConsumeGridBreadth(stream, context);
      if (!max_track_breadth || !stream.AtEnd()) {
        return nullptr;
      }
      guard.Release();
      result = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kMinmax);
      result->Append(*min_track_breadth);
      result->Append(*max_track_breadth);
    }
    stream.ConsumeWhitespace();
    return result;
  }

  return (token_id == CSSValueID::kFitContent)
             ? ConsumeFitContent(stream, context)
             : ConsumeGridBreadth(stream, context);
}

CSSCustomIdentValue* ConsumeCustomIdentForGridLine(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kAuto ||
      stream.Peek().Id() == CSSValueID::kSpan) {
    return nullptr;
  }
  return ConsumeCustomIdent(stream, context);
}

// Appends to the passed in CSSBracketedValueList if any, otherwise creates a
// new one. Returns nullptr if an empty list is consumed.
CSSBracketedValueList* ConsumeGridLineNames(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    bool is_subgrid_track_list,
    CSSBracketedValueList* line_names = nullptr) {
  if (stream.Peek().GetType() != kLeftBracketToken) {
    return nullptr;
  }
  {
    CSSParserTokenStream::RestoringBlockGuard savepoint(stream);
    stream.ConsumeWhitespace();

    if (!line_names) {
      line_names = MakeGarbageCollected<CSSBracketedValueList>();
    }

    while (CSSCustomIdentValue* line_name =
               ConsumeCustomIdentForGridLine(stream, context)) {
      line_names->Append(*line_name);
    }

    if (!savepoint.Release()) {
      return nullptr;
    }
  }
  stream.ConsumeWhitespace();

  if (!is_subgrid_track_list && line_names->length() == 0U) {
    return nullptr;
  }

  return line_names;
}

bool AppendLineNames(CSSParserTokenStream& stream,
                     const CSSParserContext& context,
                     bool is_subgrid_track_list,
                     CSSValueList* values) {
  if (CSSBracketedValueList* line_names =
          ConsumeGridLineNames(stream, context, is_subgrid_track_list)) {
    values->Append(*line_names);
    return true;
  }
  return false;
}

bool ConsumeGridTrackRepeatFunction(CSSParserTokenStream& stream,
                                    const CSSParserContext& context,
                                    bool is_subgrid_track_list,
                                    CSSValueList& list,
                                    bool& is_auto_repeat,
                                    bool& all_tracks_are_fixed_sized) {
  DCHECK_EQ(stream.Peek().GetType(), kFunctionToken);
  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();

  // <name-repeat> syntax for subgrids only supports `auto-fill`.
  if (is_subgrid_track_list &&
      IdentMatches<CSSValueID::kAutoFit>(stream.Peek().Id())) {
    return false;
  }

  is_auto_repeat = IdentMatches<CSSValueID::kAutoFill, CSSValueID::kAutoFit>(
      stream.Peek().Id());
  CSSValueList* repeated_values;
  // The number of repetitions for <auto-repeat> is not important at parsing
  // level because it will be computed later, let's set it to 1.
  wtf_size_t repetitions = 1;

  if (is_auto_repeat) {
    repeated_values = MakeGarbageCollected<cssvalue::CSSGridAutoRepeatValue>(
        stream.ConsumeIncludingWhitespace().Id());
  } else {
    // TODO(rob.buis): a consumeIntegerRaw would be more efficient here.
    CSSPrimitiveValue* repetition = ConsumePositiveInteger(stream, context);
    if (!repetition) {
      return false;
    }
    repetitions =
        ClampTo<wtf_size_t>(repetition->GetDoubleValue(), 0, kGridMaxTracks);
    repeated_values = CSSValueList::CreateSpaceSeparated();
  }

  if (!ConsumeCommaIncludingWhitespace(stream)) {
    return false;
  }

  wtf_size_t number_of_line_name_sets =
      AppendLineNames(stream, context, is_subgrid_track_list, repeated_values);
  wtf_size_t number_of_tracks = 0;
  while (!stream.AtEnd()) {
    if (is_subgrid_track_list) {
      if (!number_of_line_name_sets ||
          !AppendLineNames(stream, context, is_subgrid_track_list,
                           repeated_values)) {
        return false;
      }
      ++number_of_line_name_sets;
    } else {
      CSSValue* track_size = ConsumeGridTrackSize(stream, context);
      if (!track_size) {
        return false;
      }
      if (all_tracks_are_fixed_sized) {
        all_tracks_are_fixed_sized = IsGridTrackFixedSized(*track_size);
      }
      repeated_values->Append(*track_size);
      ++number_of_tracks;
      AppendLineNames(stream, context, is_subgrid_track_list, repeated_values);
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
    CSSParserTokenStream& stream,
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

  // See comment in Grid::ParseShorthand() about the use of AtEnd.

  do {
    // Handle leading <custom-ident>*.
    bool has_previous_line_names = line_names;
    line_names = ConsumeGridLineNames(
        stream, context, /* is_subgrid_track_list */ false, line_names);
    if (line_names && !has_previous_line_names) {
      template_rows_value_list->Append(*line_names);
    }

    // Handle a template-area's row.
    if (stream.Peek().GetType() != kStringToken ||
        !ParseGridTemplateAreasRow(
            stream.ConsumeIncludingWhitespace().Value().ToString(),
            grid_area_map, row_count, column_count)) {
      return false;
    }
    ++row_count;

    // Handle template-rows's track-size.
    CSSValue* value = ConsumeGridTrackSize(stream, context);
    if (!value) {
      value = CSSIdentifierValue::Create(CSSValueID::kAuto);
    }
    template_rows_value_list->Append(*value);

    // This will handle the trailing/leading <custom-ident>* in the grammar.
    line_names = ConsumeGridLineNames(stream, context,
                                      /* is_subgrid_track_list */ false);
    if (line_names) {
      template_rows_value_list->Append(*line_names);
    }
  } while (!stream.AtEnd() && !(stream.Peek().GetType() == kDelimiterToken &&
                                (stream.Peek().Delimiter() == '/' ||
                                 stream.Peek().Delimiter() == '!')));

  if (!stream.AtEnd() && stream.Peek().Delimiter() != '!') {
    if (!ConsumeSlashIncludingWhitespace(stream)) {
      return false;
    }
    template_columns = ConsumeGridTrackList(
        stream, context, TrackListType::kGridTemplateNoRepeat);
    if (!template_columns ||
        !(stream.AtEnd() || stream.Peek().Delimiter() == '!')) {
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

CSSValue* ConsumeGridLine(CSSParserTokenStream& stream,
                          const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return ConsumeIdent(stream);
  }

  CSSIdentifierValue* span_value = nullptr;
  CSSCustomIdentValue* grid_line_name = nullptr;
  CSSPrimitiveValue* numeric_value = ConsumeInteger(stream, context);
  if (numeric_value) {
    grid_line_name = ConsumeCustomIdentForGridLine(stream, context);
    span_value = ConsumeIdent<CSSValueID::kSpan>(stream);
  } else {
    span_value = ConsumeIdent<CSSValueID::kSpan>(stream);
    if (span_value) {
      numeric_value = ConsumeInteger(stream, context);
      grid_line_name = ConsumeCustomIdentForGridLine(stream, context);
      if (!numeric_value) {
        numeric_value = ConsumeInteger(stream, context);
      }
    } else {
      grid_line_name = ConsumeCustomIdentForGridLine(stream, context);
      if (grid_line_name) {
        numeric_value = ConsumeInteger(stream, context);
        span_value = ConsumeIdent<CSSValueID::kSpan>(stream);
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
  // If span is present, omit `1` if there's a trailing identifier.
  if (numeric_value &&
      (!span_value || !grid_line_name || numeric_value->GetIntValue() != 1)) {
    values->Append(*numeric_value);
  }
  if (grid_line_name) {
    values->Append(*grid_line_name);
  }
  DCHECK(values->length());
  return values;
}

CSSValue* ConsumeGridTrackList(CSSParserTokenStream& stream,
                               const CSSParserContext& context,
                               TrackListType track_list_type) {
  bool allow_grid_line_names = track_list_type != TrackListType::kGridAuto;
  if (!allow_grid_line_names && stream.Peek().GetType() == kLeftBracketToken) {
    return nullptr;
  }

  bool is_subgrid_track_list =
      track_list_type == TrackListType::kGridTemplateSubgrid;

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  if (is_subgrid_track_list) {
    if (IdentMatches<CSSValueID::kSubgrid>(stream.Peek().Id())) {
      values->Append(*ConsumeIdent(stream));
    } else {
      return nullptr;
    }
  }

  AppendLineNames(stream, context, is_subgrid_track_list, values);

  bool allow_repeat =
      is_subgrid_track_list || track_list_type == TrackListType::kGridTemplate;
  bool seen_auto_repeat = false;
  bool all_tracks_are_fixed_sized = true;
  auto IsRangeAtEnd = [](CSSParserTokenStream& stream) -> bool {
    return stream.AtEnd() || stream.Peek().GetType() == kDelimiterToken;
  };

  do {
    bool is_auto_repeat;
    if (stream.Peek().FunctionId() == CSSValueID::kRepeat) {
      if (!allow_repeat) {
        return nullptr;
      }
      if (!ConsumeGridTrackRepeatFunction(
              stream, context, is_subgrid_track_list, *values, is_auto_repeat,
              all_tracks_are_fixed_sized)) {
        return nullptr;
      }
      stream.ConsumeWhitespace();
      if (is_auto_repeat && seen_auto_repeat) {
        return nullptr;
      }

      seen_auto_repeat = seen_auto_repeat || is_auto_repeat;
    } else if (CSSValue* value = ConsumeGridTrackSize(stream, context)) {
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
    if (!allow_grid_line_names &&
        stream.Peek().GetType() == kLeftBracketToken) {
      return nullptr;
    }

    bool did_append_line_names =
        AppendLineNames(stream, context, is_subgrid_track_list, values);
    if (is_subgrid_track_list && !did_append_line_names &&
        stream.Peek().FunctionId() != CSSValueID::kRepeat) {
      return IsRangeAtEnd(stream) ? values : nullptr;
    }
  } while (!IsRangeAtEnd(stream));

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

CSSValue* ConsumeGridTemplatesRowsOrColumns(CSSParserTokenStream& stream,
                                            const CSSParserContext& context) {
  switch (stream.Peek().Id()) {
    case CSSValueID::kNone:
      return ConsumeIdent(stream);
    case CSSValueID::kSubgrid:
      return ConsumeGridTrackList(stream, context,
                                  TrackListType::kGridTemplateSubgrid);
    default:
      return ConsumeGridTrackList(stream, context,
                                  TrackListType::kGridTemplate);
  }
}

bool ConsumeGridItemPositionShorthand(bool important,
                                      CSSParserTokenStream& stream,
                                      const CSSParserContext& context,
                                      CSSValue*& start_value,
                                      CSSValue*& end_value) {
  // Input should be nullptrs.
  DCHECK(!start_value);
  DCHECK(!end_value);

  start_value = ConsumeGridLine(stream, context);
  if (!start_value) {
    return false;
  }

  if (ConsumeSlashIncludingWhitespace(stream)) {
    end_value = ConsumeGridLine(stream, context);
    if (!end_value) {
      return false;
    }
  } else {
    end_value = start_value->IsCustomIdentValue()
                    ? start_value
                    : CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  return true;
}

bool ConsumeGridTemplateShorthand(bool important,
                                  CSSParserTokenStream& stream,
                                  const CSSParserContext& context,
                                  const CSSValue*& template_rows,
                                  const CSSValue*& template_columns,
                                  const CSSValue*& template_areas) {
  DCHECK(!template_rows);
  DCHECK(!template_columns);
  DCHECK(!template_areas);

  DCHECK_EQ(gridTemplateShorthand().length(), 3u);

  {
    // 1- <grid-template-rows> / <grid-template-columns>
    CSSParserSavePoint savepoint(stream);
    template_rows = ConsumeIdent<CSSValueID::kNone>(stream);
    if (!template_rows) {
      template_rows = ConsumeGridTemplatesRowsOrColumns(stream, context);
    }

    if (template_rows && ConsumeSlashIncludingWhitespace(stream)) {
      template_columns = ConsumeGridTemplatesRowsOrColumns(stream, context);
      if (template_columns) {
        template_areas = CSSIdentifierValue::Create(CSSValueID::kNone);
        savepoint.Release();
        return true;
      }
    }

    template_rows = nullptr;
    template_columns = nullptr;
    template_areas = nullptr;
  }

  {
    // 2- [ <line-names>? <string> <track-size>? <line-names>? ]+
    // [ / <track-list> ]?
    CSSParserSavePoint savepoint(stream);
    if (ConsumeGridTemplateRowsAndAreasAndColumns(
            important, stream, context, template_rows, template_columns,
            template_areas)) {
      savepoint.Release();
      return true;
    }
  }

  // 3- 'none' alone case. This must come after the others, since “none“
  // could also be the start of case 1.
  template_rows = ConsumeIdent<CSSValueID::kNone>(stream);
  if (template_rows) {
    template_rows = CSSIdentifierValue::Create(CSSValueID::kNone);
    template_columns = CSSIdentifierValue::Create(CSSValueID::kNone);
    template_areas = CSSIdentifierValue::Create(CSSValueID::kNone);
    return true;
  }

  return false;
}

CSSValue* ConsumeMasonrySlack(CSSParserTokenStream& stream,
                              const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kNormal) {
    return ConsumeIdent(stream);
  }
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative);
}

CSSValue* ConsumeHyphenateLimitChars(CSSParserTokenStream& stream,
                                     const CSSParserContext& context) {
  CSSValueList* const list = CSSValueList::CreateSpaceSeparated();
  while (!stream.AtEnd() && list->length() < 3) {
    if (const CSSPrimitiveValue* value = ConsumeIntegerOrNumberCalc(
            stream, context, CSSPrimitiveValue::ValueRange::kPositiveInteger)) {
      list->Append(*value);
      continue;
    }
    if (const CSSIdentifierValue* ident =
            ConsumeIdent<CSSValueID::kAuto>(stream)) {
      list->Append(*ident);
      continue;
    }
    break;
  }
  if (list->length()) {
    return list;
  }
  return nullptr;
}

bool ConsumeFromPageBreakBetween(CSSParserTokenStream& stream,
                                 CSSValueID& value) {
  if (!ConsumeCSSValueId(stream, value)) {
    return false;
  }

  if (value == CSSValueID::kAlways) {
    value = CSSValueID::kPage;
    return true;
  }
  return value == CSSValueID::kAuto || value == CSSValueID::kAvoid ||
         value == CSSValueID::kLeft || value == CSSValueID::kRight;
}

bool ConsumeFromColumnBreakBetween(CSSParserTokenStream& stream,
                                   CSSValueID& value) {
  if (!ConsumeCSSValueId(stream, value)) {
    return false;
  }

  if (value == CSSValueID::kAlways) {
    value = CSSValueID::kColumn;
    return true;
  }
  return value == CSSValueID::kAuto || value == CSSValueID::kAvoid;
}

bool ConsumeFromColumnOrPageBreakInside(CSSParserTokenStream& stream,
                                        CSSValueID& value) {
  if (!ConsumeCSSValueId(stream, value)) {
    return false;
  }
  return value == CSSValueID::kAuto || value == CSSValueID::kAvoid;
}

bool ValidWidthOrHeightKeyword(CSSValueID id, const CSSParserContext& context) {
  // The keywords supported here should be kept in sync with
  // CalculationExpressionSizingKeywordNode::Keyword and the things that use
  // it.
  // TODO(https://crbug.com/353538495): This should also be kept in sync with
  // FlexBasis::ParseSingleValue, although we should eventually make it use
  // this function instead.
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

std::optional<SVGPathByteStream> ConsumePathStringArg(
    CSSParserTokenStream& args) {
  if (args.Peek().GetType() != kStringToken) {
    return std::nullopt;
  }

  CSSParserToken path = args.ConsumeIncludingWhitespace();
  SVGPathByteStreamBuilder builder;
  if (BuildByteStreamFromString(path.Value(), builder) !=
      SVGParseStatus::kNoError) {
    return std::nullopt;
  }

  return builder.CopyByteStream();
}

cssvalue::CSSPathValue* ConsumeBasicShapePath(CSSParserTokenStream& args) {
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

  return MakeGarbageCollected<cssvalue::CSSPathValue>(std::move(*byte_stream),
                                                      wind_rule);
}

CSSValue* ConsumePathFunction(CSSParserTokenStream& stream,
                              EmptyPathStringHandling empty_handling) {
  // FIXME: Add support for <url>, <basic-shape>, <geometry-box>.
  if (stream.Peek().FunctionId() != CSSValueID::kPath) {
    return nullptr;
  }

  CSSValue* value;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    std::optional<SVGPathByteStream> byte_stream = ConsumePathStringArg(stream);
    if (!byte_stream || !stream.AtEnd()) {
      return nullptr;
    }

    // https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-path
    // A path data string that does not conform to the to the grammar
    // and parsing rules of SVG 1.1, or that does conform but defines
    // an empty path, is invalid and causes the entire path() to be invalid.
    if (byte_stream->IsEmpty()) {
      if (empty_handling == EmptyPathStringHandling::kTreatAsNone) {
        value = CSSIdentifierValue::Create(CSSValueID::kNone);
      } else {
        return nullptr;
      }
    } else {
      value =
          MakeGarbageCollected<cssvalue::CSSPathValue>(std::move(*byte_stream));
    }

    guard.Release();
  }
  stream.ConsumeWhitespace();
  return value;
}

CSSValue* ConsumeRay(CSSParserTokenStream& stream,
                     const CSSParserContext& context) {
  if (stream.Peek().FunctionId() != CSSValueID::kRay) {
    return nullptr;
  }

  CSSValue* value;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    CSSPrimitiveValue* angle = nullptr;
    CSSIdentifierValue* size = nullptr;
    CSSIdentifierValue* contain = nullptr;
    bool position = false;
    CSSValue* x = nullptr;
    CSSValue* y = nullptr;
    while (!stream.AtEnd()) {
      if (!angle) {
        angle = ConsumeAngle(stream, context, std::optional<WebFeature>());
        if (angle) {
          continue;
        }
      }
      if (!size) {
        size =
            ConsumeIdent<CSSValueID::kClosestSide, CSSValueID::kClosestCorner,
                         CSSValueID::kFarthestSide, CSSValueID::kFarthestCorner,
                         CSSValueID::kSides>(stream);
        if (size) {
          continue;
        }
      }
      if (!contain) {
        contain = ConsumeIdent<CSSValueID::kContain>(stream);
        if (contain) {
          continue;
        }
      }
      if (!position && ConsumeIdent<CSSValueID::kAt>(stream)) {
        position = ConsumePosition(stream, context, UnitlessQuirk::kForbid,
                                   std::optional<WebFeature>(), x, y);
        if (position) {
          continue;
        }
      }
      return nullptr;
    }
    if (!angle) {
      return nullptr;
    }
    guard.Release();
    if (!size) {
      size = CSSIdentifierValue::Create(CSSValueID::kClosestSide);
    }
    value = MakeGarbageCollected<cssvalue::CSSRayValue>(*angle, *size, contain,
                                                        x, y);
  }
  stream.ConsumeWhitespace();
  return value;
}

CSSValue* ConsumeMaxWidthOrHeight(CSSParserTokenStream& stream,
                                  const CSSParserContext& context,
                                  UnitlessQuirk unitless) {
  if (stream.Peek().Id() == CSSValueID::kNone ||
      ValidWidthOrHeightKeyword(stream.Peek().Id(), context)) {
    return ConsumeIdent(stream);
  }
  return ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative, unitless,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize),
      AllowCalcSize::kAllowWithoutAuto);
}

CSSValue* ConsumeWidthOrHeight(CSSParserTokenStream& stream,
                               const CSSParserContext& context,
                               UnitlessQuirk unitless) {
  if (stream.Peek().Id() == CSSValueID::kAuto ||
      ValidWidthOrHeightKeyword(stream.Peek().Id(), context)) {
    return ConsumeIdent(stream);
  }
  return ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative, unitless,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize),
      AllowCalcSize::kAllowWithAuto);
}

CSSValue* ConsumeMarginOrOffset(CSSParserTokenStream& stream,
                                const CSSParserContext& context,
                                UnitlessQuirk unitless,
                                CSSAnchorQueryTypes allowed_anchor_queries) {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return ConsumeIdent(stream);
  }
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kAll, unitless,
                                allowed_anchor_queries);
}

CSSValue* ConsumeScrollPadding(CSSParserTokenStream& stream,
                               const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return ConsumeIdent(stream);
  }
  CSSParserContext::ParserModeOverridingScope scope(context, kHTMLStandardMode);
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                UnitlessQuirk::kForbid);
}

CSSValue* ConsumeScrollStart(CSSParserTokenStream& stream,
                             const CSSParserContext& context) {
  if (CSSIdentifierValue* ident =
          ConsumeIdent<CSSValueID::kAuto, CSSValueID::kStart,
                       CSSValueID::kCenter, CSSValueID::kEnd, CSSValueID::kTop,
                       CSSValueID::kBottom, CSSValueID::kLeft,
                       CSSValueID::kRight>(stream)) {
    return ident;
  }
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative);
}

CSSValue* ConsumeOffsetPath(CSSParserTokenStream& stream,
                            const CSSParserContext& context) {
  if (CSSValue* none = ConsumeIdent<CSSValueID::kNone>(stream)) {
    return none;
  }
  CSSValue* coord_box = ConsumeCoordBox(stream);

  CSSValue* offset_path = ConsumeRay(stream, context);
  if (!offset_path) {
    offset_path = ConsumeBasicShape(stream, context, AllowPathValue::kForbid);
  }
  if (!offset_path) {
    offset_path = ConsumeUrl(stream, context);
  }
  if (!offset_path) {
    offset_path =
        ConsumePathFunction(stream, EmptyPathStringHandling::kFailure);
  }

  if (!coord_box) {
    coord_box = ConsumeCoordBox(stream);
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

CSSValue* ConsumePathOrNone(CSSParserTokenStream& stream) {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kNone) {
    return ConsumeIdent(stream);
  }

  return ConsumePathFunction(stream, EmptyPathStringHandling::kTreatAsNone);
}

CSSValue* ConsumeOffsetRotate(CSSParserTokenStream& stream,
                              const CSSParserContext& context) {
  CSSValue* angle = ConsumeAngle(stream, context, std::optional<WebFeature>());
  CSSValue* keyword =
      ConsumeIdent<CSSValueID::kAuto, CSSValueID::kReverse>(stream);
  if (!angle && !keyword) {
    return nullptr;
  }

  if (!angle) {
    angle = ConsumeAngle(stream, context, std::optional<WebFeature>());
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

CSSValue* ConsumeInitialLetter(CSSParserTokenStream& stream,
                               const CSSParserContext& context) {
  if (ConsumeIdent<CSSValueID::kNormal>(stream)) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  CSSValueList* const list = CSSValueList::CreateSpaceSeparated();
  // ["drop" | "raise"] number[1,Inf]
  if (auto* sink_type =
          ConsumeIdent<CSSValueID::kDrop, CSSValueID::kRaise>(stream)) {
    if (auto* size = ConsumeNumber(
            stream, context, CSSPrimitiveValue::ValueRange::kNonNegative)) {
      auto* numeric_size = DynamicTo<CSSNumericLiteralValue>(size);
      if (numeric_size && numeric_size->DoubleValue() < 1) {
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
  if (auto* size = ConsumeNumber(stream, context,
                                 CSSPrimitiveValue::ValueRange::kNonNegative)) {
    auto* numeric_size = DynamicTo<CSSNumericLiteralValue>(size);
    if (numeric_size && numeric_size->DoubleValue() < 1) {
      return nullptr;
    }
    list->Append(*size);
    if (auto* sink_type =
            ConsumeIdent<CSSValueID::kDrop, CSSValueID::kRaise>(stream)) {
      list->Append(*sink_type);
      return list;
    }
    if (auto* sink = ConsumeIntegerOrNumberCalc(
            stream, context, CSSPrimitiveValue::ValueRange::kPositiveInteger)) {
      list->Append(*sink);
      return list;
    }
    return list;
  }

  return nullptr;
}

bool ConsumeRadii(std::array<CSSValue*, 4>& horizontal_radii,
                  std::array<CSSValue*, 4>& vertical_radii,
                  CSSParserTokenStream& stream,
                  const CSSParserContext& context,
                  bool use_legacy_parsing) {
  unsigned horizontal_value_count = 0;
  for (;
       horizontal_value_count < 4 && stream.Peek().GetType() != kDelimiterToken;
       ++horizontal_value_count) {
    horizontal_radii[horizontal_value_count] = ConsumeLengthOrPercent(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!horizontal_radii[horizontal_value_count]) {
      break;
    }
  }
  if (!horizontal_radii[0]) {
    return false;
  }
  if (ConsumeSlashIncludingWhitespace(stream)) {
    for (unsigned i = 0; i < 4; ++i) {
      vertical_radii[i] = ConsumeLengthOrPercent(
          stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      if (!vertical_radii[i]) {
        break;
      }
    }
    if (!vertical_radii[0]) {
      return false;
    }
  } else {
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
  }
  Complete4Sides(horizontal_radii);
  Complete4Sides(vertical_radii);
  return true;
}

CSSValue* ConsumeBasicShape(CSSParserTokenStream& stream,
                            const CSSParserContext& context,
                            AllowPathValue allow_path,
                            AllowBasicShapeRectValue allow_rect,
                            AllowBasicShapeXYWHValue allow_xywh) {
  CSSValue* shape = nullptr;
  if (stream.Peek().GetType() != kFunctionToken) {
    return nullptr;
  }
  CSSValueID id = stream.Peek().FunctionId();
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    if (id == CSSValueID::kCircle) {
      shape = ConsumeBasicShapeCircle(stream, context);
    } else if (id == CSSValueID::kEllipse) {
      shape = ConsumeBasicShapeEllipse(stream, context);
    } else if (id == CSSValueID::kPolygon) {
      shape = ConsumeBasicShapePolygon(stream, context);
    } else if (id == CSSValueID::kInset) {
      shape = ConsumeBasicShapeInset(stream, context);
    } else if (id == CSSValueID::kPath &&
               allow_path == AllowPathValue::kAllow) {
      shape = ConsumeBasicShapePath(stream);
    } else if (id == CSSValueID::kRect &&
               allow_rect == AllowBasicShapeRectValue::kAllow) {
      shape = ConsumeBasicShapeRect(stream, context);
    } else if (id == CSSValueID::kXywh &&
               allow_xywh == AllowBasicShapeXYWHValue::kAllow) {
      shape = ConsumeBasicShapeXYWH(stream, context);
    }
    if (!shape || !stream.AtEnd()) {
      return nullptr;
    }

    context.Count(WebFeature::kCSSBasicShape);
    guard.Release();
  }
  stream.ConsumeWhitespace();
  return shape;
}

// none | [ underline || overline || line-through || blink ] | spelling-error |
// grammar-error
CSSValue* ConsumeTextDecorationLine(CSSParserTokenStream& stream) {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kNone) {
    return ConsumeIdent(stream);
  }

  if (id == CSSValueID::kSpellingError || id == CSSValueID::kGrammarError) {
    // Note that StyleBuilderConverter::ConvertFlags() requires that values
    // other than 'none' appear in a CSSValueList.
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    list->Append(*ConsumeIdent(stream));
    return list;
  }

  CSSIdentifierValue* underline = nullptr;
  CSSIdentifierValue* overline = nullptr;
  CSSIdentifierValue* line_through = nullptr;
  CSSIdentifierValue* blink = nullptr;

  while (true) {
    id = stream.Peek().Id();
    if (id == CSSValueID::kUnderline && !underline) {
      underline = ConsumeIdent(stream);
    } else if (id == CSSValueID::kOverline && !overline) {
      overline = ConsumeIdent(stream);
    } else if (id == CSSValueID::kLineThrough && !line_through) {
      line_through = ConsumeIdent(stream);
    } else if (id == CSSValueID::kBlink && !blink) {
      blink = ConsumeIdent(stream);
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

// Consume the `text-box-edge` production.
CSSValue* ConsumeTextBoxEdge(CSSParserTokenStream& stream) {
  if (CSSIdentifierValue* auto_value =
          ConsumeIdent<CSSValueID::kAuto>(stream)) {
    return auto_value;
  }

  CSSIdentifierValue* over_type =
      ConsumeIdent<CSSValueID::kText, CSSValueID::kCap, CSSValueID::kEx>(
          stream);
  if (!over_type) {
    return nullptr;
  }
  // The second parameter is optional, the first parameter will be used for
  // both if the second parameter is not provided.
  if (CSSIdentifierValue* under_type =
          ConsumeIdent<CSSValueID::kText, CSSValueID::kAlphabetic>(stream)) {
    // Align with the CSS specification: "If only one value is specified,
    // both edges are assigned that same keyword if possible; else 'text' is
    // assumed as the missing value.".
    // If the `over_type` is 'cap' or 'ex', since it does not have a
    // corresponding line-under baseline, `text` will be used to fill the
    // missing value. If the `over_type` is `text`, the default `under_type` is
    // `text` to prioritize the same keyword.
    // In all cases above, the `under_type` of `text` can be omitted for
    // serialization.
    if (under_type->GetValueID() == CSSValueID::kText) {
      if (over_type->GetValueID() == CSSValueID::kText ||
          over_type->GetValueID() == CSSValueID::kCap ||
          over_type->GetValueID() == CSSValueID::kEx) {
        return over_type;
      }
    }
    CSSValueList* const list = CSSValueList::CreateSpaceSeparated();
    list->Append(*over_type);
    list->Append(*under_type);
    return list;
  }
  return over_type;
}

// Consume the `text-box-trim` production.
CSSValue* ConsumeTextBoxTrim(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kNone, CSSValueID::kTrimStart,
                      CSSValueID::kTrimEnd, CSSValueID::kTrimBoth>(stream);
}

// Consume the `autospace` production.
// https://drafts.csswg.org/css-text-4/#typedef-autospace
CSSValue* ConsumeAutospace(CSSParserTokenStream& stream) {
  // Currently, only `no-autospace` is supported.
  return ConsumeIdent<CSSValueID::kNoAutospace>(stream);
}

// Consume the `spacing-trim` production.
// https://drafts.csswg.org/css-text-4/#typedef-spacing-trim
CSSValue* ConsumeSpacingTrim(CSSParserTokenStream& stream) {
  return ConsumeIdent<CSSValueID::kTrimStart, CSSValueID::kSpaceAll,
                      CSSValueID::kSpaceFirst>(stream);
}

CSSValue* ConsumeTransformValue(CSSParserTokenStream& stream,
                                const CSSParserContext& context,
                                bool use_legacy_parsing) {
  CSSValueID function_id = stream.Peek().FunctionId();
  if (!IsValidCSSValueID(function_id)) {
    return nullptr;
  }
  CSSFunctionValue* transform_value = nullptr;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    if (stream.AtEnd()) {
      return nullptr;
    }
    transform_value = MakeGarbageCollected<CSSFunctionValue>(function_id);
    CSSValue* parsed_value = nullptr;
    switch (function_id) {
      case CSSValueID::kRotate:
      case CSSValueID::kRotateX:
      case CSSValueID::kRotateY:
      case CSSValueID::kRotateZ:
      case CSSValueID::kSkewX:
      case CSSValueID::kSkewY:
      case CSSValueID::kSkew:
        parsed_value = ConsumeAngle(stream, context,
                                    WebFeature::kUnitlessZeroAngleTransform);
        if (!parsed_value) {
          return nullptr;
        }
        if (function_id == CSSValueID::kSkew &&
            ConsumeCommaIncludingWhitespace(stream)) {
          transform_value->Append(*parsed_value);
          parsed_value = ConsumeAngle(stream, context,
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
            stream, context, CSSPrimitiveValue::ValueRange::kAll);
        if (!parsed_value) {
          return nullptr;
        }
        if (function_id == CSSValueID::kScale &&
            ConsumeCommaIncludingWhitespace(stream)) {
          transform_value->Append(*parsed_value);
          parsed_value = ConsumeNumberOrPercent(
              stream, context, CSSPrimitiveValue::ValueRange::kAll);
          if (!parsed_value) {
            return nullptr;
          }
        }
        break;
      case CSSValueID::kPerspective:
        if (!ConsumePerspective(stream, context, transform_value,
                                use_legacy_parsing)) {
          return nullptr;
        }
        break;
      case CSSValueID::kTranslateX:
      case CSSValueID::kTranslateY:
      case CSSValueID::kTranslate:
        parsed_value = ConsumeLengthOrPercent(
            stream, context, CSSPrimitiveValue::ValueRange::kAll);
        if (!parsed_value) {
          return nullptr;
        }
        if (function_id == CSSValueID::kTranslate &&
            ConsumeCommaIncludingWhitespace(stream)) {
          transform_value->Append(*parsed_value);
          parsed_value = ConsumeLengthOrPercent(
              stream, context, CSSPrimitiveValue::ValueRange::kAll);
          if (!parsed_value) {
            return nullptr;
          }
        }
        break;
      case CSSValueID::kTranslateZ:
        parsed_value =
            ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll);
        break;
      case CSSValueID::kMatrix:
      case CSSValueID::kMatrix3d:
        if (!ConsumeNumbers(stream, context, transform_value,
                            (function_id == CSSValueID::kMatrix3d) ? 16 : 6)) {
          return nullptr;
        }
        break;
      case CSSValueID::kScale3d:
        if (!ConsumeNumbersOrPercents(stream, context, transform_value, 3)) {
          return nullptr;
        }
        break;
      case CSSValueID::kRotate3d:
        if (!ConsumeNumbers(stream, context, transform_value, 3) ||
            !ConsumeCommaIncludingWhitespace(stream)) {
          return nullptr;
        }
        parsed_value = ConsumeAngle(stream, context,
                                    WebFeature::kUnitlessZeroAngleTransform);
        if (!parsed_value) {
          return nullptr;
        }
        break;
      case CSSValueID::kTranslate3d:
        if (!ConsumeTranslate3d(stream, context, transform_value)) {
          return nullptr;
        }
        break;
      default:
        return nullptr;
    }
    if (parsed_value) {
      transform_value->Append(*parsed_value);
    }
    if (!stream.AtEnd()) {
      return nullptr;
    }
    guard.Release();
  }
  stream.ConsumeWhitespace();
  return transform_value;
}

CSSValue* ConsumeTransformList(CSSParserTokenStream& stream,
                               const CSSParserContext& context,
                               const CSSParserLocalContext& local_context) {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(stream);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSValue* parsed_transform_value =
        ConsumeTransformValue(stream, context, local_context.UseAliasParsing());
    if (!parsed_transform_value) {
      break;
    }
    list->Append(*parsed_transform_value);
  } while (!stream.AtEnd());

  if (list->length() == 0) {
    return nullptr;
  }

  return list;
}

CSSValue* ConsumeTransitionProperty(CSSParserTokenStream& stream,
                                    const CSSParserContext& context) {
  const CSSParserToken& token = stream.Peek();
  if (token.GetType() != kIdentToken) {
    return nullptr;
  }
  if (token.Id() == CSSValueID::kNone) {
    return ConsumeIdent(stream);
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
    stream.ConsumeIncludingWhitespace();
    return MakeGarbageCollected<CSSCustomIdentValue>(unresolved_property);
  }
  return ConsumeCustomIdent(stream, context);
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

CSSValue* ConsumeBorderColorSide(CSSParserTokenStream& stream,
                                 const CSSParserContext& context,
                                 const CSSParserLocalContext& local_context) {
  CSSPropertyID shorthand = local_context.CurrentShorthand();
  bool allow_quirky_colors = IsQuirksModeBehavior(context.Mode()) &&
                             (shorthand == CSSPropertyID::kInvalid ||
                              shorthand == CSSPropertyID::kBorderColor);
  if (RuntimeEnabledFeatures::CustomizableSelectEnabled() &&
      stream.Peek().FunctionId() ==
          CSSValueID::kInternalAppearanceAutoBaseSelect &&
      IsUASheetBehavior(context.Mode())) {
    return ConsumeAppearanceAutoBaseSelectColor(stream, context);
  }
  return ConsumeColorInternal(stream, context, allow_quirky_colors,
                              AllowedColors::kAll);
}

CSSValue* ConsumeBorderWidth(CSSParserTokenStream& stream,
                             const CSSParserContext& context,
                             UnitlessQuirk unitless) {
  if (RuntimeEnabledFeatures::CustomizableSelectEnabled() &&
      IsUASheetBehavior(context.Mode()) &&
      stream.Peek().FunctionId() ==
          CSSValueID::kInternalAppearanceAutoBaseSelect) {
    CSSValue* auto_value;
    CSSValue* base_select_value;
    {
      CSSParserTokenStream::RestoringBlockGuard guard(stream);
      stream.ConsumeWhitespace();
      auto_value = ConsumeLineWidth(stream, context, unitless);
      if (!auto_value || !ConsumeCommaIncludingWhitespace(stream)) {
        return nullptr;
      }
      base_select_value = ConsumeLineWidth(stream, context, unitless);
      if (!base_select_value || !stream.AtEnd()) {
        return nullptr;
      }
      guard.Release();
    }
    stream.ConsumeWhitespace();
    return MakeGarbageCollected<CSSAppearanceAutoBaseSelectValuePair>(
        auto_value, base_select_value);
  }
  return ConsumeLineWidth(stream, context, unitless);
}

CSSValue* ParseSpacing(CSSParserTokenStream& stream,
                       const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kNormal) {
    return ConsumeIdent(stream);
  }
  // TODO(timloh): allow <percentage>s in word-spacing.
  return ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll,
                       UnitlessQuirk::kAllow);
}

CSSValue* ConsumeSingleContainerName(CSSParserTokenStream& stream,
                                     const CSSParserContext& context) {
  if (stream.Peek().GetType() != kIdentToken) {
    return nullptr;
  }
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return nullptr;
  }
  if (EqualIgnoringASCIICase(stream.Peek().Value(), "not")) {
    return nullptr;
  }
  if (EqualIgnoringASCIICase(stream.Peek().Value(), "and")) {
    return nullptr;
  }
  if (EqualIgnoringASCIICase(stream.Peek().Value(), "or")) {
    return nullptr;
  }
  return ConsumeCustomIdent(stream, context);
}

CSSValue* ConsumeContainerName(CSSParserTokenStream& stream,
                               const CSSParserContext& context) {
  if (CSSValue* value = ConsumeIdent<CSSValueID::kNone>(stream)) {
    return value;
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  while (CSSValue* value = ConsumeSingleContainerName(stream, context)) {
    list->Append(*value);
  }

  return list->length() ? list : nullptr;
}

CSSValue* ConsumeContainerType(CSSParserTokenStream& stream) {
  // container-type: normal | [ [ size | inline-size ] || scroll-state ]
  if (CSSValue* value = ConsumeIdent<CSSValueID::kNormal>(stream)) {
    return value;
  }

  CSSValue* size_value = nullptr;
  CSSValue* scroll_state_value = nullptr;

  do {
    if (!size_value) {
      size_value =
          ConsumeIdent<CSSValueID::kSize, CSSValueID::kInlineSize>(stream);
      if (size_value) {
        continue;
      }
    }
    if (!scroll_state_value &&
        RuntimeEnabledFeatures::CSSScrollStateContainerQueriesEnabled()) {
      scroll_state_value = ConsumeIdent<CSSValueID::kScrollState>(stream);
      if (scroll_state_value) {
        continue;
      }
    }
    break;
  } while (!stream.AtEnd());

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (size_value) {
    list->Append(*size_value);
  }
  if (scroll_state_value) {
    list->Append(*scroll_state_value);
  }
  if (list->length() == 0) {
    return nullptr;
  }
  return list;
}

CSSValue* ConsumeSVGPaint(CSSParserTokenStream& stream,
                          const CSSParserContext& context) {
  switch (stream.Peek().Id()) {
    case CSSValueID::kNone:
    case CSSValueID::kContextFill:
    case CSSValueID::kContextStroke:
      return ConsumeIdent(stream);
    default:
      break;
  }
  cssvalue::CSSURIValue* url = ConsumeUrl(stream, context);
  if (url) {
    CSSValue* parsed_value = nullptr;
    if (stream.Peek().Id() == CSSValueID::kNone) {
      parsed_value = ConsumeIdent(stream);
    } else {
      parsed_value = ConsumeColor(stream, context);
    }
    if (parsed_value) {
      CSSValueList* values = CSSValueList::CreateSpaceSeparated();
      values->Append(*url);
      values->Append(*parsed_value);
      return values;
    }
    return url;
  }
  return ConsumeColor(stream, context);
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

CSSCustomIdentValue* ConsumeCounterStyleName(CSSParserTokenStream& stream,
                                             const CSSParserContext& context) {
  // <counter-style-name> is a <custom-ident> that is not an ASCII
  // case-insensitive match for "none".
  const CSSParserToken name_token = stream.Peek();
  if (name_token.GetType() != kIdentToken ||
      !css_parsing_utils::IsCustomIdent<CSSValueID::kNone>(name_token.Id())) {
    return nullptr;
  }
  stream.ConsumeIncludingWhitespace();

  AtomicString name(name_token.Value().ToString());
  if (ShouldLowerCaseCounterStyleNameOnParse(name, context)) {
    name = name.LowerASCII();
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(name);
}

AtomicString ConsumeCounterStyleNameInPrelude(CSSParserTokenStream& stream,
                                              const CSSParserContext& context) {
  const CSSParserToken& name_token = stream.Peek();

  if (name_token.GetType() != kIdentToken ||
      !IsCustomIdent<CSSValueID::kNone>(name_token.Id())) {
    return g_null_atom;
  }

  if (context.Mode() != kUASheetMode) {
    // NOTE: Keep in sync with ListStyleType::ApplyValue().
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
  stream.ConsumeIncludingWhitespace();
  return name;
}

CSSValue* ConsumeFontSizeAdjust(CSSParserTokenStream& stream,
                                const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSIdentifierValue* font_metric =
      ConsumeIdent<CSSValueID::kExHeight, CSSValueID::kCapHeight,
                   CSSValueID::kChWidth, CSSValueID::kIcWidth,
                   CSSValueID::kIcHeight>(stream);

  CSSValue* value = css_parsing_utils::ConsumeNumber(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (!value) {
    value = ConsumeIdent<CSSValueID::kFromFont>(stream);
  }

  if (!value || !font_metric ||
      font_metric->GetValueID() == CSSValueID::kExHeight) {
    return value;
  }

  return MakeGarbageCollected<CSSValuePair>(font_metric, value,
                                            CSSValuePair::kKeepIdenticalValues);
}

namespace {

// Consume 'flip-block || flip-inline || flip-start' into `flips`,
// in the order that they appear.
//
// Returns true if anything was set in `flip`.
//
// https://drafts.csswg.org/css-anchor-position-1/#typedef-position-try-fallbacks-try-tactic
bool ConsumeFlipsInto(CSSParserTokenStream& stream,
                      std::array<CSSValue*, 3>& flips) {
  bool seen_flip_block = false;
  bool seen_flip_inline = false;
  bool seen_flip_start = false;

  wtf_size_t i = 0;

  while (!stream.AtEnd()) {
    CHECK_LE(i, 3u);
    if (!seen_flip_block &&
        (flips[i] = ConsumeIdent<CSSValueID::kFlipBlock>(stream))) {
      seen_flip_block = true;
      ++i;
      continue;
    }
    if (!seen_flip_inline &&
        (flips[i] = ConsumeIdent<CSSValueID::kFlipInline>(stream))) {
      seen_flip_inline = true;
      ++i;
      continue;
    }
    if (!seen_flip_start &&
        (flips[i] = ConsumeIdent<CSSValueID::kFlipStart>(stream))) {
      seen_flip_start = true;
      ++i;
      continue;
    }
    break;
  }
  return i != 0;
}

// [ <dashed-ident> || <try-tactic> ]
CSSValue* ConsumeDashedIdentOrTactic(CSSParserTokenStream& stream,
                                     const CSSParserContext& context) {
  CSSValue* dashed_ident = nullptr;
  std::array<CSSValue*, 3> flips = {nullptr};
  while (!stream.AtEnd()) {
    if (!dashed_ident && (dashed_ident = ConsumeDashedIdent(stream, context))) {
      continue;
    }
    if (context.Mode() == kUASheetMode && !dashed_ident) {
      if (stream.Peek().GetType() == kIdentToken &&
          stream.Peek().Value().ToString().StartsWith("-internal-")) {
        dashed_ident = ConsumeCustomIdent(stream, context);
        continue;
      }
    }
    // flip-block || flip-inline || flip-start
    if (!flips[0] && ConsumeFlipsInto(stream, flips)) {
      CHECK(flips[0]);
      continue;
    }
    break;
  }
  if (!flips[0] && !dashed_ident) {
    return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (dashed_ident) {
    list->Append(*dashed_ident);
  }
  for (CSSValue* flip : flips) {
    if (flip) {
      list->Append(*flip);
    }
  }
  return list;
}

// position-area( <position-area> )
CSSValue* ConsumePositionAreaFunction(CSSParserTokenStream& stream) {
  CHECK(!RuntimeEnabledFeatures::CSSPositionAreaValueEnabled());

  if (stream.Peek().FunctionId() != CSSValueID::kPositionArea) {
    return nullptr;
  }
  const CSSValue* position_area;
  {
    CSSParserTokenStream::BlockGuard guard(stream);
    stream.ConsumeWhitespace();
    position_area = ConsumePositionArea(stream);
    if (!position_area) {
      return nullptr;
    }
  }
  stream.ConsumeWhitespace();
  auto* function =
      MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kPositionArea);
  function->Append(*position_area);
  return function;
}

}  // namespace

CSSValue* ConsumeSinglePositionTryFallback(CSSParserTokenStream& stream,
                                           const CSSParserContext& context) {
  // // <dashed-ident> || <try-tactic>
  if (CSSValue* value = ConsumeDashedIdentOrTactic(stream, context)) {
    return value;
  }
  if (RuntimeEnabledFeatures::CSSPositionAreaValueEnabled()) {
    // <position-area>
    return ConsumePositionArea(stream);
  }
  // position-area( <position-area> )
  return ConsumePositionAreaFunction(stream);
}

CSSValue* ConsumePositionTryFallbacks(CSSParserTokenStream& stream,
                                      const CSSParserContext& context) {
  // none | [ [<dashed-ident> || <try-tactic>] | <'position-area'> ]#
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return ConsumeIdent(stream);
  }
  return ConsumeCommaSeparatedList(ConsumeSinglePositionTryFallback, stream,
                                   context);
}

namespace {

struct PositionAreaKeyword {
  STACK_ALLOCATED();

 public:
  enum Type {
    // [ span-all | center ]
    kGeneral,
    // [ left | right | span-left | span-right | x-start | x-end |
    //   span-x-start | span-x-end | x-self-start | x-self-end |
    //   span-x-self-start | span-x-self-end ]
    kHorizontal,
    // [ top | bottom | span-top | span-bottom | y-start | y-end |
    //   span-y-start | span-y-end | y-self-start | y-self-end |
    //   span-y-self-start | span-y-self-end ]
    kVertical,
    // [ inline-start | inline-end | span-inline-start | span-inline-end |
    //   self-inline-start | self-inline-end | span-self-inline-start |
    //   span-self-inline-end ]
    kInline,
    // [ block-start | block-end | span-block-start | span-block-end ]
    kBlock,
    // [ self-inline-start | self-inline-end | span-self-inline-start |
    //   span-self-inline-end ]
    kSelfInline,
    // [ self-block-start | self-block-end | span-self-block-start |
    //   span-self-block-end ]
    kSelfBlock,
    // [ start | end | span-start | span-end ]
    kStartEnd,
    // [ self-start | self-end | span-self-start | span-self-end ]
    kSelfStartEnd,
  };

  static bool IsCompatiblePair(const PositionAreaKeyword& first,
                               const PositionAreaKeyword& second) {
    if (first.type == kGeneral || second.type == kGeneral) {
      return true;
    }
    // The values must have been flipped in the canonical order before calling
    // this method.
    DCHECK(!(first.type == kVertical && second.type == kHorizontal));
    DCHECK(!(first.type == kInline && second.type == kBlock));
    DCHECK(!(first.type == kSelfInline && second.type == kSelfBlock));
    return (first.type == kHorizontal && second.type == kVertical) ||
           (first.type == kBlock && second.type == kInline) ||
           (first.type == kSelfBlock && second.type == kSelfInline) ||
           (first.type == second.type &&
            (first.type == kStartEnd || first.type == kSelfStartEnd));
  }

  CSSIdentifierValue* value;
  Type type;
};

std::optional<PositionAreaKeyword> ConsumePositionAreaKeyword(
    CSSParserTokenStream& stream) {
  PositionAreaKeyword::Type type = PositionAreaKeyword::kGeneral;
  switch (stream.Peek().Id()) {
    case CSSValueID::kSpanAll:
    case CSSValueID::kCenter:
      // General keywords
      break;
    case CSSValueID::kLeft:
    case CSSValueID::kRight:
    case CSSValueID::kSpanLeft:
    case CSSValueID::kSpanRight:
    case CSSValueID::kXStart:
    case CSSValueID::kXEnd:
    case CSSValueID::kSpanXStart:
    case CSSValueID::kSpanXEnd:
    case CSSValueID::kXSelfStart:
    case CSSValueID::kXSelfEnd:
    case CSSValueID::kSpanXSelfStart:
    case CSSValueID::kSpanXSelfEnd:
      type = PositionAreaKeyword::kHorizontal;
      break;
    case CSSValueID::kTop:
    case CSSValueID::kBottom:
    case CSSValueID::kSpanTop:
    case CSSValueID::kSpanBottom:
    case CSSValueID::kYStart:
    case CSSValueID::kYEnd:
    case CSSValueID::kSpanYStart:
    case CSSValueID::kSpanYEnd:
    case CSSValueID::kYSelfStart:
    case CSSValueID::kYSelfEnd:
    case CSSValueID::kSpanYSelfStart:
    case CSSValueID::kSpanYSelfEnd:
      type = PositionAreaKeyword::kVertical;
      break;
    case CSSValueID::kBlockStart:
    case CSSValueID::kBlockEnd:
    case CSSValueID::kSpanBlockStart:
    case CSSValueID::kSpanBlockEnd:
      type = PositionAreaKeyword::kBlock;
      break;
    case CSSValueID::kInlineStart:
    case CSSValueID::kInlineEnd:
    case CSSValueID::kSpanInlineStart:
    case CSSValueID::kSpanInlineEnd:
      type = PositionAreaKeyword::kInline;
      break;
    case CSSValueID::kSelfBlockStart:
    case CSSValueID::kSelfBlockEnd:
    case CSSValueID::kSpanSelfBlockStart:
    case CSSValueID::kSpanSelfBlockEnd:
      type = PositionAreaKeyword::kSelfBlock;
      break;
    case CSSValueID::kSelfInlineStart:
    case CSSValueID::kSelfInlineEnd:
    case CSSValueID::kSpanSelfInlineStart:
    case CSSValueID::kSpanSelfInlineEnd:
      type = PositionAreaKeyword::kSelfInline;
      break;
    case CSSValueID::kStart:
    case CSSValueID::kEnd:
    case CSSValueID::kSpanStart:
    case CSSValueID::kSpanEnd:
      type = PositionAreaKeyword::kStartEnd;
      break;
    case CSSValueID::kSelfStart:
    case CSSValueID::kSelfEnd:
    case CSSValueID::kSpanSelfStart:
    case CSSValueID::kSpanSelfEnd:
      type = PositionAreaKeyword::kSelfStartEnd;
      break;
    default:
      return std::nullopt;
  }
  return PositionAreaKeyword(css_parsing_utils::ConsumeIdent(stream), type);
}

}  // namespace

// <position-area> = [
//                  [ left | center | right | span-left | span-right |
//                    x-start | x-end | span-x-start | span-x-end |
//                    x-self-start | x-self-end | span-x-self-start |
//                    span-x-self-end | span-all ] ||
//                  [ top | center | bottom | span-top | span-bottom |
//                    y-start | y-end | span-y-start | span-y-end |
//                    y-self-start | y-self-end | span-y-self-start |
//                    span-y-self-end | span-all ]
//                 |
//                  [ block-start | center | block-end | span-block-start |
//                    span-block-end | span-all ] ||
//                  [ inline-start | center | inline-end | span-inline-start |
//                    span-inline-end | span-all ]
//                 |
//                  [ self-block-start | center | self-block-end |
//                    span-self-block-start | span-self-block-end |
//                    span-all ] ||
//                  [ self-inline-start | center | self-inline-end |
//                    span-self-inline-start | span-self-inline-end |
//                    span-all ]
//                 |
//                  [ start | center | end | span-start | span-end |
//                    span-all ]{1,2}
//                 |
//                  [ self-start | center | self-end | span-self-start |
//                    span-self-end | span-all ]{1,2}
//                ]
CSSValue* ConsumePositionArea(CSSParserTokenStream& stream) {
  std::optional<PositionAreaKeyword> first = ConsumePositionAreaKeyword(stream);
  if (!first.has_value()) {
    return nullptr;
  }
  std::optional<PositionAreaKeyword> second =
      ConsumePositionAreaKeyword(stream);
  if (!second.has_value()) {
    return first.value().value;
  }
  if (first.value().type == PositionAreaKeyword::kVertical ||
      first.value().type == PositionAreaKeyword::kInline ||
      first.value().type == PositionAreaKeyword::kSelfInline ||
      second.value().type == PositionAreaKeyword::kHorizontal ||
      second.value().type == PositionAreaKeyword::kBlock ||
      second.value().type == PositionAreaKeyword::kSelfBlock) {
    // Use grammar order.
    std::swap(first, second);
  }
  if (!PositionAreaKeyword::IsCompatiblePair(first.value(), second.value())) {
    return nullptr;
  }
  CSSIdentifierValue* first_value = first.value().value;
  CSSIdentifierValue* second_value = second.value().value;
  if (first_value->GetValueID() == second_value->GetValueID()) {
    return first_value;
  }
  if (first_value->GetValueID() == CSSValueID::kSpanAll &&
      !css_parsing_utils::IsRepeatedPositionAreaValue(
          second_value->GetValueID())) {
    return second_value;
  }
  if (second_value->GetValueID() == CSSValueID::kSpanAll &&
      !css_parsing_utils::IsRepeatedPositionAreaValue(
          first_value->GetValueID())) {
    return first_value;
  }
  return MakeGarbageCollected<CSSValuePair>(first_value, second_value,
                                            CSSValuePair::kDropIdenticalValues);
}

bool IsRepeatedPositionAreaValue(CSSValueID value_id) {
  switch (value_id) {
    case CSSValueID::kCenter:
    case CSSValueID::kStart:
    case CSSValueID::kEnd:
    case CSSValueID::kSpanStart:
    case CSSValueID::kSpanEnd:
    case CSSValueID::kSelfStart:
    case CSSValueID::kSelfEnd:
    case CSSValueID::kSpanSelfStart:
    case CSSValueID::kSpanSelfEnd:
      // A single value is repeated for the values above. For other values the
      // default is span-all.
      return true;
    default:
      return false;
  }
}

bool MaybeConsumeImportant(CSSParserTokenStream& stream,
                           bool allow_important_annotation) {
  stream.ConsumeWhitespace();
  if (stream.AtEnd() || !allow_important_annotation) {
    return false;
  }

  CSSParserSavePoint savepoint(stream);

  // !
  if (stream.Peek().GetType() != kDelimiterToken ||
      stream.Peek().Delimiter() != '!') {
    return false;
  }
  stream.ConsumeIncludingWhitespace();

  // important
  if (stream.Peek().GetType() != kIdentToken ||
      !EqualIgnoringASCIICase(stream.Peek().Value(), "important")) {
    return false;
  }
  stream.ConsumeIncludingWhitespace();

  savepoint.Release();
  return true;
}

}  // namespace css_parsing_utils
}  // namespace blink
