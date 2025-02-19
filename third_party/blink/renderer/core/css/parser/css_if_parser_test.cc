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

enum ParseResult { kParsed, kUnknown, kInvalid };

class CSSIfParserTest : public PageTestBase {
 public:
  ParseResult ParseQuery(String string) {
    const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    CSSIfParser parser(*context);
    CSSParserTokenStream stream(string);
    std::optional<IfTest> if_test = parser.ConsumeIfTest(stream);
    if (!if_test.has_value()) {
      return ParseResult::kInvalid;
    }

    if (const MediaQueryExpNode* style_test = if_test->GetStyleTest()) {
      if (style_test->HasUnknown()) {
        return ParseResult::kUnknown;
      }
      return ParseResult::kParsed;
    }

    if (const MediaQuery* media_test = if_test->GetMediaTest()) {
      if (media_test->HasUnknown()) {
        return ParseResult::kUnknown;
      }
      return ParseResult::kParsed;
    }
    return ParseResult::kInvalid;
  }
};

TEST_F(CSSIfParserTest, ConsumeValidCondition) {
  ScopedCSSInlineIfForStyleQueriesForTest scoped_style_feature(true);
  ScopedCSSInlineIfForMediaQueriesForTest scoped_media_feature(true);
  const char* valid_known_tests[] = {
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
    "media((min-width: 30em) and (max-width: 50em))"
      // clang-format on
  };

  for (const char* test : valid_known_tests) {
    EXPECT_EQ(ParseQuery(test), ParseResult::kParsed);
  }
}

TEST_F(CSSIfParserTest, ConsumeUnknownCondition) {
  const char* valid_unknown_tests[] = {
      // clang-format off
    "style(style(--x))"
    "style(var(--x))"
    "style(attr(data-foo))"
    "style(var(--x): green) and style(var(--x): 3)"
      // clang-format on
  };

  for (const char* test : valid_unknown_tests) {
    EXPECT_EQ(ParseQuery(test), ParseResult::kUnknown);
  }
}

TEST_F(CSSIfParserTest, ConsumeInvalidCondition) {
  ScopedCSSInlineIfForStyleQueriesForTest scoped_style_feature(true);
  ScopedCSSInlineIfForMediaQueriesForTest scoped_media_feature(true);
  const char* invalid_parse_time_tests[] = {
      // clang-format off
    "(min-width: 100px)",
    "not (width)",
    "(width) and (height)",
    "(((style(--x))))",
    "not style(--x)",
    "style(width)",
    "style(invalid)",
    "(style(--x: 3px) and style(--y: 3)) or (not style(--z: 6px))",
      // clang-format on
  };

  for (const char* test : invalid_parse_time_tests) {
    EXPECT_EQ(ParseQuery(test), ParseResult::kInvalid);
  }
}

}  // namespace blink
