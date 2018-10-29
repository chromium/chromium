// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/border_image_outset.h"

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* BorderImageOutset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeBorderImageOutset(range);
}

const CSSValue* BorderImageOutset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageQuad(
      style.BorderImage().Outset(), style);
}

const CSSValue* BorderImageOutset::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      Persistent<CSSValue>, zeroInteger,
      (CSSPrimitiveValue::Create(0, CSSPrimitiveValue::UnitType::kInteger)));
  DEFINE_STATIC_LOCAL(
      Persistent<CSSQuadValue>, value,
      (CSSQuadValue::Create(zeroInteger, zeroInteger, zeroInteger, zeroInteger,
                            CSSQuadValue::kSerializeAsQuad)));
  return value;
}

}  // namespace CSSLonghand
}  // namespace blink
