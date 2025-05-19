// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"

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

}  // namespace blink
