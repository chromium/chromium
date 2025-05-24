// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"

#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
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

}  // namespace blink
