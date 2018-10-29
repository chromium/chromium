// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/x.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* X::ParseSingleValue(CSSParserTokenRange& range,
                                    const CSSParserContext& context,
                                    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeSVGGeometryPropertyLength(range,
                                                                    context);
}

const CSSValue* X::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(svg_style.X(),
                                                             style);
}

}  // namespace CSSLonghand
}  // namespace blink
