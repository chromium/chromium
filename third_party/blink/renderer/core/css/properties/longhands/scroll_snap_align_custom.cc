// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/scroll_snap_align.h"

#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* ScrollSnapAlign::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValue* block_value = CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueNone, CSSValueStart, CSSValueEnd, CSSValueCenter>(range);
  if (!block_value)
    return nullptr;
  if (range.AtEnd())
    return block_value;

  CSSValue* inline_value = CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueNone, CSSValueStart, CSSValueEnd, CSSValueCenter>(range);
  if (!inline_value)
    return block_value;
  CSSValuePair* pair = CSSValuePair::Create(block_value, inline_value,
                                            CSSValuePair::kDropIdenticalValues);
  return pair;
}

const CSSValue* ScrollSnapAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForScrollSnapAlign(style.GetScrollSnapAlign(),
                                                     style);
}

}  // namespace CSSLonghand
}  // namespace blink
