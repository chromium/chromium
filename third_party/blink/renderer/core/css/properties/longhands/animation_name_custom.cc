// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/animation_name.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AnimationName::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  // Allow quoted name if this is an alias property.
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSParsingUtils::ConsumeAnimationName, range, context,
      local_context.UseAliasParsing());
}

const CSSValue* AnimationName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (wtf_size_t i = 0; i < animation_data->NameList().size(); ++i)
      list->Append(*CSSCustomIdentValue::Create(animation_data->NameList()[i]));
  } else {
    list->Append(*InitialValue());
  }
  return list;
}

const CSSValue* AnimationName::InitialValue() const {
  DEFINE_STATIC_LOCAL(Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueNone)));
  return value;
}

}  // namespace CSSLonghand
}  // namespace blink
