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
  CSSStyleSheet* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
  auto* scope_rule = DynamicTo<CSSScopeRule>(
      css_test_helpers::ParseRule(GetDocument(), R"CSS(
      @scope (.a) to (.b &) {
        .c & { }
      }
    )CSS")
          ->CreateCSSOMWrapper(/*position_hint=*/0, sheet));

  ASSERT_TRUE(scope_rule);
  ASSERT_EQ(1u, scope_rule->GetStyleRuleScope().ChildRules().size());
  StyleRule& child_rule_before =
      To<StyleRule>(*scope_rule->GetStyleRuleScope().ChildRules()[0]);

  const StyleScope& scope_before =
      scope_rule->GetStyleRuleScope().GetStyleScope();
  StyleRule* rule_before = scope_before.RuleForNesting();
  ASSERT_TRUE(rule_before);
  EXPECT_EQ(".a", rule_before->SelectorsText());

  EXPECT_EQ(rule_before, FindParentSelector(scope_before.To())->ParentRule());
  EXPECT_EQ(
      rule_before,
      FindParentSelector(child_rule_before.FirstSelector())->ParentRule());

  scope_rule->SetPreludeText(GetDocument().GetExecutionContext(),
                             "(.x) to (.b &)");

  DLOG(INFO) << "A";
  const StyleScope& scope_after =
      scope_rule->GetStyleRuleScope().GetStyleScope();
  StyleRule* rule_after = scope_after.RuleForNesting();
  ASSERT_TRUE(rule_after);
  EXPECT_EQ(".x", rule_after->SelectorsText());
  StyleRule& child_rule_afer =
      To<StyleRule>(*scope_rule->GetStyleRuleScope().ChildRules()[0]);

  // Verify that '&' (in '.b &') now points to `rule_after`.
  EXPECT_EQ(rule_after, FindParentSelector(scope_after.To())->ParentRule());
  // Verify that '&' (in '.c &') now points to `rule_after`.
  EXPECT_EQ(rule_after,
            FindParentSelector(child_rule_afer.FirstSelector())->ParentRule());
}

TEST_F(StyleRuleTest, SetPreludeTextWithEscape) {
  CSSStyleSheet* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
  auto* scope_rule = DynamicTo<CSSScopeRule>(
      css_test_helpers::ParseRule(GetDocument(), R"CSS(
      @scope (.a) to (.b &) {
        .c & { }
      }
    )CSS")
          ->CreateCSSOMWrapper(/*position_hint=*/0, sheet));

  // Don't crash.
  scope_rule->SetPreludeText(GetDocument().GetExecutionContext(),
                             "(.x) to (.\\1F60A)");
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

TEST_F(StyleRuleTest, SetPreludeTextInvalid) {
  CSSStyleSheet* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
  auto* css_scope_rule = DynamicTo<CSSScopeRule>(
      css_test_helpers::ParseRule(GetDocument(), "@scope (.a) {}")
          ->CreateCSSOMWrapper(/*position_hint=*/0, sheet));

  StyleRuleScope* before_rule = &css_scope_rule->GetStyleRuleScope();
  // Don't crash:
  css_scope_rule->SetPreludeText(GetDocument().GetExecutionContext(),
                                 "(.a) to !!!!");
  StyleRuleScope* after_rule = &css_scope_rule->GetStyleRuleScope();
  EXPECT_EQ(after_rule, before_rule);
}

TEST_F(StyleRuleTest, SetPreludeTextUnexpectedTrailingTokens) {
  CSSStyleSheet* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
  auto* css_scope_rule = DynamicTo<CSSScopeRule>(
      css_test_helpers::ParseRule(GetDocument(), "@scope (.a) {}")
          ->CreateCSSOMWrapper(/*position_hint=*/0, sheet));

  StyleRuleScope* before_rule = &css_scope_rule->GetStyleRuleScope();
  // Don't crash:
  css_scope_rule->SetPreludeText(GetDocument().GetExecutionContext(),
                                 "(.a) to (.b) trailing");
  StyleRuleScope* after_rule = &css_scope_rule->GetStyleRuleScope();
  EXPECT_EQ(after_rule, before_rule);
}

TEST_F(StyleRuleTest, RenestStyleRule) {
  auto* a = To<StyleRule>(css_test_helpers::ParseRule(GetDocument(), ".a {}"));
  auto* b = To<StyleRule>(css_test_helpers::ParseRule(GetDocument(), ".b {}"));
  auto* nested = To<StyleRule>(css_test_helpers::ParseNestedRule(
      GetDocument(), "& {}", CSSNestingType::kNesting,
      /*parent_rule_for_nesting=*/a));

  EXPECT_EQ(":is(.a)",
            nested->FirstSelector()->SelectorTextExpandingPseudoReferences(
                /*scope_id=*/0));

  auto* reparented = To<StyleRule>(nested->Renest(b));
  EXPECT_NE(nested, reparented);
  EXPECT_EQ(":is(.a)",
            nested->FirstSelector()->SelectorTextExpandingPseudoReferences(
                /*scope_id=*/0));
  EXPECT_EQ(":is(.b)",
            reparented->FirstSelector()->SelectorTextExpandingPseudoReferences(
                /*scope_id=*/0));
}

TEST_F(StyleRuleTest, RenestStyleRuleNoOp) {
  auto* a = To<StyleRule>(css_test_helpers::ParseRule(GetDocument(), ".a {}"));
  auto* nested = To<StyleRule>(css_test_helpers::ParseNestedRule(
      GetDocument(), "& {}", CSSNestingType::kNesting,
      /*parent_rule_for_nesting=*/a));
  EXPECT_EQ(":is(.a)",
            nested->FirstSelector()->SelectorTextExpandingPseudoReferences(
                /*scope_id=*/0));
  auto* reparented = To<StyleRule>(nested->Renest(a));
  EXPECT_EQ(nested, reparented);
}

TEST_F(StyleRuleTest, RenestStyleRuleMedia) {
  auto* a = To<StyleRule>(css_test_helpers::ParseRule(GetDocument(), ".a {}"));
  auto* b = To<StyleRule>(css_test_helpers::ParseRule(GetDocument(), ".b {}"));
  auto* media = To<StyleRuleMedia>(css_test_helpers::ParseNestedRule(
      GetDocument(), "@media (width) { & {} }", CSSNestingType::kNesting,
      /*parent_rule_for_nesting=*/a));

  ASSERT_EQ(1u, media->ChildRules().size());
  EXPECT_EQ(":is(.a)",
            To<StyleRule>(media->ChildRules().front().Get())
                ->FirstSelector()
                ->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));

  EXPECT_EQ(media->Renest(a), media);  // No-op.

  auto* reparented = To<StyleRuleMedia>(media->Renest(b));
  EXPECT_NE(media, reparented);
  EXPECT_EQ(":is(.a)",
            To<StyleRule>(media->ChildRules().front().Get())
                ->FirstSelector()
                ->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));
  EXPECT_EQ(":is(.b)",
            To<StyleRule>(reparented->ChildRules().front().Get())
                ->FirstSelector()
                ->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));
}

TEST_F(StyleRuleTest, RenestStyleRuleStartingStyle) {
  auto* a = To<StyleRule>(css_test_helpers::ParseRule(GetDocument(), ".a {}"));
  auto* b = To<StyleRule>(css_test_helpers::ParseRule(GetDocument(), ".b {}"));
  auto* starting_style =
      To<StyleRuleStartingStyle>(css_test_helpers::ParseNestedRule(
          GetDocument(), "@starting-style { & {} }", CSSNestingType::kNesting,
          /*parent_rule_for_nesting=*/a));

  ASSERT_EQ(1u, starting_style->ChildRules().size());
  EXPECT_EQ(":is(.a)",
            To<StyleRule>(starting_style->ChildRules().front().Get())
                ->FirstSelector()
                ->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));

  EXPECT_EQ(starting_style->Renest(a), starting_style);  // No-op.

  auto* reparented = To<StyleRuleStartingStyle>(starting_style->Renest(b));
  EXPECT_NE(starting_style, reparented);
  EXPECT_EQ(":is(.a)",
            To<StyleRule>(starting_style->ChildRules().front().Get())
                ->FirstSelector()
                ->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));
  EXPECT_EQ(":is(.b)",
            To<StyleRule>(reparented->ChildRules().front().Get())
                ->FirstSelector()
                ->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));
}

}  // namespace blink
