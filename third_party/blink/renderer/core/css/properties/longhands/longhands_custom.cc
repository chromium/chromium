// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/css_axis_value.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_cursor_image_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_font_feature_value.h"
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
#include "third_party/blink/renderer/core/css/css_reflect_value.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/font_variant_east_asian_parser.h"
#include "third_party/blink/renderer/core/css/parser/font_variant_ligatures_parser.h"
#include "third_party/blink/renderer/core/css/parser/font_variant_numeric_parser.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
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
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

// Implementations of methods in Longhand subclasses that aren't generated.

namespace blink {
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
    const SVGComputedStyle&,
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
  if (css_parsing_utils::IdentMatches<CSSValueID::kAuto>(range.Peek().Id()))
    return nullptr;
  return css_parsing_utils::ConsumeSelfPositionOverflowPosition(
      range, css_parsing_utils::IsSelfPositionKeyword);
}

const CSSValue* AlignItems::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForItemPositionWithOverflowAlignment(
      style.AlignSelf());
}
const CSSValue* AlignmentBaseline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.AlignmentBaseline());
}

const CSSValue* AnimationDelay::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeTime, range, context, kValueRangeAll);
}

const CSSValue* AnimationDelay::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDelay(style.Animations());
}

const CSSValue* AnimationDelay::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      const Persistent<CSSValue>, value,
      (CSSNumericLiteralValue::Create(CSSTimingData::InitialDelay(),
                                      CSSPrimitiveValue::UnitType::kSeconds)));
  return value;
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (wtf_size_t i = 0; i < animation_data->DirectionList().size(); ++i) {
      list->Append(*ComputedStyleUtils::ValueForAnimationDirection(
          animation_data->DirectionList()[i]));
    }
  } else {
    list->Append(*InitialValue());
  }
  return list;
}

const CSSValue* AnimationDirection::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueID::kNormal)));
  return value;
}

const CSSValue* AnimationDuration::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeTime, range, context, kValueRangeNonNegative);
}

const CSSValue* AnimationDuration::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDuration(style.Animations());
}

const CSSValue* AnimationDuration::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      const Persistent<CSSValue>, value,
      (CSSNumericLiteralValue::Create(CSSTimingData::InitialDuration(),
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (wtf_size_t i = 0; i < animation_data->FillModeList().size(); ++i) {
      list->Append(*ComputedStyleUtils::ValueForAnimationFillMode(
          animation_data->FillModeList()[i]));
    }
  } else {
    list->Append(*InitialValue());
  }
  return list;
}

const CSSValue* AnimationFillMode::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueID::kNone)));
  return value;
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (wtf_size_t i = 0; i < animation_data->IterationCountList().size();
         ++i) {
      list->Append(*ComputedStyleUtils::ValueForAnimationIterationCount(
          animation_data->IterationCountList()[i]));
    }
  } else {
    list->Append(*InitialValue());
  }
  return list;
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
    const SVGComputedStyle&,
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
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueID::kNone)));
  return value;
}

void AnimationName::ApplyValue(StyleResolverState& state,
                               const ScopedCSSValue& scoped_value) const {
  // TODO(futhark): Set the TreeScope on CSSAnimationData.
  ApplyValue(state, scoped_value.GetCSSValue());
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (wtf_size_t i = 0; i < animation_data->PlayStateList().size(); ++i) {
      list->Append(*ComputedStyleUtils::ValueForAnimationPlayState(
          animation_data->PlayStateList()[i]));
    }
  } else {
    list->Append(*InitialValue());
  }
  return list;
}

const CSSValue* AnimationPlayState::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueID::kRunning)));
  return value;
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (const auto& timeline : animation_data->TimelineList())
      list->Append(*ComputedStyleUtils::ValueForStyleNameOrKeyword(timeline));
  } else {
    list->Append(*InitialValue());
  }
  return list;
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationTimingFunction(
      style.Animations());
}

const CSSValue* AnimationTimingFunction::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueID::kEase)));
  return value;
}

const CSSValue* AspectRatio::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // Syntax: auto | auto 1/2 | 1/2 auto
  CSSValue* auto_value = nullptr;
  if (range.Peek().Id() == CSSValueID::kAuto)
    auto_value = css_parsing_utils::ConsumeIdent(range);

  if (range.AtEnd())
    return auto_value;

  CSSValue* width =
      css_parsing_utils::ConsumeNumber(range, context, kValueRangeNonNegative);
  if (!width)
    return nullptr;
  CSSValue* height = nullptr;
  if (css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
    height = css_parsing_utils::ConsumeNumber(range, context,
                                              kValueRangeNonNegative);
  } else {
    // A missing height is treated as 1.
    height = CSSNumericLiteralValue::Create(
        1.0f, CSSPrimitiveValue::UnitType::kNumber);
  }

  CSSValueList* ratio_list = CSSValueList::CreateSlashSeparated();
  ratio_list->Append(*width);
  if (height)
    ratio_list->Append(*height);
  if (!range.AtEnd()) {
    if (auto_value)
      return nullptr;
    if (range.Peek().Id() != CSSValueID::kAuto)
      return nullptr;
    auto_value = css_parsing_utils::ConsumeIdent(range);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (auto_value)
    list->Append(*auto_value);
  list->Append(*ratio_list);
  return list;
}

const CSSValue* AspectRatio::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  auto& ratio = style.AspectRatio();
  if (ratio.GetTypeForComputedStyle() == EAspectRatioType::kAuto)
    return CSSIdentifierValue::Create(CSSValueID::kAuto);

  CSSValueList* ratio_list = CSSValueList::CreateSlashSeparated();
  ratio_list->Append(*CSSNumericLiteralValue::Create(
      ratio.GetRatio().Width(), CSSPrimitiveValue::UnitType::kNumber));
  ratio_list->Append(*CSSNumericLiteralValue::Create(
      ratio.GetRatio().Height(), CSSPrimitiveValue::UnitType::kNumber));
  if (ratio.GetTypeForComputedStyle() == EAspectRatioType::kRatio)
    return ratio_list;

  DCHECK_EQ(ratio.GetTypeForComputedStyle(), EAspectRatioType::kAutoAndRatio);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  list->Append(*ratio_list);
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFilter(style, style.BackdropFilter());
}
const CSSValue* BackfaceVisibility::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const FillLayer* curr_layer = &style.BackgroundLayers(); curr_layer;
       curr_layer = curr_layer->Next())
    list->Append(*CSSIdentifierValue::Create(curr_layer->Attachment()));
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const FillLayer* curr_layer = &style.BackgroundLayers(); curr_layer;
       curr_layer = curr_layer->Next())
    list->Append(*CSSIdentifierValue::Create(curr_layer->GetBlendMode()));
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
    const SVGComputedStyle&,
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

const CSSValue* BackgroundColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

const blink::Color BackgroundColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  StyleColor background_color = style.BackgroundColor();
  if (style.ShouldForceColor(background_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBackgroundColor())
        .ColorIncludingFallback(false, style);
  }
  return background_color.Resolve(style.GetCurrentColor(),
                                  style.UsedColorScheme());
}

const CSSValue* BackgroundColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (allow_visited_style) {
    return cssvalue::CSSColorValue::Create(
        style.VisitedDependentColor(*this).Rgb());
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  const FillLayer& fill_layer = style.BackgroundLayers();
  return ComputedStyleUtils::BackgroundImageOrWebkitMaskSize(style, fill_layer);
}

const CSSValue* BaselineShift::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kBaseline || id == CSSValueID::kSub ||
      id == CSSValueID::kSuper)
    return css_parsing_utils::ConsumeIdent(range);
  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  return css_parsing_utils::ConsumeLengthOrPercent(range, context,
                                                   kValueRangeAll);
}

const CSSValue* BaselineShift::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  switch (svg_style.BaselineShift()) {
    case BS_SUPER:
      return CSSIdentifierValue::Create(CSSValueID::kSuper);
    case BS_SUB:
      return CSSIdentifierValue::Create(CSSValueID::kSub);
    case BS_LENGTH:
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          svg_style.BaselineShiftValue(), style);
  }
  NOTREACHED();
  return nullptr;
}

void BaselineShift::ApplyInherit(StyleResolverState& state) const {
  const SVGComputedStyle& parent_svg_style = state.ParentStyle()->SvgStyle();
  EBaselineShift baseline_shift = parent_svg_style.BaselineShift();
  SVGComputedStyle& svg_style = state.Style()->AccessSVGStyle();
  svg_style.SetBaselineShift(baseline_shift);
  if (baseline_shift == BS_LENGTH)
    svg_style.SetBaselineShiftValue(parent_svg_style.BaselineShiftValue());
}

void BaselineShift::ApplyValue(StyleResolverState& state,
                               const CSSValue& value) const {
  SVGComputedStyle& svg_style = state.Style()->AccessSVGStyle();
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    svg_style.SetBaselineShift(BS_LENGTH);
    svg_style.SetBaselineShiftValue(StyleBuilderConverter::ConvertLength(
        state, To<CSSPrimitiveValue>(value)));
    return;
  }
  switch (identifier_value->GetValueID()) {
    case CSSValueID::kBaseline:
      svg_style.SetBaselineShift(BS_LENGTH);
      svg_style.SetBaselineShiftValue(Length::Fixed());
      return;
    case CSSValueID::kSub:
      svg_style.SetBaselineShift(BS_SUB);
      return;
    case CSSValueID::kSuper:
      svg_style.SetBaselineShift(BS_SUPER);
      return;
    default:
      NOTREACHED();
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
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  StyleColor border_bottom_color = style.BorderBottomColor();
  if (style.ShouldForceColor(border_bottom_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(false, style);
  }
  return ComputedStyleUtils::BorderSideColor(
      style, border_bottom_color, style.BorderBottomStyle(), visited_link);
}

const CSSValue* BorderBottomColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
             ? cssvalue::CSSColorValue::Create(
                   style.VisitedDependentColor(*this).Rgb())
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForBorderRadiusCorner(
      style.BorderBottomRightRadius(), style);
}
const CSSValue* BorderBottomStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.BorderBottomWidth(), style);
}

const CSSValue* BorderCollapse::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.BorderCollapse() == EBorderCollapse::kCollapse)
    return CSSIdentifierValue::Create(CSSValueID::kCollapse);
  return CSSIdentifierValue::Create(CSSValueID::kSeparate);
}

const CSSValue* BorderImageOutset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageOutset(range, context);
}

const CSSValue* BorderImageOutset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageRepeat(style.BorderImage());
}

const CSSValue* BorderImageRepeat::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueID::kStretch)));
  return value;
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.BorderImageSource()) {
    return style.BorderImageSource()->ComputedCSSValue(style,
                                                       allow_visited_style);
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* BorderImageSource::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueID::kNone)));
  return value;
}

void BorderImageSource::ApplyValue(StyleResolverState& state,
                                   const CSSValue& value) const {
  state.Style()->SetBorderImageSource(
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
    const SVGComputedStyle&,
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
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  StyleColor border_left_color = style.BorderLeftColor();
  if (style.ShouldForceColor(border_left_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(false, style);
  }
  return ComputedStyleUtils::BorderSideColor(
      style, border_left_color, style.BorderLeftStyle(), visited_link);
}

const CSSValue* BorderLeftColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
             ? cssvalue::CSSColorValue::Create(
                   style.VisitedDependentColor(*this).Rgb())
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, border_left_color, CSSValuePhase::kUsedValue);
}

const CSSValue* BorderLeftStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  StyleColor border_right_color = style.BorderRightColor();
  if (style.ShouldForceColor(border_right_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(false, style);
  }
  return ComputedStyleUtils::BorderSideColor(style, border_right_color,
                                             style.BorderRightStyle(), false);
}

const CSSValue* BorderRightColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
             ? cssvalue::CSSColorValue::Create(
                   style.VisitedDependentColor(*this).Rgb())
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, border_right_color, CSSValuePhase::kUsedValue);
}

const CSSValue* BorderRightStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.BorderRightWidth(), style);
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
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  StyleColor border_top_color = style.BorderTopColor();
  if (style.ShouldForceColor(border_top_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(false, style);
  }
  return ComputedStyleUtils::BorderSideColor(
      style, border_top_color, style.BorderTopStyle(), visited_link);
}

const CSSValue* BorderTopColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
             ? cssvalue::CSSColorValue::Create(
                   style.VisitedDependentColor(*this).Rgb())
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForBorderRadiusCorner(
      style.BorderTopRightRadius(), style);
}

const CSSValue* BorderTopStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.BorderTopWidth(), style);
}

const CSSValue* Bottom::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context,
      css_parsing_utils::UnitlessUnlessShorthand(local_context));
}

bool Bottom::IsLayoutDependent(const ComputedStyle* style,
                               LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Bottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  // https://drafts.csswg.org/cssom/#resolved-values
  // For this property, the resolved value is the used value.
  return ComputedStyleUtils::ValueForShadowList(style.BoxShadow(), style, true,
                                                CSSValuePhase::kUsedValue);
}

const CSSValue* BoxSizing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.BoxSizing() == EBoxSizing::kContentBox)
    return CSSIdentifierValue::Create(CSSValueID::kContentBox);
  return CSSIdentifierValue::Create(CSSValueID::kBorderBox);
}

const CSSValue* BreakAfter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BreakAfter());
}

const CSSValue* BreakBefore::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BreakBefore());
}

const CSSValue* BreakInside::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BreakInside());
}

const CSSValue* BufferedRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.BufferedRendering());
}

const CSSValue* CaptionSide::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.CaptionSide());
}

const CSSValue* CaretColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color CaretColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  StyleAutoColor auto_color = style.CaretColor();
  // TODO(rego): We may want to adjust the caret color if it's the same as
  // the background to ensure good visibility and contrast.
  StyleColor result = auto_color.IsAutoColor() ? StyleColor::CurrentColor()
                                               : auto_color.ToStyleColor();
  return result.Resolve(style.GetCurrentColor(), style.UsedColorScheme());
}

const CSSValue* CaretColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (allow_visited_style) {
    return cssvalue::CSSColorValue::Create(
        style.VisitedDependentColor(*this).Rgb());
  }

  // https://drafts.csswg.org/cssom/#resolved-values
  // For this property, the resolved value is the used value.
  return ComputedStyleUtils::ValueForStyleAutoColor(style, style.CaretColor(),
                                                    CSSValuePhase::kUsedValue);
}

void CaretColor::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetCaretColor(StyleAutoColor::AutoColor());
}

void CaretColor::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetCaretColor(state.ParentStyle()->CaretColor());
}

void CaretColor::ApplyValue(StyleResolverState& state,
                            const CSSValue& value) const {
  state.Style()->SetCaretColor(
      StyleBuilderConverter::ConvertStyleAutoColor(state, value));
}

const CSSValue* Clear::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Clear());
}

namespace {

CSSValue* ConsumeClipComponent(CSSParserTokenRange& range,
                               const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeLength(
      range, context, kValueRangeAll, css_parsing_utils::UnitlessQuirk::kAllow);
}

}  // namespace

const CSSValue* Clip::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);

  if (range.Peek().FunctionId() != CSSValueID::kRect)
    return nullptr;

  CSSParserTokenRange args = css_parsing_utils::ConsumeFunction(range);
  // rect(t, r, b, l) || rect(t r b l)
  CSSValue* top = ConsumeClipComponent(args, context);
  if (!top)
    return nullptr;
  bool needs_comma = css_parsing_utils::ConsumeCommaIncludingWhitespace(args);
  CSSValue* right = ConsumeClipComponent(args, context);
  if (!right || (needs_comma &&
                 !css_parsing_utils::ConsumeCommaIncludingWhitespace(args)))
    return nullptr;
  CSSValue* bottom = ConsumeClipComponent(args, context);
  if (!bottom || (needs_comma &&
                  !css_parsing_utils::ConsumeCommaIncludingWhitespace(args)))
    return nullptr;
  CSSValue* left = ConsumeClipComponent(args, context);
  if (!left || !args.AtEnd())
    return nullptr;
  return MakeGarbageCollected<CSSQuadValue>(top, right, bottom, left,
                                            CSSQuadValue::kSerializeAsRect);
}

const CSSValue* Clip::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HasAutoClip())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
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
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);
  if (cssvalue::CSSURIValue* url =
          css_parsing_utils::ConsumeUrl(range, context))
    return url;
  return css_parsing_utils::ConsumeBasicShape(
      range, context, css_parsing_utils::AllowPathValue::kAllow);
}

const CSSValue* ClipPath::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (ClipPathOperation* operation = style.ClipPath()) {
    if (operation->GetType() == ClipPathOperation::SHAPE) {
      return ValueForBasicShape(
          style, To<ShapeClipPathOperation>(operation)->GetBasicShape());
    }
    if (operation->GetType() == ClipPathOperation::REFERENCE) {
      AtomicString url = To<ReferenceClipPathOperation>(operation)->Url();
      return MakeGarbageCollected<cssvalue::CSSURIValue>(url);
    }
  }
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* ClipRule::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.ClipRule());
}

const CSSValue* Color::ParseSingleValue(CSSParserTokenRange& range,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

const blink::Color Color::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  return style.GetCurrentColor();
}

const CSSValue* Color::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return cssvalue::CSSColorValue::Create(
      allow_visited_style ? style.VisitedDependentColor(*this).Rgb()
                          : style.GetCurrentColor().Rgb());
}

void Color::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetColor(state.Style()->InitialColorForColorScheme());
}

void Color::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetColor(state.ParentStyle()->GetColor());
}

void Color::ApplyValue(StyleResolverState& state, const CSSValue& value) const {
  // As per the spec, 'color: currentColor' is treated as 'color: inherit'
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kCurrentcolor) {
    ApplyInherit(state);
    return;
  }
  if (auto* initial_color_value = DynamicTo<CSSInitialColorValue>(value)) {
    DCHECK_EQ(state.GetElement(), state.GetDocument().documentElement());
    state.Style()->SetColor(state.Style()->InitialColorForColorScheme());
    return;
  }
  state.Style()->SetColor(
      StyleBuilderConverter::ConvertStyleColor(state, value));
}

const CSSValue* ColorInterpolation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.ColorInterpolation());
}

const CSSValue* ColorInterpolationFilters::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.ColorInterpolationFilters());
}

const CSSValue* ColorRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.ColorRendering());
}

const CSSValue* ColorScheme::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal)
    return css_parsing_utils::ConsumeIdent(range);

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  do {
    CSSValueID id = range.Peek().Id();
    // 'normal' is handled above, and 'default' is reserved for future use.
    // 'revert' is not yet implemented as a keyword, but still skip it for
    // compat and interop.
    if (id == CSSValueID::kNormal || id == CSSValueID::kRevert ||
        id == CSSValueID::kDefault) {
      return nullptr;
    }
    CSSValue* value =
        css_parsing_utils::ConsumeIdent<CSSValueID::kDark, CSSValueID::kLight>(
            range);
    if (!value)
      value = css_parsing_utils::ConsumeCustomIdent(range, context);
    if (!value)
      return nullptr;
    values->Append(*value);
  } while (!range.AtEnd());
  return values;
}

const CSSValue* ColorScheme::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.ColorScheme().IsEmpty())
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (auto ident : style.ColorScheme()) {
    list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(ident));
  }
  return list;
}

const CSSValue* ColorScheme::InitialValue() const {
  return CSSIdentifierValue::Create(CSSValueID::kNormal);
}

void ColorScheme::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetColorScheme(Vector<AtomicString>());
  state.Style()->SetDarkColorScheme(false);
}

void ColorScheme::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetColorScheme(state.ParentStyle()->ColorScheme());
  state.Style()->SetDarkColorScheme(state.ParentStyle()->DarkColorScheme());
}

void ColorScheme::ApplyValue(StyleResolverState& state,
                             const CSSValue& value) const {
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK(identifier_value->GetValueID() == CSSValueID::kNormal);
    state.Style()->SetColorScheme(Vector<AtomicString>());
    state.Style()->SetDarkColorScheme(false);
  } else if (const auto* scheme_list = DynamicTo<CSSValueList>(value)) {
    bool prefers_dark =
        state.GetDocument().GetStyleEngine().GetPreferredColorScheme() ==
        mojom::blink::PreferredColorScheme::kDark;
    bool has_dark = false;
    bool has_light = false;
    Vector<AtomicString> color_schemes;
    for (auto& item : *scheme_list) {
      if (const auto* custom_ident = DynamicTo<CSSCustomIdentValue>(*item)) {
        color_schemes.push_back(custom_ident->Value());
      } else if (const auto* ident = DynamicTo<CSSIdentifierValue>(*item)) {
        color_schemes.push_back(ident->CssText());
        if (ident->GetValueID() == CSSValueID::kDark)
          has_dark = true;
        else if (ident->GetValueID() == CSSValueID::kLight)
          has_light = true;
      } else {
        NOTREACHED();
      }
    }
    state.Style()->SetColorScheme(color_schemes);
    state.Style()->SetDarkColorScheme(has_dark && (!has_light || prefers_dark));

    if (has_dark) {
      // Record kColorSchemeDarkSupportedOnRoot if dark is present (though dark
      // may not be used). This metric is also recorded in
      // StyleEngine::UpdateColorSchemeMetrics if a meta tag supports dark.
      auto& doc = state.GetDocument();
      if (doc.documentElement() == state.ElementContext().GetElement())
        UseCounter::Count(doc, WebFeature::kColorSchemeDarkSupportedOnRoot);
    }
  } else {
    NOTREACHED();
  }
}

const CSSValue* ColumnCount::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColumnCount(range, context);
}

const CSSValue* ColumnCount::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HasAutoColumnCount())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  return CSSNumericLiteralValue::Create(style.ColumnCount(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* ColumnFill::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  StyleColor column_rule_color = style.ColumnRuleColor();
  if (style.ShouldForceColor(column_rule_color))
    return style.GetCurrentColor();
  return column_rule_color.Resolve(style.GetCurrentColor(),
                                   style.UsedColorScheme());
}

const CSSValue* ColumnRuleColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return allow_visited_style ? cssvalue::CSSColorValue::Create(
                                   style.VisitedDependentColor(*this).Rgb())
                             : ComputedStyleUtils::CurrentColorOrValidColor(
                                   style, style.ColumnRuleColor(),
                                   CSSValuePhase::kComputedValue);
}

const CSSValue* ColumnRuleStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HasAutoColumnWidth())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  return ZoomAdjustedPixelValue(style.ColumnWidth(), style);
}

// none | strict | content | [ size || layout || style || paint ]
const CSSValue* Contain::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);

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
    if (id == CSSValueID::kSize && !size)
      size = css_parsing_utils::ConsumeIdent(range);
    else if (id == CSSValueID::kLayout && !layout)
      layout = css_parsing_utils::ConsumeIdent(range);
    else if (id == CSSValueID::kStyle && !style)
      style = css_parsing_utils::ConsumeIdent(range);
    else if (id == CSSValueID::kPaint && !paint)
      paint = css_parsing_utils::ConsumeIdent(range);
    else
      break;
  }
  if (size)
    list->Append(*size);
  if (layout)
    list->Append(*layout);
  if (style) {
    context.Count(WebFeature::kCSSValueContainStyle);
    list->Append(*style);
  }
  if (paint)
    list->Append(*paint);
  if (!list->length())
    return nullptr;
  return list;
}

const CSSValue* Contain::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.Contain())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  if (style.Contain() == kContainsStrict)
    return CSSIdentifierValue::Create(CSSValueID::kStrict);
  if (style.Contain() == kContainsContent)
    return CSSIdentifierValue::Create(CSSValueID::kContent);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (style.ContainsSize())
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kSize));
  if (style.Contain() & kContainsLayout)
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kLayout));
  if (style.ContainsStyle())
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kStyle));
  if (style.ContainsPaint())
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kPaint));
  DCHECK(list->length());
  return list;
}

const CSSValue* ContainIntrinsicSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  CSSValue* width =
      css_parsing_utils::ConsumeLength(range, context, kValueRangeNonNegative);
  if (!width)
    return nullptr;
  CSSValue* height =
      css_parsing_utils::ConsumeLength(range, context, kValueRangeNonNegative);
  if (!height)
    height = width;
  return MakeGarbageCollected<CSSValuePair>(width, height,
                                            CSSValuePair::kDropIdenticalValues);
}

const CSSValue* ContainIntrinsicSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  auto& size = style.ContainIntrinsicSize();
  if (size.Width().IsAuto()) {
    DCHECK(size.Height().IsAuto());
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  return MakeGarbageCollected<CSSValuePair>(
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.ContainIntrinsicSize().Width(), style),
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.ContainIntrinsicSize().Height(), style),
      CSSValuePair::kDropIdenticalValues);
}

namespace {

CSSValue* ConsumeAttr(CSSParserTokenRange args,
                      const CSSParserContext& context) {
  if (args.Peek().GetType() != kIdentToken)
    return nullptr;

  AtomicString attr_name =
      args.ConsumeIncludingWhitespace().Value().ToAtomicString();
  if (!args.AtEnd())
    return nullptr;

  if (context.IsHTMLDocument())
    attr_name = attr_name.LowerASCII();

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
  if (!identifier)
    return nullptr;

  CSSStringValue* separator = nullptr;
  if (!counters) {
    separator = MakeGarbageCollected<CSSStringValue>(String());
  } else {
    if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(args) ||
        args.Peek().GetType() != kStringToken)
      return nullptr;
    separator = MakeGarbageCollected<CSSStringValue>(
        args.ConsumeIncludingWhitespace().Value().ToString());
  }

  CSSIdentifierValue* list_style = nullptr;
  if (css_parsing_utils::ConsumeCommaIncludingWhitespace(args)) {
    CSSValueID id = args.Peek().Id();
    if ((id != CSSValueID::kNone &&
         (id < CSSValueID::kDisc || id > CSSValueID::kKatakanaIroha)))
      return nullptr;
    list_style = css_parsing_utils::ConsumeIdent(args);
  } else {
    list_style = CSSIdentifierValue::Create(CSSValueID::kDecimal);
  }

  if (!args.AtEnd())
    return nullptr;
  return MakeGarbageCollected<cssvalue::CSSCounterValue>(identifier, list_style,
                                                         separator);
}

}  // namespace

const CSSValue* Content::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  if (css_parsing_utils::IdentMatches<CSSValueID::kNone, CSSValueID::kNormal>(
          range.Peek().Id()))
    return css_parsing_utils::ConsumeIdent(range);

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
    if (!parsed_value)
      parsed_value = css_parsing_utils::ConsumeString(range);
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
        if (!values->length())
          return nullptr;
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
    if (!alt_text)
      return nullptr;
    outer_list->Append(*alt_text);
  }
  return outer_list;
}

const CSSValue* Content::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForContentData(style, allow_visited_style);
}

void Content::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetContent(nullptr);
}

void Content::ApplyInherit(StyleResolverState& state) const {
  // FIXME: In CSS3, it will be possible to inherit content. In CSS2 it is not.
  // This note is a reminder that eventually "inherit" needs to be supported.
}

void Content::ApplyValue(StyleResolverState& state,
                         const CSSValue& value) const {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK(identifier_value->GetValueID() == CSSValueID::kNormal ||
           identifier_value->GetValueID() == CSSValueID::kNone);
    if (identifier_value->GetValueID() == CSSValueID::kNone)
      state.Style()->SetContent(MakeGarbageCollected<NoneContentData>());
    else
      state.Style()->SetContent(nullptr);
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
      const auto list_style_type =
          CssValueIDToPlatformEnum<EListStyleType>(counter_value->ListStyle());
      std::unique_ptr<CounterContent> counter =
          std::make_unique<CounterContent>(
              AtomicString(counter_value->Identifier()), list_style_type,
              AtomicString(counter_value->Separator()));
      next_content =
          MakeGarbageCollected<CounterContentData>(std::move(counter));
    } else if (auto* item_identifier_value =
                   DynamicTo<CSSIdentifierValue>(item.Get())) {
      QuoteType quote_type;
      switch (item_identifier_value->GetValueID()) {
        default:
          NOTREACHED();
          FALLTHROUGH;
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
        state.Style()->SetHasAttrContent();
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

    if (!first_content)
      first_content = next_content;
    else
      prev_content->SetNext(next_content);

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
  state.Style()->SetContent(first_content);
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    IntPoint hot_spot(-1, -1);
    bool hot_spot_specified = false;
    if (css_parsing_utils::ConsumeNumberRaw(range, context, num)) {
      hot_spot.SetX(clampTo<int>(num));
      if (!css_parsing_utils::ConsumeNumberRaw(range, context, num))
        return nullptr;
      hot_spot.SetY(clampTo<int>(num));
      hot_spot_specified = true;
    }

    if (!list)
      list = CSSValueList::CreateCommaSeparated();

    list->Append(*MakeGarbageCollected<cssvalue::CSSCursorImageValue>(
        *image, hot_spot_specified, hot_spot));
    if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(range))
      return nullptr;
  }

  CSSValueID id = range.Peek().Id();
  if (!range.AtEnd()) {
    if (id == CSSValueID::kWebkitZoomIn)
      context.Count(WebFeature::kPrefixedCursorZoomIn);
    else if (id == CSSValueID::kWebkitZoomOut)
      context.Count(WebFeature::kPrefixedCursorZoomOut);
    else if (id == CSSValueID::kWebkitGrab)
      context.Count(WebFeature::kPrefixedCursorGrab);
    else if (id == CSSValueID::kWebkitGrabbing)
      context.Count(WebFeature::kPrefixedCursorGrabbing);
  }
  CSSValue* cursor_type = nullptr;
  if (id == CSSValueID::kHand) {
    if (!in_quirks_mode)  // Non-standard behavior
      return nullptr;
    cursor_type = CSSIdentifierValue::Create(CSSValueID::kPointer);
    range.ConsumeIncludingWhitespace();
  } else if ((id >= CSSValueID::kAuto && id <= CSSValueID::kWebkitZoomOut) ||
             id == CSSValueID::kCopy || id == CSSValueID::kNone) {
    cursor_type = css_parsing_utils::ConsumeIdent(range);
  } else {
    return nullptr;
  }

  if (!list)
    return cursor_type;
  list->Append(*cursor_type);
  return list;
}

const CSSValue* Cursor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  state.Style()->ClearCursorList();
  state.Style()->SetCursor(ComputedStyleInitialValues::InitialCursor());
}

void Cursor::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetCursor(state.ParentStyle()->Cursor());
  state.Style()->SetCursorList(state.ParentStyle()->Cursors());
}

void Cursor::ApplyValue(StyleResolverState& state,
                        const CSSValue& value) const {
  state.Style()->ClearCursorList();
  if (auto* value_list = DynamicTo<CSSValueList>(value)) {
    state.Style()->SetCursor(ECursor::kAuto);
    for (const auto& item : *value_list) {
      if (const auto* cursor =
              DynamicTo<cssvalue::CSSCursorImageValue>(*item)) {
        const CSSValue& image = cursor->ImageValue();
        state.Style()->AddCursor(
            state.GetStyleImage(CSSPropertyID::kCursor, image),
            cursor->HotSpotSpecified(), cursor->HotSpot());
      } else {
        state.Style()->SetCursor(
            To<CSSIdentifierValue>(*item).ConvertTo<ECursor>());
      }
    }
  } else {
    state.Style()->SetCursor(
        To<CSSIdentifierValue>(value).ConvertTo<ECursor>());
  }
}

const CSSValue* Cx::ParseSingleValue(CSSParserTokenRange& range,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(range, context,
                                                             kValueRangeAll);
}

const CSSValue* Cx::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(svg_style.Cx(),
                                                             style);
}

const CSSValue* Cy::ParseSingleValue(CSSParserTokenRange& range,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(range, context,
                                                             kValueRangeAll);
}

const CSSValue* Cy::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(svg_style.Cy(),
                                                             style);
}

const CSSValue* D::ParseSingleValue(CSSParserTokenRange& range,
                                    const CSSParserContext&,
                                    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePathOrNone(range);
}

const CSSValue* D::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (const StylePath* style_path = svg_style.D())
    return style_path->ComputedCSSValue();
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* Direction::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Direction());
}

void Direction::ApplyValue(StyleResolverState& state,
                           const CSSValue& value) const {
  state.Style()->SetDirection(
      To<CSSIdentifierValue>(value).ConvertTo<TextDirection>());
}

namespace {

static bool IsDisplayOutside(CSSValueID id) {
  return id >= CSSValueID::kInline && id <= CSSValueID::kBlock;
}

static bool IsDisplayInside(CSSValueID id) {
  if (id >= CSSValueID::kFlowRoot && id <= CSSValueID::kGrid)
    return true;
  if (id == CSSValueID::kMath)
    return RuntimeEnabledFeatures::MathMLCoreEnabled();
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
    if (range.AtEnd())
      return display_outside;
    id = range.Peek().Id();
    if (!IsDisplayInside(id))
      return nullptr;
    display_inside = css_parsing_utils::ConsumeIdent(range);
  } else if (IsDisplayInside(id)) {
    display_inside = css_parsing_utils::ConsumeIdent(range);
    if (range.AtEnd())
      return display_inside;
    id = range.Peek().Id();
    if (!IsDisplayOutside(id))
      return nullptr;
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
      IsDisplayInternal(id) || IsDisplayLegacy(id))
    return css_parsing_utils::ConsumeIdent(range);

  if (!RuntimeEnabledFeatures::CSSLayoutAPIEnabled())
    return nullptr;

  if (!context.IsSecureContext())
    return nullptr;

  CSSValueID function = range.Peek().FunctionId();
  if (function != CSSValueID::kLayout && function != CSSValueID::kInlineLayout)
    return nullptr;

  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = css_parsing_utils::ConsumeFunction(range_copy);
  CSSCustomIdentValue* name =
      css_parsing_utils::ConsumeCustomIdent(args, context);

  // If we didn't get a custom-ident or didn't exhaust the function arguments
  // return nothing.
  if (!name || !args.AtEnd())
    return nullptr;

  range = range_copy;
  return MakeGarbageCollected<cssvalue::CSSLayoutFunctionValue>(
      name, /* is_inline */ function == CSSValueID::kInlineLayout);
}

const CSSValue* Display::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  state.Style()->SetDisplay(ComputedStyleInitialValues::InitialDisplay());
  state.Style()->SetDisplayLayoutCustomName(
      ComputedStyleInitialValues::InitialDisplayLayoutCustomName());
}

void Display::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetDisplay(state.ParentStyle()->Display());
  state.Style()->SetDisplayLayoutCustomName(
      state.ParentStyle()->DisplayLayoutCustomName());
}

void Display::ApplyValue(StyleResolverState& state,
                         const CSSValue& value) const {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    state.Style()->SetDisplay(identifier_value->ConvertTo<EDisplay>());
    state.Style()->SetDisplayLayoutCustomName(
        ComputedStyleInitialValues::InitialDisplayLayoutCustomName());
    return;
  }

  if (value.IsValueList()) {
    state.Style()->SetDisplayLayoutCustomName(
        ComputedStyleInitialValues::InitialDisplayLayoutCustomName());
    const CSSValueList& display_pair = To<CSSValueList>(value);
    DCHECK_EQ(display_pair.length(), 2u);
    DCHECK(display_pair.Item(0).IsIdentifierValue());
    DCHECK(display_pair.Item(1).IsIdentifierValue());
    const auto& outside = To<CSSIdentifierValue>(display_pair.Item(0));
    const auto& inside = To<CSSIdentifierValue>(display_pair.Item(1));
    // TODO(crbug.com/995106): should apply to more than just math.
    DCHECK(inside.GetValueID() == CSSValueID::kMath);
    if (outside.GetValueID() == CSSValueID::kBlock)
      state.Style()->SetDisplay(EDisplay::kBlockMath);
    else
      state.Style()->SetDisplay(EDisplay::kMath);
    return;
  }

  const auto& layout_function_value =
      To<cssvalue::CSSLayoutFunctionValue>(value);

  EDisplay display = layout_function_value.IsInline()
                         ? EDisplay::kInlineLayoutCustom
                         : EDisplay::kLayoutCustom;
  state.Style()->SetDisplay(display);
  state.Style()->SetDisplayLayoutCustomName(layout_function_value.GetName());
}

const CSSValue* DominantBaseline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.DominantBaseline());
}

const CSSValue* EmptyCells::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.EmptyCells());
}

const CSSValue* Fill::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  return css_parsing_utils::ParsePaintStroke(range, context);
}

const CSSValue* Fill::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGPaint(svg_style.FillPaint(), style);
}

const blink::Color Fill::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  DCHECK(style.SvgStyle().FillPaint().HasColor());
  StyleColor fill_color = style.SvgStyle().FillPaint().GetColor();
  if (style.ShouldForceColor(fill_color))
    return style.GetCurrentColor();
  return fill_color.Resolve(style.GetCurrentColor(), style.UsedColorScheme());
}

const CSSValue* FillOpacity::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(range, context);
}

const CSSValue* FillOpacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle&,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(svg_style.FillOpacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* FillRule::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.FillRule());
}

const CSSValue* Filter::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFilterFunctionList(range, context);
}

const CSSValue* Filter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFilter(style, style.Filter());
}

const CSSValue* FlexBasis::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // FIXME: Support intrinsic dimensions too.
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeLengthOrPercent(range, context,
                                                   kValueRangeNonNegative);
}

const CSSValue* FlexBasis::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.FlexBasis(),
                                                             style);
}

const CSSValue* FlexDirection::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.FlexDirection());
}

const CSSValue* FlexGrow::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(range, context,
                                          kValueRangeNonNegative);
}

const CSSValue* FlexGrow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.FlexGrow(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* FlexShrink::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(range, context,
                                          kValueRangeNonNegative);
}

const CSSValue* FlexShrink::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.FlexShrink(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* FlexWrap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.FlexWrap());
}

const CSSValue* Float::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HasOutOfFlowPosition())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
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
    const ComputedStyle& style) const {
  return style.ResolvedColor(style.FloodColor());
}

const CSSValue* FloodColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const ComputedStyle&,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(svg_style.FloodOpacity(),
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontFamily(style);
}

void FontFamily::ApplyValue(StyleResolverState& state,
                            const ScopedCSSValue& scoped_value) const {
  state.GetFontBuilder().SetFamilyTreeScope(scoped_value.GetTreeScope());
  ApplyValue(state, scoped_value.GetCSSValue());
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  const blink::FontFeatureSettings* feature_settings =
      style.GetFontDescription().FeatureSettings();
  if (!feature_settings || !feature_settings->size())
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (wtf_size_t i = 0; i < feature_settings->size(); ++i) {
    const FontFeature& feature = feature_settings->at(i);
    auto* feature_value = MakeGarbageCollected<cssvalue::CSSFontFeatureValue>(
        feature.TagString(), feature.Value());
    list->Append(*feature_value);
  }
  return list;
}

const CSSValue* FontKerning::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetFontDescription().GetKerning());
}

const CSSValue* FontOpticalSizing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(
      style.GetFontDescription().FontOpticalSizing());
}

const CSSValue* FontSizeAdjust::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSFontSizeAdjustEnabled());
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeNumber(range, context,
                                          kValueRangeNonNegative);
}

const CSSValue* FontSizeAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontVariantCaps(style);
}

const CSSValue* FontVariantEastAsian::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal)
    return css_parsing_utils::ConsumeIdent(range);

  FontVariantEastAsianParser east_asian_parser;
  do {
    if (east_asian_parser.ConsumeEastAsian(range) !=
        FontVariantEastAsianParser::ParseResult::kConsumedValue)
      return nullptr;
  } while (!range.AtEnd());

  return east_asian_parser.FinalizeValue();
}

const CSSValue* FontVariantEastAsian::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontVariantEastAsian(style);
}

const CSSValue* FontVariantLigatures::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal ||
      range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);

  FontVariantLigaturesParser ligatures_parser;
  do {
    if (ligatures_parser.ConsumeLigature(range) !=
        FontVariantLigaturesParser::ParseResult::kConsumedValue)
      return nullptr;
  } while (!range.AtEnd());

  return ligatures_parser.FinalizeValue();
}

const CSSValue* FontVariantLigatures::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontVariantLigatures(style);
}

const CSSValue* FontVariantNumeric::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal)
    return css_parsing_utils::ConsumeIdent(range);

  FontVariantNumericParser numeric_parser;
  do {
    if (numeric_parser.ConsumeNumeric(range) !=
        FontVariantNumericParser::ParseResult::kConsumedValue)
      return nullptr;
  } while (!range.AtEnd());

  return numeric_parser.FinalizeValue();
}

const CSSValue* FontVariantNumeric::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontVariantNumeric(style);
}

namespace {

cssvalue::CSSFontVariationValue* ConsumeFontVariationTag(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  // Feature tag name consists of 4-letter characters.
  static const wtf_size_t kTagNameLength = 4;

  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  // Feature tag name comes first
  if (token.GetType() != kStringToken)
    return nullptr;
  if (token.Value().length() != kTagNameLength)
    return nullptr;
  AtomicString tag = token.Value().ToAtomicString();
  for (wtf_size_t i = 0; i < kTagNameLength; ++i) {
    // Limits the range of characters to 0x20-0x7E, following the tag name rules
    // defined in the OpenType specification.
    UChar character = tag[i];
    if (character < 0x20 || character > 0x7E)
      return nullptr;
  }

  double tag_value = 0;
  if (!css_parsing_utils::ConsumeNumberRaw(range, context, tag_value))
    return nullptr;
  return MakeGarbageCollected<cssvalue::CSSFontVariationValue>(
      tag, clampTo<float>(tag_value));
}

}  // namespace

const CSSValue* FontVariationSettings::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal)
    return css_parsing_utils::ConsumeIdent(range);
  CSSValueList* variation_settings = CSSValueList::CreateCommaSeparated();
  do {
    cssvalue::CSSFontVariationValue* font_variation_value =
        ConsumeFontVariationTag(range, context);
    if (!font_variation_value)
      return nullptr;
    variation_settings->Append(*font_variation_value);
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));
  return variation_settings;
}

const CSSValue* FontVariationSettings::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  const blink::FontVariationSettings* variation_settings =
      style.GetFontDescription().VariationSettings();
  if (!variation_settings || !variation_settings->size())
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (wtf_size_t i = 0; i < variation_settings->size(); ++i) {
    const FontVariationAxis& variation_axis = variation_settings->at(i);
    cssvalue::CSSFontVariationValue* variation_value =
        MakeGarbageCollected<cssvalue::CSSFontVariationValue>(
            variation_axis.TagString(), variation_axis.Value());
    list->Append(*variation_value);
  }
  return list;
}

const CSSValue* FontWeight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontWeight(range, context);
}

const CSSValue* FontWeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontWeight(style);
}

const CSSValue* ForcedColorAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ForcedColorAdjust());
}

void InternalVisitedColor::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetInternalVisitedColor(
      state.Style()->InitialColorForColorScheme());
}

void InternalVisitedColor::ApplyInherit(StyleResolverState& state) const {
  auto color = state.ParentStyle()->GetColor();
  state.Style()->SetInternalVisitedColor(color);
}

void InternalVisitedColor::ApplyValue(StyleResolverState& state,
                                      const CSSValue& value) const {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kCurrentcolor) {
    ApplyInherit(state);
    return;
  }
  if (auto* initial_color_value = DynamicTo<CSSInitialColorValue>(value)) {
    DCHECK_EQ(state.GetElement(), state.GetDocument().documentElement());
    state.Style()->SetInternalVisitedColor(
        state.Style()->InitialColorForColorScheme());
    return;
  }
  state.Style()->SetInternalVisitedColor(
      StyleBuilderConverter::ConvertStyleColor(state, value, true));
}

const blink::Color InternalVisitedColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  return style.GetInternalVisitedCurrentColor();
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridTrackSizeList(kForColumns, style);
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
    if (!row_or_column_value && !dense_algorithm)
      return nullptr;
  }
  CSSValueList* parsed_values = CSSValueList::CreateSpaceSeparated();
  if (row_or_column_value)
    parsed_values->Append(*row_or_column_value);
  if (dense_algorithm)
    parsed_values->Append(*dense_algorithm);
  return parsed_values;
}

const CSSValue* GridAutoFlow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  switch (style.GetGridAutoFlow()) {
    case kAutoFlowRow:
    case kAutoFlowRowDense:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kRow));
      break;
    case kAutoFlowColumn:
    case kAutoFlowColumnDense:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kColumn));
      break;
    default:
      NOTREACHED();
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

const CSSValue* GridAutoRows::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTrackList(
      range, context, css_parsing_utils::TrackListType::kGridAuto);
}

const CSSValue* GridAutoRows::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridTrackSizeList(kForRows, style);
}

const CSSValue* GridColumnEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridLine(range, context);
}

const CSSValue* GridColumnEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridPosition(style.GridRowStart());
}

const CSSValue* GridTemplateAreas::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);

  NamedGridAreaMap grid_area_map;
  size_t row_count = 0;
  size_t column_count = 0;

  while (range.Peek().GetType() == kStringToken) {
    if (!css_parsing_utils::ParseGridTemplateAreasRow(
            range.ConsumeIncludingWhitespace().Value().ToString(),
            grid_area_map, row_count, column_count))
      return nullptr;
    ++row_count;
  }

  if (row_count == 0)
    return nullptr;
  DCHECK(column_count);
  return MakeGarbageCollected<cssvalue::CSSGridTemplateAreasValue>(
      grid_area_map, row_count, column_count);
}

const CSSValue* GridTemplateAreas::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  state.Style()->SetImplicitNamedGridColumnLines(
      ComputedStyleInitialValues::InitialImplicitNamedGridColumnLines());
  state.Style()->SetImplicitNamedGridRowLines(
      ComputedStyleInitialValues::InitialImplicitNamedGridRowLines());

  state.Style()->SetNamedGridArea(
      ComputedStyleInitialValues::InitialNamedGridArea());
  state.Style()->SetNamedGridAreaRowCount(
      ComputedStyleInitialValues::InitialNamedGridAreaRowCount());
  state.Style()->SetNamedGridAreaColumnCount(
      ComputedStyleInitialValues::InitialNamedGridAreaColumnCount());
}

void GridTemplateAreas::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetImplicitNamedGridColumnLines(
      state.ParentStyle()->ImplicitNamedGridColumnLines());
  state.Style()->SetImplicitNamedGridRowLines(
      state.ParentStyle()->ImplicitNamedGridRowLines());

  state.Style()->SetNamedGridArea(state.ParentStyle()->NamedGridArea());
  state.Style()->SetNamedGridAreaRowCount(
      state.ParentStyle()->NamedGridAreaRowCount());
  state.Style()->SetNamedGridAreaColumnCount(
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
  state.Style()->SetImplicitNamedGridColumnLines(
      implicit_named_grid_column_lines);
  state.Style()->SetImplicitNamedGridRowLines(implicit_named_grid_row_lines);

  state.Style()->SetNamedGridArea(new_named_grid_areas);
  state.Style()->SetNamedGridAreaRowCount(grid_template_areas_value.RowCount());
  state.Style()->SetNamedGridAreaColumnCount(
      grid_template_areas_value.ColumnCount());
}

const CSSValue* GridTemplateColumns::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTemplatesRowsOrColumns(range, context);
}

bool GridTemplateColumns::IsLayoutDependent(const ComputedStyle* style,
                                            LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGrid();
}

const CSSValue* GridTemplateColumns::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridTrackList(kForColumns, layout_object,
                                                   style);
}

const CSSValue* GridTemplateRows::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeGridTemplatesRowsOrColumns(range, context);
}

bool GridTemplateRows::IsLayoutDependent(const ComputedStyle* style,
                                         LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGrid();
}

const CSSValue* GridTemplateRows::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridTrackList(kForRows, layout_object,
                                                   style);
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
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (ComputedStyleUtils::WidthOrHeightShouldReturnUsedValue(layout_object)) {
    return ZoomAdjustedPixelValue(
        ComputedStyleUtils::UsedBoxSize(*layout_object).Height(), style);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Height(),
                                                             style);
}

const CSSValue* Hyphens::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetHyphens());
}

const CSSValue* ImageOrientation::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::ImageOrientationEnabled());
  if (range.Peek().Id() == CSSValueID::kFromImage)
    return css_parsing_utils::ConsumeIdent(range);
  if (range.Peek().Id() == CSSValueID::kNone) {
    return css_parsing_utils::ConsumeIdent(range);
  }
  return nullptr;
}

const CSSValue* ImageOrientation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.RespectImageOrientation() == kRespectImageOrientation)
    return CSSIdentifierValue::Create(CSSValueID::kFromImage);
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* ImageRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ImageRendering());
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
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* InsetBlockStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* InsetInlineEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* InsetInlineStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid);
}

const blink::Color InternalVisitedBackgroundColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(visited_link);

  StyleColor visited_background_color = style.InternalVisitedBackgroundColor();
  if (style.ShouldForceColor(visited_background_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBackgroundColor())
        .ColorIncludingFallback(true, style);
  }
  blink::Color color = visited_background_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme());

  // TODO: Technically someone could explicitly specify the color
  // transparent, but for now we'll just assume that if the background color
  // is transparent that it wasn't set. Note that it's weird that we're
  // returning unvisited info for a visited link, but given our restriction
  // that the alpha values have to match, it makes more sense to return the
  // unvisited background color if specified than it does to return black.
  // This behavior matches what Firefox 4 does as well.
  if (color == blink::Color::kTransparent) {
    return style.BackgroundColor().Resolve(style.GetCurrentColor(),
                                           style.UsedColorScheme());
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
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  StyleColor visited_border_left_color = style.InternalVisitedBorderLeftColor();
  if (style.ShouldForceColor(visited_border_left_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(true, style);
  }
  return visited_border_left_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme());
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
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  StyleColor visited_border_top_color = style.InternalVisitedBorderTopColor();
  if (style.ShouldForceColor(visited_border_top_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(true, style);
  }
  return visited_border_top_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme());
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
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  StyleAutoColor auto_color = style.InternalVisitedCaretColor();
  StyleColor result = auto_color.IsAutoColor() ? StyleColor::CurrentColor()
                                               : auto_color.ToStyleColor();
  return result.Resolve(style.GetInternalVisitedCurrentColor(),
                        style.UsedColorScheme());
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
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  StyleColor visited_border_right_color =
      style.InternalVisitedBorderRightColor();
  if (style.ShouldForceColor(visited_border_right_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(true, style);
  }
  return visited_border_right_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme());
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
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  StyleColor visited_border_bottom_color =
      style.InternalVisitedBorderBottomColor();
  if (style.ShouldForceColor(visited_border_bottom_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedBorderColor())
        .ColorIncludingFallback(true, style);
  }
  return visited_border_bottom_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme());
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
  return css_parsing_utils::ParsePaintStroke(range, context);
}

const blink::Color InternalVisitedFill::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  const SVGPaint& paint = style.SvgStyle().InternalVisitedFillPaint();

  // FIXME: This code doesn't support the uri component of the visited link
  // paint, https://bugs.webkit.org/show_bug.cgi?id=70006
  if (!paint.HasColor()) {
    return To<Longhand>(GetCSSPropertyFill())
        .ColorIncludingFallback(false, style);
  }
  StyleColor visited_fill_color = paint.GetColor();
  if (style.ShouldForceColor(visited_fill_color))
    return style.GetInternalVisitedCurrentColor();
  return visited_fill_color.Resolve(style.GetInternalVisitedCurrentColor(),
                                    style.UsedColorScheme());
}

const blink::Color InternalVisitedColumnRuleColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  StyleColor visited_column_rule_color = style.InternalVisitedColumnRuleColor();
  if (style.ShouldForceColor(visited_column_rule_color))
    return style.GetInternalVisitedCurrentColor();
  return visited_column_rule_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme());
}

const CSSValue* InternalVisitedColumnRuleColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color InternalVisitedOutlineColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  StyleColor visited_outline_color = style.InternalVisitedOutlineColor();
  if (style.ShouldForceColor(visited_outline_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedOutlineColor())
        .ColorIncludingFallback(true, style);
  }
  return visited_outline_color.Resolve(style.GetInternalVisitedCurrentColor(),
                                       style.UsedColorScheme());
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
  return css_parsing_utils::ParsePaintStroke(range, context);
}

const blink::Color InternalVisitedStroke::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  const SVGPaint& paint = style.SvgStyle().InternalVisitedStrokePaint();

  // FIXME: This code doesn't support the uri component of the visited link
  // paint, https://bugs.webkit.org/show_bug.cgi?id=70006
  if (!paint.HasColor()) {
    return To<Longhand>(GetCSSPropertyStroke())
        .ColorIncludingFallback(false, style);
  }
  StyleColor visited_stroke_color = paint.GetColor();
  if (style.ShouldForceColor(visited_stroke_color))
    return style.GetInternalVisitedCurrentColor();
  return visited_stroke_color.Resolve(style.GetInternalVisitedCurrentColor(),
                                      style.UsedColorScheme());
}

const blink::Color InternalVisitedTextDecorationColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  StyleColor visited_decoration_color =
      style.DecorationColorIncludingFallback(visited_link);
  if (style.ShouldForceColor(visited_decoration_color))
    return style.GetInternalVisitedCurrentColor();
  return visited_decoration_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme());
}

const CSSValue* InternalVisitedTextDecorationColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color InternalVisitedTextEmphasisColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  StyleColor visited_text_emphasis_color =
      style.InternalVisitedTextEmphasisColor();
  if (style.ShouldForceColor(visited_text_emphasis_color))
    return style.GetInternalVisitedCurrentColor();
  return visited_text_emphasis_color.Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme());
}

const CSSValue* InternalVisitedTextEmphasisColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color InternalVisitedTextFillColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  return style.InternalVisitedTextFillColor().Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme());
}

const CSSValue* InternalVisitedTextFillColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color InternalVisitedTextStrokeColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(visited_link);
  return style.InternalVisitedTextStrokeColor().Resolve(
      style.GetInternalVisitedCurrentColor(), style.UsedColorScheme());
}

const CSSValue* InternalVisitedTextStrokeColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color InternalForcedBackgroundColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  blink::Color current_color;
  int alpha;
  if (visited_link) {
    current_color = style.GetInternalVisitedCurrentColor();
    alpha = style.InternalVisitedBackgroundColor()
                .Resolve(current_color, style.UsedColorScheme())
                .Alpha();
  } else {
    current_color = style.GetCurrentColor();
    alpha = style.BackgroundColor()
                .Resolve(current_color, style.UsedColorScheme())
                .Alpha();
  }

  return style.InternalForcedBackgroundColor().ResolveWithAlpha(
      current_color, style.UsedColorScheme(), alpha);
}

const CSSValue*
InternalForcedBackgroundColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  bool visited_link = allow_visited_style &&
                      style.InsideLink() == EInsideLink::kInsideVisitedLink;
  return cssvalue::CSSColorValue::Create(
      ColorIncludingFallback(visited_link, style).Rgb());
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
    const ComputedStyle& style) const {
  blink::Color current_color = visited_link
                                   ? style.GetInternalVisitedCurrentColor()
                                   : style.GetCurrentColor();

  return style.InternalForcedBorderColor().Resolve(current_color,
                                                   style.UsedColorScheme());
}

const CSSValue* InternalForcedBorderColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  bool visited_link = allow_visited_style &&
                      style.InsideLink() == EInsideLink::kInsideVisitedLink;
  return cssvalue::CSSColorValue::Create(
      ColorIncludingFallback(visited_link, style).Rgb());
}

const CSSValue* InternalForcedBorderColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

const blink::Color InternalForcedOutlineColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  blink::Color current_color = visited_link
                                   ? style.GetInternalVisitedCurrentColor()
                                   : style.GetCurrentColor();

  return style.InternalForcedOutlineColor().Resolve(current_color,
                                                    style.UsedColorScheme());
}

const CSSValue* InternalForcedOutlineColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  bool visited_link = allow_visited_style &&
                      style.InsideLink() == EInsideLink::kInsideVisitedLink;
  return cssvalue::CSSColorValue::Create(
      ColorIncludingFallback(visited_link, style).Rgb());
}

const CSSValue* InternalForcedOutlineColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeColor(range, context,
                                         IsQuirksModeBehavior(context.Mode()));
}

const CSSValue* Isolation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
                                      CSSValueID::kBaseline>(range.Peek().Id()))
    return nullptr;
  return css_parsing_utils::ConsumeContentDistributionOverflowPosition(
      range, css_parsing_utils::IsContentPositionOrLeftOrRightKeyword);
}

const CSSValue* JustifyContent::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  if (css_parsing_utils::IdentMatches<CSSValueID::kAuto>(range.Peek().Id()))
    return nullptr;
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
      range, context,
      css_parsing_utils::UnitlessUnlessShorthand(local_context));
}

bool Left::IsLayoutDependent(const ComputedStyle* style,
                             LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Left::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.LetterSpacing())
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
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
    const ComputedStyle& style) const {
  return style.ResolvedColor(style.LightingColor());
}

const CSSValue* LightingColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.LightingColor(), CSSValuePhase::kComputedValue);
}

const CSSValue* LineBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForLineHeight(style);
}

const CSSValue* LineHeightStep::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(range, context,
                                          kValueRangeNonNegative);
}

const CSSValue* LineHeightStep::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.LineHeightStep(), style);
}

const CSSValue* ListStyleImage::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeImageOrNone(range, context);
}

const CSSValue* ListStyleImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.ListStyleImage())
    return style.ListStyleImage()->ComputedCSSValue(style, allow_visited_style);
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

void ListStyleImage::ApplyValue(StyleResolverState& state,
                                const CSSValue& value) const {
  state.Style()->SetListStyleImage(
      state.GetStyleImage(CSSPropertyID::kListStyleImage, value));
}

const CSSValue* ListStylePosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ListStylePosition());
}

const CSSValue* ListStyleType::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (auto* none = css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(range))
    return none;

  if (!RuntimeEnabledFeatures::CSSAtRuleCounterStyleEnabled()) {
    if (auto* ident = css_parsing_utils::ConsumeIdent(range)) {
      CSSValueID value_id = ident->GetValueID();
      if (value_id < CSSValueID::kDisc || value_id > CSSValueID::kKatakanaIroha)
        return nullptr;
      return MakeGarbageCollected<CSSCustomIdentValue>(
          AtomicString(getValueName(value_id)));
    }
  } else {
    if (auto* counter_style_name =
            css_parsing_utils::ConsumeCustomIdent(range, context))
      return counter_style_name;
  }

  return css_parsing_utils::ConsumeString(range);
}

const CSSValue* ListStyleType::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.GetListStyleType())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  if (style.GetListStyleType()->IsString()) {
    return MakeGarbageCollected<CSSStringValue>(
        style.GetListStyleType()->GetStringValue());
  }
  // TODO(crbug.com/687225): Return a scoped CSSValue?
  return MakeGarbageCollected<CSSCustomIdentValue>(
      style.GetListStyleType()->GetCounterStyleName());
}

void ListStyleType::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetListStyleType(
      ComputedStyleInitialValues::InitialListStyleType());
}

void ListStyleType::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetListStyleType(state.ParentStyle()->GetListStyleType());
}

void ListStyleType::ApplyValue(StyleResolverState& state,
                               const CSSValue& value) const {
  ApplyValue(state, ScopedCSSValue(value, nullptr));
}

void ListStyleType::ApplyValue(StyleResolverState& state,
                               const ScopedCSSValue& scoped_value) const {
  const CSSValue& value = scoped_value.GetCSSValue();
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(CSSValueID::kNone, identifier_value->GetValueID());
    state.Style()->SetListStyleType(nullptr);
    return;
  }

  if (const auto* string_value = DynamicTo<CSSStringValue>(value)) {
    state.Style()->SetListStyleType(
        ListStyleTypeData::CreateString(AtomicString(string_value->Value())));
    return;
  }

  DCHECK(value.IsCustomIdentValue());
  const auto& custom_ident_value = To<CSSCustomIdentValue>(value);
  state.Style()->SetListStyleType(ListStyleTypeData::CreateCounterStyle(
      custom_ident_value.Value(), scoped_value.GetTreeScope()));
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeUrl(range, context);
}

const CSSValue* MarkerEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGResource(svg_style.MarkerEndResource());
}

const CSSValue* MarkerMid::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeUrl(range, context);
}

const CSSValue* MarkerMid::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGResource(svg_style.MarkerMidResource());
}

const CSSValue* MarkerStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeUrl(range, context);
}

const CSSValue* MarkerStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGResource(
      svg_style.MarkerStartResource());
}

const CSSValue* Mask::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeUrl(range, context);
}

const CSSValue* Mask::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGResource(svg_style.MaskerResource());
}

const CSSValue* MaskType::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.MaskType());
}

const CSSValue* MathShift::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.MathShift());
}

const CSSValue* MathStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.MathDepth(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

void MathDepth::ApplyValue(StyleResolverState& state,
                           const CSSValue& value) const {
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    DCHECK_EQ(list->length(), 1U);
    const auto& relative_value = To<CSSPrimitiveValue>(list->Item(0));
    state.Style()->SetMathDepth(state.ParentStyle()->MathDepth() +
                                relative_value.GetIntValue());
  } else if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK(identifier_value->GetValueID() == CSSValueID::kAutoAdd);
    unsigned depth = 0;
    if (state.ParentStyle()->MathStyle() == EMathStyle::kCompact)
      depth += 1;
    state.Style()->SetMathDepth(state.ParentStyle()->MathDepth() + depth);
  } else if (DynamicTo<CSSPrimitiveValue>(value)) {
    state.Style()->SetMathDepth(To<CSSPrimitiveValue>(value).GetIntValue());
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  const Length& max_height = style.MaxHeight();
  if (max_height.IsNone())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  const Length& max_width = style.MaxWidth();
  if (max_width.IsNone())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
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
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (style.MinHeight().IsAuto())
    return ComputedStyleUtils::MinWidthOrMinHeightAuto(style);
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
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (style.MinWidth().IsAuto())
    return ComputedStyleUtils::MinWidthOrMinHeightAuto(style);
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.MinWidth(),
                                                             style);
}

const CSSValue* MixBlendMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetBlendMode());
}

const CSSValue* ObjectFit::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
                         base::Optional<WebFeature>());
}

const CSSValue* ObjectPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return MakeGarbageCollected<CSSValuePair>(
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.ObjectPosition().X(), style),
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.ObjectPosition().Y(), style),
      CSSValuePair::kKeepIdenticalValues);
}

const CSSValue* OffsetAnchor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumePosition(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid,
      base::Optional<WebFeature>());
}

const CSSValue* OffsetAnchor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForPosition(style.OffsetAnchor(), style);
}

const CSSValue* OffsetDistance::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLengthOrPercent(range, context,
                                                   kValueRangeAll);
}

const CSSValue* OffsetDistance::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (const BasicShape* style_motion_path = style.OffsetPath())
    return ValueForBasicShape(style, style_motion_path);
  return CSSIdentifierValue::Create(CSSValueID::kNone);
}

const CSSValue* OffsetPosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  CSSValue* value = css_parsing_utils::ConsumePosition(
      range, context, css_parsing_utils::UnitlessQuirk::kForbid,
      base::Optional<WebFeature>());

  // Count when we receive a valid position other than 'auto'.
  if (value && value->IsValuePair())
    context.Count(WebFeature::kCSSOffsetInEffect);
  return value;
}

const CSSValue* OffsetPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (style.OffsetRotate().type == OffsetRotationType::kAuto)
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.Order(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* OriginTrialTestProperty::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
  if (range.Peek().Id() == CSSValueID::kWebkitFocusRingColor)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color OutlineColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  StyleColor outline_color = style.OutlineColor();
  if (style.ShouldForceColor(outline_color)) {
    return To<Longhand>(GetCSSPropertyInternalForcedOutlineColor())
        .ColorIncludingFallback(false, style);
  }
  return outline_color.Resolve(style.GetCurrentColor(),
                               style.UsedColorScheme());
}

const CSSValue* OutlineColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
             ? cssvalue::CSSColorValue::Create(
                   style.VisitedDependentColor(*this).Rgb())
             : ComputedStyleUtils::CurrentColorOrValidColor(
                   style, outline_color, CSSValuePhase::kUsedValue);
}

const CSSValue* OutlineOffset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(range, context, kValueRangeAll);
}

const CSSValue* OutlineOffset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.OutlineOffset(), style);
}

const CSSValue* OutlineStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.OutlineStyleIsAuto())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  return CSSIdentifierValue::Create(style.OutlineStyle());
}

void OutlineStyle::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetOutlineStyleIsAuto(
      ComputedStyleInitialValues::InitialOutlineStyleIsAuto());
  state.Style()->SetOutlineStyle(EBorderStyle::kNone);
}

void OutlineStyle::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetOutlineStyleIsAuto(
      state.ParentStyle()->OutlineStyleIsAuto());
  state.Style()->SetOutlineStyle(state.ParentStyle()->OutlineStyle());
}

void OutlineStyle::ApplyValue(StyleResolverState& state,
                              const CSSValue& value) const {
  const auto& identifier_value = To<CSSIdentifierValue>(value);
  state.Style()->SetOutlineStyleIsAuto(
      static_cast<bool>(identifier_value.ConvertTo<OutlineIsAuto>()));
  state.Style()->SetOutlineStyle(identifier_value.ConvertTo<EBorderStyle>());
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.OutlineWidth(), style);
}

const CSSValue* OverflowAnchor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverflowAnchor());
}

const CSSValue* OverflowClipMargin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.OverflowClipMargin(), style);
}

const CSSValue* OverflowClipMargin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(range, context,
                                          kValueRangeNonNegative);
}

const CSSValue* OverflowWrap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverflowWrap());
}

const CSSValue* OverflowX::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverflowX());
}

const CSSValue* OverflowY::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverflowY());
}

const CSSValue* OverscrollBehaviorX::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverscrollBehaviorX());
}

const CSSValue* OverscrollBehaviorY::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  return ConsumeLengthOrPercent(range, context, kValueRangeNonNegative,
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
  return ConsumeLengthOrPercent(range, context, kValueRangeNonNegative,
                                css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* PaddingBottom::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(range, context, kValueRangeNonNegative,
                                css_parsing_utils::UnitlessQuirk::kAllow);
}

bool PaddingBottom::IsLayoutDependent(const ComputedStyle* style,
                                      LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingBottom().IsFixed());
}

const CSSValue* PaddingBottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  return ConsumeLengthOrPercent(range, context, kValueRangeNonNegative,
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
  return ConsumeLengthOrPercent(range, context, kValueRangeNonNegative,
                                css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* PaddingLeft::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(range, context, kValueRangeNonNegative,
                                css_parsing_utils::UnitlessQuirk::kAllow);
}

bool PaddingLeft::IsLayoutDependent(const ComputedStyle* style,
                                    LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingLeft().IsFixed());
}

const CSSValue* PaddingLeft::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  return ConsumeLengthOrPercent(range, context, kValueRangeNonNegative,
                                css_parsing_utils::UnitlessQuirk::kAllow);
}

bool PaddingRight::IsLayoutDependent(const ComputedStyle* style,
                                     LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingRight().IsFixed());
}

const CSSValue* PaddingRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  return ConsumeLengthOrPercent(range, context, kValueRangeNonNegative,
                                css_parsing_utils::UnitlessQuirk::kAllow);
}

bool PaddingTop::IsLayoutDependent(const ComputedStyle* style,
                                   LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingTop().IsFixed());
}

const CSSValue* PaddingTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeCustomIdent(range, context);
}

const CSSValue* Page::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.Page().IsNull())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  return MakeGarbageCollected<CSSCustomIdentValue>(style.Page());
}

const CSSValue* PaintOrder::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNormal)
    return css_parsing_utils::ConsumeIdent(range);

  Vector<CSSValueID, 3> paint_type_list;
  CSSIdentifierValue* fill = nullptr;
  CSSIdentifierValue* stroke = nullptr;
  CSSIdentifierValue* markers = nullptr;
  do {
    CSSValueID id = range.Peek().Id();
    if (id == CSSValueID::kFill && !fill)
      fill = css_parsing_utils::ConsumeIdent(range);
    else if (id == CSSValueID::kStroke && !stroke)
      stroke = css_parsing_utils::ConsumeIdent(range);
    else if (id == CSSValueID::kMarkers && !markers)
      markers = css_parsing_utils::ConsumeIdent(range);
    else
      return nullptr;
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
        if (paint_type_list.at(1) == CSSValueID::kMarkers)
          paint_order_list->Append(*markers);
      }
      break;
    case CSSValueID::kMarkers:
      paint_order_list->Append(*markers);
      if (paint_type_list.size() > 1) {
        if (paint_type_list.at(1) == CSSValueID::kStroke)
          paint_order_list->Append(*stroke);
      }
      break;
    default:
      NOTREACHED();
  }

  return paint_order_list;
}

const CSSValue* PaintOrder::CSSValueFromComputedStyleInternal(
    const ComputedStyle&,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const EPaintOrder paint_order = svg_style.PaintOrder();
  if (paint_order == kPaintOrderNormal)
    return CSSIdentifierValue::Create(CSSValueID::kNormal);

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
  DCHECK_LT(static_cast<size_t>(paint_order) - 1, base::size(canonical_form));
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (const auto& keyword : canonical_form[paint_order - 1]) {
    const auto paint_order_type = static_cast<EPaintOrderType>(keyword);
    if (paint_order_type == PT_NONE)
      break;
    list->Append(*CSSIdentifierValue::Create(paint_order_type));
  }
  return list;
}

const CSSValue* Perspective::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& localContext) const {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);
  CSSPrimitiveValue* parsed_value =
      css_parsing_utils::ConsumeLength(range, context, kValueRangeAll);
  bool use_legacy_parsing = localContext.UseAliasParsing();
  if (!parsed_value && use_legacy_parsing) {
    double perspective;
    if (!css_parsing_utils::ConsumeNumberRaw(range, context, perspective))
      return nullptr;
    context.Count(WebFeature::kUnitlessPerspectiveInPerspectiveProperty);
    parsed_value = CSSNumericLiteralValue::Create(
        perspective, CSSPrimitiveValue::UnitType::kPixels);
  }
  if (parsed_value &&
      (parsed_value->IsCalculated() || parsed_value->GetDoubleValue() > 0))
    return parsed_value;
  return nullptr;
}

const CSSValue* Perspective::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.HasPerspective())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  return ZoomAdjustedPixelValue(style.Perspective(), style);
}

const CSSValue* PerspectiveOrigin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumePosition(range, context,
                         css_parsing_utils::UnitlessQuirk::kForbid,
                         base::Optional<WebFeature>());
}

bool PerspectiveOrigin::IsLayoutDependent(const ComputedStyle* style,
                                          LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* PerspectiveOrigin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (layout_object) {
    LayoutRect box;
    if (layout_object->IsBox())
      box = To<LayoutBox>(layout_object)->BorderBoxRect();

    return MakeGarbageCollected<CSSValuePair>(
        ZoomAdjustedPixelValue(
            MinimumValueForLength(style.PerspectiveOriginX(), box.Width()),
            style),
        ZoomAdjustedPixelValue(
            MinimumValueForLength(style.PerspectiveOriginY(), box.Height()),
            style),
        CSSValuePair::kKeepIdenticalValues);
  } else {
    return MakeGarbageCollected<CSSValuePair>(
        ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            style.PerspectiveOriginX(), style),
        ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
            style.PerspectiveOriginY(), style),
        CSSValuePair::kKeepIdenticalValues);
  }
}

const CSSValue* PointerEvents::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.PointerEvents());
}

const CSSValue* Position::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetPosition());
}

void Position::ApplyInherit(StyleResolverState& state) const {
  if (!state.ParentNode()->IsDocumentNode())
    state.Style()->SetPosition(state.ParentStyle()->GetPosition());
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
    if (!parsed_value)
      return nullptr;
    values->Append(*parsed_value);
  }
  if (values->length() && values->length() % 2 == 0)
    return values;
  return nullptr;
}

const CSSValue* Quotes::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.Quotes())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
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
      range, context, kValueRangeNonNegative);
}

const CSSValue* R::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(svg_style.R(),
                                                             style);
}

const CSSValue* Resize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  state.Style()->SetResize(r);
}

const CSSValue* Right::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return css_parsing_utils::ConsumeMarginOrOffset(
      range, context,
      css_parsing_utils::UnitlessUnlessShorthand(local_context));
}

bool Right::IsLayoutDependent(const ComputedStyle* style,
                              LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Right::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  if (id == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  CSSValue* rotation = css_parsing_utils::ConsumeAngle(
      range, context, base::Optional<WebFeature>());

  CSSValue* axis = css_parsing_utils::ConsumeAxis(range, context);
  if (axis)
    list->Append(*axis);
  else if (!rotation)
    return nullptr;

  if (!rotation) {
    rotation = css_parsing_utils::ConsumeAngle(range, context,
                                               base::Optional<WebFeature>());
    if (!rotation)
      return nullptr;
  }
  list->Append(*rotation);

  return list;
}

const CSSValue* Rotate::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.Rotate())
    return CSSIdentifierValue::Create(CSSValueID::kNone);

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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool) const {
  return ComputedStyleUtils::ValueForGapLength(style.RowGap(), style);
}

const CSSValue* Rx::ParseSingleValue(CSSParserTokenRange& range,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      range, context, kValueRangeNonNegative);
}

const CSSValue* Rx::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(svg_style.Rx(),
                                                             style);
}

const CSSValue* Ry::ParseSingleValue(CSSParserTokenRange& range,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(
      range, context, kValueRangeNonNegative);
}

const CSSValue* Ry::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(svg_style.Ry(),
                                                             style);
}

const CSSValue* Scale::ParseSingleValue(CSSParserTokenRange& range,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSIndependentTransformPropertiesEnabled());

  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);

  CSSValue* x_scale =
      css_parsing_utils::ConsumeNumber(range, context, kValueRangeAll);
  if (!x_scale)
    return nullptr;

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*x_scale);

  CSSValue* y_scale =
      css_parsing_utils::ConsumeNumber(range, context, kValueRangeAll);
  if (y_scale) {
    CSSValue* z_scale =
        css_parsing_utils::ConsumeNumber(range, context, kValueRangeAll);
    if (z_scale) {
      list->Append(*y_scale);
      list->Append(*z_scale);
    } else if (To<CSSPrimitiveValue>(x_scale)->GetDoubleValue() !=
               To<CSSPrimitiveValue>(y_scale)->GetDoubleValue()) {
      list->Append(*y_scale);
    }
  }

  return list;
}

const CSSValue* Scale::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  ScaleTransformOperation* scale = style.Scale();
  if (!scale)
    return CSSIdentifierValue::Create(CSSValueID::kNone);

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
// auto | [ stable | always ] && both? && force?
const CSSValue* ScrollbarGutter::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (!RuntimeEnabledFeatures::ScrollbarGutterEnabled())
    return nullptr;

  if (auto* value = css_parsing_utils::ConsumeIdent<CSSValueID::kAuto>(range))
    return value;

  CSSIdentifierValue* stable_or_always = nullptr;
  CSSIdentifierValue* both = nullptr;
  CSSIdentifierValue* force = nullptr;

  while (!range.AtEnd()) {
    if (!stable_or_always) {
      if ((stable_or_always =
               css_parsing_utils::ConsumeIdent<CSSValueID::kStable,
                                               CSSValueID::kAlways>(range)))
        continue;
    }
    CSSValueID id = range.Peek().Id();
    if (id == CSSValueID::kBoth && !both)
      both = css_parsing_utils::ConsumeIdent(range);
    else if (id == CSSValueID::kForce && !force)
      force = css_parsing_utils::ConsumeIdent(range);
    else
      return nullptr;
  }
  if (!stable_or_always)
    return nullptr;
  if (both || force) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    list->Append(*stable_or_always);
    if (both)
      list->Append(*both);
    if (force)
      list->Append(*force);
    return list;
  }
  return stable_or_always;
}

const CSSValue* ScrollbarGutter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  auto scrollbar_gutter = style.ScrollbarGutter();
  if (scrollbar_gutter == kScrollbarGutterAuto)
    return CSSIdentifierValue::Create(CSSValueID::kAuto);

  DCHECK(scrollbar_gutter & (kScrollbarGutterStable | kScrollbarGutterAlways));

  CSSValue* main_value = nullptr;
  if (scrollbar_gutter & kScrollbarGutterStable)
    main_value = CSSIdentifierValue::Create(CSSValueID::kStable);
  else
    main_value = CSSIdentifierValue::Create(CSSValueID::kAlways);

  if (!(scrollbar_gutter & (kScrollbarGutterBoth | kScrollbarGutterForce)))
    return main_value;

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*main_value);
  if (scrollbar_gutter & kScrollbarGutterBoth)
    list->Append(*CSSIdentifierValue::Create(kScrollbarGutterBoth));
  if (scrollbar_gutter & kScrollbarGutterForce)
    list->Append(*CSSIdentifierValue::Create(kScrollbarGutterForce));
  return list;
}

const CSSValue* ScrollbarWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ScrollbarWidth());
}

const CSSValue* ScrollBehavior::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  if (!ConsumePan(range, &pan_x, &pan_y))
    return nullptr;
  if (!range.AtEnd() && !ConsumePan(range, &pan_x, &pan_y))
    return nullptr;

  if (pan_x)
    list->Append(*pan_x);
  if (pan_y)
    list->Append(*pan_y);
  return list;
}

const CSSValue* ScrollCustomization::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ScrollCustomizationFlagsToCSSValue(
      style.ScrollCustomization());
}

const CSSValue* ScrollMarginBlockEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, kValueRangeAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginBlockStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, kValueRangeAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginBottom::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, kValueRangeAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginBottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginBottom(), style);
}

const CSSValue* ScrollMarginInlineEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, kValueRangeAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginInlineStart::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, kValueRangeAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginLeft::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, kValueRangeAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginLeft::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginLeft(), style);
}

const CSSValue* ScrollMarginRight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, kValueRangeAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginRight(), style);
}

const CSSValue* ScrollMarginTop::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context, kValueRangeAll,
                       css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
  if (!block_value)
    return nullptr;
  if (range.AtEnd())
    return block_value;

  CSSValue* inline_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kNone, CSSValueID::kStart,
                                      CSSValueID::kEnd, CSSValueID::kCenter>(
          range);
  if (!inline_value)
    return block_value;
  auto* pair = MakeGarbageCollected<CSSValuePair>(
      block_value, inline_value, CSSValuePair::kDropIdenticalValues);
  return pair;
}

const CSSValue* ScrollSnapAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForScrollSnapAlign(style.GetScrollSnapAlign(),
                                                     style);
}

const CSSValue* ScrollSnapStop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
      axis_id != CSSValueID::kInline && axis_id != CSSValueID::kBoth)
    return nullptr;
  CSSValue* axis_value = css_parsing_utils::ConsumeIdent(range);
  if (range.AtEnd() || axis_id == CSSValueID::kNone)
    return axis_value;

  CSSValueID strictness_id = range.Peek().Id();
  if (strictness_id != CSSValueID::kProximity &&
      strictness_id != CSSValueID::kMandatory)
    return axis_value;
  CSSValue* strictness_value = css_parsing_utils::ConsumeIdent(range);
  if (strictness_id == CSSValueID::kProximity)
    return axis_value;  // Shortest serialization.
  auto* pair = MakeGarbageCollected<CSSValuePair>(
      axis_value, strictness_value, CSSValuePair::kDropIdenticalValues);
  return pair;
}

const CSSValue* ScrollSnapType::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForScrollSnapType(style.GetScrollSnapType(),
                                                    style);
}

const CSSValue* ShapeImageThreshold::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(range, context);
}

const CSSValue* ShapeImageThreshold::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.ShapeImageThreshold(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* ShapeMargin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLengthOrPercent(range, context,
                                                   kValueRangeNonNegative);
}

const CSSValue* ShapeMargin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSValue::Create(style.ShapeMargin(), style.EffectiveZoom());
}

const CSSValue* ShapeOutside::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (CSSValue* image_value =
          css_parsing_utils::ConsumeImageOrNone(range, context))
    return image_value;
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  CSSValue* box_value = css_parsing_utils::ConsumeShapeBox(range);
  if (CSSValue* shape_value = css_parsing_utils::ConsumeBasicShape(
          range, context, css_parsing_utils::AllowPathValue::kForbid)) {
    list->Append(*shape_value);
    if (!box_value) {
      box_value = css_parsing_utils::ConsumeShapeBox(range);
    }
  }
  if (box_value)
    list->Append(*box_value);
  if (!list->length())
    return nullptr;
  return list;
}

const CSSValue* ShapeOutside::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForShape(style, allow_visited_style,
                                           style.ShapeOutside());
}

const CSSValue* ShapeRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.ShapeRendering());
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
static FloatSize GetPageSizeFromName(const CSSIdentifierValue& page_size_name) {
  switch (page_size_name.GetValueID()) {
    case CSSValueID::kA5:
      return FloatSize(MmToPx(148), MmToPx(210));
    case CSSValueID::kA4:
      return FloatSize(MmToPx(210), MmToPx(297));
    case CSSValueID::kA3:
      return FloatSize(MmToPx(297), MmToPx(420));
    case CSSValueID::kB5:
      return FloatSize(MmToPx(176), MmToPx(250));
    case CSSValueID::kB4:
      return FloatSize(MmToPx(250), MmToPx(353));
    case CSSValueID::kJisB5:
      return FloatSize(MmToPx(182), MmToPx(257));
    case CSSValueID::kJisB4:
      return FloatSize(MmToPx(257), MmToPx(364));
    case CSSValueID::kLetter:
      return FloatSize(InchToPx(8.5), InchToPx(11));
    case CSSValueID::kLegal:
      return FloatSize(InchToPx(8.5), InchToPx(14));
    case CSSValueID::kLedger:
      return FloatSize(InchToPx(11), InchToPx(17));
    default:
      NOTREACHED();
      return FloatSize(0, 0);
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
          range, context, kValueRangeNonNegative)) {
    CSSValue* height = css_parsing_utils::ConsumeLength(range, context,
                                                        kValueRangeNonNegative);
    result->Append(*width);
    if (height)
      result->Append(*height);
    return result;
  }

  CSSValue* page_size = ConsumePageSize(range);
  CSSValue* orientation =
      css_parsing_utils::ConsumeIdent<CSSValueID::kPortrait,
                                      CSSValueID::kLandscape>(range);
  if (!page_size)
    page_size = ConsumePageSize(range);

  if (!orientation && !page_size)
    return nullptr;
  if (page_size)
    result->Append(*page_size);
  if (orientation)
    result->Append(*orientation);
  return result;
}

void Size::ApplyInitial(StyleResolverState& state) const {}

void Size::ApplyInherit(StyleResolverState& state) const {}

void Size::ApplyValue(StyleResolverState& state, const CSSValue& value) const {
  state.Style()->ResetPageSizeType();
  FloatSize size;
  PageSizeType page_size_type = PageSizeType::kAuto;
  const auto& list = To<CSSValueList>(value);
  if (list.length() == 2) {
    // <length>{2} | <page-size> <orientation>
    const CSSValue& first = list.Item(0);
    const CSSValue& second = list.Item(1);
    auto* first_primitive_value = DynamicTo<CSSPrimitiveValue>(first);
    if (first_primitive_value && first_primitive_value->IsLength()) {
      // <length>{2}
      size = FloatSize(
          first_primitive_value->ComputeLength<float>(
              state.CssToLengthConversionData().CopyWithAdjustedZoom(1.0)),
          To<CSSPrimitiveValue>(second).ComputeLength<float>(
              state.CssToLengthConversionData().CopyWithAdjustedZoom(1.0)));
    } else {
      // <page-size> <orientation>
      size = GetPageSizeFromName(To<CSSIdentifierValue>(first));

      DCHECK(To<CSSIdentifierValue>(second).GetValueID() ==
                 CSSValueID::kLandscape ||
             To<CSSIdentifierValue>(second).GetValueID() ==
                 CSSValueID::kPortrait);
      if (To<CSSIdentifierValue>(second).GetValueID() == CSSValueID::kLandscape)
        size = size.TransposedSize();
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
          state.CssToLengthConversionData().CopyWithAdjustedZoom(1.0));
      size = FloatSize(width, width);
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
  state.Style()->SetPageSizeType(page_size_type);
  state.Style()->SetPageSize(size);
}

const CSSValue* Speak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const ComputedStyle& style) const {
  return style.ResolvedColor(style.StopColor());
}

const CSSValue* StopColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const ComputedStyle&,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(svg_style.StopOpacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* Stroke::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return css_parsing_utils::ParsePaintStroke(range, context);
}

const CSSValue* Stroke::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForSVGPaint(svg_style.StrokePaint(), style);
}

const blink::Color Stroke::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  DCHECK(style.SvgStyle().StrokePaint().HasColor());
  StyleColor stroke_color = style.SvgStyle().StrokePaint().GetColor();
  if (style.ShouldForceColor(stroke_color))
    return style.GetCurrentColor();
  return stroke_color.Resolve(style.GetCurrentColor(), style.UsedColorScheme());
}

const CSSValue* StrokeDasharray::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);

  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  CSSValueList* dashes = CSSValueList::CreateCommaSeparated();
  do {
    CSSPrimitiveValue* dash = css_parsing_utils::ConsumeLengthOrPercent(
        range, context, kValueRangeNonNegative);
    if (!dash || (css_parsing_utils::ConsumeCommaIncludingWhitespace(range) &&
                  range.AtEnd()))
      return nullptr;
    dashes->Append(*dash);
  } while (!range.AtEnd());
  return dashes;
}

const CSSValue* StrokeDasharray::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::StrokeDashArrayToCSSValueList(
      *svg_style.StrokeDashArray(), style);
}

const CSSValue* StrokeDashoffset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  return css_parsing_utils::ConsumeLengthOrPercent(
      range, context, kValueRangeAll,
      css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* StrokeDashoffset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.StrokeDashOffset(), style);
}

const CSSValue* StrokeLinecap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.CapStyle());
}

const CSSValue* StrokeLinejoin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.JoinStyle());
}

const CSSValue* StrokeMiterlimit::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(range, context,
                                          kValueRangeNonNegative);
}

const CSSValue* StrokeMiterlimit::CSSValueFromComputedStyleInternal(
    const ComputedStyle&,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(svg_style.StrokeMiterLimit(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* StrokeOpacity::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeAlphaValue(range, context);
}

const CSSValue* StrokeOpacity::CSSValueFromComputedStyleInternal(
    const ComputedStyle&,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(svg_style.StrokeOpacity(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* StrokeWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSParserContext::ParserModeOverridingScope scope(context, kSVGAttributeMode);
  return css_parsing_utils::ConsumeLengthOrPercent(
      range, context, kValueRangeNonNegative,
      css_parsing_utils::UnitlessQuirk::kForbid);
}

const CSSValue* StrokeWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  // We store the unzoomed stroke-width value using ConvertUnzoomedLength().
  // Don't apply zoom here either.
  return CSSValue::Create(svg_style.StrokeWidth().length(), 1);
}

const CSSValue* ContentVisibility::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.ContentVisibility());
}

const CSSValue* ContentVisibility::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kHiddenMatchable &&
      !RuntimeEnabledFeatures::CSSContentVisibilityHiddenMatchableEnabled(
          context.GetExecutionContext())) {
    return nullptr;
  }
  return css_parsing_utils::ConsumeIdent<CSSValueID::kVisible,
                                         CSSValueID::kAuto, CSSValueID::kHidden,
                                         CSSValueID::kHiddenMatchable>(range);
}

const CSSValue* TabSize::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSPrimitiveValue* parsed_value =
      css_parsing_utils::ConsumeNumber(range, context, kValueRangeNonNegative);
  if (parsed_value)
    return parsed_value;
  return css_parsing_utils::ConsumeLength(range, context,
                                          kValueRangeNonNegative);
}

const CSSValue* TabSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(
      style.GetTabSize().GetPixelSize(1.0),
      style.GetTabSize().IsSpaces() ? CSSPrimitiveValue::UnitType::kNumber
                                    : CSSPrimitiveValue::UnitType::kPixels);
}

const CSSValue* TableLayout::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.TableLayout());
}

const CSSValue* TextAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetTextAlign());
}

void TextAlign::ApplyValue(StyleResolverState& state,
                           const CSSValue& value) const {
  const auto* ident_value = DynamicTo<CSSIdentifierValue>(value);
  if (ident_value &&
      ident_value->GetValueID() != CSSValueID::kWebkitMatchParent) {
    // Special case for th elements - UA stylesheet text-align does not apply if
    // parent's computed value for text-align is not its initial value
    // https://html.spec.whatwg.org/C/#tables-2
    if (ident_value->GetValueID() == CSSValueID::kInternalCenter &&
        state.ParentStyle()->GetTextAlign() !=
            ComputedStyleInitialValues::InitialTextAlign())
      state.Style()->SetTextAlign(state.ParentStyle()->GetTextAlign());
    else
      state.Style()->SetTextAlign(ident_value->ConvertTo<ETextAlign>());
  } else if (state.ParentStyle()->GetTextAlign() == ETextAlign::kStart) {
    state.Style()->SetTextAlign(state.ParentStyle()->IsLeftToRightDirection()
                                    ? ETextAlign::kLeft
                                    : ETextAlign::kRight);
  } else if (state.ParentStyle()->GetTextAlign() == ETextAlign::kEnd) {
    state.Style()->SetTextAlign(state.ParentStyle()->IsLeftToRightDirection()
                                    ? ETextAlign::kRight
                                    : ETextAlign::kLeft);
  } else {
    state.Style()->SetTextAlign(state.ParentStyle()->GetTextAlign());
  }
}

const CSSValue* TextAlignLast::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.TextAlignLast());
}

const CSSValue* TextAnchor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.TextAnchor());
}

const CSSValue* TextCombineUpright::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  StyleColor decoration_color =
      style.DecorationColorIncludingFallback(visited_link);
  if (style.ShouldForceColor(decoration_color))
    return style.GetCurrentColor();
  return decoration_color.Resolve(style.GetCurrentColor(),
                                  style.UsedColorScheme());
}

const CSSValue* TextDecorationColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::RenderTextDecorationFlagsToCSSValue(
      style.GetTextDecoration());
}

const CSSValue* TextDecorationSkipInk::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForTextDecorationSkipInk(
      style.TextDecorationSkipInk());
}

const CSSValue* TextDecorationStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForTextDecorationStyle(
      style.TextDecorationStyle());
}

const CSSValue* TextDecorationThickness::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::UnderlineOffsetThicknessEnabled());
  if (auto* ident = css_parsing_utils::ConsumeIdent<CSSValueID::kFromFont,
                                                    CSSValueID::kAuto>(range)) {
    return ident;
  }
  return css_parsing_utils::ConsumeLengthOrPercent(range, context,
                                                   kValueRangeAll);
}

const CSSValue* TextDecorationThickness::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  DCHECK(RuntimeEnabledFeatures::UnderlineOffsetThicknessEnabled());

  if (style.GetTextDecorationThickness().IsFromFont())
    return CSSIdentifierValue::Create(CSSValueID::kFromFont);

  if (style.GetTextDecorationThickness().IsAuto())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);

  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.GetTextDecorationThickness().Thickness(), style);
}

const CSSValue* TextIndent::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // [ <length> | <percentage> ] && hanging? && each-line?
  // Keywords only allowed when css3Text is enabled.
  CSSValue* length_percentage = nullptr;
  CSSValue* hanging = nullptr;
  CSSValue* each_line = nullptr;
  do {
    if (!length_percentage) {
      length_percentage = css_parsing_utils::ConsumeLengthOrPercent(
          range, context, kValueRangeAll,
          css_parsing_utils::UnitlessQuirk::kAllow);
      if (length_percentage) {
        continue;
      }
    }

    if (RuntimeEnabledFeatures::CSS3TextEnabled()) {
      CSSValueID id = range.Peek().Id();
      if (!hanging && id == CSSValueID::kHanging) {
        hanging = css_parsing_utils::ConsumeIdent(range);
        continue;
      }
      if (!each_line && id == CSSValueID::kEachLine) {
        each_line = css_parsing_utils::ConsumeIdent(range);
        continue;
      }
    }
    return nullptr;
  } while (!range.AtEnd());

  if (!length_percentage)
    return nullptr;
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*length_percentage);

  if (hanging)
    list->Append(*hanging);

  if (each_line)
    list->Append(*each_line);

  return list;
}

const CSSValue* TextIndent::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.TextIndent(), style));
  if (RuntimeEnabledFeatures::CSS3TextEnabled()) {
    if (style.GetTextIndentType() == TextIndentType::kHanging)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kHanging));
    if (style.GetTextIndentLine() == TextIndentLine::kEachLine)
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kEachLine));
  }
  return list;
}

void TextIndent::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetTextIndent(ComputedStyleInitialValues::InitialTextIndent());
  state.Style()->SetTextIndentLine(
      ComputedStyleInitialValues::InitialTextIndentLine());
  state.Style()->SetTextIndentType(
      ComputedStyleInitialValues::InitialTextIndentType());
}

void TextIndent::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetTextIndent(state.ParentStyle()->TextIndent());
  state.Style()->SetTextIndentLine(state.ParentStyle()->GetTextIndentLine());
  state.Style()->SetTextIndentType(state.ParentStyle()->GetTextIndentType());
}

void TextIndent::ApplyValue(StyleResolverState& state,
                            const CSSValue& value) const {
  Length length_or_percentage_value;
  TextIndentLine text_indent_line_value =
      ComputedStyleInitialValues::InitialTextIndentLine();
  TextIndentType text_indent_type_value =
      ComputedStyleInitialValues::InitialTextIndentType();

  for (auto& list_value : To<CSSValueList>(value)) {
    if (auto* list_primitive_value =
            DynamicTo<CSSPrimitiveValue>(*list_value)) {
      length_or_percentage_value = list_primitive_value->ConvertToLength(
          state.CssToLengthConversionData());
    } else if (To<CSSIdentifierValue>(*list_value).GetValueID() ==
               CSSValueID::kEachLine) {
      text_indent_line_value = TextIndentLine::kEachLine;
    } else if (To<CSSIdentifierValue>(*list_value).GetValueID() ==
               CSSValueID::kHanging) {
      text_indent_type_value = TextIndentType::kHanging;
    } else {
      NOTREACHED();
    }
  }

  state.Style()->SetTextIndent(length_or_percentage_value);
  state.Style()->SetTextIndentLine(text_indent_line_value);
  state.Style()->SetTextIndentType(text_indent_type_value);
}

const CSSValue* TextJustify::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetTextJustify());
}

const CSSValue* TextOrientation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.TextOverflow() != ETextOverflow::kClip)
    return CSSIdentifierValue::Create(CSSValueID::kEllipsis);
  return CSSIdentifierValue::Create(CSSValueID::kClip);
}

const CSSValue* TextRendering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForShadowList(
      style.TextShadow(), style, false, CSSValuePhase::kComputedValue);
}

const CSSValue* TextSizeAdjust::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumePercent(range, context,
                                           kValueRangeNonNegative);
}

const CSSValue* TextSizeAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.GetTextSizeAdjust().IsAuto())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  return CSSNumericLiteralValue::Create(
      style.GetTextSizeAdjust().Multiplier() * 100,
      CSSPrimitiveValue::UnitType::kPercentage);
}

const CSSValue* TextTransform::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);

  CSSIdentifierValue* from_font_or_under_value =
      RuntimeEnabledFeatures::UnderlineOffsetThicknessEnabled()
          ? css_parsing_utils::ConsumeIdent<CSSValueID::kFromFont,
                                            CSSValueID::kUnder>(range)
          : css_parsing_utils::ConsumeIdent<CSSValueID::kUnder>(range);
  CSSIdentifierValue* left_or_right_value =
      css_parsing_utils::ConsumeIdent<CSSValueID::kLeft, CSSValueID::kRight>(
          range);
  if (left_or_right_value && !from_font_or_under_value) {
    from_font_or_under_value =
        RuntimeEnabledFeatures::UnderlineOffsetThicknessEnabled()
            ? css_parsing_utils::ConsumeIdent<CSSValueID::kFromFont,
                                              CSSValueID::kUnder>(range)
            : css_parsing_utils::ConsumeIdent<CSSValueID::kUnder>(range);
  }
  if (!from_font_or_under_value && !left_or_right_value)
    return nullptr;
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (from_font_or_under_value)
    list->Append(*from_font_or_under_value);
  if (left_or_right_value)
    list->Append(*left_or_right_value);
  return list;
}

const CSSValue* TextUnderlinePosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  auto text_underline_position = style.TextUnderlinePosition();
  if (text_underline_position == kTextUnderlinePositionAuto)
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  if (text_underline_position == kTextUnderlinePositionFromFont)
    return CSSIdentifierValue::Create(CSSValueID::kFromFont);
  if (text_underline_position == kTextUnderlinePositionUnder)
    return CSSIdentifierValue::Create(CSSValueID::kUnder);
  if (text_underline_position == kTextUnderlinePositionLeft)
    return CSSIdentifierValue::Create(CSSValueID::kLeft);
  if (text_underline_position == kTextUnderlinePositionRight)
    return CSSIdentifierValue::Create(CSSValueID::kRight);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (text_underline_position & kTextUnderlinePositionFromFont) {
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kFromFont));
  } else {
    DCHECK(text_underline_position & kTextUnderlinePositionUnder);
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnder));
  }
  if (text_underline_position & kTextUnderlinePositionLeft)
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kLeft));
  if (text_underline_position & kTextUnderlinePositionRight)
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kRight));
  DCHECK_EQ(list->length(), 2U);
  return list;
}

const CSSValue* TextUnderlineOffset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::UnderlineOffsetThicknessEnabled());
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeLengthOrPercent(range, context,
                                                   kValueRangeAll);
}

const CSSValue* TextUnderlineOffset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
      range, context,
      css_parsing_utils::UnitlessUnlessShorthand(local_context));
}

bool Top::IsLayoutDependent(const ComputedStyle* style,
                            LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Top::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  if (!ConsumePan(range, pan_x, pan_y, pinch_zoom))
    return nullptr;
  if (!range.AtEnd() && !ConsumePan(range, pan_x, pan_y, pinch_zoom))
    return nullptr;
  if (!range.AtEnd() && !ConsumePan(range, pan_x, pan_y, pinch_zoom))
    return nullptr;

  if (pan_x)
    list->Append(*pan_x);
  if (pan_y)
    list->Append(*pan_y);
  if (pinch_zoom)
    list->Append(*pinch_zoom);
  return list;
}

const CSSValue* TouchAction::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::TouchActionFlagsToCSSValue(style.GetTouchAction());
}

const CSSValue* TransformBox::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    CSSValue* result_z =
        css_parsing_utils::ConsumeLength(range, context, kValueRangeAll);
    if (result_z)
      list->Append(*result_z);
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
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (layout_object) {
    FloatRect reference_box = ComputedStyleUtils::ReferenceBoxForTransform(
        *layout_object, ComputedStyleUtils::kDontUsePixelSnappedBox);
    FloatSize resolved_origin(
        FloatValueForLength(style.TransformOriginX(), reference_box.Width()),
        FloatValueForLength(style.TransformOriginY(), reference_box.Height()));
    list->Append(*ZoomAdjustedPixelValue(resolved_origin.Width(), style));
    list->Append(*ZoomAdjustedPixelValue(resolved_origin.Height(), style));
  } else {
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.TransformOriginX(), style));
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.TransformOriginY(), style));
  }
  if (style.TransformOriginZ() != 0)
    list->Append(*ZoomAdjustedPixelValue(style.TransformOriginZ(), style));
  return list;
}

const CSSValue* TransformStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
      css_parsing_utils::ConsumeTime, range, context, kValueRangeAll);
}

const CSSValue* TransitionDelay::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDelay(style.Transitions());
}

const CSSValue* TransitionDelay::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      const Persistent<CSSValue>, value,
      (CSSNumericLiteralValue::Create(CSSTimingData::InitialDelay(),
                                      CSSPrimitiveValue::UnitType::kSeconds)));
  return value;
}

const CSSValue* TransitionDuration::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeTime, range, context, kValueRangeNonNegative);
}

const CSSValue* TransitionDuration::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDuration(style.Transitions());
}

const CSSValue* TransitionDuration::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      const Persistent<CSSValue>, value,
      (CSSNumericLiteralValue::Create(CSSTimingData::InitialDuration(),
                                      CSSPrimitiveValue::UnitType::kSeconds)));
  return value;
}

const CSSValue* TransitionProperty::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueList* list = css_parsing_utils::ConsumeCommaSeparatedList(
      css_parsing_utils::ConsumeTransitionProperty, range, context);
  if (!list || !css_parsing_utils::IsValidPropertyList(*list))
    return nullptr;
  return list;
}

const CSSValue* TransitionProperty::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForTransitionProperty(style.Transitions());
}

const CSSValue* TransitionProperty::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueID::kAll)));
  return value;
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationTimingFunction(
      style.Transitions());
}

const CSSValue* TransitionTimingFunction::InitialValue() const {
  DEFINE_STATIC_LOCAL(const Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueID::kEase)));
  return value;
}

const CSSValue* Translate::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSIndependentTransformPropertiesEnabled());
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);

  CSSValue* translate_x =
      css_parsing_utils::ConsumeLengthOrPercent(range, context, kValueRangeAll);
  if (!translate_x)
    return nullptr;
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*translate_x);
  CSSPrimitiveValue* translate_y =
      css_parsing_utils::ConsumeLengthOrPercent(range, context, kValueRangeAll);
  if (translate_y) {
    CSSValue* translate_z =
        css_parsing_utils::ConsumeLength(range, context, kValueRangeAll);
    if (translate_y->IsZero() && !translate_z)
      return list;

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
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (!style.Translate())
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.Translate()->X(), style));

  if (!style.Translate()->Y().IsZero() || style.Translate()->Z() != 0) {
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.Translate()->Y(), style));
  }

  if (style.Translate()->Z() != 0)
    list->Append(*ZoomAdjustedPixelValue(style.Translate()->Z(), style));

  return list;
}

const CSSValue* UnicodeBidi::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetUnicodeBidi());
}

const CSSValue* UserSelect::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.UserSelect());
}

const CSSValue* VectorEffect::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(svg_style.VectorEffect());
}

const CSSValue* VerticalAlign::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValue* parsed_value = css_parsing_utils::ConsumeIdentRange(
      range, CSSValueID::kBaseline, CSSValueID::kWebkitBaselineMiddle);
  if (!parsed_value) {
    parsed_value = css_parsing_utils::ConsumeLengthOrPercent(
        range, context, kValueRangeAll,
        css_parsing_utils::UnitlessQuirk::kAllow);
  }
  return parsed_value;
}

const CSSValue* VerticalAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  EVerticalAlign vertical_align = state.ParentStyle()->VerticalAlign();
  state.Style()->SetVerticalAlign(vertical_align);
  if (vertical_align == EVerticalAlign::kLength) {
    state.Style()->SetVerticalAlignLength(
        state.ParentStyle()->GetVerticalAlignLength());
  }
}

void VerticalAlign::ApplyValue(StyleResolverState& state,
                               const CSSValue& value) const {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    state.Style()->SetVerticalAlign(
        identifier_value->ConvertTo<EVerticalAlign>());
  } else {
    state.Style()->SetVerticalAlignLength(
        To<CSSPrimitiveValue>(value).ConvertToLength(
            state.CssToLengthConversionData()));
  }
}

const CSSValue* Visibility::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Visibility());
}

const CSSValue* WebkitAppRegion::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.DraggableRegionMode() == EDraggableRegionMode::kNone)
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  return CSSIdentifierValue::Create(style.DraggableRegionMode() ==
                                            EDraggableRegionMode::kDrag
                                        ? CSSValueID::kDrag
                                        : CSSValueID::kNoDrag);
}

void WebkitAppRegion::ApplyInitial(StyleResolverState& state) const {}

void WebkitAppRegion::ApplyInherit(StyleResolverState& state) const {}

void WebkitAppRegion::ApplyValue(StyleResolverState& state,
                                 const CSSValue& value) const {
  const auto& identifier_value = To<CSSIdentifierValue>(value);
  state.Style()->SetDraggableRegionMode(identifier_value.GetValueID() ==
                                                CSSValueID::kDrag
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
    if (local_context.UseAliasParsing())
      property = CSSPropertyID::kAliasWebkitAppearance;
    css_parsing_utils::CountKeywordOnlyPropertyUsage(property, context, id);
    return css_parsing_utils::ConsumeIdent(range);
  }
  return nullptr;
}

const CSSValue* Appearance::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Appearance());
}

const CSSValue* WebkitBorderHorizontalSpacing::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(range, context,
                                          kValueRangeNonNegative);
}

const CSSValue*
WebkitBorderHorizontalSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
  state.Style()->SetBorderImage(image);
}

const CSSValue* WebkitBorderVerticalSpacing::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(range, context,
                                          kValueRangeNonNegative);
}

const CSSValue* WebkitBorderVerticalSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.VerticalBorderSpacing(), style);
}

const CSSValue* WebkitBoxAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BoxAlign());
}

const CSSValue* WebkitBoxDecorationBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.BoxDecorationBreak() == EBoxDecorationBreak::kSlice)
    return CSSIdentifierValue::Create(CSSValueID::kSlice);
  return CSSIdentifierValue::Create(CSSValueID::kClone);
}

const CSSValue* WebkitBoxDirection::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BoxDirection());
}

const CSSValue* WebkitBoxFlex::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeNumber(range, context, kValueRangeAll);
}

const CSSValue* WebkitBoxFlex::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSNumericLiteralValue::Create(style.BoxOrdinalGroup(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* WebkitBoxOrient::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.BoxOrient());
}

const CSSValue* WebkitBoxPack::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  if (!direction)
    return nullptr;

  CSSPrimitiveValue* offset = nullptr;
  if (range.AtEnd()) {
    offset =
        CSSNumericLiteralValue::Create(0, CSSPrimitiveValue::UnitType::kPixels);
  } else {
    offset = ConsumeLengthOrPercent(range, context, kValueRangeAll,
                                    css_parsing_utils::UnitlessQuirk::kForbid);
    if (!offset)
      return nullptr;
  }

  CSSValue* mask = nullptr;
  if (!range.AtEnd()) {
    mask = css_parsing_utils::ConsumeWebkitBorderImage(range, context);
    if (!mask)
      return nullptr;
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
    const SVGComputedStyle&,
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
      range, context, kValueRangeAll, css_parsing_utils::UnitlessQuirk::kAllow);
}

const CSSValue* WebkitFontSmoothing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetFontDescription().FontSmoothing());
}

const CSSValue* WebkitHighlight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeString(range);
}

const CSSValue* WebkitHighlight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.Highlight() == g_null_atom)
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  return MakeGarbageCollected<CSSStringValue>(style.Highlight());
}

const CSSValue* WebkitHyphenateCharacter::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeString(range);
}

const CSSValue* WebkitHyphenateCharacter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HyphenationString().IsNull())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  return MakeGarbageCollected<CSSStringValue>(style.HyphenationString());
}

const CSSValue* WebkitLineBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (!style.HasLineClamp())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  return CSSNumericLiteralValue::Create(style.LineClamp(),
                                        CSSPrimitiveValue::UnitType::kNumber);
}

const CSSValue* WebkitLocale::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeString(range);
}

const CSSValue* WebkitLocale::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.Locale().IsNull())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
  state.Style()->SetMaskBoxImageSource(
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.MaskLayers();
  for (; curr_layer; curr_layer = curr_layer->Next())
    list->Append(*CSSIdentifierValue::Create(curr_layer->Composite()));
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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

const CSSValue* WebkitPerspectiveOriginY::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueID::kTop,
                                                    CSSValueID::kBottom>(
      range, context);
}

const CSSValue* WebkitPrintColorAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.PrintColorAdjust());
}

const CSSValue* WebkitRtlOrdering::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.RtlOrdering() == EOrder::kVisual
                                        ? CSSValueID::kVisual
                                        : CSSValueID::kLogical);
}

const CSSValue* WebkitRubyPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetRubyPosition());
}

const CSSValue* RubyPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const ComputedStyle& style) const {
  StyleColor highlight_color = style.TapHighlightColor();
  if (style.ShouldForceColor(highlight_color)) {
    return visited_link ? style.GetInternalVisitedCurrentColor()
                        : style.GetCurrentColor();
  }
  return style.ResolvedColor(style.TapHighlightColor());
}

const CSSValue* WebkitTapHighlightColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TapHighlightColor(), CSSValuePhase::kComputedValue);
}

const CSSValue* WebkitTextCombine::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.TextCombine() == ETextCombine::kAll)
    return CSSIdentifierValue::Create(CSSValueID::kHorizontal);
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::RenderTextDecorationFlagsToCSSValue(
      style.TextDecorationsInEffect());
}

const CSSValue* WebkitTextEmphasisColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeColor(range, context);
}

const blink::Color WebkitTextEmphasisColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  StyleColor text_emphasis_color = style.TextEmphasisColor();
  if (style.ShouldForceColor(text_emphasis_color))
    return style.GetCurrentColor();
  return text_emphasis_color.Resolve(style.GetCurrentColor(),
                                     style.UsedColorScheme());
}

const CSSValue* WebkitTextEmphasisColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TextEmphasisColor(), CSSValuePhase::kComputedValue);
}

// [ over | under ] && [ right | left ]?
// If [ right | left ] is omitted, it defaults to right.
const CSSValue* WebkitTextEmphasisPosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSIdentifierValue* values[2] = {
      css_parsing_utils::ConsumeIdent<CSSValueID::kOver, CSSValueID::kUnder,
                                      CSSValueID::kRight, CSSValueID::kLeft>(
          range),
      nullptr};
  if (!values[0])
    return nullptr;
  values[1] =
      css_parsing_utils::ConsumeIdent<CSSValueID::kOver, CSSValueID::kUnder,
                                      CSSValueID::kRight, CSSValueID::kLeft>(
          range);
  CSSIdentifierValue* over_under = nullptr;
  CSSIdentifierValue* left_right = nullptr;

  for (auto* value : values) {
    if (!value)
      break;
    switch (value->GetValueID()) {
      case CSSValueID::kOver:
      case CSSValueID::kUnder:
        if (over_under)
          return nullptr;
        over_under = value;
        break;
      case CSSValueID::kLeft:
      case CSSValueID::kRight:
        if (left_right)
          return nullptr;
        left_right = value;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  if (!over_under)
    return nullptr;
  if (!left_right)
    left_right = CSSIdentifierValue::Create(CSSValueID::kRight);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*over_under);
  list->Append(*left_right);
  return list;
}

const CSSValue* WebkitTextEmphasisPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  switch (style.GetTextEmphasisPosition()) {
    case TextEmphasisPosition::kOverRight:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kOver));
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kRight));
      break;
    case TextEmphasisPosition::kOverLeft:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kOver));
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kLeft));
      break;
    case TextEmphasisPosition::kUnderRight:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnder));
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kRight));
      break;
    case TextEmphasisPosition::kUnderLeft:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnder));
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kLeft));
      break;
  }
  return list;
}

const CSSValue* WebkitTextEmphasisStyle::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);

  if (CSSValue* text_emphasis_style = css_parsing_utils::ConsumeString(range))
    return text_emphasis_style;

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
  if (fill)
    return fill;
  if (shape)
    return shape;
  return nullptr;
}

const CSSValue* WebkitTextEmphasisStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
      FALLTHROUGH;
    case TextEmphasisMark::kDot:
    case TextEmphasisMark::kCircle:
    case TextEmphasisMark::kDoubleCircle:
    case TextEmphasisMark::kTriangle:
    case TextEmphasisMark::kSesame: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*CSSIdentifierValue::Create(style.GetTextEmphasisFill()));
      list->Append(*CSSIdentifierValue::Create(style.GetTextEmphasisMark()));
      return list;
    }
  }
  NOTREACHED();
  return nullptr;
}

void WebkitTextEmphasisStyle::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetTextEmphasisFill(
      ComputedStyleInitialValues::InitialTextEmphasisFill());
  state.Style()->SetTextEmphasisMark(
      ComputedStyleInitialValues::InitialTextEmphasisMark());
  state.Style()->SetTextEmphasisCustomMark(
      ComputedStyleInitialValues::InitialTextEmphasisCustomMark());
}

void WebkitTextEmphasisStyle::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetTextEmphasisFill(
      state.ParentStyle()->GetTextEmphasisFill());
  state.Style()->SetTextEmphasisMark(
      state.ParentStyle()->GetTextEmphasisMark());
  state.Style()->SetTextEmphasisCustomMark(
      state.ParentStyle()->TextEmphasisCustomMark());
}

void WebkitTextEmphasisStyle::ApplyValue(StyleResolverState& state,
                                         const CSSValue& value) const {
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    DCHECK_EQ(list->length(), 2U);
    for (unsigned i = 0; i < 2; ++i) {
      const auto& ident_value = To<CSSIdentifierValue>(list->Item(i));
      if (ident_value.GetValueID() == CSSValueID::kFilled ||
          ident_value.GetValueID() == CSSValueID::kOpen) {
        state.Style()->SetTextEmphasisFill(
            ident_value.ConvertTo<TextEmphasisFill>());
      } else {
        state.Style()->SetTextEmphasisMark(
            ident_value.ConvertTo<TextEmphasisMark>());
      }
    }
    state.Style()->SetTextEmphasisCustomMark(g_null_atom);
    return;
  }

  if (auto* string_value = DynamicTo<CSSStringValue>(value)) {
    state.Style()->SetTextEmphasisFill(TextEmphasisFill::kFilled);
    state.Style()->SetTextEmphasisMark(TextEmphasisMark::kCustom);
    state.Style()->SetTextEmphasisCustomMark(
        AtomicString(string_value->Value()));
    return;
  }

  const auto& identifier_value = To<CSSIdentifierValue>(value);

  state.Style()->SetTextEmphasisCustomMark(g_null_atom);

  if (identifier_value.GetValueID() == CSSValueID::kFilled ||
      identifier_value.GetValueID() == CSSValueID::kOpen) {
    state.Style()->SetTextEmphasisFill(
        identifier_value.ConvertTo<TextEmphasisFill>());
    state.Style()->SetTextEmphasisMark(TextEmphasisMark::kAuto);
  } else {
    state.Style()->SetTextEmphasisFill(TextEmphasisFill::kFilled);
    state.Style()->SetTextEmphasisMark(
        identifier_value.ConvertTo<TextEmphasisMark>());
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
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  return style.TextFillColor().Resolve(style.GetCurrentColor(),
                                       style.UsedColorScheme());
}

const CSSValue* WebkitTextFillColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(
      style, style.TextFillColor(), CSSValuePhase::kComputedValue);
}

const CSSValue* WebkitTextOrientation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.GetTextOrientation() == ETextOrientation::kMixed)
    return CSSIdentifierValue::Create(CSSValueID::kVerticalRight);
  return CSSIdentifierValue::Create(style.GetTextOrientation());
}

void WebkitTextOrientation::ApplyInitial(StyleResolverState& state) const {
  state.SetTextOrientation(
      ComputedStyleInitialValues::InitialTextOrientation());
}

void WebkitTextOrientation::ApplyInherit(StyleResolverState& state) const {
  state.SetTextOrientation(state.ParentStyle()->GetTextOrientation());
}

void WebkitTextOrientation::ApplyValue(StyleResolverState& state,
                                       const CSSValue& value) const {
  state.SetTextOrientation(
      To<CSSIdentifierValue>(value).ConvertTo<ETextOrientation>());
}

const CSSValue* WebkitTextSecurity::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const ComputedStyle& style) const {
  DCHECK(!visited_link);
  return style.TextStrokeColor().Resolve(style.GetCurrentColor(),
                                         style.UsedColorScheme());
}

const CSSValue* WebkitTextStrokeColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.TextStrokeWidth(), style);
}

const CSSValue* WebkitTransformOriginX::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueID::kLeft,
                                                    CSSValueID::kRight>(
      range, context);
}

const CSSValue* WebkitTransformOriginY::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueID::kTop,
                                                    CSSValueID::kBottom>(
      range, context);
}

const CSSValue* WebkitTransformOriginZ::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeLength(range, context, kValueRangeAll);
}

const CSSValue* WebkitUserDrag::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.UserDrag());
}

const CSSValue* WebkitUserModify::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.UserModify());
}

const CSSValue* WebkitWritingMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetWritingMode());
}

void WebkitWritingMode::ApplyInitial(StyleResolverState& state) const {
  state.SetWritingMode(ComputedStyleInitialValues::InitialWritingMode());
}
void WebkitWritingMode::ApplyInherit(StyleResolverState& state) const {
  state.SetWritingMode(state.ParentStyle()->GetWritingMode());
}

void WebkitWritingMode::ApplyValue(StyleResolverState& state,
                                   const CSSValue& value) const {
  state.SetWritingMode(
      To<CSSIdentifierValue>(value).ConvertTo<blink::WritingMode>());
}

const CSSValue* WhiteSpace::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  if (ComputedStyleUtils::WidthOrHeightShouldReturnUsedValue(layout_object)) {
    return ZoomAdjustedPixelValue(
        ComputedStyleUtils::UsedBoxSize(*layout_object).Width(), style);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Width(),
                                                             style);
}

const CSSValue* WillChange::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);

  CSSValueList* values = CSSValueList::CreateCommaSeparated();
  // Every comma-separated list of identifiers is a valid will-change value,
  // unless the list includes an explicitly disallowed identifier.
  while (true) {
    if (range.Peek().GetType() != kIdentToken)
      return nullptr;
    CSSPropertyID unresolved_property = UnresolvedCSSPropertyID(
        context.GetExecutionContext(), range.Peek().Value());
    if (unresolved_property != CSSPropertyID::kInvalid &&
        unresolved_property != CSSPropertyID::kVariable) {
#if DCHECK_IS_ON()
      DCHECK(CSSProperty::Get(resolveCSSPropertyID(unresolved_property))
                 .IsWebExposed(context.GetExecutionContext()));
#endif
      // Now "all" is used by both CSSValue and CSSPropertyValue.
      // Need to return nullptr when currentValue is CSSPropertyID::kAll.
      if (unresolved_property == CSSPropertyID::kWillChange ||
          unresolved_property == CSSPropertyID::kAll)
        return nullptr;
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

    if (range.AtEnd())
      break;
    if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(range))
      return nullptr;
  }

  return values;
}

const CSSValue* WillChange::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForWillChange(
      style.WillChangeProperties(), style.WillChangeContents(),
      style.WillChangeScrollPosition());
}

void WillChange::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetWillChangeContents(false);
  state.Style()->SetWillChangeScrollPosition(false);
  state.Style()->SetWillChangeProperties(Vector<CSSPropertyID>());
  state.Style()->SetSubtreeWillChangeContents(
      state.ParentStyle()->SubtreeWillChangeContents());
}

void WillChange::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetWillChangeContents(
      state.ParentStyle()->WillChangeContents());
  state.Style()->SetWillChangeScrollPosition(
      state.ParentStyle()->WillChangeScrollPosition());
  state.Style()->SetWillChangeProperties(
      state.ParentStyle()->WillChangeProperties());
  state.Style()->SetSubtreeWillChangeContents(
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
  state.Style()->SetWillChangeContents(will_change_contents);
  state.Style()->SetWillChangeScrollPosition(will_change_scroll_position);
  state.Style()->SetWillChangeProperties(will_change_properties);
  state.Style()->SetSubtreeWillChangeContents(
      will_change_contents || state.ParentStyle()->SubtreeWillChangeContents());
}

const CSSValue* WordBreak::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.WordSpacing(), style);
}

const CSSValue* WritingMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(range, context,
                                                             kValueRangeAll);
}

const CSSValue* X::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(svg_style.X(),
                                                             style);
}

const CSSValue* Y::ParseSingleValue(CSSParserTokenRange& range,
                                    const CSSParserContext& context,
                                    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeSVGGeometryPropertyLength(range, context,
                                                             kValueRangeAll);
}

const CSSValue* Y::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(svg_style.Y(),
                                                             style);
}

const CSSValue* ZIndex::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeInteger(range, context);
}

const CSSValue* ZIndex::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (style.HasAutoZIndex())
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
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
    zoom = css_parsing_utils::ConsumePercent(range, context,
                                             kValueRangeNonNegative);
    if (!zoom) {
      zoom = css_parsing_utils::ConsumeNumber(range, context,
                                              kValueRangeNonNegative);
    }
  }
  if (zoom) {
    if (!(token.Id() == CSSValueID::kNormal ||
          (token.GetType() == kNumberToken &&
           To<CSSPrimitiveValue>(zoom)->GetDoubleValue() == 1) ||
          (token.GetType() == kPercentageToken &&
           To<CSSPrimitiveValue>(zoom)->GetDoubleValue() == 100)))
      context.Count(WebFeature::kCSSZoomNotEqualToOne);
  }
  return zoom;
}

const CSSValue* Zoom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
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
