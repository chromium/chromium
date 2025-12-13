// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_if_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class CSSIfParserTest : public PageTestBase {
 public:
  bool ParseQuery(String string) {
    const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    CSSIfParser parser(*context);
    CSSParserTokenStream stream(string);
    return !!parser.ConsumeIfCondition(stream);
  }
};

TEST_F(CSSIfParserTest, ConsumeValidCondition) {
  ScopedCSSInlineIfForStyleQueriesForTest scoped_style_feature(true);
  ScopedCSSInlineIfForMediaQueriesForTest scoped_media_feature(true);
  ScopedCSSInlineIfForSupportsQueriesForTest scoped_supports_feature(true);
  const char* valid_tests[] = {
      // clang-format off
    "style(--x)",
    "style(((--x)))",
    "style((--x) and (--y: 10))",
    "style((--x) and ((--y) or (not (--z))))",
    "style(not (--x))",
    "style(--x: var(--y))",
    "style((--y: green) and (--x: 3))",
    "style(((--x: 3px) and (--y: 3)) or (not (--z: 6px)))",
    "media(screen)",
    "media(screen and (color))",
    "media(all and (min-width:500px))",
    "media((min-width : 500px))",
    "media(not (min-width : -100px))",
    "media(only screen and (color))",
    "media((min-width: 30em) and (max-width: 50em))",
    "supports(transform-origin: 5% 5%)",
    "supports(not (transform-origin: 10em 10em 10em))",
    "supports(display: table-cell)",
    "supports((display: table-cell))",
    "supports((display: table-cell) and (display: list-item))",
    "media(screen) and supports(display: table-cell)",
    "media(screen) and (supports(display: table-cell) or style(--x))",
    "supports(general-enclosed)",
    "not (media(screen))",
    "(media(screen and (color))) and (style(--x))",
    "(media(screen and (color)) and style(--x)) or (style(not (--y)))",
    "style(style(--x))",
    "style(var(--x))",
    "style(attr(data-foo))",
    "style(var(--x): green) and style(var(--x): 3)",
    "(style(style(--x))) and (media((color)))",
      // clang-format on
  };

  for (const char* test : valid_tests) {
    EXPECT_TRUE(ParseQuery(test));
  }
}

TEST_F(CSSIfParserTest, ConsumeInvalidCondition) {
  ScopedCSSInlineIfForStyleQueriesForTest scoped_style_feature(true);
  ScopedCSSInlineIfForMediaQueriesForTest scoped_media_feature(true);
  ScopedCSSInlineIfForSupportsQueriesForTest scoped_supports_feature(true);
  const char* invalid_parse_time_tests[] = {
      // clang-format off
    "invalid",
    "style(invalid) and invalid",
    "media(invalid) or invalid",
    "invalid or style(invalid)",
    "invalid or supports(invalid)",
    "supports(invalid) and invalid",
      // clang-format on
  };

  for (const char* test : invalid_parse_time_tests) {
    EXPECT_FALSE(ParseQuery(test));
  }
}

}  // namespace blink
