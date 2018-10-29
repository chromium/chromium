// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/animation_iteration_count.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AnimationIterationCount::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSParsingUtils::ConsumeAnimationIterationCount, range);
}

const CSSValue* AnimationIterationCount::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (wtf_size_t i = 0; i < animation_data->IterationCountList().size();
         ++i) {
      list->Append(*ComputedStyleUtils::ValueForAnimationIterationCount(
          animation_data->IterationCountList()[i]));
    }
  } else {
    list->Append(*InitialValue());
  }
  return list;
}

const CSSValue* AnimationIterationCount::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      Persistent<CSSValue>, value,
      (CSSPrimitiveValue::Create(CSSAnimationData::InitialIterationCount(),
                                 CSSPrimitiveValue::UnitType::kNumber)));
  return value;
}

}  // namespace CSSLonghand
}  // namespace blink
