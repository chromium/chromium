// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/transition_property.h"

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* TransitionProperty::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  CSSValueList* list = CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSParsingUtils::ConsumeTransitionProperty, range);
  if (!list || !CSSParsingUtils::IsValidPropertyList(*list))
    return nullptr;
  return list;
}

const CSSValue* TransitionProperty::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForTransitionProperty(style.Transitions());
}

const CSSValue* TransitionProperty::InitialValue() const {
  DEFINE_STATIC_LOCAL(Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueAll)));
  return value;
}

}  // namespace CSSLonghand
}  // namespace blink
