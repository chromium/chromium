// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_FAST_PATHS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_FAST_PATHS_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CSSValue;

class CORE_EXPORT CSSParserFastPaths {
  STATIC_ONLY(CSSParserFastPaths);

 public:
  // Parses simple values like '10px' or 'green', but makes no guarantees
  // about handling any property completely.
  static CSSValue* MaybeParseValue(CSSPropertyID, const String&, CSSParserMode);

  // Properties handled here shouldn't be explicitly handled in
  // CSSPropertyParser
  static bool IsKeywordPropertyID(CSSPropertyID);

  // Returns if a property should be handled by the fast path, but have other
  // non-keyword values which should be handled by the CSSPropertyParser.
  static bool IsPartialKeywordPropertyID(CSSPropertyID);

  static bool IsValidKeywordPropertyAndValue(CSSPropertyID,
                                             CSSValueID,
                                             CSSParserMode);

  static CSSValue* ParseColor(const String&, CSSParserMode);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_FAST_PATHS_H_
