// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule.h"

#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class StyleRuleTest : public PageTestBase {};

namespace {

const CSSSelector* FindParentSelector(const CSSSelector* selector) {
  for (const CSSSelector* s = selector; s; s = s->NextSimpleSelector()) {
    if (s->GetPseudoType() == CSSSelector::kPseudoParent) {
      return s;
    }
  }
  return nullptr;
}

}  // namespace

TEST_F(StyleRuleTest, StyleRulePropertyCopy) {
  auto* base_rule = css_test_helpers::ParseRule(GetDocument(), R"CSS(
      @property --foo {
        syntax: "<length>";
        initial-value: 0px;
        inherits: false;
      }
    )CSS");

  ASSERT_TRUE(base_rule);
  auto* base_copy = base_rule->Copy();

  EXPECT_NE(base_rule, base_copy);
  EXPECT_EQ(base_rule->GetType(), base_copy->GetType());

  auto* rule = DynamicTo<StyleRuleProperty>(base_rule);
  auto* copy = DynamicTo<StyleRuleProperty>(base_copy);

  ASSERT_TRUE(rule);
  ASSERT_TRUE(copy);

  EXPECT_EQ(rule->GetName(), copy->GetName());
  EXPECT_EQ(rule->GetSyntax(), copy->GetSyntax());
  EXPECT_EQ(rule->Inherits(), copy->Inherits());
  EXPECT_EQ(rule->GetInitialValue(), copy->GetInitialValue());
}

TEST_F(StyleRuleTest, StyleRuleScopeSetPreludeTextNesting) {
  auto* scope_rule = DynamicTo<StyleRuleScope>(
      css_test_helpers::ParseRule(GetDocument(), R"CSS(
      @scope (.a) to (.b &) {
        .c & { }
      }
    )CSS"));

  ASSERT_TRUE(scope_rule);
  ASSERT_EQ(1u, scope_rule->ChildRules().size());
  StyleRule& child_rule = To<StyleRule>(*scope_rule->ChildRules()[0]);

  const StyleScope& scope_before = scope_rule->GetStyleScope();
  StyleRule* rule_before = scope_before.RuleForNesting();
  ASSERT_TRUE(rule_before);
  EXPECT_EQ(".a", rule_before->SelectorsText());

  EXPECT_EQ(rule_before, FindParentSelector(scope_before.To())->ParentRule());
  EXPECT_EQ(rule_before,
            FindParentSelector(child_rule.FirstSelector())->ParentRule());

  scope_rule->SetPreludeText(GetDocument().GetExecutionContext(),
                             "(.x) to (.b &)");

  const StyleScope& scope_after = scope_rule->GetStyleScope();
  StyleRule* rule_after = scope_after.RuleForNesting();
  ASSERT_TRUE(rule_after);
  EXPECT_EQ(".x", rule_after->SelectorsText());

  // Verify that '&' (in '.b &') now points to `rule_after`.
  EXPECT_EQ(rule_after, FindParentSelector(scope_after.To())->ParentRule());
  // Verify that '&' (in '.c &') now points to `rule_after`.
  EXPECT_EQ(rule_after,
            FindParentSelector(child_rule.FirstSelector())->ParentRule());
}

}  // namespace blink
