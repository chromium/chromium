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
    case CSSGapDecorationPropertyType::kInsetCapEnd:
      return direction == CSSGapDecorationPropertyDirection::kRow
                 ? CSSPropertyID::kRowRuleInsetCapEnd
                 : CSSPropertyID::kColumnRuleInsetCapEnd;
    case CSSGapDecorationPropertyType::kInsetCapStart:
      return direction == CSSGapDecorationPropertyDirection::kRow
                 ? CSSPropertyID::kRowRuleInsetCapStart
                 : CSSPropertyID::kColumnRuleInsetCapStart;
    case CSSGapDecorationPropertyType::kInsetJunctionStart:
      return direction == CSSGapDecorationPropertyDirection::kRow
                 ? CSSPropertyID::kRowRuleInsetJunctionStart
                 : CSSPropertyID::kColumnRuleInsetJunctionStart;
    case CSSGapDecorationPropertyType::kInsetJunctionEnd:
      return direction == CSSGapDecorationPropertyDirection::kRow
                 ? CSSPropertyID::kRowRuleInsetJunctionEnd
                 : CSSPropertyID::kColumnRuleInsetJunctionEnd;
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

        for (wtf_size_t i = 0; i < count; ++i) {
          for (const auto& value : repeater->RepeatedValues()) {
            expanded_values.push_back(GapData<T>(value));
          }
        }
      }
    }
  }

  return expanded_values;
}

Vector<int> CSSGapDecorationUtils::GetExpandedWidths(
    const GapDataList<int>& gap_data_list,
    wtf_size_t gap_count) {
  GapDataListIterator<int> iter(gap_data_list.GetGapDataList(), gap_count);
  Vector<int> result;
  result.ReserveInitialCapacity(gap_count);
  while (iter.HasNext()) {
    result.push_back(iter.Next());
  }
  return result;
}

RuleBreak CSSGapDecorationUtils::ResolveRuleBreakValue(
    const ComputedStyle& style,
    GridTrackSizingDirection direction,
    GapGeometry::ContainerType container_type) {
  RuleBreak rule_break =
      direction == kForColumns ? style.ColumnRuleBreak() : style.RowRuleBreak();

  // For multicol containers, `normal` resolves to `none` for row-rule-break
  // and `intersection` for column-rule-break.
  if (container_type == GapGeometry::ContainerType::kMultiColumn &&
      rule_break == RuleBreak::kNormal) {
    return direction == kForColumns ? RuleBreak::kIntersection
                                    : RuleBreak::kNone;
  }

  return rule_break;
}

RuleVisibilityItems CSSGapDecorationUtils::ResolveRuleVisibilityItemsValue(
    const ComputedStyle& style,
    GapGeometry::ContainerType container_type,
    GridTrackSizingDirection direction) {
  RuleVisibilityItems rule_visibility = direction == kForColumns
                                            ? style.ColumnRuleVisibilityItems()
                                            : style.RowRuleVisibilityItems();
  if (rule_visibility != RuleVisibilityItems::kNormal) {
    return rule_visibility;
  }

  // Resolve `normal` value based on the container type.
  //
  // https://drafts.csswg.org/css-gaps-1/#visibility-rules.
  switch (container_type) {
    case GapGeometry::ContainerType::kGrid:
      return RuleVisibilityItems::kAll;
    case GapGeometry::ContainerType::kFlex:
    case GapGeometry::ContainerType::kMultiColumn:
      return RuleVisibilityItems::kBetween;
  }
}

bool CSSGapDecorationUtils::IsRuleSegmentVisible(
    GridTrackSizingDirection track_direction,
    wtf_size_t gap_index,
    wtf_size_t intersection_index,
    RuleVisibilityItems rule_visibility,
    const GapGeometry& gap_geometry) {
  if (rule_visibility == RuleVisibilityItems::kAll) {
    return true;
  }

  GapSegmentState gap_state = gap_geometry.GetIntersectionGapSegmentState(
      track_direction, gap_index, intersection_index);

  switch (rule_visibility) {
    case RuleVisibilityItems::kAround:
      // Paint if either side of the segment is occupied (i.e. not empty on both
      // sides).
      return !gap_state.IsEmpty();
    case RuleVisibilityItems::kBetween:
      // Paint only when both sides of the segment are occupied (i.e. gap
      // segment state has no empty status).
      return !gap_state.HasEmptyStatus();
    case RuleVisibilityItems::kAll:
    case RuleVisibilityItems::kNormal:
      // `kAll` should have been handled as an early return at the beginning of
      // this function. `normal` should have been resolved before reaching this
      // point.
      NOTREACHED();
  }

  NOTREACHED();
}

bool CSSGapDecorationUtils::HasOverlapJoin(const ComputedStyle& style,
                                           bool is_column_gap) {
  return (is_column_gap ? style.ColumnRuleInsetCapStart()
                        : style.RowRuleInsetCapStart())
             .IsOverlapJoin() ||
         (is_column_gap ? style.ColumnRuleInsetCapEnd()
                        : style.RowRuleInsetCapEnd())
             .IsOverlapJoin() ||
         (is_column_gap ? style.ColumnRuleInsetJunctionStart()
                        : style.RowRuleInsetJunctionStart())
             .IsOverlapJoin() ||
         (is_column_gap ? style.ColumnRuleInsetJunctionEnd()
                        : style.RowRuleInsetJunctionEnd())
             .IsOverlapJoin();
}

bool CSSGapDecorationUtils::HasCrossGapSegment(
    GridTrackSizingDirection cross_direction,
    wtf_size_t gap_index,
    wtf_size_t intersection_index,
    RuleVisibilityItems rule_visibility,
    RuleVisibilityItems cross_rule_visibility,
    const GapGeometry& gap_geometry,
    const Vector<GapIntersection>& intersections) {
  if ((gap_geometry.GetContainerType() != GapGeometry::ContainerType::kGrid &&
       gap_geometry.GetContainerType() !=
           GapGeometry::ContainerType::kMultiColumn) ||
      rule_visibility != RuleVisibilityItems::kBetween) {
    return true;
  }

  const wtf_size_t cross_gap_index = intersection_index - 1;
  const wtf_size_t cross_intersection_index = gap_index + 1;

  const bool is_cross_before_visible =
      IsRuleSegmentVisible(cross_direction, cross_gap_index, gap_index,
                           cross_rule_visibility, gap_geometry);
  const bool is_cross_after_visible = IsRuleSegmentVisible(
      cross_direction, cross_gap_index, cross_intersection_index,
      cross_rule_visibility, gap_geometry);

  const BlockedStatus cross_blocked = gap_geometry.GetIntersectionBlockedStatus(
      cross_direction, cross_gap_index, cross_intersection_index,
      intersections);

  const bool is_cross_before_present =
      is_cross_before_visible &&
      !cross_blocked.HasBlockedStatus(BlockedStatus::kBlockedBefore);
  const bool is_cross_after_present =
      is_cross_after_visible &&
      !cross_blocked.HasBlockedStatus(BlockedStatus::kBlockedAfter);

  return is_cross_before_present || is_cross_after_present;
}

// Explicit template instantiations
template GapDataList<StyleColor>::GapDataVector
CSSGapDecorationUtils::GetExpandedGapDataList(
    const GapDataList<StyleColor>& gap_data_list);

template GapDataList<int>::GapDataVector
CSSGapDecorationUtils::GetExpandedGapDataList(
    const GapDataList<int>& gap_data_list);

}  // namespace blink
