// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/selector_checker.h"

#include <bitset>
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/selector_checker-inl.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

struct ScopeProximityTestData {
  const char* html;
  const char* rule;
  absl::optional<unsigned> proximity;
};

ScopeProximityTestData scope_proximity_test_data[] = {
    // clang-format off

    // Selecting the scoping root.
    {
      R"HTML(
        <div id=target></div>
      )HTML",
      R"CSS(
        @scope (#target) {
          :scope { z-index:1; }
        }
      )CSS",
      0
    },

    // Selecting a child.
    {
      R"HTML(
        <div class=a>
          <div id=target></div>
        </div>
      )HTML",
      R"CSS(
        @scope (.a) {
          #target { z-index: 1; }
        }
      )CSS",
      1
    },

    // Selecting a descendant.
    {
      R"HTML(
        <div class=a>
          <div>
            <div>
              <div>
                <div id=target></div>
              </div>
            </div>
          </div>
        </div>
      )HTML",
      R"CSS(
        @scope (.a) {
          #target { z-index: 1; }
        }
      )CSS",
      4
    },

    // The proximity is determined according to the nearest scoping root.
    // (Nested scopes from same @scope rule).
    {
      R"HTML(
        <div class=a>
          <div>
            <div class=a>
              <div>
                <div id=target></div>
              </div>
            </div>
          </div>
        </div>
      )HTML",
      R"CSS(
        @scope (.a) {
          #target { z-index: 1; }
        }
      )CSS",
      2
    },

    // The proximity is determined according to the nearest scoping root.
    // (Nested scopes from different @scope rules).
    {
      R"HTML(
        <div class=a>
          <div class=b>
            <div>
              <div>
                <div id=target></div>
              </div>
            </div>
          </div>
        </div>
      )HTML",
      R"CSS(
        @scope (.a) {
          @scope (.b) {
            #target { z-index: 1; }
          }
        }
      )CSS",
      3
    },

    // @scope(.a) creates two scopes, but the selector only matches in the
    // outermost scope.
    {
      R"HTML(
        <div class=b>
          <div class=a>
            <div class=a>
              <div id=target></div>
            </div>
          </div>
        </div>
      )HTML",
      R"CSS(
        @scope (.a) {
          .b > :scope #target { z-index: 1; }
        }
      )CSS",
      2
    },
    // clang-format on
};

class ScopeProximityTest
    : public PageTestBase,
      public testing::WithParamInterface<ScopeProximityTestData>,
      private ScopedCSSScopeForTest {
 public:
  ScopeProximityTest() : ScopedCSSScopeForTest(true) {}
};

INSTANTIATE_TEST_SUITE_P(SelectorChecker,
                         ScopeProximityTest,
                         testing::ValuesIn(scope_proximity_test_data));

TEST_P(ScopeProximityTest, All) {
  ScopeProximityTestData param = GetParam();
  SCOPED_TRACE(param.html);
  SCOPED_TRACE(param.rule);

  SetHtmlInnerHTML(param.html);
  auto* rule = css_test_helpers::ParseRule(GetDocument(), param.rule);
  ASSERT_TRUE(rule);

  const StyleScope* scope = nullptr;

  // Find the inner StyleRule.
  while (IsA<StyleRuleScope>(rule)) {
    auto& scope_rule = To<StyleRuleScope>(*rule);
    scope = scope_rule.GetStyleScope().CopyWithParent(scope);
    const HeapVector<Member<StyleRuleBase>>& child_rules =
        scope_rule.ChildRules();
    ASSERT_EQ(1u, child_rules.size());
    rule = child_rules[0].Get();
  }

  ASSERT_TRUE(scope);

  auto* style_rule = DynamicTo<StyleRule>(rule);
  ASSERT_TRUE(style_rule);

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  SelectorChecker checker(SelectorChecker::kResolvingStyle);
  StyleScopeFrame style_scope_frame(*target, /* parent */ nullptr);
  SelectorChecker::SelectorCheckingContext context(target);
  context.selector = style_rule->FirstSelector();
  context.style_scope = scope;
  context.style_scope_frame = &style_scope_frame;

  SelectorChecker::MatchResult result;
  bool match = checker.Match(context, result);

  EXPECT_EQ(param.proximity,
            match ? absl::optional<unsigned>(result.proximity) : absl::nullopt);
}

struct MatchFlagsTestData {
  // If of element to match.
  const char* selector;
  MatchFlags expected;
};

constexpr MatchFlags Active() {
  return static_cast<MatchFlags>(MatchFlag::kAffectedByActive);
}
constexpr MatchFlags Drag() {
  return static_cast<MatchFlags>(MatchFlag::kAffectedByDrag);
}
constexpr MatchFlags FocusWithin() {
  return static_cast<MatchFlags>(MatchFlag::kAffectedByFocusWithin);
}
constexpr MatchFlags Hover() {
  return static_cast<MatchFlags>(MatchFlag::kAffectedByHover);
}

MatchFlagsTestData result_flags_test_data[] = {
    // clang-format off
    { "div", 0 },
    { ".foo", 0 },
    { ":active", Active() },
    { ":-webkit-drag", Drag() },
    { ":focus-within", FocusWithin() },
    { ":hover", Hover() },

    // We never evaluate :hover, since :active fails to match.
    { ":active:hover", Active() },

    // Non-rightmost compound:
    { ":active *", 0 },
    { ":-webkit-drag *", 0 },
    { ":focus-within *", 0 },
    { ":hover *", 0 },
    { ":is(:hover) *", 0 },
    { ":not(:hover) *", 0 },

    // Within pseudo-classes:
    { ":is(:active, :hover)", Active() | Hover() },
    { ":not(:active, :hover)", Active() | Hover() },
    { ":where(:active, :hover)", Active() | Hover() },
    { ":-webkit-any(:active, :hover)", Active() | Hover() },
    // TODO(andruud): Don't over-mark for :has().
    { ":has(:active, :hover)", Active() | Hover() },

    // Within pseudo-elements:
    { "::cue(:hover)", Hover() },
    { "::slotted(:hover)", Hover() },
    // clang-format on
};

class MatchFlagsTest : public PageTestBase,
                       public testing::WithParamInterface<MatchFlagsTestData> {
};

INSTANTIATE_TEST_SUITE_P(SelectorChecker,
                         MatchFlagsTest,
                         testing::ValuesIn(result_flags_test_data));

TEST_P(MatchFlagsTest, All) {
  MatchFlagsTestData param = GetParam();

  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=target>
      <div></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* element = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(element);

  CSSSelectorList* selector_list =
      css_test_helpers::ParseSelectorList(param.selector);
  ASSERT_TRUE(selector_list);
  ASSERT_TRUE(selector_list->HasOneSelector());

  SelectorChecker checker(SelectorChecker::kResolvingStyle);
  SelectorChecker::SelectorCheckingContext context(element);
  context.selector = selector_list->First();

  SelectorChecker::MatchResult result;
  checker.Match(context, result);

  // Comparing using std::bitset produces error messages that are easier to
  // interpret.
  using Bits = std::bitset<sizeof(MatchFlags) * 8>;

  SCOPED_TRACE(param.selector);
  EXPECT_EQ(Bits(param.expected), Bits(result.flags));
}

class ImpactTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();

    GetDocument().body()->setInnerHTML(R"HTML(
      <div id=outer>
        <div id=middle>
          <div id=inner>
            <div></div>
          </div>
        </div>
      </div>
    )HTML");
    UpdateAllLifecyclePhasesForTest();
  }

  Element& Outer() const {
    return *GetDocument().getElementById(AtomicString("outer"));
  }
  Element& Middle() const {
    return *GetDocument().getElementById(AtomicString("middle"));
  }
  Element& Inner() const {
    return *GetDocument().getElementById(AtomicString("inner"));
  }

  using Impact = SelectorChecker::Impact;

  MatchFlags Match(String selector, Element& element, Impact impact) {
    CSSSelectorList* selector_list =
        css_test_helpers::ParseSelectorList(selector);
    DCHECK(selector_list);
    DCHECK(selector_list->HasOneSelector());

    SelectorChecker checker(SelectorChecker::kResolvingStyle);
    SelectorChecker::SelectorCheckingContext context(&element);
    context.selector = selector_list->First();
    context.impact = impact;

    SelectorChecker::MatchResult result;
    checker.Match(context, result);

    return result.flags;
  }
};

// :hover

TEST_F(ImpactTest, HoverSubjectOnly) {
  MatchFlags flags = Match("#inner:hover", Inner(), Impact::kSubject);
  EXPECT_EQ(Hover(), flags);
  EXPECT_FALSE(Inner().ChildrenOrSiblingsAffectedByHover());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByHover());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByHover());
}

TEST_F(ImpactTest, HoverNonSubjectOnly) {
  MatchFlags flags = Match("#inner:hover", Inner(), Impact::kNonSubject);
  EXPECT_EQ(0u, flags);
  EXPECT_TRUE(Inner().ChildrenOrSiblingsAffectedByHover());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByHover());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByHover());
}

TEST_F(ImpactTest, HoverBoth) {
  MatchFlags flags = Match("#inner:hover", Inner(), Impact::kBoth);
  EXPECT_EQ(Hover(), flags);
  EXPECT_TRUE(Inner().ChildrenOrSiblingsAffectedByHover());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByHover());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByHover());
}

TEST_F(ImpactTest, HoverDescendantCombinatorSubject) {
  MatchFlags flags = Match(":hover #inner", Inner(), Impact::kSubject);
  EXPECT_EQ(0u, flags);
  EXPECT_FALSE(Inner().ChildrenOrSiblingsAffectedByHover());
  EXPECT_TRUE(Middle().ChildrenOrSiblingsAffectedByHover());
  EXPECT_TRUE(Outer().ChildrenOrSiblingsAffectedByHover());
}

// :-webkit-drag

TEST_F(ImpactTest, DragSubjectOnly) {
  MatchFlags flags = Match("#inner:-webkit-drag", Inner(), Impact::kSubject);
  EXPECT_EQ(Drag(), flags);
  EXPECT_FALSE(Inner().ChildrenOrSiblingsAffectedByDrag());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByDrag());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByDrag());
}

TEST_F(ImpactTest, DragNonSubjectOnly) {
  MatchFlags flags = Match("#inner:-webkit-drag", Inner(), Impact::kNonSubject);
  EXPECT_EQ(0u, flags);
  EXPECT_TRUE(Inner().ChildrenOrSiblingsAffectedByDrag());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByDrag());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByDrag());
}

TEST_F(ImpactTest, DragBoth) {
  MatchFlags flags = Match("#inner:-webkit-drag", Inner(), Impact::kBoth);
  EXPECT_EQ(Drag(), flags);
  EXPECT_TRUE(Inner().ChildrenOrSiblingsAffectedByDrag());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByDrag());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByDrag());
}

TEST_F(ImpactTest, DragDescendantCombinatorSubject) {
  MatchFlags flags = Match(":-webkit-drag #inner", Inner(), Impact::kSubject);
  EXPECT_EQ(0u, flags);
  EXPECT_FALSE(Inner().ChildrenOrSiblingsAffectedByDrag());
  EXPECT_TRUE(Middle().ChildrenOrSiblingsAffectedByDrag());
  EXPECT_TRUE(Outer().ChildrenOrSiblingsAffectedByDrag());
}

// :focus-within

TEST_F(ImpactTest, FocusWithinSubjectOnly) {
  MatchFlags flags = Match("#inner:focus-within", Inner(), Impact::kSubject);
  EXPECT_EQ(FocusWithin(), flags);
  EXPECT_FALSE(Inner().ChildrenOrSiblingsAffectedByFocusWithin());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByFocusWithin());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByFocusWithin());
}

TEST_F(ImpactTest, FocusWithinNonSubjectOnly) {
  MatchFlags flags = Match("#inner:focus-within", Inner(), Impact::kNonSubject);
  EXPECT_EQ(0u, flags);
  EXPECT_TRUE(Inner().ChildrenOrSiblingsAffectedByFocusWithin());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByFocusWithin());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByFocusWithin());
}

TEST_F(ImpactTest, FocusWithinBoth) {
  MatchFlags flags = Match("#inner:focus-within", Inner(), Impact::kBoth);
  EXPECT_EQ(FocusWithin(), flags);
  EXPECT_TRUE(Inner().ChildrenOrSiblingsAffectedByFocusWithin());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByFocusWithin());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByFocusWithin());
}

TEST_F(ImpactTest, FocusWithinDescendantCombinatorSubject) {
  MatchFlags flags = Match(":focus-within #inner", Inner(), Impact::kSubject);
  EXPECT_EQ(0u, flags);
  EXPECT_FALSE(Inner().ChildrenOrSiblingsAffectedByFocusWithin());
  EXPECT_TRUE(Middle().ChildrenOrSiblingsAffectedByFocusWithin());
  EXPECT_TRUE(Outer().ChildrenOrSiblingsAffectedByFocusWithin());
}

// :active

TEST_F(ImpactTest, ActiveSubjectOnly) {
  MatchFlags flags = Match("#inner:active", Inner(), Impact::kSubject);
  EXPECT_EQ(Active(), flags);
  EXPECT_FALSE(Inner().ChildrenOrSiblingsAffectedByActive());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByActive());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByActive());
}

TEST_F(ImpactTest, ActiveNonSubjectOnly) {
  MatchFlags flags = Match("#inner:active", Inner(), Impact::kNonSubject);
  EXPECT_EQ(0u, flags);
  EXPECT_TRUE(Inner().ChildrenOrSiblingsAffectedByActive());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByActive());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByActive());
}

TEST_F(ImpactTest, ActiveBoth) {
  MatchFlags flags = Match("#inner:active", Inner(), Impact::kBoth);
  EXPECT_EQ(Active(), flags);
  EXPECT_TRUE(Inner().ChildrenOrSiblingsAffectedByActive());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByActive());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByActive());
}

TEST_F(ImpactTest, ActiveDescendantCombinatorSubject) {
  MatchFlags flags = Match(":active #inner", Inner(), Impact::kSubject);
  EXPECT_EQ(0u, flags);
  EXPECT_FALSE(Inner().ChildrenOrSiblingsAffectedByActive());
  EXPECT_TRUE(Middle().ChildrenOrSiblingsAffectedByActive());
  EXPECT_TRUE(Outer().ChildrenOrSiblingsAffectedByActive());
}

// :focus-visible

TEST_F(ImpactTest, FocusVisibleSubjectOnly) {
  // Note that :focus-visible does not set any flags for Impact::kSubject.
  // (There is no corresponding MatchFlag).
  Match("#inner:focus-visible", Inner(), Impact::kSubject);
  EXPECT_FALSE(Inner().ChildrenOrSiblingsAffectedByFocusVisible());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByFocusVisible());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByFocusVisible());
}

TEST_F(ImpactTest, FocusVisibleNonSubjectOnly) {
  Match("#inner:focus-visible", Inner(), Impact::kNonSubject);
  EXPECT_TRUE(Inner().ChildrenOrSiblingsAffectedByFocusVisible());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByFocusVisible());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByFocusVisible());
}

TEST_F(ImpactTest, FocusVisibleBoth) {
  Match("#inner:focus-visible", Inner(), Impact::kBoth);
  EXPECT_TRUE(Inner().ChildrenOrSiblingsAffectedByFocusVisible());
  EXPECT_FALSE(Middle().ChildrenOrSiblingsAffectedByFocusVisible());
  EXPECT_FALSE(Outer().ChildrenOrSiblingsAffectedByFocusVisible());
}

TEST_F(ImpactTest, FocusVisibleDescendantCombinatorSubject) {
  Match(":focus-visible #inner", Inner(), Impact::kSubject);
  EXPECT_FALSE(Inner().ChildrenOrSiblingsAffectedByFocusVisible());
  EXPECT_TRUE(Middle().ChildrenOrSiblingsAffectedByFocusVisible());
  EXPECT_TRUE(Outer().ChildrenOrSiblingsAffectedByFocusVisible());
}

// :has()

TEST_F(ImpactTest, HasSubjectOnly) {
  Match("#inner:has(.foo)", Inner(), Impact::kSubject);

  EXPECT_TRUE(Inner().AffectedBySubjectHas());
  EXPECT_FALSE(Middle().AffectedBySubjectHas());
  EXPECT_FALSE(Outer().AffectedBySubjectHas());

  EXPECT_FALSE(Inner().AffectedByNonSubjectHas());
  EXPECT_FALSE(Middle().AffectedByNonSubjectHas());
  EXPECT_FALSE(Outer().AffectedByNonSubjectHas());
}

TEST_F(ImpactTest, HasNonSubjectOnly) {
  Match("#inner:has(.foo)", Inner(), Impact::kNonSubject);

  EXPECT_FALSE(Inner().AffectedBySubjectHas());
  EXPECT_FALSE(Middle().AffectedBySubjectHas());
  EXPECT_FALSE(Outer().AffectedBySubjectHas());

  EXPECT_TRUE(Inner().AffectedByNonSubjectHas());
  EXPECT_FALSE(Middle().AffectedByNonSubjectHas());
  EXPECT_FALSE(Outer().AffectedByNonSubjectHas());
}

TEST_F(ImpactTest, HasBoth) {
  Match("#inner:has(.foo)", Inner(), Impact::kBoth);

  EXPECT_TRUE(Inner().AffectedBySubjectHas());
  EXPECT_FALSE(Middle().AffectedBySubjectHas());
  EXPECT_FALSE(Outer().AffectedBySubjectHas());

  EXPECT_TRUE(Inner().AffectedByNonSubjectHas());
  EXPECT_FALSE(Middle().AffectedByNonSubjectHas());
  EXPECT_FALSE(Outer().AffectedByNonSubjectHas());
}

TEST_F(ImpactTest, HasDescendantCombinatorSubject) {
  Match(":has(.foo) #inner", Inner(), Impact::kSubject);

  EXPECT_FALSE(Inner().AffectedBySubjectHas());
  EXPECT_FALSE(Middle().AffectedBySubjectHas());
  EXPECT_FALSE(Outer().AffectedBySubjectHas());

  EXPECT_FALSE(Inner().AffectedByNonSubjectHas());
  EXPECT_TRUE(Middle().AffectedByNonSubjectHas());
  EXPECT_TRUE(Outer().AffectedByNonSubjectHas());
}

TEST_F(ImpactTest, HasDescendantCombinatorBoth) {
  Match(":has(.foo) #inner", Inner(), Impact::kBoth);

  EXPECT_FALSE(Inner().AffectedBySubjectHas());
  EXPECT_FALSE(Middle().AffectedBySubjectHas());
  EXPECT_FALSE(Outer().AffectedBySubjectHas());

  EXPECT_FALSE(Inner().AffectedByNonSubjectHas());
  EXPECT_TRUE(Middle().AffectedByNonSubjectHas());
  EXPECT_TRUE(Outer().AffectedByNonSubjectHas());
}

TEST_F(ImpactTest, HasSubjectAndDescendantCombinatorBoth) {
  Match(":has(.foo) #inner:has(div)", Inner(), Impact::kBoth);

  EXPECT_TRUE(Inner().AffectedBySubjectHas());
  EXPECT_FALSE(Middle().AffectedBySubjectHas());
  EXPECT_FALSE(Outer().AffectedBySubjectHas());

  EXPECT_TRUE(Inner().AffectedByNonSubjectHas());
  EXPECT_TRUE(Middle().AffectedByNonSubjectHas());
  EXPECT_TRUE(Outer().AffectedByNonSubjectHas());
}

TEST_F(ImpactTest, HasDescendantCombinatorWithinIsBoth) {
  Match("#inner:is(:has(.foo) *)", Inner(), Impact::kBoth);

  EXPECT_FALSE(Inner().AffectedBySubjectHas());
  EXPECT_FALSE(Middle().AffectedBySubjectHas());
  EXPECT_FALSE(Outer().AffectedBySubjectHas());

  EXPECT_FALSE(Inner().AffectedByNonSubjectHas());
  EXPECT_TRUE(Middle().AffectedByNonSubjectHas());
  EXPECT_TRUE(Outer().AffectedByNonSubjectHas());
}

TEST_F(ImpactTest, HasDescendantCombinatorWithIsBoth) {
  Match(":is(:has(.foo) #middle) #inner", Inner(), Impact::kBoth);

  EXPECT_FALSE(Inner().AffectedBySubjectHas());
  EXPECT_FALSE(Middle().AffectedBySubjectHas());
  EXPECT_FALSE(Outer().AffectedBySubjectHas());

  EXPECT_FALSE(Inner().AffectedByNonSubjectHas());
  EXPECT_FALSE(Middle().AffectedByNonSubjectHas());
  EXPECT_TRUE(Outer().AffectedByNonSubjectHas());
}

// Cases involving :host are special, because we need to call SelectorChecker
// with a non-nullptr scope node.

MatchFlagsTestData result_flags_shadow_test_data[] = {
    // clang-format off
    { ":host(:active)", Active() },
    { ":host-context(:active)", Active() },
    // clang-format on
};

class MatchFlagsShadowTest
    : public PageTestBase,
      public testing::WithParamInterface<MatchFlagsTestData> {};

INSTANTIATE_TEST_SUITE_P(SelectorChecker,
                         MatchFlagsShadowTest,
                         testing::ValuesIn(result_flags_shadow_test_data));

TEST_P(MatchFlagsShadowTest, Host) {
  MatchFlagsTestData param = GetParam();

  GetDocument().body()->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <div id=host>
      <template shadowroot="open">
        <div></div>
      </template>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->GetShadowRoot());

  CSSSelectorList* selector_list =
      css_test_helpers::ParseSelectorList(param.selector);
  ASSERT_TRUE(selector_list);
  ASSERT_TRUE(selector_list->HasOneSelector());

  SelectorChecker checker(SelectorChecker::kResolvingStyle);
  SelectorChecker::SelectorCheckingContext context(host);
  context.selector = selector_list->First();
  context.scope = host->GetShadowRoot();

  SelectorChecker::MatchResult result;
  checker.Match(context, result);

  // Comparing using std::bitset produces error messages that are easier to
  // interpret.
  using Bits = std::bitset<sizeof(MatchFlags) * 8>;

  SCOPED_TRACE(param.selector);
  EXPECT_EQ(Bits(param.expected), Bits(result.flags));
}

class EasySelectorCheckerTest : public PageTestBase {
 protected:
  bool Matches(const String& selector_text, const char* id);
  static bool IsEasy(const String& selector_text);
};

bool EasySelectorCheckerTest::Matches(const String& selector_text,
                                      const char* id) {
  StyleRule* rule = To<StyleRule>(
      css_test_helpers::ParseRule(GetDocument(), selector_text + " {}"));
  CHECK(EasySelectorChecker::IsEasy(rule->FirstSelector()));
  return EasySelectorChecker::Match(rule->FirstSelector(), GetElementById(id));
}

#if DCHECK_IS_ON()  // Requires all_rules_, to find back the rules we add.

// Parse the given selector, buckets it and returns whether it was counted
// as easy or not.
bool EasySelectorCheckerTest::IsEasy(const String& selector_text) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules(selector_text + " { }");
  RuleSet& rule_set = sheet.GetRuleSet();
  const HeapVector<RuleData>& rules = rule_set.AllRulesForTest();

  wtf_size_t easy_count = 0;
  for (const RuleData& rule_data : rules) {
    if (EasySelectorChecker::IsEasy(&rule_data.Selector())) {
      ++easy_count;
    }
  }

  // Visited-dependent rules are added twice to the RuleSet. This verifies
  // that both RuleData objects have the same easy-status.
  EXPECT_TRUE((easy_count == 0) || (easy_count == rules.size()));

  return easy_count;
}

TEST_F(EasySelectorCheckerTest, IsEasy) {
  EXPECT_TRUE(IsEasy(".a"));
  EXPECT_TRUE(IsEasy(".a.b"));
  EXPECT_TRUE(IsEasy("#id"));
  EXPECT_TRUE(IsEasy("div"));
  EXPECT_FALSE(IsEasy(":visited"));
  EXPECT_FALSE(IsEasy("a:visited"));
  EXPECT_FALSE(IsEasy("a:link"));
  EXPECT_FALSE(IsEasy("::before"));
  EXPECT_FALSE(IsEasy("div::before"));
  EXPECT_FALSE(IsEasy("* .a"));  // Due to the universal selector.
  EXPECT_TRUE(IsEasy("[attr]"));
  EXPECT_TRUE(IsEasy("[attr=\"foo\"]"));
  EXPECT_FALSE(IsEasy("[attr=\"foo\" i]"));
  EXPECT_TRUE(IsEasy(":root"));       // Due to bucketing.
  EXPECT_TRUE(IsEasy(":any-link"));   // Due to bucketing.
  EXPECT_TRUE(IsEasy("a:any-link"));  // Due to bucketing.
  EXPECT_TRUE(IsEasy(".a .b"));
  EXPECT_TRUE(IsEasy(".a .b.c.d"));
  EXPECT_FALSE(IsEasy(".a > .b"));
  EXPECT_FALSE(IsEasy(".a ~ .b"));
  EXPECT_FALSE(IsEasy("&"));
  EXPECT_FALSE(IsEasy(":not(.a)"));
}

#endif  // DCHECK_IS_ON()

TEST_F(EasySelectorCheckerTest, SmokeTest) {
  SetHtmlInnerHTML(
      R"HTML(
        <div id="a"><div id="b"><div id="c" class="cls1" attr="foo"><span id="d"></span></div></div></div>
      )HTML");
  EXPECT_TRUE(Matches("div", "c"));
  EXPECT_FALSE(Matches("div", "d"));
  EXPECT_TRUE(Matches(".cls1", "c"));
  EXPECT_FALSE(Matches(".cls1", "b"));
  EXPECT_TRUE(Matches("div.cls1", "c"));
  EXPECT_TRUE(Matches("*|div.cls1", "c"));
  EXPECT_TRUE(Matches("#b .cls1", "c"));
  EXPECT_TRUE(Matches("#a .cls1", "c"));
  EXPECT_FALSE(Matches("#b .cls1", "a"));
  EXPECT_FALSE(Matches("#a .cls1", "b"));
  EXPECT_TRUE(Matches("[attr]", "c"));
  EXPECT_TRUE(Matches("[attr=\"foo\"]", "c"));
  EXPECT_FALSE(Matches("[attr=\"bar\"]", "c"));
  EXPECT_FALSE(Matches("[attr]", "b"));
  EXPECT_TRUE(Matches("div#a #c.cls1", "c"));
  EXPECT_FALSE(Matches("div#a #c.cls1", "b"));
  EXPECT_FALSE(Matches("#c .cls1", "c"));
  EXPECT_FALSE(Matches("div #a .cls1", "c"));
}

class SelectorCheckerTest : public PageTestBase {};

TEST_F(SelectorCheckerTest, PseudoScopeWithoutScope) {
  GetDocument().body()->setInnerHTML("<div id=foo></div>");
  UpdateAllLifecyclePhasesForTest();

  CSSSelectorList* selector_list =
      css_test_helpers::ParseSelectorList(":scope #foo");
  ASSERT_TRUE(selector_list);
  ASSERT_TRUE(selector_list->First());

  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  ASSERT_TRUE(foo);

  SelectorChecker checker(SelectorChecker::kResolvingStyle);
  SelectorChecker::SelectorCheckingContext context(foo);
  context.selector = selector_list->First();
  // We have a selector with :scope, but no context.scope:
  context.scope = nullptr;

  SelectorChecker::MatchResult result;

  // Don't crash.
  EXPECT_FALSE(checker.Match(context, result));
}

TEST_F(SelectorCheckerTest, PseudoTrue) {
  GetDocument().body()->setInnerHTML("<div id=foo></div>");
  UpdateAllLifecyclePhasesForTest();

  CSSSelector selector;
  selector.SetTrue();
  selector.SetLastInComplexSelector(true);

  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  ASSERT_TRUE(foo);

  SelectorChecker checker(SelectorChecker::kResolvingStyle);
  SelectorChecker::SelectorCheckingContext context(foo);
  context.selector = &selector;

  SelectorChecker::MatchResult result;
  EXPECT_TRUE(checker.Match(context, result));
}

TEST_F(SelectorCheckerTest, PseudoTrueMatchesHost) {
  GetDocument().body()->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <div id=host>
      <template shadowroot=open>
      </template>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  CSSSelector selector;
  selector.SetTrue();
  selector.SetLastInComplexSelector(true);

  Element* host = GetElementById("host");
  ASSERT_TRUE(host);
  ShadowRoot* shadow = host->GetShadowRoot();
  ASSERT_TRUE(shadow);

  SelectorChecker checker(SelectorChecker::kResolvingStyle);
  SelectorChecker::SelectorCheckingContext context(host);
  context.selector = &selector;
  context.scope = shadow;

  SelectorChecker::MatchResult result;
  EXPECT_TRUE(checker.Match(context, result));
}

}  // namespace blink
