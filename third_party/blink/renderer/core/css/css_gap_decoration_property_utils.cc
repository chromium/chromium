// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"

#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/gap_data_list.h"

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
  }
}

CSSPropertyID CSSGapDecorationUtils::GetShorthandProperty(
    CSSGapDecorationPropertyDirection direction) {
  return direction == CSSGapDecorationPropertyDirection::kRow
             ? CSSPropertyID::kRowRule
             : CSSPropertyID::kColumnRule;
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

}  // namespace blink
