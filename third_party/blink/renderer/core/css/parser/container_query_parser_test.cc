// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/container_selector.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class ContainerQueryParserTest : public PageTestBase {
 public:
  template <typename Functor>
  String ParseQuery(String string, Functor& container_query_parser_func) {
    const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    ContainerQueryParser parser(*context);
    const ConditionalExpNode* node =
        container_query_parser_func(string, parser);
    if (!node) {
      return g_null_atom;
    }
    if (ContainerSelector::CollectFeatureFlags(*node) &
        ContainerSelector::kFeatureUnknown) {
      return "<unknown>";
    }
    StringBuilder builder;
    node->SerializeTo(builder);
    return builder.ReleaseString();
  }

  class TestFeatureSet : public MediaQueryParser::FeatureSet {
    STACK_ALLOCATED();

   public:
    bool IsAllowed(const AtomicString& feature) const override {
      return feature == "width";
    }
    bool IsAllowedWithoutValue(const AtomicString& feature,
                               const ExecutionContext*) const override {
      return true;
    }
    bool IsAllowedWithValue(const AtomicString& feature) const override {
      return true;
    }
    bool IsCaseSensitive(const AtomicString& feature) const override {
      return false;
    }
    bool SupportsRange() const override { return true; }
    bool SupportsStyleRange() const override { return false; }
    bool SupportsElementDependent() const override { return false; }
  };

  // E.g. https://drafts.csswg.org/css-contain-3/#typedef-style-query
  String ParseFeatureQuery(String feature_query) {
    const auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    CSSParserTokenStream stream(feature_query);
    const ConditionalExpNode* node =
        ContainerQueryParser(*context).ConsumeFeatureQuery(stream,
                                                           TestFeatureSet());
    if (!node || !stream.AtEnd()) {
      return g_null_atom;
    }
    return node->Serialize();
  }
};

TEST_F(ContainerQueryParserTest, ParseQuery) {
  auto container_query_parser_func = [](String string,
                                        ContainerQueryParser& parser) {
    return parser.ParseCondition(string);
  };
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
    EXPECT_EQ(String(test), ParseQuery(test, container_query_parser_func));
  }

  // Escaped (unnecessarily but validly) characters in the identifier.
  EXPECT_EQ("(width)", ParseQuery("(\\77 idth)", container_query_parser_func));
  // Repro case for b/341640868
  EXPECT_EQ("(min-width: 100px)",
            ParseQuery("(min\\2d width: 100px)", container_query_parser_func));

  // Invalid:
  EXPECT_EQ("<unknown>",
            ParseQuery("(min-width)", container_query_parser_func));
  EXPECT_EQ("<unknown>", ParseQuery("((width) or (width) and (width))",
                                    container_query_parser_func));
  EXPECT_EQ("<unknown>", ParseQuery("((width) and (width) or (width))",
                                    container_query_parser_func));
  EXPECT_EQ("<unknown>", ParseQuery("((width) or (height) and (width))",
                                    container_query_parser_func));
  EXPECT_EQ("<unknown>", ParseQuery("((width) and (height) or (width))",
                                    container_query_parser_func));
  EXPECT_EQ("<unknown>", ParseQuery("((width) and (height) 50px)",
                                    container_query_parser_func));
  EXPECT_EQ("<unknown>", ParseQuery("((width) and (height 50px))",
                                    container_query_parser_func));
  EXPECT_EQ("<unknown>", ParseQuery("((width) and 50px (height))",
                                    container_query_parser_func));
  EXPECT_EQ("<unknown>", ParseQuery("foo(width)", container_query_parser_func));
  EXPECT_EQ("<unknown>",
            ParseQuery("size(width)", container_query_parser_func));
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
