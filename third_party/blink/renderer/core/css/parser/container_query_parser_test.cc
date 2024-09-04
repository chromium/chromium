// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class ContainerQueryParserTest : public PageTestBase {
 public:
  String ParseQuery(String string) {
    const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    const MediaQueryExpNode* node =
        ContainerQueryParser(*context).ParseCondition(string);
    if (!node) {
      return g_null_atom;
    }
    if (node->HasUnknown()) {
      return "<unknown>";
    }
    StringBuilder builder;
    node->SerializeTo(builder);
    return builder.ReleaseString();
  }

  class TestFeatureSet : public MediaQueryParser::FeatureSet {
    STACK_ALLOCATED();

   public:
    bool IsAllowed(const String& feature) const override {
      return feature == "width";
    }
    bool IsAllowedWithoutValue(const String& feature,
                               const ExecutionContext*) const override {
      return true;
    }
    bool IsCaseSensitive(const String& feature) const override { return false; }
    bool SupportsRange() const override { return true; }
  };

  // E.g. https://drafts.csswg.org/css-contain-3/#typedef-style-query
  String ParseFeatureQuery(String feature_query) {
    const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    CSSParserTokenStream stream(feature_query);
    const MediaQueryExpNode* node =
        ContainerQueryParser(*context).ConsumeFeatureQuery(stream,
                                                           TestFeatureSet());
    if (!node || !stream.AtEnd()) {
      return g_null_atom;
    }
    return node->Serialize();
  }
};

TEST_F(ContainerQueryParserTest, ParseQuery) {
  const char* tests[] = {
      "(width)",
      "(min-width: 100px)",
      "(width > 100px)",
      "(width: 100px)",
      "(not (width))",
      "((not (width)) and (width))",
      "((not (width)) and (width))",
      "((width) and (width))",
      "((width) or ((width) and (not (width))))",
      "((width > 100px) and (width > 200px))",
      "((width) and (width) and (width))",
      "((width) or (width) or (width))",
      "not (width)",
      "(width) and (height)",
      "(width) or (height)",
  };

  for (const char* test : tests) {
    EXPECT_EQ(String(test), ParseQuery(test));
  }

  // Escaped (unnecessarily but validly) characters in the identifier.
  EXPECT_EQ("(width)", ParseQuery("(\\77 idth)"));
  // Repro case for b/341640868
  EXPECT_EQ("(min-width: 100px)", ParseQuery("(min\\2d width: 100px)"));

  // Invalid:
  EXPECT_EQ("<unknown>", ParseQuery("(min-width)"));
  EXPECT_EQ("<unknown>", ParseQuery("((width) or (width) and (width))"));
  EXPECT_EQ("<unknown>", ParseQuery("((width) and (width) or (width))"));
  EXPECT_EQ("<unknown>", ParseQuery("((width) or (height) and (width))"));
  EXPECT_EQ("<unknown>", ParseQuery("((width) and (height) or (width))"));
  EXPECT_EQ("<unknown>", ParseQuery("((width) and (height) 50px)"));
  EXPECT_EQ("<unknown>", ParseQuery("((width) and (height 50px))"));
  EXPECT_EQ("<unknown>", ParseQuery("((width) and 50px (height))"));
  EXPECT_EQ("<unknown>", ParseQuery("foo(width)"));
  EXPECT_EQ("<unknown>", ParseQuery("size(width)"));
}

// This test exists primarily to not lose coverage of
// `ContainerQueryParser::ConsumeFeatureQuery`, which is unused until
// style() queries are supported (crbug.com/1302630).
TEST_F(ContainerQueryParserTest, ParseFeatureQuery) {
  const char* tests[] = {
      "width",
      "width: 100px",
      "(not (width)) and (width)",
      "(width > 100px) and (width > 200px)",
      "(width) and (width) and (width)",
      "(width) or (width) or (width)",
  };

  for (const char* test : tests) {
    EXPECT_EQ(String(test), ParseFeatureQuery(test));
  }

  // Invalid:
  EXPECT_EQ(g_null_atom, ParseFeatureQuery("unsupported"));
  EXPECT_EQ(g_null_atom, ParseFeatureQuery("(width) or (width) and (width)"));
  EXPECT_EQ(g_null_atom, ParseFeatureQuery("(width) and (width) or (width)"));
}

}  // namespace blink
