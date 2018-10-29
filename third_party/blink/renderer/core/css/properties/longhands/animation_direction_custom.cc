// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/animation_direction.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AnimationDirection::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueNormal, CSSValueAlternate,
                                             CSSValueReverse,
                                             CSSValueAlternateReverse>,
      range);
}

const CSSValue* AnimationDirection::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (wtf_size_t i = 0; i < animation_data->DirectionList().size(); ++i) {
      list->Append(*ComputedStyleUtils::ValueForAnimationDirection(
          animation_data->DirectionList()[i]));
    }
  } else {
    list->Append(*InitialValue());
  }
  return list;
}

const CSSValue* AnimationDirection::InitialValue() const {
  DEFINE_STATIC_LOCAL(Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueNormal)));
  return value;
}

}  // namespace CSSLonghand
}  // namespace blink
