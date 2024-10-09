// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/numerics/clamped_math.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/css_anchor_query_enums.h"
#include "third_party/blink/renderer/core/css/css_appearance_auto_base_select_value_pair.h"
#include "third_party/blink/renderer/core/css/css_axis_value.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"
#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_cursor_image_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_dynamic_range_limit_mix_value.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_font_variation_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_initial_color_value.h"
#include "third_party/blink/renderer/core/css/css_layout_function_value.h"
#include "third_party/blink/renderer/core/css/css_light_dark_value_pair.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/css_ratio_value.h"
#include "third_party/blink/renderer/core/css/css_reflect_value.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_scoped_keyword_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_save_point.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
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
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/zoom_adjusted_pixel_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/coord_box_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/geometry_box_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/core/style/paint_order_array.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/reference_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/style_overflow_clip_margin.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_style_tracker.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
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

CSSCustomIdentValue* ConsumeCustomIdentExcludingNone(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return nullptr;
  }
  return css_parsing_utils::ConsumeCustomIdent(stream, context);
}

// ConsumeAppearanceAutoBaseSelect is used for parsing values of properties
// which need to support -internal-appearance-auto-base-select() in the UA
// stylesheet. `consume_value` is a function which consumes the individual
// values provided inside -internal-appearance-auto-base-select() and is called
// twice in order to consume each value. If
// -internal-appearance-auto-base-select() is not found, then this function just
// calls consume_value on the input and returns the result.
template <typename Func, typename... Args>
const CSSValue* ConsumeAppearanceAutoBaseSelect(Func consume_value,
                                                CSSParserTokenStream& stream,
                                                const CSSParserContext& context,
                                                Args&&... args) {
  if (!RuntimeEnabledFeatures::CustomizableSelectEnabled() ||
      !IsUASheetBehavior(context.Mode()) ||
      stream.Peek().FunctionId() !=
          CSSValueID::kInternalAppearanceAutoBaseSelect) {
    return consume_value(stream, context, std::forward<Args>(args)...);
  }

  const CSSValue* auto_value;
  const CSSValue* base_select_value;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    auto_value = consume_value(stream, context, std::forward<Args>(args)...);
    if (!auto_value ||
        !css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
      return nullptr;
    }
    base_select_value =
        consume_value(stream, context, std::forward<Args>(args)...);
    if (!base_select_value || !stream.AtEnd()) {
      return nullptr;
    }
    guard.Release();
  }
  stream.ConsumeWhitespace();
  return MakeGarbageCollected<CSSAppearanceAutoBaseSelectValuePair>(
      auto_value, base_select_value);
}

}  // namespace

namespace css_longhand {

const CSSValue* AlignContent::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeContentDistributionOverflowPosition(
      stream, css_parsing_utils::IsContentPositionKeyword);
}

const CSSValue* AlignContent::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::
      ValueForContentPositionAndDistributionWithOverflowAlignment(
          style.AlignContent());
}

const CSSValue* AlignItems::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // align-items property does not allow the 'auto' value.
  if (css_parsing_utils::IdentMatches<CSSValueID::kAuto>(stream.Peek().Id())) {
    return nullptr;
  }
  return css_parsing_utils::ConsumeSelfPositionOverflowPosition(
      stream, css_parsing_utils::IsSelfPositionKeyword);
}

const CSSValue* AlignItems::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForItemPositionWithOverflowAlignment(
      style.AlignItems());
}

const CSSValue* AlignSelf::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSelfPositionOverflowPosition(
      stream, css_parsing_utils::IsSelfPositionKeyword);
}

const CSSValue* AlignSelf::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForItemPositionWithOverflowAlignment(
      style.AlignSelf());
}
const CSSValue* AlignmentBaseline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.AlignmentBaseline());
}

const CSSValue* PositionAnchor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (CSSValue* value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kAuto>(stream)) {
    return value;
  }
  return css_parsing_utils::ConsumeDashedIdent(stream, context);
}
const CSSValue* PositionAnchor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (!style.PositionAnchor()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(*style.PositionAnchor());
}

void PositionAnchor::ApplyInitial(StyleResolverState& state) const {
  state.SetPositionAnchor(ComputedStyleInitialValues::InitialPositionAnchor());
}

void PositionAnchor::ApplyInherit(StyleResolverState& state) const {
  state.SetPositionAnchor(state.ParentStyle()->PositionAnchor());
}

void PositionAnchor::ApplyValue(StyleResolverState& state,
                                const CSSValue& value,
                                ValueMode) const {
  state.SetPositionAnchor(
      StyleBuilderConverter::ConvertPositionAnchor(state, value));
}

// https://drafts.csswg.org/css-anchor-position-1/#position-visibility
// position-visibility:
//   always | [ anchors-valid | anchors-visible ] || no-overflow
// TODO(crbug.com/332933527): Support anchors-valid. For now,
// we only support the modified grammar:
//   position-visibility: always | anchors-visible || no-overflow
const CSSValue* PositionVisibility::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAlways) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSIdentifierValue* anchors_visible =
      css_parsing_utils::ConsumeIdent<CSSValueID::kAnchorsVisible>(stream);
  CSSIdentifierValue* no_overflow =
      css_parsing_utils::ConsumeIdent<CSSValueID::kNoOverflow>(stream);
  if (!anchors_visible) {
    anchors_visible =
        css_parsing_utils::ConsumeIdent<CSSValueID::kAnchorsVisible>(stream);
  }

  if (!anchors_visible && !no_overflow) {
    return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (anchors_visible) {
    list->Append(*anchors_visible);
  }
  if (no_overflow) {
    list->Append(*no_overflow);
  }
  return list;
}

const CSSValue* PositionVisibility::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  blink::PositionVisibility position_visibility = style.GetPositionVisibility();
  if (position_visibility == blink::PositionVisibility::kAlways) {
    return CSSIdentifierValue::Create(CSSValueID::kAlways);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (EnumHasFlags(position_visibility,
                   blink::PositionVisibility::kAnchorsVisible)) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAnchorsVisible));
  }
  if (EnumHasFlags(position_visibility,
                   blink::PositionVisibility::kNoOverflow)) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kNoOverflow));
  }
  return list;
}

// anchor-name: none | <dashed-ident>#
const CSSValue* AnchorName::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (CSSValue* value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(stream)) {
    return value;
  }
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeDashedIdent, stream, context);
}
const CSSValue* AnchorName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (!style.AnchorName()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const Member<const ScopedCSSName>& name :
       style.AnchorName()->GetNames()) {
    list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(*name));
  }
  return list;
}

const CSSValue* AnchorScope::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (CSSValue* value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(stream)) {
    return value;
  }
  if (CSSValue* value =
          css_parsing_utils::ConsumeScopedKeywordValue<CSSValueID::kAll>(
              stream)) {
    return value;
  }
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeDashedIdent, stream, context);
}

const CSSValue* AnchorScope::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const StyleAnchorScope& anchor_scope = style.AnchorScope();
  if (anchor_scope.IsNone()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  if (anchor_scope.IsAll()) {
    return CSSIdentifierValue::Create(CSSValueID::kAll);
  }
  CHECK(anchor_scope.Names());
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const Member<const ScopedCSSName>& name :
       anchor_scope.Names()->GetNames()) {
    list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(*name));
  }
  return list;
}

const CSSValue* AnimationComposition::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList<CSSIdentifierValue*(
      CSSParserTokenStream&)>(
      css_parsing_utils::ConsumeIdent<CSSValueID::kReplace, CSSValueID::kAdd,
                                      CSSValueID::kAccumulate>,
      stream);
}

const CSSValue* AnimationComposition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      static_cast<CSSPrimitiveValue* (*)(CSSParserTokenStream&,
                                         const CSSParserContext&,
                                         CSSPrimitiveValue::ValueRange)>(
          css_parsing_utils::ConsumeTime),
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* AnimationDelay::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationDelayList(style.Animations());
}

const CSSValue* AnimationDelay::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (ComputedStyleUtils::ValueForAnimationDelay(
                          CSSTimingData::InitialDelayStart())));
  return value;
}

const CSSValue* AnimationDirection::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList<CSSIdentifierValue*(
      CSSParserTokenStream&)>(
      css_parsing_utils::ConsumeIdent<
          CSSValueID::kNormal, CSSValueID::kAlternate, CSSValueID::kReverse,
          CSSValueID::kAlternateReverse>,
      stream);
}

const CSSValue* AnimationDirection::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationDirectionList(style.Animations());
}

const CSSValue* AnimationDirection::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNormal);
}

const CSSValue* AnimationDuration::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationDuration, stream, context);
}

const CSSValue* AnimationDuration::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationDurationList(style.Animations(),
                                                           value_phase);
}

const CSSValue* AnimationDuration::InitialValue() const {
  return ComputedStyleUtils::ValueForAnimationDuration(
      CSSAnimationData::InitialDuration(), /* resolve_auto_to_zero */ false);
}

const CSSValue* AnimationFillMode::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList<CSSIdentifierValue*(
      CSSParserTokenStream&)>(
      css_parsing_utils::ConsumeIdent<CSSValueID::kNone, CSSValueID::kForwards,
                                      CSSValueID::kBackwards,
                                      CSSValueID::kBoth>,
      stream);
}

const CSSValue* AnimationFillMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationFillModeList(style.Animations());
}

const CSSValue* AnimationFillMode::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* AnimationIterationCount::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationIterationCount, stream, context);
}

const CSSValue* AnimationIterationCount::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  // Allow quoted name if this is an alias property.
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationName, stream, context,
      local_context.UseAliasParsing());
}

const CSSValue* AnimationName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList<CSSIdentifierValue*(
      CSSParserTokenStream&)>(
      css_parsing_utils::ConsumeIdent<CSSValueID::kRunning,
                                      CSSValueID::kPaused>,
      stream);
}

const CSSValue* AnimationPlayState::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationPlayStateList(style.Animations());
}

const CSSValue* AnimationPlayState::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kRunning);
}

const CSSValue* AnimationRangeStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::ScrollTimelineEnabled());
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationRange, stream, context,
      /* default_offset_percent */ 0.0);
}

const CSSValue* AnimationRangeStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationRangeStartList(style.Animations(),
                                                             style);
}

const CSSValue* AnimationRangeStart::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNormal);
}

const CSSValue* AnimationRangeEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::ScrollTimelineEnabled());
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationRange, stream, context,
      /* default_offset_percent */ 100.0);
}

const CSSValue* AnimationRangeEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationRangeEndList(style.Animations(),
                                                           style);
}

const CSSValue* AnimationRangeEnd::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNormal);
}

const CSSValue* AnimationTimeline::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationTimeline, stream, context);
}

const CSSValue* AnimationTimeline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationTimelineList(style.Animations());
}

const CSSValue* AnimationTimeline::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kAuto);
}

const CSSValue* AnimationTimingFunction::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationTimingFunction, stream, context);
}

const CSSValue* AnimationTimingFunction::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationTimingFunctionList(
      style.Animations());
}

const CSSValue* AnimationTimingFunction::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kEase);
}

const CSSValue* AspectRatio::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // Syntax: auto | auto 1/2 | 1/2 auto | 1/2
  CSSValue* auto_value = nullptr;
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    auto_value = css_parsing_utils::ConsumeIdent(stream);
  }

  CSSValue* ratio = css_parsing_utils::ConsumeRatio(stream, context);
  if (!ratio) {
    return auto_value;  // Either auto alone, or failure.
  }

  if (!auto_value && stream.Peek().Id() == CSSValueID::kAuto) {
    auto_value = css_parsing_utils::ConsumeIdent(stream);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  auto& ratio = style.AspectRatio();
  if (ratio.GetTypeForComputedStyle() == EAspectRatioType::kAuto) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  auto* ratio_value = MakeGarbageCollected<cssvalue::CSSRatioValue>(
      *CSSNumericLiteralValue::Create(ratio.GetRatio().width(),
                                      CSSPrimitiveValue::UnitType::kNumber),
      *CSSNumericLiteralValue::Create(ratio.GetRatio().height(),
                                      CSSPrimitiveValue::UnitType::kNumber));

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (ratio.GetTypeForComputedStyle() != EAspectRatioType::kRatio) {
    DCHECK_EQ(ratio.GetTypeForComputedStyle(), EAspectRatioType::kAutoAndRatio);
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  }

  list->Append(*ratio_value);
  return list;
}

const CSSValue* BackdropFilter::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFilterFunctionList(stream, context);
}

const CSSValue* BackdropFilter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFilter(style, style.BackdropFilter());
}

void BackdropFilter::ApplyValue(StyleResolverState& state,
                                const CSSValue& value,
                                ValueMode) const {
  state.StyleBuilder().SetBackdropFilter(
      StyleBuilderConverter::ConvertFilterOperations(state, value,
                                                     PropertyID()));
}

const CSSValue* BackfaceVisibility::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(
      (style.BackfaceVisibility() == EBackfaceVisibility::kHidden)
          ? CSSValueID::kHidden
          : CSSValueID::kVisible);
}

const CSSValue* BackgroundAttachment::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeBackgroundAttachment, stream);
}

const CSSValue* BackgroundAttachment::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const FillLayer* curr_layer = &style.BackgroundLayers(); curr_layer;
       curr_layer = curr_layer->Next()) {
    list->Append(*CSSIdentifierValue::Create(curr_layer->Attachment()));
  }
  return list;
}

const CSSValue* BackgroundBlendMode::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeBackgroundBlendMode, stream);
}

const CSSValue* BackgroundBlendMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const FillLayer* curr_layer = &style.BackgroundLayers(); curr_layer;
       curr_layer = curr_layer->Next()) {
    list->Append(*CSSIdentifierValue::Create(curr_layer->GetBlendMode()));
  }
  return list;
}

const CSSValue* BackgroundClip::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext& local_context) const {
  if (RuntimeEnabledFeatures::CSSBackgroundClipUnprefixEnabled()) {
    return css_parsing_utils::ConsumeCommaSeparatedList(
        css_parsing_utils::ConsumeBackgroundBoxOrText, stream);
  } else {
    return css_parsing_utils::ParseBackgroundBox(
        stream, local_context, css_parsing_utils::AllowTextValue::kAllow);
  }
}

const CSSValue* BackgroundClip::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
                                const CSSValue& value,
                                ValueMode) const {
  Document& document = state.GetDocument();
  FillLayer* curr_child = &state.StyleBuilder().AccessBackgroundLayers();
  FillLayer* prev_child = nullptr;
  const auto* value_list = DynamicTo<CSSValueList>(value);
  if (value_list && !value.IsImageSetValue()) {
    // Walk each value and put it into a layer, creating new layers as needed.
    // As per https://w3c.github.io/csswg-drafts/css-backgrounds/#layering
    while (curr_child) {
      for (auto curr_val : *value_list) {
        if (!curr_child) {
          curr_child = prev_child->EnsureNext();
        }
        CSSToStyleMap::MapFillClip(state, curr_child, *curr_val);
        UseCountBackgroundClip(document, *curr_val);
        prev_child = curr_child;
        curr_child = curr_child->Next();
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColorMaybeQuirky(stream, context);
}

const blink::Color BackgroundColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& background_color = style.BackgroundColor();
  if (!style.InForcedColorsMode() && !background_color.HasColorKeyword() &&
      !background_color.IsUnresolvedColorFunction()) {
    // Fast path.
    if (is_current_color) {
      *is_current_color = false;
    }
    return background_color.GetColor();
  } else {
    if (style.ShouldForceColor(background_color)) {
      return GetCSSPropertyInternalForcedBackgroundColor()
          .ColorIncludingFallback(false, style, is_current_color);
    }
    return background_color.Resolve(style.GetCurrentColor(),
                                    style.UsedColorScheme(), is_current_color);
  }
}

const CSSValue* BackgroundColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (allow_visited_style) {
    return cssvalue::CSSColor::Create(style.VisitedDependentColor(*this));
  }

  const StyleColor& background_color = style.BackgroundColor();
  if (value_phase == CSSValuePhase::kResolvedValue &&
      style.ShouldForceColor(background_color)) {
    return GetCSSPropertyInternalForcedBackgroundColor()
        .CSSValueFromComputedStyle(style, nullptr, allow_visited_style,
                                   value_phase);
  }
  return ComputedStyleUtils::CurrentColorOrValidColor(style, background_color,
                                                      value_phase);
}

const CSSValue* BackgroundImage::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeImageOrNone, stream, context);
}

const CSSValue* BackgroundImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const FillLayer& fill_layer = style.BackgroundLayers();
  return ComputedStyleUtils::BackgroundImageOrMaskImage(
      style, allow_visited_style, fill_layer, value_phase);
}

const CSSValue* BackgroundOrigin::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBackgroundBox(
      stream, local_context, css_parsing_utils::AllowTextValue::kForbid);
}

const CSSValue* BackgroundOrigin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.BackgroundLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    EFillBox box = curr_layer->Origin();
    list->Append(*CSSIdentifierValue::Create(box));
  }
  return list;
}

const CSSValue* BackgroundPositionX::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumePositionLonghand<CSSValueID::kLeft,
                                                 CSSValueID::kRight>,
      stream, context);
}

const CSSValue* BackgroundPositionX::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const FillLayer* curr_layer = &style.BackgroundLayers();
  return ComputedStyleUtils::BackgroundPositionXOrWebkitMaskPositionX(
      style, curr_layer);
}

const CSSValue* BackgroundPositionY::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumePositionLonghand<CSSValueID::kTop,
                                                 CSSValueID::kBottom>,
      stream, context);
}

const CSSValue* BackgroundPositionY::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const FillLayer* curr_layer = &style.BackgroundLayers();
  return ComputedStyleUtils::BackgroundPositionYOrWebkitMaskPositionY(
      style, curr_layer);
}

const CSSValue* BackgroundSize::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBackgroundSize(
      stream, context, local_context, WebFeature::kNegativeBackgroundSize);
}

const CSSValue* BackgroundSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const FillLayer& fill_layer = style.BackgroundLayers();
  return ComputedStyleUtils::BackgroundImageOrMaskSize(style, fill_layer);
}

const CSSValue* BackgroundRepeat::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseRepeatStyle(stream);
}

const CSSValue* BackgroundRepeat::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::RepeatStyle(&style.BackgroundLayers());
}

const CSSValue* BaselineSource::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BaselineSource());
}

const CSSValue* BaselineShift::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kBaseline || id == CSSValueID::kSub ||
      id == CSSValueID::kSuper) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  return css_parsing_utils::ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* BaselineShift::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  switch (style.BaselineShiftType()) {
    case EBaselineShiftType::kSuper:
      return CSSIdentifierValue::Create(CSSValueID::kSuper);
    case EBaselineShiftType::kSub:
      return CSSIdentifierValue::Create(CSSValueID::kSub);
    case EBaselineShiftType::kLength:
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.BaselineShift(), style);
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void BaselineShift::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetBaselineShiftType(state.ParentStyle()->BaselineShiftType());
  builder.SetBaselineShift(state.ParentStyle()->BaselineShift());
}

void BaselineShift::ApplyValue(StyleResolverState& state,
                               const CSSValue& value,
                               ValueMode) const {
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
        NOTREACHED_IN_MIGRATION();
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(stream, context);
}

bool BlockSize::IsLayoutDependent(const ComputedStyle* style,
                                  LayoutObject* layout_object) const {
  return layout_object && (layout_object->IsBox() || layout_object->IsSVG());
}

const CSSValue* BorderBlockEndColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const CSSValue* BorderBlockEndWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderWidth(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* BorderBlockStartColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const CSSValue* BorderBlockStartWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderWidth(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* BorderBottomColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const blink::Color BorderBottomColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& border_bottom_color = style.BorderBottomColor();
  if (style.ShouldForceColor(border_bottom_color)) {
    return GetCSSPropertyInternalForcedBorderColor().ColorIncludingFallback(
        false, style, is_current_color);
  }
  return ComputedStyleUtils::BorderSideColor(style, border_bottom_color,
                                             style.BorderBottomStyle(),
                                             visited_link, is_current_color);
}

const CSSValue* BorderBottomColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const StyleColor& border_bottom_color = style.BorderBottomColor();
  if (value_phase == CSSValuePhase::kResolvedValue &&
      style.ShouldForceColor(border_bottom_color)) {
    return GetCSSPropertyInternalForcedBorderColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style, value_phase);
  }
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, border_bottom_color, value_phase);
}

const CSSValue* BorderBottomLeftRadius::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(stream, context);
}

const CSSValue* BorderBottomLeftRadius::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForBorderRadiusCorner(
      style.BorderBottomLeftRadius(), style);
}

const CSSValue* BorderBottomRightRadius::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(stream, context);
}

const CSSValue* BorderBottomRightRadius::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForBorderRadiusCorner(
      style.BorderBottomRightRadius(), style);
}

const CSSValue* BorderBottomStyle::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderStyleSide(stream, context);
}

const CSSValue* BorderBottomStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BorderBottomStyle());
}

const CSSValue* BorderBottomWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderWidthSide(stream, context,
                                                 local_context);
}

const CSSValue* BorderBottomWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.BorderBottomWidth(), style);
}

const CSSValue* BorderCollapse::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.BorderCollapse() == EBorderCollapse::kCollapse) {
    return CSSIdentifierValue::Create(CSSValueID::kCollapse);
  }
  return CSSIdentifierValue::Create(CSSValueID::kSeparate);
}

const CSSValue* BorderEndEndRadius::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(stream, context);
}

const CSSValue* BorderEndStartRadius::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(stream, context);
}

const CSSValue* BorderImageOutset::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageOutset(stream, context);
}

const CSSValue* BorderImageOutset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageRepeat(stream);
}

const CSSValue* BorderImageRepeat::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForNinePieceImageRepeat(style.BorderImage());
}

const CSSValue* BorderImageRepeat::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kStretch);
}

const CSSValue* BorderImageSlice::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageSlice(
      stream, context, css_parsing_utils::DefaultFill::kNoFill);
}

const CSSValue* BorderImageSlice::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeImageOrNone(stream, context);
}

const CSSValue* BorderImageSource::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.BorderImageSource()) {
    return style.BorderImageSource()->ComputedCSSValue(
        style, allow_visited_style, value_phase);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* BorderImageSource::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

void BorderImageSource::ApplyValue(StyleResolverState& state,
                                   const CSSValue& value,
                                   ValueMode) const {
  state.StyleBuilder().SetBorderImageSource(
      state.GetStyleImage(CSSPropertyID::kBorderImageSource, value));
}

const CSSValue* BorderImageWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageWidth(stream, context);
}

const CSSValue* BorderImageWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const CSSValue* BorderInlineEndWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderWidth(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* BorderInlineStartColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const CSSValue* BorderInlineStartWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderWidth(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* BorderLeftColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const blink::Color BorderLeftColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& border_left_color = style.BorderLeftColor();
  if (style.ShouldForceColor(border_left_color)) {
    return GetCSSPropertyInternalForcedBorderColor().ColorIncludingFallback(
        false, style, is_current_color);
  }
  return ComputedStyleUtils::BorderSideColor(style, border_left_color,
                                             style.BorderLeftStyle(),
                                             visited_link, is_current_color);
}

const CSSValue* BorderLeftColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const StyleColor& border_left_color = style.BorderLeftColor();
  if (value_phase == CSSValuePhase::kResolvedValue &&
      style.ShouldForceColor(border_left_color)) {
    return GetCSSPropertyInternalForcedBorderColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style, value_phase);
  }
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, border_left_color, value_phase);
}

const CSSValue* BorderLeftStyle::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderStyleSide(stream, context);
}

const CSSValue* BorderLeftStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BorderLeftStyle());
}

const CSSValue* BorderLeftWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderWidthSide(stream, context,
                                                 local_context);
}

const CSSValue* BorderLeftWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.BorderLeftWidth(), style);
}

const CSSValue* BorderRightColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const blink::Color BorderRightColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& border_right_color = style.BorderRightColor();
  if (style.ShouldForceColor(border_right_color)) {
    return GetCSSPropertyInternalForcedBorderColor().ColorIncludingFallback(
        false, style, is_current_color);
  }
  return ComputedStyleUtils::BorderSideColor(style, border_right_color,
                                             style.BorderRightStyle(), false,
                                             is_current_color);
}

const CSSValue* BorderRightColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const StyleColor& border_right_color = style.BorderRightColor();
  if (value_phase == CSSValuePhase::kResolvedValue &&
      style.ShouldForceColor(border_right_color)) {
    return GetCSSPropertyInternalForcedBorderColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style, value_phase);
  }
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, border_right_color, value_phase);
}

const CSSValue* BorderRightStyle::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderStyleSide(stream, context);
}

const CSSValue* BorderRightStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BorderRightStyle());
}

const CSSValue* BorderRightWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderWidthSide(stream, context,
                                                 local_context);
}

const CSSValue* BorderRightWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.BorderRightWidth(), style);
}

const CSSValue* BorderStartStartRadius::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(stream, context);
}

const CSSValue* BorderStartEndRadius::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(stream, context);
}

const CSSValue* BorderTopColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const blink::Color BorderTopColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& border_top_color = style.BorderTopColor();
  if (style.ShouldForceColor(border_top_color)) {
    return GetCSSPropertyInternalForcedBorderColor().ColorIncludingFallback(
        false, style, is_current_color);
  }
  return ComputedStyleUtils::BorderSideColor(style, border_top_color,
                                             style.BorderTopStyle(),
                                             visited_link, is_current_color);
}

const CSSValue* BorderTopColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const StyleColor& border_top_color = style.BorderTopColor();
  if (value_phase == CSSValuePhase::kResolvedValue &&
      style.ShouldForceColor(border_top_color)) {
    return GetCSSPropertyInternalForcedBorderColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style, value_phase);
  }
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::ComputedStyleUtils::CurrentColorOrValidColor(
                   style, border_top_color, value_phase);
}

const CSSValue* BorderTopLeftRadius::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(stream, context);
}

const CSSValue* BorderTopLeftRadius::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForBorderRadiusCorner(
      style.BorderTopLeftRadius(), style);
}

const CSSValue* BorderTopRightRadius::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseBorderRadiusCorner(stream, context);
}

const CSSValue* BorderTopRightRadius::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForBorderRadiusCorner(
      style.BorderTopRightRadius(), style);
}

const CSSValue* BorderTopStyle::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderStyleSide(stream, context);
}

const CSSValue* BorderTopStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BorderTopStyle());
}

const CSSValue* BorderTopWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseBorderWidthSide(stream, context,
                                                 local_context);
}

const CSSValue* BorderTopWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.BorderTopWidth(), style);
}

const CSSValue* Bottom::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context,
      css_parsing_utils::UnitlessUnlessShorthand(local_context),
      kCSSAnchorQueryTypesAll);
}

bool Bottom::IsLayoutDependent(const ComputedStyle* style,
                               LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Bottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForPositionOffset(style, *this,
                                                    layout_object);
}

const CSSValue* BoxDecorationBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.BoxDecorationBreak() == EBoxDecorationBreak::kSlice) {
    return CSSIdentifierValue::Create(CSSValueID::kSlice);
  }
  return CSSIdentifierValue::Create(CSSValueID::kClone);
}

const CSSValue* BoxShadow::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeShadow(
      stream, context, css_parsing_utils::AllowInsetAndSpread::kAllow);
}

const CSSValue* BoxShadow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForShadowList(style.BoxShadow(), style, true,
                                                value_phase);
}

const CSSValue* BoxSizing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.BoxSizing() == EBoxSizing::kContentBox) {
    return CSSIdentifierValue::Create(CSSValueID::kContentBox);
  }
  return CSSIdentifierValue::Create(CSSValueID::kBorderBox);
}

const CSSValue* BreakAfter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BreakAfter());
}

const CSSValue* BreakBefore::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BreakBefore());
}

const CSSValue* BreakInside::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BreakInside());
}

const CSSValue* BufferedRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BufferedRendering());
}

const CSSValue* CaptionSide::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.CaptionSide());
}

const CSSValue* CaretAnimation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.CaretAnimation());
}

const CSSValue* CaretColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color CaretColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleAutoColor& auto_color = style.CaretColor();
  // TODO(rego): We may want to adjust the caret color if it's the same as
  // the background to ensure good visibility and contrast.
  const StyleColor result = auto_color.IsAutoColor()
                                ? StyleColor::CurrentColor()
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (allow_visited_style) {
    return cssvalue::CSSColor::Create(style.VisitedDependentColor(*this));
  }

  const StyleAutoColor& auto_color = style.CaretColor();
  // TODO(rego): We may want to adjust the caret color if it's the same as
  // the background to ensure good visibility and contrast.
  const StyleColor result = auto_color.IsAutoColor()
                                ? StyleColor::CurrentColor()
                                : auto_color.ToStyleColor();
  if (value_phase == CSSValuePhase::kResolvedValue &&
      style.ShouldForceColor(result)) {
    return cssvalue::CSSColor::Create(style.GetInternalForcedCurrentColor());
  }

  return ComputedStyleUtils::ValueForStyleAutoColor(style, style.CaretColor(),
                                                    value_phase);
}

const CSSValue* Clear::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.Clear());
}

namespace {

CSSValue* ConsumeClipComponent(CSSParserTokenStream& stream,
                               const CSSParserContext& context) {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeLength(
      stream, context, CSSPrimitiveValue::ValueRange::kAll,
      css_parsing_utils::UnitlessQuirk::kAllow);
}

}  // namespace

const CSSValue* Clip::ParseSingleValue(CSSParserTokenStream& stream,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  if (stream.Peek().FunctionId() != CSSValueID::kRect) {
    return nullptr;
  }

  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();
  // rect(t, r, b, l) || rect(t r b l)
  CSSValue* top = ConsumeClipComponent(stream, context);
  if (!top) {
    return nullptr;
  }
  bool needs_comma = css_parsing_utils::ConsumeCommaIncludingWhitespace(stream);
  CSSValue* right = ConsumeClipComponent(stream, context);
  if (!right || (needs_comma &&
                 !css_parsing_utils::ConsumeCommaIncludingWhitespace(stream))) {
    return nullptr;
  }
  CSSValue* bottom = ConsumeClipComponent(stream, context);
  if (!bottom ||
      (needs_comma &&
       !css_parsing_utils::ConsumeCommaIncludingWhitespace(stream))) {
    return nullptr;
  }
  CSSValue* left = ConsumeClipComponent(stream, context);
  if (!left || !stream.AtEnd()) {
    // NOTE: This AtEnd() is fine, because we test within the
    // RestoringBlockGuard. But we need the stream to rewind in that case.
    return nullptr;
  }
  guard.Release();
  return MakeGarbageCollected<CSSQuadValue>(top, right, bottom, left,
                                            CSSQuadValue::kSerializeAsRect);
}

const CSSValue* Clip::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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

const CSSValue* ClipPath::ParseSingleValue(CSSParserTokenStream& stream,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  if (cssvalue::CSSURIValue* url =
          css_parsing_utils::ConsumeUrl(stream, context)) {
    return url;
  }

  CSSValue* geometry_box = css_parsing_utils::ConsumeGeometryBox(stream);
  CSSValue* basic_shape = css_parsing_utils::ConsumeBasicShape(stream, context);
  if (basic_shape && !geometry_box) {
    geometry_box = css_parsing_utils::ConsumeGeometryBox(stream);
  }
  if (basic_shape || geometry_box) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    if (basic_shape) {
      list->Append(*basic_shape);
    }
    if (geometry_box) {
      if (list->length() == 0 ||
          To<CSSIdentifierValue>(geometry_box)->GetValueID() !=
              CSSValueID::kBorderBox) {
        list->Append(*geometry_box);
      }
    }
    return list;
  }

  return nullptr;
}

const CSSValue* ClipPath::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (ClipPathOperation* operation = style.ClipPath()) {
    if (auto* box = DynamicTo<GeometryBoxClipPathOperation>(operation)) {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      GeometryBox geometry_box = box->GetGeometryBox();
      list->Append(*CSSIdentifierValue::Create(geometry_box));
      return list;
    }
    if (auto* shape = DynamicTo<ShapeClipPathOperation>(operation)) {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      auto* basic_shape = ValueForBasicShape(style, shape->GetBasicShape());
      list->Append(*basic_shape);
      GeometryBox geometry_box = shape->GetGeometryBox();
      if (geometry_box != GeometryBox::kBorderBox) {
        list->Append(*CSSIdentifierValue::Create(geometry_box));
      }
      return list;
    }
    if (operation->GetType() == ClipPathOperation::kReference) {
      AtomicString url = To<ReferenceClipPathOperation>(operation)->Url();
      return MakeGarbageCollected<cssvalue::CSSURIValue>(CSSUrlData(url));
    }
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* ClipRule::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ClipRule());
}

const CSSValue* Color::ParseSingleValue(CSSParserTokenStream& stream,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColorMaybeQuirky(stream, context);
}

const blink::Color Color::ColorIncludingFallback(bool visited_link,
                                                 const ComputedStyle& style,
                                                 bool* is_current_color) const {
  DCHECK(!visited_link);
  if (style.ShouldForceColor(style.Color())) {
    return GetCSSPropertyInternalForcedColor().ColorIncludingFallback(
        false, style, is_current_color);
  }
  return style.GetCurrentColor(is_current_color);
}

const CSSValue* Color::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (value_phase == CSSValuePhase::kResolvedValue &&
      style.ShouldForceColor(style.Color())) {
    return GetCSSPropertyInternalForcedColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style, value_phase);
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

void Color::ApplyValue(StyleResolverState& state,
                       const CSSValue& value,
                       ValueMode) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (value.IsInitialColorValue()) {
    DCHECK_EQ(state.GetElement(), state.GetDocument().documentElement());
    builder.SetColor(builder.InitialColorForColorScheme());
  } else {
    StyleColor color = StyleBuilderConverter::ConvertStyleColor(state, value);
    if (color.IsUnresolvedColorFunction()) {
      // Unresolved color functions are a special case for this property.
      // currentColor used in the color property value refers to the parent's
      // computed currentColor which means we can fully resolve currentColor at
      // ApplyValue time to get the correct resolved and used values for the
      // color property in all cases.
      // For typed OM, currentColor and color functions containing
      // currentColor should have been preserved for values in
      // computedStyleMap().
      // See crbug.com/1099874
      color = StyleColor(color.GetUnresolvedColorFunction().Resolve(
          state.ParentStyle()->Color().GetColor()));
    } else if (color.IsCurrentColor()) {
      // As per the spec, 'color: currentColor' is treated as 'color: inherit'
      ApplyInherit(state);
      builder.SetColorIsCurrentColor(true);
      if (state.UsesHighlightPseudoInheritance() &&
          state.OriginatingElementStyle()) {
        builder.SetColor(state.OriginatingElementStyle()->Color());
      }
      return;
    }
    builder.SetColor(color);
  }
  builder.SetColorIsInherited(false);
  builder.SetColorIsCurrentColor(false);
}

const CSSValue* ColorInterpolation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ColorInterpolation());
}

const CSSValue* ColorInterpolationFilters::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ColorInterpolationFilters());
}

const CSSValue* ColorRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ColorRendering());
}

const CSSValue* ColorScheme::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSValue* only = nullptr;
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  do {
    CSSValueID id = stream.Peek().Id();
    // 'normal' is handled above, and needs to be excluded from
    // ConsumeCustomIdent below.
    if (id == CSSValueID::kNormal) {
      return nullptr;
    }
    CSSValue* value =
        css_parsing_utils::ConsumeIdent<CSSValueID::kDark, CSSValueID::kLight,
                                        CSSValueID::kOnly>(stream);
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
      value = css_parsing_utils::ConsumeCustomIdent(stream, context);
    }
    if (!value) {
      break;
    }
    values->Append(*value);
  } while (!stream.AtEnd());
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
                             const CSSValue& value,
                             ValueMode) const {
  const CSSValueList* scheme_list = DynamicTo<CSSValueList>(value);
  DCHECK(scheme_list || (value.IsIdentifierValue() &&
                         DynamicTo<CSSIdentifierValue>(value)->GetValueID() ==
                             CSSValueID::kNormal));
  ApplyColorSchemeValue(state, scheme_list);
}

const CSSValue* ColumnCount::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColumnCount(stream, context);
}

const CSSValue* ColumnCount::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.HasAutoColumnCount()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return CSSNumericLiteralValue::Create(style.ColumnCount(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* ColumnFill::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetColumnFill());
}

const CSSValue* ColumnGap::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGapLength(stream, context);
}

const CSSValue* ColumnGap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGapLength(style.ColumnGap(), style);
}

const CSSValue* ColumnRuleColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGapDecorationColorList(stream, context);
}

const blink::Color ColumnRuleColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& column_rule_color = style.ColumnRuleColor();
  if (style.ShouldForceColor(column_rule_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return column_rule_color.Resolve(style.GetCurrentColor(),
                                   style.UsedColorScheme(), is_current_color);
}

const CSSValue* ColumnRuleColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, style.ColumnRuleColor(), value_phase);
}

const CSSValue* ColumnRuleStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ColumnRuleStyle());
}

const CSSValue* ColumnRuleWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLineWidth(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ColumnRuleWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.ColumnRuleWidth(), style);
}

const CSSValue* ColumnSpan::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIdent<CSSValueID::kAll, CSSValueID::kNone>(
      stream);
}

const CSSValue* ColumnSpan::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(static_cast<unsigned>(style.GetColumnSpan())
                                        ? CSSValueID::kAll
                                        : CSSValueID::kNone);
}

const CSSValue* ColumnWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColumnWidth(stream, context);
}

const CSSValue* ColumnWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.HasAutoColumnWidth()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return ZoomAdjustedPixelValue(style.ColumnWidth(), style);
}

// none | strict | content | [ size || layout || style || paint ]
const CSSValue* Contain::ParseSingleValue(CSSParserTokenStream& stream,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (id == CSSValueID::kStrict || id == CSSValueID::kContent) {
    list->Append(*css_parsing_utils::ConsumeIdent(stream));
    return list;
  }

  CSSIdentifierValue* size = nullptr;
  CSSIdentifierValue* layout = nullptr;
  CSSIdentifierValue* style = nullptr;
  CSSIdentifierValue* paint = nullptr;
  while (true) {
    id = stream.Peek().Id();
    if ((id == CSSValueID::kSize ||

         id == CSSValueID::kInlineSize) &&
        !size) {
      size = css_parsing_utils::ConsumeIdent(stream);
    } else if (id == CSSValueID::kLayout && !layout) {
      layout = css_parsing_utils::ConsumeIdent(stream);
    } else if (id == CSSValueID::kStyle && !style) {
      style = css_parsing_utils::ConsumeIdent(stream);
    } else if (id == CSSValueID::kPaint && !paint) {
      paint = css_parsing_utils::ConsumeIdent(stream);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIntrinsicSizeLonghand(stream, context);
}

const CSSValue* ContainIntrinsicWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForIntrinsicLength(
      style, style.ContainIntrinsicWidth());
}

const CSSValue* ContainIntrinsicHeight::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIntrinsicSizeLonghand(stream, context);
}

const CSSValue* ContainIntrinsicHeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForIntrinsicLength(
      style, style.ContainIntrinsicHeight());
}

const CSSValue* ContainIntrinsicInlineSize::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIntrinsicSizeLonghand(stream, context);
}

const CSSValue* ContainIntrinsicBlockSize::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIntrinsicSizeLonghand(stream, context);
}

const CSSValue* ContainerName::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeContainerName(stream, context);
}

const CSSValue* ContainerName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeContainerType(stream);
}

const CSSValue* ContainerType::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  DCHECK_NE(style.ContainerType() & kContainerTypeSize,
            kContainerTypeBlockSize);

  if (style.ContainerType() == kContainerTypeNormal) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  if ((style.ContainerType() & kContainerTypeBlockSize) ==
      kContainerTypeBlockSize) {
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kSize));
  } else if (style.ContainerType() & kContainerTypeInlineSize) {
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kInlineSize));
  }
  if (style.ContainerType() & kContainerTypeScrollState) {
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kScrollState));
  }
  return values;
}

namespace {

CSSValue* ConsumeAttr(CSSParserTokenStream& stream,
                      const CSSParserContext& context) {
  DCHECK(!RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled());

  AtomicString attr_name;
  {
    CSSParserTokenStream::BlockGuard guard(stream);
    stream.ConsumeWhitespace();
    if (stream.Peek().GetType() != kIdentToken) {
      return nullptr;
    }

    attr_name = stream.ConsumeIncludingWhitespace().Value().ToAtomicString();
    if (!stream.AtEnd()) {
      // NOTE: This AtEnd() is fine, because we are inside a function block
      // (i.e., inside a BlockGuard).
      return nullptr;
    }
  }

  stream.ConsumeWhitespace();
  if (context.IsHTMLDocument()) {
    attr_name = attr_name.LowerASCII();
  }

  CSSFunctionValue* attr_value =
      MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kAttr);
  attr_value->Append(*MakeGarbageCollected<CSSCustomIdentValue>(attr_name));
  return attr_value;
}

CSSValue* ConsumeCounterContent(CSSParserTokenStream& stream,
                                const CSSParserContext& context,
                                bool counters) {
  CSSCustomIdentValue* identifier;
  CSSCustomIdentValue* list_style = nullptr;
  CSSStringValue* separator = nullptr;

  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    identifier = css_parsing_utils::ConsumeCustomIdent(stream, context);
    if (!identifier) {
      return nullptr;
    }

    if (!counters) {
      separator = MakeGarbageCollected<CSSStringValue>(String());
    } else {
      if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(stream) ||
          stream.Peek().GetType() != kStringToken) {
        return nullptr;
      }
      separator = MakeGarbageCollected<CSSStringValue>(
          stream.ConsumeIncludingWhitespace().Value().ToString());
    }

    if (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
      // Note: CSS3 spec doesn't allow 'none' but CSS2.1 allows it. We currently
      // allow it for backward compatibility.
      // See https://github.com/w3c/csswg-drafts/issues/5795 for details.
      if (stream.Peek().Id() == CSSValueID::kNone) {
        list_style =
            MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("none"));
        stream.ConsumeIncludingWhitespace();
      } else {
        list_style =
            css_parsing_utils::ConsumeCounterStyleName(stream, context);
      }
    } else {
      list_style =
          MakeGarbageCollected<CSSCustomIdentValue>(keywords::kDecimal);
    }

    if (!list_style || !stream.AtEnd()) {
      // NOTE: This AtEnd() is fine, because we are inside a function block
      // (i.e., inside a RestoringBlockGuard).
      return nullptr;
    }
    guard.Release();
  }
  stream.ConsumeWhitespace();
  return MakeGarbageCollected<cssvalue::CSSCounterValue>(identifier, list_style,
                                                         separator);
}

const CSSValue* ParseContentValue(CSSParserTokenStream& stream,
                                  const CSSParserContext& context) {
  if (css_parsing_utils::IdentMatches<CSSValueID::kNone, CSSValueID::kNormal>(
          stream.Peek().Id())) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  CSSValueList* outer_list = CSSValueList::CreateSlashSeparated();
  bool alt_text_present = false;
  do {
    CSSParserSavePoint savepoint(stream);
    CSSValue* parsed_value = css_parsing_utils::ConsumeImage(stream, context);
    if (!parsed_value) {
      parsed_value = css_parsing_utils::ConsumeIdent<
          CSSValueID::kOpenQuote, CSSValueID::kCloseQuote,
          CSSValueID::kNoOpenQuote, CSSValueID::kNoCloseQuote>(stream);
    }
    if (!parsed_value) {
      parsed_value = css_parsing_utils::ConsumeString(stream);
    }
    if (!parsed_value) {
      if (stream.Peek().FunctionId() == CSSValueID::kAttr &&
          !RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled()) {
        parsed_value = ConsumeAttr(stream, context);
      } else if (stream.Peek().FunctionId() == CSSValueID::kCounter) {
        parsed_value = ConsumeCounterContent(stream, context, false);
      } else if (stream.Peek().FunctionId() == CSSValueID::kCounters) {
        parsed_value = ConsumeCounterContent(stream, context, true);
      }
    }
    if (!parsed_value) {
      if (css_parsing_utils::ConsumeSlashIncludingWhitespace(stream)) {
        // No values were parsed before the slash, so nothing to apply the
        // alternative text to.
        if (!values->length()) {
          return nullptr;
        }
        alt_text_present = true;
      } else {
        break;
      }
    } else {
      values->Append(*parsed_value);
    }
    savepoint.Release();
  } while (!stream.AtEnd() && !alt_text_present);
  if (!values->length()) {
    return nullptr;
  }
  outer_list->Append(*values);
  if (alt_text_present) {
    CSSValueList* alt_text_values = CSSValueList::CreateSpaceSeparated();
    do {
      CSSParserSavePoint savepoint(stream);
      CSSValue* alt_text = nullptr;
      if (stream.Peek().FunctionId() == CSSValueID::kAttr &&
          !RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled()) {
        alt_text = ConsumeAttr(stream, context);
      } else {
        alt_text = css_parsing_utils::ConsumeString(stream);
      }
      if (!alt_text) {
        break;
      }
      alt_text_values->Append(*alt_text);
      savepoint.Release();
    } while (!stream.AtEnd());
    if (!alt_text_values->length()) {
      return nullptr;
    }

    outer_list->Append(*alt_text_values);
  }
  return outer_list;
}

}  // namespace

const CSSValue* Content::ParseSingleValue(CSSParserTokenStream& stream,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  return ConsumeAppearanceAutoBaseSelect(ParseContentValue, stream, context);
}

const CSSValue* Content::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForContentData(style, allow_visited_style,
                                                 value_phase);
}

void Content::ApplyInitial(StyleResolverState& state) const {
  state.StyleBuilder().SetContent(nullptr);
}

void Content::ApplyInherit(StyleResolverState& state) const {
  // FIXME: In CSS3, it will be possible to inherit content. In CSS2 it is
  // not. This note is a reminder that eventually "inherit" needs to be
  // supported.
}

namespace {

String GetStringFromAttributeOrStringValue(const CSSValue& value,
                                           StyleResolverState& state,
                                           ComputedStyleBuilder& builder) {
  String string = g_empty_string;
  if (const auto* function_value = DynamicTo<CSSFunctionValue>(value)) {
    DCHECK(!RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled());
    DCHECK_EQ(function_value->FunctionType(), CSSValueID::kAttr);
    builder.SetHasAttrFunction();
    // TODO: Can a namespace be specified for an attr(foo)?
    QualifiedName attr(
        To<CSSCustomIdentValue>(function_value->Item(0)).Value());
    const AtomicString& attr_value = state.GetElement().getAttribute(attr);
    string = attr_value.IsNull() ? g_empty_string : attr_value.GetString();
  } else {
    // We should be able to assume at this point that `value` is a
    // CSSStringValue, since all other types of CSSValues produced in
    // Content::ParseSingleValue should have been handled by Content::ApplyValue
    // before reaching this point. However, as observed in crbug.com/348304397
    // there is some unexpected type that is not getting handled. The following
    // two DCHECKs are intended to help investigate this. The first DCHECK tests
    // the theory that the unexpected type is coming from ConsumeImage, where a
    // light-dark() function in a UA shadow DOM could cause a
    // CSSLightDarkValuePair to be created. The second DCHECK will hit if this
    // first theory is wrong and `value` has some other unexpected type.
    DCHECK(!IsA<CSSLightDarkValuePair>(value));
    DCHECK(IsA<CSSStringValue>(value));
    if (const auto* string_value = DynamicTo<CSSStringValue>(value)) {
      string = string_value->Value();
    }
  }
  return string;
}

}  // namespace

void Content::ApplyValue(StyleResolverState& state,
                         const CSSValue& value,
                         ValueMode) const {
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
          NOTREACHED_IN_MIGRATION();
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
      String string =
          GetStringFromAttributeOrStringValue(*item, state, builder);
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
    CHECK_EQ(outer_list.length(), 2U);
    for (auto& item : To<CSSValueList>(outer_list.Item(1))) {
      auto* alt_content = MakeGarbageCollected<AltTextContentData>(
          GetStringFromAttributeOrStringValue(*item, state, builder));
      prev_content->SetNext(alt_content);
      prev_content = alt_content;
    }
  }
  DCHECK(first_content);
  builder.SetContent(first_content);
}

const int kCounterIncrementDefaultValue = 1;

const CSSValue* CounterIncrement::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCounter(stream, context,
                                           kCounterIncrementDefaultValue);
}

const CSSValue* CounterIncrement::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForCounterDirectives(
      style, CountersAttachmentContext::Type::kIncrementType);
}

const int kCounterResetDefaultValue = 0;

const CSSValue* CounterReset::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCounter(stream, context,
                                           kCounterResetDefaultValue);
}

const CSSValue* CounterReset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForCounterDirectives(
      style, CountersAttachmentContext::Type::kResetType);
}

const int kCounterSetDefaultValue = 0;

const CSSValue* CounterSet::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCounter(stream, context,
                                           kCounterSetDefaultValue);
}

const CSSValue* CounterSet::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForCounterDirectives(
      style, CountersAttachmentContext::Type::kSetType);
}

const CSSValue* Cursor::ParseSingleValue(CSSParserTokenStream& stream,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  bool in_quirks_mode = IsQuirksModeBehavior(context.Mode());
  CSSValueList* list = nullptr;
  while (CSSValue* image = css_parsing_utils::ConsumeImage(
             stream, context,
             css_parsing_utils::ConsumeGeneratedImagePolicy::kForbid)) {
    double num;
    gfx::Point hot_spot(-1, -1);
    bool hot_spot_specified = false;
    if (css_parsing_utils::ConsumeNumberRaw(stream, context, num)) {
      hot_spot.set_x(ClampTo<int>(num));
      if (!css_parsing_utils::ConsumeNumberRaw(stream, context, num)) {
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
    if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
      return nullptr;
    }
  }

  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kWebkitZoomIn) {
    context.Count(WebFeature::kPrefixedCursorZoomIn);
  } else if (id == CSSValueID::kWebkitZoomOut) {
    context.Count(WebFeature::kPrefixedCursorZoomOut);
  } else if (id == CSSValueID::kWebkitGrab) {
    context.Count(WebFeature::kPrefixedCursorGrab);
  } else if (id == CSSValueID::kWebkitGrabbing) {
    context.Count(WebFeature::kPrefixedCursorGrabbing);
  }
  CSSIdentifierValue* cursor_type = nullptr;
  if (id == CSSValueID::kHand) {
    if (!in_quirks_mode) {  // Non-standard behavior
      return nullptr;
    }
    cursor_type = MakeGarbageCollected<CSSIdentifierValue>(
        CSSValueID::kPointer,
        /*was_quirky=*/true);  // Cannot use the identifier value pool due to
                               // was_quirky.
    stream.ConsumeIncludingWhitespace();
  } else if ((id >= CSSValueID::kAuto && id <= CSSValueID::kWebkitZoomOut) ||
             id == CSSValueID::kCopy || id == CSSValueID::kNone) {
    cursor_type = css_parsing_utils::ConsumeIdent(stream);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  CSSValueList* list = nullptr;
  CursorList* cursors = style.Cursors();
  if (cursors && cursors->size() > 0) {
    list = CSSValueList::CreateCommaSeparated();
    for (const CursorData& cursor : *cursors) {
      if (StyleImage* image = cursor.GetImage()) {
        list->Append(*MakeGarbageCollected<cssvalue::CSSCursorImageValue>(
            *image->ComputedCSSValue(style, allow_visited_style, value_phase),
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
                        const CSSValue& value,
                        ValueMode) const {
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

const CSSValue* Cx::ParseSingleValue(CSSParserTokenStream& stream,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* Cx::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Cx(), style);
}

const CSSValue* Cy::ParseSingleValue(CSSParserTokenStream& stream,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* Cy::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Cy(), style);
}

const CSSValue* D::ParseSingleValue(CSSParserTokenStream& stream,
                                    const CSSParserContext&,
                                    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePathOrNone(stream);
}

const CSSValue* D::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (const StylePath* style_path = style.D()) {
    return style_path->ComputedCSSValue();
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* Direction::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.Direction());
}

void Direction::ApplyValue(StyleResolverState& state,
                           const CSSValue& value,
                           ValueMode) const {
  state.StyleBuilder().SetDirection(
      To<CSSIdentifierValue>(value).ConvertTo<TextDirection>());
}

namespace {

static bool IsDisplayOutside(CSSValueID id) {
  return id >= CSSValueID::kInline && id <= CSSValueID::kBlock;
}

static bool IsDisplayInside(CSSValueID id) {
  if (id == CSSValueID::kMasonry) {
    return RuntimeEnabledFeatures::CSSMasonryLayoutEnabled();
  }
  return (id >= CSSValueID::kFlowRoot && id <= CSSValueID::kMasonry) ||
         id == CSSValueID::kMath || id == CSSValueID::kRuby;
}

static bool IsDisplayBox(CSSValueID id) {
  return css_parsing_utils::IdentMatches<CSSValueID::kNone,
                                         CSSValueID::kContents>(id);
}

static bool IsDisplayInternal(CSSValueID id) {
  return id >= CSSValueID::kTableRowGroup && id <= CSSValueID::kRubyText;
}

static bool IsDisplayLegacy(CSSValueID id) {
  if (id == CSSValueID::kInlineMasonry) {
    return RuntimeEnabledFeatures::CSSMasonryLayoutEnabled();
  }
  return id >= CSSValueID::kInlineBlock && id <= CSSValueID::kWebkitInlineFlex;
}

bool IsDisplayListItem(CSSValueID id) {
  return id == CSSValueID::kListItem;
}

struct DisplayValidationResult {
  STACK_ALLOCATED();

 public:
  const CSSIdentifierValue* outside;
  const CSSIdentifierValue* inside;
  const CSSIdentifierValue* list_item;
};

// Find <display-outside>, <display-inside>, and `list-item` in the unordered
// keyword list `values`.  Returns nullopt if `values` contains an invalid
// combination of keywords.
std::optional<DisplayValidationResult> ValidateDisplayKeywords(
    const CSSValueList& values) {
  const CSSIdentifierValue* outside = nullptr;
  const CSSIdentifierValue* inside = nullptr;
  const CSSIdentifierValue* list_item = nullptr;
  for (const auto& item : values) {
    const CSSIdentifierValue* value = To<CSSIdentifierValue>(item.Get());
    CSSValueID value_id = value->GetValueID();
    if (!outside && IsDisplayOutside(value_id)) {
      outside = value;
    } else if (!inside && IsDisplayInside(value_id)) {
      inside = value;
    } else if (!list_item && IsDisplayListItem(value_id)) {
      list_item = value;
    } else {
      return std::nullopt;
    }
  }
  DisplayValidationResult result{outside, inside, list_item};
  return result;
}

// Drop redundant keywords, and update to backward-compatible keywords.
// e.g. {outside:"block", inside:"flow"} ==> {outside:"block", inside:null}
//      {outside:"inline", inside:"flow-root"} ==>
//          {outside:null, inside:"inline-block"}
void AdjustDisplayKeywords(DisplayValidationResult& result) {
  CSSValueID outside =
      result.outside ? result.outside->GetValueID() : CSSValueID::kInvalid;
  CSSValueID inside =
      result.inside ? result.inside->GetValueID() : CSSValueID::kInvalid;
  switch (inside) {
    case CSSValueID::kFlow:
      if (result.outside) {
        result.inside = nullptr;
      }
      break;
    case CSSValueID::kFlex:
    case CSSValueID::kFlowRoot:
    case CSSValueID::kGrid:
    case CSSValueID::kTable:
      if (outside == CSSValueID::kBlock) {
        result.outside = nullptr;
      } else if (RuntimeEnabledFeatures::CssDisplaySerialziationFixEnabled() &&
                 outside == CSSValueID::kInline && !result.list_item) {
        CSSValueID new_id = CSSValueID::kInvalid;
        if (inside == CSSValueID::kFlex) {
          new_id = CSSValueID::kInlineFlex;
        } else if (inside == CSSValueID::kFlowRoot) {
          new_id = CSSValueID::kInlineBlock;
        } else if (inside == CSSValueID::kGrid) {
          new_id = CSSValueID::kInlineGrid;
        } else if (inside == CSSValueID::kTable) {
          new_id = CSSValueID::kInlineTable;
        }
        CHECK_NE(new_id, CSSValueID::kInvalid);
        result.outside = nullptr;
        result.inside = CSSIdentifierValue::Create(new_id);
      }
      break;
    case CSSValueID::kMath:
    case CSSValueID::kRuby:
      if (outside == CSSValueID::kInline) {
        result.outside = nullptr;
      }
      break;
    default:
      break;
  }

  if (result.list_item) {
    if (outside == CSSValueID::kBlock) {
      result.outside = nullptr;
    }
    if (inside == CSSValueID::kFlow) {
      result.inside = nullptr;
    }
  }
}

const CSSValue* ParseDisplayMultipleKeywords(
    CSSParserTokenStream& stream,
    const CSSIdentifierValue* first_value) {
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  values->Append(*first_value);
  values->Append(*css_parsing_utils::ConsumeIdent(stream));
  if (stream.Peek().Id() != CSSValueID::kInvalid) {
    values->Append(*css_parsing_utils::ConsumeIdent(stream));
  }
  // `values` now has two or three CSSIdentifierValue pointers.

  auto result = ValidateDisplayKeywords(*values);
  if (!result) {
    return nullptr;
  }

  if (result->list_item && result->inside) {
    CSSValueID inside = result->inside->GetValueID();
    if (inside != CSSValueID::kFlow && inside != CSSValueID::kFlowRoot) {
      return nullptr;
    }
  }

  AdjustDisplayKeywords(*result);
  CSSValueList* result_list = CSSValueList::CreateSpaceSeparated();
  if (result->outside) {
    result_list->Append(*result->outside);
  }
  if (result->inside) {
    result_list->Append(*result->inside);
  }
  if (result->list_item) {
    result_list->Append(*result->list_item);
  }
  return result_list->length() == 1u ? &result_list->Item(0) : result_list;
}

}  // namespace

// https://drafts.csswg.org/css-display/#the-display-properties
//   [<display-outside> || <display-inside>] |
//   [<display-outside>? && [ flow | flow-root ]? && list-item] |
//   <display-internal> | <display-box> | <display-legacy>
const CSSValue* Display::ParseSingleValue(CSSParserTokenStream& stream,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSValueID id = stream.Peek().Id();
  if (id != CSSValueID::kInvalid) {
    const CSSIdentifierValue* value = css_parsing_utils::ConsumeIdent(stream);
    if (stream.Peek().Id() != CSSValueID::kInvalid) {
      context.Count(WebFeature::kCssDisplayPropertyMultipleValues);
      return ParseDisplayMultipleKeywords(stream, value);
    }

    // The property has only one keyword (or one keyword and then junk,
    // in which case the caller will abort for us).
    if (RuntimeEnabledFeatures::CssDisplaySerialziationFixEnabled() &&
        id == CSSValueID::kFlow) {
      return CSSIdentifierValue::Create(CSSValueID::kBlock);
    } else if (id == CSSValueID::kListItem || IsDisplayBox(id) ||
               IsDisplayInternal(id) || IsDisplayLegacy(id) ||
               IsDisplayInside(id) || IsDisplayOutside(id)) {
      return value;
    } else {
      return nullptr;
    }
  }

  if (!RuntimeEnabledFeatures::CSSLayoutAPIEnabled()) {
    return nullptr;
  }

  if (!context.IsSecureContext()) {
    return nullptr;
  }

  CSSValueID function = stream.Peek().FunctionId();
  if (function != CSSValueID::kLayout &&
      function != CSSValueID::kInlineLayout) {
    return nullptr;
  }

  CSSCustomIdentValue* name;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    name = css_parsing_utils::ConsumeCustomIdent(stream, context);

    // If we didn't get a custom-ident or didn't exhaust the function arguments
    // return nothing.
    // NOTE: This AtEnd() is fine, because we are inside a RestoringBlockGuard
    // (i.e., we are testing the end of the argument list).
    if (!name || !stream.AtEnd()) {
      return nullptr;
    }

    guard.Release();
  }
  stream.ConsumeWhitespace();
  return MakeGarbageCollected<cssvalue::CSSLayoutFunctionValue>(
      name, /* is_inline */ function == CSSValueID::kInlineLayout);
}

const CSSValue* Display::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
  if (style.Display() == EDisplay::kBlockRuby) {
    CSSValueList* values = CSSValueList::CreateSpaceSeparated();
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kBlock));
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kRuby));
    return values;
  }
  if (style.Display() == EDisplay::kInlineListItem) {
    CSSValueList* values = CSSValueList::CreateSpaceSeparated();
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kInline));
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kListItem));
    return values;
  }
  if (style.Display() == EDisplay::kFlowRootListItem) {
    CSSValueList* values = CSSValueList::CreateSpaceSeparated();
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kFlowRoot));
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kListItem));
    return values;
  }
  if (style.Display() == EDisplay::kInlineFlowRootListItem) {
    CSSValueList* values = CSSValueList::CreateSpaceSeparated();
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kInline));
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kFlowRoot));
    values->Append(*CSSIdentifierValue::Create(CSSValueID::kListItem));
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
                         const CSSValue& value,
                         ValueMode) const {
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
    const CSSValueList& list = To<CSSValueList>(value);
    DCHECK(list.length() == 2u ||
           (list.length() == 3u && list.Item(2).IsIdentifierValue()));
    DCHECK(list.Item(0).IsIdentifierValue());
    DCHECK(list.Item(1).IsIdentifierValue());
    auto result = ValidateDisplayKeywords(list);
    DCHECK(result);
    CSSValueID outside =
        result->outside ? result->outside->GetValueID() : CSSValueID::kInvalid;
    CSSValueID inside =
        result->inside ? result->inside->GetValueID() : CSSValueID::kInvalid;

    if (result->list_item) {
      const bool is_block =
          outside == CSSValueID::kBlock || !IsValidCSSValueID(outside);
      if (inside != CSSValueID::kFlowRoot) {
        builder.SetDisplay(is_block ? EDisplay::kListItem
                                    : EDisplay::kInlineListItem);
      } else {
        builder.SetDisplay(is_block ? EDisplay::kFlowRootListItem
                                    : EDisplay::kInlineFlowRootListItem);
      }
      return;
    }

    DCHECK(IsDisplayOutside(outside));
    DCHECK(IsDisplayInside(inside));
    const bool is_block = outside == CSSValueID::kBlock;
    if (inside == CSSValueID::kFlowRoot) {
      builder.SetDisplay(is_block ? EDisplay::kFlowRoot
                                  : EDisplay::kInlineBlock);
    } else if (inside == CSSValueID::kFlow) {
      builder.SetDisplay(is_block ? EDisplay::kBlock : EDisplay::kInline);
    } else if (inside == CSSValueID::kTable) {
      builder.SetDisplay(is_block ? EDisplay::kTable : EDisplay::kInlineTable);
    } else if (inside == CSSValueID::kFlex) {
      builder.SetDisplay(is_block ? EDisplay::kFlex : EDisplay::kInlineFlex);
    } else if (inside == CSSValueID::kGrid) {
      builder.SetDisplay(is_block ? EDisplay::kGrid : EDisplay::kInlineGrid);
    } else if (inside == CSSValueID::kMath) {
      builder.SetDisplay(is_block ? EDisplay::kBlockMath : EDisplay::kMath);
    } else if (inside == CSSValueID::kRuby) {
      builder.SetDisplay(is_block ? EDisplay::kBlockRuby : EDisplay::kRuby);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.DominantBaseline());
}

const CSSValue* DynamicRangeLimit::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  if (const CSSValue* const keyword_value = css_parsing_utils::ConsumeIdent<
          CSSValueID::kStandard, CSSValueID::kHigh,
          CSSValueID::kConstrainedHigh>(stream)) {
    return keyword_value;
  }

  if (stream.Peek().FunctionId() != CSSValueID::kDynamicRangeLimitMix) {
    return nullptr;
  }

  const CSSValue* limit1;
  const CSSValue* limit2;
  const CSSPrimitiveValue* percentage;
  {
    CSSParserTokenStream::BlockGuard guard(stream);
    stream.ConsumeWhitespace();

    limit1 =
        DynamicRangeLimit::ParseSingleValue(stream, context, local_context);
    if (limit1 == nullptr ||
        !css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
      return nullptr;
    }
    limit2 =
        DynamicRangeLimit::ParseSingleValue(stream, context, local_context);
    if (limit2 == nullptr ||
        !css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
      return nullptr;
    }
    percentage = css_parsing_utils::ConsumePercent(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (percentage == nullptr) {
      return nullptr;
    }
  }
  stream.ConsumeWhitespace();

  return MakeGarbageCollected<cssvalue::CSSDynamicRangeLimitMixValue>(
      limit1, limit2, percentage);
}

const CSSValue* DynamicRangeLimit::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const auto& limit = style.GetDynamicRangeLimit();
  if (limit.standard_mix == 1.f) {
    return CSSIdentifierValue::Create(CSSValueID::kStandard);
  }
  if (limit.constrained_high_mix == 1.f) {
    return CSSIdentifierValue::Create(CSSValueID::kConstrainedHigh);
  }
  if (limit.standard_mix == 0.f && limit.constrained_high_mix == 0.f) {
    return CSSIdentifierValue::Create(CSSValueID::kHigh);
  }
  const float high_mix = 1 - limit.standard_mix - limit.constrained_high_mix;
  if (limit.standard_mix == 0.f) {
    return MakeGarbageCollected<cssvalue::CSSDynamicRangeLimitMixValue>(
        CSSIdentifierValue::Create(CSSValueID::kConstrainedHigh),
        CSSIdentifierValue::Create(CSSValueID::kHigh),
        CSSNumericLiteralValue::Create(
            100 * high_mix, CSSPrimitiveValue::UnitType::kPercentage));
  }
  if (limit.constrained_high_mix == 0.f) {
    return MakeGarbageCollected<cssvalue::CSSDynamicRangeLimitMixValue>(
        CSSIdentifierValue::Create(CSSValueID::kStandard),
        CSSIdentifierValue::Create(CSSValueID::kHigh),
        CSSNumericLiteralValue::Create(
            100 * high_mix, CSSPrimitiveValue::UnitType::kPercentage));
  }
  if (high_mix == 0.f) {
    return MakeGarbageCollected<cssvalue::CSSDynamicRangeLimitMixValue>(
        CSSIdentifierValue::Create(CSSValueID::kStandard),
        CSSIdentifierValue::Create(CSSValueID::kConstrainedHigh),
        CSSNumericLiteralValue::Create(
            100 * limit.constrained_high_mix,
            CSSPrimitiveValue::UnitType::kPercentage));
  }
  // If there is a bit of all three, nest two binary mixtures:
  // mix(standard, mix(constrained-high, high, b%), a%)
  // where b% must take into account that a% will also be applied to it.
  return MakeGarbageCollected<cssvalue::CSSDynamicRangeLimitMixValue>(
      CSSIdentifierValue::Create(CSSValueID::kStandard),
      MakeGarbageCollected<cssvalue::CSSDynamicRangeLimitMixValue>(
          CSSIdentifierValue::Create(CSSValueID::kConstrainedHigh),
          CSSIdentifierValue::Create(CSSValueID::kHigh),
          CSSNumericLiteralValue::Create(
              100 * (1 - limit.constrained_high_mix / (1 - limit.standard_mix)),
              CSSPrimitiveValue::UnitType::kPercentage)),
      CSSNumericLiteralValue::Create(100 * (1 - limit.standard_mix),
                                     CSSPrimitiveValue::UnitType::kPercentage));
}

const CSSValue* EmptyCells::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.EmptyCells());
}

const CSSValue* Fill::ParseSingleValue(CSSParserTokenStream& stream,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGPaint(stream, context);
}

const CSSValue* Fill::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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

void Fill::ApplyValue(StyleResolverState& state,
                      const CSSValue& value,
                      ValueMode) const {
  state.StyleBuilder().SetFillPaint(StyleBuilderConverter::ConvertSVGPaint(
      state, value, false, PropertyID()));
}

const CSSValue* FillOpacity::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(stream, context);
}

const CSSValue* FillOpacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.FillOpacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* FillRule::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.FillRule());
}

const CSSValue* Filter::ParseSingleValue(CSSParserTokenStream& stream,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFilterFunctionList(stream, context);
}

const CSSValue* Filter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFilter(style, style.Filter());
}

void Filter::ApplyValue(StyleResolverState& state,
                        const CSSValue& value,
                        ValueMode) const {
  state.StyleBuilder().SetFilter(StyleBuilderConverter::ConvertFilterOperations(
      state, value, PropertyID()));
}

const CSSValue* FlexBasis::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // TODO(https://crbug.com/353538495): This should really use
  // css_parsing_utils::ValidWidthOrHeightKeyword.
  if (css_parsing_utils::IdentMatches<
          CSSValueID::kAuto, CSSValueID::kContent, CSSValueID::kMinContent,
          CSSValueID::kMaxContent, CSSValueID::kFitContent>(
          stream.Peek().Id())) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative,
      css_parsing_utils::UnitlessQuirk::kForbid, kCSSAnchorQueryTypesNone,
      css_parsing_utils::AllowCalcSize::kAllowWithAutoAndContent);
}

const CSSValue* FlexBasis::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.FlexBasis(),
                                                             style);
}

const CSSValue* FlexDirection::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.FlexDirection());
}

const CSSValue* FlexDirection::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kRow);
}

const CSSValue* FlexGrow::ParseSingleValue(CSSParserTokenStream& stream,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* FlexGrow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.FlexGrow(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* FlexShrink::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* FlexShrink::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.FlexShrink(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* FlexWrap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.FlexWrap());
}

const CSSValue* FlexWrap::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNowrap);
}

const CSSValue* Float::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.HasOutOfFlowPosition()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return CSSIdentifierValue::Create(style.Floating());
}

const CSSValue* FloodColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color FloodColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  const StyleColor& flood_color = style.FloodColor();
  if (style.ShouldForceColor(flood_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return style.ResolvedColor(flood_color, is_current_color);
}

const CSSValue* FloodColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(style, style.FloodColor(),
                                                      value_phase);
}

const CSSValue* FloodOpacity::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(stream, context);
}

const CSSValue* FloodOpacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.FloodOpacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* FontFamily::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontFamily(stream);
}

const CSSValue* FontFamily::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontFeatureSettings(stream, context);
}

const CSSValue* FontFeatureSettings::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontFeatureSettings(style);
}

const CSSValue* FontKerning::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontKerning(style);
}

const CSSValue* FontOpticalSizing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontOpticalSizing(style);
}

const CSSValue* FontPalette::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontPalette(style);
}

const CSSValue* FontPalette::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontPalette(stream, context);
}

const CSSValue* FontSizeAdjust::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSFontSizeAdjustEnabled());
  return css_parsing_utils::ConsumeFontSizeAdjust(stream, context);
}

const CSSValue* FontSizeAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontSizeAdjust(style);
}

const CSSValue* FontSize::ParseSingleValue(CSSParserTokenStream& stream,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontSize(
      stream, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* FontSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontSize(style);
}

const CSSValue* FontStretch::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontStretch(stream, context);
}

const CSSValue* FontStretch::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontStretch(style);
}

const CSSValue* FontStyle::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontStyle(stream, context);
}

const CSSValue* FontStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontStyle(style);
}

const CSSValue* FontVariantCaps::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIdent<
      CSSValueID::kNormal, CSSValueID::kSmallCaps, CSSValueID::kAllSmallCaps,
      CSSValueID::kPetiteCaps, CSSValueID::kAllPetiteCaps, CSSValueID::kUnicase,
      CSSValueID::kTitlingCaps>(stream);
}

const CSSValue* FontVariantCaps::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontVariantCaps(style);
}

const CSSValue* FontVariantEastAsian::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  bool found_any = false;

  FontVariantEastAsianParser east_asian_parser;
  do {
    if (east_asian_parser.ConsumeEastAsian(stream) !=
        FontVariantEastAsianParser::ParseResult::kConsumedValue) {
      break;
    }
    found_any = true;
  } while (!stream.AtEnd());

  if (!found_any) {
    return nullptr;
  }

  return east_asian_parser.FinalizeValue();
}

const CSSValue* FontVariantEastAsian::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontVariantEastAsian(style);
}

const CSSValue* FontVariantLigatures::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNormal ||
      stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  bool found_any = false;

  FontVariantLigaturesParser ligatures_parser;
  do {
    if (ligatures_parser.ConsumeLigature(stream) !=
        FontVariantLigaturesParser::ParseResult::kConsumedValue) {
      break;
    }
    found_any = true;
  } while (!stream.AtEnd());

  if (!found_any) {
    return nullptr;
  }

  return ligatures_parser.FinalizeValue();
}

const CSSValue* FontVariantLigatures::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontVariantLigatures(style);
}

const CSSValue* FontVariantNumeric::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  bool found_any = false;

  FontVariantNumericParser numeric_parser;
  do {
    if (numeric_parser.ConsumeNumeric(stream) !=
        FontVariantNumericParser::ParseResult::kConsumedValue) {
      break;
    }
    found_any = true;
  } while (!stream.AtEnd());

  if (!found_any) {
    return nullptr;
  }

  return numeric_parser.FinalizeValue();
}

const CSSValue* FontVariantNumeric::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontVariantNumeric(style);
}

const CSSValue* FontVariantAlternates::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  bool found_any = false;

  FontVariantAlternatesParser alternates_parser;
  do {
    if (alternates_parser.ConsumeAlternates(stream, context) !=
        FontVariantAlternatesParser::ParseResult::kConsumedValue) {
      break;
    }
    found_any = true;
  } while (!stream.AtEnd());

  if (!found_any) {
    return nullptr;
  }

  return alternates_parser.FinalizeValue();
}

const CSSValue* FontVariantAlternates::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontVariantAlternates(style);
}

namespace {

cssvalue::CSSFontVariationValue* ConsumeFontVariationTag(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  // Feature tag name consists of 4-letter characters.
  static const wtf_size_t kTagNameLength = 4;

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
  for (wtf_size_t i = 0; i < kTagNameLength; ++i) {
    // Limits the range of characters to 0x20-0x7E, following the tag name
    // rules defined in the OpenType specification.
    UChar character = tag[i];
    if (character < 0x20 || character > 0x7E) {
      return nullptr;
    }
  }

  double tag_value = 0;
  if (!css_parsing_utils::ConsumeNumberRaw(stream, context, tag_value)) {
    return nullptr;
  }
  return MakeGarbageCollected<cssvalue::CSSFontVariationValue>(
      tag, ClampTo<float>(tag_value));
}

}  // namespace

const CSSValue* FontVariationSettings::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  CSSValueList* variation_settings = CSSValueList::CreateCommaSeparated();
  do {
    cssvalue::CSSFontVariationValue* font_variation_value =
        ConsumeFontVariationTag(stream, context);
    if (!font_variation_value) {
      return nullptr;
    }
    variation_settings->Append(*font_variation_value);
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream));
  return variation_settings;
}

const CSSValue* FontVariationSettings::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontVariationSettings(style);
}

const CSSValue* FontWeight::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontWeight(stream, context);
}

const CSSValue* FontWeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontWeight(style);
}

const CSSValue* FontSynthesisWeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(
      style.GetFontDescription().GetFontSynthesisWeight());
}

const CSSValue* FontSynthesisStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(
      style.GetFontDescription().GetFontSynthesisStyle());
}

const CSSValue* FontSynthesisSmallCaps::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(
      style.GetFontDescription().GetFontSynthesisSmallCaps());
}

const CSSValue* FontVariantPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForFontVariantPosition(style);
}

const CSSValue* FontVariantEmoji::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  DCHECK(RuntimeEnabledFeatures::FontVariantEmojiEnabled());
  return CSSIdentifierValue::Create(style.GetFontDescription().VariantEmoji());
}

const CSSValue* ForcedColorAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ForcedColorAdjust());
}

const CSSValue* FieldSizing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.FieldSizing());
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
                                      const CSSValue& value,
                                      ValueMode) const {
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
    // Unresolved color functions are a special case for this property.
    // See Color::ApplyValue.
    // Using Color instead of InternalVisitedColor here, see
    // https://bugs.chromium.org/p/chromium/issues/detail?id=1236297#c5.
    StyleColor color =
        StyleBuilderConverter::ConvertStyleColor(state, value, true);
    if (color.IsUnresolvedColorFunction()) {
      color = StyleColor(color.GetUnresolvedColorFunction().Resolve(
          state.ParentStyle()->Color().GetColor()));
    }
    builder.SetInternalVisitedColor(color);
  }
  builder.SetInternalVisitedColorIsCurrentColor(false);
}

const blink::Color InternalVisitedColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  if (style.ShouldForceColor(style.InternalVisitedColor())) {
    return GetCSSPropertyInternalForcedVisitedColor().ColorIncludingFallback(
        true, style, is_current_color);
  }
  return style.GetInternalVisitedCurrentColor(is_current_color);
}

const CSSValue* InternalVisitedColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColorMaybeQuirky(stream, context);
}

const CSSValue* GridAutoColumns::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTrackList(
      stream, context, css_parsing_utils::TrackListType::kGridAuto);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGridAutoTrackList(kForColumns,
                                                       layout_object, style);
}

const CSSValue* GridAutoColumns::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kAuto);
}

const CSSValue* GridAutoFlow::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSIdentifierValue* row_or_column_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kRow, CSSValueID::kColumn>(
          stream);
  CSSIdentifierValue* dense_algorithm =
      css_parsing_utils::ConsumeIdent<CSSValueID::kDense>(stream);
  if (!row_or_column_value) {
    row_or_column_value =
        css_parsing_utils::ConsumeIdent<CSSValueID::kRow, CSSValueID::kColumn>(
            stream);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTrackList(
      stream, context, css_parsing_utils::TrackListType::kGridAuto);
}

const CSSValue* GridAutoRows::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGridAutoTrackList(kForRows, layout_object,
                                                       style);
}

const CSSValue* GridAutoRows::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kAuto);
}

const CSSValue* GridColumnEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridLine(stream, context);
}

const CSSValue* GridColumnEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGridPosition(style.GridColumnEnd());
}

const CSSValue* GridColumnStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridLine(stream, context);
}

const CSSValue* GridColumnStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGridPosition(style.GridColumnStart());
}

const CSSValue* GridRowEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridLine(stream, context);
}

const CSSValue* GridRowEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGridPosition(style.GridRowEnd());
}

const CSSValue* GridRowStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridLine(stream, context);
}

const CSSValue* GridRowStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGridPosition(style.GridRowStart());
}

const CSSValue* GridTemplateAreas::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  NamedGridAreaMap grid_area_map;
  wtf_size_t row_count = 0;
  wtf_size_t column_count = 0;

  while (stream.Peek().GetType() == kStringToken) {
    if (!css_parsing_utils::ParseGridTemplateAreasRow(
            stream.ConsumeIncludingWhitespace().Value().ToString(),
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (const auto& grid_template_areas = style.GridTemplateAreas()) {
    return MakeGarbageCollected<cssvalue::CSSGridTemplateAreasValue>(
        grid_template_areas->named_areas, grid_template_areas->row_count,
        grid_template_areas->column_count);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* GridTemplateAreas::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* GridTemplateColumns::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTemplatesRowsOrColumns(stream, context);
}

bool GridTemplateColumns::IsLayoutDependent(const ComputedStyle* style,
                                            LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGrid();
}

const CSSValue* GridTemplateColumns::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGridTrackList(kForColumns, layout_object,
                                                   style);
}

const CSSValue* GridTemplateColumns::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* GridTemplateRows::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTemplatesRowsOrColumns(stream, context);
}

bool GridTemplateRows::IsLayoutDependent(const ComputedStyle* style,
                                         LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGrid();
}

const CSSValue* GridTemplateRows::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGridTrackList(kForRows, layout_object,
                                                   style);
}

const CSSValue* GridTemplateRows::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* Height::ParseSingleValue(CSSParserTokenStream& stream,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(
      stream, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

bool Height::IsLayoutDependent(const ComputedStyle* style,
                               LayoutObject* layout_object) const {
  return layout_object && (layout_object->IsBox() || layout_object->IsSVG());
}

const CSSValue* Height::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (value_phase == CSSValuePhase::kResolvedValue &&
      ComputedStyleUtils::WidthOrHeightShouldReturnUsedValue(layout_object)) {
    return ZoomAdjustedPixelValue(
        ComputedStyleUtils::UsedBoxSize(*layout_object).height(), style);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Height(),
                                                             style);
}

const CSSValue* PopoverShowDelay::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeTime(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* PopoverShowDelay::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.PopoverShowDelay(),
                                        CSSPrimitiveValue::UnitType::kSeconds);
}

const CSSValue* PopoverHideDelay::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeTime(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* PopoverHideDelay::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.PopoverHideDelay(),
                                        CSSPrimitiveValue::UnitType::kSeconds);
}

const CSSValue* HyphenateLimitChars::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeHyphenateLimitChars(stream, context);
}

const CSSValue* Hyphens::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetHyphens());
}

const CSSValue* ImageOrientation::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIdent<CSSValueID::kFromImage,
                                         CSSValueID::kNone>(stream);
}

const CSSValue* ImageOrientation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const CSSValueID value = style.ImageOrientation() == kRespectImageOrientation
                               ? CSSValueID::kFromImage
                               : CSSValueID::kNone;
  return CSSIdentifierValue::Create(value);
}

const CSSValue* ImageRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ImageRendering());
}

const CSSValue* InitialLetter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeInitialLetter(stream, context);
}

const CSSValue* InlineSize::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(stream, context);
}

bool InlineSize::IsLayoutDependent(const ComputedStyle* style,
                                   LayoutObject* layout_object) const {
  return layout_object && (layout_object->IsBox() || layout_object->IsSVG());
}

const CSSValue* PositionArea::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumePositionArea(stream);
}

// TODO(crbug.com/352360007): this can be removed when inset-area is removed.
const CSSValue* InsetArea::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  if (const auto* document = context.GetDocument()) {
    document->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kDeprecation,
        mojom::blink::ConsoleMessageLevel::kWarning,
        String(
            "The 'inset-area' property has been deprecated, "
            "and will be removed from this browser very soon. Please use the "
            "'position-area' property instead.")));
    Deprecation::CountDeprecation(document->GetExecutionContext(),
                                  WebFeature::kCSSInsetAreaProperty);
  }
  return css_parsing_utils::ConsumePositionArea(stream);
}

const CSSValue* PositionArea::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForPositionArea(style.GetPositionArea());
}

// TODO(crbug.com/352360007): this can be removed when inset-area is removed.
const CSSValue* InsetArea::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForPositionArea(style.GetPositionArea());
}

namespace {

void ComputeAnchorEdgeOffsetsForPositionArea(
    StyleResolverState& state,
    blink::PositionArea position_area) {
  if (AnchorEvaluator* evaluator =
          state.CssToLengthConversionData().GetAnchorEvaluator()) {
    state.SetPositionAreaOffsets(evaluator->ComputePositionAreaOffsetsForLayout(
        state.StyleBuilder().PositionAnchor(), position_area));
  }
  state.StyleBuilder().SetHasAnchorFunctions();
}

}  // namespace

void PositionArea::ApplyValue(StyleResolverState& state,
                              const CSSValue& value,
                              ValueMode) const {
  blink::PositionArea position_area =
      StyleBuilderConverter::ConvertPositionArea(state, value);
  state.StyleBuilder().SetPositionArea(position_area);
  if (!position_area.IsNone()) {
    ComputeAnchorEdgeOffsetsForPositionArea(state, position_area);
  }
}

void PositionArea::ApplyInherit(StyleResolverState& state) const {
  blink::PositionArea position_area = state.ParentStyle()->GetPositionArea();
  state.StyleBuilder().SetPositionArea(position_area);
  if (!position_area.IsNone()) {
    ComputeAnchorEdgeOffsetsForPositionArea(state, position_area);
  }
}

const CSSValue* InsetBlockEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid,
      kCSSAnchorQueryTypesAll);
}

bool InsetBlockEnd::IsLayoutDependent(const ComputedStyle* style,
                                      LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* InsetBlockStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid,
      kCSSAnchorQueryTypesAll);
}

bool InsetBlockStart::IsLayoutDependent(const ComputedStyle* style,
                                        LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* InsetInlineEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid,
      kCSSAnchorQueryTypesAll);
}

bool InsetInlineEnd::IsLayoutDependent(const ComputedStyle* style,
                                       LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* InsetInlineStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid,
      kCSSAnchorQueryTypesAll);
}

bool InsetInlineStart::IsLayoutDependent(const ComputedStyle* style,
                                         LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const blink::Color InternalVisitedBackgroundColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);

  const StyleColor& visited_background_color =
      style.InternalVisitedBackgroundColor();
  if (style.ShouldForceColor(visited_background_color)) {
    return GetCSSPropertyInternalForcedBackgroundColor().ColorIncludingFallback(
        true, style, is_current_color);
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColorMaybeQuirky(stream, context);
}

const blink::Color InternalVisitedBorderLeftColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  const StyleColor& visited_border_left_color =
      style.InternalVisitedBorderLeftColor();
  if (style.ShouldForceColor(visited_border_left_color)) {
    return GetCSSPropertyInternalForcedBorderColor().ColorIncludingFallback(
        true, style, is_current_color);
  }
  return visited_border_left_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedBorderLeftColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const blink::Color InternalVisitedBorderTopColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  const StyleColor& visited_border_top_color =
      style.InternalVisitedBorderTopColor();
  if (style.ShouldForceColor(visited_border_top_color)) {
    return GetCSSPropertyInternalForcedBorderColor().ColorIncludingFallback(
        true, style, is_current_color);
  }
  return visited_border_top_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedBorderTopColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const blink::Color InternalVisitedCaretColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  const StyleAutoColor& auto_color = style.InternalVisitedCaretColor();
  const StyleColor result = auto_color.IsAutoColor()
                                ? StyleColor::CurrentColor()
                                : auto_color.ToStyleColor();
  if (style.ShouldForceColor(result)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return result.Resolve(style.GetInternalVisitedCurrentColor(),
                        style.UsedColorScheme(), is_current_color);
}

const CSSValue* InternalVisitedCaretColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return GetCSSPropertyCaretColor().ParseSingleValue(stream, context,
                                                     local_context);
}

const blink::Color InternalVisitedBorderRightColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  const StyleColor& visited_border_right_color =
      style.InternalVisitedBorderRightColor();
  if (style.ShouldForceColor(visited_border_right_color)) {
    return GetCSSPropertyInternalForcedBorderColor().ColorIncludingFallback(
        true, style, is_current_color);
  }
  return visited_border_right_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedBorderRightColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const blink::Color InternalVisitedBorderBottomColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  const StyleColor& visited_border_bottom_color =
      style.InternalVisitedBorderBottomColor();
  if (style.ShouldForceColor(visited_border_bottom_color)) {
    return GetCSSPropertyInternalForcedBorderColor().ColorIncludingFallback(
        true, style, is_current_color);
  }
  return visited_border_bottom_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedBorderBottomColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const CSSValue* InternalVisitedBorderInlineStartColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const CSSValue* InternalVisitedBorderInlineEndColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const CSSValue* InternalVisitedBorderBlockStartColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const CSSValue* InternalVisitedBorderBlockEndColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeBorderColorSide(stream, context,
                                                   local_context);
}

const CSSValue* InternalVisitedFill::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeSVGPaint(stream, context);
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
    return GetCSSPropertyFill().ColorIncludingFallback(false, style,
                                                       is_current_color);
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
  const StyleColor& visited_column_rule_color =
      style.InternalVisitedColumnRuleColor();
  if (style.ShouldForceColor(visited_column_rule_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_column_rule_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedColumnRuleColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color InternalVisitedOutlineColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  const StyleColor& visited_outline_color = style.InternalVisitedOutlineColor();
  if (style.ShouldForceColor(visited_outline_color)) {
    return GetCSSPropertyInternalForcedOutlineColor().ColorIncludingFallback(
        true, style, is_current_color);
  }
  return visited_outline_color.Resolve(style.GetInternalVisitedCurrentColor(),
                                       style.UsedColorScheme(),
                                       is_current_color);
}

const CSSValue* InternalVisitedOutlineColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return GetCSSPropertyOutlineColor().ParseSingleValue(stream, context,
                                                       local_context);
}

const CSSValue* InternalVisitedStroke::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeSVGPaint(stream, context);
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
    return GetCSSPropertyStroke().ColorIncludingFallback(false, style,
                                                         is_current_color);
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
  const StyleColor& visited_decoration_color =
      style.DecorationColorIncludingFallback(visited_link);
  if (style.ShouldForceColor(visited_decoration_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_decoration_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedTextDecorationColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color InternalVisitedTextEmphasisColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  const StyleColor& visited_text_emphasis_color =
      style.InternalVisitedTextEmphasisColor();
  if (style.ShouldForceColor(visited_text_emphasis_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_text_emphasis_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedTextEmphasisColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color InternalVisitedTextFillColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  const StyleColor& visited_text_fill_color =
      style.InternalVisitedTextFillColor();
  if (style.ShouldForceColor(visited_text_fill_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_text_fill_color.Resolve(style.GetInternalVisitedCurrentColor(),
                                         style.UsedColorScheme(),
                                         is_current_color);
}

const CSSValue* InternalVisitedTextFillColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color InternalVisitedTextStrokeColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(visited_link);
  const StyleColor& visited_text_stroke_color =
      style.InternalVisitedTextStrokeColor();
  if (style.ShouldForceColor(visited_text_stroke_color)) {
    return style.GetInternalForcedVisitedCurrentColor(is_current_color);
  }
  return visited_text_stroke_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme(),
      is_current_color);
}

const CSSValue* InternalVisitedTextStrokeColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(stream, context);
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
                .AlphaAsInteger();
  } else {
    forced_current_color = style.GetInternalForcedCurrentColor(
        /* No is_current_color because we might not be forced_current_color */);
    alpha = style.BackgroundColor()
                .Resolve(style.GetCurrentColor(), style.UsedColorScheme(),
                         &alpha_is_current_color)
                .AlphaAsInteger();
  }

  bool result_is_current_color;
  blink::Color result = style.InternalForcedBackgroundColor().ResolveWithAlpha(
      forced_current_color, style.UsedColorScheme(), alpha,
      &result_is_current_color);

  if (is_current_color) {
    *is_current_color = alpha_is_current_color || result_is_current_color;
  }
  return result;
}

const CSSValue*
InternalForcedBackgroundColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  bool visited_link = allow_visited_style &&
                      style.InsideLink() == EInsideLink::kInsideVisitedLink;
  return cssvalue::CSSColor::Create(
      ColorIncludingFallback(visited_link, style));
}

const CSSValue* InternalForcedBackgroundColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColorMaybeQuirky(stream, context);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  bool visited_link = allow_visited_style &&
                      style.InsideLink() == EInsideLink::kInsideVisitedLink;
  return cssvalue::CSSColor::Create(
      ColorIncludingFallback(visited_link, style));
}

const CSSValue* InternalForcedBorderColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColorMaybeQuirky(stream, context);
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
                                     const CSSValue& value,
                                     ValueMode) const {
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return cssvalue::CSSColor::Create(
      allow_visited_style ? style.VisitedDependentColor(*this)
                          : style.GetInternalForcedCurrentColor());
}

const CSSValue* InternalForcedColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColorMaybeQuirky(stream, context);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  bool visited_link = allow_visited_style &&
                      style.InsideLink() == EInsideLink::kInsideVisitedLink;
  return cssvalue::CSSColor::Create(
      ColorIncludingFallback(visited_link, style));
}

const CSSValue* InternalForcedOutlineColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColorMaybeQuirky(stream, context);
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
                                            const CSSValue& value,
                                            ValueMode) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColorMaybeQuirky(stream, context);
}

const CSSValue* InterpolateSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.InterpolateSize());
}

const CSSValue* Isolation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.Isolation());
}

const CSSValue* JustifyContent::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // justify-content property does not allow the <baseline-position> values.
  if (css_parsing_utils::IdentMatches<CSSValueID::kFirst, CSSValueID::kLast,
                                      CSSValueID::kBaseline>(
          stream.Peek().Id())) {
    return nullptr;
  }
  return css_parsing_utils::ConsumeContentDistributionOverflowPosition(
      stream, css_parsing_utils::IsContentPositionOrLeftOrRightKeyword);
}

const CSSValue* JustifyContent::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::
      ValueForContentPositionAndDistributionWithOverflowAlignment(
          style.JustifyContent());
}

const CSSValue* JustifyItems::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSParserTokenStream::State savepoint = stream.Save();
  // justify-items property does not allow the 'auto' value.
  if (css_parsing_utils::IdentMatches<CSSValueID::kAuto>(stream.Peek().Id())) {
    return nullptr;
  }
  CSSIdentifierValue* legacy =
      css_parsing_utils::ConsumeIdent<CSSValueID::kLegacy>(stream);
  CSSIdentifierValue* position_keyword =
      css_parsing_utils::ConsumeIdent<CSSValueID::kCenter, CSSValueID::kLeft,
                                      CSSValueID::kRight>(stream);
  if (!legacy) {
    legacy = css_parsing_utils::ConsumeIdent<CSSValueID::kLegacy>(stream);
  }
  if (!legacy) {
    stream.Restore(savepoint);
  }
  if (legacy) {
    if (position_keyword) {
      context.Count(WebFeature::kCSSLegacyAlignment);
      return MakeGarbageCollected<CSSValuePair>(
          legacy, position_keyword, CSSValuePair::kDropIdenticalValues);
    }
    return legacy;
  }

  return css_parsing_utils::ConsumeSelfPositionOverflowPosition(
      stream, css_parsing_utils::IsSelfPositionOrLeftOrRightKeyword);
}

const CSSValue* JustifyItems::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForItemPositionWithOverflowAlignment(
      style.JustifyItems().GetPosition() == ItemPosition::kAuto
          ? ComputedStyleInitialValues::InitialDefaultAlignment()
          : style.JustifyItems());
}

const CSSValue* JustifySelf::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSelfPositionOverflowPosition(
      stream, css_parsing_utils::IsSelfPositionOrLeftOrRightKeyword);
}

const CSSValue* JustifySelf::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForItemPositionWithOverflowAlignment(
      style.JustifySelf());
}

const CSSValue* Left::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context,
      css_parsing_utils::UnitlessUnlessShorthand(local_context),
      kCSSAnchorQueryTypesAll);
}

bool Left::IsLayoutDependent(const ComputedStyle* style,
                             LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Left::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForPositionOffset(style, *this,
                                                    layout_object);
}

const CSSValue* LetterSpacing::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseSpacing(stream, context);
}

const CSSValue* LetterSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (!style.LetterSpacing()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
  return ZoomAdjustedPixelValue(style.LetterSpacing(), style);
}

const CSSValue* LightingColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color LightingColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  const StyleColor& lighting_color = style.LightingColor();
  if (style.ShouldForceColor(lighting_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return style.ResolvedColor(lighting_color, is_current_color);
}

const CSSValue* LightingColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.LightingColor(), value_phase);
}

const CSSValue* LineBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetLineBreak());
}

void LineClamp::ApplyInitial(StyleResolverState& state) const {
  // initial needs to be customized so it doesn't default to `auto`.
  state.StyleBuilder().SetStandardLineClamp(0);
}

const CSSValue* LineClamp::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone ||
      stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  } else {
    return css_parsing_utils::ConsumePositiveInteger(stream, context);
  }
}

const CSSValue* LineClamp::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.HasAutoStandardLineClamp()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  if (style.StandardLineClamp() == 0) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return CSSNumericLiteralValue::Create(style.StandardLineClamp(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* LineHeight::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLineHeight(stream, context);
}

const CSSValue* LineHeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (value_phase == CSSValuePhase::kComputedValue) {
    return ComputedStyleUtils::ComputedValueForLineHeight(style);
  }
  return ComputedStyleUtils::ValueForLineHeight(style);
}

const CSSValue* ListStyleImage::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeImageOrNone(stream, context);
}

const CSSValue* ListStyleImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.ListStyleImage()) {
    return style.ListStyleImage()->ComputedCSSValue(style, allow_visited_style,
                                                    value_phase);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

void ListStyleImage::ApplyValue(StyleResolverState& state,
                                const CSSValue& value,
                                ValueMode) const {
  state.StyleBuilder().SetListStyleImage(
      state.GetStyleImage(CSSPropertyID::kListStyleImage, value));
}

const CSSValue* ListStylePosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ListStylePosition());
}

const CSSValue* ListStyleType::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (auto* none = css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(stream)) {
    return none;
  }

  if (auto* counter_style_name =
          css_parsing_utils::ConsumeCounterStyleName(stream, context)) {
    return counter_style_name;
  }

  return css_parsing_utils::ConsumeString(stream);
}

const CSSValue* ListStyleType::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (!style.ListStyleType()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  const ListStyleTypeData& list_style_type = *style.ListStyleType();
  if (list_style_type.IsString()) {
    return MakeGarbageCollected<CSSStringValue>(
        list_style_type.GetStringValue());
  }
  return &MakeGarbageCollected<CSSCustomIdentValue>(
              list_style_type.GetCounterStyleName())
              ->PopulateWithTreeScope(list_style_type.GetTreeScope());
}

void ListStyleType::ApplyValue(StyleResolverState& state,
                               const CSSValue& value,
                               ValueMode) const {
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
  // “The non-overridable counter-style names are the keywords decimal,
  // disc, square, circle, disclosure-open, and disclosure-closed.”
  //
  // NOTE: Keep in sync with ConsumeCounterStyleNameInPrelude().
  //
  // https://drafts.csswg.org/css-counter-styles/#the-counter-style-rule
  if (custom_ident_value.Value() != keywords::kDecimal &&
      custom_ident_value.Value() != keywords::kDisc &&
      custom_ident_value.Value() != keywords::kSquare &&
      custom_ident_value.Value() != keywords::kCircle &&
      custom_ident_value.Value() != keywords::kDisclosureOpen &&
      custom_ident_value.Value() != keywords::kDisclosureClosed) {
    state.SetHasTreeScopedReference();
  }
  builder.SetListStyleType(ListStyleTypeData::CreateCounterStyle(
      custom_ident_value.Value(), custom_ident_value.GetTreeScope()));
}

bool MarginBlockEnd::IsLayoutDependent(const ComputedStyle* style,
                                       LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* MarginBlockEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize));
}

bool MarginBlockStart::IsLayoutDependent(const ComputedStyle* style,
                                         LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* MarginBlockStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize));
}

const CSSValue* MarginBottom::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kAllow,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize));
}

bool MarginBottom::IsLayoutDependent(const ComputedStyle* style,
                                     LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* MarginBottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (const LayoutBox* box = DynamicTo<LayoutBox>(layout_object)) {
    if (!style.MarginBottom().IsFixed()) {
      return ZoomAdjustedPixelValue(box->MarginBottom(), style);
    }
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.MarginBottom(), style);
}

bool MarginInlineEnd::IsLayoutDependent(const ComputedStyle* style,
                                        LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* MarginInlineEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize));
}

bool MarginInlineStart::IsLayoutDependent(const ComputedStyle* style,
                                          LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* MarginInlineStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize));
}

const CSSValue* MarginLeft::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kAllow,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize));
}

bool MarginLeft::IsLayoutDependent(const ComputedStyle* style,
                                   LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->MarginLeft().IsFixed());
}

const CSSValue* MarginLeft::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (const LayoutBox* box = DynamicTo<LayoutBox>(layout_object)) {
    if (!style.MarginLeft().IsFixed()) {
      return ZoomAdjustedPixelValue(box->MarginLeft(), style);
    }
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.MarginLeft(),
                                                             style);
}

const CSSValue* MarginRight::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kAllow,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize));
}

bool MarginRight::IsLayoutDependent(const ComputedStyle* style,
                                    LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->MarginRight().IsFixed());
}

const CSSValue* MarginRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (const LayoutBox* box = DynamicTo<LayoutBox>(layout_object)) {
    if (!style.MarginRight().IsFixed()) {
      return ZoomAdjustedPixelValue(box->MarginRight(), style);
    }
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.MarginRight(), style);
}

const CSSValue* MarginTop::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context, css_parsing_utils::UnitlessQuirk::kAllow,
      static_cast<CSSAnchorQueryTypes>(CSSAnchorQueryType::kAnchorSize));
}

bool MarginTop::IsLayoutDependent(const ComputedStyle* style,
                                  LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->MarginTop().IsFixed());
}

const CSSValue* MarginTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (const LayoutBox* box = DynamicTo<LayoutBox>(layout_object)) {
    if (!style.MarginTop().IsFixed()) {
      return ZoomAdjustedPixelValue(box->MarginTop(), style);
    }
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.MarginTop(),
                                                             style);
}

const CSSValue* MarkerEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeUrl(stream, context);
}

const CSSValue* MarkerEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForSVGResource(style.MarkerEndResource());
}

void MarkerEnd::ApplyValue(StyleResolverState& state,
                           const CSSValue& value,
                           ValueMode) const {
  state.StyleBuilder().SetMarkerEndResource(
      StyleBuilderConverter::ConvertElementReference(state, value,
                                                     PropertyID()));
}

const CSSValue* MarkerMid::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeUrl(stream, context);
}

const CSSValue* MarkerMid::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForSVGResource(style.MarkerMidResource());
}

void MarkerMid::ApplyValue(StyleResolverState& state,
                           const CSSValue& value,
                           ValueMode) const {
  state.StyleBuilder().SetMarkerMidResource(
      StyleBuilderConverter::ConvertElementReference(state, value,
                                                     PropertyID()));
}

const CSSValue* MarkerStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeUrl(stream, context);
}

const CSSValue* MarkerStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForSVGResource(style.MarkerStartResource());
}

void MarkerStart::ApplyValue(StyleResolverState& state,
                             const CSSValue& value,
                             ValueMode) const {
  state.StyleBuilder().SetMarkerStartResource(
      StyleBuilderConverter::ConvertElementReference(state, value,
                                                     PropertyID()));
}

const CSSValue* MaskType::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.MaskType());
}

const CSSValue* MasonrySlack::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMasonrySlack(stream, context);
}

const CSSValue* MasonrySlack::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForMasonrySlack(style.MasonrySlack(), style);
}

const CSSValue* MasonryTemplateTracks::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTemplatesRowsOrColumns(stream, context);
}

bool MasonryTemplateTracks::IsLayoutDependent(
    const ComputedStyle* style,
    LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutMasonry();
}

const CSSValue* MasonryTemplateTracks::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForMasonryTrackList(layout_object, style);
}

const CSSValue* MasonryTemplateTracks::InitialValue() const {
  auto* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  return list;
}

const CSSValue* MasonryTrackEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridLine(stream, context);
}

const CSSValue* MasonryTrackEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGridPosition(style.MasonryTrackEnd());
}

const CSSValue* MasonryTrackStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridLine(stream, context);
}

const CSSValue* MasonryTrackStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGridPosition(style.MasonryTrackStart());
}

const CSSValue* MathShift::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.MathShift());
}

const CSSValue* MathStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.MathStyle());
}

const CSSValue* MathDepth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMathDepth(stream, context);
}

const CSSValue* MathDepth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.MathDepth(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

void MathDepth::ApplyValue(StyleResolverState& state,
                           const CSSValue& value,
                           ValueMode) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    DCHECK_EQ(list->length(), 1U);
    const auto& relative_value = To<CSSPrimitiveValue>(list->Item(0));
    builder.SetMathDepth(base::ClampAdd(
        state.ParentStyle()->MathDepth(),
        relative_value.ComputeInteger(state.CssToLengthConversionData())));
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
        ClampTo<int16_t>(To<CSSPrimitiveValue>(value).ComputeInteger(
            state.CssToLengthConversionData())));
  }
}

const CSSValue* MaxBlockSize::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMaxWidthOrHeight(stream, context);
}

const CSSValue* MaxHeight::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMaxWidthOrHeight(
      stream, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* MaxHeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const Length& max_height = style.MaxHeight();
  if (max_height.IsNone()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(max_height, style);
}

const CSSValue* MaxInlineSize::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMaxWidthOrHeight(stream, context);
}

const CSSValue* MaxWidth::ParseSingleValue(CSSParserTokenStream& stream,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMaxWidthOrHeight(
      stream, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* MaxWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const Length& max_width = style.MaxWidth();
  if (max_width.IsNone()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(max_width, style);
}

const CSSValue* MinBlockSize::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeAppearanceAutoBaseSelect(
      css_parsing_utils::ConsumeWidthOrHeight, stream, context,
      css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* MinHeight::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(
      stream, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* MinHeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.MinHeight().IsAuto()) {
    if (value_phase == CSSValuePhase::kComputedValue) {
      return CSSIdentifierValue::Create(CSSValueID::kAuto);
    }
    return ComputedStyleUtils::MinWidthOrMinHeightAuto(style);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.MinHeight(),
                                                             style);
}

const CSSValue* MinInlineSize::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeAppearanceAutoBaseSelect(
      css_parsing_utils::ConsumeWidthOrHeight, stream, context,
      css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* MinWidth::ParseSingleValue(CSSParserTokenStream& stream,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(
      stream, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* MinWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.MinWidth().IsAuto()) {
    if (value_phase == CSSValuePhase::kComputedValue) {
      return CSSIdentifierValue::Create(CSSValueID::kAuto);
    }
    return ComputedStyleUtils::MinWidthOrMinHeightAuto(style);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.MinWidth(),
                                                             style);
}

const CSSValue* MixBlendMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetBlendMode());
}

const CSSValue* ObjectFit::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetObjectFit());
}

const CSSValue* ObjectPosition::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumePosition(stream, context,
                         css_parsing_utils::UnitlessQuirk::kForbid,
                         std::optional<WebFeature>());
}

const CSSValue* ObjectPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return MakeGarbageCollected<CSSValuePair>(
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.ObjectPosition().X(), style),
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.ObjectPosition().Y(), style),
      CSSValuePair::kKeepIdenticalValues);
}

const CSSValue* ObjectViewBox::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  auto* css_value = css_parsing_utils::ConsumeBasicShape(
      stream, context, css_parsing_utils::AllowPathValue::kForbid);

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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (auto* basic_shape = style.ObjectViewBox()) {
    return ValueForBasicShape(style, basic_shape);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* OffsetAnchor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumePosition(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid,
      std::optional<WebFeature>());
}

const CSSValue* OffsetAnchor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForPosition(style.OffsetAnchor(), style);
}

const CSSValue* OffsetDistance::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* OffsetDistance::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.OffsetDistance(), style);
}

const CSSValue* OffsetPath::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeOffsetPath(stream, context);
}

const CSSValue* OffsetPath::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const OffsetPathOperation* operation = style.OffsetPath();
  if (operation) {
    if (const auto* shape_operation =
            DynamicTo<ShapeOffsetPathOperation>(operation)) {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      CSSValue* shape =
          ValueForBasicShape(style, &shape_operation->GetBasicShape());
      list->Append(*shape);
      CoordBox coord_box = shape_operation->GetCoordBox();
      if (coord_box != CoordBox::kBorderBox) {
        list->Append(*CSSIdentifierValue::Create(coord_box));
      }
      return list;
    }
    if (const auto* coord_box_operation =
            DynamicTo<CoordBoxOffsetPathOperation>(operation)) {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      CoordBox coord_box = coord_box_operation->GetCoordBox();
      list->Append(*CSSIdentifierValue::Create(coord_box));
      return list;
    }
    const auto& reference_operation =
        To<ReferenceOffsetPathOperation>(*operation);
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    AtomicString url = reference_operation.Url();
    list->Append(*MakeGarbageCollected<cssvalue::CSSURIValue>(CSSUrlData(url)));
    CoordBox coord_box = reference_operation.GetCoordBox();
    if (coord_box != CoordBox::kBorderBox) {
      list->Append(*CSSIdentifierValue::Create(coord_box));
    }
    return list;
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* OffsetPosition::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  if (id == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  CSSValue* value = css_parsing_utils::ConsumePosition(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid,
      std::optional<WebFeature>());

  // Count when we receive a valid position other than 'auto'.
  if (value && value->IsValuePair()) {
    context.Count(WebFeature::kCSSOffsetInEffect);
  }
  return value;
}

const CSSValue* OffsetPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForPosition(style.OffsetPosition(), style);
}

const CSSValue* OffsetRotate::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeOffsetRotate(stream, context);
}

const CSSValue* OffsetRotate::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (style.OffsetRotate().type == OffsetRotationType::kAuto) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  }
  list->Append(*CSSNumericLiteralValue::Create(
      style.OffsetRotate().angle, CSSPrimitiveValue::UnitType::kDegrees));
  return list;
}

const CSSValue* Opacity::ParseSingleValue(CSSParserTokenStream& stream,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(stream, context);
}

const CSSValue* Opacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.Opacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* Order::ParseSingleValue(CSSParserTokenStream& stream,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeInteger(stream, context);
}

const CSSValue* Order::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.Order(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* OriginTrialTestProperty::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.OriginTrialTestProperty());
  ;
}

const CSSValue* Orphans::ParseSingleValue(CSSParserTokenStream& stream,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositiveInteger(stream, context);
}

const CSSValue* Orphans::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.Orphans(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* OutlineColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // Allow the special focus color even in HTML Standard parsing mode.
  if (stream.Peek().Id() == CSSValueID::kWebkitFocusRingColor) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeColor(stream, context);
}

const CSSValue* AccentColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeColor(stream, context);
}

const CSSValue* AccentColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const StyleAutoColor& auto_color = style.AccentColor();
  if (auto_color.IsAutoColor()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  return ComputedStyleUtils::ValueForStyleAutoColor(style, style.AccentColor(),
                                                    value_phase);
}

const blink::Color OutlineColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& outline_color = style.OutlineColor();
  if (style.ShouldForceColor(outline_color)) {
    return GetCSSPropertyInternalForcedOutlineColor().ColorIncludingFallback(
        false, style, is_current_color);
  }
  return outline_color.Resolve(style.GetCurrentColor(), style.UsedColorScheme(),
                               is_current_color);
}

const CSSValue* OutlineColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const StyleColor& outline_color = style.OutlineColor();
  if (value_phase == CSSValuePhase::kResolvedValue &&
      style.ShouldForceColor(outline_color)) {
    return GetCSSPropertyInternalForcedOutlineColor().CSSValueFromComputedStyle(
        style, nullptr, allow_visited_style, value_phase);
  }
  return allow_visited_style
             ? cssvalue::CSSColor::Create(style.VisitedDependentColor(*this))
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, outline_color, value_phase);
}

const CSSValue* OutlineOffset::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(stream, context,
                                          CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* OutlineOffset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.OutlineOffset(), style);
}

const CSSValue* OutlineStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
                              const CSSValue& value,
                              ValueMode) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  const auto& identifier_value = To<CSSIdentifierValue>(value);
  builder.SetOutlineStyleIsAuto(
      static_cast<bool>(identifier_value.ConvertTo<OutlineIsAuto>()));
  builder.SetOutlineStyle(identifier_value.ConvertTo<EBorderStyle>());
}

const CSSValue* OutlineWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLineWidth(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* OutlineWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.OutlineWidth(), style);
}

const CSSValue* OverflowAnchor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.OverflowAnchor());
}

const CSSValue* OverflowClipMargin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  auto* css_value_list = CSSValueList::CreateSpaceSeparated();

  if (!style.OverflowClipMargin()) {
    css_value_list->Append(
        *CSSPrimitiveValue::CreateFromLength(Length::Fixed(0), 1.f));
    return css_value_list;
  }

  if (style.OverflowClipMargin()->GetReferenceBox() ==
          StyleOverflowClipMargin::ReferenceBox::kPaddingBox &&
      style.OverflowClipMargin()->GetMargin() == LayoutUnit()) {
    css_value_list->Append(
        *CSSPrimitiveValue::CreateFromLength(Length::Fixed(0), 1.f));
    return css_value_list;
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSPrimitiveValue* length;
  CSSIdentifierValue* reference_box;

  if (stream.Peek().GetType() != kIdentToken &&
      stream.Peek().GetType() != kDimensionToken) {
    return nullptr;
  }

  if (stream.Peek().GetType() == kIdentToken) {
    reference_box = css_parsing_utils::ConsumeVisualBox(stream);
    length = css_parsing_utils::ConsumeLength(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  } else {
    length = css_parsing_utils::ConsumeLength(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    reference_box = css_parsing_utils::ConsumeVisualBox(stream);
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
  } else if (reference_box && length &&
             length->IsZero() == CSSPrimitiveValue::BoolStatus::kTrue) {
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.OverflowWrap());
}

const CSSValue* OverflowX::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
                           const CSSValue& value,
                           ValueMode) const {
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
                           const CSSValue& value,
                           ValueMode) const {
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.OverscrollBehaviorX());
}

const CSSValue* OverscrollBehaviorY::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.OverscrollBehaviorY());
}

bool PaddingBlockEnd::IsLayoutDependent(const ComputedStyle* style,
                                        LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* PaddingBlockEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kForbid);
}

bool PaddingBlockStart::IsLayoutDependent(const ComputedStyle* style,
                                          LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* PaddingBlockStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* PaddingBottom::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeAppearanceAutoBaseSelect(
      css_parsing_utils::ConsumeLengthOrPercent, stream, context,
      CSSPrimitiveValue::ValueRange::kNonNegative,
      css_parsing_utils::UnitlessQuirk::kAllow, kCSSAnchorQueryTypesNone,
      css_parsing_utils::AllowCalcSize::kForbid);
}

bool PaddingBottom::IsLayoutDependent(const ComputedStyle* style,
                                      LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingBottom().IsFixed());
}

const CSSValue* PaddingBottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kForbid);
}

bool PaddingInlineStart::IsLayoutDependent(const ComputedStyle* style,
                                           LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* PaddingInlineStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(stream, context,
                                CSSPrimitiveValue::ValueRange::kNonNegative,
                                css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* PaddingLeft::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeAppearanceAutoBaseSelect(
      css_parsing_utils::ConsumeLengthOrPercent, stream, context,
      CSSPrimitiveValue::ValueRange::kNonNegative,
      css_parsing_utils::UnitlessQuirk::kAllow, kCSSAnchorQueryTypesNone,
      css_parsing_utils::AllowCalcSize::kForbid);
}

bool PaddingLeft::IsLayoutDependent(const ComputedStyle* style,
                                    LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingLeft().IsFixed());
}

const CSSValue* PaddingLeft::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const Length& padding_left = style.PaddingLeft();
  if (padding_left.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(padding_left,
                                                               style);
  }
  return ZoomAdjustedPixelValue(
      To<LayoutBox>(layout_object)->ComputedCSSPaddingLeft(), style);
}

const CSSValue* PaddingRight::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeAppearanceAutoBaseSelect(
      css_parsing_utils::ConsumeLengthOrPercent, stream, context,
      CSSPrimitiveValue::ValueRange::kNonNegative,
      css_parsing_utils::UnitlessQuirk::kAllow, kCSSAnchorQueryTypesNone,
      css_parsing_utils::AllowCalcSize::kForbid);
}

bool PaddingRight::IsLayoutDependent(const ComputedStyle* style,
                                     LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingRight().IsFixed());
}

const CSSValue* PaddingRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const Length& padding_right = style.PaddingRight();
  if (padding_right.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(padding_right,
                                                               style);
  }
  return ZoomAdjustedPixelValue(
      To<LayoutBox>(layout_object)->ComputedCSSPaddingRight(), style);
}

const CSSValue* PaddingTop::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeAppearanceAutoBaseSelect(
      css_parsing_utils::ConsumeLengthOrPercent, stream, context,
      CSSPrimitiveValue::ValueRange::kNonNegative,
      css_parsing_utils::UnitlessQuirk::kAllow, kCSSAnchorQueryTypesNone,
      css_parsing_utils::AllowCalcSize::kForbid);
}

bool PaddingTop::IsLayoutDependent(const ComputedStyle* style,
                                   LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingTop().IsFixed());
}

const CSSValue* PaddingTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const Length& padding_top = style.PaddingTop();
  if (padding_top.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(padding_top,
                                                               style);
  }
  return ZoomAdjustedPixelValue(
      To<LayoutBox>(layout_object)->ComputedCSSPaddingTop(), style);
}

const CSSValue* Page::ParseSingleValue(CSSParserTokenStream& stream,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeCustomIdent(stream, context);
}

const CSSValue* Page::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.Page().IsNull()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(style.Page());
}

const CSSValue* ViewTransitionName::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return RuntimeEnabledFeatures::CSSViewTransitionAutoNameEnabled()
               ? css_parsing_utils::ConsumeIdent(stream)
               : nullptr;
  }
  return css_parsing_utils::ConsumeCustomIdent(stream, context);
}

const CSSValue* ViewTransitionName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (!style.ViewTransitionName()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  if (style.ViewTransitionName()->IsAuto()) {
    CHECK(RuntimeEnabledFeatures::CSSViewTransitionAutoNameEnabled());
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  CHECK(style.ViewTransitionName()->IsCustom());
  return MakeGarbageCollected<CSSCustomIdentValue>(
      style.ViewTransitionName()->CustomName());
}

const CSSValue* ViewTransitionClass::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // The valid syntax is `none | <<custom-ident>>*` where the list of custom
  // idents can't include `none`. So handle `none` separately, and then consume
  // a list without `none`s.
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeSpaceSeparatedList(
      ConsumeCustomIdentExcludingNone, stream, context);
}

const CSSValue* ViewTransitionClass::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const auto& view_transition_class = style.ViewTransitionClass();
  if (!view_transition_class || view_transition_class->GetNames().empty()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  CSSValueList* ident_list = CSSValueList::CreateSpaceSeparated();
  for (const auto& class_name : view_transition_class->GetNames()) {
    auto* value =
        MakeGarbageCollected<CSSCustomIdentValue>(class_name->GetName());
    value->EnsureScopedValue(class_name->GetTreeScope());
    ident_list->Append(*value);
  }
  return ident_list;
}

const CSSValue* ViewTransitionGroup::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  auto id = stream.Peek().Id();
  if (id == CSSValueID::kNormal || id == CSSValueID::kNearest ||
      id == CSSValueID::kContain) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeCustomIdent(stream, context);
}

const CSSValue* ViewTransitionGroup::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.ViewTransitionGroup().IsNormal()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  } else if (style.ViewTransitionGroup().IsNearest()) {
    return CSSIdentifierValue::Create(CSSValueID::kNearest);
  } else if (style.ViewTransitionGroup().IsContain()) {
    return CSSIdentifierValue::Create(CSSValueID::kContain);
  }
  return MakeGarbageCollected<CSSCustomIdentValue>(
      style.ViewTransitionGroup().CustomName());
}

const CSSValue* PaintOrder::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNormal) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  Vector<CSSValueID, 3> paint_type_list;
  CSSIdentifierValue* fill = nullptr;
  CSSIdentifierValue* stroke = nullptr;
  CSSIdentifierValue* markers = nullptr;
  do {
    CSSValueID id = stream.Peek().Id();
    if (id == CSSValueID::kFill && !fill) {
      fill = css_parsing_utils::ConsumeIdent(stream);
    } else if (id == CSSValueID::kStroke && !stroke) {
      stroke = css_parsing_utils::ConsumeIdent(stream);
    } else if (id == CSSValueID::kMarkers && !markers) {
      markers = css_parsing_utils::ConsumeIdent(stream);
    } else {
      break;
    }
    paint_type_list.push_back(id);
  } while (!stream.AtEnd());

  if (paint_type_list.empty()) {
    return nullptr;
  }

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
      NOTREACHED_IN_MIGRATION();
  }

  return paint_order_list;
}

const CSSValue* PaintOrder::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const EPaintOrder paint_order = style.PaintOrder();
  if (paint_order == kPaintOrderNormal) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  const unsigned canonical_length =
      PaintOrderArray::CanonicalLength(paint_order);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  const PaintOrderArray paint_order_array(paint_order);
  for (unsigned i = 0; i < canonical_length; ++i) {
    list->Append(*CSSIdentifierValue::Create(paint_order_array[i]));
  }
  return list;
}

const CSSValue* Perspective::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& localContext) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  CSSPrimitiveValue* parsed_value = css_parsing_utils::ConsumeLength(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  bool use_legacy_parsing = localContext.UseAliasParsing();
  if (!parsed_value && use_legacy_parsing) {
    double perspective;
    if (!css_parsing_utils::ConsumeNumberRaw(stream, context, perspective) ||
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (!style.HasPerspective()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return ZoomAdjustedPixelValue(style.Perspective(), style);
}

const CSSValue* PerspectiveOrigin::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumePosition(stream, context,
                         css_parsing_utils::UnitlessQuirk::kForbid,
                         std::optional<WebFeature>());
}

bool PerspectiveOrigin::IsLayoutDependent(const ComputedStyle* style,
                                          LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* PerspectiveOrigin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (layout_object) {
    PhysicalRect box;
    if (layout_object->IsBox()) {
      box = To<LayoutBox>(layout_object)->PhysicalBorderBoxRect();
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.PointerEvents());
}

const CSSValue* Position::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.PositionInternal());
}

const CSSValue* PositionTryFallbacks::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionTryFallbacks(stream, context);
}

const CSSValue* PositionTryFallbacks::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (const blink::PositionTryFallbacks* fallbacks =
          style.GetPositionTryFallbacks()) {
    return ComputedStyleUtils::ValueForPositionTryFallbacks(*fallbacks);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

void PositionTryFallbacks::ApplyValue(StyleResolverState& state,
                                      const CSSValue& value,
                                      ValueMode) const {
  if (value.IsIdentifierValue()) {
    DCHECK(To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kNone);
    // Just represent as nullptr.
    return;
  }
  HeapVector<PositionTryFallback> fallbacks;
  for (const auto& fallback : To<CSSValueList>(value)) {
    // position-area( <position-area> )
    if (const auto* function = DynamicTo<CSSFunctionValue>(fallback.Get())) {
      CHECK(!RuntimeEnabledFeatures::CSSPositionAreaValueEnabled());
      CHECK_EQ(1u, function->length());
      blink::PositionArea position_area =
          StyleBuilderConverter::ConvertPositionArea(state, function->First());
      fallbacks.push_back(PositionTryFallback(position_area));
      continue;
    }
    // <'position-area'>
    if (IsA<CSSValuePair>(fallback.Get()) ||
        IsA<CSSIdentifierValue>(fallback.Get())) {
      blink::PositionArea position_area =
          StyleBuilderConverter::ConvertPositionArea(state, *fallback.Get());
      fallbacks.push_back(PositionTryFallback(position_area));
      continue;
    }
    // [<dashed-ident> || <try-tactic>]
    const ScopedCSSName* scoped_name = nullptr;
    TryTacticList tactic_list = {TryTactic::kNone};
    wtf_size_t tactic_index = 0;
    for (const auto& name_or_tactic : To<CSSValueList>(*fallback)) {
      if (const auto* name = DynamicTo<CSSCustomIdentValue>(*name_or_tactic)) {
        scoped_name = StyleBuilderConverter::ConvertCustomIdent(state, *name);
        continue;
      }
      CHECK_LT(tactic_index, tactic_list.size());
      tactic_list[tactic_index++] =
          To<CSSIdentifierValue>(*name_or_tactic).ConvertTo<TryTactic>();
    }
    fallbacks.push_back(PositionTryFallback(scoped_name, tactic_list));
  }
  DCHECK(!fallbacks.empty());
  state.StyleBuilder().SetPositionTryFallbacks(
      MakeGarbageCollected<blink::PositionTryFallbacks>(fallbacks));
}

const CSSValue* PositionTryOrder::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNormal);
}

const CSSValue* PositionTryOrder::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.PositionTryOrder());
}

const CSSValue* Quotes::ParseSingleValue(CSSParserTokenStream& stream,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  if (auto* value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kAuto, CSSValueID::kNone>(
              stream)) {
    return value;
  }
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  while (!stream.AtEnd()) {
    CSSStringValue* parsed_value = css_parsing_utils::ConsumeString(stream);
    if (!parsed_value) {
      // NOTE: Technically, if we consumed an odd number of strings,
      // we should have returned success here but un-consumed
      // the last string (since we should allow any arbitrary junk).
      // However, in practice, the only thing we need to care about
      // is !important, since we're not part of a shorthand,
      // so we let it slip.
      break;
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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

const CSSValue* R::ParseSingleValue(CSSParserTokenStream& stream,
                                    const CSSParserContext& context,
                                    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* R::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.R(), style);
}

const CSSValue* ReadingFlow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool,
    CSSValuePhase) const {
  return CSSIdentifierValue::Create(style.ReadingFlow());
}

const CSSValue* Resize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.Resize());
}

void Resize::ApplyValue(StyleResolverState& state,
                        const CSSValue& value,
                        ValueMode) const {
  const CSSIdentifierValue& identifier_value = To<CSSIdentifierValue>(value);

  EResize r = EResize::kNone;
  if (identifier_value.GetValueID() == CSSValueID::kAuto ||
      identifier_value.GetValueID() == CSSValueID::kInternalTextareaAuto) {
    if (Settings* settings = state.GetDocument().GetSettings()) {
      r = settings->GetTextAreasAreResizable() ? EResize::kBoth
                                               : EResize::kNone;
    }
    if (identifier_value.GetValueID() == CSSValueID::kAuto) {
      UseCounter::Count(state.GetDocument(), WebFeature::kCSSResizeAuto);
    }
  } else {
    r = identifier_value.ConvertTo<EResize>();
  }
  state.StyleBuilder().SetResize(r);
}

const CSSValue* Right::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context,
      css_parsing_utils::UnitlessUnlessShorthand(local_context),
      kCSSAnchorQueryTypesAll);
}

bool Right::IsLayoutDependent(const ComputedStyle* style,
                              LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Right::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForPositionOffset(style, *this,
                                                    layout_object);
}

const CSSValue* Rotate::ParseSingleValue(CSSParserTokenStream& stream,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  CSSValue* rotation = css_parsing_utils::ConsumeAngle(
      stream, context, std::optional<WebFeature>());

  CSSValue* axis = css_parsing_utils::ConsumeAxis(stream, context);
  if (axis) {
    if (To<cssvalue::CSSAxisValue>(axis)->AxisName() != CSSValueID::kZ) {
      // The z axis should be normalized away and stored as a 2D rotate.
      list->Append(*axis);
    }
  } else if (!rotation) {
    return nullptr;
  }

  if (!rotation) {
    rotation = css_parsing_utils::ConsumeAngle(stream, context,
                                               std::optional<WebFeature>());
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (!style.Rotate()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (style.Rotate()->X() != 0 || style.Rotate()->Y() != 0 ||
      style.Rotate()->Z() != 1) {
    const cssvalue::CSSAxisValue* axis =
        MakeGarbageCollected<cssvalue::CSSAxisValue>(
            MakeGarbageCollected<CSSNumericLiteralValue>(
                style.Rotate()->X(), CSSPrimitiveValue::UnitType::kNumber),
            MakeGarbageCollected<CSSNumericLiteralValue>(
                style.Rotate()->Y(), CSSPrimitiveValue::UnitType::kNumber),
            MakeGarbageCollected<CSSNumericLiteralValue>(
                style.Rotate()->Z(), CSSPrimitiveValue::UnitType::kNumber));
    list->Append(*axis);
  }
  list->Append(*CSSNumericLiteralValue::Create(
      style.Rotate()->Angle(), CSSPrimitiveValue::UnitType::kDegrees));
  return list;
}

const CSSValue* RowGap::ParseSingleValue(CSSParserTokenStream& stream,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGapLength(stream, context);
}

const CSSValue* RowGap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForGapLength(style.RowGap(), style);
}

const CSSValue* Rx::ParseSingleValue(CSSParserTokenStream& stream,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* Rx::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Rx(), style);
}

const CSSValue* Ry::ParseSingleValue(CSSParserTokenStream& stream,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* Ry::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Ry(), style);
}

const CSSValue* Scale::ParseSingleValue(CSSParserTokenStream& stream,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSPrimitiveValue* x_scale = css_parsing_utils::ConsumeNumberOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!x_scale) {
    return nullptr;
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*x_scale);

  CSSPrimitiveValue* y_scale = css_parsing_utils::ConsumeNumberOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
  if (y_scale) {
    CSSPrimitiveValue* z_scale = css_parsing_utils::ConsumeNumberOrPercent(
        stream, context, CSSPrimitiveValue::ValueRange::kAll);
    if (z_scale &&
        (!z_scale->IsNumericLiteralValue() ||
         To<CSSNumericLiteralValue>(z_scale)->DoubleValue() != 1.0)) {
      list->Append(*y_scale);
      list->Append(*z_scale);
    } else if (!x_scale->IsNumericLiteralValue() ||
               !y_scale->IsNumericLiteralValue() ||
               To<CSSNumericLiteralValue>(x_scale)->DoubleValue() !=
                   To<CSSNumericLiteralValue>(y_scale)->DoubleValue()) {
      list->Append(*y_scale);
    }
  }

  return list;
}

const CSSValue* Scale::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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

const CSSValue* ScrollMarkerGroup::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ScrollMarkerGroup());
}

// https://www.w3.org/TR/css-scrollbars/
// auto | <color>{2}
const CSSValue* ScrollbarColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::ScrollbarColorEnabled());

  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSValue* thumb_color = css_parsing_utils::ConsumeColor(stream, context);
  if (!thumb_color) {
    return nullptr;
  }

  CSSValue* track_color = css_parsing_utils::ConsumeColor(stream, context);
  if (!track_color) {
    return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*thumb_color);
  list->Append(*track_color);
  return list;
}

const CSSValue* ScrollbarColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const StyleScrollbarColor* scrollbar_color = style.UsedScrollbarColor();
  if (!scrollbar_color) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ComputedStyleUtils::CurrentColorOrValidColor(
      style, scrollbar_color->GetThumbColor(), value_phase));
  list->Append(*ComputedStyleUtils::CurrentColorOrValidColor(
      style, scrollbar_color->GetTrackColor(), value_phase));
  return list;
}

// https://www.w3.org/TR/css-overflow-4
// auto | stable && both-edges?
const CSSValue* ScrollbarGutter::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (auto* value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kAuto>(stream)) {
    return value;
  }

  CSSIdentifierValue* stable = nullptr;
  CSSIdentifierValue* both_edges = nullptr;

  while (!stream.AtEnd()) {
    if (!stable) {
      if ((stable =
               css_parsing_utils::ConsumeIdent<CSSValueID::kStable>(stream))) {
        continue;
      }
    }
    CSSValueID id = stream.Peek().Id();
    if (id == CSSValueID::kBothEdges && !both_edges) {
      both_edges = css_parsing_utils::ConsumeIdent(stream);
    } else {
      // Something that didn't parse, or end-of-stream, or duplicate both-edges.
      // End-of-stream is success; the caller will clean up for us in the
      // failure case (since we didn't consume the erroneous token).
      break;
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.UsedScrollbarWidth());
}

const CSSValue* ScrollBehavior::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetScrollBehavior());
}

const CSSValue* ScrollMarginBlockEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginBlockStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginBottom::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginBottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginBottom(), style);
}

const CSSValue* ScrollMarginInlineEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginInlineStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginLeft::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginLeft::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginLeft(), style);
}

const CSSValue* ScrollMarginRight::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginRight(), style);
}

const CSSValue* ScrollMarginTop::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(stream, context, CSSPrimitiveValue::ValueRange::kAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginTop(), style);
}

const CSSValue* ScrollPaddingBlockEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(stream, context);
}

const CSSValue* ScrollPaddingBlockStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(stream, context);
}

const CSSValue* ScrollPaddingBottom::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(stream, context);
}

const CSSValue* ScrollPaddingBottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.ScrollPaddingBottom(), style);
}

const CSSValue* ScrollPaddingInlineEnd::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(stream, context);
}

const CSSValue* ScrollPaddingInlineStart::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(stream, context);
}

const CSSValue* ScrollPaddingLeft::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(stream, context);
}

const CSSValue* ScrollPaddingLeft::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.ScrollPaddingLeft(), style);
}

const CSSValue* ScrollPaddingRight::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(stream, context);
}

const CSSValue* ScrollPaddingRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.ScrollPaddingRight(), style);
}

const CSSValue* ScrollPaddingTop::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollPadding(stream, context);
}

const CSSValue* ScrollPaddingTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.ScrollPaddingTop(), style);
}

const CSSValue* ScrollSnapAlign::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValue* block_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kNone, CSSValueID::kStart,
                                      CSSValueID::kEnd, CSSValueID::kCenter>(
          stream);
  if (!block_value) {
    return nullptr;
  }

  CSSValue* inline_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kNone, CSSValueID::kStart,
                                      CSSValueID::kEnd, CSSValueID::kCenter>(
          stream);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForScrollSnapAlign(style.GetScrollSnapAlign(),
                                                     style);
}

const CSSValue* ScrollSnapStop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ScrollSnapStop());
}

const CSSValue* ScrollSnapType::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID axis_id = stream.Peek().Id();
  if (axis_id != CSSValueID::kNone && axis_id != CSSValueID::kX &&
      axis_id != CSSValueID::kY && axis_id != CSSValueID::kBlock &&
      axis_id != CSSValueID::kInline && axis_id != CSSValueID::kBoth) {
    return nullptr;
  }
  CSSValue* axis_value = css_parsing_utils::ConsumeIdent(stream);
  if (axis_id == CSSValueID::kNone) {
    return axis_value;
  }

  CSSValueID strictness_id = stream.Peek().Id();
  if (strictness_id != CSSValueID::kProximity &&
      strictness_id != CSSValueID::kMandatory) {
    return axis_value;
  }
  CSSValue* strictness_value = css_parsing_utils::ConsumeIdent(stream);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForScrollSnapType(style.GetScrollSnapType(),
                                                    style);
}

const CSSValue* ScrollStartBlock::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollStart(stream, context);
}

const CSSValue* ScrollStartInline::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollStart(stream, context);
}

const CSSValue* ScrollStartX::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollStart(stream, context);
}

const CSSValue* ScrollStartX::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForScrollStart(style, style.ScrollStartX());
}

const CSSValue* ScrollStartY::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeScrollStart(stream, context);
}

const CSSValue* ScrollStartY::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForScrollStart(style, style.ScrollStartY());
}

const CSSValue* ScrollStartTarget::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ScrollStartTarget());
}

const CSSValue* ScrollTimelineAxis::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  using css_parsing_utils::ConsumeCommaSeparatedList;
  using css_parsing_utils::ConsumeSingleTimelineAxis;
  return ConsumeCommaSeparatedList(ConsumeSingleTimelineAxis, stream);
}

const CSSValue* ScrollTimelineAxis::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  using css_parsing_utils::ConsumeCommaSeparatedList;
  using css_parsing_utils::ConsumeSingleTimelineName;
  return ConsumeCommaSeparatedList(ConsumeSingleTimelineName, stream, context);
}

const CSSValue* ScrollTimelineName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(stream, context);
}

const CSSValue* ShapeImageThreshold::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.ShapeImageThreshold(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* ShapeMargin::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* ShapeMargin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSValue::Create(style.ShapeMargin(), style.EffectiveZoom());
}

const CSSValue* ShapeOutside::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (CSSValue* image_value =
          css_parsing_utils::ConsumeImageOrNone(stream, context)) {
    return image_value;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  CSSValue* box_value = css_parsing_utils::ConsumeShapeBox(stream);
  CSSValue* shape_value = css_parsing_utils::ConsumeBasicShape(
      stream, context, css_parsing_utils::AllowPathValue::kForbid,
      css_parsing_utils::AllowBasicShapeRectValue::kForbid,
      css_parsing_utils::AllowBasicShapeXYWHValue::kForbid);
  if (shape_value) {
    list->Append(*shape_value);
    if (!box_value) {
      box_value = css_parsing_utils::ConsumeShapeBox(stream);
    }
  }
  if (box_value) {
    if (!shape_value || To<CSSIdentifierValue>(box_value)->GetValueID() !=
                            CSSValueID::kMarginBox) {
      list->Append(*box_value);
    }
  }
  if (!list->length()) {
    return nullptr;
  }
  return list;
}

const CSSValue* ShapeOutside::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForShape(style, allow_visited_style,
                                           style.ShapeOutside(), value_phase);
}

const CSSValue* ShapeRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ShapeRendering());
}

static CSSValue* ConsumePageSize(CSSParserTokenStream& stream) {
  return css_parsing_utils::ConsumeIdent<
      CSSValueID::kA3, CSSValueID::kA4, CSSValueID::kA5, CSSValueID::kB4,
      CSSValueID::kB5, CSSValueID::kJisB5, CSSValueID::kJisB4,
      CSSValueID::kLedger, CSSValueID::kLegal, CSSValueID::kLetter>(stream);
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
      NOTREACHED_IN_MIGRATION();
      return gfx::SizeF(0, 0);
  }
}

const CSSValue* Size::ParseSingleValue(CSSParserTokenStream& stream,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  CSSValueList* result = CSSValueList::CreateSpaceSeparated();

  if (stream.Peek().Id() == CSSValueID::kAuto) {
    result->Append(*css_parsing_utils::ConsumeIdent(stream));
    return result;
  }

  if (CSSValue* width = css_parsing_utils::ConsumeLength(
          stream, context, CSSPrimitiveValue::ValueRange::kNonNegative)) {
    CSSValue* height = css_parsing_utils::ConsumeLength(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    result->Append(*width);
    if (height) {
      result->Append(*height);
    }
    return result;
  }

  CSSValue* page_size = ConsumePageSize(stream);
  CSSValue* orientation =
      css_parsing_utils::ConsumeIdent<CSSValueID::kPortrait,
                                      CSSValueID::kLandscape>(stream);
  if (!page_size) {
    page_size = ConsumePageSize(stream);
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

void Size::ApplyValue(StyleResolverState& state,
                      const CSSValue& value,
                      ValueMode) const {
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.Speak());
}

const CSSValue* StopColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color StopColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  const StyleColor& stop_color = style.StopColor();
  if (style.ShouldForceColor(stop_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return style.ResolvedColor(stop_color, is_current_color);
}

const CSSValue* StopColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(style, style.StopColor(),
                                                      value_phase);
}

const CSSValue* StopOpacity::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(stream, context);
}

const CSSValue* StopOpacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.StopOpacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* Stroke::ParseSingleValue(CSSParserTokenStream& stream,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGPaint(stream, context);
}

const CSSValue* Stroke::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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

void Stroke::ApplyValue(StyleResolverState& state,
                        const CSSValue& value,
                        ValueMode) const {
  state.StyleBuilder().SetStrokePaint(StyleBuilderConverter::ConvertSVGPaint(
      state, value, false, PropertyID()));
}

const CSSValue* StrokeDasharray::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // Syntax: comma- or whitespace-separated list of <length-or-percent>
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  CSSValueList* dashes = CSSValueList::CreateCommaSeparated();
  bool need_next_value = true;
  for (;;) {
    CSSPrimitiveValue* dash = css_parsing_utils::ConsumeLengthOrPercent(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!dash) {
      if (need_next_value) {
        return nullptr;
      } else {
        break;
      }
    }
    dashes->Append(*dash);
    need_next_value =
        css_parsing_utils::ConsumeCommaIncludingWhitespace(stream);
  }
  return dashes;
}

const CSSValue* StrokeDasharray::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::StrokeDashArrayToCSSValueList(
      *style.StrokeDashArray(), style);
}

const CSSValue* StrokeDashoffset::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  return css_parsing_utils::ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll,
      css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* StrokeDashoffset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.StrokeDashOffset(), style);
}

const CSSValue* StrokeLinecap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.CapStyle());
}

const CSSValue* StrokeLinejoin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.JoinStyle());
}

const CSSValue* StrokeMiterlimit::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* StrokeMiterlimit::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.StrokeMiterLimit(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* StrokeOpacity::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(stream, context);
}

const CSSValue* StrokeOpacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.StrokeOpacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* StrokeWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  return css_parsing_utils::ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative,
      css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* StrokeWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  // We store the unzoomed stroke-width value using ConvertUnzoomedLength().
  // Don't apply zoom here either.
  return CSSValue::Create(style.StrokeWidth().length(), 1);
}

const CSSValue* ContentVisibility::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.ContentVisibility());
}

const CSSValue* ContentVisibility::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIdent<
      CSSValueID::kVisible, CSSValueID::kAuto, CSSValueID::kHidden>(stream);
}

const CSSValue* TabSize::ParseSingleValue(CSSParserTokenStream& stream,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSPrimitiveValue* parsed_value = css_parsing_utils::ConsumeNumber(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  if (parsed_value) {
    return parsed_value;
  }
  return css_parsing_utils::ConsumeLength(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* TabSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(
      style.GetTabSize().GetPixelSize(1.0),
      style.GetTabSize().IsSpaces() ? CSSPrimitiveValue::UnitType::kNumber
                                    : CSSPrimitiveValue::UnitType::kPixels);
}

const CSSValue* TableLayout::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.TableLayout());
}

const CSSValue* TextAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetTextAlign());
}

void TextAlign::ApplyValue(StyleResolverState& state,
                           const CSSValue& value,
                           ValueMode) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  const auto* ident_value = DynamicTo<CSSIdentifierValue>(value);
  if (ident_value &&
      ident_value->GetValueID() != CSSValueID::kWebkitMatchParent) {
    // Special case for th elements - UA stylesheet text-align does not apply
    // if parent's computed value for text-align is not its initial value
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.TextAlignLast());
}

const CSSValue* TextAnchor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.TextAnchor());
}

const CSSValue* TextAutospace::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.TextAutospace());
}

const CSSValue* TextBoxEdge::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const blink::TextBoxEdge& text_box_edge = style.GetTextBoxEdge();
  if (text_box_edge.Under() == text_box_edge.Over()) {
    return CSSIdentifierValue::Create(text_box_edge.Over());
  }
  if (text_box_edge.Under() == ::blink::TextBoxEdge::Type::kText) {
    using enum ::blink::TextBoxEdge::Type;
    switch (text_box_edge.Over()) {
      case kCap:
      case kEx:
        return CSSIdentifierValue::Create(text_box_edge.Over());
      case kAlphabetic:
        break;
      case kAuto:
      case kText:
        NOTREACHED();
    }
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(text_box_edge.Over()));
  list->Append(*CSSIdentifierValue::Create(text_box_edge.Under()));
  return list;
}

const CSSValue* TextBoxEdge::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeTextBoxEdge(stream);
}

const CSSValue* TextBoxTrim::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.TextBoxTrim());
}

const CSSValue* TextCombineUpright::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.TextCombine());
}

const CSSValue* TextDecorationColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color TextDecorationColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& decoration_color =
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TextDecorationColor(), value_phase);
}

const CSSValue* TextDecorationLine::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeTextDecorationLine(stream);
}

const CSSValue* TextDecorationLine::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::RenderTextDecorationFlagsToCSSValue(
      style.GetTextDecorationLine());
}

const CSSValue* TextDecorationSkipInk::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForTextDecorationSkipInk(
      style.TextDecorationSkipInk());
}

const CSSValue* TextDecorationStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForTextDecorationStyle(
      style.TextDecorationStyle());
}

const CSSValue* TextDecorationThickness::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (auto* ident =
          css_parsing_utils::ConsumeIdent<CSSValueID::kFromFont,
                                          CSSValueID::kAuto>(stream)) {
    return ident;
  }
  return css_parsing_utils::ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* TextDecorationThickness::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // [ <length> | <percentage> ]
  CSSValue* length_percentage = css_parsing_utils::ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll,
      css_parsing_utils::UnitlessQuirk::kAllow);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.TextIndent(), style));
  return list;
}

void TextIndent::ApplyValue(StyleResolverState& state,
                            const CSSValue& value,
                            ValueMode) const {
  Length length_or_percentage_value;

  for (auto& list_value : To<CSSValueList>(value)) {
    if (auto* list_primitive_value =
            DynamicTo<CSSPrimitiveValue>(*list_value)) {
      length_or_percentage_value = list_primitive_value->ConvertToLength(
          state.CssToLengthConversionData());
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  state.StyleBuilder().SetTextIndent(length_or_percentage_value);
}

const CSSValue* TextOrientation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
                                 const CSSValue& value,
                                 ValueMode) const {
  state.SetTextOrientation(
      To<CSSIdentifierValue>(value).ConvertTo<ETextOrientation>());
}

const CSSValue* TextOverflow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.TextOverflow() != ETextOverflow::kClip) {
    return CSSIdentifierValue::Create(CSSValueID::kEllipsis);
  }
  return CSSIdentifierValue::Create(CSSValueID::kClip);
}

const CSSValue* TextRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetFontDescription().TextRendering());
}

const CSSValue* TextShadow::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeShadow(
      stream, context, css_parsing_utils::AllowInsetAndSpread::kForbid);
}

const CSSValue* TextShadow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForShadowList(style.TextShadow(), style,
                                                false, value_phase);
}

const CSSValue* TextSizeAdjust::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumePercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* TextSizeAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.GetTextSizeAdjust().IsAuto()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return CSSNumericLiteralValue::Create(
      style.GetTextSizeAdjust().Multiplier() * 100,
      CSSPrimitiveValue::UnitType::kPercentage);
}

const CSSValue* TextSpacingTrim::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(
      style.GetFontDescription().GetTextSpacingTrim());
}

const CSSValue* TextTransform::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.TextTransform());
}

// https://drafts.csswg.org/css-text-decor-4/#text-underline-position-property
// auto | [ from-font | under ] || [ left | right ] - default: auto
const CSSValue* TextUnderlinePosition::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSIdentifierValue* from_font_or_under_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kFromFont,
                                      CSSValueID::kUnder>(stream);
  CSSIdentifierValue* left_or_right_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kLeft, CSSValueID::kRight>(
          stream);
  if (left_or_right_value && !from_font_or_under_value) {
    from_font_or_under_value =
        css_parsing_utils::ConsumeIdent<CSSValueID::kFromFont,
                                        CSSValueID::kUnder>(stream);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  auto text_underline_position = style.GetTextUnderlinePosition();
  if (text_underline_position == blink::TextUnderlinePosition::kAuto) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  if (text_underline_position == blink::TextUnderlinePosition::kFromFont) {
    return CSSIdentifierValue::Create(CSSValueID::kFromFont);
  }
  if (text_underline_position == blink::TextUnderlinePosition::kUnder) {
    return CSSIdentifierValue::Create(CSSValueID::kUnder);
  }
  if (text_underline_position == blink::TextUnderlinePosition::kLeft) {
    return CSSIdentifierValue::Create(CSSValueID::kLeft);
  }
  if (text_underline_position == blink::TextUnderlinePosition::kRight) {
    return CSSIdentifierValue::Create(CSSValueID::kRight);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (EnumHasFlags(text_underline_position,
                   blink::TextUnderlinePosition::kFromFont)) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kFromFont));
  } else {
    DCHECK(EnumHasFlags(text_underline_position,
                        blink::TextUnderlinePosition::kUnder));
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnder));
  }
  if (EnumHasFlags(text_underline_position,
                   blink::TextUnderlinePosition::kLeft)) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kLeft));
  }
  if (EnumHasFlags(text_underline_position,
                   blink::TextUnderlinePosition::kRight)) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kRight));
  }
  DCHECK_EQ(list->length(), 2U);
  return list;
}

const CSSValue* TextUnderlineOffset::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* TextUnderlineOffset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.TextUnderlineOffset(), style);
}

const CSSValue* Top::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      stream, context,
      css_parsing_utils::UnitlessUnlessShorthand(local_context),
      kCSSAnchorQueryTypesAll);
}

bool Top::IsLayoutDependent(const ComputedStyle* style,
                            LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Top::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForPositionOffset(style, *this,
                                                    layout_object);
}

namespace {

static bool ConsumePan(CSSParserTokenStream& stream,
                       CSSValue*& pan_x,
                       CSSValue*& pan_y,
                       CSSValue*& pinch_zoom) {
  CSSValueID id = stream.Peek().Id();
  if ((id == CSSValueID::kPanX || id == CSSValueID::kPanRight ||
       id == CSSValueID::kPanLeft) &&
      !pan_x) {
    pan_x = css_parsing_utils::ConsumeIdent(stream);
  } else if ((id == CSSValueID::kPanY || id == CSSValueID::kPanDown ||
              id == CSSValueID::kPanUp) &&
             !pan_y) {
    pan_y = css_parsing_utils::ConsumeIdent(stream);
  } else if (id == CSSValueID::kPinchZoom && !pinch_zoom) {
    pinch_zoom = css_parsing_utils::ConsumeIdent(stream);
  } else {
    return false;
  }
  return true;
}

}  // namespace

const CSSValue* TouchAction::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kAuto || id == CSSValueID::kNone ||
      id == CSSValueID::kManipulation) {
    list->Append(*css_parsing_utils::ConsumeIdent(stream));
    return list;
  }

  CSSValue* pan_x = nullptr;
  CSSValue* pan_y = nullptr;
  CSSValue* pinch_zoom = nullptr;
  if (!ConsumePan(stream, pan_x, pan_y, pinch_zoom)) {
    return nullptr;
  }
  ConsumePan(stream, pan_x, pan_y, pinch_zoom);  // May fail.
  ConsumePan(stream, pan_x, pan_y, pinch_zoom);  // May fail.

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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::TouchActionFlagsToCSSValue(style.GetTouchAction());
}

const CSSValue* TransformBox::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.TransformBox());
}

const CSSValue* Transform::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeTransformList(stream, context,
                                                 local_context);
}

bool Transform::IsLayoutDependent(const ComputedStyle* style,
                                  LayoutObject* layout_object) const {
  return layout_object &&
         (layout_object->IsBox() || layout_object->IsSVGChild());
}

const CSSValue* Transform::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (value_phase == CSSValuePhase::kComputedValue) {
    return ComputedStyleUtils::ComputedTransformList(style, layout_object);
  }
  return ComputedStyleUtils::ResolvedTransform(layout_object, style);
}

const CSSValue* TransformOrigin::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValue* result_x = nullptr;
  CSSValue* result_y = nullptr;
  if (css_parsing_utils::ConsumeOneOrTwoValuedPosition(
          stream, context, css_parsing_utils::UnitlessQuirk::kForbid, result_x,
          result_y)) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    list->Append(*result_x);
    list->Append(*result_y);
    CSSValue* result_z = css_parsing_utils::ConsumeLength(
        stream, context, CSSPrimitiveValue::ValueRange::kAll);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (layout_object) {
    gfx::RectF reference_box =
        ComputedStyleUtils::ReferenceBoxForTransform(*layout_object);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(
      (style.TransformStyle3D() == ETransformStyle3D::kPreserve3d)
          ? CSSValueID::kPreserve3d
          : CSSValueID::kFlat);
}

const CSSValue* TransitionDelay::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      static_cast<CSSPrimitiveValue* (*)(CSSParserTokenStream&,
                                         const CSSParserContext&,
                                         CSSPrimitiveValue::ValueRange)>(
          css_parsing_utils::ConsumeTime),
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* TransitionDelay::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationDelayList(style.Transitions());
}

const CSSValue* TransitionDelay::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (ComputedStyleUtils::ValueForAnimationDelay(
                          CSSTimingData::InitialDelayStart())));
  return value;
}

const CSSValue* TransitionDuration::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      static_cast<CSSPrimitiveValue* (*)(CSSParserTokenStream&,
                                         const CSSParserContext&,
                                         CSSPrimitiveValue::ValueRange)>(
          css_parsing_utils::ConsumeTime),
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* TransitionDuration::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationDurationList(style.Transitions());
}

const CSSValue* TransitionDuration::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (CSSNumericLiteralValue::Create(
                          CSSTransitionData::InitialDuration().value(),
                          CSSPrimitiveValue::UnitType::kSeconds)));
  return value;
}

const CSSValue* TransitionProperty::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueList* list = css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeTransitionProperty, stream, context);
  if (!list || !css_parsing_utils::IsValidPropertyList(*list)) {
    return nullptr;
  }
  return list;
}

const CSSValue* TransitionProperty::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForTransitionProperty(style.Transitions());
}

const CSSValue* TransitionProperty::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kAll);
}

namespace {
CSSIdentifierValue* ConsumeIdentNoTemplate(CSSParserTokenStream& stream,
                                           const CSSParserContext&) {
  return css_parsing_utils::ConsumeIdent(stream);
}
}  // namespace

const CSSValue* TransitionBehavior::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueList* list = css_parsing_utils::ConsumeCommaSeparatedList(
      ConsumeIdentNoTemplate, stream, context);
  if (!list || !css_parsing_utils::IsValidTransitionBehaviorList(*list)) {
    return nullptr;
  }
  return list;
}

const CSSValue* TransitionBehavior::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForTransitionBehavior(style.Transitions());
}

const CSSValue* TransitionBehavior::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNormal);
}

const CSSValue* TransitionTimingFunction::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeAnimationTimingFunction, stream, context);
}

const CSSValue* TransitionTimingFunction::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForAnimationTimingFunctionList(
      style.Transitions());
}

const CSSValue* TransitionTimingFunction::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kEase);
}

const CSSValue* Translate::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSValue* translate_x = css_parsing_utils::ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
  if (!translate_x) {
    return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*translate_x);
  CSSPrimitiveValue* translate_y = css_parsing_utils::ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
  if (translate_y) {
    CSSPrimitiveValue* translate_z = css_parsing_utils::ConsumeLength(
        stream, context, CSSPrimitiveValue::ValueRange::kAll);

    if (translate_z &&
        translate_z->IsZero() == CSSPrimitiveValue::BoolStatus::kTrue) {
      translate_z = nullptr;
    }
    if (translate_y->IsZero() == CSSPrimitiveValue::BoolStatus::kTrue &&
        !translate_y->HasPercentage() && !translate_z) {
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (!style.Translate()) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  const Length& x = style.Translate()->X();
  const Length& y = style.Translate()->Y();
  double z = style.Translate()->Z();

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(x, style));

  if (!y.IsZero() || y.HasPercent() || z != 0) {
    list->Append(
        *ComputedStyleUtils::ZoomAdjustedPixelValueForLength(y, style));
  }

  if (z != 0) {
    list->Append(*ZoomAdjustedPixelValue(z, style));
  }

  return list;
}

const CSSValue* UnicodeBidi::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetUnicodeBidi());
}

const CSSValue* UserSelect::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.UserSelect());
}

const CSSValue* VectorEffect::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.VectorEffect());
}

const CSSValue* VerticalAlign::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValue* parsed_value = css_parsing_utils::ConsumeIdentRange(
      stream, CSSValueID::kBaseline, CSSValueID::kWebkitBaselineMiddle);
  if (!parsed_value) {
    parsed_value = css_parsing_utils::ConsumeLengthOrPercent(
        stream, context, CSSPrimitiveValue::ValueRange::kAll,
        css_parsing_utils::UnitlessQuirk::kAllow);
  }
  return parsed_value;
}

const CSSValue* VerticalAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
  NOTREACHED_IN_MIGRATION();
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
                               const CSSValue& value,
                               ValueMode) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    builder.SetVerticalAlign(identifier_value->ConvertTo<EVerticalAlign>());
  } else {
    builder.SetVerticalAlignLength(To<CSSPrimitiveValue>(value).ConvertToLength(
        state.CssToLengthConversionData()));
  }
}

const CSSValue* ViewTimelineAxis::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  using css_parsing_utils::ConsumeCommaSeparatedList;
  using css_parsing_utils::ConsumeSingleTimelineAxis;
  return ConsumeCommaSeparatedList(ConsumeSingleTimelineAxis, stream);
}

const CSSValue* ViewTimelineAxis::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  using css_parsing_utils::ConsumeCommaSeparatedList;
  using css_parsing_utils::ConsumeSingleTimelineInset;
  return ConsumeCommaSeparatedList(ConsumeSingleTimelineInset, stream, context);
}

const CSSValue* ViewTimelineInset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const Vector<TimelineInset>& vector = style.ViewTimelineInset();
  if (vector.empty()) {
    return InitialValue();
  }
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const TimelineInset& inset : vector) {
    list->Append(*ComputedStyleUtils::ValueForTimelineInset(inset, style));
  }
  return list;
}

const CSSValue* ViewTimelineInset::InitialValue() const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  return list;
}

const CSSValue* ViewTimelineName::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  using css_parsing_utils::ConsumeCommaSeparatedList;
  using css_parsing_utils::ConsumeSingleTimelineName;
  return ConsumeCommaSeparatedList(ConsumeSingleTimelineName, stream, context);
}

const CSSValue* ViewTimelineName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.Visibility());
}

const CSSValue* AppRegion::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
                           const CSSValue& value,
                           ValueMode) const {
  const auto& identifier_value = To<CSSIdentifierValue>(value);
  state.StyleBuilder().SetDraggableRegionMode(
      identifier_value.GetValueID() == CSSValueID::kDrag
          ? EDraggableRegionMode::kDrag
          : EDraggableRegionMode::kNoDrag);
  state.GetDocument().SetHasDraggableRegions(true);
}

const CSSValue* Appearance::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  CSSValueID id = stream.Peek().Id();
  CSSPropertyID property = CSSPropertyID::kAppearance;
  if (local_context.UseAliasParsing()) {
    property = CSSPropertyID::kAliasWebkitAppearance;
  }
  if (CSSParserFastPaths::IsValidKeywordPropertyAndValue(property, id,
                                                         context.Mode())) {
    css_parsing_utils::CountKeywordOnlyPropertyUsage(property, context, id);
    return css_parsing_utils::ConsumeIdent(stream);
  }
  css_parsing_utils::WarnInvalidKeywordPropertyUsage(property, context, id);
  return nullptr;
}

const CSSValue* Appearance::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.Appearance());
}

const CSSValue* WebkitBorderHorizontalSpacing::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue*
WebkitBorderHorizontalSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.HorizontalBorderSpacing(), style);
}

const CSSValue* WebkitBorderImage::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWebkitBorderImage(stream, context);
}

const CSSValue* WebkitBorderImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForNinePieceImage(
      style.BorderImage(), style, allow_visited_style, value_phase);
}

void WebkitBorderImage::ApplyValue(StyleResolverState& state,
                                   const CSSValue& value,
                                   ValueMode) const {
  NinePieceImage image;
  CSSToStyleMap::MapNinePieceImage(state, CSSPropertyID::kWebkitBorderImage,
                                   value, image);
  state.StyleBuilder().SetBorderImage(image);
}

const CSSValue* WebkitBorderVerticalSpacing::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

const CSSValue* WebkitBorderVerticalSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.VerticalBorderSpacing(), style);
}

const CSSValue* WebkitBoxAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BoxAlign());
}

const CSSValue* WebkitBoxDecorationBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return GetCSSPropertyBoxDecorationBreak().CSSValueFromComputedStyleInternal(
      style, layout_object, allow_visited_style, value_phase);
}

const CSSValue* WebkitBoxDirection::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BoxDirection());
}

const CSSValue* WebkitBoxFlex::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(stream, context,
                                          CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* WebkitBoxFlex::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.BoxFlex(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* WebkitBoxOrdinalGroup::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositiveInteger(stream, context);
}

const CSSValue* WebkitBoxOrdinalGroup::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.BoxOrdinalGroup(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* WebkitBoxOrient::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BoxOrient());
}

const CSSValue* WebkitBoxPack::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.BoxPack());
}

namespace {

CSSValue* ConsumeReflect(CSSParserTokenStream& stream,
                         const CSSParserContext& context) {
  CSSIdentifierValue* direction =
      css_parsing_utils::ConsumeIdent<CSSValueID::kAbove, CSSValueID::kBelow,
                                      CSSValueID::kLeft, CSSValueID::kRight>(
          stream);
  if (!direction) {
    return nullptr;
  }

  CSSPrimitiveValue* offset = ConsumeLengthOrPercent(
      stream, context, CSSPrimitiveValue::ValueRange::kAll,
      css_parsing_utils::UnitlessQuirk::kForbid);
  if (!offset) {
    // End of stream or parse error; in the latter case,
    // the caller will clean up since we're not at the end.
    offset =
        CSSNumericLiteralValue::Create(0, CSSPrimitiveValue::UnitType::kPixels);
    return MakeGarbageCollected<cssvalue::CSSReflectValue>(direction, offset,
                                                           /*mask=*/nullptr);
  }

  CSSValue* mask_or_null =
      css_parsing_utils::ConsumeWebkitBorderImage(stream, context);
  return MakeGarbageCollected<cssvalue::CSSReflectValue>(direction, offset,
                                                         mask_or_null);
}

}  // namespace

const CSSValue* WebkitBoxReflect::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeReflect(stream, context);
}

const CSSValue* WebkitBoxReflect::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForReflection(
      style.BoxReflect(), style, allow_visited_style, value_phase);
}

const CSSValue* InternalFontSizeDelta::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(
      stream, context, CSSPrimitiveValue::ValueRange::kAll,
      css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* WebkitFontSmoothing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetFontDescription().FontSmoothing());
}

const CSSValue* HyphenateCharacter::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeString(stream);
}

const CSSValue* HyphenateCharacter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.HyphenationString().IsNull()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return MakeGarbageCollected<CSSStringValue>(style.HyphenationString());
}

const CSSValue* WebkitLineBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetLineBreak());
}

const CSSValue* WebkitLineClamp::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  } else {
    // When specifying number of lines, don't allow 0 as a valid value.
    return css_parsing_utils::ConsumePositiveInteger(stream, context);
  }
}

const CSSValue* WebkitLineClamp::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.WebkitLineClamp() == 0) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  return CSSNumericLiteralValue::Create(style.WebkitLineClamp(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* WebkitLocale::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeString(stream);
}

const CSSValue* WebkitLocale::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.Locale().IsNull()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return MakeGarbageCollected<CSSStringValue>(style.Locale());
}

void WebkitLocale::ApplyValue(StyleResolverState& state,
                              const CSSValue& value,
                              ValueMode) const {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kAuto);
    state.GetFontBuilder().SetLocale(nullptr);
  } else {
    state.GetFontBuilder().SetLocale(
        LayoutLocale::Get(AtomicString(To<CSSStringValue>(value).Value())));
  }
}

const CSSValue* WebkitMaskBoxImageOutset::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageOutset(stream, context);
}

const CSSValue* WebkitMaskBoxImageOutset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForNinePieceImageQuad(
      style.MaskBoxImage().Outset(), style);
}

const CSSValue* WebkitMaskBoxImageRepeat::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageRepeat(stream);
}

const CSSValue* WebkitMaskBoxImageRepeat::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForNinePieceImageRepeat(style.MaskBoxImage());
}

const CSSValue* WebkitMaskBoxImageSlice::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageSlice(
      stream, context, css_parsing_utils::DefaultFill::kNoFill);
}

const CSSValue* WebkitMaskBoxImageSlice::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForNinePieceImageSlice(style.MaskBoxImage());
}

const CSSValue* WebkitMaskBoxImageSource::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeImageOrNone(stream, context);
}

const CSSValue* WebkitMaskBoxImageSource::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.MaskBoxImageSource()) {
    return style.MaskBoxImageSource()->ComputedCSSValue(
        style, allow_visited_style, value_phase);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

void WebkitMaskBoxImageSource::ApplyValue(StyleResolverState& state,
                                          const CSSValue& value,
                                          ValueMode) const {
  state.StyleBuilder().SetMaskBoxImageSource(
      state.GetStyleImage(CSSPropertyID::kWebkitMaskBoxImageSource, value));
}

const CSSValue* WebkitMaskBoxImageWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageWidth(stream, context);
}

const CSSValue* WebkitMaskBoxImageWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForNinePieceImageQuad(
      style.MaskBoxImage().BorderSlices(), style);
}

const CSSValue* MaskClip::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext& local_context) const {
  if (local_context.UseAliasParsing()) {
    return css_parsing_utils::ConsumeCommaSeparatedList(
        css_parsing_utils::ConsumePrefixedBackgroundBox, stream,
        css_parsing_utils::AllowTextValue::kAllow);
  }
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeCoordBoxOrNoClip, stream);
}

const CSSValue* MaskClip::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.MaskLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    EFillBox box = curr_layer->Clip();
    list->Append(*CSSIdentifierValue::Create(box));
  }
  return list;
}

const CSSValue* MaskClip::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kBorderBox);
}

const CSSValue* MaskComposite::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext& local_context) const {
  if (local_context.UseAliasParsing()) {
    return css_parsing_utils::ConsumeCommaSeparatedList(
        css_parsing_utils::ConsumePrefixedMaskComposite, stream);
  }
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeMaskComposite, stream);
}

const CSSValue* MaskComposite::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.MaskLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    list->Append(
        *CSSIdentifierValue::Create(curr_layer->CompositingOperator()));
  }
  return list;
}

const CSSValue* MaskComposite::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kAdd);
}

const CSSValue* MaskImage::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeImageOrNone, stream, context);
}

const CSSValue* MaskImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const FillLayer& fill_layer = style.MaskLayers();
  return ComputedStyleUtils::BackgroundImageOrMaskImage(
      style, allow_visited_style, fill_layer, value_phase);
}

const CSSValue* MaskMode::ParseSingleValue(CSSParserTokenStream& stream,
                                           const CSSParserContext&,
                                           const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeMaskMode, stream);
}

const CSSValue* MaskMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::MaskMode(&style.MaskLayers());
}

const CSSValue* MaskMode::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kMatchSource);
}

const CSSValue* MaskOrigin::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext& local_context) const {
  if (local_context.UseAliasParsing()) {
    return css_parsing_utils::ConsumeCommaSeparatedList(
        css_parsing_utils::ConsumePrefixedBackgroundBox, stream,
        css_parsing_utils::AllowTextValue::kForbid);
  }
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeCoordBox, stream);
}

const CSSValue* MaskOrigin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.MaskLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    EFillBox box = curr_layer->Origin();
    list->Append(*CSSIdentifierValue::Create(box));
  }
  return list;
}

const CSSValue* MaskOrigin::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kBorderBox);
}

const CSSValue* WebkitMaskPositionX::ParseSingleValue(
    CSSParserTokenStream& Stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumePositionLonghand<CSSValueID::kLeft,
                                                 CSSValueID::kRight>,
      Stream, context);
}

const CSSValue* WebkitMaskPositionX::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const FillLayer* curr_layer = &style.MaskLayers();
  return ComputedStyleUtils::BackgroundPositionXOrWebkitMaskPositionX(
      style, curr_layer);
}

const CSSValue* WebkitMaskPositionX::InitialValue() const {
  return CSSNumericLiteralValue::Create(
      0, CSSPrimitiveValue::UnitType::kPercentage);
}

const CSSValue* WebkitMaskPositionY::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumePositionLonghand<CSSValueID::kTop,
                                                 CSSValueID::kBottom>,
      stream, context);
}

const CSSValue* WebkitMaskPositionY::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const FillLayer* curr_layer = &style.MaskLayers();
  return ComputedStyleUtils::BackgroundPositionYOrWebkitMaskPositionY(
      style, curr_layer);
}

const CSSValue* WebkitMaskPositionY::InitialValue() const {
  return CSSNumericLiteralValue::Create(
      0, CSSPrimitiveValue::UnitType::kPercentage);
}

const CSSValue* MaskRepeat::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseRepeatStyle(stream);
}

const CSSValue* MaskRepeat::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::RepeatStyle(&style.MaskLayers());
}

const CSSValue* MaskRepeat::InitialValue() const {
  return MakeGarbageCollected<CSSRepeatStyleValue>(
      CSSIdentifierValue::Create(CSSValueID::kRepeat));
}

const CSSValue* MaskSize::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ParseMaskSize(stream, context, local_context,
                                          WebFeature::kNegativeMaskSize);
}

const CSSValue* MaskSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const FillLayer& fill_layer = style.MaskLayers();
  return ComputedStyleUtils::BackgroundImageOrMaskSize(style, fill_layer);
}

const CSSValue* MaskSize::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kAuto);
}

const CSSValue* WebkitPerspectiveOriginX::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueID::kLeft,
                                                    CSSValueID::kRight>(
      stream, context);
}

void WebkitPerspectiveOriginX::ApplyInherit(StyleResolverState& state) const {
  state.StyleBuilder().SetPerspectiveOriginX(
      state.ParentStyle()->PerspectiveOrigin().X());
}

const CSSValue* WebkitPerspectiveOriginY::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueID::kTop,
                                                    CSSValueID::kBottom>(
      stream, context);
}

void WebkitPerspectiveOriginY::ApplyInherit(StyleResolverState& state) const {
  state.StyleBuilder().SetPerspectiveOriginY(
      state.ParentStyle()->PerspectiveOrigin().Y());
}

const CSSValue* WebkitPrintColorAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.PrintColorAdjust());
}

const CSSValue* WebkitRtlOrdering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.RtlOrdering() == EOrder::kVisual
                                        ? CSSValueID::kVisual
                                        : CSSValueID::kLogical);
}

const CSSValue* RubyAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.RubyAlign());
}

const CSSValue* WebkitRubyPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  switch (style.GetRubyPosition()) {
    case blink::RubyPosition::kOver:
      return CSSIdentifierValue::Create(CSSValueID::kBefore);
    case blink::RubyPosition::kUnder:
      return CSSIdentifierValue::Create(CSSValueID::kAfter);
  }
  NOTREACHED_IN_MIGRATION();
  return CSSIdentifierValue::Create(CSSValueID::kOver);
}

const CSSValue* RubyPosition::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID value_id = stream.Peek().Id();
  if (css_parsing_utils::IdentMatches<CSSValueID::kOver, CSSValueID::kUnder>(
          value_id)) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  if (value_id == CSSValueID::kAlternate) {
    context.Count(WebFeature::kRubyPositionAlternate);
  } else if (value_id == CSSValueID::kInterCharacter) {
    context.Count(WebFeature::kRubyPositionInterCharacter);
  }
  return nullptr;
}

const CSSValue* RubyPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetRubyPosition());
}

const CSSValue* WebkitTapHighlightColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color WebkitTapHighlightColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  const StyleColor& highlight_color = style.TapHighlightColor();
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TapHighlightColor(), value_phase);
}

const CSSValue* WebkitTextCombine::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.TextCombine() == ETextCombine::kAll) {
    return CSSIdentifierValue::Create(CSSValueID::kHorizontal);
  }
  return CSSIdentifierValue::Create(style.TextCombine());
}

const CSSValue* WebkitTextDecorationsInEffect::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeTextDecorationLine(stream);
}

const CSSValue*
WebkitTextDecorationsInEffect::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::RenderTextDecorationFlagsToCSSValue(
      style.TextDecorationsInEffect());
}

const CSSValue* TextEmphasisColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color TextEmphasisColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& text_emphasis_color = style.TextEmphasisColor();
  if (style.ShouldForceColor(text_emphasis_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return text_emphasis_color.Resolve(style.GetCurrentColor(),
                                     style.UsedColorScheme(), is_current_color);
}

const CSSValue* TextEmphasisColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TextEmphasisColor(), value_phase);
}

// [ over | under ] && [ right | left ]?
// If [ right | left ] is omitted, it defaults to right.
const CSSValue* TextEmphasisPosition::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSIdentifierValue* values[2] = {
      css_parsing_utils::ConsumeIdent<CSSValueID::kOver, CSSValueID::kUnder,
                                      CSSValueID::kRight, CSSValueID::kLeft>(
          stream),
      nullptr};
  if (!values[0]) {
    return nullptr;
  }
  values[1] =
      css_parsing_utils::ConsumeIdent<CSSValueID::kOver, CSSValueID::kUnder,
                                      CSSValueID::kRight, CSSValueID::kLeft>(
          stream);
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
        NOTREACHED_IN_MIGRATION();
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = stream.Peek().Id();
  if (id == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  if (CSSValue* text_emphasis_style =
          css_parsing_utils::ConsumeString(stream)) {
    return text_emphasis_style;
  }

  CSSIdentifierValue* fill =
      css_parsing_utils::ConsumeIdent<CSSValueID::kFilled, CSSValueID::kOpen>(
          stream);
  CSSIdentifierValue* shape = css_parsing_utils::ConsumeIdent<
      CSSValueID::kDot, CSSValueID::kCircle, CSSValueID::kDoubleCircle,
      CSSValueID::kTriangle, CSSValueID::kSesame>(stream);
  if (!fill) {
    fill =
        css_parsing_utils::ConsumeIdent<CSSValueID::kFilled, CSSValueID::kOpen>(
            stream);
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  switch (style.GetTextEmphasisMark()) {
    case TextEmphasisMark::kNone:
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    case TextEmphasisMark::kCustom:
      return MakeGarbageCollected<CSSStringValue>(
          style.TextEmphasisCustomMark());
    case TextEmphasisMark::kAuto:
      NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
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
                                   const CSSValue& in_value,
                                   ValueMode) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();

  const CSSValue* value = &in_value;
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    if (list->length() == 1) {
      value = &list->First();
    }
  }

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

  const CSSIdentifierValue& identifier_value = *To<CSSIdentifierValue>(value);

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
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color WebkitTextFillColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& text_fill_color = style.TextFillColor();
  if (style.ShouldForceColor(text_fill_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return text_fill_color.Resolve(style.GetCurrentColor(),
                                 style.UsedColorScheme(), is_current_color);
}

const CSSValue* WebkitTextFillColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TextFillColor(), value_phase);
}

const CSSValue* WebkitTextOrientation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.GetTextOrientation() == ETextOrientation::kMixed) {
    return CSSIdentifierValue::Create(CSSValueID::kVerticalRight);
  }
  return CSSIdentifierValue::Create(style.GetTextOrientation());
}

const CSSValue* WebkitTextSecurity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.TextSecurity());
}

const CSSValue* WebkitTextStrokeColor::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(stream, context);
}

const blink::Color WebkitTextStrokeColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style,
    bool* is_current_color) const {
  DCHECK(!visited_link);
  const StyleColor& text_stroke_color = style.TextStrokeColor();
  if (style.ShouldForceColor(text_stroke_color)) {
    return style.GetInternalForcedCurrentColor(is_current_color);
  }
  return text_stroke_color.Resolve(style.GetCurrentColor(),
                                   style.UsedColorScheme(), is_current_color);
}

const CSSValue* WebkitTextStrokeColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TextStrokeColor(), value_phase);
}

const CSSValue* WebkitTextStrokeWidth::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLineWidth(
      stream, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* WebkitTextStrokeWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.TextStrokeWidth(), style);
}

const CSSValue* TimelineScope::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  using css_parsing_utils::ConsumeCommaSeparatedList;
  using css_parsing_utils::ConsumeCustomIdent;
  return ConsumeCommaSeparatedList<CSSCustomIdentValue*(
      CSSParserTokenStream&, const CSSParserContext&)>(ConsumeCustomIdent,
                                                       stream, context);
}

const CSSValue* TimelineScope::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (!style.TimelineScope()) {
    return MakeGarbageCollected<CSSIdentifierValue>(CSSValueID::kNone);
  }
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const Member<const ScopedCSSName>& name :
       style.TimelineScope()->GetNames()) {
    list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(name->GetName()));
  }
  return list;
}

const CSSValue* WebkitTransformOriginX::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueID::kLeft,
                                                    CSSValueID::kRight>(
      stream, context);
}

void WebkitTransformOriginX::ApplyInherit(StyleResolverState& state) const {
  state.StyleBuilder().SetTransformOriginX(
      state.ParentStyle()->GetTransformOrigin().X());
}

const CSSValue* Overlay::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.Overlay());
}

const CSSValue* WebkitTransformOriginY::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueID::kTop,
                                                    CSSValueID::kBottom>(
      stream, context);
}

void WebkitTransformOriginY::ApplyInherit(StyleResolverState& state) const {
  state.StyleBuilder().SetTransformOriginY(
      state.ParentStyle()->GetTransformOrigin().Y());
}

const CSSValue* WebkitTransformOriginZ::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(stream, context,
                                          CSSPrimitiveValue::ValueRange::kAll);
}

void WebkitTransformOriginZ::ApplyInherit(StyleResolverState& state) const {
  state.StyleBuilder().SetTransformOriginZ(
      state.ParentStyle()->GetTransformOrigin().Z());
}

const CSSValue* WebkitUserDrag::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.UserDrag());
}

const CSSValue* WebkitUserModify::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.UserModify());
}

const CSSValue* WebkitWritingMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetWritingMode());
}

// Longhands for `white-space`: `white-space-collapse` and `text-wrap`.
const CSSValue* WhiteSpaceCollapse::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetWhiteSpaceCollapse());
}

const CSSValue* TextWrapMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetTextWrapMode());
}

const CSSValue* TextWrapStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetTextWrapStyle());
}

const CSSValue* Widows::ParseSingleValue(CSSParserTokenStream& stream,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositiveInteger(stream, context);
}

const CSSValue* Widows::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.Widows(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* Width::ParseSingleValue(CSSParserTokenStream& stream,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(
      stream, context, css_parsing_utils::UnitlessQuirk::kAllow);
}

bool Width::IsLayoutDependent(const ComputedStyle* style,
                              LayoutObject* layout_object) const {
  return layout_object && (layout_object->IsBox() || layout_object->IsSVG());
}

const CSSValue* Width::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (value_phase == CSSValuePhase::kResolvedValue &&
      ComputedStyleUtils::WidthOrHeightShouldReturnUsedValue(layout_object)) {
    return ZoomAdjustedPixelValue(
        ComputedStyleUtils::UsedBoxSize(*layout_object).width(), style);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Width(),
                                                             style);
}

const CSSValue* WillChange::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  CSSValueList* values = CSSValueList::CreateCommaSeparated();
  // Every comma-separated list of identifiers is a valid will-change value,
  // unless the list includes an explicitly disallowed identifier.
  while (true) {
    if (stream.Peek().GetType() != kIdentToken) {
      return nullptr;
    }
    CSSPropertyID unresolved_property = UnresolvedCSSPropertyID(
        context.GetExecutionContext(), stream.Peek().Value());
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
      stream.ConsumeIncludingWhitespace();
    } else {
      switch (stream.Peek().Id()) {
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
          values->Append(*css_parsing_utils::ConsumeIdent(stream));
          break;
        default:
          stream.ConsumeIncludingWhitespace();
          break;
      }
    }

    if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
      break;
    }
  }

  return values;
}

const CSSValue* WillChange::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ValueForWillChange(
      style.WillChangeProperties(), style.WillChangeContents(),
      style.WillChangeScrollPosition());
}

void WillChange::ApplyInitial(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetWillChangeContents(false);
  builder.SetWillChangeScrollPosition(false);
  builder.SetWillChangeProperties(Vector<CSSPropertyID>());
}

void WillChange::ApplyInherit(StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  builder.SetWillChangeContents(state.ParentStyle()->WillChangeContents());
  builder.SetWillChangeScrollPosition(
      state.ParentStyle()->WillChangeScrollPosition());
  builder.SetWillChangeProperties(state.ParentStyle()->WillChangeProperties());
}

void WillChange::ApplyValue(StyleResolverState& state,
                            const CSSValue& value,
                            ValueMode) const {
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
        NOTREACHED_IN_MIGRATION();
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
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.WordBreak());
}

const CSSValue* WordSpacing::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseSpacing(stream, context);
}

const CSSValue* WordSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ZoomAdjustedPixelValue(style.WordSpacing(), style);
}

const CSSValue* WritingMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSIdentifierValue::Create(style.GetWritingMode());
}

void WritingMode::ApplyInitial(StyleResolverState& state) const {
  state.SetWritingMode(ComputedStyleInitialValues::InitialWritingMode());
}

void WritingMode::ApplyInherit(StyleResolverState& state) const {
  state.SetWritingMode(state.ParentStyle()->GetWritingMode());
}

void WritingMode::ApplyValue(StyleResolverState& state,
                             const CSSValue& value,
                             ValueMode) const {
  state.SetWritingMode(
      To<CSSIdentifierValue>(value).ConvertTo<blink::WritingMode>());
}

void TextSizeAdjust::ApplyInitial(StyleResolverState& state) const {
  state.SetTextSizeAdjust(ComputedStyleInitialValues::InitialTextSizeAdjust());
}

void TextSizeAdjust::ApplyInherit(StyleResolverState& state) const {
  state.SetTextSizeAdjust(state.ParentStyle()->GetTextSizeAdjust());
}

void TextSizeAdjust::ApplyValue(StyleResolverState& state,
                                const CSSValue& value,
                                ValueMode) const {
  state.SetTextSizeAdjust(
      StyleBuilderConverter::ConvertTextSizeAdjust(state, value));
}

const CSSValue* X::ParseSingleValue(CSSParserTokenStream& stream,
                                    const CSSParserContext& context,
                                    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* X::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.X(), style);
}

const CSSValue* Y::ParseSingleValue(CSSParserTokenStream& stream,
                                    const CSSParserContext& context,
                                    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      stream, context, CSSPrimitiveValue::ValueRange::kAll);
}

const CSSValue* Y::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Y(), style);
}

const CSSValue* ZIndex::ParseSingleValue(CSSParserTokenStream& stream,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  if (stream.Peek().Id() == CSSValueID::kAuto) {
    return css_parsing_utils::ConsumeIdent(stream);
  }
  return css_parsing_utils::ConsumeInteger(
      stream, context, /* minimum_value */ -std::numeric_limits<double>::max(),
      /* is_percentage_allowed */ false);
}

const CSSValue* ZIndex::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  if (style.HasAutoZIndex()) {
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return CSSNumericLiteralValue::Create(style.ZIndex(),
                                        CSSPrimitiveValue::UnitType::kInteger);
}

const CSSValue* Zoom::ParseSingleValue(CSSParserTokenStream& stream,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  const CSSParserToken token = stream.Peek();
  CSSValue* zoom = nullptr;
  if (token.GetType() == kIdentToken) {
    zoom = css_parsing_utils::ConsumeIdent<CSSValueID::kNormal>(stream);
  } else {
    zoom = css_parsing_utils::ConsumePercent(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!zoom) {
      zoom = css_parsing_utils::ConsumeNumber(
          stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    }
  }
  if (zoom) {
    if (!(token.Id() == CSSValueID::kNormal ||
          (token.GetType() == kNumberToken &&
           To<CSSPrimitiveValue>(zoom)->IsOne() ==
               CSSPrimitiveValue::BoolStatus::kTrue) ||
          (token.GetType() == kPercentageToken &&
           To<CSSPrimitiveValue>(zoom)->IsHundred() ==
               CSSPrimitiveValue::BoolStatus::kTrue))) {
      context.Count(WebFeature::kCSSZoomNotEqualToOne);
    }
  }
  return zoom;
}

const CSSValue* Zoom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return CSSNumericLiteralValue::Create(style.Zoom(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

void Zoom::ApplyInitial(StyleResolverState& state) const {
  state.SetZoom(ComputedStyleInitialValues::InitialZoom());
}

void Zoom::ApplyInherit(StyleResolverState& state) const {
  state.SetZoom(state.ParentStyle()->Zoom());
}

void Zoom::ApplyValue(StyleResolverState& state,
                      const CSSValue& value,
                      ValueMode) const {
  state.SetZoom(StyleBuilderConverter::ConvertZoom(state, value));
}

const CSSValue* InternalAlignContentBlock::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIdent<CSSValueID::kCenter,
                                         CSSValueID::kNormal>(stream);
}

const CSSValue* InternalEmptyLineHeight::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeIdent<CSSValueID::kFabricated,
                                         CSSValueID::kNone>(stream);
}

}  // namespace css_longhand
}  // namespace blink
