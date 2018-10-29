// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/border_image_slice.h"

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* BorderImageSlice::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeBorderImageSlice(
      range, CSSParsingUtils::DefaultFill::kNoFill);
}

const CSSValue* BorderImageSlice::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageSlice(style.BorderImage());
}

const CSSValue* BorderImageSlice::InitialValue() const {
  DEFINE_STATIC_LOCAL(Persistent<CSSValue>, value,
                      (CSSPrimitiveValue::Create(
                          100, CSSPrimitiveValue::UnitType::kPercentage)));
  return value;
}

}  // namespace CSSLonghand
}  // namespace blink
