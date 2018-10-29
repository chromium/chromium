// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/transition_duration.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* TransitionDuration::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyParserHelpers::ConsumeTime, range, kValueRangeNonNegative);
}

const CSSValue* TransitionDuration::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForAnimationDuration(style.Transitions());
}

const CSSValue* TransitionDuration::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      Persistent<CSSValue>, value,
      (CSSPrimitiveValue::Create(CSSTimingData::InitialDuration(),
                                 CSSPrimitiveValue::UnitType::kSeconds)));
  return value;
}

}  // namespace CSSLonghand
}  // namespace blink
