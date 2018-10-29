// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/animation_delay.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AnimationDelay::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyParserHelpers::ConsumeTime, range, kValueRangeAll);
}

const CSSValue* AnimationDelay::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDelay(style.Animations());
}

const CSSValue* AnimationDelay::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      Persistent<CSSValue>, value,
      (CSSPrimitiveValue::Create(CSSTimingData::InitialDelay(),
                                 CSSPrimitiveValue::UnitType::kSeconds)));
  return value;
}

}  // namespace CSSLonghand
}  // namespace blink
