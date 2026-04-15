// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_scope.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class StyleScopeTest : public PageTestBase {
 public:
  String ToString(const CSSSelector* selector_list) {
    if (!selector_list) {
      return "";
    }
    return CSSSelectorList::SelectorsText(selector_list);
  }

  String RemainingTextAfterConsume(StringView input) {
    auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    auto* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
    CSSParserTokenStream stream(input);
    StyleScope::Consume(stream, context, CSSNestingType::kNone, nullptr,
                        sheet->Contents());
    return stream.RemainingText().ToString();
  }
};

TEST_F(StyleScopeTest, Copy) {
  auto* rule = css_test_helpers::ParseRule(GetDocument(), R"CSS(
        @scope (.x) to (.y) {
          #target { z-index: 1; }
        }
      )CSS");
  ASSERT_TRUE(rule);
  auto& scope_rule = To<StyleRuleScope>(*rule);
  const StyleScope& a = scope_rule.GetStyleScope();
  const StyleScope& b = *MakeGarbageCollected<StyleScope>(a);

  EXPECT_FALSE(a.IsImplicit());
  EXPECT_FALSE(b.IsImplicit());

  EXPECT_EQ(ToString(a.From()), ToString(b.From()));
  EXPECT_EQ(ToString(a.To()), ToString(b.To()));
}

TEST_F(StyleScopeTest, CopyImplicit) {
  auto* rule = css_test_helpers::ParseRule(GetDocument(), R"CSS(
        @scope {
          #target { z-index: 1; }
        }
      )CSS");
  ASSERT_TRUE(rule);
  auto& scope_rule = To<StyleRuleScope>(*rule);
  const StyleScope& a = scope_rule.GetStyleScope();
  const StyleScope& b = *MakeGarbageCollected<StyleScope>(a);

  // Mostly just don't crash.
  EXPECT_TRUE(a.IsImplicit());
  EXPECT_TRUE(b.IsImplicit());

  EXPECT_EQ(ToString(a.From()), ToString(b.From()));
  EXPECT_EQ(ToString(a.To()), ToString(b.To()));
}

TEST_F(StyleScopeTest, CopyParent) {
  auto* rule = css_test_helpers::ParseRule(GetDocument(), R"CSS(
        @scope (.x) {
          #target { z-index: 1; }
        }
      )CSS");
  ASSERT_TRUE(rule);
  auto& scope_rule = To<StyleRuleScope>(*rule);

  const StyleScope& a = scope_rule.GetStyleScope();
  const StyleScope& b = *MakeGarbageCollected<StyleScope>(a);

  const StyleScope& c = *b.CopyWithParent(&b);
  const StyleScope& d = *MakeGarbageCollected<StyleScope>(c);

  EXPECT_FALSE(a.Parent());
  EXPECT_FALSE(b.Parent());
  EXPECT_EQ(&b, c.Parent());
  EXPECT_EQ(&b, d.Parent());
}

TEST_F(StyleScopeTest, ConsumeStreamState) {
  EXPECT_EQ("", RemainingTextAfterConsume("(.x)"));
  EXPECT_EQ("", RemainingTextAfterConsume("to (.y)"));
  EXPECT_EQ("", RemainingTextAfterConsume("(.x) to (.y)"));
  EXPECT_EQ("rest", RemainingTextAfterConsume("(.x) rest"));
  EXPECT_EQ("rest", RemainingTextAfterConsume("to (.y) rest"));
  EXPECT_EQ("rest", RemainingTextAfterConsume("(.x) to (.y) rest"));
  EXPECT_EQ("rest", RemainingTextAfterConsume("(.x) to (.y) rest"));
  EXPECT_EQ("(!) to (.y)", RemainingTextAfterConsume("(!) to (.y)"));
  EXPECT_EQ("! (.y)", RemainingTextAfterConsume("(.x) ! (.y)"));
  EXPECT_EQ("to (!)", RemainingTextAfterConsume("(.x) to (!)"));
  EXPECT_EQ("bar (.y)", RemainingTextAfterConsume("(.x) bar (.y)"));
  EXPECT_EQ("10px (.y)", RemainingTextAfterConsume("(.x) 10px (.y)"));
}

}  // namespace blink
