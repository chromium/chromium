// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_rule.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_container_condition.h"
#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class ContainerRuleTest : public PageTestBase {};

TEST_F(ContainerRuleTest, SetConditionText_conditions) {
  SetHtmlInnerHTML(R"CSS(
    <style>
      @container --name1 {}
    </style>
  )CSS");
  UpdateAllLifecyclePhasesForTest();

  auto* style = DynamicTo<HTMLStyleElement>(
      GetDocument().QuerySelector(AtomicString("style")));
  ASSERT_TRUE(style);
  CSSStyleSheet* sheet = style->sheet();
  ASSERT_TRUE(sheet);
  CSSRuleList* rules = sheet->cssRules(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(rules);
  ASSERT_EQ(1u, rules->length());
  auto* container_rule = To<CSSContainerRule>(rules->ItemInternal(0));

  ASSERT_EQ(1u, container_rule->conditions().size());
  ASSERT_EQ(AtomicString("--name1"), container_rule->conditions()[0]->name());
  ASSERT_EQ(AtomicString(""), container_rule->conditions()[0]->query());

  container_rule->SetConditionText(GetDocument().GetExecutionContext(),
                                   String("style(--foo: bar)"));

  ASSERT_EQ(1u, container_rule->conditions().size());
  ASSERT_EQ(AtomicString(""), container_rule->conditions()[0]->name());
  ASSERT_EQ(AtomicString("style(--foo: bar)"),
            container_rule->conditions()[0]->query());
}

TEST_F(ContainerRuleTest, SetQueryText_conditions) {
  SetHtmlInnerHTML(R"CSS(
    <style>
      @container --name1 {}
    </style>
  )CSS");
  UpdateAllLifecyclePhasesForTest();

  auto* style = DynamicTo<HTMLStyleElement>(
      GetDocument().QuerySelector(AtomicString("style")));
  ASSERT_TRUE(style);
  CSSStyleSheet* sheet = style->sheet();
  ASSERT_TRUE(sheet);
  CSSRuleList* rules = sheet->cssRules(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(rules);
  ASSERT_EQ(1u, rules->length());
  auto* container_rule = To<CSSContainerRule>(rules->ItemInternal(0));

  ASSERT_EQ(1u, container_rule->conditions().size());
  ASSERT_EQ(AtomicString("--name1"), container_rule->conditions()[0]->name());
  ASSERT_EQ(AtomicString(""), container_rule->conditions()[0]->query());

  container_rule->SetQueryText(GetDocument().GetExecutionContext(),
                               String("style(--foo: bar)"));

  ASSERT_EQ(1u, container_rule->conditions().size());
  ASSERT_EQ(AtomicString("--name1"), container_rule->conditions()[0]->name());
  ASSERT_EQ(AtomicString("style(--foo: bar)"),
            container_rule->conditions()[0]->query());
}

}  // namespace blink
