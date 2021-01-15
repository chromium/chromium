// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"

#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ContainerQueryTest : public PageTestBase,
                           private ScopedCSSContainerQueriesForTest {
 public:
  ContainerQueryTest() : ScopedCSSContainerQueriesForTest(true) {}

  StyleRuleContainer* ParseAtContainer(String rule_string) {
    return DynamicTo<StyleRuleContainer>(
        css_test_helpers::ParseRule(GetDocument(), rule_string));
  }

  String SerializeCondition(StyleRuleContainer* container) {
    if (!container)
      return "";
    return container->GetContainerQuery().ToString();
  }

  // TODO(crbug.com/1145970): Remove this when ContainerQuery no longer
  // relies on MediaQuerySet.
  MediaQuerySet& GetMediaQuerySet(ContainerQuery& container_query) {
    return *container_query.media_queries_;
  }
};

TEST_F(ContainerQueryTest, PreludeParsing) {
  // Valid:
  EXPECT_EQ(
      "(min-width: 300px)",
      SerializeCondition(ParseAtContainer("@container (min-width: 300px) {}")));
  EXPECT_EQ(
      "(max-width: 500px)",
      SerializeCondition(ParseAtContainer("@container (max-width: 500px) {}")));

  // TODO(crbug.com/1145970): The MediaQuery parser emits a "not all"
  // MediaQuery for parse failures. When ContainerQuery has its own parser,
  // it should probably return nullptr instead.

  // Invalid:
  EXPECT_EQ("not all",
            SerializeCondition(ParseAtContainer("@container 100px {}")));
  EXPECT_EQ("not all",
            SerializeCondition(ParseAtContainer("@container calc(1) {}")));
}

TEST_F(ContainerQueryTest, RuleParsing) {
  StyleRuleContainer* container = ParseAtContainer(R"CSS(
    @container (min-width: 100px) {
      div { width: 100px; }
      span { height: 100px; }
    }
  )CSS");
  ASSERT_TRUE(container);

  CSSStyleSheet* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
  auto* rule =
      DynamicTo<CSSContainerRule>(container->CreateCSSOMWrapper(sheet));
  ASSERT_TRUE(rule);
  ASSERT_EQ(2u, rule->length());

  auto* div_rule = rule->Item(0);
  ASSERT_TRUE(div_rule);
  EXPECT_EQ("div { width: 100px; }", div_rule->cssText());

  auto* span_rule = rule->Item(1);
  ASSERT_TRUE(span_rule);
  EXPECT_EQ("span { height: 100px; }", span_rule->cssText());
}

TEST_F(ContainerQueryTest, RuleCopy) {
  StyleRuleContainer* container = ParseAtContainer(R"CSS(
    @container (min-width: 100px) {
      div { width: 100px; }
    }
  )CSS");
  ASSERT_TRUE(container);

  // Copy via StyleRuleBase to test switch dispatch.
  auto* copy_base = static_cast<StyleRuleBase*>(container)->Copy();
  auto* copy = DynamicTo<StyleRuleContainer>(copy_base);
  ASSERT_TRUE(copy);

  // The StyleRuleContainer object should be copied.
  EXPECT_NE(container, copy);

  // The rules should be copied.
  auto rules = container->ChildRules();
  auto rules_copy = copy->ChildRules();
  ASSERT_EQ(1u, rules.size());
  ASSERT_EQ(1u, rules_copy.size());
  EXPECT_NE(rules[0], rules_copy[0]);

  // The ContainerQuery should be copied.
  EXPECT_NE(&container->GetContainerQuery(), &copy->GetContainerQuery());

  // The inner MediaQuerySet should be copied.
  EXPECT_NE(&GetMediaQuerySet(container->GetContainerQuery()),
            &GetMediaQuerySet(copy->GetContainerQuery()));
}

TEST_F(ContainerQueryTest, ContainerQueryEvaluation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        contain: size layout;
        width: 500px;
        height: 500px;
      }
      #container.adjust {
        width: 600px;
      }

      div { z-index:1; }
      /* Should apply: */
      @container (min-width: 500px) {
        div { z-index:2; }
      }
      /* Should initially not apply: */
      @container (min-width: 600px) {
        div { z-index:3; }
      }
    </style>
    <div id=container>
      <div id=div></div>
    </div>
  )HTML");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  EXPECT_EQ(2, div->ComputedStyleRef().ZIndex());

  // Check that dependent elements are responsive to changes:
  Element* container = GetDocument().getElementById("container");
  ASSERT_TRUE(container);
  container->setAttribute(html_names::kClassAttr, "adjust");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(3, div->ComputedStyleRef().ZIndex());

  container->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2, div->ComputedStyleRef().ZIndex());
}

}  // namespace blink
