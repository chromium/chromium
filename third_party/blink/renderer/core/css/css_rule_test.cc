// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_rule.h"

#include "third_party/blink/renderer/core/css/css_grouping_rule.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CSSRuleTest : public PageTestBase {};

TEST_F(CSSRuleTest, QuietlyInsertRemove_CSSStyleRule) {
  SetHtmlInnerHTML(R"CSS(
    <style>
      span { z-index: 1; }
      div { }
    </style>
    <div>
      <span></span>
    </div>
  )CSS");
  UpdateAllLifecyclePhasesForTest();

  auto* style = DynamicTo<HTMLStyleElement>(
      GetDocument().QuerySelector(AtomicString("style")));
  Element* span = GetDocument().QuerySelector(AtomicString("span"));

  ASSERT_TRUE(style);
  CSSStyleSheet* sheet = style->sheet();
  CSSRuleList* rules = sheet->cssRules(ASSERT_NO_EXCEPTION);
  ASSERT_EQ(2u, rules->length());
  auto* div_rule = DynamicTo<CSSStyleRule>(rules->ItemInternal(1));
  EXPECT_EQ("div", div_rule->selectorText());
  EXPECT_EQ(0u, div_rule->cssRules()->length());

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  // Quietly inserting a rule should not make anything dirty.
  div_rule->QuietlyInsertRule(GetDocument().GetExecutionContext(),
                              "span{z-index:2;}", /*index=*/0u);
  EXPECT_EQ(1u, div_rule->cssRules()->length());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(span->ComputedStyleRef().ZIndex(), 1);

  // Quietly removing it should also not make anything dirty.
  div_rule->QuietlyDeleteRule(/*index=*/0u);
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, div_rule->cssRules()->length());
  EXPECT_EQ(span->ComputedStyleRef().ZIndex(), 1);
}

TEST_F(CSSRuleTest, QuietlyInsertRemove_CSSGroupingRule) {
  SetHtmlInnerHTML(R"CSS(
    <style>
      span { z-index: 1; }
      @media (width > 0px) { }
    </style>
    <div>
      <span></span>
    </div>
  )CSS");
  UpdateAllLifecyclePhasesForTest();

  auto* style = DynamicTo<HTMLStyleElement>(
      GetDocument().QuerySelector(AtomicString("style")));
  Element* span = GetDocument().QuerySelector(AtomicString("span"));

  ASSERT_TRUE(style);
  CSSStyleSheet* sheet = style->sheet();
  CSSRuleList* rules = sheet->cssRules(ASSERT_NO_EXCEPTION);
  ASSERT_EQ(2u, rules->length());
  auto* media_rule = DynamicTo<CSSGroupingRule>(rules->ItemInternal(1));
  EXPECT_EQ(0u, media_rule->cssRules()->length());

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  // Quietly inserting a rule should not make anything dirty.
  media_rule->QuietlyInsertRule(GetDocument().GetExecutionContext(),
                                "span{z-index:2;}", /*index=*/0u);
  EXPECT_EQ(1u, media_rule->cssRules()->length());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(span->ComputedStyleRef().ZIndex(), 1);

  // Quietly removing it should also not make anything dirty.
  media_rule->QuietlyDeleteRule(/*index=*/0u);
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, media_rule->cssRules()->length());
  EXPECT_EQ(span->ComputedStyleRef().ZIndex(), 1);
}

}  // namespace blink
