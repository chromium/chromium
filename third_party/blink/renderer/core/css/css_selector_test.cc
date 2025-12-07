// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using css_test_helpers::ParseRule;
using css_test_helpers::ParseSelectorList;

namespace {

unsigned Specificity(const String& selector_text) {
  CSSSelectorList* selector_list =
      css_test_helpers::ParseSelectorList(selector_text);
  return selector_list->First()->Specificity();
}

bool HasLinkOrVisited(const String& selector_text) {
  CSSSelectorList* selector_list =
      css_test_helpers::ParseSelectorList(selector_text);
  return selector_list->First()->HasLinkOrVisited();
}

}  // namespace

TEST(CSSSelector, Representations) {
  test::TaskEnvironment task_environment;
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
      "div:nth-child(2n+1 of .a){}"
      ".class#id { }"
      "#id.class { }"
      "[attr]#id { }"
      "div[attr]#id { }"
      "div::first-line { }"
      ".a.b.c { }"
      "div:not(.a) { }"        // without class a
      "div:not(:visited) { }"  // without the visited pseudo-class

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
  test::TaskEnvironment task_environment;
  int max_int = std::numeric_limits<int>::max();
  int min_int = std::numeric_limits<int>::min();
  CSSSelector selector;

  // Overflow count - b (max_int - -1 = max_int + 1)
  selector.SetNth(1, -1, /*sub_selector=*/nullptr);
  EXPECT_FALSE(selector.MatchNth(max_int));
  // 0 - (min_int) = max_int + 1
  selector.SetNth(1, min_int, /*sub_selector=*/nullptr);
  EXPECT_FALSE(selector.MatchNth(0));

  // min_int - 1
  selector.SetNth(-1, min_int, /*sub_selector=*/nullptr);
  EXPECT_FALSE(selector.MatchNth(1));

  // a shouldn't negate to itself (and min_int negates to itself).
  // Note: This test can only fail when using ubsan.
  selector.SetNth(min_int, 10, /*sub_selector=*/nullptr);
  EXPECT_FALSE(selector.MatchNth(2));
}

TEST(CSSSelector, Specificity_Is) {
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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

TEST(CSSSelector, Specificity_Slotted) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(Specificity("::slotted(.a)"), Specificity(".a::first-line"));
  EXPECT_EQ(Specificity("::slotted(*)"), Specificity("::first-line"));
}

TEST(CSSSelector, Specificity_Host) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(Specificity(":host"), Specificity(".host"));
  EXPECT_EQ(Specificity(":host(.a)"), Specificity(".host .a"));
  EXPECT_EQ(Specificity(":host(div#a.b)"), Specificity(".host div#a.b"));
}

TEST(CSSSelector, Specificity_HostContext) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(Specificity(":host-context(.a)"), Specificity(".host-context .a"));
  EXPECT_EQ(Specificity(":host-context(div#a.b)"),
            Specificity(".host-context div#a.b"));
}

TEST(CSSSelector, Specificity_Not) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(Specificity(":not(div)"), Specificity(":is(div)"));
  EXPECT_EQ(Specificity(":not(.a)"), Specificity(":is(.a)"));
  EXPECT_EQ(Specificity(":not(div.a)"), Specificity(":is(div.a)"));
  EXPECT_EQ(Specificity(".a :not(.b, div.c)"),
            Specificity(".a :is(.b, div.c)"));
  EXPECT_EQ(Specificity(".a :not(.c#d, .e)"), Specificity(".a :is(.c#d, .e)"));
  EXPECT_EQ(Specificity(".a :not(.e+.f, .g>.b, .h#i)"),
            Specificity(".a :is(.e+.f, .g>.b, .h#i)"));
  EXPECT_EQ(Specificity(":not(.c + .c + .c, .b + .c:not(span), .b + .c + .e)"),
            Specificity(":is(.c + .c + .c, .b + .c:not(span), .b + .c + .e)"));
}

TEST(CSSSelector, Specificity_Has) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(Specificity(":has(div)"), Specificity("div"));
  EXPECT_EQ(Specificity(":has(div)"), Specificity("* div"));
  EXPECT_EQ(Specificity(":has(~ div)"), Specificity("* ~ div"));
  EXPECT_EQ(Specificity(":has(> .a)"), Specificity("* > .a"));
  EXPECT_EQ(Specificity(":has(+ div.a)"), Specificity("* + div.a"));
  EXPECT_EQ(Specificity(".a :has(.b, div.c)"), Specificity(".a div.c"));
  EXPECT_EQ(Specificity(".a :has(.c#d, .e)"), Specificity(".a .c#d"));
  EXPECT_EQ(Specificity(":has(.e+.f, .g>.b, .h)"), Specificity(".e+.f"));
  EXPECT_EQ(Specificity(".a :has(.e+.f, .g>.b, .h#i)"), Specificity(".a .h#i"));
  EXPECT_EQ(Specificity("div > :has(div, div:where(span:where(.b ~ .c)))"),
            Specificity("div > div"));
  EXPECT_EQ(Specificity(":has(.c + .c + .c, .b + .c:not(span), .b + .c + .e)"),
            Specificity(".c + .c + .c"));
}

TEST(CSSSelector, HasLinkOrVisited) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(HasLinkOrVisited("tag"));
  EXPECT_FALSE(HasLinkOrVisited("visited"));
  EXPECT_FALSE(HasLinkOrVisited("link"));
  EXPECT_FALSE(HasLinkOrVisited(".a"));
  EXPECT_FALSE(HasLinkOrVisited("#a:is(visited)"));
  EXPECT_FALSE(HasLinkOrVisited(":not(link):hover"));
  EXPECT_FALSE(HasLinkOrVisited(":hover"));
  EXPECT_FALSE(HasLinkOrVisited(":is(:hover)"));
  EXPECT_FALSE(HasLinkOrVisited(":not(:is(:hover))"));

  EXPECT_TRUE(HasLinkOrVisited(":visited"));
  EXPECT_TRUE(HasLinkOrVisited(":link"));
  EXPECT_TRUE(HasLinkOrVisited(":visited:link"));
  EXPECT_TRUE(HasLinkOrVisited(":not(:visited)"));
  EXPECT_TRUE(HasLinkOrVisited(":not(:link)"));
  EXPECT_TRUE(HasLinkOrVisited(":not(:is(:link))"));
  EXPECT_TRUE(HasLinkOrVisited(":is(:link)"));
  EXPECT_TRUE(HasLinkOrVisited(":is(.a, .b, :is(:visited))"));
  EXPECT_TRUE(HasLinkOrVisited("::cue(:visited)"));
  EXPECT_TRUE(HasLinkOrVisited("::cue(:link)"));
  EXPECT_TRUE(HasLinkOrVisited(":host(:link)"));
  EXPECT_TRUE(HasLinkOrVisited(":host-context(:link)"));
}

TEST(CSSSelector, CueDefaultNamespace) {
  test::TaskEnvironment task_environment;
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(R"HTML(
    @namespace "http://www.w3.org/1999/xhtml";
    video::cue(b) {}
  )HTML");

  const CSSSelector& cue_selector =
      sheet.GetRuleSet().CuePseudoRules()[0].Selector();
  EXPECT_EQ(cue_selector.GetPseudoType(), CSSSelector::kPseudoCue);

  const CSSSelectorList* cue_arguments = cue_selector.SelectorList();
  ASSERT_TRUE(cue_arguments);
  const CSSSelector* vtt_type_selector = cue_arguments->First();
  ASSERT_TRUE(vtt_type_selector);
  EXPECT_EQ(vtt_type_selector->TagQName().LocalName(), "b");
  // Default namespace should not affect VTT node type selector.
  EXPECT_EQ(vtt_type_selector->TagQName().NamespaceURI(), g_star_atom);
}

TEST(CSSSelector, CopyInvalidList) {
  test::TaskEnvironment task_environment;
  CSSSelectorList* list = CSSSelectorList::Empty();
  EXPECT_FALSE(list->IsValid());
  EXPECT_FALSE(list->Copy()->IsValid());
}

TEST(CSSSelector, CopyValidList) {
  test::TaskEnvironment task_environment;
  CSSSelectorList* list = css_test_helpers::ParseSelectorList(".a");
  EXPECT_TRUE(list->IsValid());
  EXPECT_TRUE(list->Copy()->IsValid());
}

TEST(CSSSelector, FirstInInvalidList) {
  test::TaskEnvironment task_environment;
  CSSSelectorList* list = CSSSelectorList::Empty();
  EXPECT_FALSE(list->IsValid());
  EXPECT_FALSE(list->First());
}

TEST(CSSSelector, ImplicitPseudoDescendant) {
  test::TaskEnvironment task_environment;
  CSSSelector selector[2] = {
      CSSSelector(html_names::kDivTag,
                  /* is_implicit */ false),
      CSSSelector(AtomicString("scope"), /* is_implicit */ true)};
  selector[0].SetRelation(CSSSelector::kDescendant);
  selector[1].SetLastInComplexSelector(true);
  EXPECT_EQ("div", selector[0].SelectorText());
}

TEST(CSSSelector, ImplicitPseudoChild) {
  test::TaskEnvironment task_environment;
  CSSSelector selector[2] = {
      CSSSelector(html_names::kDivTag,
                  /* is_implicit */ false),
      CSSSelector(AtomicString("scope"), /* is_implicit */ true)};
  selector[0].SetRelation(CSSSelector::kChild);
  selector[1].SetLastInComplexSelector(true);
  EXPECT_EQ("> div", selector[0].SelectorText());
}

TEST(CSSSelector, NonImplicitPseudoChild) {
  test::TaskEnvironment task_environment;
  CSSSelector selector[2] = {
      CSSSelector(html_names::kDivTag,
                  /* is_implicit */ false),
      CSSSelector(AtomicString("scope"), /* is_implicit */ false)};
  selector[0].SetRelation(CSSSelector::kChild);
  selector[1].SetLastInComplexSelector(true);
  EXPECT_EQ(":scope > div", selector[0].SelectorText());
}

TEST(CSSSelector, ImplicitScopeSpecificity) {
  test::TaskEnvironment task_environment;
  CSSSelector selector[2] = {
      CSSSelector(html_names::kDivTag,
                  /* is_implicit */ false),
      CSSSelector(AtomicString("scope"), /* is_implicit */ true)};
  selector[0].SetRelation(CSSSelector::kChild);
  selector[1].SetLastInComplexSelector(true);
  EXPECT_EQ("> div", selector[0].SelectorText());
  EXPECT_EQ(CSSSelector::kTagSpecificity, selector[0].Specificity());
}

TEST(CSSSelector, ExplicitScopeSpecificity) {
  test::TaskEnvironment task_environment;
  CSSSelector selector[2] = {
      CSSSelector(html_names::kDivTag,
                  /* is_implicit */ false),
      CSSSelector(AtomicString("scope"), /* is_implicit */ false)};
  selector[0].SetRelation(CSSSelector::kChild);
  selector[1].SetLastInComplexSelector(true);
  EXPECT_EQ(":scope > div", selector[0].SelectorText());
  EXPECT_EQ(CSSSelector::kTagSpecificity | CSSSelector::kClassLikeSpecificity,
            selector[0].Specificity());
}

TEST(CSSSelector, SelectorTextExpandingPseudoReferences_Parent) {
  test::TaskEnvironment task_environment;

  css_test_helpers::TestStyleSheet sheet;
  sheet.AddCSSRules(
      ".a { .b { .c, &.c, .c:has(&) {} } }"
      ".d .e { .f:has(> &) {} }");
  RuleSet& rule_set = sheet.GetRuleSet();

  base::span<const RuleData> rules = rule_set.ClassRules(AtomicString("a"));
  ASSERT_EQ(1u, rules.size());
  const CSSSelector* selector = &rules[0].Selector();
  EXPECT_EQ(".a", selector->SelectorText());

  rules = rule_set.ClassRules(AtomicString("b"));
  ASSERT_EQ(1u, rules.size());
  selector = &rules[0].Selector();
  EXPECT_EQ("& .b", selector->SelectorText());
  EXPECT_EQ(":is(.a) .b",
            selector->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));

  rules = rule_set.ClassRules(AtomicString("c"));
  ASSERT_EQ(3u, rules.size());
  selector = &rules[0].Selector();
  EXPECT_EQ("& .c", selector->SelectorText());
  EXPECT_EQ(":is(:is(.a) .b) .c",
            selector->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));
  selector = &rules[1].Selector();
  EXPECT_EQ("&.c", selector->SelectorText());
  EXPECT_EQ(":is(:is(.a) .b).c",
            selector->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));
  selector = &rules[2].Selector();
  EXPECT_EQ(".c:has(&)", selector->SelectorText());
  EXPECT_EQ(".c:has(:is(:is(.a) .b))",
            selector->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));

  rules = rule_set.ClassRules(AtomicString("e"));
  ASSERT_EQ(1u, rules.size());
  selector = &rules[0].Selector();
  EXPECT_EQ(".d .e", selector->SelectorText());

  rules = rule_set.ClassRules(AtomicString("f"));
  ASSERT_EQ(1u, rules.size());
  selector = &rules[0].Selector();
  EXPECT_EQ(".f:has(> &)", selector->SelectorText());
  EXPECT_EQ(".f:has(> :is(.d .e))",
            selector->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));
}

TEST(CSSSelector, SelectorTextExpandingPseudoReferences_Scope) {
  test::TaskEnvironment task_environment;

  auto expanded_selector_text = [](String s) {
    return css_test_helpers::ParseSelectorList(s)
        ->First()
        ->SelectorTextExpandingPseudoReferences(/*scope_id=*/42);
  };

  EXPECT_EQ(".a .b :has(.c)", expanded_selector_text(".a .b :has(.c)"));

  EXPECT_EQ(":-internal-scope-42", expanded_selector_text(":scope"));
  EXPECT_EQ(".a:-internal-scope-42", expanded_selector_text(".a:scope"));
  EXPECT_EQ(".a :-internal-scope-42", expanded_selector_text(".a :scope"));
  EXPECT_EQ(":-internal-scope-42.a", expanded_selector_text(":scope.a"));
  EXPECT_EQ(":is(.a, :-internal-scope-42)",
            expanded_selector_text(":is(.a, :scope)"));
  EXPECT_EQ(":not(.a, :-internal-scope-42)",
            expanded_selector_text(":not(.a, :scope)"));
  EXPECT_EQ(
      ":not(.a, :-internal-scope-42)"
      ":where(:-internal-scope-42, :-internal-scope-42):-internal-scope-42",
      expanded_selector_text(":not(.a, :scope):where(:scope, :scope):scope"));
  EXPECT_EQ(":has(.b:not(:-internal-scope-42 .a *))",
            expanded_selector_text(":has(.b:not(:scope .a *))"));
}

TEST(CSSSelector, SelectorTextExpandingPseudoReferences_ImplicitScope) {
  test::TaskEnvironment task_environment;

  css_test_helpers::TestStyleSheet sheet;
  sheet.AddCSSRules(R"CSS(
    @scope (.a) {
      /* There's an implicit (non-serialized) ':scope' (plus descendant
         combinator) prepended to the selector below. */
      .b {}
    }
  )CSS");
  RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> rules = rule_set.ClassRules(AtomicString("b"));
  ASSERT_EQ(1u, rules.size());
  const CSSSelector* selector = &rules[0].Selector();
  EXPECT_EQ(".b", selector->SelectorText());
  EXPECT_EQ(":-internal-scope-99 .b",
            selector->SelectorTextExpandingPseudoReferences(/*scope_id=*/99));
}

TEST(CSSSelector, CheckHasArgumentMatchInShadowTreeFlag) {
  test::TaskEnvironment task_environment;

  css_test_helpers::TestStyleSheet sheet;
  sheet.AddCSSRules(
      ":host:has(.a) {}"
      ":has(.a):host {}"
      ":host:has(.a):has(.b) {}"
      ":has(.a):has(.b):host {}"
      ":host:has(.a) .b {}"
      ":has(.a):host .b {}"
      ":host:has(.a):has(.b) .c {}"
      ":has(.a):has(.b):host .c {}"
      ":host :has(.a) {}"
      ":host :has(.a) .b {}"
      ":host:has(.a):host(.b):has(.c):host-context(.d):has(.e) :has(.f) {}"
      ":has(.a):host:has(.b):host(.c):has(.d):host-context(.e) :has(.f) {}");
  RuleSet& rule_set = sheet.GetRuleSet();

  base::span<const RuleData> rules = rule_set.ShadowHostRules();
  ASSERT_EQ(4u, rules.size());
  const CSSSelector* selector = &rules[0].Selector();
  EXPECT_EQ(":host:has(.a)", selector->SelectorText());
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHost);
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());

  selector = &rules[1].Selector();
  EXPECT_EQ(":has(.a):host", selector->SelectorText());
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHost);

  selector = &rules[2].Selector();
  EXPECT_EQ(":host:has(.a):has(.b)", selector->SelectorText());
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHost);
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());

  selector = &rules[3].Selector();
  EXPECT_EQ(":has(.a):has(.b):host", selector->SelectorText());
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHost);

  rules = rule_set.ClassRules(AtomicString("b"));
  ASSERT_EQ(3u, rules.size());
  selector = &rules[0].Selector();
  EXPECT_EQ(":host:has(.a) .b", selector->SelectorText());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHost);
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());

  selector = &rules[1].Selector();
  EXPECT_EQ(":has(.a):host .b", selector->SelectorText());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHost);

  selector = &rules[2].Selector();
  EXPECT_EQ(":host :has(.a) .b", selector->SelectorText());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_FALSE(selector->HasArgumentMatchInShadowTree());

  rules = rule_set.ClassRules(AtomicString("c"));
  ASSERT_EQ(2u, rules.size());
  selector = &rules[0].Selector();
  EXPECT_EQ(":host:has(.a):has(.b) .c", selector->SelectorText());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHost);
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());

  selector = &rules[1].Selector();
  EXPECT_EQ(":has(.a):has(.b):host .c", selector->SelectorText());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHost);

  rules = rule_set.UniversalRules();
  ASSERT_EQ(3u, rules.size());
  selector = &rules[0].Selector();
  EXPECT_EQ(":host :has(.a)", selector->SelectorText());
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_FALSE(selector->HasArgumentMatchInShadowTree());

  selector = &rules[1].Selector();
  EXPECT_EQ(":host:has(.a):host(.b):has(.c):host-context(.d):has(.e) :has(.f)",
            selector->SelectorText());
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_FALSE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHost);
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());

  selector = &rules[2].Selector();
  EXPECT_EQ(":has(.a):host:has(.b):host(.c):has(.d):host-context(.e) :has(.f)",
            selector->SelectorText());
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_FALSE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHost);
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHost);
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHas);
  EXPECT_TRUE(selector->HasArgumentMatchInShadowTree());
  selector = selector->NextSimpleSelector();
  EXPECT_EQ(selector->GetPseudoType(), CSSSelector::kPseudoHostContext);
}

TEST(CSSSelector, RenestAmpersand) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext scoped_execution_context;
  Document* document =
      Document::CreateForTest(scoped_execution_context.GetExecutionContext());

  StyleRule* a = To<StyleRule>(ParseRule(*document, ".a {}"));
  StyleRule* b = To<StyleRule>(ParseRule(*document, ".b {}"));

  CSSSelectorList* list = ParseSelectorList("&", CSSNestingType::kNesting,
                                            /*parent_rule_for_nesting=*/a);
  ASSERT_TRUE(list && list->IsValid());
  EXPECT_EQ(a, list->First()->ParentRule());

  EXPECT_EQ(std::nullopt, list->First()->Renest(a));  // No-op.

  std::optional<CSSSelector> renested = list->First()->Renest(b);
  ASSERT_TRUE(renested.has_value());
  EXPECT_EQ(b, renested->ParentRule());
}

TEST(CSSSelector, RenestList) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext scoped_execution_context;
  Document* document =
      Document::CreateForTest(scoped_execution_context.GetExecutionContext());

  StyleRule* a = To<StyleRule>(ParseRule(*document, ".a {}"));
  StyleRule* b = To<StyleRule>(ParseRule(*document, ".b {}"));

  CSSSelectorList* old_list = ParseSelectorList(
      "&:is(&, .d)", CSSNestingType::kNesting, /*parent_rule_for_nesting=*/a);
  ASSERT_TRUE(old_list && old_list->IsValid());

  EXPECT_EQ(old_list, old_list->Renest(a));  // No-op.

  CSSSelectorList* new_list = old_list->Renest(b);
  EXPECT_NE(old_list, new_list);

  EXPECT_EQ(
      ":is(.a):is(:is(.a), .d)",
      old_list->First()->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));
  EXPECT_EQ(
      ":is(.b):is(:is(.b), .d)",
      new_list->First()->SelectorTextExpandingPseudoReferences(/*scope_id=*/0));
}

TEST(CSSSelector, RenestNoNesting) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext scoped_execution_context;
  Document* document =
      Document::CreateForTest(scoped_execution_context.GetExecutionContext());

  StyleRule* a = To<StyleRule>(ParseRule(*document, ".a {}"));
  CSSSelectorList* list = ParseSelectorList(".a:is(.b, .c):not(.c)");
  ASSERT_TRUE(list && list->IsValid());
  EXPECT_EQ(list, list->Renest(a));
}

TEST(CSSSelector, RenestScope) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext scoped_execution_context;
  Document* document =
      Document::CreateForTest(scoped_execution_context.GetExecutionContext());

  StyleRule* a = To<StyleRule>(ParseRule(*document, ".a {}"));
  StyleRule* b = To<StyleRule>(ParseRule(*document, ".b {}"));
  CSSSelectorList* list =
      ParseSelectorList(":scope:is(:scope)", CSSNestingType::kScope,
                        /*parent_rule_for_nesting=*/a);
  ASSERT_TRUE(list && list->IsValid());
  EXPECT_EQ(list, list->Renest(a));
  // There is no '&' selector in `list`: no need to re-nest even when
  // the parent rule changed:
  EXPECT_EQ(list, list->Renest(b));
}

#if DCHECK_IS_ON()

TEST(CSSSelector, ShowWithParentPseudo) {
  test::TaskEnvironment task_environment;
  CSSSelectorList* list = ParseSelectorList("& .x");
  ASSERT_TRUE(list);
  ASSERT_TRUE(list->First());
  list->First()->Show();  // Don't crash.
}

#endif  // DCHECK_IS_ON()

}  // namespace blink
