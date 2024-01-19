// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_page_rule.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(CSSPageRule, Serializing) {
  test::TaskEnvironment task_environment;
  css_test_helpers::TestStyleSheet sheet;

  const char* css_rule = "@page :left { size: auto; }";
  sheet.AddCSSRules(css_rule);
  if (sheet.CssRules()) {
    EXPECT_EQ(1u, sheet.CssRules()->length());
    EXPECT_EQ(String(css_rule), sheet.CssRules()->item(0)->cssText());
    EXPECT_EQ(CSSRule::kPageRule, sheet.CssRules()->item(0)->GetType());
    auto* page_rule = To<CSSPageRule>(sheet.CssRules()->item(0));
    EXPECT_EQ(":left", page_rule->selectorText());
  }
}

TEST(CSSPageRule, selectorText) {
  test::TaskEnvironment task_environment;
  css_test_helpers::TestStyleSheet sheet;

  const char* css_rule = "@page :left { size: auto; }";
  sheet.AddCSSRules(css_rule);
  DCHECK(sheet.CssRules());
  EXPECT_EQ(1u, sheet.CssRules()->length());

  auto* page_rule = To<CSSPageRule>(sheet.CssRules()->item(0));
  EXPECT_EQ(":left", page_rule->selectorText());
  auto* context = MakeGarbageCollected<NullExecutionContext>();

  // set invalid page selector.
  page_rule->setSelectorText(context, ":hover");
  EXPECT_EQ(":left", page_rule->selectorText());

  // set invalid page selector.
  page_rule->setSelectorText(context, "right { bla");
  EXPECT_EQ(":left", page_rule->selectorText());

  // set page pseudo class selector.
  page_rule->setSelectorText(context, ":right");
  EXPECT_EQ(":right", page_rule->selectorText());

  // set page type selector.
  page_rule->setSelectorText(context, "namedpage");
  EXPECT_EQ("namedpage", page_rule->selectorText());

  context->NotifyContextDestroyed();
}

TEST(CSSPageRule, MarginRules) {
  ScopedPageMarginBoxesForTest enabled(true);
  test::TaskEnvironment task_environment;
  css_test_helpers::TestStyleSheet sheet;

  const char* css_rule =
      "@page { size: auto; @top-right { content: \"fisk\"; } }";
  sheet.AddCSSRules(css_rule);
  ASSERT_TRUE(sheet.CssRules());
  EXPECT_EQ(1u, sheet.CssRules()->length());
  EXPECT_EQ(String(css_rule), sheet.CssRules()->item(0)->cssText());
  EXPECT_EQ(CSSRule::kPageRule, sheet.CssRules()->item(0)->GetType());
  auto* page_rule = To<CSSPageRule>(sheet.CssRules()->item(0));
  EXPECT_EQ("", page_rule->selectorText());
}

TEST(CSSPageRule, MarginRulesInvalidPrelude) {
  ScopedPageMarginBoxesForTest enabled(true);
  test::TaskEnvironment task_environment;
  css_test_helpers::TestStyleSheet sheet;

  const char* css_rule =
      "@page { size: auto; @top-right invalid { content: \"fisk\"; } }";
  sheet.AddCSSRules(css_rule);
  ASSERT_TRUE(sheet.CssRules());
  EXPECT_EQ(1u, sheet.CssRules()->length());
  EXPECT_EQ("@page { size: auto; }", sheet.CssRules()->item(0)->cssText());
  EXPECT_EQ(CSSRule::kPageRule, sheet.CssRules()->item(0)->GetType());
}

TEST(CSSPageRule, MarginRulesIgnoredWhenDisabled) {
  ScopedPageMarginBoxesForTest enabled(false);
  test::TaskEnvironment task_environment;
  css_test_helpers::TestStyleSheet sheet;

  const char* css_rule =
      "@page { size: auto; @top-right { content: \"fisk\"; margin-bottom: 1cm; "
      "} margin-top: 2cm; }";
  sheet.AddCSSRules(css_rule);
  ASSERT_TRUE(sheet.CssRules());
  EXPECT_EQ(1u, sheet.CssRules()->length());
  EXPECT_EQ("@page { size: auto; margin-top: 2cm; }",
            sheet.CssRules()->item(0)->cssText());
  EXPECT_EQ(CSSRule::kPageRule, sheet.CssRules()->item(0)->GetType());
  auto* page_rule = To<CSSPageRule>(sheet.CssRules()->item(0));
  EXPECT_EQ("", page_rule->selectorText());
}

}  // namespace blink
