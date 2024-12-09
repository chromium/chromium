// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_FAST_PATHS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_FAST_PATHS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CSSValue;

enum class ParseColorResult {
  kFailure,

  // The string identified a color keyword.
  kKeyword,

  // The string identified a valid color.
  kColor,
};

class CORE_EXPORT CSSParserFastPaths {
  STATIC_ONLY(CSSParserFastPaths);

 public:
  // Parses simple values like '10px' or 'green', but makes no guarantees
  // about handling any property completely.
  static CSSValue* MaybeParseValue(CSSPropertyID,
                                   StringView,
                                   const CSSParserContext*);

  // NOTE: Properties handled here shouldn't be explicitly handled in
  // CSSPropertyParser, so if this returns true, the fast path is the only path.
  static bool IsHandledByKeywordFastPath(CSSPropertyID property_id) {
    return handled_by_keyword_fast_paths_properties_.Has(property_id);
  }

  static bool IsBorderStyleValue(CSSValueID);

  static bool IsValidKeywordPropertyAndValue(CSSPropertyID,
                                             CSSValueID,
                                             CSSParserMode);

  static bool IsValidSystemFont(CSSValueID);

  // Tries parsing a string as a color, returning the result. Sets `color` if
  // the result is `kColor`.
  static ParseColorResult ParseColor(const String&,
                                     CSSParserMode,
                                     Color& color);

  // Checks for a value that looks like env(safe-area-inset-bottom) or
  // calc(env(safe-area-inset-bottom) + ...).
  //
  // Note that this function does not verify the validity of
  // the fallback (if any). In other words, it assumes that
  // the 'safe-area-inset-bottom' environment variable is supported
  // and therefore the fallback will not be taken.
  //
  // See go/cc-dynamic-sai and comments on IsBottomRelativeToSafeAreaInset in
  // computed_style_extra_fields.json5.
  static bool IsSafeAreaInsetBottom(StringView);

 private:
  static CSSBitset handled_by_keyword_fast_paths_properties_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_FAST_PATHS_H_
