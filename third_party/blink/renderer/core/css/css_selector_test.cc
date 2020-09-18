// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/rule_set.h"

#include "testing/gtest/include/gtest/gtest.h"

#include <iostream>

namespace blink {

namespace {

unsigned Specificity(const String& selector_text) {
  CSSSelectorList selector_list = CSSParser::ParseSelector(
      StrictCSSParserContext(SecureContextMode::kInsecureContext), nullptr,
      selector_text);

  const CSSSelector* selector = selector_list.First();
  return selector->Specificity();
}

}  // namespace

TEST(CSSSelector, Representations) {
  css_test_helpers::TestStyleSheet sheet;

  const char* css_rules =
      "summary::-webkit-details-marker { }"
      "* {}"
      "div {}"
      "#id {}"
      ".class {}"
      "[attr] {}"
      "div:hover {}"
      "div:nth-child(2){}"
      ".class#id { }"
      "#id.class { }"
      "[attr]#id { }"
      "div[attr]#id { }"
      "div::content { }"
      "div::first-line { }"
      ".a.b.c { }"
      "div:not(.a) { }"        // without class a
      "div:not(:visited) { }"  // without the visited pseudo class

      "[attr=\"value\"] { }"   // Exact equality
      "[attr~=\"value\"] { }"  // One of a space-separated list
      "[attr^=\"value\"] { }"  // Begins with
      "[attr$=\"value\"] { }"  // Ends with
      "[attr*=\"value\"] { }"  // Substring equal to
      "[attr|=\"value\"] { }"  // One of a hyphen-separated list

      ".a .b { }"    // .b is a descendant of .a
      ".a > .b { }"  // .b is a direct descendant of .a
      ".a ~ .b { }"  // .a precedes .b in sibling order
      ".a + .b { }"  // .a element immediately precedes .b in sibling order
      ".a, .b { }"   // matches .a or .b

      ".a.b .c {}";

  sheet.AddCSSRules(css_rules);
  EXPECT_EQ(30u,
            sheet.GetRuleSet().RuleCount());  // .a, .b counts as two rules.
#ifndef NDEBUG
  sheet.GetRuleSet().Show();
#endif
}

TEST(CSSSelector, OverflowRareDataMatchNth) {
  int max_int = std::numeric_limits<int>::max();
  int min_int = std::numeric_limits<int>::min();
  CSSSelector selector;

  // Overflow count - b (max_int - -1 = max_int + 1)
  selector.SetNth(1, -1);
  EXPECT_FALSE(selector.MatchNth(max_int));
  // 0 - (min_int) = max_int + 1
  selector.SetNth(1, min_int);
  EXPECT_FALSE(selector.MatchNth(0));

  // min_int - 1
  selector.SetNth(-1, min_int);
  EXPECT_FALSE(selector.MatchNth(1));

  // a shouldn't negate to itself (and min_int negates to itself).
  // Note: This test can only fail when using ubsan.
  selector.SetNth(min_int, 10);
  EXPECT_FALSE(selector.MatchNth(2));
}

TEST(CSSSelector, Specificity_Is) {
  EXPECT_EQ(Specificity(".a :is(.b, div.c)"), Specificity(".a div.c"));
  EXPECT_EQ(Specificity(".a :is(.c#d, .e)"), Specificity(".a .c#d"));
  EXPECT_EQ(Specificity(":is(.e+.f, .g>.b, .h)"), Specificity(".e+.f"));
  EXPECT_EQ(Specificity(".a :is(.e+.f, .g>.b, .h#i)"), Specificity(".a .h#i"));
  EXPECT_EQ(Specificity(".a+:is(.b+span.f, :is(.c>.e, .g))"),
            Specificity(".a+.b+span.f"));
  EXPECT_EQ(Specificity("div > :is(div:where(span:where(.b ~ .c)))"),
            Specificity("div > div"));
  EXPECT_EQ(Specificity(":is(.c + .c + .c, .b + .c:not(span), .b + .c + .e)"),
            Specificity(".c + .c + .c"));
}

TEST(CSSSelector, Specificity_Where) {
  EXPECT_EQ(Specificity(".a :where(.b, div.c)"), Specificity(".a"));
  EXPECT_EQ(Specificity(".a :where(.c#d, .e)"), Specificity(".a"));
  EXPECT_EQ(Specificity(":where(.e+.f, .g>.b, .h)"), Specificity("*"));
  EXPECT_EQ(Specificity(".a :where(.e+.f, .g>.b, .h#i)"), Specificity(".a"));
  EXPECT_EQ(Specificity("div > :where(.b+span.f, :where(.c>.e, .g))"),
            Specificity("div"));
  EXPECT_EQ(Specificity("div > :where(div:is(span:is(.b ~ .c)))"),
            Specificity("div"));
  EXPECT_EQ(
      Specificity(":where(.c + .c + .c, .b + .c:not(span), .b + .c + .e)"),
      Specificity("*"));
}

}  // namespace blink
