// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GAP_DECORATION_PROPERTY_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GAP_DECORATION_PROPERTY_UTILS_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class CSSPropertyValue;
class CSSValueList;
template <typename T>
class GapDataList;
class StyleColor;

enum class CSSGapDecorationPropertyType : int {
  kColor,
  kWidth,
  kStyle,
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
  static CSSPropertyID GetShorthandProperty(
      CSSGapDecorationPropertyDirection direction);
  static void AddProperties(CSSGapDecorationPropertyDirection direction,
                            const CSSValueList& rule_widths,
                            const CSSValueList& rule_styles,
                            const CSSValueList& rule_colors,
                            bool important,
                            HeapVector<CSSPropertyValue, 64>& properties);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GAP_DECORATION_PROPERTY_UTILS_H_
