// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule.h"

#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_scope_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using css_test_helpers::ParseRule;

class StyleRuleTest : public PageTestBase {};

namespace {

// Find first occurrence of a simple selector with the given PseudoType,
// traversing into lists (e.g. :is()).
const CSSSelector* FindPseudoSelector(const CSSSelector* selector,
                                      CSSSelector::PseudoType pseudo_type) {
  for (const CSSSelector* s = selector; s; s = s->NextSimpleSelector()) {
    if (s->GetPseudoType() == pseudo_type) {
      return s;
    }
    if (s->SelectorList()) {
      for (const CSSSelector* complex = s->SelectorList()->First(); complex;
           complex = CSSSelectorList::Next(*complex)) {
        if (const CSSSelector* parent =
                FindPseudoSelector(complex, pseudo_type)) {
          return parent;
        }
      }
    }
  }
  return nullptr;
}

const CSSSelector* FindParentSelector(const CSSSelector* selector) {
  return FindPseudoSelector(selector, CSSSelector::kPseudoParent);
}

const CSSSelector* FindUnparsedSelector(const CSSSelector* selector) {
  return FindPseudoSelector(selector, CSSSelector::kPseudoUnparsed);
}

// Finds the CSSNestingType (as captured by the first kPseudoUnparsed selector)
// and the parent rule for nesting (as captured by the first kPseudoParent
// selector).
std::pair<CSSNestingType, const StyleRule*> FindNestingContext(
    const CSSSelector* selector) {
  const CSSSelector* unparsed_selector = FindUnparsedSelector(selector);
  const CSSSelector* parent_selector = FindParentSelector(selector);
  return std::make_pair<CSSNestingType, const StyleRule*>(
      unparsed_selector ? unparsed_selector->GetNestingType()
                        : CSSNestingType::kNone,
      parent_selector ? parent_selector->ParentRule() : nullptr);
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

TEST_F(StyleRuleTest, SetPreludeTextReparentsStyleRules) {
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

  // Note that CSSNestingType::kNone here refers to the nesting context outside
  // of `scope_rule` (which in this case has no parent rule).
  scope_rule->SetPreludeText(GetDocument().GetExecutionContext(),
                             "(.x) to (.b &)", CSSNestingType::kNone,
                             /* parent_rule_for_nesting */ nullptr,
                             /* is_within_scope */ false,
                             /* style_sheet */ nullptr);

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

TEST_F(StyleRuleTest, SetPreludeTextWithEscape) {
  auto* scope_rule = DynamicTo<StyleRuleScope>(
      css_test_helpers::ParseRule(GetDocument(), R"CSS(
      @scope (.a) to (.b &) {
        .c & { }
      }
    )CSS"));

  // Don't crash.
  scope_rule->SetPreludeText(GetDocument().GetExecutionContext(),
                             "(.x) to (.\\1F60A)", CSSNestingType::kNone,
                             /* parent_rule_for_nesting */ nullptr,
                             /* is_within_scope */ false,
                             /* style_sheet */ nullptr);
}

TEST_F(StyleRuleTest, SetPreludeTextPreservesNestingContext) {
  CSSStyleSheet* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
  // Note that this test is making use of the fact that unparsed
  // :is()-arguments that contain either & or :scope *capture* whether they
  // contained & or :scope.
  //
  // See CSSSelector::SetUnparsedPlaceholder and CSSSelector::GetNestingType.
  sheet->SetText(R"CSS(
      div {
        @scope (:is(&, !&)) {
          .b {}
        }
      }

      @scope (div) {
        @scope (:is(&, !:scope)) {
          .b {}
        }
      }
    )CSS",
                 CSSImportRules::kIgnoreWithWarning);

  DummyExceptionStateForTesting exception_state;
  CSSRuleList* rules = sheet->rules(exception_state);
  ASSERT_TRUE(rules && rules->length() == 2u);

  // Nesting case (&).
  {
    auto* style_rule = DynamicTo<CSSStyleRule>(rules->item(0));
    ASSERT_TRUE(style_rule && style_rule->length() == 1u);
    auto* scope_rule = DynamicTo<CSSScopeRule>(style_rule->Item(0));
    ASSERT_TRUE(scope_rule);

    // Verify that SetPreludeText preservers nesting type and parent rule for
    // nesting.
    const auto& [nesting_type_before, parent_rule_before] = FindNestingContext(
        scope_rule->GetStyleRuleScope().GetStyleScope().From());
    EXPECT_EQ(CSSNestingType::kNesting, nesting_type_before);
    EXPECT_TRUE(parent_rule_before);
    scope_rule->SetPreludeText(GetDocument().GetExecutionContext(),
                               "(:is(.x, &, !&))");
    const auto& [nesting_type_after, parent_rule_after] = FindNestingContext(
        scope_rule->GetStyleRuleScope().GetStyleScope().From());
    EXPECT_EQ(nesting_type_before, nesting_type_after);
    EXPECT_EQ(parent_rule_before, parent_rule_after);
  }

  // @scope case
  {
    auto* outer_scope_rule = DynamicTo<CSSScopeRule>(rules->item(1));
    ASSERT_TRUE(outer_scope_rule && outer_scope_rule->length() == 1u);
    auto* inner_scope_rule = DynamicTo<CSSScopeRule>(outer_scope_rule->Item(0));
    ASSERT_TRUE(inner_scope_rule);

    // Verify that SetPreludeText preservers nesting type and parent rule for
    // nesting.
    const auto& [nesting_type_before, parent_rule_before] = FindNestingContext(
        inner_scope_rule->GetStyleRuleScope().GetStyleScope().From());
    EXPECT_EQ(CSSNestingType::kScope, nesting_type_before);
    EXPECT_TRUE(parent_rule_before);
    inner_scope_rule->SetPreludeText(GetDocument().GetExecutionContext(),
                                     "(:is(.x, &, !:scope))");
    const auto& [nesting_type_after, parent_rule_after] = FindNestingContext(
        inner_scope_rule->GetStyleRuleScope().GetStyleScope().From());
    EXPECT_EQ(nesting_type_before, nesting_type_after);
    EXPECT_EQ(parent_rule_before, parent_rule_after);
  }
}

TEST_F(StyleRuleTest, SetPreludeTextPreservesImplicitScope) {
  CSSStyleSheet* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
  sheet->SetText(R"CSS(
      @scope {
        .a {}
      }
    )CSS",
                 CSSImportRules::kIgnoreWithWarning);

  DummyExceptionStateForTesting exception_state;
  CSSRuleList* rules = sheet->rules(exception_state);
  ASSERT_TRUE(rules && rules->length() == 1u);
  auto* scope_rule = DynamicTo<CSSScopeRule>(rules->item(0));
  ASSERT_TRUE(scope_rule);

  EXPECT_TRUE(scope_rule->GetStyleRuleScope().GetStyleScope().IsImplicit());
  scope_rule->SetPreludeText(GetDocument().GetExecutionContext(), "");
  EXPECT_TRUE(scope_rule->GetStyleRuleScope().GetStyleScope().IsImplicit());
}

TEST_F(StyleRuleTest, SetPreludeTextBecomesImplicitScope) {
  CSSStyleSheet* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
  sheet->SetText(R"CSS(
      @scope (.a) {
        .b {}
      }
    )CSS",
                 CSSImportRules::kIgnoreWithWarning);

  DummyExceptionStateForTesting exception_state;
  CSSRuleList* rules = sheet->rules(exception_state);
  ASSERT_TRUE(rules && rules->length() == 1u);
  auto* scope_rule = DynamicTo<CSSScopeRule>(rules->item(0));
  ASSERT_TRUE(scope_rule);

  EXPECT_FALSE(scope_rule->GetStyleRuleScope().GetStyleScope().IsImplicit());
  scope_rule->SetPreludeText(GetDocument().GetExecutionContext(), "");
  EXPECT_TRUE(scope_rule->GetStyleRuleScope().GetStyleScope().IsImplicit());
}

TEST_F(StyleRuleTest, SetPreludeTextBecomesNonImplicitScope) {
  CSSStyleSheet* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
  sheet->SetText(R"CSS(
      @scope {
        .b {}
      }
    )CSS",
                 CSSImportRules::kIgnoreWithWarning);

  DummyExceptionStateForTesting exception_state;
  CSSRuleList* rules = sheet->rules(exception_state);
  ASSERT_TRUE(rules && rules->length() == 1u);
  auto* scope_rule = DynamicTo<CSSScopeRule>(rules->item(0));
  ASSERT_TRUE(scope_rule);

  EXPECT_TRUE(scope_rule->GetStyleRuleScope().GetStyleScope().IsImplicit());
  scope_rule->SetPreludeText(GetDocument().GetExecutionContext(), "(.a)");
  EXPECT_FALSE(scope_rule->GetStyleRuleScope().GetStyleScope().IsImplicit());
}

}  // namespace blink
