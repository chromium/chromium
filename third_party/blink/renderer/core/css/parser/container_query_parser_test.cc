// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class ContainerQueryParserTest : public PageTestBase {
 public:
  String ParseSelector(String string) {
    const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    auto tokens = CSSTokenizer(string).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    absl::optional<ContainerSelector> selector =
        ContainerQueryParser(*context).ConsumeSelector(range);
    if (!selector || !range.AtEnd())
      return g_null_atom;
    return selector->ToString();
  }

  String ParseQuery(String string) {
    const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    std::unique_ptr<MediaQueryExpNode> node =
        ContainerQueryParser(*context).ParseQuery(string);
    if (!node)
      return g_null_atom;
    if (node->HasUnknown())
      return "<unknown>";
    StringBuilder builder;
    node->SerializeTo(builder);
    return builder.ReleaseString();
  }
};

TEST_F(ContainerQueryParserTest, ConsumeSelector) {
  struct {
    const char* input;
    const char* output;
  } valid_tests[] = {
      {"foo", nullptr},
      {"foo ", "foo"},
      {"name(foo)", "foo"},
      {"type(inline-size)", nullptr},
      {"type(block-size)", nullptr},
      {"type(size)", nullptr},
      {"name(foo) type(inline-size)", nullptr},
  };

  for (const auto& test : valid_tests) {
    String actual = ParseSelector(test.input);
    String expected(test.output ? test.output : test.input);
    EXPECT_EQ(expected, actual);
  }

  // Invalid:
  EXPECT_EQ(g_null_atom, ParseSelector(" foo"));
  EXPECT_EQ(g_null_atom, ParseSelector("name()"));
  EXPECT_EQ(g_null_atom, ParseSelector("name(50px)"));
  EXPECT_EQ(g_null_atom, ParseSelector("type()"));
  EXPECT_EQ(g_null_atom, ParseSelector("type(unknown)"));
  EXPECT_EQ(g_null_atom, ParseSelector("name(foo) name(bar)"));
}

TEST_F(ContainerQueryParserTest, ParseQuery) {
  struct {
    const char* input;
    const char* output;
  } tests[] = {
      {"size(width)", nullptr},
      {"size(width: 100px)", nullptr},
      {"(not size(width))", nullptr},
      {"((not size(width)) and size(width))", nullptr},
      {"size((not (width)) and (width))", nullptr},
      {"(size(width) and size(width))", nullptr},
      {"(size(width) or (size(width) and (not size(width))))", nullptr},
      {"size((width > 100px) and (width > 200px))", nullptr},
      {"size((width) and (width) and (width))", nullptr},
      {"size((width) or (width) or (width))", nullptr},
  };

  for (const auto& test : tests) {
    String actual = ParseQuery(test.input);
    String expected(test.output ? test.output : test.input);
    EXPECT_EQ(expected, actual);
  }

  // Invalid:
  EXPECT_EQ("<unknown>", ParseQuery("(width)"));
  EXPECT_EQ("<unknown>", ParseQuery("(min-width)"));
  EXPECT_EQ("<unknown>", ParseQuery("(min-width: 100px)"));
  EXPECT_EQ("<unknown>", ParseQuery("(width > 100px)"));
  EXPECT_EQ(g_null_atom, ParseQuery("size(width) and size(height)"));
  EXPECT_EQ(g_null_atom, ParseQuery("size(width) or size(height)"));
  EXPECT_EQ("<unknown>", ParseQuery("size((width) or (width) and (width))"));
  EXPECT_EQ("<unknown>", ParseQuery("size((width) and (width) or (width))"));
  EXPECT_EQ("<unknown>",
            ParseQuery("(size(width) or size(height) and size(width))"));
  EXPECT_EQ("<unknown>",
            ParseQuery("(size(width) and size(height) or size(width))"));
  EXPECT_EQ("<unknown>", ParseQuery("(size(width) and size(height) 50px)"));
  EXPECT_EQ("<unknown>", ParseQuery("(size(width) and size(height 50px))"));
  EXPECT_EQ("<unknown>", ParseQuery("(size(width) and 50px size(height))"));
  EXPECT_EQ("<unknown>", ParseQuery("foo(width)"));
}

}  // namespace blink
