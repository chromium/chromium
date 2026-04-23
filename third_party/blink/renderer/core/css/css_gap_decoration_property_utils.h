// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GAP_DECORATION_PROPERTY_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GAP_DECORATION_PROPERTY_UTILS_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/gap_data_list.h"
#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class ComputedStyle;
class CSSPropertyValue;
class CSSValueList;
class StyleColor;
class StyleResolverState;

enum class CSSGapDecorationPropertyType : int {
  kColor,
  kWidth,
  kStyle,
  kInsetCapEnd,
  kInsetCapStart,
  kInsetJunctionStart,
  kInsetJunctionEnd,
};

enum class CSSGapDecorationPropertyDirection : int {
  kRow,
  kColumn,
};

class CORE_EXPORT CSSGapDecorationUtils {
  STATIC_ONLY(CSSGapDecorationUtils);

 public:
  static bool RuleColorMaybeDependsOnCurrentColor(
      const GapDataList<StyleColor>& gap_rule_color);

  static CSSPropertyID GetLonghandProperty(
      CSSGapDecorationPropertyDirection direction,
      CSSGapDecorationPropertyType type);
  static void AddProperties(CSSGapDecorationPropertyDirection direction,
                            const CSSValueList& rule_widths,
                            const CSSValueList& rule_styles,
                            const CSSValueList& rule_colors,
                            bool important,
                            HeapVector<CSSPropertyValue, 64>& properties);

  static BoxSide BoxSideFromDirection(const ComputedStyle& style,
                                      GridTrackSizingDirection direction);

  // Creates and returns a vector with the expanded gap data values of repeaters
  // present. Auto repeaters are not expanded.
  template <typename T>
  static typename GapDataList<T>::GapDataVector GetExpandedGapDataList(
      const GapDataList<T>& gap_data_list);

  // Expands a GapDataList<int> (rule widths) into a Vector<int> of exactly
  // `gap_count` values, fully resolving all repeaters (including auto
  // repeaters) based on the known number of gaps.
  static Vector<int> GetExpandedWidths(const GapDataList<int>& gap_data_list,
                                       wtf_size_t gap_count);

  static CSSValueList* GetExpandedCSSValueListForGapData(
      const CSSValueList& list,
      const StyleResolverState& state);

  // Resolves the `rule-break` value for a given direction and container type.
  // For multicol containers, we treat `normal` as `none` for
  // `row-rule-break` and as `intersection` for `column-rule-break`.
  static RuleBreak ResolveRuleBreakValue(
      const ComputedStyle& style,
      GridTrackSizingDirection direction,
      GapGeometry::ContainerType container_type);

  // Resolves the `rule-visibility-items` value for a given direction and
  // container. For multicol and flex containers, `normal` resolves to `between`
  // while for `grid`, `normal` resolves to `all`.
  static RuleVisibilityItems ResolveRuleVisibilityItemsValue(
      const ComputedStyle& style,
      GapGeometry::ContainerType container_type,
      GridTrackSizingDirection direction);

  // Determines if the segment at `intersection_index` within the gap at
  // `gap_index` is visible based on `rule_visibility`.
  static bool IsRuleSegmentVisible(GridTrackSizingDirection track_direction,
                                   wtf_size_t gap_index,
                                   wtf_size_t intersection_index,
                                   RuleVisibilityItems rule_visibility,
                                   const GapGeometry& gap_geometry);

  // Returns true if any inset property in the given direction uses
  // `overlap-join`.
  static bool HasOverlapJoin(const ComputedStyle& style, bool is_column_gap);

  // Checks whether a cross-direction gap segment exists at the given
  // intersection. A segment is "present" if it passes the cross-direction
  // visibility rules and is not blocked by a spanning item. Returns true if at
  // least one segment (before or after) is present. Only applies to grid
  // containers with `rule-visibility-items: between`.
  static bool HasCrossGapSegment(GridTrackSizingDirection cross_direction,
                                 wtf_size_t gap_index,
                                 wtf_size_t intersection_index,
                                 RuleVisibilityItems rule_visibility,
                                 RuleVisibilityItems cross_rule_visibility,
                                 const GapGeometry& gap_geometry,
                                 const Vector<GapIntersection>& intersections);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GAP_DECORATION_PROPERTY_UTILS_H_
