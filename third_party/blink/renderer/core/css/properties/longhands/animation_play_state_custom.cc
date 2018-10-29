// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/animation_play_state.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AnimationPlayState::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueRunning, CSSValuePaused>,
      range);
}

const CSSValue* AnimationPlayState::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (wtf_size_t i = 0; i < animation_data->PlayStateList().size(); ++i) {
      list->Append(*ComputedStyleUtils::ValueForAnimationPlayState(
          animation_data->PlayStateList()[i]));
    }
  } else {
    list->Append(*InitialValue());
  }
  return list;
}

const CSSValue* AnimationPlayState::InitialValue() const {
  DEFINE_STATIC_LOCAL(Persistent<CSSValue>, value,
                      (CSSIdentifierValue::Create(CSSValueRunning)));
  return value;
}

}  // namespace CSSLonghand
}  // namespace blink
