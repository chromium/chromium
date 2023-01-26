// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/numerics/clamped_math.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/css_anchor_query_type.h"
#include "third_party/blink/renderer/core/css/css_axis_value.h"
#include "third_party/blink/renderer/core/css/css_bracketed_value_list.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_cursor_image_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_font_variation_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_initial_color_value.h"
#include "third_party/blink/renderer/core/css/css_layout_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/css_ratio_value.h"
#include "third_party/blink/renderer/core/css/css_reflect_value.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/font_variant_alternates_parser.h"
#include "third_party/blink/renderer/core/css/parser/font_variant_east_asian_parser.h"
#include "third_party/blink/renderer/core/css/parser/font_variant_ligatures_parser.h"
#include "third_party/blink/renderer/core/css/parser/font_variant_numeric_parser.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/scoped_css_value.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/zoom_adjusted_pixel_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/counter_node.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/style_overflow_clip_margin.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_style_tracker.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/quotes_data.h"

// Implementations of methods in Longhand subclasses that aren't generated.

namespace blink {

namespace {

void AppendIntegerOrAutoIfZero(unsigned value, CSSValueList* list) {
  if (!value) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
    return;
  }
  list->Append(*CSSNumericLiteralValue::Create(
      value, CSSPrimitiveValue::UnitType::kInteger));
}

}  // namespace

namespace css_longhand {

const CSSValue* AlignContent::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeContentDistributionOverflowPosition(
      range, css_parsing_utils::IsContentPositionKeyword);
}

const CSSValue* AlignContent::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::
      ValueForContentPositionAndDistributionWithOverflowAlignment(
          style.AlignContent());
}

const CSSValue* AlignItems::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // align-items property does not allow the 'auto' value.
  if (css_parsing_utils::IdentMatches<CSSValueID::kAuto>(range.Peek().Id())) {
    return nullptr;
  }
  return css_parsing_utils::ConsumeSelfPositionOverflowPosition(
      range, css_parsing_utils::IsSelfPositionKeyword);
}

const CSSValue* AlignItems::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForItemPositionWithOverflowAlignment(
      style.AlignItems());
}

const CSSValue* AlignSelf::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSelfPositionOverflowPosition(
      range, css_parsing_utils::IsSelfPositionKeyword);
}

const CSSValue* AlignSelf::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForItemPositionWithOverflowAlignment(
      style.AlignSelf());
}
const CSSValue* AlignmentBaseline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.AlignmentBaseline());
}

const CSSValue* AnchorName::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (CSSValue* value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(range)) {
    return value;
  }
  return css_parsing_utils::ConsumeDashedIdent(range, context);
}
const CSSValue* AnchorName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.AnchorName()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(
      style.AnchorName()->GetName());
}

const CSSValue* AnchorScroll::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (CSSValue* value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kNone,
                                          CSSValueID::kImplicit>(range)) {
    return value;
  }
  return css_parsing_utils::ConsumeDashedIdent(range, context);
}
const CSSValue* AnchorScroll::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.AnchorScroll()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  if (style.AnchorScroll()->IsImplicit()) {
    return CSSIdentifierValue::Create(CSSValueID::kImplicit);
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(
      style.AnchorScroll()->GetName().GetName());
}

const CSSValue* AnimationComposition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSAnimationCompositionEnabled());
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeIdent<CSSValueID::kReplace, CSSValueID::kAdd,
                                      CSSValueID::kAccumulate>,
      range);
}

const CSSValue* AnimationComposition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  DCHECK(RuntimeEnabledFeatures::CSSAnimationCompositionEnabled());
  if (!style.Animations()) {
    return InitialValue();
  }
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const auto& composition_list = style.Animations()->CompositionList();
  for (const auto& composition : composition_list) {
    list->Append(*CSSIdentifierValue::Create(composition));
  }
  return list;
}

const CSSValue* AnimationComposition::InitialValue() const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kReplace));
  return list;
}

const CSSValue* AnimationDelay::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeTime, range, context,
      CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* AnimationDelay::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  // When CSSScrollTimeline is enabled, animation-delay is a shorthand
  // which expands to animation-delay-start/end, therefore this should not
  // be reachable without that feature.
  DCHECK(!RuntimeEnabledFeatures::CSSScrollTimelineEnabled());
  return ComputedStyleUtils::ValueForAnimationDelayStartList(
      style.Animations());
}

const CSSValue* AnimationDelay::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (ComputedStyleUtils::ValueForAnimationDelayStart(
                          CSSTimingData::InitialDelayStart())));
  return value;
}

const CSSValue* AnimationDelayStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSScrollTimelineEnabled());
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationDelay, range, context);
}

const CSSValue* AnimationDelayStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDelayStartList(
      style.Animations());
}

const CSSValue* AnimationDelayStart::InitialValue() const {
  return ComputedStyleUtils::ValueForAnimationDelayStart(
      CSSTimingData::InitialDelayStart());
}

const CSSValue* AnimationDelayEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSScrollTimelineEnabled());
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationDelay, range, context);
}

const CSSValue* AnimationDelayEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDelayEndList(style.Animations());
}

const CSSValue* AnimationDelayEnd::InitialValue() const {
  return ComputedStyleUtils::ValueForAnimationDelayEnd(
      CSSTimingData::InitialDelayEnd());
}

const CSSValue* AnimationDirection::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeIdent<
          CSSValueID::kNormal, CSSValueID::kAlternate, CSSValueID::kReverse,
          CSSValueID::kAlternateReverse>,
      range);
}

const CSSValue* AnimationDirection::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDirectionList(style.Animations());
}

const CSSValue* AnimationDirection::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNormal);
}

const CSSValue* AnimationDuration::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationDuration, range, context);
}

const CSSValue* AnimationDuration::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDurationList(style.Animations());
}

const CSSValue* AnimationDuration::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      const Persistent<CSSValue>, value,
      (CSSNumericLiteralValue::Create(CSSTimingData::InitialDuration().value(),
                                      CSSPrimitiveValue::UnitType::kSeconds)));
  return value;
}

const CSSValue* AnimationFillMode::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeIdent<CSSValueID::kNone, CSSValueID::kForwards,
                                      CSSValueID::kBackwards,
                                      CSSValueID::kBoth>,
      range);
}

const CSSValue* AnimationFillMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationFillModeList(style.Animations());
}

const CSSValue* AnimationFillMode::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* AnimationIterationCount::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationIterationCount, range, context);
}

const CSSValue* AnimationIterationCount::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationIterationCountList(
      style.Animations());
}

const CSSValue* AnimationIterationCount::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      const Persistent<CSSValue>, value,
      (CSSNumericLiteralValue::Create(CSSAnimationData::InitialIterationCount(),
                                      CSSPrimitiveValue::UnitType::kNumber)));
  return value;
}

const CSSValue* AnimationName::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  // Allow quoted name if this is an alias property.
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationName, range, context,
      local_context.UseAliasParsing());
}

const CSSValue* AnimationName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (wtf_size_t i = 0; i < animation_data->NameList().size(); ++i) {
      list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(
          animation_data->NameList()[i]));
    }
  } else {
    list->Append(*InitialValue());
  }
  return list;
}

const CSSValue* AnimationName::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* AnimationPlayState::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeIdent<CSSValueID::kRunning,
                                      CSSValueID::kPaused>,
      range);
}

const CSSValue* AnimationPlayState::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationPlayStateList(style.Animations());
}

const CSSValue* AnimationPlayState::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kRunning);
}

const CSSValue* AnimationRangeStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSScrollTimelineEnabled());
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationRange, range, context);
}

const CSSValue* AnimationRangeStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationRangeStartList(style.Animations(),
                                                             style);
}

const CSSValue* AnimationRangeEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSScrollTimelineEnabled());
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationRange, range, context);
}

const CSSValue* AnimationRangeEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationRangeEndList(style.Animations(),
                                                           style);
}

const CSSValue* AnimationTimeline::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationTimeline, range, context);
}

const CSSValue* AnimationTimeline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationTimelineList(style.Animations());
}

const CSSValue* AnimationTimeline::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kAuto);
}

const CSSValue* AnimationTimingFunction::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationTimingFunction, range, context);
}

const CSSValue* AnimationTimingFunction::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationTimingFunctionList(
      style.Animations());
}

const CSSValue* AnimationTimingFunction::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kEase);
}

const CSSValue* AspectRatio::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // Syntax: auto | auto 1/2 | 1/2 auto
  CSSValue* auto_value = nullptr;
  if (range.Peek().Id() == CSSValueID::kAuto) {
    auto_value = css_parsing_utils::ConsumeIdent(range);
  }

  if (range.AtEnd()) {
    return auto_value;
  }

  CSSValue* ratio = css_parsing_utils::ConsumeRatio(range, context);
  if (!ratio) {
    return nullptr;
  }

  if (!range.AtEnd()) {
    if (auto_value) {
      return nullptr;
    }
    if (range.Peek().Id() != CSSValueID::kAuto) {
      return nullptr;
    }
    auto_value = css_parsing_utils::ConsumeIdent(range);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (auto_value) {
    list->Append(*auto_value);
  }
  list->Append(*ratio);
  return list;
}

const CSSValue* AspectRatio::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  auto& ratio = style.AspectRatio();
  if (ratio.GetTypeForComputedStyle() == EAspectRatioType::kAuto) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  auto* ratio_value = MakeGarbageCollected<cssvalue::CSSRatioValue>(
      *CSSNumericLiteralValue::Create(ratio.GetRatio().width(),
                                      CSSPrimitiveValue::UnitType::kNumber),
      *CSSNumericLiteralValue::Create(ratio.GetRatio().height(),
                                      CSSPrimitiveValue::UnitType::kNumber));
  if (ratio.GetTypeForComputedStyle() == EAspectRatioType::kRatio) {
    return ratio_value;
  }

  DCHECK_EQ(ratio.GetTypeForComputedStyle(), EAspectRatioType::kAutoAndRatio);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  list->Append(*ratio_value);
  return list;
}

const CSSValue* BackdropFilter::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFilterFunctionList(range, context);
}

const CSSValue* BackdropFilter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFilter(style, style.BackdropFilter());
}

void BackdropFilter::ApplyValue(StyleResolverState& state,
                                const CSSValue& value) const {
  state.StyleBuilder().SetBackdropFilter(
      StyleBuilderConverter::ConvertFilterOperations(state, value,
                                                     PropertyID()));
}

const CSSValue* BackfaceVisibility::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(
      (style.BackfaceVisibility() == EBackfaceVisibility::kHidden)
          ? CSSValueID::kHidden
          : CSSValueID::kVisible);
}

const CSSValue* BackgroundAttachment::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeBackgroundAttachment, range);
}

const CSSValue* BackgroundAttachment::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const FillLayer* curr_layer = &style.BackgroundLayers(); curr_layer;
       curr_layer = curr_layer->Next()) {
    list->Append(*CSSIdentifierValue::Create(curr_layer->Attachment()));
  }
  return list;
}

const CSSValue* BackgroundBlendMode::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeBackgroundBlendMode, range);
}

const CSSValue* BackgroundBlendMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const FillLayer* curr_layer = &style.BackgroundLayers(); curr_layer;
       curr_layer = curr_layer->Next()) {
    list->Append(*CSSIdentifierValue::Create(curr_layer->GetBlendMode()));
  }
  return list;
}

const CSSValue* BackgroundClip::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBackgroundBox(
      range, local_context, css_parsing_utils::AllowTextValue::kAllow);
}

const CSSValue* BackgroundClip::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.BackgroundLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    EFillBox box = curr_layer->Clip();
    list->Append(*CSSIdentifierValue::Create(box));
  }
  return list;
}

void UseCountBackgroundClip(Document& document, const CSSValue& value) {
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kBorder:
        UseCounter::Count(document, WebFeature::kCSSBackgroundClipBorder);
        break;
      case CSSValueID::kContent:
        UseCounter::Count(document, WebFeature::kCSSBackgroundClipContent);
        break;
      case CSSValueID::kPadding:
        UseCounter::Count(document, WebFeature::kCSSBackgroundClipPadding);
        break;
      default:
        break;
    }
  }
}

// TODO(crbug.com/1339290): Revert to use the generated implementation once the
// use counters are no longer needed. Also remove UseCountBackgroundClip above.
void BackgroundClip::ApplyValue(StyleResolverState& state,
                                const CSSValue& value) const {
  Document& document = state.GetDocument();
  FillLayer* curr_child = &state.StyleBuilder().AccessBackgroundLayers();
  FillLayer* prev_child = nullptr;
  const auto* value_list = DynamicTo<CSSValueList>(value);
  if (value_list && !value.IsImageSetValue()) {
    // Walk each value and put it into a layer, creating new layers as needed.
    const auto* curr_val = value_list->begin();
    while (curr_child || curr_val != value_list->end()) {
      if (!curr_child) {
        curr_child = prev_child->EnsureNext();
      }
      CSSToStyleMap::MapFillClip(state, curr_child, *curr_val->Get());
      UseCountBackgroundClip(document, *curr_val->Get());
      prev_child = curr_child;
      curr_child = curr_child->Next();
      // as per https://w3c.github.io/csswg-drafts/css-backgrounds/#layering
      if (++curr_val == value_list->end() && curr_child) {
        curr_val = value_list->begin();
      }
    }
  } else {
    while (curr_child) {
      CSSToStyleMap::MapFillClip(state, curr_child, value);
      curr_child = curr_child->Next();
    }
  }
}

const CSSValue* BackgroundColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

const blink::Color BackgroundColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& background_color = style.BackgroundColor();
  if (!style.InForcedColorsMode() && !background_color.HasColorKeyword()) {
    // Fast path.
    if (is_current_color) {
      *is_current_color = false;
    }
    return background_color.GetColor();
  } else {
    if (style.ShouldForceColor(background_color)) {
      return To<Longhand>(GetCSSPropertyInternalForcedBackgroundColor())
          .ColorIncludingFallback(false, style, is_current_color);
    }
    return background_color.Resolve(style.GetCurrentColor(),
                                    style.UsedColorScheme(), is_current_color);
  }
}

const CSSValue* BackgroundColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (allow_visited_style) {
    return cssvalue::CSSColor::Create(style.VisitedDependentColor(*this));
  }

  StyleColor background_color = style.BackgroundColor();
  if (style.ShouldForceColor(background_color)) {
    return GetCSSPropertyInternalForcedBackgroundColor()
        .CSSValueFromComputedStyle(style, nullptr, allow_visited_style);
  }
  // https://drafts.csswg.org/cssom/#resolved-values
  // For this property, the resolved value is the used value.
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, background_color, CSSValuePhase::kUsedValue);
}

const CSSValue* BackgroundImage::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeImageOrNone, range, context);
}

const CSSValue* BackgroundImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const FillLayer& fill_layer = style.BackgroundLayers();
  return ComputedStyleUtils::BackgroundImageOrWebkitMaskImage(
      style, allow_visited_style, fill_layer);
}

const CSSValue* BackgroundOrigin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBackgroundBox(
      range, local_context, css_parsing_utils::AllowTextValue::kForbid);
}

const CSSValue* BackgroundOrigin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.BackgroundLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    EFillBox box = curr_layer->Origin();
    list->Append(*CSSIdentifierValue::Create(box));
  }
  return list;
}

const CSSValue* BackgroundPositionX::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumePositionLonghand<CSSValueID::kLeft,
                                                 CSSValueID::kRight>,
      range, context);
}

const CSSValue* BackgroundPositionX::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const FillLayer* curr_layer = &style.BackgroundLayers();
  return ComputedStyleUtils::BackgroundPositionXOrWebkitMaskPositionX(
      style, curr_layer);
}

const CSSValue* BackgroundPositionY::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumePositionLonghand<CSSValueID::kTop,
                                                 CSSValueID::kBottom>,
      range, context);
}

const CSSValue* BackgroundPositionY::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const FillLayer* curr_layer = &style.BackgroundLayers();
  return ComputedStyleUtils::BackgroundPositionYOrWebkitMaskPositionY(
      style, curr_layer);
}

const CSSValue* BackgroundSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBackgroundOrMaskSize(
      range, context, local_context, WebFeature::kNegativeBackgroundSize);
}

const CSSValue* BackgroundSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const FillLayer& fill_layer = style.BackgroundLayers();
  return ComputedStyleUtils::BackgroundImageOrWebkitMaskSize(style, fill_layer);
}

const CSSValue* BaselineSource::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BaselineSource());
}

const CSSValue* BaselineShift::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kBaseline || id == CSSValueID::kSub ||
      id == CSSValueID::kSuper) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  return css_parsing_utils::ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* BaselineShift::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  switch (style.BaselineShiftType()) {
    case EBaselineShiftType::kSuper:
      return CSSIdentifierValue::Create(CSSValueID::kSuper);
    case EBaselineShiftType::kSub:
      return CSSIdentifierValue::Create(CSSValueID::kSub);
    case EBaselineShiftType::kLength:
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.BaselineShift(), style);
  }
  NOTREACHED();
  return nullptr;
}

void BaselineShift::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetBaselineShiftType(state.ParentStyle()->BaselineShiftType());
  builder.SetBaselineShift(state.ParentStyle()->BaselineShift());
}

void BaselineShift::ApplyValue(StyleResolverState& state,
                               const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    EBaselineShiftType baseline_shift_type = EBaselineShiftType::kLength;
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kBaseline:
        baseline_shift_type = EBaselineShiftType::kLength;
        break;
      case CSSValueID::kSub:
        baseline_shift_type = EBaselineShiftType::kSub;
        break;
      case CSSValueID::kSuper:
        baseline_shift_type = EBaselineShiftType::kSuper;
        break;
      default:
        NOTREACHED();
    }
    builder.SetBaselineShiftType(baseline_shift_type);
    builder.SetBaselineShift(Length::Fixed());
  } else {
    builder.SetBaselineShiftType(EBaselineShiftType::kLength);
    builder.SetBaselineShift(StyleBuilderConverter::ConvertLength(
        state, To<CSSPrimitiveValue>(value)));
  }
}

const CSSValue* BlockSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(range, context);
}

bool BlockSize::IsLayoutDependent(const ComputedStyle* style,
                                  LayoutObject* layout_object) const {
  return layout_object && (layout_object->IsBox() || layout_object->IsSVG());
}

const CSSValue* BorderBlockEndColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const CSSValue* BorderBlockEndWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderWidth(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* BorderBlockStartColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const CSSValue* BorderBlockStartWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderWidth(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* BorderBottomColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const blink::Color BorderBottomColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  StyleColor border_bottom_color = style.BorderBottomColor();
  if (style.ShouldForceColor(border_bottom_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(false, style, is_current_color);
  }
  return ComputedStyleUtils::BorderSideColor(style, border_bottom_color,
                                             style.BorderBottomStyle(),
                                             visited_link, is_current_color);
}

const CSSValue* BorderBottomColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  StyleColor border_bottom_color = style.BorderBottomColor();
  if (style.ShouldForceColor(border_bottom_color)) {
    return GetCSSPropertyInternalForcedBorderColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style);
  }
  // https://drafts.csswg.org/cssom/#resolved-values
  // For this property, the resolved value is the used value.
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, border_bottom_color, CSSValuePhase::kUsedValue);
}

const CSSValue* BorderBottomLeftRadius::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(range, context);
}

const CSSValue* BorderBottomLeftRadius::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForBorderRadiusCorner(
      style.BorderBottomLeftRadius(), style);
}

const CSSValue* BorderBottomRightRadius::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(range, context);
}

const CSSValue* BorderBottomRightRadius::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForBorderRadiusCorner(
      style.BorderBottomRightRadius(), style);
}
const CSSValue* BorderBottomStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BorderBottomStyle());
}

const CSSValue* BorderBottomWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderWidthSide(range, context, local_context);
}

const CSSValue* BorderBottomWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.BorderBottomWidth(), style);
}

const CSSValue* BorderCollapse::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.BorderCollapse() == EBorderCollapse::kCollapse) {
    return CSSIdentifierValue::Create(CSSValueID::kCollapse);
  }
  return CSSIdentifierValue::Create(CSSValueID::kSeparate);
}

const CSSValue* BorderEndEndRadius::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(range, context);
}

const CSSValue* BorderEndStartRadius::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(range, context);
}

const CSSValue* BorderImageOutset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageOutset(range, context);
}

const CSSValue* BorderImageOutset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageQuad(
      style.BorderImage().Outset(), style);
}

const CSSValue* BorderImageOutset::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSQuadValue>, value,
                      (MakeGarbageCollected<CSSQuadValue>(
                          CSSNumericLiteralValue::Create(
                              0, CSSPrimitiveValue::UnitType::kInteger),
                          CSSQuadValue::kSerializeAsQuad)));
  return value;
}

const CSSValue* BorderImageRepeat::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageRepeat(range);
}

const CSSValue* BorderImageRepeat::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageRepeat(style.BorderImage());
}

const CSSValue* BorderImageRepeat::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kStretch);
}

const CSSValue* BorderImageSlice::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageSlice(
      range, context, css_parsing_utils::DefaultFill::kNoFill);
}

const CSSValue* BorderImageSlice::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageSlice(style.BorderImage());
}

const CSSValue* BorderImageSlice::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      const Persistent<cssvalue::CSSBorderImageSliceValue>, value,
      (MakeGarbageCollected<cssvalue::CSSBorderImageSliceValue>(
          MakeGarbageCollected<CSSQuadValue>(
              CSSNumericLiteralValue::Create(
                  100, CSSPrimitiveValue::UnitType::kPercentage),
              CSSQuadValue::kSerializeAsQuad),
          /* fill */ false)));
  return value;
}

const CSSValue* BorderImageSource::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeImageOrNone(range, context);
}

const CSSValue* BorderImageSource::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.BorderImageSource()) {
    return style.BorderImageSource()->ComputedCSSValue(style,
                                                       allow_visited_style);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* BorderImageSource::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

void BorderImageSource::ApplyValue(StyleResolverState& state,
                                   const CSSValue& value) const {
  state.StyleBuilder().SetBorderImageSource(
      state.GetStyleImage(CSSPropertyID::kBorderImageSource, value));
}

const CSSValue* BorderImageWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageWidth(range, context);
}

const CSSValue* BorderImageWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageQuad(
      style.BorderImage().BorderSlices(), style);
}

const CSSValue* BorderImageWidth::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSQuadValue>, value,
                      (MakeGarbageCollected<CSSQuadValue>(
                          CSSNumericLiteralValue::Create(
                              1, CSSPrimitiveValue::UnitType::kInteger),
                          CSSQuadValue::kSerializeAsQuad)));
  return value;
}

const CSSValue* BorderInlineEndColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const CSSValue* BorderInlineEndWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderWidth(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* BorderInlineStartColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const CSSValue* BorderInlineStartWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderWidth(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* BorderLeftColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const blink::Color BorderLeftColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  StyleColor border_left_color = style.BorderLeftColor();
  if (style.ShouldForceColor(border_left_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(false, style, is_current_color);
  }
  return ComputedStyleUtils::BorderSideColor(style, border_left_color,
                                             style.BorderLeftStyle(),
                                             visited_link, is_current_color);
}

const CSSValue* BorderLeftColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  StyleColor border_left_color = style.BorderLeftColor();
  if (style.ShouldForceColor(border_left_color)) {
    return GetCSSPropertyInternalForcedBorderColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style);
  }
  // https://drafts.csswg.org/cssom/#resolved-values
  // For this property, the resolved value is the used value.
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, border_left_color, CSSValuePhase::kUsedValue);
}

const CSSValue* BorderLeftStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BorderLeftStyle());
}

const CSSValue* BorderLeftWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderWidthSide(range, context, local_context);
}

const CSSValue* BorderLeftWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.BorderLeftWidth(), style);
}

const CSSValue* BorderRightColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const blink::Color BorderRightColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  StyleColor border_right_color = style.BorderRightColor();
  if (style.ShouldForceColor(border_right_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(false, style, is_current_color);
  }
  return ComputedStyleUtils::BorderSideColor(style, border_right_color,
                                             style.BorderRightStyle(), false,
                                             is_current_color);
}

const CSSValue* BorderRightColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  StyleColor border_right_color = style.BorderRightColor();
  if (style.ShouldForceColor(border_right_color)) {
    return GetCSSPropertyInternalForcedBorderColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style);
  }
  // https://drafts.csswg.org/cssom/#resolved-values
  // For this property, the resolved value is the used value.
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, border_right_color, CSSValuePhase::kUsedValue);
}

const CSSValue* BorderRightStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BorderRightStyle());
}

const CSSValue* BorderRightWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderWidthSide(range, context, local_context);
}

const CSSValue* BorderRightWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.BorderRightWidth(), style);
}

const CSSValue* BorderStartStartRadius::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(range, context);
}

const CSSValue* BorderStartEndRadius::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(range, context);
}

const CSSValue* BorderTopColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const blink::Color BorderTopColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  StyleColor border_top_color = style.BorderTopColor();
  if (style.ShouldForceColor(border_top_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(false, style, is_current_color);
  }
  return ComputedStyleUtils::BorderSideColor(style, border_top_color,
                                             style.BorderTopStyle(),
                                             visited_link, is_current_color);
}

const CSSValue* BorderTopColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  StyleColor border_top_color = style.BorderTopColor();
  if (style.ShouldForceColor(border_top_color)) {
    return GetCSSPropertyInternalForcedBorderColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style);
  }
  // https://drafts.csswg.org/cssom/#resolved-values
  // For this property, the resolved value is the used value.
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::ComputedStyleUtils::CurrentColorOrValidColor(
                   style, border_top_color, CSSValuePhase::kUsedValue);
}

const CSSValue* BorderTopLeftRadius::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(range, context);
}

const CSSValue* BorderTopLeftRadius::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForBorderRadiusCorner(
      style.BorderTopLeftRadius(), style);
}

const CSSValue* BorderTopRightRadius::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(range, context);
}

const CSSValue* BorderTopRightRadius::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForBorderRadiusCorner(
      style.BorderTopRightRadius(), style);
}

const CSSValue* BorderTopStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BorderTopStyle());
}

const CSSValue* BorderTopWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderWidthSide(range, context, local_context);
}

const CSSValue* BorderTopWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.BorderTopWidth(), style);
}

const CSSValue* Bottom::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessUnlessShorthand(local_context),
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchor));
}

bool Bottom::IsLayoutDependent(const ComputedStyle* style,
                               LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Bottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForPositionOffset(style, *this,
                                                    layout_object);
}

const CSSValue* BoxShadow::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeShadow(
      range, context, css_parsing_utils::AllowInsetAndSpread::kAllow);
}

const CSSValue* BoxShadow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  // https://drafts.csswg.org/cssom/#resolved-values
  // For this property, the resolved value is the used value.
  return ComputedStyleUtils::ValueForShadowList(style.BoxShadow(), style, true,
                                                CSSValuePhase::kUsedValue);
}

const CSSValue* BoxSizing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.BoxSizing() == EBoxSizing::kContentBox) {
    return CSSIdentifierValue::Create(CSSValueID::kContentBox);
  }
  return CSSIdentifierValue::Create(CSSValueID::kBorderBox);
}

const CSSValue* BreakAfter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BreakAfter());
}

const CSSValue* BreakBefore::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BreakBefore());
}

const CSSValue* BreakInside::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BreakInside());
}

const CSSValue* BufferedRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BufferedRendering());
}

const CSSValue* CaptionSide::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.CaptionSide());
}

const CSSValue* CaretColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color CaretColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  StyleAutoColor auto_color = style.CaretColor();
  // TODO(rego): We may want to adjust the caret color if it's the same as
  // the background to ensure good visibility and contrast.
  StyleColor result = auto_color.IsAutoColor() ? StyleColor::CurrentColor()
                                               : auto_color.ToStyleColor();
  if (style.ShouldForceColor(result)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return result.Resolve(style.GetCurrentColor(), style.UsedColorScheme(),
                        is_current_color);
}

const CSSValue* CaretColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (allow_visited_style) {
    return cssvalue::CSSColor::Create(style.VisitedDependentColor(*this));
  }

  StyleAutoColor auto_color = style.CaretColor();
  // TODO(rego): We may want to adjust the caret color if it's the same as
  // the background to ensure good visibility and contrast.
  StyleColor result = auto_color.IsAutoColor() ? StyleColor::CurrentColor()
                                               : auto_color.ToStyleColor();
  if (style.ShouldForceColor(result)) {
    return cssvalue::CSSColor::Create(style.GetInternalForcedCurrentColor());
  }

  // https://drafts.csswg.org/cssom/#resolved-values
  // For this property, the resolved value is the used value.
  return ComputedStyleUtils::ValueForStyleAutoColor(style, style.CaretColor(),
                                                    CSSValuePhase::kUsedValue);
}

const CSSValue* Clear::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Clear());
}

namespace {

CSSValue* ConsumeClipComponent(CSSParserTokenRange& range,
                               const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeLength(
      range, context, CSSPrimitiveValue::ValueRange::kAll,
      css_parsing_utils::UnitlessQuirk::kAllow);
}

}  // namespace

const CSSValue* Clip::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  if (range.Peek().FunctionId() != CSSValueID::kRect) {
    return nullptr;
  }

  CSSParserTokenRange args = css_parsing_utils::ConsumeFunction(range);
  // rect(t, r, b, l) || rect(t r b l)
  CSSValue* top = ConsumeClipComponent(args, context);
  if (!top) {
    return nullptr;
  }
  bool needs_comma = css_parsing_utils::ConsumeCommaIncludingWhitespace(args);
  CSSValue* right = ConsumeClipComponent(args, context);
  if (!right || (needs_comma &&
                 !css_parsing_utils::ConsumeCommaIncludingWhitespace(args))) {
    return nullptr;
  }
  CSSValue* bottom = ConsumeClipComponent(args, context);
  if (!bottom || (needs_comma &&
                  !css_parsing_utils::ConsumeCommaIncludingWhitespace(args))) {
    return nullptr;
  }
  CSSValue* left = ConsumeClipComponent(args, context);
  if (!left || !args.AtEnd()) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSQuadValue>(top, right, bottom, left,
                                            CSSQuadValue::kSerializeAsRect);
}

const CSSValue* Clip::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HasAutoClip()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  CSSValue* top = ComputedStyleUtils::ZoomAdjustedPixelValueOrAuto(
      style.Clip().Top(), style);
  CSSValue* right = ComputedStyleUtils::ZoomAdjustedPixelValueOrAuto(
      style.Clip().Right(), style);
  CSSValue* bottom = ComputedStyleUtils::ZoomAdjustedPixelValueOrAuto(
      style.Clip().Bottom(), style);
  CSSValue* left = ComputedStyleUtils::ZoomAdjustedPixelValueOrAuto(
      style.Clip().Left(), style);
  return MakeGarbageCollected<CSSQuadValue>(top, right, bottom, left,
                                            CSSQuadValue::kSerializeAsRect);
}

const CSSValue* ClipPath::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  if (cssvalue::CSSURIValue* url =
          css_parsing_utils::ConsumeUrl(range, context)) {
    return url;
  }
  return css_parsing_utils::ConsumeBasicShape(
      range, context, css_parsing_utils::AllowPathValue::kAllow);
}

const CSSValue* ClipPath::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (ClipPathOperation* operation = style.ClipPath()) {
    if (operation->GetType() == ClipPathOperation::kShape) {
      return ValueForBasicShape(
          style, To<ShapeClipPathOperation>(operation)->GetBasicShape());
    }
    if (operation->GetType() == ClipPathOperation::kReference) {
      AtomicString url = To<ReferenceClipPathOperation>(operation)->Url();
      return MakeGarbageCollected<cssvalue::CSSURIValue>(url);
    }
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* ClipRule::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ClipRule());
}

const CSSValue* Color::ParseSingleValue(CSSParserTokenRange& range,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

const blink::Color Color::ColorIncludingFallback(bool visited_link,
                                                 const ComputedStyle& style,
                                                 bool* is_current_color) const {
  DCHECK(!visited_link);
  if (style.ShouldForceColor(style.Color())) {
    return To<Longhand>(GetCSSPropertyInternalForcedColor())
        .ColorIncludingFallback(false, style, is_current_color);
  }
  return style.GetCurrentColor(is_current_color);
}

const CSSValue* Color::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.ShouldForceColor(style.Color())) {
    return GetCSSPropertyInternalForcedColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style);
  }
  return cssvalue::CSSColor::Create(allow_visited_style
                                        ? style.VisitedDependentColor(*this)
                                        : style.GetCurrentColor());
}

void Color::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetColor(builder.InitialColorForColorScheme());
  builder.SetColorIsInherited(false);
  builder.SetColorIsCurrentColor(false);
}

void Color::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (builder.ShouldPreserveParentColor()) {
    builder.SetColor(StyleColor(
        state.ParentStyle()->VisitedDependentColor(GetCSSPropertyColor())));
  } else {
    builder.SetColor(state.ParentStyle()->Color());
  }
  builder.SetColorIsInherited(true);
  builder.SetColorIsCurrentColor(state.ParentStyle()->ColorIsCurrentColor());
}

void Color::ApplyValue(StyleResolverState& state, const CSSValue& value) const {
  // As per the spec, 'color: currentColor' is treated as 'color: inherit'
  ComputedStyleBuilder& builder = state.StyleBuilder();
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kCurrentcolor) {
    ApplyInherit(state);
    builder.SetColorIsCurrentColor(true);
    if (state.UsesHighlightPseudoInheritance() &&
        state.OriginatingElementStyle()) {
      builder.SetColor(state.OriginatingElementStyle()->Color());
    }
    return;
  }
  if (value.IsInitialColorValue()) {
    DCHECK_EQ(state.GetElement(), state.GetDocument().documentElement());
    builder.SetColor(builder.InitialColorForColorScheme());
  } else {
    builder.SetColor(StyleBuilderConverter::ConvertStyleColor(state, value));
  }
  builder.SetColorIsInherited(false);
  builder.SetColorIsCurrentColor(false);
}

const CSSValue* ColorInterpolation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ColorInterpolation());
}

const CSSValue* ColorInterpolationFilters::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ColorInterpolationFilters());
}

const CSSValue* ColorRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ColorRendering());
}

const CSSValue* ColorScheme::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  CSSValue* only = nullptr;
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  do {
    CSSValueID id = range.Peek().Id();
    // 'normal' is handled above, and needs to be excluded from
    // ConsumeCustomIdent below.
    if (id == CSSValueID::kNormal) {
      return nullptr;
    }
    CSSValue* value =
        css_parsing_utils::ConsumeIdent<CSSValueID::kDark, CSSValueID::kLight,
                                        CSSValueID::kOnly>(range);
    if (id == CSSValueID::kOnly) {
      if (only) {
        return nullptr;
      }
      if (values->length()) {
        values->Append(*value);
        return values;
      }
      only = value;
      continue;
    }
    if (!value) {
      value = css_parsing_utils::ConsumeCustomIdent(range, context);
    }
    if (!value) {
      return nullptr;
    }
    values->Append(*value);
  } while (!range.AtEnd());
  if (!values->length()) {
    return nullptr;
  }
  if (only) {
    values->Append(*only);
  }
  return values;
}

const CSSValue* ColorScheme::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.ColorScheme().empty()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (auto ident : style.ColorScheme()) {
    list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(ident));
  }
  return list;
}

const CSSValue* ColorScheme::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNormal);
}

namespace {

void ApplyColorSchemeValue(StyleResolverState& state,
                           const CSSValueList* scheme_list) {
  ColorSchemeFlags flags =
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal);
  Vector<AtomicString> color_schemes;

  Document& document = state.GetDocument();
  if (scheme_list) {
    flags = StyleBuilderConverter::ExtractColorSchemes(document, *scheme_list,
                                                       &color_schemes);
  } else {
    flags = document.GetStyleEngine().GetPageColorSchemes();
  }

  state.StyleBuilder().SetColorScheme(std::move(color_schemes));
  state.StyleBuilder().SetUsedColorScheme(
      flags, document.GetStyleEngine().GetPreferredColorScheme(),
      document.GetStyleEngine().GetForceDarkModeEnabled());

  if (flags & static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark)) {
    // Record kColorSchemeDarkSupportedOnRoot if dark is present (though dark
    // may not be used). This metric is also recorded in
    // StyleEngine::UpdateColorSchemeMetrics if a meta tag supports dark.
    if (document.documentElement() == state.GetElement()) {
      UseCounter::Count(document, WebFeature::kColorSchemeDarkSupportedOnRoot);
    }
  }
}

}  // namespace

void ColorScheme::ApplyInitial(StyleResolverState& state) const {
  ApplyColorSchemeValue(state, nullptr /* scheme_list */);
}

void ColorScheme::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetColorScheme(state.ParentStyle()->ColorScheme());
  builder.SetDarkColorScheme(state.ParentStyle()->DarkColorScheme());
  builder.SetColorSchemeForced(state.ParentStyle()->ColorSchemeForced());
}

void ColorScheme::ApplyValue(StyleResolverState& state,
                             const CSSValue& value) const {
  const CSSValueList* scheme_list = DynamicTo<CSSValueList>(value);
  DCHECK(scheme_list || (value.IsIdentifierValue() &&
                         DynamicTo<CSSIdentifierValue>(value)->GetValueID() ==
                             CSSValueID::kNormal));
  ApplyColorSchemeValue(state, scheme_list);
}

const CSSValue* ColumnCount::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColumnCount(range, context);
}

const CSSValue* ColumnCount::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HasAutoColumnCount()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return CSSNumericLiteralValue::Create(style.ColumnCount(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* ColumnFill::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetColumnFill());
}

const CSSValue* ColumnGap::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGapLength(range, context);
}

const CSSValue* ColumnGap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool) const {
  return ComputedStyleUtils::ValueForGapLength(style.ColumnGap(), style);
}

const CSSValue* ColumnRuleColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color ColumnRuleColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  StyleColor column_rule_color = style.ColumnRuleColor();
  if (style.ShouldForceColor(column_rule_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return column_rule_color.Resolve(style.GetCurrentColor(),
                                   style.UsedColorScheme(), is_current_color);
}

const CSSValue* ColumnRuleColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, style.ColumnRuleColor(),
                   CSSValuePhase::kComputedValue);
}

const CSSValue* ColumnRuleStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ColumnRuleStyle());
}

const CSSValue* ColumnRuleWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLineWidth(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ColumnRuleWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.ColumnRuleWidth(), style);
}

const CSSValue* ColumnSpan::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIdent<CSSValueID::kAll, CSSValueID::kNone>(
      range);
}

const CSSValue* ColumnSpan::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(static_cast<unsigned>(style.GetColumnSpan())
                                        ? CSSValueID::kAll
                                        : CSSValueID::kNone);
}

const CSSValue* ColumnWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColumnWidth(range, context);
}

const CSSValue* ColumnWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HasAutoColumnWidth()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return ZoomAdjustedPixelValue(style.ColumnWidth(), style);
}

// none | strict | content | [ size || layout || style || paint ]
const CSSValue* Contain::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (id == CSSValueID::kStrict || id == CSSValueID::kContent) {
    list->Append(*css_parsing_utils::ConsumeIdent(range));
    return list;
  }

  CSSIdentifierValue* size = nullptr;
  CSSIdentifierValue* layout = nullptr;
  CSSIdentifierValue* style = nullptr;
  CSSIdentifierValue* paint = nullptr;
  while (true) {
    id = range.Peek().Id();
    if ((id == CSSValueID::kSize ||

         id == CSSValueID::kInlineSize) &&
        !size) {
      size = css_parsing_utils::ConsumeIdent(range);
    } else if (id == CSSValueID::kLayout && !layout) {
      layout = css_parsing_utils::ConsumeIdent(range);
    } else if (id == CSSValueID::kStyle && !style) {
      style = css_parsing_utils::ConsumeIdent(range);
    } else if (id == CSSValueID::kPaint && !paint) {
      paint = css_parsing_utils::ConsumeIdent(range);
    } else {
      break;
    }
  }
  if (size) {
    list->Append(*size);
  }
  if (layout) {
    list->Append(*layout);
  }
  if (style) {
    context.Count(WebFeature::kCSSValueContainStyle);
    list->Append(*style);
  }
  if (paint) {
    list->Append(*paint);
  }
  if (!list->length()) {
    return nullptr;
  }
  return list;
}

const CSSValue* Contain::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.Contain()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  if (style.Contain() == kContainsStrict) {
    return CSSIdentifierValue::Create(CSSValueID::kStrict);
  }
  if (style.Contain() == kContainsContent) {
    return CSSIdentifierValue::Create(CSSValueID::kContent);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  DCHECK_NE(style.Contain() & kContainsSize, kContainsBlockSize);
  if ((style.Contain() & kContainsSize) == kContainsSize) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kSize));
  } else {
    if (style.Contain() & kContainsInlineSize) {
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kInlineSize));
    }
  }
  if (style.Contain() & kContainsLayout) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kLayout));
  }
  if (style.Contain() & kContainsStyle) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kStyle));
  }
  if (style.Contain() & kContainsPaint) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kPaint));
  }
  DCHECK(list->length());
  return list;
}

const CSSValue* ContainIntrinsicWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIntrinsicSizeLonghand(range, context);
}

const CSSValue* ContainIntrinsicWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForIntrinsicLength(
      style, style.ContainIntrinsicWidth());
}

const CSSValue* ContainIntrinsicHeight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIntrinsicSizeLonghand(range, context);
}

const CSSValue* ContainIntrinsicHeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForIntrinsicLength(
      style, style.ContainIntrinsicHeight());
}

const CSSValue* ContainIntrinsicInlineSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIntrinsicSizeLonghand(range, context);
}

const CSSValue* ContainIntrinsicBlockSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIntrinsicSizeLonghand(range, context);
}

const CSSValue* ContainerName::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeContainerName(range, context);
}

const CSSValue* ContainerName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (!style.ContainerName()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  for (const Member<const ScopedCSSName>& name :
       style.ContainerName()->GetNames()) {
    list->Append(*ComputedStyleUtils::ValueForCustomIdentOrNone(name.Get()));
  }
  return list;
}

const CSSValue* ContainerType::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeContainerType(range);
}

const CSSValue* ContainerType::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  DCHECK_NE(style.ContainerType() & kContainerTypeSize,
            kContainerTypeBlockSize);
  if (style.ContainerType() == kContainerTypeNormal) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
  if (style.ContainerType() == kContainerTypeSize) {
    return CSSIdentifierValue::Create(CSSValueID::kSize);
  }
  if (style.ContainerType() == kContainerTypeInlineSize) {
    return CSSIdentifierValue::Create(CSSValueID::kInlineSize);
  }
  NOTREACHED();
  return nullptr;
}

namespace {

CSSValue* ConsumeAttr(CSSParserTokenRange args,
                      const CSSParserContext& context) {
  if (args.Peek().GetType() != kIdentToken) {
    return nullptr;
  }

  AtomicString attr_name =
      args.ConsumeIncludingWhitespace().Value().ToAtomicString();
  if (!args.AtEnd()) {
    return nullptr;
  }

  if (context.IsHTMLDocument()) {
    attr_name = attr_name.LowerASCII();
  }

  CSSFunctionValue* attr_value =
      MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kAttr);
  attr_value->Append(*MakeGarbageCollected<CSSCustomIdentValue>(attr_name));
  return attr_value;
}

CSSValue* ConsumeCounterContent(CSSParserTokenRange args,
                                const CSSParserContext& context,
                                bool counters) {
  CSSCustomIdentValue* identifier =
      css_parsing_utils::ConsumeCustomIdent(args, context);
  if (!identifier) {
    return nullptr;
  }

  CSSStringValue* separator = nullptr;
  if (!counters) {
    separator = MakeGarbageCollected<CSSStringValue>(String());
  } else {
    if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(args) ||
        args.Peek().GetType() != kStringToken) {
      return nullptr;
    }
    separator = MakeGarbageCollected<CSSStringValue>(
        args.ConsumeIncludingWhitespace().Value().ToString());
  }

  CSSCustomIdentValue* list_style = nullptr;
  if (css_parsing_utils::ConsumeCommaIncludingWhitespace(args)) {
    // Note: CSS3 spec doesn't allow 'none' but CSS2.1 allows it. We currently
    // allow it for backward compatibility.
    // See https://github.com/w3c/csswg-drafts/issues/5795 for details.
    if (args.Peek().Id() == CSSValueID::kNone) {
      list_style = MakeGarbageCollected<CSSCustomIdentValue>("none");
      args.ConsumeIncludingWhitespace();
    } else {
      list_style = css_parsing_utils::ConsumeCounterStyleName(args, context);
    }
  } else {
    list_style = MakeGarbageCollected<CSSCustomIdentValue>("decimal");
  }

  if (!list_style || !args.AtEnd()) {
    return nullptr;
  }
  return MakeGarbageCollected<cssvalue::CSSCounterValue>(identifier, list_style,
                                                         separator);
}

}  // namespace

const CSSValue* Content::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  if (css_parsing_utils::IdentMatches<CSSValueID::kNone, CSSValueID::kNormal>(
          range.Peek().Id())) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  CSSValueList* outer_list = CSSValueList::CreateSlashSeparated();
  bool alt_text_present = false;
  do {
    CSSValue* parsed_value = css_parsing_utils::ConsumeImage(range, context);
    if (!parsed_value) {
      parsed_value = css_parsing_utils::ConsumeIdent<
          CSSValueID::kOpenQuote, CSSValueID::kCloseQuote,
          CSSValueID::kNoOpenQuote, CSSValueID::kNoCloseQuote>(range);
    }
    if (!parsed_value) {
      parsed_value = css_parsing_utils::ConsumeString(range);
    }
    if (!parsed_value) {
      if (range.Peek().FunctionId() == CSSValueID::kAttr) {
        parsed_value =
            ConsumeAttr(css_parsing_utils::ConsumeFunction(range), context);
      } else if (range.Peek().FunctionId() == CSSValueID::kCounter) {
        parsed_value = ConsumeCounterContent(
            css_parsing_utils::ConsumeFunction(range), context, false);
      } else if (range.Peek().FunctionId() == CSSValueID::kCounters) {
        parsed_value = ConsumeCounterContent(
            css_parsing_utils::ConsumeFunction(range), context, true);
      }
    }
    if (!parsed_value) {
      if (css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
        // No values were parsed before the slash, so nothing to apply the
        // alternative text to.
        if (!values->length()) {
          return nullptr;
        }
        alt_text_present = true;
      } else {
        return nullptr;
      }
    } else {
      values->Append(*parsed_value);
    }
  } while (!range.AtEnd() && !alt_text_present);
  outer_list->Append(*values);
  if (alt_text_present) {
    CSSStringValue* alt_text = css_parsing_utils::ConsumeString(range);
    if (!alt_text) {
      return nullptr;
    }
    outer_list->Append(*alt_text);
  }
  return outer_list;
}

const CSSValue* Content::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForContentData(style, allow_visited_style);
}

void Content::ApplyInitial(StyleResolverState& state) const {
  state.StyleBuilder().SetContent(nullptr);
}

void Content::ApplyInherit(StyleResolverState& state) const {
  // FIXME: In CSS3, it will be possible to inherit content. In CSS2 it is not.
  // This note is a reminder that eventually "inherit" needs to be supported.
}

void Content::ApplyValue(StyleResolverState& state,
                         const CSSValue& value) const {
  DCHECK(value.IsScopedValue());
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK(identifier_value->GetValueID() == CSSValueID::kNormal ||
           identifier_value->GetValueID() == CSSValueID::kNone);
    if (identifier_value->GetValueID() == CSSValueID::kNone) {
      builder.SetContent(MakeGarbageCollected<NoneContentData>());
    } else {
      builder.SetContent(nullptr);
    }
    return;
  }
  const CSSValueList& outer_list = To<CSSValueList>(value);
  ContentData* first_content = nullptr;
  ContentData* prev_content = nullptr;
  for (auto& item : To<CSSValueList>(outer_list.Item(0))) {
    ContentData* next_content = nullptr;
    if (item->IsImageGeneratorValue() || item->IsImageSetValue() ||
        item->IsImageValue()) {
      next_content = MakeGarbageCollected<ImageContentData>(
          state.GetStyleImage(CSSPropertyID::kContent, *item));
    } else if (const auto* counter_value =
                   DynamicTo<cssvalue::CSSCounterValue>(item.Get())) {
      next_content = MakeGarbageCollected<CounterContentData>(
          AtomicString(counter_value->Identifier()), counter_value->ListStyle(),
          AtomicString(counter_value->Separator()),
          counter_value->GetTreeScope());
    } else if (auto* item_identifier_value =
                   DynamicTo<CSSIdentifierValue>(item.Get())) {
      QuoteType quote_type;
      switch (item_identifier_value->GetValueID()) {
        default:
          NOTREACHED();
          [[fallthrough]];
        case CSSValueID::kOpenQuote:
          quote_type = QuoteType::kOpen;
          break;
        case CSSValueID::kCloseQuote:
          quote_type = QuoteType::kClose;
          break;
        case CSSValueID::kNoOpenQuote:
          quote_type = QuoteType::kNoOpen;
          break;
        case CSSValueID::kNoCloseQuote:
          quote_type = QuoteType::kNoClose;
          break;
      }
      next_content = MakeGarbageCollected<QuoteContentData>(quote_type);
    } else {
      String string;
      if (const auto* function_value =
              DynamicTo<CSSFunctionValue>(item.Get())) {
        DCHECK_EQ(function_value->FunctionType(), CSSValueID::kAttr);
        builder.SetHasAttrContent();
        // TODO: Can a namespace be specified for an attr(foo)?
        QualifiedName attr(
            g_null_atom,
            To<CSSCustomIdentValue>(function_value->Item(0)).Value(),
            g_null_atom);
        const AtomicString& attr_value = state.GetElement().getAttribute(attr);
        string = attr_value.IsNull() ? g_empty_string : attr_value.GetString();
      } else {
        string = To<CSSStringValue>(*item).Value();
      }
      if (prev_content && prev_content->IsText()) {
        TextContentData* text_content = To<TextContentData>(prev_content);
        text_content->SetText(text_content->GetText() + string);
        continue;
      }
      next_content = MakeGarbageCollected<TextContentData>(string);
    }

    if (!first_content) {
      first_content = next_content;
    } else {
      prev_content->SetNext(next_content);
    }

    prev_content = next_content;
  }
  // If alt text was provided, it will be present as the final element of the
  // outer list.
  if (outer_list.length() > 1) {
    String string = To<CSSStringValue>(outer_list.Item(1)).Value();
    auto* alt_content = MakeGarbageCollected<AltTextContentData>(string);
    prev_content->SetNext(alt_content);
  }
  DCHECK(first_content);
  builder.SetContent(first_content);
}

const int kCounterIncrementDefaultValue = 1;

const CSSValue* CounterIncrement::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCounter(range, context,
                                           kCounterIncrementDefaultValue);
}

const CSSValue* CounterIncrement::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForCounterDirectives(
      style, CounterNode::kIncrementType);
}

const int kCounterResetDefaultValue = 0;

const CSSValue* CounterReset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCounter(range, context,
                                           kCounterResetDefaultValue);
}

const CSSValue* CounterReset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForCounterDirectives(style,
                                                       CounterNode::kResetType);
}

const int kCounterSetDefaultValue = 0;

const CSSValue* CounterSet::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCounter(range, context,
                                           kCounterSetDefaultValue);
}

const CSSValue* CounterSet::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForCounterDirectives(style,
                                                       CounterNode::kSetType);
}

const CSSValue* Cursor::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  bool in_quirks_mode = IsQuirksModeBehavior(context.Mode());
  CSSValueList* list = nullptr;
  while (CSSValue* image = css_parsing_utils::ConsumeImage(
             range, context,
             css_parsing_utils::ConsumeGeneratedImagePolicy::kForbid)) {
    double num;
    gfx::Point hot_spot(-1, -1);
    bool hot_spot_specified = false;
    if (css_parsing_utils::ConsumeNumberRaw(range, context, num)) {
      hot_spot.set_x(ClampTo<int>(num));
      if (!css_parsing_utils::ConsumeNumberRaw(range, context, num)) {
        return nullptr;
      }
      hot_spot.set_y(ClampTo<int>(num));
      hot_spot_specified = true;
    }

    if (!list) {
      list = CSSValueList::CreateCommaSeparated();
    }

    list->Append(*MakeGarbageCollected<cssvalue::CSSCursorImageValue>(
        *image, hot_spot_specified, hot_spot));
    if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(range)) {
      return nullptr;
    }
  }

  CSSValueID id = range.Peek().Id();
  if (!range.AtEnd()) {
    if (id == CSSValueID::kWebkitZoomIn) {
      context.Count(WebFeature::kPrefixedCursorZoomIn);
    } else if (id == CSSValueID::kWebkitZoomOut) {
      context.Count(WebFeature::kPrefixedCursorZoomOut);
    } else if (id == CSSValueID::kWebkitGrab) {
      context.Count(WebFeature::kPrefixedCursorGrab);
    } else if (id == CSSValueID::kWebkitGrabbing) {
      context.Count(WebFeature::kPrefixedCursorGrabbing);
    }
  }
  CSSValue* cursor_type = nullptr;
  if (id == CSSValueID::kHand) {
    if (!in_quirks_mode) {  // Non-standard behavior
      return nullptr;
    }
    cursor_type = CSSIdentifierValue::Create(CSSValueID::kPointer);
    range.ConsumeIncludingWhitespace();
  } else if ((id >= CSSValueID::kAuto && id <= CSSValueID::kWebkitZoomOut) ||
             id == CSSValueID::kCopy || id == CSSValueID::kNone) {
    cursor_type = css_parsing_utils::ConsumeIdent(range);
  } else {
    return nullptr;
  }

  if (!list) {
    return cursor_type;
  }
  list->Append(*cursor_type);
  return list;
}

const CSSValue* Cursor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = nullptr;
  CursorList* cursors = style.Cursors();
  if (cursors && cursors->size() > 0) {
    list = CSSValueList::CreateCommaSeparated();
    for (const CursorData& cursor : *cursors) {
      if (StyleImage* image = cursor.GetImage()) {
        list->Append(*MakeGarbageCollected<cssvalue::CSSCursorImageValue>(
            *image->ComputedCSSValue(style, allow_visited_style),
            cursor.HotSpotSpecified(), cursor.HotSpot()));
      }
    }
  }
  CSSValue* value = CSSIdentifierValue::Create(style.Cursor());
  if (list) {
    list->Append(*value);
    return list;
  }
  return value;
}

void Cursor::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.ClearCursorList();
  builder.SetCursor(ComputedStyleInitialValues::InitialCursor());
}

void Cursor::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetCursor(state.ParentStyle()->Cursor());
  builder.SetCursorList(state.ParentStyle()->Cursors());
}

void Cursor::ApplyValue(StyleResolverState& state,
                        const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.ClearCursorList();
  if (auto* value_list = DynamicTo<CSSValueList>(value)) {
    builder.SetCursor(ECursor::kAuto);
    for (const auto& item : *value_list) {
      if (const auto* cursor =
              DynamicTo<cssvalue::CSSCursorImageValue>(*item)) {
        const CSSValue& image = cursor->ImageValue();
        builder.AddCursor(state.GetStyleImage(CSSPropertyID::kCursor, image),
                          cursor->HotSpotSpecified(), cursor->HotSpot());
      } else {
        builder.SetCursor(To<CSSIdentifierValue>(*item).ConvertTo<ECursor>());
      }
    }
  } else {
    builder.SetCursor(To<CSSIdentifierValue>(value).ConvertTo<ECursor>());
  }
}

const CSSValue* Cx::ParseSingleValue(CSSParserTokenRange& range,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* Cx::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Cx(), style);
}

const CSSValue* Cy::ParseSingleValue(CSSParserTokenRange& range,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* Cy::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Cy(), style);
}

const CSSValue* D::ParseSingleValue(CSSParserTokenRange& range,
                                    const CSSParserContext&,
                                    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePathOrNone(range);
}

const CSSValue* D::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (const StylePath* style_path = style.D()) {
    return style_path->ComputedCSSValue();
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* Direction::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Direction());
}

void Direction::ApplyValue(StyleResolverState& state,
                           const CSSValue& value) const {
  state.StyleBuilder().SetDirection(
      To<CSSIdentifierValue>(value).ConvertTo<TextDirection>());
}

namespace {

static bool IsDisplayOutside(CSSValueID id) {
  return id >= CSSValueID::kInline && id <= CSSValueID::kBlock;
}

static bool IsDisplayInside(CSSValueID id) {
  if (id >= CSSValueID::kFlowRoot && id <= CSSValueID::kGrid) {
    return true;
  }
  if (id == CSSValueID::kMath) {
    return RuntimeEnabledFeatures::MathMLCoreEnabled();
  }
  return false;
}

static bool IsDisplayBox(CSSValueID id) {
  return css_parsing_utils::IdentMatches<CSSValueID::kNone,
                                         CSSValueID::kContents>(id);
}

static bool IsDisplayInternal(CSSValueID id) {
  return id >= CSSValueID::kTableRowGroup && id <= CSSValueID::kTableCaption;
}

static bool IsDisplayLegacy(CSSValueID id) {
  return id >= CSSValueID::kInlineBlock && id <= CSSValueID::kWebkitInlineFlex;
}

}  // namespace

const CSSValue* Display::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  CSSIdentifierValue* display_outside = nullptr;
  CSSIdentifierValue* display_inside = nullptr;
  if (IsDisplayOutside(id)) {
    display_outside = css_parsing_utils::ConsumeIdent(range);
    if (range.AtEnd()) {
      return display_outside;
    }
    id = range.Peek().Id();
    if (!IsDisplayInside(id)) {
      return nullptr;
    }
    display_inside = css_parsing_utils::ConsumeIdent(range);
  } else if (IsDisplayInside(id)) {
    display_inside = css_parsing_utils::ConsumeIdent(range);
    if (range.AtEnd()) {
      return display_inside;
    }
    id = range.Peek().Id();
    if (!IsDisplayOutside(id)) {
      return nullptr;
    }
    display_outside = css_parsing_utils::ConsumeIdent(range);
  }
  if (display_outside && display_inside) {
    // TODO(crbug.com/995106): should apply to more than just math.
    if (display_inside->GetValueID() == CSSValueID::kMath) {
      CSSValueList* parsed_values = CSSValueList::CreateSpaceSeparated();
      parsed_values->Append(*display_outside);
      parsed_values->Append(*display_inside);
      return parsed_values;
    }
    return nullptr;
  }
  if (id == CSSValueID::kListItem || IsDisplayBox(id) ||
      IsDisplayInternal(id) || IsDisplayLegacy(id)) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  if (!RuntimeEnabledFeatures::CSSLayoutAPIEnabled()) {
    return nullptr;
  }

  if (!context.IsSecureContext()) {
    return nullptr;
  }

  CSSValueID function = range.Peek().FunctionId();
  if (function != CSSValueID::kLayout &&
      function != CSSValueID::kInlineLayout) {
    return nullptr;
  }

  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = css_parsing_utils::ConsumeFunction(range_copy);
  CSSCustomIdentValue* name =
      css_parsing_utils::ConsumeCustomIdent(args, context);

  // If we didn't get a custom-ident or didn't exhaust the function arguments
  // return nothing.
  if (!name || !args.AtEnd()) {
    return nullptr;
  }

  range = range_copy;
  return MakeGarbageCollected<cssvalue::CSSLayoutFunctionValue>(
      name, /* is_inline */ function == CSSValueID::kInlineLayout);
}

const CSSValue* Display::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.IsDisplayLayoutCustomBox()) {
    return MakeGarbageCollected<cssvalue::CSSLayoutFunctionValue>(
        MakeGarbageCollected<CSSCustomIdentValue>(
            style.DisplayLayoutCustomName()),
        style.IsDisplayInlineType());
  }

  if (style.Display() == EDisplay::kBlockMath) {
    CSSValueList* values = CSSValueList::CreateSpaceSeparated();
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kBlock));
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kMath));
    return values;
  }

  return CSSIdentifierValue::Create(style.Display());
}

void Display::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetDisplay(ComputedStyleInitialValues::InitialDisplay());
  builder.SetDisplayLayoutCustomName(
      ComputedStyleInitialValues::InitialDisplayLayoutCustomName());
}

void Display::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetDisplay(state.ParentStyle()->Display());
  builder.SetDisplayLayoutCustomName(
      state.ParentStyle()->DisplayLayoutCustomName());
}

void Display::ApplyValue(StyleResolverState& state,
                         const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    builder.SetDisplay(identifier_value->ConvertTo<EDisplay>());
    builder.SetDisplayLayoutCustomName(
        ComputedStyleInitialValues::InitialDisplayLayoutCustomName());
    return;
  }

  if (value.IsValueList()) {
    builder.SetDisplayLayoutCustomName(
        ComputedStyleInitialValues::InitialDisplayLayoutCustomName());
    const CSSValueList& display_pair = To<CSSValueList>(value);
    DCHECK_EQ(display_pair.length(), 2u);
    DCHECK(display_pair.Item(0).IsIdentifierValue());
    DCHECK(display_pair.Item(1).IsIdentifierValue());
    const auto& outside = To<CSSIdentifierValue>(display_pair.Item(0));
    const auto& inside = To<CSSIdentifierValue>(display_pair.Item(1));
    // TODO(crbug.com/995106): should apply to more than just math.
    DCHECK(inside.GetValueID() == CSSValueID::kMath);
    if (outside.GetValueID() == CSSValueID::kBlock) {
      builder.SetDisplay(EDisplay::kBlockMath);
    } else {
      builder.SetDisplay(EDisplay::kMath);
    }
    return;
  }

  const auto& layout_function_value =
      To<cssvalue::CSSLayoutFunctionValue>(value);

  EDisplay display = layout_function_value.IsInline()
                         ? EDisplay::kInlineLayoutCustom
                         : EDisplay::kLayoutCustom;
  builder.SetDisplay(display);
  builder.SetDisplayLayoutCustomName(layout_function_value.GetName());
}

const CSSValue* DominantBaseline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.DominantBaseline());
}

const CSSValue* EmptyCells::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.EmptyCells());
}

const CSSValue* Fill::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGPaint(range, context);
}

const CSSValue* Fill::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGPaint(style.FillPaint(), style);
}

const blink::Color Fill::ColorIncludingFallback(bool visited_link,
                                                const ComputedStyle& style,
                                                bool* is_current_color) const {
  DCHECK(!visited_link);
  DCHECK(style.FillPaint().HasColor());
  const StyleColor& fill_color = style.FillPaint().GetColor();
  if (style.ShouldForceColor(fill_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return fill_color.Resolve(style.GetCurrentColor(), style.UsedColorScheme(),
                            is_current_color);
}

const CSSValue* FillOpacity::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(range, context);
}

const CSSValue* FillOpacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.FillOpacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* FillRule::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.FillRule());
}

const CSSValue* Filter::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFilterFunctionList(range, context);
}

const CSSValue* Filter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFilter(style, style.Filter());
}

void Filter::ApplyValue(StyleResolverState& state,
                        const CSSValue& value) const {
  state.StyleBuilder().SetFilter(StyleBuilderConverter::ConvertFilterOperations(
      state, value, PropertyID()));
}

const CSSValue* FlexBasis::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (css_parsing_utils::IdentMatches<
          CSSValueID::kAuto, CSSValueID::kContent, CSSValueID::kMinContent,
          CSSValueID::kMaxContent, CSSValueID::kFitContent>(
          range.Peek().Id())) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* FlexBasis::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.FlexBasis(),
                                                             style);
}

const CSSValue* FlexDirection::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.FlexDirection());
}

const CSSValue* FlexGrow::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* FlexGrow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.FlexGrow(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* FlexShrink::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* FlexShrink::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.FlexShrink(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* FlexWrap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.FlexWrap());
}

const CSSValue* Float::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HasOutOfFlowPosition()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return CSSIdentifierValue::Create(style.Floating());
}

const CSSValue* FloodColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color FloodColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  StyleColor flood_color = style.FloodColor();
  if (style.ShouldForceColor(flood_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return style.ResolvedColor(flood_color, is_current_color);
}

const CSSValue* FloodColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.FloodColor(), CSSValuePhase::kComputedValue);
}

const CSSValue* FloodOpacity::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(range, context);
}

const CSSValue* FloodOpacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.FloodOpacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* FontFamily::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontFamily(range);
}

const CSSValue* FontFamily::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontFamily(style);
}

void FontFamily::ApplyInitial(StyleResolverState& state) const {
  state.GetFontBuilder().SetFamilyDescription(
      FontBuilder::InitialFamilyDescription());
  state.GetFontBuilder().SetFamilyTreeScope(nullptr);
}

void FontFamily::ApplyInherit(StyleResolverState& state) const {
  state.GetFontBuilder().SetFamilyDescription(
      state.ParentFontDescription().GetFamilyDescription());
  CSSFontSelector* selector = static_cast<CSSFontSelector*>(
      state.ParentStyle()->GetFont().GetFontSelector());
  const TreeScope* tree_scope = selector ? selector->GetTreeScope() : nullptr;
  state.GetFontBuilder().SetFamilyTreeScope(tree_scope);
}

const CSSValue* FontFeatureSettings::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontFeatureSettings(range, context);
}

const CSSValue* FontFeatureSettings::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontFeatureSettings(style);
}

const CSSValue* FontKerning::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontKerning(style);
}

const CSSValue* FontOpticalSizing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontOpticalSizing(style);
}

const CSSValue* FontPalette::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  blink::FontPalette* palette = style.GetFontDescription().GetFontPalette();

  if (!palette) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  switch (palette->GetPaletteNameKind()) {
    case blink::FontPalette::kNormalPalette:
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
    case blink::FontPalette::kLightPalette:
      return CSSIdentifierValue::Create(CSSValueID::kLight);
    case blink::FontPalette::kDarkPalette:
      return CSSIdentifierValue::Create(CSSValueID::kDark);
    case blink::FontPalette::kCustomPalette:
      return MakeGarbageCollected<CSSCustomIdentValue>(
          palette->GetPaletteValuesName());
    default:
      NOTREACHED();
  }
  return CSSIdentifierValue::Create(CSSValueID::kNormal);
}

const CSSValue* FontPalette::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal ||
      range.Peek().Id() == CSSValueID::kLight ||
      range.Peek().Id() == CSSValueID::kDark) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  return css_parsing_utils::ConsumeDashedIdent(range, context);
}

const CSSValue* FontSizeAdjust::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSFontSizeAdjustEnabled());
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeNumber(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* FontSizeAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HasFontSizeAdjust()) {
    return CSSNumericLiteralValue::Create(style.FontSizeAdjust(),
                                          CSSPrimitiveValue::UnitType::kNumber);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* FontSize::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontSize(
      range, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* FontSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontSize(style);
}

const CSSValue* FontStretch::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontStretch(range, context);
}

const CSSValue* FontStretch::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontStretch(style);
}

const CSSValue* FontStyle::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontStyle(range, context);
}

const CSSValue* FontStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontStyle(style);
}

const CSSValue* FontVariantCaps::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIdent<
      CSSValueID::kNormal, CSSValueID::kSmallCaps, CSSValueID::kAllSmallCaps,
      CSSValueID::kPetiteCaps, CSSValueID::kAllPetiteCaps, CSSValueID::kUnicase,
      CSSValueID::kTitlingCaps>(range);
}

const CSSValue* FontVariantCaps::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontVariantCaps(style);
}

const CSSValue* FontVariantEastAsian::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  FontVariantEastAsianParser east_asian_parser;
  do {
    if (east_asian_parser.ConsumeEastAsian(range) !=
        FontVariantEastAsianParser::ParseResult::kConsumedValue) {
      return nullptr;
    }
  } while (!range.AtEnd());

  return east_asian_parser.FinalizeValue();
}

const CSSValue* FontVariantEastAsian::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontVariantEastAsian(style);
}

const CSSValue* FontVariantLigatures::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal ||
      range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  FontVariantLigaturesParser ligatures_parser;
  do {
    if (ligatures_parser.ConsumeLigature(range) !=
        FontVariantLigaturesParser::ParseResult::kConsumedValue) {
      return nullptr;
    }
  } while (!range.AtEnd());

  return ligatures_parser.FinalizeValue();
}

const CSSValue* FontVariantLigatures::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontVariantLigatures(style);
}

const CSSValue* FontVariantNumeric::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  FontVariantNumericParser numeric_parser;
  do {
    if (numeric_parser.ConsumeNumeric(range) !=
        FontVariantNumericParser::ParseResult::kConsumedValue) {
      return nullptr;
    }
  } while (!range.AtEnd());

  return numeric_parser.FinalizeValue();
}

const CSSValue* FontVariantNumeric::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontVariantNumeric(style);
}

const CSSValue* FontVariantAlternates::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::FontVariantAlternatesEnabled());

  if (range.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  FontVariantAlternatesParser alternates_parser;
  do {
    if (alternates_parser.ConsumeAlternates(range, context) !=
        FontVariantAlternatesParser::ParseResult::kConsumedValue) {
      return nullptr;
    }
  } while (!range.AtEnd());

  return alternates_parser.FinalizeValue();
}

const CSSValue* FontVariantAlternates::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontVariantAlternates(style);
}

namespace {

cssvalue::CSSFontVariationValue* ConsumeFontVariationTag(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  // Feature tag name consists of 4-letter characters.
  static const wtf_size_t kTagNameLength = 4;

  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  // Feature tag name comes first
  if (token.GetType() != kStringToken) {
    return nullptr;
  }
  if (token.Value().length() != kTagNameLength) {
    return nullptr;
  }
  AtomicString tag = token.Value().ToAtomicString();
  for (wtf_size_t i = 0; i < kTagNameLength; ++i) {
    // Limits the range of characters to 0x20-0x7E, following the tag name rules
    // defined in the OpenType specification.
    UChar character = tag[i];
    if (character < 0x20 || character > 0x7E) {
      return nullptr;
    }
  }

  double tag_value = 0;
  if (!css_parsing_utils::ConsumeNumberRaw(range, context, tag_value)) {
    return nullptr;
  }
  return MakeGarbageCollected<cssvalue::CSSFontVariationValue>(
      tag, ClampTo<float>(tag_value));
}

}  // namespace

const CSSValue* FontVariationSettings::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  CSSValueList* variation_settings = CSSValueList::CreateCommaSeparated();
  do {
    cssvalue::CSSFontVariationValue* font_variation_value =
        ConsumeFontVariationTag(range, context);
    if (!font_variation_value) {
      return nullptr;
    }
    variation_settings->Append(*font_variation_value);
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));
  return variation_settings;
}

const CSSValue* FontVariationSettings::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontVariationSettings(style);
}

const CSSValue* FontWeight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontWeight(range, context);
}

const CSSValue* FontWeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontWeight(style);
}

const CSSValue* FontSynthesisWeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(
      style.GetFontDescription().GetFontSynthesisWeight());
}

const CSSValue* FontSynthesisStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(
      style.GetFontDescription().GetFontSynthesisStyle());
}

const CSSValue* FontSynthesisSmallCaps::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(
      style.GetFontDescription().GetFontSynthesisSmallCaps());
}

const CSSValue* FontVariantPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  DCHECK(RuntimeEnabledFeatures::FontVariantPositionEnabled());
  return ComputedStyleUtils::ValueForFontVariantPosition(style);
}

const CSSValue* ForcedColorAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ForcedColorAdjust());
}

void InternalVisitedColor::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetInternalVisitedColor(builder.InitialColorForColorScheme());
  builder.SetInternalVisitedColorIsCurrentColor(false);
}

void InternalVisitedColor::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (builder.ShouldPreserveParentColor()) {
    builder.SetInternalVisitedColor(StyleColor(
        state.ParentStyle()->VisitedDependentColor(GetCSSPropertyColor())));
  } else {
    builder.SetInternalVisitedColor(state.ParentStyle()->Color());
  }
  builder.SetInternalVisitedColorIsCurrentColor(
      state.ParentStyle()->InternalVisitedColorIsCurrentColor());
}

void InternalVisitedColor::ApplyValue(StyleResolverState& state,
                                      const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kCurrentcolor) {
    ApplyInherit(state);
    builder.SetInternalVisitedColorIsCurrentColor(true);
    if (state.UsesHighlightPseudoInheritance() &&
        state.OriginatingElementStyle()) {
      builder.SetInternalVisitedColor(
          state.OriginatingElementStyle()->InternalVisitedColor());
    }
    return;
  }
  if (value.IsInitialColorValue()) {
    DCHECK_EQ(state.GetElement(), state.GetDocument().documentElement());
    builder.SetInternalVisitedColor(builder.InitialColorForColorScheme());
  } else {
    builder.SetInternalVisitedColor(
        StyleBuilderConverter::ConvertStyleColor(state, value, true));
  }
  builder.SetInternalVisitedColorIsCurrentColor(false);
}

const blink::Color InternalVisitedColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  if (style.ShouldForceColor(style.InternalVisitedColor())) {
    return To<Longhand>(GetCSSPropertyInternalForcedVisitedColor())
        .ColorIncludingFallback(true, style, is_current_color);
  }
  return style.GetInternalVisitedCurrentColor(is_current_color);
}

const CSSValue* InternalVisitedColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

const CSSValue* GridAutoColumns::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTrackList(
      range, context, css_parsing_utils::TrackListType::kGridAuto);
}

// Specs mention that getComputedStyle() should return the used value of the
// property instead of the computed one for grid-template-{rows|columns} but
// not for the grid-auto-{rows|columns} as things like grid-auto-columns:
// 2fr; cannot be resolved to a value in pixels as the '2fr' means very
// different things depending on the size of the explicit grid or the number
// of implicit tracks added to the grid. See
// http://lists.w3.org/Archives/Public/www-style/2013Nov/0014.html
const CSSValue* GridAutoColumns::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridAutoTrackList(kForColumns,
                                                       layout_object, style);
}

const CSSValue* GridAutoColumns::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kAuto);
}

const CSSValue* GridAutoFlow::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSIdentifierValue* row_or_column_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kRow, CSSValueID::kColumn>(
          range);
  CSSIdentifierValue* dense_algorithm =
      css_parsing_utils::ConsumeIdent<CSSValueID::kDense>(range);
  if (!row_or_column_value) {
    row_or_column_value =
        css_parsing_utils::ConsumeIdent<CSSValueID::kRow, CSSValueID::kColumn>(
            range);
    if (!row_or_column_value && !dense_algorithm) {
      return nullptr;
    }
  }
  CSSValueList* parsed_values = CSSValueList::CreateSpaceSeparated();
  if (row_or_column_value) {
    CSSValueID value = row_or_column_value->GetValueID();
    if (value == CSSValueID::kColumn ||
        (value == CSSValueID::kRow && !dense_algorithm)) {
      parsed_values->Append(*row_or_column_value);
    }
  }
  if (dense_algorithm) {
    parsed_values->Append(*dense_algorithm);
  }
  return parsed_values;
}

const CSSValue* GridAutoFlow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  switch (style.GetGridAutoFlow()) {
    case kAutoFlowRow:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kRow));
      break;
    case kAutoFlowColumn:
    case kAutoFlowColumnDense:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kColumn));
      break;
    default:
      // Do nothing.
      break;
  }

  switch (style.GetGridAutoFlow()) {
    case kAutoFlowRowDense:
    case kAutoFlowColumnDense:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kDense));
      break;
    default:
      // Do nothing.
      break;
  }

  return list;
}

const CSSValue* GridAutoFlow::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kRow);
}

const CSSValue* GridAutoRows::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTrackList(
      range, context, css_parsing_utils::TrackListType::kGridAuto);
}

const CSSValue* GridAutoRows::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridAutoTrackList(kForRows, layout_object,
                                                       style);
}

const CSSValue* GridAutoRows::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kAuto);
}

const CSSValue* GridColumnEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridLine(range, context);
}

const CSSValue* GridColumnEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridPosition(style.GridColumnEnd());
}

const CSSValue* GridColumnStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridLine(range, context);
}

const CSSValue* GridColumnStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridPosition(style.GridColumnStart());
}

const CSSValue* GridRowEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridLine(range, context);
}

const CSSValue* GridRowEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridPosition(style.GridRowEnd());
}

const CSSValue* GridRowStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridLine(range, context);
}

const CSSValue* GridRowStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridPosition(style.GridRowStart());
}

const CSSValue* GridTemplateAreas::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  NamedGridAreaMap grid_area_map;
  wtf_size_t row_count = 0;
  wtf_size_t column_count = 0;

  while (range.Peek().GetType() == kStringToken) {
    if (!css_parsing_utils::ParseGridTemplateAreasRow(
            range.ConsumeIncludingWhitespace().Value().ToString(),
            grid_area_map, row_count, column_count)) {
      return nullptr;
    }
    ++row_count;
  }

  if (row_count == 0) {
    return nullptr;
  }
  DCHECK(column_count);
  return MakeGarbageCollected<cssvalue::CSSGridTemplateAreasValue>(
      grid_area_map, row_count, column_count);
}

const CSSValue* GridTemplateAreas::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.NamedGridAreaRowCount()) {
    DCHECK(!style.NamedGridAreaColumnCount());
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  return MakeGarbageCollected<cssvalue::CSSGridTemplateAreasValue>(
      style.NamedGridArea(), style.NamedGridAreaRowCount(),
      style.NamedGridAreaColumnCount());
}

void GridTemplateAreas::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetImplicitNamedGridColumnLines(
      ComputedStyleInitialValues::InitialImplicitNamedGridColumnLines());
  builder.SetImplicitNamedGridRowLines(
      ComputedStyleInitialValues::InitialImplicitNamedGridRowLines());

  builder.SetNamedGridArea(ComputedStyleInitialValues::InitialNamedGridArea());
  builder.SetNamedGridAreaRowCount(
      ComputedStyleInitialValues::InitialNamedGridAreaRowCount());
  builder.SetNamedGridAreaColumnCount(
      ComputedStyleInitialValues::InitialNamedGridAreaColumnCount());
}

void GridTemplateAreas::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetImplicitNamedGridColumnLines(
      state.ParentStyle()->ImplicitNamedGridColumnLines());
  builder.SetImplicitNamedGridRowLines(
      state.ParentStyle()->ImplicitNamedGridRowLines());

  builder.SetNamedGridArea(state.ParentStyle()->NamedGridArea());
  builder.SetNamedGridAreaRowCount(
      state.ParentStyle()->NamedGridAreaRowCount());
  builder.SetNamedGridAreaColumnCount(
      state.ParentStyle()->NamedGridAreaColumnCount());
}

void GridTemplateAreas::ApplyValue(StyleResolverState& state,
                                   const CSSValue& value) const {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    ApplyInitial(state);
    return;
  }

  const auto& grid_template_areas_value =
      To<cssvalue::CSSGridTemplateAreasValue>(value);
  const NamedGridAreaMap& new_named_grid_areas =
      grid_template_areas_value.GridAreaMap();

  NamedGridLinesMap implicit_named_grid_column_lines;
  NamedGridLinesMap implicit_named_grid_row_lines;
  StyleBuilderConverter::CreateImplicitNamedGridLinesFromGridArea(
      new_named_grid_areas, implicit_named_grid_column_lines, kForColumns);
  StyleBuilderConverter::CreateImplicitNamedGridLinesFromGridArea(
      new_named_grid_areas, implicit_named_grid_row_lines, kForRows);

  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetImplicitNamedGridColumnLines(implicit_named_grid_column_lines);
  builder.SetImplicitNamedGridRowLines(implicit_named_grid_row_lines);

  builder.SetNamedGridArea(new_named_grid_areas);
  builder.SetNamedGridAreaRowCount(grid_template_areas_value.RowCount());
  builder.SetNamedGridAreaColumnCount(grid_template_areas_value.ColumnCount());
}

const CSSValue* GridTemplateAreas::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* GridTemplateColumns::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTemplatesRowsOrColumns(range, context);
}

bool GridTemplateColumns::IsLayoutDependent(const ComputedStyle* style,
                                            LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGridIncludingNG();
}

const CSSValue* GridTemplateColumns::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridTrackList(kForColumns, layout_object,
                                                   style);
}

const CSSValue* GridTemplateColumns::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* GridTemplateRows::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTemplatesRowsOrColumns(range, context);
}

bool GridTemplateRows::IsLayoutDependent(const ComputedStyle* style,
                                         LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGridIncludingNG();
}

const CSSValue* GridTemplateRows::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridTrackList(kForRows, layout_object,
                                                   style);
}

const CSSValue* GridTemplateRows::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* Height::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(
      range, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

bool Height::IsLayoutDependent(const ComputedStyle* style,
                               LayoutObject* layout_object) const {
  return layout_object && (layout_object->IsBox() || layout_object->IsSVG());
}

const CSSValue* Height::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (ComputedStyleUtils::WidthOrHeightShouldReturnUsedValue(layout_object)) {
    return ZoomAdjustedPixelValue(
        ComputedStyleUtils::UsedBoxSize(*layout_object).height(), style);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Height(),
                                                             style);
}

const CSSValue* HyphenateLimitChars::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const StyleHyphenateLimitChars& value = style.HyphenateLimitChars();
  if (value.IsAuto()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  AppendIntegerOrAutoIfZero(value.MinWordChars(), list);
  if (value.MinBeforeChars() || value.MinAfterChars()) {
    AppendIntegerOrAutoIfZero(value.MinBeforeChars(), list);
    if (value.MinAfterChars()) {
      list->Append(*CSSNumericLiteralValue::Create(
          value.MinAfterChars(), CSSPrimitiveValue::UnitType::kInteger));
    }
  }
  return list;
}

const CSSValue* HyphenateLimitChars::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeHyphenateLimitChars(range, context);
}

const CSSValue* Hyphens::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetHyphens());
}

const CSSValue* ImageOrientation::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kFromImage) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return nullptr;
}

const CSSValue* ImageOrientation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.RespectImageOrientation() == kRespectImageOrientation) {
    return CSSIdentifierValue::Create(CSSValueID::kFromImage);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* ImageRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ImageRendering());
}

const CSSValue* InitialLetter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const StyleInitialLetter initial_letter = style.InitialLetter();
  if (initial_letter.IsNormal()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSNumericLiteralValue::Create(
      initial_letter.Size(), CSSPrimitiveValue::UnitType::kNumber));
  if (initial_letter.IsIntegerSink()) {
    list->Append(*CSSNumericLiteralValue::Create(
        initial_letter.Sink(), CSSPrimitiveValue::UnitType::kInteger));
  } else if (initial_letter.IsDrop()) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kDrop));
  } else if (initial_letter.IsRaise()) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kRaise));
  }
  return list;
}

const CSSValue* InitialLetter::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeInitialLetter(range, context);
}

const CSSValue* InlineSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(range, context);
}

bool InlineSize::IsLayoutDependent(const ComputedStyle* style,
                                   LayoutObject* layout_object) const {
  return layout_object && (layout_object->IsBox() || layout_object->IsSVG());
}

const CSSValue* InsetBlockEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchor));
}

const CSSValue* InsetBlockStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchor));
}

const CSSValue* InsetInlineEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchor));
}

const CSSValue* InsetInlineStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchor));
}

const blink::Color InternalVisitedBackgroundColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);

  StyleColor visited_background_color = style.InternalVisitedBackgroundColor();
  if (style.ShouldForceColor(visited_background_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBackgroundColor())
        .ColorIncludingFallback(true, style, is_current_color);
  }
  blink::Color color = visited_background_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);

  // TODO: Technically someone could explicitly specify the color
  // transparent, but for now we'll just assume that if the background color
  // is transparent that it wasn't set. Note that it's weird that we're
  // returning unvisited info for a visited link, but given our restriction
  // that the alpha values have to match, it makes more sense to return the
  // unvisited background color if specified than it does to return black.
  // This behavior matches what Firefox 4 does as well.
  if (color == blink::Color::kTransparent) {
    // Overwrite is_current_color based on the unvisited background color.
    return style.BackgroundColor().Resolve(
        style.GetCurrentColor(), style.UsedColorScheme(), is_current_color);
  }

  return color;
}

const CSSValue* InternalVisitedBackgroundColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

const blink::Color InternalVisitedBorderLeftColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  StyleColor visited_border_left_color = style.InternalVisitedBorderLeftColor();
  if (style.ShouldForceColor(visited_border_left_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(true, style, is_current_color);
  }
  return visited_border_left_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedBorderLeftColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const blink::Color InternalVisitedBorderTopColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  StyleColor visited_border_top_color = style.InternalVisitedBorderTopColor();
  if (style.ShouldForceColor(visited_border_top_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(true, style, is_current_color);
  }
  return visited_border_top_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedBorderTopColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const blink::Color InternalVisitedCaretColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  StyleAutoColor auto_color = style.InternalVisitedCaretColor();
  StyleColor result = auto_color.IsAutoColor() ? StyleColor::CurrentColor()
                                               : auto_color.ToStyleColor();
  if (style.ShouldForceColor(result)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return result.Resolve(style.GetInternalVisitedCurrentColor(),
                        style.UsedColorScheme(), is_current_color);
}

const CSSValue* InternalVisitedCaretColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return To<Longhand>(GetCSSPropertyCaretColor())
      .ParseSingleValue(range, context, local_context);
}

const blink::Color InternalVisitedBorderRightColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  StyleColor visited_border_right_color =
      style.InternalVisitedBorderRightColor();
  if (style.ShouldForceColor(visited_border_right_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(true, style, is_current_color);
  }
  return visited_border_right_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedBorderRightColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const blink::Color InternalVisitedBorderBottomColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  StyleColor visited_border_bottom_color =
      style.InternalVisitedBorderBottomColor();
  if (style.ShouldForceColor(visited_border_bottom_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(true, style, is_current_color);
  }
  return visited_border_bottom_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedBorderBottomColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const CSSValue* InternalVisitedBorderInlineStartColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const CSSValue* InternalVisitedBorderInlineEndColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const CSSValue* InternalVisitedBorderBlockStartColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const CSSValue* InternalVisitedBorderBlockEndColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(range, context,
                                                   local_context);
}

const CSSValue* InternalVisitedFill::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeSVGPaint(range, context);
}

const blink::Color InternalVisitedFill::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  const SVGPaint& paint = style.InternalVisitedFillPaint();

  // FIXME: This code doesn't support the uri component of the visited link
  // paint, https://bugs.webkit.org/show_bug.cgi?id=70006
  if (!paint.HasColor()) {
    return To<Longhand>(GetCSSPropertyFill())
        .ColorIncludingFallback(false, style, is_current_color);
  }
  const StyleColor& visited_fill_color = paint.GetColor();
  if (style.ShouldForceColor(visited_fill_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_fill_color.Resolve(style.GetInternalVisitedCurrentColor(),
                                    style.UsedColorScheme(), is_current_color);
}

const blink::Color InternalVisitedColumnRuleColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  StyleColor visited_column_rule_color = style.InternalVisitedColumnRuleColor();
  if (style.ShouldForceColor(visited_column_rule_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_column_rule_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedColumnRuleColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color InternalVisitedOutlineColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  StyleColor visited_outline_color = style.InternalVisitedOutlineColor();
  if (style.ShouldForceColor(visited_outline_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedOutlineColor())
        .ColorIncludingFallback(true, style, is_current_color);
  }
  return visited_outline_color.Resolve(style.GetInternalVisitedCurrentColor(),
                                       style.UsedColorScheme(),
                                       is_current_color);
}

const CSSValue* InternalVisitedOutlineColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return To<Longhand>(GetCSSPropertyOutlineColor())
      .ParseSingleValue(range, context, local_context);
}

const CSSValue* InternalVisitedStroke::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeSVGPaint(range, context);
}

const blink::Color InternalVisitedStroke::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  const SVGPaint& paint = style.InternalVisitedStrokePaint();

  // FIXME: This code doesn't support the uri component of the visited link
  // paint, https://bugs.webkit.org/show_bug.cgi?id=70006
  if (!paint.HasColor()) {
    return To<Longhand>(GetCSSPropertyStroke())
        .ColorIncludingFallback(false, style, is_current_color);
  }
  const StyleColor& visited_stroke_color = paint.GetColor();
  if (style.ShouldForceColor(visited_stroke_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_stroke_color.Resolve(style.GetInternalVisitedCurrentColor(),
                                      style.UsedColorScheme(),
                                      is_current_color);
}

const blink::Color InternalVisitedTextDecorationColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  StyleColor visited_decoration_color =
      style.DecorationColorIncludingFallback(visited_link);
  if (style.ShouldForceColor(visited_decoration_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_decoration_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedTextDecorationColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color InternalVisitedTextEmphasisColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  StyleColor visited_text_emphasis_color =
      style.InternalVisitedTextEmphasisColor();
  if (style.ShouldForceColor(visited_text_emphasis_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_text_emphasis_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedTextEmphasisColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color InternalVisitedTextFillColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  StyleColor visited_text_fill_color = style.InternalVisitedTextFillColor();
  if (style.ShouldForceColor(visited_text_fill_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_text_fill_color.Resolve(style.GetInternalVisitedCurrentColor(),
                                         style.UsedColorScheme(),
                                         is_current_color);
}

const CSSValue* InternalVisitedTextFillColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color InternalVisitedTextStrokeColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  StyleColor visited_text_stroke_color = style.InternalVisitedTextStrokeColor();
  if (style.ShouldForceColor(visited_text_stroke_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_text_stroke_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedTextStrokeColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color InternalForcedBackgroundColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  blink::Color forced_current_color;
  int alpha;
  bool alpha_is_current_color;
  if (visited_link) {
    forced_current_color = style.GetInternalForcedVisitedCurrentColor(
        /* No is_current_color because we might not be forced_current_color */);
    alpha = style.InternalVisitedBackgroundColor()
                .Resolve(style.GetInternalVisitedCurrentColor(),
                         style.UsedColorScheme(), &alpha_is_current_color)
                .Alpha();
  } else {
    forced_current_color = style.GetInternalForcedCurrentColor(
        /* No is_current_color because we might not be forced_current_color */);
    alpha = style.BackgroundColor()
                .Resolve(style.GetCurrentColor(), style.UsedColorScheme(),
                         &alpha_is_current_color)
                .Alpha();
  }

  bool result_is_current_color;
  blink::Color result = style.InternalForcedBackgroundColor().ResolveWithAlpha(
      forced_current_color, style.UsedColorScheme(), alpha,
      &result_is_current_color, /* is_forced_color */ true);

  if (is_current_color) {
    *is_current_color = alpha_is_current_color || result_is_current_color;
  }
  return result;
}

const CSSValue*
InternalForcedBackgroundColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  bool visited_link = allow_visited_style &&
                      style.InsideLink() == EInsideLink::kInsideVisitedLink;
  return cssvalue::CSSColor::Create(
      ColorIncludingFallback(visited_link, style));
}

const CSSValue* InternalForcedBackgroundColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

const blink::Color InternalForcedBorderColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  // Don’t pass is_current_color here because we might not be current_color
  blink::Color current_color =
      visited_link ? style.GetInternalForcedVisitedCurrentColor()
                   : style.GetInternalForcedCurrentColor();

  return style.InternalForcedBorderColor().Resolve(
      current_color, style.UsedColorScheme(), is_current_color);
}

const CSSValue* InternalForcedBorderColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  bool visited_link = allow_visited_style &&
                      style.InsideLink() == EInsideLink::kInsideVisitedLink;
  return cssvalue::CSSColor::Create(
      ColorIncludingFallback(visited_link, style));
}

const CSSValue* InternalForcedBorderColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

void InternalForcedColor::ApplyInitial(StyleResolverState& state) const {
  state.StyleBuilder().SetInternalForcedColor(
      ComputedStyleInitialValues::InitialInternalForcedColor());
}

void InternalForcedColor::ApplyInherit(StyleResolverState& state) const {
  state.StyleBuilder().SetInternalForcedColor(
      state.ParentStyle()->InternalForcedColor());
}

void InternalForcedColor::ApplyValue(StyleResolverState& state,
                                     const CSSValue& value) const {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kCurrentcolor) {
    ApplyInherit(state);
    return;
  }
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (value.IsInitialColorValue()) {
    DCHECK_EQ(state.GetElement(), state.GetDocument().documentElement());
    builder.SetInternalForcedColor(
        ComputedStyleInitialValues::InitialInternalForcedColor());
    return;
  }
  builder.SetInternalForcedColor(
      StyleBuilderConverter::ConvertStyleColor(state, value));
}

const blink::Color InternalForcedColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  return style.GetInternalForcedCurrentColor(is_current_color);
}

const CSSValue* InternalForcedColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return cssvalue::CSSColor::Create(
      allow_visited_style ? style.VisitedDependentColor(*this)
                          : style.GetInternalForcedCurrentColor());
}

const CSSValue* InternalForcedColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

const blink::Color InternalForcedOutlineColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  // No is_current_color here because we might not be current_color
  blink::Color current_color =
      visited_link ? style.GetInternalForcedVisitedCurrentColor()
                   : style.GetInternalForcedCurrentColor();

  return style.InternalForcedOutlineColor().Resolve(
      current_color, style.UsedColorScheme(), is_current_color);
}

const CSSValue* InternalForcedOutlineColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  bool visited_link = allow_visited_style &&
                      style.InsideLink() == EInsideLink::kInsideVisitedLink;
  return cssvalue::CSSColor::Create(
      ColorIncludingFallback(visited_link, style));
}

const CSSValue* InternalForcedOutlineColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

void InternalForcedVisitedColor::ApplyInitial(StyleResolverState& state) const {
  state.StyleBuilder().SetInternalForcedVisitedColor(
      ComputedStyleInitialValues::InitialInternalForcedVisitedColor());
}

void InternalForcedVisitedColor::ApplyInherit(StyleResolverState& state) const {
  auto color = state.ParentStyle()->InternalForcedVisitedColor();
  state.StyleBuilder().SetInternalForcedVisitedColor(color);
}

void InternalForcedVisitedColor::ApplyValue(StyleResolverState& state,
                                            const CSSValue& value) const {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kCurrentcolor) {
    ApplyInherit(state);
    return;
  }
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (value.IsInitialColorValue()) {
    DCHECK_EQ(state.GetElement(), state.GetDocument().documentElement());
    builder.SetInternalForcedVisitedColor(
        ComputedStyleInitialValues::InitialInternalForcedVisitedColor());
    return;
  }
  builder.SetInternalForcedVisitedColor(
      StyleBuilderConverter::ConvertStyleColor(state, value, true));
}

const blink::Color InternalForcedVisitedColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  return style.GetInternalForcedVisitedCurrentColor(is_current_color);
}

const CSSValue* InternalForcedVisitedColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

const CSSValue* Isolation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Isolation());
}

const CSSValue* JustifyContent::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // justify-content property does not allow the <baseline-position> values.
  if (css_parsing_utils::IdentMatches<CSSValueID::kFirst, CSSValueID::kLast,
                                      CSSValueID::kBaseline>(
          range.Peek().Id())) {
    return nullptr;
  }
  return css_parsing_utils::ConsumeContentDistributionOverflowPosition(
      range, css_parsing_utils::IsContentPositionOrLeftOrRightKeyword);
}

const CSSValue* JustifyContent::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::
      ValueForContentPositionAndDistributionWithOverflowAlignment(
          style.JustifyContent());
}

const CSSValue* JustifyItems::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSParserTokenRange range_copy = range;
  // justify-items property does not allow the 'auto' value.
  if (css_parsing_utils::IdentMatches<CSSValueID::kAuto>(range.Peek().Id())) {
    return nullptr;
  }
  CSSIdentifierValue* legacy =
      css_parsing_utils::ConsumeIdent<CSSValueID::kLegacy>(range_copy);
  CSSIdentifierValue* position_keyword =
      css_parsing_utils::ConsumeIdent<CSSValueID::kCenter, CSSValueID::kLeft,
                                      CSSValueID::kRight>(range_copy);
  if (!legacy) {
    legacy = css_parsing_utils::ConsumeIdent<CSSValueID::kLegacy>(range_copy);
  }
  if (legacy) {
    range = range_copy;
    if (position_keyword) {
      context.Count(WebFeature::kCSSLegacyAlignment);
      return MakeGarbageCollected<CSSValuePair>(
          legacy, position_keyword, CSSValuePair::kDropIdenticalValues);
    }
    return legacy;
  }

  return css_parsing_utils::ConsumeSelfPositionOverflowPosition(
      range, css_parsing_utils::IsSelfPositionOrLeftOrRightKeyword);
}

const CSSValue* JustifyItems::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForItemPositionWithOverflowAlignment(
      style.JustifyItems().GetPosition() == ItemPosition::kAuto
          ? ComputedStyleInitialValues::InitialDefaultAlignment()
          : style.JustifyItems());
}

const CSSValue* JustifySelf::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSelfPositionOverflowPosition(
      range, css_parsing_utils::IsSelfPositionOrLeftOrRightKeyword);
}

const CSSValue* JustifySelf::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForItemPositionWithOverflowAlignment(
      style.JustifySelf());
}

const CSSValue* Left::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessUnlessShorthand(local_context),
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchor));
}

bool Left::IsLayoutDependent(const ComputedStyle* style,
                             LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Left::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForPositionOffset(style, *this,
                                                    layout_object);
}

const CSSValue* LetterSpacing::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseSpacing(range, context);
}

const CSSValue* LetterSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.LetterSpacing()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
  return ZoomAdjustedPixelValue(style.LetterSpacing(), style);
}

const CSSValue* LightingColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color LightingColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  StyleColor lighting_color = style.LightingColor();
  if (style.ShouldForceColor(lighting_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return style.ResolvedColor(lighting_color, is_current_color);
}

const CSSValue* LightingColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.LightingColor(), CSSValuePhase::kComputedValue);
}

const CSSValue* LineBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetLineBreak());
}

const CSSValue* LineHeight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLineHeight(range, context);
}

const CSSValue* LineHeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForLineHeight(style);
}

const CSSValue* ListStyleImage::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeImageOrNone(range, context);
}

const CSSValue* ListStyleImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.ListStyleImage()) {
    return style.ListStyleImage()->ComputedCSSValue(style, allow_visited_style);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

void ListStyleImage::ApplyValue(StyleResolverState& state,
                                const CSSValue& value) const {
  state.StyleBuilder().SetListStyleImage(
      state.GetStyleImage(CSSPropertyID::kListStyleImage, value));
}

const CSSValue* ListStylePosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ListStylePosition());
}

const CSSValue* ListStyleType::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (auto* none = css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(range)) {
    return none;
  }

  if (auto* counter_style_name =
          css_parsing_utils::ConsumeCounterStyleName(range, context)) {
    return counter_style_name;
  }

  return css_parsing_utils::ConsumeString(range);
}

const CSSValue* ListStyleType::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.ListStyleType()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  const ListStyleTypeData& list_style_type = *style.ListStyleType();
  if (list_style_type.IsString()) {
    return MakeGarbageCollected<CSSStringValue>(
        list_style_type.GetStringValue());
  }
  // TODO(crbug.com/687225): Return a scoped CSSValue?
  return MakeGarbageCollected<CSSCustomIdentValue>(
      list_style_type.GetCounterStyleName());
}

void ListStyleType::ApplyValue(StyleResolverState& state,
                               const CSSValue& value) const {
  DCHECK(value.IsScopedValue());
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(CSSValueID::kNone, identifier_value->GetValueID());
    builder.SetListStyleType(nullptr);
    return;
  }

  if (const auto* string_value = DynamicTo<CSSStringValue>(value)) {
    builder.SetListStyleType(
        ListStyleTypeData::CreateString(AtomicString(string_value->Value())));
    return;
  }

  DCHECK(value.IsCustomIdentValue());
  const auto& custom_ident_value = To<CSSCustomIdentValue>(value);
  builder.SetListStyleType(ListStyleTypeData::CreateCounterStyle(
      custom_ident_value.Value(), custom_ident_value.GetTreeScope()));
}

bool MarginBlockEnd::IsLayoutDependent(const ComputedStyle* style,
                                       LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* MarginBlockEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

bool MarginBlockStart::IsLayoutDependent(const ComputedStyle* style,
                                         LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* MarginBlockStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* MarginBottom::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

bool MarginBottom::IsLayoutDependent(const ComputedStyle* style,
                                     LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->MarginBottom().IsFixed());
}

const CSSValue* MarginBottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const Length& margin_bottom = style.MarginBottom();
  if (margin_bottom.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(margin_bottom,
                                                               style);
  }
  return ZoomAdjustedPixelValue(To<LayoutBox>(layout_object)->MarginBottom(),
                                style);
}

bool MarginInlineEnd::IsLayoutDependent(const ComputedStyle* style,
                                        LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* MarginInlineEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

bool MarginInlineStart::IsLayoutDependent(const ComputedStyle* style,
                                          LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* MarginInlineStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* MarginLeft::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

bool MarginLeft::IsLayoutDependent(const ComputedStyle* style,
                                   LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->MarginLeft().IsFixed());
}

const CSSValue* MarginLeft::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const Length& margin_left = style.MarginLeft();
  if (margin_left.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(margin_left,
                                                               style);
  }
  return ZoomAdjustedPixelValue(To<LayoutBox>(layout_object)->MarginLeft(),
                                style);
}

const CSSValue* MarginRight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

bool MarginRight::IsLayoutDependent(const ComputedStyle* style,
                                    LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->MarginRight().IsFixed());
}

const CSSValue* MarginRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const Length& margin_right = style.MarginRight();
  if (margin_right.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(margin_right,
                                                               style);
  }
  float value;
  const auto& box = *To<LayoutBox>(layout_object);
  if (margin_right.IsPercentOrCalc()) {
    // LayoutBox gives a marginRight() that is the distance between the
    // right-edge of the child box and the right-edge of the containing box,
    // when display == EDisplay::kBlock. Let's calculate the absolute value
    // of the specified margin-right % instead of relying on LayoutBox's
    // marginRight() value.
    value = MinimumValueForLength(margin_right,
                                  box.ContainingBlockLogicalWidthForContent())
                .ToFloat();
  } else {
    value = box.MarginRight().ToFloat();
  }
  return ZoomAdjustedPixelValue(value, style);
}

const CSSValue* MarginTop::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

bool MarginTop::IsLayoutDependent(const ComputedStyle* style,
                                  LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->MarginTop().IsFixed());
}

const CSSValue* MarginTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const Length& margin_top = style.MarginTop();
  if (margin_top.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(margin_top,
                                                               style);
  }
  return ZoomAdjustedPixelValue(To<LayoutBox>(layout_object)->MarginTop(),
                                style);
}

const CSSValue* MarkerEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeUrl(range, context);
}

const CSSValue* MarkerEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGResource(style.MarkerEndResource());
}

const CSSValue* MarkerMid::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeUrl(range, context);
}

const CSSValue* MarkerMid::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGResource(style.MarkerMidResource());
}

const CSSValue* MarkerStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeUrl(range, context);
}

const CSSValue* MarkerStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGResource(style.MarkerStartResource());
}

const CSSValue* Mask::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeUrl(range, context);
}

const CSSValue* Mask::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGResource(style.MaskerResource());
}

const CSSValue* MaskType::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.MaskType());
}

const CSSValue* MathShift::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.MathShift());
}

const CSSValue* MathStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.MathStyle());
}

const CSSValue* MathDepth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMathDepth(range, context);
}

const CSSValue* MathDepth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.MathDepth(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

void MathDepth::ApplyValue(StyleResolverState& state,
                           const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    DCHECK_EQ(list->length(), 1U);
    const auto& relative_value = To<CSSPrimitiveValue>(list->Item(0));
    builder.SetMathDepth(base::ClampAdd(state.ParentStyle()->MathDepth(),
                                        relative_value.GetIntValue()));
  } else if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK(identifier_value->GetValueID() == CSSValueID::kAutoAdd);
    int16_t depth = 0;
    if (state.ParentStyle()->MathStyle() == EMathStyle::kCompact) {
      depth += 1;
    }
    builder.SetMathDepth(
        base::ClampAdd(state.ParentStyle()->MathDepth(), depth));
  } else if (DynamicTo<CSSPrimitiveValue>(value)) {
    builder.SetMathDepth(
        ClampTo<int16_t>(To<CSSPrimitiveValue>(value).GetIntValue()));
  }
}

const CSSValue* MaxBlockSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMaxWidthOrHeight(range, context);
}

const CSSValue* MaxHeight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMaxWidthOrHeight(
      range, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* MaxHeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const Length& max_height = style.MaxHeight();
  if (max_height.IsNone()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(max_height, style);
}

const CSSValue* MaxInlineSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMaxWidthOrHeight(range, context);
}

const CSSValue* MaxWidth::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMaxWidthOrHeight(
      range, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* MaxWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const Length& max_width = style.MaxWidth();
  if (max_width.IsNone()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(max_width, style);
}

const CSSValue* MinBlockSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(range, context);
}

const CSSValue* MinHeight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(
      range, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* MinHeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (style.MinHeight().IsAuto()) {
    return ComputedStyleUtils::MinWidthOrMinHeightAuto(style);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.MinHeight(),
                                                             style);
}

const CSSValue* MinInlineSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(range, context);
}

const CSSValue* MinWidth::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(
      range, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* MinWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (style.MinWidth().IsAuto()) {
    return ComputedStyleUtils::MinWidthOrMinHeightAuto(style);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.MinWidth(),
                                                             style);
}

const CSSValue* MixBlendMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetBlendMode());
}

const CSSValue* ObjectFit::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetObjectFit());
}

const CSSValue* ObjectPosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumePosition(range, context,
                         css_parsing_utils::UnitlessQuirk::kForbid,
                         absl::optional<WebFeature>());
}

const CSSValue* ObjectPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return MakeGarbageCollected<CSSValuePair>(
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.ObjectPosition().X(), style),
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.ObjectPosition().Y(), style),
      CSSValuePair::kKeepIdenticalValues);
}

const CSSValue* ObjectViewBox::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSObjectViewBoxEnabled());

  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  auto* css_value = css_parsing_utils::ConsumeBasicShape(
      range, context, css_parsing_utils::AllowPathValue::kForbid,
      css_parsing_utils::AllowBasicShapeRectValue::kAllow,
      css_parsing_utils::AllowBasicShapeXYWHValue::kAllow);

  if (!css_value || css_value->IsBasicShapeInsetValue() ||
      css_value->IsBasicShapeRectValue() ||
      css_value->IsBasicShapeXYWHValue()) {
    return css_value;
  }

  return nullptr;
}

const CSSValue* ObjectViewBox::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!RuntimeEnabledFeatures::CSSObjectViewBoxEnabled()) {
    DCHECK(!style.ObjectViewBox());
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  if (auto* basic_shape = style.ObjectViewBox()) {
    return ValueForBasicShape(style, basic_shape);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* OffsetAnchor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumePosition(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid,
      absl::optional<WebFeature>());
}

const CSSValue* OffsetAnchor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForPosition(style.OffsetAnchor(), style);
}

const CSSValue* OffsetDistance::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* OffsetDistance::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.OffsetDistance(), style);
}

const CSSValue* OffsetPath::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeOffsetPath(range, context);
}

const CSSValue* OffsetPath::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (const BasicShape* style_motion_path = style.OffsetPath()) {
    return ValueForBasicShape(style, style_motion_path);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* OffsetPosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  CSSValue* value = css_parsing_utils::ConsumePosition(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid,
      absl::optional<WebFeature>());

  // Count when we receive a valid position other than 'auto'.
  if (value && value->IsValuePair()) {
    context.Count(WebFeature::kCSSOffsetInEffect);
  }
  return value;
}

const CSSValue* OffsetPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForPosition(style.OffsetPosition(), style);
}

const CSSValue* OffsetRotate::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeOffsetRotate(range, context);
}
const CSSValue* OffsetRotate::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (style.OffsetRotate().type == OffsetRotationType::kAuto) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  }
  list->Append(*CSSNumericLiteralValue::Create(
      style.OffsetRotate().angle, CSSPrimitiveValue::UnitType::kDegrees));
  return list;
}

const CSSValue* Opacity::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(range, context);
}

const CSSValue* Opacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.Opacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* Order::ParseSingleValue(CSSParserTokenRange& range,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeInteger(range, context);
}

const CSSValue* Order::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.Order(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* OriginTrialTestProperty::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OriginTrialTestProperty());
  ;
}

const CSSValue* Orphans::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositiveInteger(range, context);
}

const CSSValue* Orphans::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.Orphans(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* OutlineColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // Allow the special focus color even in HTML Standard parsing mode.
  if (range.Peek().Id() == CSSValueID::kWebkitFocusRingColor) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeColor(range, context);
}

const CSSValue* AccentColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeColor(range, context);
}

const CSSValue* AccentColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  StyleAutoColor auto_color = style.AccentColor();
  if (auto_color.IsAutoColor()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  return ComputedStyleUtils::ValueForStyleAutoColor(
      style, style.AccentColor(), CSSValuePhase::kComputedValue);
}

const blink::Color OutlineColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  StyleColor outline_color = style.OutlineColor();
  if (style.ShouldForceColor(outline_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedOutlineColor())
        .ColorIncludingFallback(false, style, is_current_color);
  }
  return outline_color.Resolve(style.GetCurrentColor(), style.UsedColorScheme(),
                               is_current_color);
}

const CSSValue* OutlineColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  StyleColor outline_color = style.OutlineColor();
  if (style.ShouldForceColor(outline_color)) {
    return GetCSSPropertyInternalForcedOutlineColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style);
  }
  // https://drafts.csswg.org/cssom/#resolved-values
  // For this property, the resolved value is the used value.
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, outline_color, CSSValuePhase::kUsedValue);
}

const CSSValue* OutlineOffset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(range, context,
                                          CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* OutlineOffset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.OutlineOffset(), style);
}

const CSSValue* OutlineStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.OutlineStyleIsAuto()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return CSSIdentifierValue::Create(style.OutlineStyle());
}

void OutlineStyle::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetOutlineStyleIsAuto(
      ComputedStyleInitialValues::InitialOutlineStyleIsAuto());
  builder.SetOutlineStyle(EBorderStyle::kNone);
}

void OutlineStyle::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetOutlineStyleIsAuto(state.ParentStyle()->OutlineStyleIsAuto());
  builder.SetOutlineStyle(state.ParentStyle()->OutlineStyle());
}

void OutlineStyle::ApplyValue(StyleResolverState& state,
                              const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  const auto& identifier_value = To<CSSIdentifierValue>(value);
  builder.SetOutlineStyleIsAuto(
      static_cast<bool>(identifier_value.ConvertTo<OutlineIsAuto>()));
  builder.SetOutlineStyle(identifier_value.ConvertTo<EBorderStyle>());
}

const CSSValue* OutlineWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLineWidth(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* OutlineWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.OutlineWidth(), style);
}

const CSSValue* OverflowAnchor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverflowAnchor());
}

const CSSValue* OverflowClipMargin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.OverflowClipMargin()) {
    return CSSPrimitiveValue::CreateFromLength(Length::Fixed(0), 1.f);
  }

  if (style.OverflowClipMargin()->GetReferenceBox() ==
          StyleOverflowClipMargin::ReferenceBox::kPaddingBox &&
      style.OverflowClipMargin()->GetMargin() == LayoutUnit()) {
    return CSSPrimitiveValue::CreateFromLength(Length::Fixed(0), 1.f);
  }

  CSSValueID reference_box;
  switch (style.OverflowClipMargin()->GetReferenceBox()) {
    case StyleOverflowClipMargin::ReferenceBox::kBorderBox:
      reference_box = CSSValueID::kBorderBox;
      break;
    case StyleOverflowClipMargin::ReferenceBox::kContentBox:
      reference_box = CSSValueID::kContentBox;
      break;
    case StyleOverflowClipMargin::ReferenceBox::kPaddingBox:
      reference_box = CSSValueID::kPaddingBox;
      break;
  }

  auto* css_value_list = CSSValueList::CreateSpaceSeparated();
  if (reference_box != CSSValueID::kPaddingBox) {
    css_value_list->Append(*CSSIdentifierValue::Create(reference_box));
  }
  if (style.OverflowClipMargin()->GetMargin() != LayoutUnit()) {
    css_value_list->Append(*ZoomAdjustedPixelValue(
        style.OverflowClipMargin()->GetMargin(), style));
  }

  DCHECK_GT(css_value_list->length(), 0u);
  return css_value_list;
}

const CSSValue* OverflowClipMargin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSPrimitiveValue* length;
  CSSIdentifierValue* reference_box;

  if (range.Peek().GetType() != kIdentToken &&
      range.Peek().GetType() != kDimensionToken) {
    return nullptr;
  }

  if (range.Peek().GetType() == kIdentToken) {
    reference_box = css_parsing_utils::ConsumeVisualBox(range);
    length = css_parsing_utils::ConsumeLength(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  } else {
    length = css_parsing_utils::ConsumeLength(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    reference_box = css_parsing_utils::ConsumeVisualBox(range);
  }

  // At least one of |reference_box| and |length| must be provided.
  if (!reference_box && !length) {
    return nullptr;
  }

  if (reference_box && reference_box->GetValueID() == CSSValueID::kPaddingBox) {
    reference_box = nullptr;
    if (!length) {
      length = CSSPrimitiveValue::CreateFromLength(Length::Fixed(0), 1.f);
    }
  } else if (reference_box && length && length->IsZero()) {
    length = nullptr;
  }

  auto* css_value_list = CSSValueList::CreateSpaceSeparated();
  if (reference_box) {
    css_value_list->Append(*reference_box);
  }
  if (length) {
    css_value_list->Append(*length);
  }
  return css_value_list;
}

const CSSValue* OverflowWrap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverflowWrap());
}

const CSSValue* OverflowX::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverflowX());
}

void OverflowX::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetOverflowX(ComputedStyleInitialValues::InitialOverflowX());

  DCHECK_EQ(builder.OverflowX(), EOverflow::kVisible);
  builder.SetHasExplicitOverflowXVisible();
}

void OverflowX::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  auto parent_value = state.ParentStyle()->OverflowX();
  builder.SetOverflowX(parent_value);

  if (parent_value == EOverflow::kVisible) {
    builder.SetHasExplicitOverflowXVisible();
  }
}

void OverflowX::ApplyValue(StyleResolverState& state,
                           const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  auto converted_value =
      To<CSSIdentifierValue>(value).ConvertTo<blink::EOverflow>();
  builder.SetOverflowX(converted_value);

  if (converted_value == EOverflow::kVisible) {
    builder.SetHasExplicitOverflowXVisible();
  }
}

const CSSValue* OverflowY::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverflowY());
}

void OverflowY::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetOverflowY(ComputedStyleInitialValues::InitialOverflowY());

  DCHECK_EQ(builder.OverflowY(), EOverflow::kVisible);
  builder.SetHasExplicitOverflowYVisible();
}

void OverflowY::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  auto parent_value = state.ParentStyle()->OverflowY();
  builder.SetOverflowY(parent_value);

  if (parent_value == EOverflow::kVisible) {
    builder.SetHasExplicitOverflowYVisible();
  }
}

void OverflowY::ApplyValue(StyleResolverState& state,
                           const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  auto converted_value =
      To<CSSIdentifierValue>(value).ConvertTo<blink::EOverflow>();
  builder.SetOverflowY(converted_value);

  if (converted_value == EOverflow::kVisible) {
    builder.SetHasExplicitOverflowYVisible();
  }
}

const CSSValue* OverscrollBehaviorX::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverscrollBehaviorX());
}

const CSSValue* OverscrollBehaviorY::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverscrollBehaviorY());
}

bool PaddingBlockEnd::IsLayoutDependent(const ComputedStyle* style,
                                        LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* PaddingBlockEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kForbid);
}

bool PaddingBlockStart::IsLayoutDependent(const ComputedStyle* style,
                                          LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* PaddingBlockStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* PaddingBottom::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kAllow);
}

bool PaddingBottom::IsLayoutDependent(const ComputedStyle* style,
                                      LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingBottom().IsFixed());
}

const CSSValue* PaddingBottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const Length& padding_bottom = style.PaddingBottom();
  if (padding_bottom.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(padding_bottom,
                                                               style);
  }
  return ZoomAdjustedPixelValue(
      To<LayoutBox>(layout_object)->ComputedCSSPaddingBottom(), style);
}

bool PaddingInlineEnd::IsLayoutDependent(const ComputedStyle* style,
                                         LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* PaddingInlineEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kForbid);
}

bool PaddingInlineStart::IsLayoutDependent(const ComputedStyle* style,
                                           LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* PaddingInlineStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* PaddingLeft::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kAllow);
}

bool PaddingLeft::IsLayoutDependent(const ComputedStyle* style,
                                    LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingLeft().IsFixed());
}

const CSSValue* PaddingLeft::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const Length& padding_left = style.PaddingLeft();
  if (padding_left.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(padding_left,
                                                               style);
  }
  return ZoomAdjustedPixelValue(
      To<LayoutBox>(layout_object)->ComputedCSSPaddingLeft(), style);
}

const CSSValue* PaddingRight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kAllow);
}

bool PaddingRight::IsLayoutDependent(const ComputedStyle* style,
                                     LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingRight().IsFixed());
}

const CSSValue* PaddingRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const Length& padding_right = style.PaddingRight();
  if (padding_right.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(padding_right,
                                                               style);
  }
  return ZoomAdjustedPixelValue(
      To<LayoutBox>(layout_object)->ComputedCSSPaddingRight(), style);
}

const CSSValue* PaddingTop::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kAllow);
}

bool PaddingTop::IsLayoutDependent(const ComputedStyle* style,
                                   LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingTop().IsFixed());
}

const CSSValue* PaddingTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const Length& padding_top = style.PaddingTop();
  if (padding_top.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(padding_top,
                                                               style);
  }
  return ZoomAdjustedPixelValue(
      To<LayoutBox>(layout_object)->ComputedCSSPaddingTop(), style);
}

const CSSValue* Page::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeCustomIdent(range, context);
}

const CSSValue* Page::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.Page().IsNull()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(style.Page());
}

const CSSValue* ViewTransitionName::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeCustomIdent(range, context);
}

const CSSValue* ViewTransitionName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.ViewTransitionName().IsNull()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(style.ViewTransitionName());
}

const CSSValue* PaintOrder::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  Vector<CSSValueID, 3> paint_type_list;
  CSSIdentifierValue* fill = nullptr;
  CSSIdentifierValue* stroke = nullptr;
  CSSIdentifierValue* markers = nullptr;
  do {
    CSSValueID id = range.Peek().Id();
    if (id == CSSValueID::kFill && !fill) {
      fill = css_parsing_utils::ConsumeIdent(range);
    } else if (id == CSSValueID::kStroke && !stroke) {
      stroke = css_parsing_utils::ConsumeIdent(range);
    } else if (id == CSSValueID::kMarkers && !markers) {
      markers = css_parsing_utils::ConsumeIdent(range);
    } else {
      return nullptr;
    }
    paint_type_list.push_back(id);
  } while (!range.AtEnd());

  // After parsing we serialize the paint-order list. Since it is not possible
  // to pop a last list items from CSSValueList without bigger cost, we create
  // the list after parsing.
  CSSValueID first_paint_order_type = paint_type_list.at(0);
  CSSValueList* paint_order_list = CSSValueList::CreateSpaceSeparated();
  switch (first_paint_order_type) {
    case CSSValueID::kFill:
    case CSSValueID::kStroke:
      paint_order_list->Append(
          first_paint_order_type == CSSValueID::kFill ? *fill : *stroke);
      if (paint_type_list.size() > 1) {
        if (paint_type_list.at(1) == CSSValueID::kMarkers) {
          paint_order_list->Append(*markers);
        }
      }
      break;
    case CSSValueID::kMarkers:
      paint_order_list->Append(*markers);
      if (paint_type_list.size() > 1) {
        if (paint_type_list.at(1) == CSSValueID::kStroke) {
          paint_order_list->Append(*stroke);
        }
      }
      break;
    default:
      NOTREACHED();
  }

  return paint_order_list;
}

const CSSValue* PaintOrder::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const EPaintOrder paint_order = style.PaintOrder();
  if (paint_order == kPaintOrderNormal) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  // Table mapping to the shortest (canonical) form of the property.
  //
  // Per spec, if any keyword is omitted it will be added last using
  // the standard ordering. So "stroke" implies an order "stroke fill
  // markers" etc. From a serialization PoV this means we never need
  // to emit the last keyword.
  //
  // https://svgwg.org/svg2-draft/painting.html#PaintOrder
  static const uint8_t canonical_form[][2] = {
      // kPaintOrderNormal is handled above.
      {PT_FILL, PT_NONE},       // kPaintOrderFillStrokeMarkers
      {PT_FILL, PT_MARKERS},    // kPaintOrderFillMarkersStroke
      {PT_STROKE, PT_NONE},     // kPaintOrderStrokeFillMarkers
      {PT_STROKE, PT_MARKERS},  // kPaintOrderStrokeMarkersFill
      {PT_MARKERS, PT_NONE},    // kPaintOrderMarkersFillStroke
      {PT_MARKERS, PT_STROKE},  // kPaintOrderMarkersStrokeFill
  };
  DCHECK_LT(static_cast<size_t>(paint_order) - 1, std::size(canonical_form));
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (const auto& keyword : canonical_form[paint_order - 1]) {
    const auto paint_order_type = static_cast<EPaintOrderType>(keyword);
    if (paint_order_type == PT_NONE) {
      break;
    }
    list->Append(*CSSIdentifierValue::Create(paint_order_type));
  }
  return list;
}

const CSSValue* Perspective::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& localContext) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  CSSPrimitiveValue* parsed_value = css_parsing_utils::ConsumeLength(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  bool use_legacy_parsing = localContext.UseAliasParsing();
  if (!parsed_value && use_legacy_parsing) {
    double perspective;
    if (!css_parsing_utils::ConsumeNumberRaw(range, context, perspective) ||
        perspective < 0.0) {
      return nullptr;
    }
    context.Count(WebFeature::kUnitlessPerspectiveInPerspectiveProperty);
    parsed_value = CSSNumericLiteralValue::Create(
        perspective, CSSPrimitiveValue::UnitType::kPixels);
  }
  return parsed_value;
}

const CSSValue* Perspective::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.HasPerspective()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return ZoomAdjustedPixelValue(style.Perspective(), style);
}

const CSSValue* PerspectiveOrigin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumePosition(range, context,
                         css_parsing_utils::UnitlessQuirk::kForbid,
                         absl::optional<WebFeature>());
}

bool PerspectiveOrigin::IsLayoutDependent(const ComputedStyle* style,
                                          LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* PerspectiveOrigin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (layout_object) {
    LayoutRect box;
    if (layout_object->IsBox()) {
      box = To<LayoutBox>(layout_object)->BorderBoxRect();
    }

    return MakeGarbageCollected<CSSValuePair>(
        ZoomAdjustedPixelValue(
            MinimumValueForLength(style.PerspectiveOrigin().X(), box.Width()),
            style),
        ZoomAdjustedPixelValue(
            MinimumValueForLength(style.PerspectiveOrigin().Y(), box.Height()),
            style),
        CSSValuePair::kKeepIdenticalValues);
  } else {
    return MakeGarbageCollected<CSSValuePair>(
        ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            style.PerspectiveOrigin().X(), style),
        ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            style.PerspectiveOrigin().Y(), style),
        CSSValuePair::kKeepIdenticalValues);
  }
}

const CSSValue* PointerEvents::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.PointerEvents());
}

const CSSValue* Position::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.PositionInternal());
}

void Position::ApplyInherit(StyleResolverState& state) const {
  if (!state.ParentNode()->IsDocumentNode()) {
    state.StyleBuilder().SetPosition(state.ParentStyle()->GetPosition());
  }
}

const CSSValue* PositionFallback::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (CSSValue* value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(range)) {
    return value;
  }
  if (CSSValue* value = css_parsing_utils::ConsumeDashedIdent(range, context)) {
    return value;
  }
  if (context.Mode() == kUASheetMode) {
    CSSCustomIdentValue* value =
        css_parsing_utils::ConsumeCustomIdent(range, context);
    if (value && value->Value().StartsWith("-internal-")) {
      return value;
    }
  }
  return nullptr;
}
const CSSValue* PositionFallback::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.PositionFallback()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(
      style.PositionFallback()->GetName());
}

const CSSValue* Quotes::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  if (auto* value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kAuto, CSSValueID::kNone>(
              range)) {
    return value;
  }
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  while (!range.AtEnd()) {
    CSSStringValue* parsed_value = css_parsing_utils::ConsumeString(range);
    if (!parsed_value) {
      return nullptr;
    }
    values->Append(*parsed_value);
  }
  if (values->length() && values->length() % 2 == 0) {
    return values;
  }
  return nullptr;
}

const CSSValue* Quotes::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.Quotes()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  if (style.Quotes()->size()) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    for (int i = 0; i < style.Quotes()->size(); i++) {
      list->Append(*MakeGarbageCollected<CSSStringValue>(
          style.Quotes()->GetOpenQuote(i)));
      list->Append(*MakeGarbageCollected<CSSStringValue>(
          style.Quotes()->GetCloseQuote(i)));
    }
    return list;
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* R::ParseSingleValue(CSSParserTokenRange& range,
                                    const CSSParserContext& context,
                                    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* R::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.R(), style);
}

const CSSValue* Resize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Resize());
}

void Resize::ApplyValue(StyleResolverState& state,
                        const CSSValue& value) const {
  const CSSIdentifierValue& identifier_value = To<CSSIdentifierValue>(value);

  EResize r = EResize::kNone;
  if (identifier_value.GetValueID() == CSSValueID::kAuto) {
    if (Settings* settings = state.GetDocument().GetSettings()) {
      r = settings->GetTextAreasAreResizable() ? EResize::kBoth
                                               : EResize::kNone;
    }
    UseCounter::Count(state.GetDocument(), WebFeature::kCSSResizeAuto);
  } else {
    r = identifier_value.ConvertTo<EResize>();
  }
  state.StyleBuilder().SetResize(r);
}

const CSSValue* Right::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessUnlessShorthand(local_context),
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchor));
}

bool Right::IsLayoutDependent(const ComputedStyle* style,
                              LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Right::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForPositionOffset(style, *this,
                                                    layout_object);
}

const CSSValue* Rotate::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSIndependentTransformPropertiesEnabled());

  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  CSSValue* rotation = css_parsing_utils::ConsumeAngle(
      range, context, absl::optional<WebFeature>());

  CSSValue* axis = css_parsing_utils::ConsumeAxis(range, context);
  if (axis) {
    if (To<cssvalue::CSSAxisValue>(axis)->AxisName() != CSSValueID::kZ) {
      // The z axis should be normalized away and stored as a 2D rotate.
      list->Append(*axis);
    }
  } else if (!rotation) {
    return nullptr;
  }

  if (!rotation) {
    rotation = css_parsing_utils::ConsumeAngle(range, context,
                                               absl::optional<WebFeature>());
    if (!rotation) {
      return nullptr;
    }
  }
  list->Append(*rotation);

  return list;
}

const CSSValue* Rotate::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.Rotate()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (style.Rotate()->X() != 0 || style.Rotate()->Y() != 0 ||
      style.Rotate()->Z() != 1) {
    const cssvalue::CSSAxisValue* axis =
        MakeGarbageCollected<cssvalue::CSSAxisValue>(
            style.Rotate()->X(), style.Rotate()->Y(), style.Rotate()->Z());
    list->Append(*axis);
  }
  list->Append(*CSSNumericLiteralValue::Create(
      style.Rotate()->Angle(), CSSPrimitiveValue::UnitType::kDegrees));
  return list;
}

const CSSValue* RowGap::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGapLength(range, context);
}

const CSSValue* RowGap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool) const {
  return ComputedStyleUtils::ValueForGapLength(style.RowGap(), style);
}

const CSSValue* Rx::ParseSingleValue(CSSParserTokenRange& range,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* Rx::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Rx(), style);
}

const CSSValue* Ry::ParseSingleValue(CSSParserTokenRange& range,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* Ry::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Ry(), style);
}

const CSSValue* Scale::ParseSingleValue(CSSParserTokenRange& range,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSIndependentTransformPropertiesEnabled());

  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  CSSPrimitiveValue* x_scale = css_parsing_utils::ConsumeNumberOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!x_scale) {
    return nullptr;
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*x_scale);

  CSSPrimitiveValue* y_scale = css_parsing_utils::ConsumeNumberOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
  if (y_scale) {
    CSSPrimitiveValue* z_scale = css_parsing_utils::ConsumeNumberOrPercent(
        range, context, CSSPrimitiveValue::ValueRange::kAll);
    if (z_scale && z_scale->GetDoubleValue() != 1.0) {
      list->Append(*y_scale);
      list->Append(*z_scale);
    } else if (x_scale->GetDoubleValue() != y_scale->GetDoubleValue()) {
      list->Append(*y_scale);
    }
  }

  return list;
}

const CSSValue* Scale::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  ScaleTransformOperation* scale = style.Scale();
  if (!scale) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSNumericLiteralValue::Create(
      scale->X(), CSSPrimitiveValue::UnitType::kNumber));

  if (scale->Z() == 1) {
    if (scale->X() != scale->Y()) {
      list->Append(*CSSNumericLiteralValue::Create(
          scale->Y(), CSSPrimitiveValue::UnitType::kNumber));
    }
  } else {
    list->Append(*CSSNumericLiteralValue::Create(
        scale->Y(), CSSPrimitiveValue::UnitType::kNumber));
    list->Append(*CSSNumericLiteralValue::Create(
        scale->Z(), CSSNumericLiteralValue::UnitType::kNumber));
  }
  return list;
}

// https://www.w3.org/TR/css-overflow-4
// auto | stable && both-edges?
const CSSValue* ScrollbarGutter::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (auto* value = css_parsing_utils::ConsumeIdent<CSSValueID::kAuto>(range)) {
    return value;
  }

  CSSIdentifierValue* stable = nullptr;
  CSSIdentifierValue* both_edges = nullptr;

  while (!range.AtEnd()) {
    if (!stable) {
      if ((stable =
               css_parsing_utils::ConsumeIdent<CSSValueID::kStable>(range))) {
        continue;
      }
    }
    CSSValueID id = range.Peek().Id();
    if (id == CSSValueID::kBothEdges && !both_edges) {
      both_edges = css_parsing_utils::ConsumeIdent(range);
    } else {
      return nullptr;
    }
  }
  if (!stable) {
    return nullptr;
  }
  if (both_edges) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    list->Append(*stable);
    list->Append(*both_edges);
    return list;
  }
  return stable;
}

const CSSValue* ScrollbarGutter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  auto scrollbar_gutter = style.ScrollbarGutter();
  if (scrollbar_gutter == kScrollbarGutterAuto) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  DCHECK(scrollbar_gutter & kScrollbarGutterStable);

  CSSValue* stable = nullptr;
  if (scrollbar_gutter & kScrollbarGutterStable) {
    stable = CSSIdentifierValue::Create(CSSValueID::kStable);
  }

  if (!(scrollbar_gutter & kScrollbarGutterBothEdges)) {
    return stable;
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*stable);
  if (scrollbar_gutter & kScrollbarGutterBothEdges) {
    list->Append(*CSSIdentifierValue::Create(kScrollbarGutterBothEdges));
  }
  return list;
}

const CSSValue* ScrollbarWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ScrollbarWidth());
}

const CSSValue* ScrollBehavior::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetScrollBehavior());
}

namespace {

static bool ConsumePan(CSSParserTokenRange& range,
                       CSSValue** pan_x,
                       CSSValue** pan_y) {
  CSSValueID id = range.Peek().Id();
  if ((id == CSSValueID::kPanX || id == CSSValueID::kPanRight ||
       id == CSSValueID::kPanLeft) &&
      !*pan_x) {
    *pan_x = css_parsing_utils::ConsumeIdent(range);
  } else if ((id == CSSValueID::kPanY || id == CSSValueID::kPanDown ||
              id == CSSValueID::kPanUp) &&
             !*pan_y) {
    *pan_y = css_parsing_utils::ConsumeIdent(range);
  } else {
    return false;
  }
  return true;
}

}  // namespace

const CSSValue* ScrollCustomization::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kAuto || id == CSSValueID::kNone) {
    list->Append(*css_parsing_utils::ConsumeIdent(range));
    return list;
  }

  CSSValue* pan_x = nullptr;
  CSSValue* pan_y = nullptr;
  if (!ConsumePan(range, &pan_x, &pan_y)) {
    return nullptr;
  }
  if (!range.AtEnd() && !ConsumePan(range, &pan_x, &pan_y)) {
    return nullptr;
  }

  if (pan_x) {
    list->Append(*pan_x);
  }
  if (pan_y) {
    list->Append(*pan_y);
  }
  return list;
}

const CSSValue* ScrollCustomization::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ScrollCustomizationFlagsToCSSValue(
      style.ScrollCustomization());
}

const CSSValue* ScrollMarginBlockEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginBlockStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginBottom::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginBottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginBottom(), style);
}

const CSSValue* ScrollMarginInlineEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginInlineStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginLeft::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginLeft::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginLeft(), style);
}

const CSSValue* ScrollMarginRight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginRight(), style);
}

const CSSValue* ScrollMarginTop::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginTop(), style);
}

const CSSValue* ScrollPaddingBlockEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(range, context);
}

const CSSValue* ScrollPaddingBlockStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(range, context);
}

const CSSValue* ScrollPaddingBottom::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(range, context);
}

const CSSValue* ScrollPaddingBottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.ScrollPaddingBottom(), style);
}

const CSSValue* ScrollPaddingInlineEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(range, context);
}

const CSSValue* ScrollPaddingInlineStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(range, context);
}

const CSSValue* ScrollPaddingLeft::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(range, context);
}

const CSSValue* ScrollPaddingLeft::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.ScrollPaddingLeft(), style);
}

const CSSValue* ScrollPaddingRight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(range, context);
}

const CSSValue* ScrollPaddingRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.ScrollPaddingRight(), style);
}

const CSSValue* ScrollPaddingTop::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(range, context);
}

const CSSValue* ScrollPaddingTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.ScrollPaddingTop(), style);
}

const CSSValue* ScrollSnapAlign::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValue* block_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kNone, CSSValueID::kStart,
                                      CSSValueID::kEnd, CSSValueID::kCenter>(
          range);
  if (!block_value) {
    return nullptr;
  }
  if (range.AtEnd()) {
    return block_value;
  }

  CSSValue* inline_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kNone, CSSValueID::kStart,
                                      CSSValueID::kEnd, CSSValueID::kCenter>(
          range);
  if (!inline_value) {
    return block_value;
  }
  auto* pair = MakeGarbageCollected<CSSValuePair>(
      block_value, inline_value, CSSValuePair::kDropIdenticalValues);
  return pair;
}

const CSSValue* ScrollSnapAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForScrollSnapAlign(style.GetScrollSnapAlign(),
                                                     style);
}

const CSSValue* ScrollSnapStop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ScrollSnapStop());
}

const CSSValue* ScrollSnapType::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID axis_id = range.Peek().Id();
  if (axis_id != CSSValueID::kNone && axis_id != CSSValueID::kX &&
      axis_id != CSSValueID::kY && axis_id != CSSValueID::kBlock &&
      axis_id != CSSValueID::kInline && axis_id != CSSValueID::kBoth) {
    return nullptr;
  }
  CSSValue* axis_value = css_parsing_utils::ConsumeIdent(range);
  if (range.AtEnd() || axis_id == CSSValueID::kNone) {
    return axis_value;
  }

  CSSValueID strictness_id = range.Peek().Id();
  if (strictness_id != CSSValueID::kProximity &&
      strictness_id != CSSValueID::kMandatory) {
    return axis_value;
  }
  CSSValue* strictness_value = css_parsing_utils::ConsumeIdent(range);
  if (strictness_id == CSSValueID::kProximity) {
    return axis_value;  // Shortest serialization.
  }
  auto* pair = MakeGarbageCollected<CSSValuePair>(
      axis_value, strictness_value, CSSValuePair::kDropIdenticalValues);
  return pair;
}

const CSSValue* ScrollSnapType::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForScrollSnapType(style.GetScrollSnapType(),
                                                    style);
}

const CSSValue* ScrollTimelineAxis::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  using css_parsing_utils::ConsumeCommaSeparatedList;
  using css_parsing_utils::ConsumeSingleTimelineAxis;
  return ConsumeCommaSeparatedList(ConsumeSingleTimelineAxis, range);
}

const CSSValue* ScrollTimelineAxis::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const Vector<TimelineAxis>& vector = style.ScrollTimelineAxis();
  if (vector.empty()) {
    return InitialValue();
  }
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (TimelineAxis axis : vector) {
    list->Append(*CSSIdentifierValue::Create(axis));
  }
  return list;
}

const CSSValue* ScrollTimelineAxis::InitialValue() const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kBlock));
  return list;
}

const CSSValue* ScrollTimelineName::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  using css_parsing_utils::ConsumeCommaSeparatedList;
  using css_parsing_utils::ConsumeSingleTimelineName;
  return ConsumeCommaSeparatedList(ConsumeSingleTimelineName, range, context);
}

const CSSValue* ScrollTimelineName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (!style.ScrollTimelineName()) {
    return InitialValue();
  }
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const Member<const ScopedCSSName>& name :
       style.ScrollTimelineName()->GetNames()) {
    list->Append(*ComputedStyleUtils::ValueForCustomIdentOrNone(name.Get()));
  }
  return list;
}

const CSSValue* ScrollTimelineName::InitialValue() const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kNone));
  return list;
}

const CSSValue* ShapeImageThreshold::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(range, context);
}

const CSSValue* ShapeImageThreshold::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.ShapeImageThreshold(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* ShapeMargin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* ShapeMargin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSValue::Create(style.ShapeMargin(), style.EffectiveZoom());
}

const CSSValue* ShapeOutside::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (CSSValue* image_value =
          css_parsing_utils::ConsumeImageOrNone(range, context)) {
    return image_value;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  CSSValue* box_value = css_parsing_utils::ConsumeShapeBox(range);
  if (CSSValue* shape_value = css_parsing_utils::ConsumeBasicShape(
          range, context, css_parsing_utils::AllowPathValue::kForbid)) {
    list->Append(*shape_value);
    if (!box_value) {
      box_value = css_parsing_utils::ConsumeShapeBox(range);
    }
  }
  if (box_value) {
    list->Append(*box_value);
  }
  if (!list->length()) {
    return nullptr;
  }
  return list;
}

const CSSValue* ShapeOutside::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForShape(style, allow_visited_style,
                                           style.ShapeOutside());
}

const CSSValue* ShapeRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ShapeRendering());
}

static CSSValue* ConsumePageSize(CSSParserTokenRange& range) {
  return css_parsing_utils::ConsumeIdent<
      CSSValueID::kA3, CSSValueID::kA4, CSSValueID::kA5, CSSValueID::kB4,
      CSSValueID::kB5, CSSValueID::kJisB5, CSSValueID::kJisB4,
      CSSValueID::kLedger, CSSValueID::kLegal, CSSValueID::kLetter>(range);
}

static float MmToPx(float mm) {
  return mm * kCssPixelsPerMillimeter;
}
static float InchToPx(float inch) {
  return inch * kCssPixelsPerInch;
}
static gfx::SizeF GetPageSizeFromName(
    const CSSIdentifierValue& page_size_name) {
  switch (page_size_name.GetValueID()) {
    case CSSValueID::kA5:
      return gfx::SizeF(MmToPx(148), MmToPx(210));
    case CSSValueID::kA4:
      return gfx::SizeF(MmToPx(210), MmToPx(297));
    case CSSValueID::kA3:
      return gfx::SizeF(MmToPx(297), MmToPx(420));
    case CSSValueID::kB5:
      return gfx::SizeF(MmToPx(176), MmToPx(250));
    case CSSValueID::kB4:
      return gfx::SizeF(MmToPx(250), MmToPx(353));
    case CSSValueID::kJisB5:
      return gfx::SizeF(MmToPx(182), MmToPx(257));
    case CSSValueID::kJisB4:
      return gfx::SizeF(MmToPx(257), MmToPx(364));
    case CSSValueID::kLetter:
      return gfx::SizeF(InchToPx(8.5), InchToPx(11));
    case CSSValueID::kLegal:
      return gfx::SizeF(InchToPx(8.5), InchToPx(14));
    case CSSValueID::kLedger:
      return gfx::SizeF(InchToPx(11), InchToPx(17));
    default:
      NOTREACHED();
      return gfx::SizeF(0, 0);
  }
}

const CSSValue* Size::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  CSSValueList* result = CSSValueList::CreateSpaceSeparated();

  if (range.Peek().Id() == CSSValueID::kAuto) {
    result->Append(*css_parsing_utils::ConsumeIdent(range));
    return result;
  }

  if (CSSValue* width = css_parsing_utils::ConsumeLength(
          range, context, CSSPrimitiveValue::ValueRange::kNonNegative)) {
    CSSValue* height = css_parsing_utils::ConsumeLength(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    result->Append(*width);
    if (height) {
      result->Append(*height);
    }
    return result;
  }

  CSSValue* page_size = ConsumePageSize(range);
  CSSValue* orientation =
      css_parsing_utils::ConsumeIdent<CSSValueID::kPortrait,
                                      CSSValueID::kLandscape>(range);
  if (!page_size) {
    page_size = ConsumePageSize(range);
  }

  if (!orientation && !page_size) {
    return nullptr;
  }
  if (page_size) {
    result->Append(*page_size);
  }
  if (orientation) {
    result->Append(*orientation);
  }
  return result;
}

void Size::ApplyInitial(StyleResolverState& state) const {}

void Size::ApplyInherit(StyleResolverState& state) const {}

void Size::ApplyValue(StyleResolverState& state, const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.ResetPageSizeType();
  gfx::SizeF size;
  PageSizeType page_size_type = PageSizeType::kAuto;
  const auto& list = To<CSSValueList>(value);
  if (list.length() == 2) {
    // <length>{2} | <page-size> <orientation>
    const CSSValue& first = list.Item(0);
    const CSSValue& second = list.Item(1);
    auto* first_primitive_value = DynamicTo<CSSPrimitiveValue>(first);
    if (first_primitive_value && first_primitive_value->IsLength()) {
      CSSToLengthConversionData unzoomed_conversion_data =
          state.CssToLengthConversionData().Unzoomed();
      // <length>{2}
      size = gfx::SizeF(
          first_primitive_value->ComputeLength<float>(unzoomed_conversion_data),
          To<CSSPrimitiveValue>(second).ComputeLength<float>(
              unzoomed_conversion_data));
    } else {
      // <page-size> <orientation>
      size = GetPageSizeFromName(To<CSSIdentifierValue>(first));

      DCHECK(To<CSSIdentifierValue>(second).GetValueID() ==
                 CSSValueID::kLandscape ||
             To<CSSIdentifierValue>(second).GetValueID() ==
                 CSSValueID::kPortrait);
      if (To<CSSIdentifierValue>(second).GetValueID() ==
          CSSValueID::kLandscape) {
        size.Transpose();
      }
    }
    page_size_type = PageSizeType::kFixed;
  } else {
    DCHECK_EQ(list.length(), 1U);
    // <length> | auto | <page-size> | [ portrait | landscape]
    const CSSValue& first = list.Item(0);
    auto* first_primitive_value = DynamicTo<CSSPrimitiveValue>(first);
    if (first_primitive_value && first_primitive_value->IsLength()) {
      // <length>
      page_size_type = PageSizeType::kFixed;
      float width = first_primitive_value->ComputeLength<float>(
          state.CssToLengthConversionData().Unzoomed());
      size = gfx::SizeF(width, width);
    } else {
      const auto& ident = To<CSSIdentifierValue>(first);
      switch (ident.GetValueID()) {
        case CSSValueID::kAuto:
          page_size_type = PageSizeType::kAuto;
          break;
        case CSSValueID::kPortrait:
          page_size_type = PageSizeType::kPortrait;
          break;
        case CSSValueID::kLandscape:
          page_size_type = PageSizeType::kLandscape;
          break;
        default:
          // <page-size>
          page_size_type = PageSizeType::kFixed;
          size = GetPageSizeFromName(ident);
      }
    }
  }
  builder.SetPageSizeType(page_size_type);
  builder.SetPageSize(size);
}

const CSSValue* Speak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Speak());
}

const CSSValue* StopColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color StopColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  StyleColor stop_color = style.StopColor();
  if (style.ShouldForceColor(stop_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return style.ResolvedColor(stop_color, is_current_color);
}

const CSSValue* StopColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.StopColor(), CSSValuePhase::kComputedValue);
}

const CSSValue* StopOpacity::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(range, context);
}

const CSSValue* StopOpacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.StopOpacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* Stroke::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGPaint(range, context);
}

const CSSValue* Stroke::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGPaint(style.StrokePaint(), style);
}

const blink::Color Stroke::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  DCHECK(style.StrokePaint().HasColor());
  const StyleColor& stroke_color = style.StrokePaint().GetColor();
  if (style.ShouldForceColor(stroke_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return stroke_color.Resolve(style.GetCurrentColor(), style.UsedColorScheme(),
                              is_current_color);
}

const CSSValue* StrokeDasharray::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  CSSValueList* dashes = CSSValueList::CreateCommaSeparated();
  do {
    CSSPrimitiveValue* dash = css_parsing_utils::ConsumeLengthOrPercent(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!dash || (css_parsing_utils::ConsumeCommaIncludingWhitespace(range) &&
                  range.AtEnd())) {
      return nullptr;
    }
    dashes->Append(*dash);
  } while (!range.AtEnd());
  return dashes;
}

const CSSValue* StrokeDasharray::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::StrokeDashArrayToCSSValueList(
      *style.StrokeDashArray(), style);
}

const CSSValue* StrokeDashoffset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  return css_parsing_utils::ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kAll,
      css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* StrokeDashoffset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.StrokeDashOffset(), style);
}

const CSSValue* StrokeLinecap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.CapStyle());
}

const CSSValue* StrokeLinejoin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.JoinStyle());
}

const CSSValue* StrokeMiterlimit::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* StrokeMiterlimit::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.StrokeMiterLimit(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* StrokeOpacity::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(range, context);
}

const CSSValue* StrokeOpacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.StrokeOpacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* StrokeWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  return css_parsing_utils::ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative,
      css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* StrokeWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  // We store the unzoomed stroke-width value using ConvertUnzoomedLength().
  // Don't apply zoom here either.
  return CSSValue::Create(style.StrokeWidth().length(), 1);
}

const CSSValue* ContentVisibility::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ContentVisibility());
}

const CSSValue* ContentVisibility::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIdent<
      CSSValueID::kVisible, CSSValueID::kAuto, CSSValueID::kHidden>(range);
}

const CSSValue* TabSize::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSPrimitiveValue* parsed_value = css_parsing_utils::ConsumeNumber(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (parsed_value) {
    return parsed_value;
  }
  return css_parsing_utils::ConsumeLength(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* TabSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(
      style.GetTabSize().GetPixelSize(1.0),
      style.GetTabSize().IsSpaces() ? CSSPrimitiveValue::UnitType::kNumber
                                    : CSSPrimitiveValue::UnitType::kPixels);
}

const CSSValue* TableLayout::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.TableLayout());
}

const CSSValue* TextAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetTextAlign());
}

void TextAlign::ApplyValue(StyleResolverState& state,
                           const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  const auto* ident_value = DynamicTo<CSSIdentifierValue>(value);
  if (ident_value &&
      ident_value->GetValueID() != CSSValueID::kWebkitMatchParent) {
    // Special case for th elements - UA stylesheet text-align does not apply if
    // parent's computed value for text-align is not its initial value
    // https://html.spec.whatwg.org/C/#tables-2
    if (ident_value->GetValueID() == CSSValueID::kInternalCenter &&
        state.ParentStyle()->GetTextAlign() !=
            ComputedStyleInitialValues::InitialTextAlign()) {
      builder.SetTextAlign(state.ParentStyle()->GetTextAlign());
    } else {
      builder.SetTextAlign(ident_value->ConvertTo<ETextAlign>());
    }
  } else if (state.ParentStyle()->GetTextAlign() == ETextAlign::kStart) {
    builder.SetTextAlign(state.ParentStyle()->IsLeftToRightDirection()
                             ? ETextAlign::kLeft
                             : ETextAlign::kRight);
  } else if (state.ParentStyle()->GetTextAlign() == ETextAlign::kEnd) {
    builder.SetTextAlign(state.ParentStyle()->IsLeftToRightDirection()
                             ? ETextAlign::kRight
                             : ETextAlign::kLeft);
  } else {
    builder.SetTextAlign(state.ParentStyle()->GetTextAlign());
  }
}

const CSSValue* TextAlignLast::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.TextAlignLast());
}

const CSSValue* TextAnchor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.TextAnchor());
}

const CSSValue* TextCombineUpright::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.TextCombine());
}

const CSSValue* TextDecorationColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color TextDecorationColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  StyleColor decoration_color =
      style.DecorationColorIncludingFallback(visited_link);
  if (style.ShouldForceColor(decoration_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return decoration_color.Resolve(style.GetCurrentColor(),
                                  style.UsedColorScheme(), is_current_color);
}

const CSSValue* TextDecorationColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TextDecorationColor(), CSSValuePhase::kComputedValue);
}

const CSSValue* TextDecorationLine::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeTextDecorationLine(range);
}

const CSSValue* TextDecorationLine::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::RenderTextDecorationFlagsToCSSValue(
      style.GetTextDecorationLine());
}

const CSSValue* TextDecorationSkipInk::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForTextDecorationSkipInk(
      style.TextDecorationSkipInk());
}

const CSSValue* TextDecorationStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForTextDecorationStyle(
      style.TextDecorationStyle());
}

const CSSValue* TextDecorationThickness::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (auto* ident = css_parsing_utils::ConsumeIdent<CSSValueID::kFromFont,
                                                    CSSValueID::kAuto>(range)) {
    return ident;
  }
  return css_parsing_utils::ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* TextDecorationThickness::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.GetTextDecorationThickness().IsFromFont()) {
    return CSSIdentifierValue::Create(CSSValueID::kFromFont);
  }

  if (style.GetTextDecorationThickness().IsAuto()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.GetTextDecorationThickness().Thickness(), style);
}

const CSSValue* TextIndent::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // [ <length> | <percentage> ]
  CSSValue* length_percentage = nullptr;
  do {
    if (!length_percentage) {
      length_percentage = css_parsing_utils::ConsumeLengthOrPercent(
          range, context, CSSPrimitiveValue::ValueRange::kAll,
          css_parsing_utils::UnitlessQuirk::kAllow);
      if (length_percentage) {
        continue;
      }
    }
    return nullptr;
  } while (!range.AtEnd());

  if (!length_percentage) {
    return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*length_percentage);

  return list;
}

const CSSValue* TextIndent::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.TextIndent(), style));
  return list;
}

void TextIndent::ApplyValue(StyleResolverState& state,
                            const CSSValue& value) const {
  Length length_or_percentage_value;

  for (auto& list_value : To<CSSValueList>(value)) {
    if (auto* list_primitive_value =
            DynamicTo<CSSPrimitiveValue>(*list_value)) {
      length_or_percentage_value = list_primitive_value->ConvertToLength(
          state.CssToLengthConversionData());
    } else {
      NOTREACHED();
    }
  }

  state.StyleBuilder().SetTextIndent(length_or_percentage_value);
}

const CSSValue* TextOrientation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetTextOrientation());
}

void TextOrientation::ApplyInitial(StyleResolverState& state) const {
  state.SetTextOrientation(
      ComputedStyleInitialValues::InitialTextOrientation());
}

void TextOrientation::ApplyInherit(StyleResolverState& state) const {
  state.SetTextOrientation(state.ParentStyle()->GetTextOrientation());
}

void TextOrientation::ApplyValue(StyleResolverState& state,
                                 const CSSValue& value) const {
  state.SetTextOrientation(
      To<CSSIdentifierValue>(value).ConvertTo<ETextOrientation>());
}

const CSSValue* TextOverflow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.TextOverflow() != ETextOverflow::kClip) {
    return CSSIdentifierValue::Create(CSSValueID::kEllipsis);
  }
  return CSSIdentifierValue::Create(CSSValueID::kClip);
}

const CSSValue* TextRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetFontDescription().TextRendering());
}

const CSSValue* TextShadow::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeShadow(
      range, context, css_parsing_utils::AllowInsetAndSpread::kForbid);
}

const CSSValue* TextShadow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForShadowList(
      style.TextShadow(), style, false, CSSValuePhase::kComputedValue);
}

const CSSValue* TextSizeAdjust::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumePercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* TextSizeAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.GetTextSizeAdjust().IsAuto()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return CSSNumericLiteralValue::Create(
      style.GetTextSizeAdjust().Multiplier() * 100,
      CSSPrimitiveValue::UnitType::kPercentage);
}

const CSSValue* TextTransform::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.TextTransform());
}

// https://drafts.csswg.org/css-text-decor-4/#text-underline-position-property
// auto | [ from-font | under ] || [ left | right ] - default: auto
const CSSValue* TextUnderlinePosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  CSSIdentifierValue* from_font_or_under_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kFromFont,
                                      CSSValueID::kUnder>(range);
  CSSIdentifierValue* left_or_right_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kLeft, CSSValueID::kRight>(
          range);
  if (left_or_right_value && !from_font_or_under_value) {
    from_font_or_under_value =
        css_parsing_utils::ConsumeIdent<CSSValueID::kFromFont,
                                        CSSValueID::kUnder>(range);
  }
  if (!from_font_or_under_value && !left_or_right_value) {
    return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (from_font_or_under_value) {
    list->Append(*from_font_or_under_value);
  }
  if (left_or_right_value) {
    list->Append(*left_or_right_value);
  }
  return list;
}

const CSSValue* TextUnderlinePosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  auto text_underline_position = style.TextUnderlinePosition();
  if (text_underline_position == kTextUnderlinePositionAuto) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  if (text_underline_position == kTextUnderlinePositionFromFont) {
    return CSSIdentifierValue::Create(CSSValueID::kFromFont);
  }
  if (text_underline_position == kTextUnderlinePositionUnder) {
    return CSSIdentifierValue::Create(CSSValueID::kUnder);
  }
  if (text_underline_position == kTextUnderlinePositionLeft) {
    return CSSIdentifierValue::Create(CSSValueID::kLeft);
  }
  if (text_underline_position == kTextUnderlinePositionRight) {
    return CSSIdentifierValue::Create(CSSValueID::kRight);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (text_underline_position & kTextUnderlinePositionFromFont) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kFromFont));
  } else {
    DCHECK(text_underline_position & kTextUnderlinePositionUnder);
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnder));
  }
  if (text_underline_position & kTextUnderlinePositionLeft) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kLeft));
  }
  if (text_underline_position & kTextUnderlinePositionRight) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kRight));
  }
  DCHECK_EQ(list->length(), 2U);
  return list;
}

const CSSValue* TextUnderlineOffset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* TextUnderlineOffset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.TextUnderlineOffset(), style);
}

const CSSValue* Top::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessUnlessShorthand(local_context),
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchor));
}

bool Top::IsLayoutDependent(const ComputedStyle* style,
                            LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Top::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForPositionOffset(style, *this,
                                                    layout_object);
}

namespace {

static bool ConsumePan(CSSParserTokenRange& range,
                       CSSValue*& pan_x,
                       CSSValue*& pan_y,
                       CSSValue*& pinch_zoom) {
  CSSValueID id = range.Peek().Id();
  if ((id == CSSValueID::kPanX || id == CSSValueID::kPanRight ||
       id == CSSValueID::kPanLeft) &&
      !pan_x) {
    pan_x = css_parsing_utils::ConsumeIdent(range);
  } else if ((id == CSSValueID::kPanY || id == CSSValueID::kPanDown ||
              id == CSSValueID::kPanUp) &&
             !pan_y) {
    pan_y = css_parsing_utils::ConsumeIdent(range);
  } else if (id == CSSValueID::kPinchZoom && !pinch_zoom) {
    pinch_zoom = css_parsing_utils::ConsumeIdent(range);
  } else {
    return false;
  }
  return true;
}

}  // namespace

const CSSValue* TouchAction::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kAuto || id == CSSValueID::kNone ||
      id == CSSValueID::kManipulation) {
    list->Append(*css_parsing_utils::ConsumeIdent(range));
    return list;
  }

  CSSValue* pan_x = nullptr;
  CSSValue* pan_y = nullptr;
  CSSValue* pinch_zoom = nullptr;
  if (!ConsumePan(range, pan_x, pan_y, pinch_zoom)) {
    return nullptr;
  }
  if (!range.AtEnd() && !ConsumePan(range, pan_x, pan_y, pinch_zoom)) {
    return nullptr;
  }
  if (!range.AtEnd() && !ConsumePan(range, pan_x, pan_y, pinch_zoom)) {
    return nullptr;
  }

  if (pan_x) {
    list->Append(*pan_x);
  }
  if (pan_y) {
    list->Append(*pan_y);
  }
  if (pinch_zoom) {
    list->Append(*pinch_zoom);
  }
  return list;
}

const CSSValue* TouchAction::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::TouchActionFlagsToCSSValue(style.GetTouchAction());
}

const CSSValue* TransformBox::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.TransformBox());
}

const CSSValue* Transform::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeTransformList(range, context, local_context);
}

bool Transform::IsLayoutDependent(const ComputedStyle* style,
                                  LayoutObject* layout_object) const {
  return layout_object &&
         (layout_object->IsBox() || layout_object->IsSVGChild());
}

const CSSValue* Transform::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ResolvedTransform(layout_object, style);
}

const CSSValue* TransformOrigin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValue* result_x = nullptr;
  CSSValue* result_y = nullptr;
  if (css_parsing_utils::ConsumeOneOrTwoValuedPosition(
          range, context, css_parsing_utils::UnitlessQuirk::kForbid, result_x,
          result_y)) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    list->Append(*result_x);
    list->Append(*result_y);
    CSSValue* result_z = css_parsing_utils::ConsumeLength(
        range, context, CSSPrimitiveValue::ValueRange::kAll);
    if (result_z) {
      list->Append(*result_z);
    }
    return list;
  }
  return nullptr;
}

bool TransformOrigin::IsLayoutDependent(const ComputedStyle* style,
                                        LayoutObject* layout_object) const {
  return layout_object &&
         (layout_object->IsBox() || layout_object->IsSVGChild());
}

const CSSValue* TransformOrigin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (layout_object) {
    gfx::RectF reference_box = ComputedStyleUtils::ReferenceBoxForTransform(
        *layout_object, ComputedStyleUtils::kDontUsePixelSnappedBox);
    gfx::PointF resolved_origin(
        FloatValueForLength(style.GetTransformOrigin().X(),
                            reference_box.width()),
        FloatValueForLength(style.GetTransformOrigin().Y(),
                            reference_box.height()));
    list->Append(*ZoomAdjustedPixelValue(resolved_origin.x(), style));
    list->Append(*ZoomAdjustedPixelValue(resolved_origin.y(), style));
  } else {
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.GetTransformOrigin().X(), style));
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.GetTransformOrigin().Y(), style));
  }
  if (style.GetTransformOrigin().Z() != 0) {
    list->Append(
        *ZoomAdjustedPixelValue(style.GetTransformOrigin().Z(), style));
  }
  return list;
}

const CSSValue* TransformStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(
      (style.TransformStyle3D() == ETransformStyle3D::kPreserve3d)
          ? CSSValueID::kPreserve3d
          : CSSValueID::kFlat);
}

const CSSValue* TransitionDelay::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeTime, range, context,
      CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* TransitionDelay::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDelayStartList(
      style.Transitions());
}

const CSSValue* TransitionDelay::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (ComputedStyleUtils::ValueForAnimationDelayStart(
                          CSSTimingData::InitialDelayStart())));
  return value;
}

const CSSValue* TransitionDuration::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeTime, range, context,
      CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* TransitionDuration::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDurationList(style.Transitions());
}

const CSSValue* TransitionDuration::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      const Persistent<CSSValue>, value,
      (CSSNumericLiteralValue::Create(CSSTimingData::InitialDuration().value(),
                                      CSSPrimitiveValue::UnitType::kSeconds)));
  return value;
}

const CSSValue* TransitionProperty::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueList* list = css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeTransitionProperty, range, context);
  if (!list || !css_parsing_utils::IsValidPropertyList(*list)) {
    return nullptr;
  }
  return list;
}

const CSSValue* TransitionProperty::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForTransitionProperty(style.Transitions());
}

const CSSValue* TransitionProperty::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kAll);
}

const CSSValue* TransitionTimingFunction::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationTimingFunction, range, context);
}

const CSSValue* TransitionTimingFunction::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationTimingFunctionList(
      style.Transitions());
}

const CSSValue* TransitionTimingFunction::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kEase);
}

const CSSValue* Translate::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSIndependentTransformPropertiesEnabled());
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  CSSValue* translate_x = css_parsing_utils::ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!translate_x) {
    return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*translate_x);
  CSSPrimitiveValue* translate_y = css_parsing_utils::ConsumeLengthOrPercent(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
  if (translate_y) {
    CSSPrimitiveValue* translate_z = css_parsing_utils::ConsumeLength(
        range, context, CSSPrimitiveValue::ValueRange::kAll);

    if (translate_z && translate_z->IsZero()) {
      translate_z = nullptr;
    }
    if (translate_y->IsZero() && !translate_z) {
      return list;
    }

    list->Append(*translate_y);
    if (translate_z) {
      list->Append(*translate_z);
    }
  }

  return list;
}

bool Translate::IsLayoutDependent(const ComputedStyle* style,
                                  LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Translate::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (!style.Translate()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.Translate()->X(), style));

  if (!style.Translate()->Y().IsZero() || style.Translate()->Z() != 0) {
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.Translate()->Y(), style));
  }

  if (style.Translate()->Z() != 0) {
    list->Append(*ZoomAdjustedPixelValue(style.Translate()->Z(), style));
  }

  return list;
}

const CSSValue* UnicodeBidi::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetUnicodeBidi());
}

const CSSValue* UserSelect::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.UserSelect());
}

const CSSValue* VectorEffect::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.VectorEffect());
}

const CSSValue* VerticalAlign::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValue* parsed_value = css_parsing_utils::ConsumeIdentRange(
      range, CSSValueID::kBaseline, CSSValueID::kWebkitBaselineMiddle);
  if (!parsed_value) {
    parsed_value = css_parsing_utils::ConsumeLengthOrPercent(
        range, context, CSSPrimitiveValue::ValueRange::kAll,
        css_parsing_utils::UnitlessQuirk::kAllow);
  }
  return parsed_value;
}

const CSSValue* VerticalAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  switch (style.VerticalAlign()) {
    case EVerticalAlign::kBaseline:
      return CSSIdentifierValue::Create(CSSValueID::kBaseline);
    case EVerticalAlign::kMiddle:
      return CSSIdentifierValue::Create(CSSValueID::kMiddle);
    case EVerticalAlign::kSub:
      return CSSIdentifierValue::Create(CSSValueID::kSub);
    case EVerticalAlign::kSuper:
      return CSSIdentifierValue::Create(CSSValueID::kSuper);
    case EVerticalAlign::kTextTop:
      return CSSIdentifierValue::Create(CSSValueID::kTextTop);
    case EVerticalAlign::kTextBottom:
      return CSSIdentifierValue::Create(CSSValueID::kTextBottom);
    case EVerticalAlign::kTop:
      return CSSIdentifierValue::Create(CSSValueID::kTop);
    case EVerticalAlign::kBottom:
      return CSSIdentifierValue::Create(CSSValueID::kBottom);
    case EVerticalAlign::kBaselineMiddle:
      return CSSIdentifierValue::Create(CSSValueID::kWebkitBaselineMiddle);
    case EVerticalAlign::kLength:
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.GetVerticalAlignLength(), style);
  }
  NOTREACHED();
  return nullptr;
}

void VerticalAlign::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  EVerticalAlign vertical_align = state.ParentStyle()->VerticalAlign();
  builder.SetVerticalAlign(vertical_align);
  if (vertical_align == EVerticalAlign::kLength) {
    builder.SetVerticalAlignLength(
        state.ParentStyle()->GetVerticalAlignLength());
  }
}

void VerticalAlign::ApplyValue(StyleResolverState& state,
                               const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    builder.SetVerticalAlign(identifier_value->ConvertTo<EVerticalAlign>());
  } else {
    builder.SetVerticalAlignLength(To<CSSPrimitiveValue>(value).ConvertToLength(
        state.CssToLengthConversionData()));
  }
}

const CSSValue* ViewTimelineAxis::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  using css_parsing_utils::ConsumeCommaSeparatedList;
  using css_parsing_utils::ConsumeSingleTimelineAxis;
  return ConsumeCommaSeparatedList(ConsumeSingleTimelineAxis, range);
}

const CSSValue* ViewTimelineAxis::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const Vector<TimelineAxis>& vector = style.ViewTimelineAxis();
  if (vector.empty()) {
    return InitialValue();
  }
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (TimelineAxis axis : vector) {
    list->Append(*CSSIdentifierValue::Create(axis));
  }
  return list;
}

const CSSValue* ViewTimelineAxis::InitialValue() const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kBlock));
  return list;
}

const CSSValue* ViewTimelineInset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  using css_parsing_utils::ConsumeCommaSeparatedList;
  using css_parsing_utils::ConsumeSingleTimelineInset;
  return ConsumeCommaSeparatedList(ConsumeSingleTimelineInset, range, context);
}

const CSSValue* ViewTimelineInset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const Vector<TimelineInset>& vector = style.ViewTimelineInset();
  if (vector.empty()) {
    return InitialValue();
  }
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const TimelineInset& inset : vector) {
    list->Append(*MakeGarbageCollected<CSSValuePair>(
        ComputedStyleUtils::ZoomAdjustedPixelValueForLength(inset.GetStart(),
                                                            style),
        ComputedStyleUtils::ZoomAdjustedPixelValueForLength(inset.GetEnd(),
                                                            style),
        CSSValuePair::kDropIdenticalValues));
  }
  return list;
}

const CSSValue* ViewTimelineInset::InitialValue() const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  list->Append(
      *CSSNumericLiteralValue::Create(0, CSSPrimitiveValue::UnitType::kPixels));
  return list;
}

const CSSValue* ViewTimelineName::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  using css_parsing_utils::ConsumeCommaSeparatedList;
  using css_parsing_utils::ConsumeSingleTimelineName;
  return ConsumeCommaSeparatedList(ConsumeSingleTimelineName, range, context);
}

const CSSValue* ViewTimelineName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (!style.ViewTimelineName()) {
    return InitialValue();
  }
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const Member<const ScopedCSSName>& name :
       style.ViewTimelineName()->GetNames()) {
    list->Append(*ComputedStyleUtils::ValueForCustomIdentOrNone(name.Get()));
  }
  return list;
}

const CSSValue* ViewTimelineName::InitialValue() const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kNone));
  return list;
}

const CSSValue* Visibility::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Visibility());
}

const CSSValue* AppRegion::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.DraggableRegionMode() == EDraggableRegionMode::kNone) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return CSSIdentifierValue::Create(style.DraggableRegionMode() ==
                                            EDraggableRegionMode::kDrag
                                        ? CSSValueID::kDrag
                                        : CSSValueID::kNoDrag);
}

void AppRegion::ApplyInitial(StyleResolverState& state) const {}

void AppRegion::ApplyInherit(StyleResolverState& state) const {}

void AppRegion::ApplyValue(StyleResolverState& state,
                           const CSSValue& value) const {
  const auto& identifier_value = To<CSSIdentifierValue>(value);
  state.StyleBuilder().SetDraggableRegionMode(
      identifier_value.GetValueID() == CSSValueID::kDrag
          ? EDraggableRegionMode::kDrag
          : EDraggableRegionMode::kNoDrag);
  state.GetDocument().SetHasAnnotatedRegions(true);
}

const CSSValue* Appearance::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  CSSValueID id = range.Peek().Id();
  CSSPropertyID property = CSSPropertyID::kAppearance;
  if (CSSParserFastPaths::IsValidKeywordPropertyAndValue(property, id,
                                                         context.Mode())) {
    if (local_context.UseAliasParsing()) {
      property = CSSPropertyID::kAliasWebkitAppearance;
    }
    css_parsing_utils::CountKeywordOnlyPropertyUsage(property, context, id);
    return css_parsing_utils::ConsumeIdent(range);
  }
  return nullptr;
}

const CSSValue* Appearance::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Appearance());
}

const CSSValue* WebkitBorderHorizontalSpacing::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue*
WebkitBorderHorizontalSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.HorizontalBorderSpacing(), style);
}

const CSSValue* WebkitBorderImage::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWebkitBorderImage(range, context);
}

const CSSValue* WebkitBorderImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImage(style.BorderImage(), style,
                                                    allow_visited_style);
}

void WebkitBorderImage::ApplyValue(StyleResolverState& state,
                                   const CSSValue& value) const {
  NinePieceImage image;
  CSSToStyleMap::MapNinePieceImage(state, CSSPropertyID::kWebkitBorderImage,
                                   value, image);
  state.StyleBuilder().SetBorderImage(image);
}

const CSSValue* WebkitBorderVerticalSpacing::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* WebkitBorderVerticalSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.VerticalBorderSpacing(), style);
}

const CSSValue* WebkitBoxAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BoxAlign());
}

const CSSValue* WebkitBoxDecorationBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.BoxDecorationBreak() == EBoxDecorationBreak::kSlice) {
    return CSSIdentifierValue::Create(CSSValueID::kSlice);
  }
  return CSSIdentifierValue::Create(CSSValueID::kClone);
}

const CSSValue* WebkitBoxDirection::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BoxDirection());
}

const CSSValue* WebkitBoxFlex::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(range, context,
                                          CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* WebkitBoxFlex::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.BoxFlex(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* WebkitBoxOrdinalGroup::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositiveInteger(range, context);
}

const CSSValue* WebkitBoxOrdinalGroup::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.BoxOrdinalGroup(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* WebkitBoxOrient::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BoxOrient());
}

const CSSValue* WebkitBoxPack::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BoxPack());
}

namespace {

CSSValue* ConsumeReflect(CSSParserTokenRange& range,
                         const CSSParserContext& context) {
  CSSIdentifierValue* direction =
      css_parsing_utils::ConsumeIdent<CSSValueID::kAbove, CSSValueID::kBelow,
                                      CSSValueID::kLeft, CSSValueID::kRight>(
          range);
  if (!direction) {
    return nullptr;
  }

  CSSPrimitiveValue* offset = nullptr;
  if (range.AtEnd()) {
    offset =
        CSSNumericLiteralValue::Create(0, CSSPrimitiveValue::UnitType::kPixels);
  } else {
    offset = ConsumeLengthOrPercent(range, context,
                                    CSSPrimitiveValue::ValueRange::kAll,
                                    css_parsing_utils::UnitlessQuirk::kForbid);
    if (!offset) {
      return nullptr;
    }
  }

  CSSValue* mask = nullptr;
  if (!range.AtEnd()) {
    mask = css_parsing_utils::ConsumeWebkitBorderImage(range, context);
    if (!mask) {
      return nullptr;
    }
  }
  return MakeGarbageCollected<cssvalue::CSSReflectValue>(direction, offset,
                                                         mask);
}

}  // namespace

const CSSValue* WebkitBoxReflect::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeReflect(range, context);
}

const CSSValue* WebkitBoxReflect::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForReflection(style.BoxReflect(), style,
                                                allow_visited_style);
}

const CSSValue* InternalFontSizeDelta::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(
      range, context, CSSPrimitiveValue::ValueRange::kAll,
      css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* WebkitFontSmoothing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetFontDescription().FontSmoothing());
}

const CSSValue* WebkitHighlight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeString(range);
}

const CSSValue* WebkitHighlight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.Highlight() == g_null_atom) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return MakeGarbageCollected<CSSStringValue>(style.Highlight());
}

const CSSValue* HyphenateCharacter::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeString(range);
}

const CSSValue* HyphenateCharacter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HyphenationString().IsNull()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return MakeGarbageCollected<CSSStringValue>(style.HyphenationString());
}

const CSSValue* WebkitLineBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetLineBreak());
}

const CSSValue* WebkitLineClamp::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // When specifying number of lines, don't allow 0 as a valid value.
  return css_parsing_utils::ConsumePositiveInteger(range, context);
}

const CSSValue* WebkitLineClamp::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.HasLineClamp()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return CSSNumericLiteralValue::Create(style.LineClamp(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* WebkitLocale::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeString(range);
}

const CSSValue* WebkitLocale::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.Locale().IsNull()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return MakeGarbageCollected<CSSStringValue>(style.Locale());
}

void WebkitLocale::ApplyValue(StyleResolverState& state,
                              const CSSValue& value) const {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kAuto);
    state.GetFontBuilder().SetLocale(nullptr);
  } else {
    state.GetFontBuilder().SetLocale(
        LayoutLocale::Get(AtomicString(To<CSSStringValue>(value).Value())));
  }
}

const CSSValue* WebkitMaskBoxImageOutset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageOutset(range, context);
}

const CSSValue* WebkitMaskBoxImageOutset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageQuad(
      style.MaskBoxImage().Outset(), style);
}

const CSSValue* WebkitMaskBoxImageRepeat::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageRepeat(range);
}

const CSSValue* WebkitMaskBoxImageRepeat::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageRepeat(style.MaskBoxImage());
}

const CSSValue* WebkitMaskBoxImageSlice::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageSlice(
      range, context, css_parsing_utils::DefaultFill::kNoFill);
}

const CSSValue* WebkitMaskBoxImageSlice::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageSlice(style.MaskBoxImage());
}

const CSSValue* WebkitMaskBoxImageSource::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeImageOrNone(range, context);
}

const CSSValue* WebkitMaskBoxImageSource::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.MaskBoxImageSource()) {
    return style.MaskBoxImageSource()->ComputedCSSValue(style,
                                                        allow_visited_style);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

void WebkitMaskBoxImageSource::ApplyValue(StyleResolverState& state,
                                          const CSSValue& value) const {
  state.StyleBuilder().SetMaskBoxImageSource(
      state.GetStyleImage(CSSPropertyID::kWebkitMaskBoxImageSource, value));
}

const CSSValue* WebkitMaskBoxImageWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageWidth(range, context);
}

const CSSValue* WebkitMaskBoxImageWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageQuad(
      style.MaskBoxImage().BorderSlices(), style);
}

const CSSValue* WebkitMaskClip::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumePrefixedBackgroundBox, range,
      css_parsing_utils::AllowTextValue::kAllow);
}

const CSSValue* WebkitMaskClip::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.MaskLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    EFillBox box = curr_layer->Clip();
    list->Append(*CSSIdentifierValue::Create(box));
  }
  return list;
}

const CSSValue* WebkitMaskComposite::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeBackgroundComposite, range);
}

const CSSValue* WebkitMaskComposite::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.MaskLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    list->Append(*CSSIdentifierValue::Create(curr_layer->Composite()));
  }
  return list;
}

const CSSValue* WebkitMaskImage::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeImageOrNone, range, context);
}

const CSSValue* WebkitMaskImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const FillLayer& fill_layer = style.MaskLayers();
  return ComputedStyleUtils::BackgroundImageOrWebkitMaskImage(
      style, allow_visited_style, fill_layer);
}

const CSSValue* WebkitMaskOrigin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumePrefixedBackgroundBox, range,
      css_parsing_utils::AllowTextValue::kForbid);
}

const CSSValue* WebkitMaskOrigin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.MaskLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    EFillBox box = curr_layer->Origin();
    list->Append(*CSSIdentifierValue::Create(box));
  }
  return list;
}

const CSSValue* WebkitMaskPositionX::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumePositionLonghand<CSSValueID::kLeft,
                                                 CSSValueID::kRight>,
      range, context);
}

const CSSValue* WebkitMaskPositionX::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const FillLayer* curr_layer = &style.MaskLayers();
  return ComputedStyleUtils::BackgroundPositionXOrWebkitMaskPositionX(
      style, curr_layer);
}

const CSSValue* WebkitMaskPositionY::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumePositionLonghand<CSSValueID::kTop,
                                                 CSSValueID::kBottom>,
      range, context);
}

const CSSValue* WebkitMaskPositionY::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const FillLayer* curr_layer = &style.MaskLayers();
  return ComputedStyleUtils::BackgroundPositionYOrWebkitMaskPositionY(
      style, curr_layer);
}

const CSSValue* WebkitMaskSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBackgroundOrMaskSize(
      range, context, local_context, WebFeature::kNegativeMaskSize);
}

const CSSValue* WebkitMaskSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const FillLayer& fill_layer = style.MaskLayers();
  return ComputedStyleUtils::BackgroundImageOrWebkitMaskSize(style, fill_layer);
}

const CSSValue* WebkitPerspectiveOriginX::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueID::kLeft,
                                                    CSSValueID::kRight>(
      range, context);
}

void WebkitPerspectiveOriginX::ApplyInherit(StyleResolverState& state) const {
  state.StyleBuilder().SetPerspectiveOriginX(
      state.ParentStyle()->PerspectiveOrigin().X());
}

const CSSValue* WebkitPerspectiveOriginY::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueID::kTop,
                                                    CSSValueID::kBottom>(
      range, context);
}

void WebkitPerspectiveOriginY::ApplyInherit(StyleResolverState& state) const {
  state.StyleBuilder().SetPerspectiveOriginY(
      state.ParentStyle()->PerspectiveOrigin().Y());
}

const CSSValue* WebkitPrintColorAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.PrintColorAdjust());
}

const CSSValue* WebkitRtlOrdering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.RtlOrdering() == EOrder::kVisual
                                        ? CSSValueID::kVisual
                                        : CSSValueID::kLogical);
}

const CSSValue* WebkitRubyPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetRubyPosition());
}

const CSSValue* RubyPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  switch (style.GetRubyPosition()) {
    case blink::RubyPosition::kBefore:
      return CSSIdentifierValue::Create(CSSValueID::kOver);
    case blink::RubyPosition::kAfter:
      return CSSIdentifierValue::Create(CSSValueID::kUnder);
  }
  NOTREACHED();
  return CSSIdentifierValue::Create(CSSValueID::kOver);
}

const CSSValue* WebkitTapHighlightColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color WebkitTapHighlightColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  StyleColor highlight_color = style.TapHighlightColor();
  if (style.ShouldForceColor(highlight_color)) {
    return visited_link
               ? style.GetInternalForcedVisitedCurrentColor(is_current_color)
               : style.GetInternalForcedCurrentColor(is_current_color);
  }
  return style.ResolvedColor(style.TapHighlightColor(), is_current_color);
}

const CSSValue* WebkitTapHighlightColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TapHighlightColor(), CSSValuePhase::kComputedValue);
}

const CSSValue* WebkitTextCombine::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.TextCombine() == ETextCombine::kAll) {
    return CSSIdentifierValue::Create(CSSValueID::kHorizontal);
  }
  return CSSIdentifierValue::Create(style.TextCombine());
}

const CSSValue* WebkitTextDecorationsInEffect::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeTextDecorationLine(range);
}

const CSSValue*
WebkitTextDecorationsInEffect::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::RenderTextDecorationFlagsToCSSValue(
      style.TextDecorationsInEffect());
}

const CSSValue* TextEmphasisColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color TextEmphasisColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  StyleColor text_emphasis_color = style.TextEmphasisColor();
  if (style.ShouldForceColor(text_emphasis_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return text_emphasis_color.Resolve(style.GetCurrentColor(),
                                     style.UsedColorScheme(), is_current_color);
}

const CSSValue* TextEmphasisColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TextEmphasisColor(), CSSValuePhase::kComputedValue);
}

// [ over | under ] && [ right | left ]?
// If [ right | left ] is omitted, it defaults to right.
const CSSValue* TextEmphasisPosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSIdentifierValue* values[2] = {
      css_parsing_utils::ConsumeIdent<CSSValueID::kOver, CSSValueID::kUnder,
                                      CSSValueID::kRight, CSSValueID::kLeft>(
          range),
      nullptr};
  if (!values[0]) {
    return nullptr;
  }
  values[1] =
      css_parsing_utils::ConsumeIdent<CSSValueID::kOver, CSSValueID::kUnder,
                                      CSSValueID::kRight, CSSValueID::kLeft>(
          range);
  CSSIdentifierValue* over_under = nullptr;
  CSSIdentifierValue* left_right = nullptr;

  for (auto* value : values) {
    if (!value) {
      break;
    }
    switch (value->GetValueID()) {
      case CSSValueID::kOver:
      case CSSValueID::kUnder:
        if (over_under) {
          return nullptr;
        }
        over_under = value;
        break;
      case CSSValueID::kLeft:
      case CSSValueID::kRight:
        if (left_right) {
          return nullptr;
        }
        left_right = value;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  if (!over_under) {
    return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*over_under);
  if (left_right) {
    list->Append(*left_right);
  }
  return list;
}

const CSSValue* TextEmphasisPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  switch (style.GetTextEmphasisPosition()) {
    case blink::TextEmphasisPosition::kOverRight:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kOver));
      break;
    case blink::TextEmphasisPosition::kOverLeft:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kOver));
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kLeft));
      break;
    case blink::TextEmphasisPosition::kUnderRight:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnder));
      break;
    case blink::TextEmphasisPosition::kUnderLeft:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnder));
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kLeft));
      break;
  }
  return list;
}

const CSSValue* TextEmphasisStyle::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  if (CSSValue* text_emphasis_style = css_parsing_utils::ConsumeString(range)) {
    return text_emphasis_style;
  }

  CSSIdentifierValue* fill =
      css_parsing_utils::ConsumeIdent<CSSValueID::kFilled, CSSValueID::kOpen>(
          range);
  CSSIdentifierValue* shape = css_parsing_utils::ConsumeIdent<
      CSSValueID::kDot, CSSValueID::kCircle, CSSValueID::kDoubleCircle,
      CSSValueID::kTriangle, CSSValueID::kSesame>(range);
  if (!fill) {
    fill =
        css_parsing_utils::ConsumeIdent<CSSValueID::kFilled, CSSValueID::kOpen>(
            range);
  }
  if (fill && shape) {
    CSSValueList* parsed_values = CSSValueList::CreateSpaceSeparated();
    parsed_values->Append(*fill);
    parsed_values->Append(*shape);
    return parsed_values;
  }
  if (fill) {
    return fill;
  }
  if (shape) {
    return shape;
  }
  return nullptr;
}

const CSSValue* TextEmphasisStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  switch (style.GetTextEmphasisMark()) {
    case TextEmphasisMark::kNone:
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    case TextEmphasisMark::kCustom:
      return MakeGarbageCollected<CSSStringValue>(
          style.TextEmphasisCustomMark());
    case TextEmphasisMark::kAuto:
      NOTREACHED();
      [[fallthrough]];
    case TextEmphasisMark::kDot:
    case TextEmphasisMark::kCircle:
    case TextEmphasisMark::kDoubleCircle:
    case TextEmphasisMark::kTriangle:
    case TextEmphasisMark::kSesame: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (style.GetTextEmphasisFill() != TextEmphasisFill::kFilled) {
        list->Append(*CSSIdentifierValue::Create(style.GetTextEmphasisFill()));
      }
      list->Append(*CSSIdentifierValue::Create(style.GetTextEmphasisMark()));
      return list;
    }
  }
  NOTREACHED();
  return nullptr;
}

void TextEmphasisStyle::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetTextEmphasisFill(
      ComputedStyleInitialValues::InitialTextEmphasisFill());
  builder.SetTextEmphasisMark(
      ComputedStyleInitialValues::InitialTextEmphasisMark());
  builder.SetTextEmphasisCustomMark(
      ComputedStyleInitialValues::InitialTextEmphasisCustomMark());
}

void TextEmphasisStyle::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetTextEmphasisFill(state.ParentStyle()->GetTextEmphasisFill());
  builder.SetTextEmphasisMark(state.ParentStyle()->GetTextEmphasisMark());
  builder.SetTextEmphasisCustomMark(
      state.ParentStyle()->TextEmphasisCustomMark());
}

void TextEmphasisStyle::ApplyValue(StyleResolverState& state,
                                   const CSSValue& value) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    DCHECK_EQ(list->length(), 2U);
    for (unsigned i = 0; i < 2; ++i) {
      const auto& ident_value = To<CSSIdentifierValue>(list->Item(i));
      if (ident_value.GetValueID() == CSSValueID::kFilled ||
          ident_value.GetValueID() == CSSValueID::kOpen) {
        builder.SetTextEmphasisFill(ident_value.ConvertTo<TextEmphasisFill>());
      } else {
        builder.SetTextEmphasisMark(ident_value.ConvertTo<TextEmphasisMark>());
      }
    }
    builder.SetTextEmphasisCustomMark(g_null_atom);
    return;
  }

  if (auto* string_value = DynamicTo<CSSStringValue>(value)) {
    builder.SetTextEmphasisFill(TextEmphasisFill::kFilled);
    builder.SetTextEmphasisMark(TextEmphasisMark::kCustom);
    builder.SetTextEmphasisCustomMark(AtomicString(string_value->Value()));
    return;
  }

  const auto& identifier_value = To<CSSIdentifierValue>(value);

  builder.SetTextEmphasisCustomMark(g_null_atom);

  if (identifier_value.GetValueID() == CSSValueID::kFilled ||
      identifier_value.GetValueID() == CSSValueID::kOpen) {
    builder.SetTextEmphasisFill(identifier_value.ConvertTo<TextEmphasisFill>());
    builder.SetTextEmphasisMark(TextEmphasisMark::kAuto);
  } else {
    builder.SetTextEmphasisFill(TextEmphasisFill::kFilled);
    builder.SetTextEmphasisMark(identifier_value.ConvertTo<TextEmphasisMark>());
  }
}

const CSSValue* WebkitTextFillColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color WebkitTextFillColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  StyleColor text_fill_color = style.TextFillColor();
  if (style.ShouldForceColor(text_fill_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return text_fill_color.Resolve(style.GetCurrentColor(),
                                 style.UsedColorScheme(), is_current_color);
}

const CSSValue* WebkitTextFillColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TextFillColor(), CSSValuePhase::kComputedValue);
}

const CSSValue* WebkitTextOrientation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.GetTextOrientation() == ETextOrientation::kMixed) {
    return CSSIdentifierValue::Create(CSSValueID::kVerticalRight);
  }
  return CSSIdentifierValue::Create(style.GetTextOrientation());
}

const CSSValue* WebkitTextSecurity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.TextSecurity());
}

const CSSValue* WebkitTextStrokeColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color WebkitTextStrokeColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  StyleColor text_stroke_color = style.TextStrokeColor();
  if (style.ShouldForceColor(text_stroke_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return text_stroke_color.Resolve(style.GetCurrentColor(),
                                   style.UsedColorScheme(), is_current_color);
}

const CSSValue* WebkitTextStrokeColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TextStrokeColor(), CSSValuePhase::kComputedValue);
}

const CSSValue* WebkitTextStrokeWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLineWidth(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* WebkitTextStrokeWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.TextStrokeWidth(), style);
}

const CSSValue* ToggleGroup::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeToggleGroup, range, context);
}

const CSSValue* ToggleGroup::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const auto* toggle_group = style.ToggleGroup();
  if (!toggle_group) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* result_list = CSSValueList::CreateCommaSeparated();
  for (const auto& item : toggle_group->Groups()) {
    CSSValueList* item_list = CSSValueList::CreateSpaceSeparated();
    item_list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(item.Name()));
    switch (item.Scope()) {
      case ToggleScope::kWide:
        break;
      case ToggleScope::kNarrow:
        item_list->Append(*CSSIdentifierValue::Create(CSSValueID::kSelf));
        break;
    }
    result_list->Append(*item_list);
  }
  return result_list;
}

const CSSValue* ToggleRoot::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeToggleSpecifier, range, context);
}

const CSSValue* ToggleRoot::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const auto* toggle_root = style.ToggleRoot();
  if (!toggle_root) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* result_list = CSSValueList::CreateCommaSeparated();
  for (const auto& item : toggle_root->Roots()) {
    CSSValueList* item_list = CSSValueList::CreateSpaceSeparated();
    item_list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(item.Name()));
    const auto& states = item.StateSet();
    bool states_is_default = states.IsInteger() && states.AsInteger() == 1u;
    const auto& initial_state = item.InitialState();
    bool initial_is_default =
        initial_state.IsInteger() && initial_state.AsInteger() == 0u;
    if (!states_is_default || !initial_is_default) {
      switch (states.GetType()) {
        using Type = decltype(states.GetType());
        case Type::Integer: {
          auto maximum_state = states.AsInteger();
          item_list->Append(*CSSNumericLiteralValue::Create(
              maximum_state, CSSPrimitiveValue::UnitType::kInteger));
          break;
        }
        case Type::Names: {
          auto* state_list =
              MakeGarbageCollected<cssvalue::CSSBracketedValueList>();
          for (const auto& state_name : states.AsNames()) {
            state_list->Append(
                *MakeGarbageCollected<CSSCustomIdentValue>(state_name));
          }
          item_list->Append(*state_list);
          break;
        }
      }

      if (!initial_is_default) {
        item_list->Append(*CSSIdentifierValue::Create(CSSValueID::kAt));
        switch (initial_state.GetType()) {
          using Type = decltype(initial_state.GetType());
          case Type::Integer: {
            auto initial_state_index = initial_state.AsInteger();
            item_list->Append(*CSSNumericLiteralValue::Create(
                initial_state_index, CSSPrimitiveValue::UnitType::kInteger));
            break;
          }
          case Type::Name: {
            const AtomicString& initial_state_name = initial_state.AsName();
            item_list->Append(
                *MakeGarbageCollected<CSSCustomIdentValue>(initial_state_name));
            break;
          }
        }
      }
    }
    switch (item.Overflow()) {
      case ToggleOverflow::kCycle:
        // serialize nothing since it's the default
        break;
      case ToggleOverflow::kCycleOn:
        item_list->Append(*CSSIdentifierValue::Create(CSSValueID::kCycleOn));
        break;
      case ToggleOverflow::kSticky:
        item_list->Append(*CSSIdentifierValue::Create(CSSValueID::kSticky));
        break;
    }
    if (item.IsGroup()) {
      item_list->Append(*CSSIdentifierValue::Create(CSSValueID::kGroup));
    }
    switch (item.Scope()) {
      case ToggleScope::kWide:
        break;
      case ToggleScope::kNarrow:
        item_list->Append(*CSSIdentifierValue::Create(CSSValueID::kSelf));
        break;
    }
    result_list->Append(*item_list);
  }
  return result_list;
}

const CSSValue* ToggleTrigger::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeToggleTrigger, range, context);
}

const CSSValue* ToggleTrigger::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const auto* toggle_trigger = style.ToggleTrigger();
  if (!toggle_trigger) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* result_list = CSSValueList::CreateCommaSeparated();
  for (const auto& item : toggle_trigger->Triggers()) {
    CSSValueList* item_list = CSSValueList::CreateSpaceSeparated();
    item_list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(item.Name()));
    CSSValueID id = CSSValueID::kInvalid;
    switch (item.Mode()) {
      case ToggleTriggerMode::kPrev:
        id = CSSValueID::kPrev;
        break;
      case ToggleTriggerMode::kNext:
        id = CSSValueID::kNext;
        break;
      case ToggleTriggerMode::kSet:
        id = CSSValueID::kSet;
        break;
    }
    const auto& value = item.Value();
    switch (value.GetType()) {
      using Type = decltype(value.GetType());
      case Type::Integer: {
        auto int_value = value.AsInteger();
        if (id == CSSValueID::kSet || int_value != 1u) {
          item_list->Append(*CSSIdentifierValue::Create(id));
          item_list->Append(*CSSNumericLiteralValue::Create(
              int_value, CSSPrimitiveValue::UnitType::kInteger));
        } else if (id != CSSValueID::kNext) {
          item_list->Append(*CSSIdentifierValue::Create(id));
        }
        break;
      }
      case Type::Name: {
        DCHECK_EQ(id, CSSValueID::kSet);
        item_list->Append(*CSSIdentifierValue::Create(id));
        item_list->Append(
            *MakeGarbageCollected<CSSCustomIdentValue>(value.AsName()));
        break;
      }
    }
    result_list->Append(*item_list);
  }
  return result_list;
}

const CSSValue* WebkitTransformOriginX::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueID::kLeft,
                                                    CSSValueID::kRight>(
      range, context);
}

void WebkitTransformOriginX::ApplyInherit(StyleResolverState& state) const {
  state.StyleBuilder().SetTransformOriginX(
      state.ParentStyle()->GetTransformOrigin().X());
}

const CSSValue* ToggleVisibility::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSIdentifierValue* ident =
      css_parsing_utils::ConsumeIdent<CSSValueID::kNormal, CSSValueID::kToggle>(
          range);
  if (!ident || ident->GetValueID() == CSSValueID::kNormal) {
    return ident;
  }

  CSSValueList* result_list = CSSValueList::CreateSpaceSeparated();
  result_list->Append(*ident);
  CSSValue* name = css_parsing_utils::ConsumeCustomIdent(range, context);
  if (!name) {
    return nullptr;
  }
  result_list->Append(*name);
  return result_list;
}

const CSSValue* ToggleVisibility::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const AtomicString& toggle_visibility = style.ToggleVisibility();
  if (toggle_visibility.IsNull()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  CSSValueList* result_list = CSSValueList::CreateSpaceSeparated();
  result_list->Append(*CSSIdentifierValue::Create(CSSValueID::kToggle));
  result_list->Append(
      *MakeGarbageCollected<CSSCustomIdentValue>(toggle_visibility));
  return result_list;
}

const CSSValue* TopLayer::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.TopLayer());
}

const CSSValue* WebkitTransformOriginY::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueID::kTop,
                                                    CSSValueID::kBottom>(
      range, context);
}

void WebkitTransformOriginY::ApplyInherit(StyleResolverState& state) const {
  state.StyleBuilder().SetTransformOriginY(
      state.ParentStyle()->GetTransformOrigin().Y());
}

const CSSValue* WebkitTransformOriginZ::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(range, context,
                                          CSSPrimitiveValue::ValueRange::kAll);
}

void WebkitTransformOriginZ::ApplyInherit(StyleResolverState& state) const {
  state.StyleBuilder().SetTransformOriginZ(
      state.ParentStyle()->GetTransformOrigin().Z());
}

const CSSValue* WebkitUserDrag::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.UserDrag());
}

const CSSValue* WebkitUserModify::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.UserModify());
}

const CSSValue* WebkitWritingMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetWritingMode());
}

const CSSValue* WhiteSpace::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.WhiteSpace());
}

const CSSValue* Widows::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositiveInteger(range, context);
}

const CSSValue* Widows::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.Widows(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* Width::ParseSingleValue(CSSParserTokenRange& range,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(
      range, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

bool Width::IsLayoutDependent(const ComputedStyle* style,
                              LayoutObject* layout_object) const {
  return layout_object && (layout_object->IsBox() || layout_object->IsSVG());
}

const CSSValue* Width::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (ComputedStyleUtils::WidthOrHeightShouldReturnUsedValue(layout_object)) {
    return ZoomAdjustedPixelValue(
        ComputedStyleUtils::UsedBoxSize(*layout_object).width(), style);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Width(),
                                                             style);
}

const CSSValue* WillChange::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }

  CSSValueList* values = CSSValueList::CreateCommaSeparated();
  // Every comma-separated list of identifiers is a valid will-change value,
  // unless the list includes an explicitly disallowed identifier.
  while (true) {
    if (range.Peek().GetType() != kIdentToken) {
      return nullptr;
    }
    CSSPropertyID unresolved_property = UnresolvedCSSPropertyID(
        context.GetExecutionContext(), range.Peek().Value());
    if (unresolved_property != CSSPropertyID::kInvalid &&
        unresolved_property != CSSPropertyID::kVariable) {
#if DCHECK_IS_ON()
      DCHECK(CSSProperty::Get(ResolveCSSPropertyID(unresolved_property))
                 .IsWebExposed(context.GetExecutionContext()));
#endif
      // Now "all" is used by both CSSValue and CSSPropertyValue.
      // Need to return nullptr when currentValue is CSSPropertyID::kAll.
      if (unresolved_property == CSSPropertyID::kWillChange ||
          unresolved_property == CSSPropertyID::kAll) {
        return nullptr;
      }
      values->Append(
          *MakeGarbageCollected<CSSCustomIdentValue>(unresolved_property));
      range.ConsumeIncludingWhitespace();
    } else {
      switch (range.Peek().Id()) {
        case CSSValueID::kNone:
        case CSSValueID::kAll:
        case CSSValueID::kAuto:
        case CSSValueID::kDefault:
        case CSSValueID::kInitial:
        case CSSValueID::kInherit:
        case CSSValueID::kRevert:
          return nullptr;
        case CSSValueID::kContents:
        case CSSValueID::kScrollPosition:
          values->Append(*css_parsing_utils::ConsumeIdent(range));
          break;
        default:
          range.ConsumeIncludingWhitespace();
          break;
      }
    }

    if (range.AtEnd()) {
      break;
    }
    if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(range)) {
      return nullptr;
    }
  }

  return values;
}

const CSSValue* WillChange::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForWillChange(
      style.WillChangeProperties(), style.WillChangeContents(),
      style.WillChangeScrollPosition());
}

void WillChange::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetWillChangeContents(false);
  builder.SetWillChangeScrollPosition(false);
  builder.SetWillChangeProperties(Vector<CSSPropertyID>());
  builder.SetSubtreeWillChangeContents(
      state.ParentStyle()->SubtreeWillChangeContents());
}

void WillChange::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetWillChangeContents(state.ParentStyle()->WillChangeContents());
  builder.SetWillChangeScrollPosition(
      state.ParentStyle()->WillChangeScrollPosition());
  builder.SetWillChangeProperties(state.ParentStyle()->WillChangeProperties());
  builder.SetSubtreeWillChangeContents(
      state.ParentStyle()->SubtreeWillChangeContents());
}

void WillChange::ApplyValue(StyleResolverState& state,
                            const CSSValue& value) const {
  bool will_change_contents = false;
  bool will_change_scroll_position = false;
  Vector<CSSPropertyID> will_change_properties;

  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kAuto);
  } else {
    for (auto& will_change_value : To<CSSValueList>(value)) {
      if (auto* ident_value =
              DynamicTo<CSSCustomIdentValue>(will_change_value.Get())) {
        will_change_properties.push_back(ident_value->ValueAsPropertyID());
      } else if (To<CSSIdentifierValue>(*will_change_value).GetValueID() ==
                 CSSValueID::kContents) {
        will_change_contents = true;
      } else if (To<CSSIdentifierValue>(*will_change_value).GetValueID() ==
                 CSSValueID::kScrollPosition) {
        will_change_scroll_position = true;
      } else {
        NOTREACHED();
      }
    }
  }
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetWillChangeContents(will_change_contents);
  builder.SetWillChangeScrollPosition(will_change_scroll_position);
  builder.SetWillChangeProperties(will_change_properties);
  builder.SetSubtreeWillChangeContents(
      will_change_contents || state.ParentStyle()->SubtreeWillChangeContents());
}

const CSSValue* WordBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.WordBreak());
}

const CSSValue* WordSpacing::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseSpacing(range, context);
}

const CSSValue* WordSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.WordSpacing(), style);
}

const CSSValue* WritingMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetWritingMode());
}

void WritingMode::ApplyInitial(StyleResolverState& state) const {
  state.SetWritingMode(ComputedStyleInitialValues::InitialWritingMode());
}

void WritingMode::ApplyInherit(StyleResolverState& state) const {
  state.SetWritingMode(state.ParentStyle()->GetWritingMode());
}

void WritingMode::ApplyValue(StyleResolverState& state,
                             const CSSValue& value) const {
  state.SetWritingMode(
      To<CSSIdentifierValue>(value).ConvertTo<blink::WritingMode>());
}

const CSSValue* X::ParseSingleValue(CSSParserTokenRange& range,
                                    const CSSParserContext& context,
                                    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* X::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.X(), style);
}

const CSSValue* Y::ParseSingleValue(CSSParserTokenRange& range,
                                    const CSSParserContext& context,
                                    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      range, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* Y::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Y(), style);
}

const CSSValue* ZIndex::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return css_parsing_utils::ConsumeInteger(range, context);
}

const CSSValue* ZIndex::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HasAutoZIndex()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return CSSNumericLiteralValue::Create(style.ZIndex(),
                                        CSSPrimitiveValue::UnitType::kInteger);
}

const CSSValue* Zoom::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  const CSSParserToken& token = range.Peek();
  CSSValue* zoom = nullptr;
  if (token.GetType() == kIdentToken) {
    zoom = css_parsing_utils::ConsumeIdent<CSSValueID::kNormal>(range);
  } else {
    zoom = css_parsing_utils::ConsumePercent(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!zoom) {
      zoom = css_parsing_utils::ConsumeNumber(
          range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    }
  }
  if (zoom) {
    if (!(token.Id() == CSSValueID::kNormal ||
          (token.GetType() == kNumberToken &&
           To<CSSPrimitiveValue>(zoom)->GetDoubleValue() == 1) ||
          (token.GetType() == kPercentageToken &&
           To<CSSPrimitiveValue>(zoom)->GetDoubleValue() == 100))) {
      context.Count(WebFeature::kCSSZoomNotEqualToOne);
    }
  }
  return zoom;
}

const CSSValue* Zoom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.Zoom(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

void Zoom::ApplyInitial(StyleResolverState& state) const {
  state.SetZoom(ComputedStyleInitialValues::InitialZoom());
}

void Zoom::ApplyInherit(StyleResolverState& state) const {
  state.SetZoom(state.ParentStyle()->Zoom());
}

void Zoom::ApplyValue(StyleResolverState& state, const CSSValue& value) const {
  state.SetZoom(StyleBuilderConverter::ConvertZoom(state, value));
}

const CSSValue* InternalAlignSelfBlock::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIdent<CSSValueID::kCenter,
                                         CSSValueID::kNormal>(range);
}

const CSSValue* InternalEmptyLineHeight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIdent<CSSValueID::kFabricated,
                                         CSSValueID::kNone>(range);
}

}  // namespace css_longhand
}  // namespace blink
