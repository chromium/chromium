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
  kEdgeInsetEnd,
  kEdgeInsetStart,
  kInteriorInsetStart,
  kInteriorInsetEnd,
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

  static CSSValueList* GetExpandedCSSValueListForGapData(
      const CSSValueList& list,
      const StyleResolverState& state);

  static RuleBreak ResolveRuleBreakValue(
      const ComputedStyle& style,
      GapGeometry::ContainerType container_type,
      GridTrackSizingDirection direction);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GAP_DECORATION_PROPERTY_UTILS_H_
