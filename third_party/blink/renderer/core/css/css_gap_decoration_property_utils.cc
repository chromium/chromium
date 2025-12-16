// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"

#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/css_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/gap_data_list.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"

namespace blink {

CSSPropertyID CSSGapDecorationUtils::GetLonghandProperty(
    CSSGapDecorationPropertyDirection direction,
    CSSGapDecorationPropertyType type) {
  switch (type) {
    case CSSGapDecorationPropertyType::kWidth:
      return direction == CSSGapDecorationPropertyDirection::kRow
                 ? CSSPropertyID::kRowRuleWidth
                 : CSSPropertyID::kColumnRuleWidth;
    case CSSGapDecorationPropertyType::kStyle:
      return direction == CSSGapDecorationPropertyDirection::kRow
                 ? CSSPropertyID::kRowRuleStyle
                 : CSSPropertyID::kColumnRuleStyle;
    case CSSGapDecorationPropertyType::kColor:
      return direction == CSSGapDecorationPropertyDirection::kRow
                 ? CSSPropertyID::kRowRuleColor
                 : CSSPropertyID::kColumnRuleColor;
    case CSSGapDecorationPropertyType::kEdgeInsetEnd:
      return direction == CSSGapDecorationPropertyDirection::kRow
                 ? CSSPropertyID::kRowRuleEdgeInsetEnd
                 : CSSPropertyID::kColumnRuleEdgeInsetEnd;
    case CSSGapDecorationPropertyType::kEdgeInsetStart:
      return direction == CSSGapDecorationPropertyDirection::kRow
                 ? CSSPropertyID::kRowRuleEdgeInsetStart
                 : CSSPropertyID::kColumnRuleEdgeInsetStart;
    case CSSGapDecorationPropertyType::kInteriorInsetStart:
      return direction == CSSGapDecorationPropertyDirection::kRow
                 ? CSSPropertyID::kRowRuleInteriorInsetStart
                 : CSSPropertyID::kColumnRuleInteriorInsetStart;
    case CSSGapDecorationPropertyType::kInteriorInsetEnd:
      return direction == CSSGapDecorationPropertyDirection::kRow
                 ? CSSPropertyID::kRowRuleInteriorInsetEnd
                 : CSSPropertyID::kColumnRuleInteriorInsetEnd;
  }
}

bool CSSGapDecorationUtils::RuleColorMaybeDependsOnCurrentColor(
    const GapDataList<StyleColor>& gap_rule_color) {
  return std::ranges::any_of(
      gap_rule_color.GetGapDataList(), [](const GapData<StyleColor>& gap_data) {
        // If it’s a simple value, just test it directly.
        if (!gap_data.IsRepeaterData()) {
          const StyleColor& v = gap_data.GetValue();
          return v.DependsOnCurrentColor();
        }

        // Otherwise it’s a repeater: walk through its RepeatedValues()
        const auto* rep = gap_data.GetValueRepeater();
        return std::ranges::any_of(
            rep->RepeatedValues(),
            [](const StyleColor& v) { return v.DependsOnCurrentColor(); });
      });
}

void CSSGapDecorationUtils::AddProperties(
    CSSGapDecorationPropertyDirection direction,
    const CSSValueList& rule_widths,
    const CSSValueList& rule_styles,
    const CSSValueList& rule_colors,
    bool important,
    HeapVector<CSSPropertyValue, 64>& properties) {
  CSSPropertyID rule_shorthand_id;
  CSSPropertyID rule_width_id;
  CSSPropertyID rule_style_id;
  CSSPropertyID rule_color_id;

  if (direction == CSSGapDecorationPropertyDirection::kColumn) {
    rule_shorthand_id = CSSPropertyID::kColumnRule;
    rule_width_id = CSSPropertyID::kColumnRuleWidth;
    rule_style_id = CSSPropertyID::kColumnRuleStyle;
    rule_color_id = CSSPropertyID::kColumnRuleColor;
  } else {
    rule_shorthand_id = CSSPropertyID::kRowRule;
    rule_width_id = CSSPropertyID::kRowRuleWidth;
    rule_style_id = CSSPropertyID::kRowRuleStyle;
    rule_color_id = CSSPropertyID::kRowRuleColor;
  }

  css_parsing_utils::AddProperty(
      rule_width_id, rule_shorthand_id, rule_widths, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      rule_style_id, rule_shorthand_id, rule_styles, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      rule_color_id, rule_shorthand_id, rule_colors, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
}

BoxSide CSSGapDecorationUtils::BoxSideFromDirection(
    const ComputedStyle& style,
    GridTrackSizingDirection direction) {
  PhysicalToLogical<BoxSide> logical_sides(style.GetWritingDirection(),
                                           BoxSide::kTop, BoxSide::kRight,
                                           BoxSide::kBottom, BoxSide::kLeft);

  return (direction == kForColumns) ? logical_sides.InlineStart()
                                    : logical_sides.BlockStart();
}

CSSValueList* CSSGapDecorationUtils::GetExpandedCSSValueListForGapData(
    const CSSValueList& list,
    const StyleResolverState& state) {
  CSSValueList* expanded_list =
      MakeGarbageCollected<CSSValueList>(CSSValueList::kSpaceSeparator);

  for (const auto& value : list) {
    if (auto* gap_repeat_value =
            DynamicTo<cssvalue::CSSRepeatValue>(*value.Get())) {
      // Auto repeaters are not expanded.
      if (gap_repeat_value->IsAutoRepeatValue()) {
        expanded_list->Append(*value);
        continue;
      } else {
        int repeat_count = gap_repeat_value->Repetitions()->ComputeInteger(
            state.CssToLengthConversionData());
        for (int i = 0; i < repeat_count; ++i) {
          for (const auto& inner_value : gap_repeat_value->Values()) {
            expanded_list->Append(*inner_value);
          }
        }
      }
    } else {
      expanded_list->Append(*value);
    }
  }
  return expanded_list;
}

template <typename T>
typename GapDataList<T>::GapDataVector
CSSGapDecorationUtils::GetExpandedGapDataList(
    const GapDataList<T>& gap_data_list) {
  // Create a new GapDataList::GapDataVector with only the expanded values.
  // Auto repeaters are not expanded.
  typename GapDataList<T>::GapDataVector expanded_values;
  for (const auto& gap_data : gap_data_list.GetGapDataList()) {
    if (!gap_data.IsRepeaterData()) {
      // Simple single value, add to `expanded_values`.
      expanded_values.push_back(gap_data);
    } else {
      const ValueRepeater<T>* repeater = gap_data.GetValueRepeater();

      if (repeater->IsAutoRepeater()) {
        expanded_values.push_back(gap_data);
      } else {
        // Integer repeater, add values `count` times.
        wtf_size_t count = repeater->RepeatCount();

        for (size_t i = 0; i < count; ++i) {
          for (const auto& value : repeater->RepeatedValues()) {
            expanded_values.push_back(GapData<T>(value));
          }
        }
      }
    }
  }

  return expanded_values;
}

RuleBreak CSSGapDecorationUtils::ResolveRuleBreakValue(
    const ComputedStyle& style,
    GapGeometry::ContainerType container_type,
    GridTrackSizingDirection direction) {
  RuleBreak rule_break =
      direction == kForColumns ? style.ColumnRuleBreak() : style.RowRuleBreak();
  if (rule_break != RuleBreak::kAuto) {
    return rule_break;
  }

  // Resolve `auto` value based on thecontainer type.
  //
  // TODO(javiercon): For now, `auto` will always resolve to `none` for flex and
  // multicol. This may change in the future depending on the resolution to
  // https://github.com/w3c/csswg-drafts/issues/13127
  //
  // https://drafts.csswg.org/css-gaps-1/#break
  switch (container_type) {
    case GapGeometry::ContainerType::kGrid:
      return RuleBreak::kSpanningItem;
    case GapGeometry::ContainerType::kFlex:
    case GapGeometry::ContainerType::kMultiColumn:
      return RuleBreak::kNone;
  }
}

// Explicit template instantiations
template GapDataList<StyleColor>::GapDataVector
CSSGapDecorationUtils::GetExpandedGapDataList(
    const GapDataList<StyleColor>& gap_data_list);

template GapDataList<int>::GapDataVector
CSSGapDecorationUtils::GetExpandedGapDataList(
    const GapDataList<int>& gap_data_list);

}  // namespace blink
