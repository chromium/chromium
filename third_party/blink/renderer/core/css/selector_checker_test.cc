// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/selector_checker.h"

#include <bitset>
#include <cstdio>
#include <optional>

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/selector_checker-inl.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

struct ScopeProximityTestData {
  const char* html;
  const char* rule;
  std::optional<unsigned> proximity;
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
    // (#target is the scope itself, selected with :scope).
    {
      R"HTML(
        <div class=a>
          <div>
            <div>
              <div>
                <div id=target class=a></div>
              </div>
            </div>
          </div>
        </div>
      )HTML",
      R"CSS(
        @scope (.a) {
          :scope { z-index: 1; }
        }
      )CSS",
      0
    },

    // The proximity is determined according to the nearest scoping root.
    // (#target is the scope itself, selected with &).
    {
      R"HTML(
        <div class=a>
          <div>
            <div>
              <div>
                <div id=target class=a></div>
              </div>
            </div>
          </div>
        </div>
      )HTML",
      R"CSS(
        @scope (.a) {
          & { z-index: 1; }
        }
      )CSS",
      0
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
      public testing::WithParamInterface<ScopeProximityTestData> {};

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
  SelectorChecker::SelectorCheckingContext context{
      ElementResolveContext(*target)};
  context.selector = style_rule->FirstSelector();
  context.style_scope = scope;
  context.style_scope_frame = &style_scope_frame;

  SelectorChecker::MatchResult result;
  bool match = checker.Match(context, result);

  EXPECT_EQ(param.proximity,
            match ? std::optional<unsigned>(result.proximity) : std::nullopt);
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

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
  ASSERT_TRUE(selector_list->IsSingleComplexSelector());

  SelectorChecker checker(SelectorChecker::kResolvingStyle);
  SelectorChecker::SelectorCheckingContext context{
      ElementResolveContext(*element)};
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

    GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
    DCHECK(selector_list->IsSingleComplexSelector());

    SelectorChecker checker(SelectorChecker::kResolvingStyle);
    SelectorChecker::SelectorCheckingContext context{
        ElementResolveContext(element)};
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

  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id=host>
      <template shadowrootmode="open">
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
  ASSERT_TRUE(selector_list->IsSingleComplexSelector());

  SelectorChecker checker(SelectorChecker::kResolvingStyle);
  SelectorChecker::SelectorCheckingContext context{
      ElementResolveContext(*host)};
  context.selector = selector_list->First();
  context.scope = host->GetShadowRoot();
  context.tree_scope = host->GetShadowRoot();

  SelectorChecker::MatchResult result;
  checker.Match(context, result);

  // Comparing using std::bitset produces error messages that are easier to
  // interpret.
  using Bits = std::bitset<sizeof(MatchFlags) * 8>;

  SCOPED_TRACE(param.selector);
  EXPECT_EQ(Bits(param.expected), Bits(result.flags));
}

class MatchFlagsScopeTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
      <style id=style>
      </style>
      <div id=outer>
        <div id=inner></div>
      </div>
    )HTML");
    UpdateAllLifecyclePhasesForTest();
  }

  void SetStyle(String text) {
    Element* style = GetDocument().getElementById(AtomicString("style"));
    DCHECK(style);
    style->setTextContent(text);
    UpdateAllLifecyclePhasesForTest();
  }

  Element& Outer() const {
    return *GetDocument().getElementById(AtomicString("outer"));
  }
  Element& Inner() const {
    return *GetDocument().getElementById(AtomicString("inner"));
  }

  bool AffectedByHover(Element& element) {
    return element.ComputedStyleRef().AffectedByHover();
  }
};

TEST_F(MatchFlagsScopeTest, NoHover) {
  SetStyle(R"HTML(
    @scope (#inner) to (.unknown) {
      :scope { --x:1; }
    }
    @scope (#outer) to (.unknown) {
      :scope #inner { --x:1; }
    }
  )HTML");
  EXPECT_FALSE(AffectedByHover(Outer()));
  EXPECT_FALSE(AffectedByHover(Inner()));
}

TEST_F(MatchFlagsScopeTest, HoverSubject) {
  SetStyle(R"HTML(
    @scope (#outer) {
      :scope #inner:hover { --x:1; }
    }
  )HTML");
  EXPECT_FALSE(AffectedByHover(Outer()));
  EXPECT_TRUE(AffectedByHover(Inner()));
}

TEST_F(MatchFlagsScopeTest, HoverNonSubject) {
  SetStyle(R"HTML(
    @scope (#outer) {
      :scope:hover #inner { --x:1; }
    }
  )HTML");
  EXPECT_FALSE(AffectedByHover(Outer()));
  EXPECT_FALSE(AffectedByHover(Inner()));
}

TEST_F(MatchFlagsScopeTest, ScopeSubject) {
  SetStyle(R"HTML(
    @scope (#inner:hover) {
      :scope { --x:1; }
    }
  )HTML");
  EXPECT_FALSE(AffectedByHover(Outer()));
  EXPECT_TRUE(AffectedByHover(Inner()));
}

TEST_F(MatchFlagsScopeTest, ScopeNonSubject) {
  SetStyle(R"HTML(
    @scope (#outer:hover) {
      :scope #inner { --x:1; }
    }
  )HTML");
  EXPECT_FALSE(AffectedByHover(Outer()));
  EXPECT_FALSE(AffectedByHover(Inner()));
}

TEST_F(MatchFlagsScopeTest, ScopeLimit) {
  SetStyle(R"HTML(
    @scope (#inner) to (#inner:hover) {
      :scope { --x:1; }
    }
  )HTML");
  EXPECT_FALSE(AffectedByHover(Outer()));
  EXPECT_TRUE(AffectedByHover(Inner()));
}

TEST_F(MatchFlagsScopeTest, ScopeLimitNonSubject) {
  SetStyle(R"HTML(
    @scope (#middle) to (#middle:hover) {
      :scope #inner { --x:1; }
    }
  )HTML");
  EXPECT_FALSE(AffectedByHover(Outer()));
  EXPECT_FALSE(AffectedByHover(Inner()));
}

// The pseudo-child tests follow the following rules:
//
// A document is loaded with the following HTML:
//
//  <div id=a class=b></div>
//
// This div is then used as the (ultimate) originating element for a chain
// of PseudoElements specified by `pseudo_element_chain`. The innermost
// pseudo-element in that chain is the passed to the ElementResolveContext,
// and we match `rule` against that context.
struct PseudoChildMatchTestData {
  // A chain of pseudo-elements to create, using #a (see above) as the ultimate
  // originating element.
  const std::vector<PseudoId> pseudo_element_chain;
  // The rule to match against the innermost pseudo-element in the above chain.
  const char* rule;
  bool expected_match;
};

PseudoChildMatchTestData pseudo_child_match_data[] = {
    // clang-format off

    // Basic cases:
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = "div::before {}",
      .expected_match = true,
    },
    {
      .pseudo_element_chain = { kPseudoIdAfter },
      .rule = "div::after {}",
      .expected_match = true,
    },
    {
      .pseudo_element_chain = { kPseudoIdMarker },
      .rule = "div::marker {}",
      .expected_match = true,
    },

    // Logical combinations:
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = ":is(::before) {}",
      .expected_match = true,
    },
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = ":not(:not(::before)) {}",
      .expected_match = true,
    },
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = ":not(::marker):is(::before) {}",
      .expected_match = true,
    },
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = ":not(:hover):is(::before) {}",
      .expected_match = true,
    },

    // Nested cases:

    {
      .pseudo_element_chain = { kPseudoIdBefore, kPseudoIdMarker },
      .rule = "div::before::marker {}",
      .expected_match = true,
    },

    // Universal selector should not match, since nothing is explicitly
    // matching with ::before. See the new proposed selectors data model:
    // https://github.com/w3c/csswg-drafts/issues/9702#issuecomment-3250059981
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = "* {}",
      .expected_match = false,
    },

    // Universal ultimate originating compound:
    {
      .pseudo_element_chain = { kPseudoIdBefore, kPseudoIdMarker },
      .rule = "::before::marker {}",
      .expected_match = true,
    },

    // Universal ultimate originating compound (explicit):
    {
      .pseudo_element_chain = { kPseudoIdBefore, kPseudoIdMarker },
      .rule = "*::before::marker {}",
      .expected_match = true,
    },

    // Tests below this line are expected to *not* match.

    // Mismatched pseudo-element:
    {
      .pseudo_element_chain = { kPseudoIdAfter },
      .rule = "div::before {}",
      .expected_match = false,
    },

    // Pseudo-elements can not match tags, IDs, classes, nor attributes.
    //
    // Note: we're using an originating element <div id=a class=b>
    // for all of these tests. We need to make sure that we're not
    // actually matching against the originating element when we're
    // really requesting a match against a pseudo-element.
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = "div {}",
      .expected_match = false,
    },
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = "#a {}",
      .expected_match = false,
    },
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = ".b {}",
      .expected_match = false,
    },
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = "[id] {}",
      .expected_match = false,
    },
    // Like the previous four tests, but via :is() this time, plus explicitly
    // matching ::before.
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = ":is(::before):is(div) {}",
      .expected_match = false,
    },
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = ":is(::before):is(#a) {}",
      .expected_match = false,
    },
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = ":is(::before):is(.b) {}",
      .expected_match = false,
    },
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = ":is(::before):is([id]) {}",
      .expected_match = false,
    },

    // An element can't both be a before-pseudo-element
    // and a marker-pseudo-element.
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = ":is(::before):is(::marker) {}",
      .expected_match = false,
    },

    // No pseudo-element to match ::before. (The before pseudo-element that
    // we do have attempts to match against ::marker.)
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = "div::before::marker {}",
      .expected_match = false,
    },

    // Non-matching originating pseudo:
    {
      .pseudo_element_chain = { kPseudoIdAfter, kPseudoIdMarker },
      .rule = "div::before::marker {}",
      .expected_match = false,
    },

    // Non-matching ultimate originating element:
    {
      .pseudo_element_chain = { kPseudoIdBefore, kPseudoIdMarker },
      .rule = "#noexist::before::marker {}",
      .expected_match = false,
    },

    // clang-format on
};

class PseudoChildMatchTest
    : public PageTestBase,
      public testing::WithParamInterface<PseudoChildMatchTestData> {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    SetHtmlInnerHTML("<div id=a class=b></div>");
    originating_element_ = GetDocument().getElementById(AtomicString("a"));
    CHECK(originating_element_);
  }

  // Creating a chain of PseudoElements according to `chain`,
  // using `originating_element_` as the ultimate originating element.
  // Returns the innermost pseudo-element in the chain, or the originating
  // element itself, if `chain` is empty.
  Element* AttachPseudoElementChain(const std::vector<PseudoId>& chain) {
    Element* leaf = originating_element_.Get();
    for (PseudoId pseudo_id : chain) {
      leaf = PseudoElement::Create(/*parent=*/leaf, pseudo_id);
    }
    return leaf;
  }

  bool Match(SelectorChecker::SelectorCheckingContext& context) {
    SelectorChecker checker(SelectorChecker::kResolvingStyle);
    return checker.Match(context);
  }

  Persistent<Element> originating_element_;
};

INSTANTIATE_TEST_SUITE_P(SelectorChecker,
                         PseudoChildMatchTest,
                         testing::ValuesIn(pseudo_child_match_data));

TEST_P(PseudoChildMatchTest, PseudoElementObjects) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(true);

  PseudoChildMatchTestData param = GetParam();
  SCOPED_TRACE(param.rule);

  auto* style_rule = DynamicTo<StyleRule>(
      css_test_helpers::ParseRule(GetDocument(), param.rule));
  ASSERT_TRUE(style_rule);

  Element* candidate = AttachPseudoElementChain(param.pseudo_element_chain);
  ASSERT_TRUE(candidate);

  SelectorChecker::SelectorCheckingContext context{
      ElementResolveContext(*candidate)};
  context.selector = style_rule->FirstSelector();
  ASSERT_TRUE(context.pseudo_element);

  EXPECT_EQ(param.expected_match, Match(context));
}

// This is a version of the above PseudoElementObjects test, which, instead of
// creating PseudoElement objects for every item in the pseudo-element chain,
// only does so for all but the last item. The last PseudoId in the chain
// is instead set on SelectorCheckingContext::pseudo_id, to simulate
// "virtual pseudo matching", as described near the implementation of
// SelectorChecker::CheckVirtualPseudo.
TEST_P(PseudoChildMatchTest, VirtualPseudo) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(true);

  PseudoChildMatchTestData param = GetParam();
  SCOPED_TRACE(param.rule);

  auto* style_rule = DynamicTo<StyleRule>(
      css_test_helpers::ParseRule(GetDocument(), param.rule));
  ASSERT_TRUE(style_rule);

  // We won't create a PseudoElement for the rightmost pseudo-element selector.
  // Instead, we'll simply set SelectorCheckingContext::pseudo_id to simulate
  // e.g. getComputedStyle(e, '::before') when no before element actually
  // exists.
  PseudoId rightmost_pseudo_id = kPseudoIdNone;
  std::vector<PseudoId> amended_chain = param.pseudo_element_chain;
  if (!amended_chain.empty()) {
    rightmost_pseudo_id = amended_chain.back();
    amended_chain.pop_back();
  }

  Element* candidate = AttachPseudoElementChain(amended_chain);
  ASSERT_TRUE(candidate);

  SelectorChecker::SelectorCheckingContext context{
      ElementResolveContext(*candidate)};
  context.selector = style_rule->FirstSelector();
  context.pseudo_id = rightmost_pseudo_id;

  EXPECT_EQ(param.expected_match, Match(context));
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
  EXPECT_TRUE(IsEasy("* .a"));
  EXPECT_TRUE(IsEasy(".a *"));
  EXPECT_TRUE(IsEasy("[attr]"));
  EXPECT_TRUE(IsEasy("[attr=\"foo\"]"));
  EXPECT_TRUE(IsEasy("[attr=\"foo\" i]"));
  EXPECT_TRUE(IsEasy(":root"));       // Due to bucketing.
  EXPECT_TRUE(IsEasy(":any-link"));   // Due to bucketing.
  EXPECT_TRUE(IsEasy("a:any-link"));  // Due to bucketing.
  EXPECT_TRUE(IsEasy(".a .b"));
  EXPECT_TRUE(IsEasy(".a .b.c.d"));
  EXPECT_TRUE(IsEasy(".a > .b"));
  EXPECT_TRUE(IsEasy(".a .b > .c"));
  EXPECT_FALSE(IsEasy(".a > .b .c"));
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("<div id=foo></div>");
  UpdateAllLifecyclePhasesForTest();

  CSSSelectorList* selector_list =
      css_test_helpers::ParseSelectorList(":scope #foo");
  ASSERT_TRUE(selector_list);
  ASSERT_TRUE(selector_list->First());

  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  ASSERT_TRUE(foo);

  SelectorChecker checker(SelectorChecker::kResolvingStyle);
  SelectorChecker::SelectorCheckingContext context{ElementResolveContext(*foo)};
  context.selector = selector_list->First();
  // We have a selector with :scope, but no context.scope:
  context.scope = nullptr;

  SelectorChecker::MatchResult result;

  // Don't crash.
  EXPECT_FALSE(checker.Match(context, result));
}

class LangTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
        <div id="en" lang="en">English</div>
        <div id="en-US" lang="en-US">US English</div>
        <div id="en-GB" lang="en-GB">British English</div>
        <div id="en-CA" lang="en-CA">Canada English</div>
        <div id="fr" lang="fr">French</div>
        <div id="fr-FR" lang="fr-FR">France French</div>
        <div id="fr-CA" lang="fr-CA">Canada French</div>
        <div id="ja" lang="ja">Japanese</div>
        <div id="ja-JP" lang="ja-JP">Japan Japanese</div>
        <div id="ja-Jpan-JP" lang="ja-Jpan-JP">Japan Japanese, complete script</div>
        <div id="ja-Hira-JP" lang="ja-Hira-JP">Japan Japanese, Hiragana script</div>
        <div id="x-private" lang="x-private">Private use</div>
        <div id="en-x-private" lang="en-x-private">English with "private" singleton</div>
        <div id="en-x-US" lang="en-x-US">English with "US" singleton</div>
        <div id="fr-x-foobar" lang="fr-x-foobar">French with private subtag</div>
        <div id="fr-Latn-FR-x-foobar" lang="fr-Latn-FR-x-foobar">French with script, region, and private subtag</div>
        <div id="empty" lang="">Empty language</div>
        <div id="no-lang">No language tag</div>
        <div id="und" lang="und">Undetermined language</div>
      )HTML");
    UpdateAllLifecyclePhasesForTest();
  }

  bool MatchesLang(const String& selector_text, const char* element_id) {
    CSSSelectorList* selector_list =
        css_test_helpers::ParseSelectorList(selector_text);

    Element* element = GetDocument().getElementById(AtomicString(element_id));
    DCHECK(element);

    SelectorChecker checker(SelectorChecker::kResolvingStyle);
    SelectorChecker::SelectorCheckingContext context{
        ElementResolveContext(*element)};
    context.selector = selector_list->First();

    if (!context.selector) {
      return false;
    }

    SelectorChecker::MatchResult result;
    return checker.Match(context, result);
  }

  bool MatchesLangTagValue(const String& selector_text,
                           const String& lang_value) {
    Element* element = GetDocument().CreateRawElement(html_names::kDivTag);
    element->setAttribute(html_names::kLangAttr, AtomicString(lang_value));
    GetDocument().body()->appendChild(element);

    CSSSelectorList* selector_list =
        css_test_helpers::ParseSelectorList(selector_text);
    if (!selector_list || !selector_list->First()) {
      return false;
    }

    SelectorChecker checker(SelectorChecker::kResolvingStyle);
    SelectorChecker::SelectorCheckingContext context{
        ElementResolveContext(*element)};
    context.selector = selector_list->First();

    SelectorChecker::MatchResult result;
    return checker.Match(context, result);
  }
};

// This class is used to validate against the RFC 4647 basic language range
// grammar, regardless of the value of CSSLangExtendedRanges.
// language-range = (1*8ALPHA *("-" 1*8alphanum)) / "*"
class LangInvariantTest : public LangTest,
                          public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    scoped_feature_ =
        std::make_unique<ScopedCSSLangExtendedRangesForTest>(GetParam());
    LangTest::SetUp();
  }

 private:
  std::unique_ptr<ScopedCSSLangExtendedRangesForTest> scoped_feature_;
};

INSTANTIATE_TEST_SUITE_P(SelectorChecker, LangInvariantTest, testing::Bool());

TEST_P(LangInvariantTest, ExactLanguageMatch) {
  EXPECT_TRUE(MatchesLang(":lang(en)", "en"));
  EXPECT_FALSE(MatchesLang(":lang(fr)", "en"));

  EXPECT_TRUE(MatchesLang(":lang(en-US)", "en-US"));
  EXPECT_FALSE(MatchesLang(":lang(en-US)", "en-GB"));

  EXPECT_FALSE(MatchesLang(":lang(en-)", "en"));
  EXPECT_FALSE(MatchesLang(":lang(-en)", "en"));
  EXPECT_FALSE(MatchesLang(":lang(US-)", "en-US"));
  EXPECT_FALSE(MatchesLang(":lang(-US)", "en-US"));
}

TEST_P(LangInvariantTest, SpecificVariantMatch) {
  EXPECT_TRUE(MatchesLang(":lang(fr)", "fr-CA"));
  EXPECT_TRUE(MatchesLang(":lang(fr-CA)", "fr-CA"));
  EXPECT_TRUE(MatchesLang(":lang(ja-Jpan-JP)", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(ja-Hira-JP)", "ja-Hira-JP"));

  EXPECT_FALSE(MatchesLang(":lang(en)", "fr-CA"));
  EXPECT_FALSE(MatchesLang(":lang(fr-FR)", "fr-CA"));
  EXPECT_FALSE(MatchesLang(":lang(ja-Jpan-JP)", "ja-Hira-JP"));
}

TEST_P(LangInvariantTest, CaseInsensitiveMatch) {
  EXPECT_TRUE(MatchesLang(":lang(JA-HIRA-JP)", "ja-Hira-JP"));
  EXPECT_TRUE(MatchesLang(":lang(ja-hira-jp)", "ja-Hira-JP"));
  EXPECT_TRUE(MatchesLang(":lang(jA-hIrA-jP)", "ja-Hira-JP"));
}

TEST_P(LangInvariantTest, SingletonBlocking) {
  EXPECT_TRUE(MatchesLang(":lang(x-private)", "x-private"));
  EXPECT_TRUE(MatchesLang(":lang(x)", "x-private"));

  EXPECT_FALSE(MatchesLang(":lang(en-US)", "en-x-private"));
  EXPECT_FALSE(MatchesLang(":lang(en-US)", "en-x-US"));
}

TEST_P(LangInvariantTest, UntaggedLanguageMatching) {
  EXPECT_FALSE(MatchesLang(":lang(en)", "empty"));
  EXPECT_FALSE(MatchesLang(":lang(en)", "no-lang"));

  EXPECT_TRUE(MatchesLang(":lang(und)", "und"));
}

// Extended language ranges tests

TEST_F(LangTest, SimpleWildcardMatching) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLang(":lang(\"*\")", "en"));
  EXPECT_TRUE(MatchesLang(":lang(\"*\")", "en-US"));
  EXPECT_TRUE(MatchesLang(":lang(\"*\")", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(\"*\")", "und"));

  EXPECT_FALSE(MatchesLang(":lang(\"*\")", "empty"));
  EXPECT_FALSE(MatchesLang(":lang(\"*\")", "no-lang"));
}

TEST_F(LangTest, EscapedSimpleWildcardMatching) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLang(":lang(\\*)", "en"));
  EXPECT_TRUE(MatchesLang(":lang(\\*)", "en-US"));
  EXPECT_TRUE(MatchesLang(":lang(\\*)", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(\\*)", "und"));

  EXPECT_FALSE(MatchesLang(":lang(\\*)", "empty"));
  EXPECT_FALSE(MatchesLang(":lang(\\*)", "no-lang"));
}

TEST_F(LangTest, ComplexWildcardMatching) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLang(":lang(\"en-*\")", "en-CA"));
  EXPECT_TRUE(MatchesLang(":lang(\"*-CA\")", "en-CA"));

  EXPECT_FALSE(MatchesLang(":lang(\"en-*\")", "en"));
  EXPECT_FALSE(MatchesLang(":lang(\"*-US\")", "en"));
  EXPECT_FALSE(MatchesLang(":lang(\"*-GB\")", "en-CA"));
  EXPECT_FALSE(MatchesLang(":lang(\"fr-*\")", "en-CA"));
}

TEST_F(LangTest, EscapedComplexWildcardMatching) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLang(":lang(en-\\*)", "en-CA"));
  EXPECT_TRUE(MatchesLang(":lang(\\*-CA)", "en-CA"));

  EXPECT_FALSE(MatchesLang(":lang(en-\\*)", "en"));
  EXPECT_FALSE(MatchesLang(":lang(\\*-US)", "en"));
  EXPECT_FALSE(MatchesLang(":lang(\\*-GB)", "en-CA"));
  EXPECT_FALSE(MatchesLang(":lang(fr-\\*)", "en-CA"));
}

TEST_F(LangTest, MultipleWildcardMatching) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLang(":lang(\"*-*\")", "ja-JP"));

  EXPECT_FALSE(MatchesLang(":lang(\"*-*\")", "ja"));
  EXPECT_FALSE(MatchesLang(":lang(\"*-*-*\")", "ja-JP"));

  EXPECT_TRUE(MatchesLang(":lang(\"*-*\")", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(\"*-*-*\")", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(\"ja-*-JP\")", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(\"ja-*-*\")", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(\"*-Jpan-*\")", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(\"*-*-jp\")", "ja-Jpan-JP"));

  EXPECT_FALSE(MatchesLang(":lang(\"*-Hira-*\")", "ja-Jpan-JP"));
}

TEST_F(LangTest, EscapedMultipleWildcardMatching) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLang(":lang(\\*-\\*)", "en-US"));

  EXPECT_FALSE(MatchesLang(":lang(\\*-\\*)", "en"));
  EXPECT_FALSE(MatchesLang(":lang(\\*-\\*-\\*)", "en-US"));

  EXPECT_TRUE(MatchesLang(":lang(\\*-\\*)", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(\\*-\\*-\\*)", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(ja-\\*-JP)", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(ja-\\*-\\*)", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(\\*-Jpan-\\*)", "ja-Jpan-JP"));
  EXPECT_TRUE(MatchesLang(":lang(\\*-\\*-jp)", "ja-Jpan-JP"));

  EXPECT_FALSE(MatchesLang(":lang(\\-Hira-\\*)", "ja-Jpan-JP"));
}

TEST_F(LangTest, SubtagSkipping) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLangTagValue(":lang(de-DE)", "de-DE"));
  EXPECT_TRUE(MatchesLangTagValue(":lang(de-DE)", "de-DE-1996"));
  EXPECT_TRUE(MatchesLangTagValue(":lang(de-DE)", "de-Latn-DE"));
  EXPECT_TRUE(MatchesLangTagValue(":lang(de-DE)", "de-Latn-DE-1996"));

  EXPECT_FALSE(MatchesLangTagValue(":lang(de-DE)", "de"));
  EXPECT_FALSE(MatchesLangTagValue(":lang(de-DE)", "nl-DE"));
  EXPECT_FALSE(MatchesLangTagValue(":lang(de-DE)", "de-AT"));
  EXPECT_FALSE(MatchesLangTagValue(":lang(de-DE)", "de-AT-1996"));
  EXPECT_FALSE(MatchesLangTagValue(":lang(de-DE)", "de-Latn-AT"));
}

TEST_F(LangTest, MultipleRanges) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLang(":lang(fr, \"en-*\", zh)", "fr"));
  EXPECT_TRUE(MatchesLang(":lang(fr, \"en-*\", zh)", "fr-CA"));
  EXPECT_FALSE(MatchesLang(":lang(fr, \"en-*\", zh)", "en"));
  EXPECT_FALSE(MatchesLang(":lang(fr, \"en-*\", zh)", "ja"));

  EXPECT_TRUE(MatchesLang(":lang(\"*\", en)", "en"));
  EXPECT_TRUE(MatchesLang(":lang(\"*\", en)", "ja-JP"));

  EXPECT_TRUE(MatchesLang(":lang(en-GB, en-US)", "en-US"));
  EXPECT_FALSE(MatchesLang(":lang(en-GB, en-US)", "en"));
  EXPECT_FALSE(MatchesLang(":lang(en-GB, en-US)", "en-CA"));
}

TEST_F(LangTest, EscapedMultipleRanges) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLang(":lang(fr, en-\\*, zh)", "fr"));
  EXPECT_TRUE(MatchesLang(":lang(fr, en-\\*, zh)", "fr-CA"));
  EXPECT_FALSE(MatchesLang(":lang(fr, en-\\*, zh)", "en"));
  EXPECT_FALSE(MatchesLang(":lang(fr, en-\\*, zh)", "ja"));

  EXPECT_TRUE(MatchesLang(":lang(\\*, en)", "en"));
  EXPECT_TRUE(MatchesLang(":lang(\\*, en)", "ja-JP"));
}

TEST_F(LangTest, EmptyStringMatching) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLang(":lang(\"\")", "empty"));

  EXPECT_FALSE(MatchesLang(":lang(\"\")", "no-lang"));
  EXPECT_FALSE(MatchesLang(":lang(\"\")", "und"));
}

TEST_F(LangTest, PrivateSubtagMatching) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLang(":lang(\"fr-x-foobar\")", "fr-x-foobar"));
  EXPECT_TRUE(MatchesLang(":lang(\"fr-x-foobar\")", "fr-Latn-FR-x-foobar"));
  EXPECT_TRUE(MatchesLang(":lang(\"*-x-foobar\")", "fr-Latn-FR-x-foobar"));

  EXPECT_FALSE(MatchesLang(":lang(\"fr-x-foobar\")", "fr"));
  EXPECT_FALSE(MatchesLang(":lang(\"fr-x-foobar\")", "fr-FR"));
}

TEST_F(LangTest, EscapedPrivateSubtagMatching) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  EXPECT_TRUE(MatchesLang(":lang(fr-x-foobar)", "fr-x-foobar"));
  EXPECT_TRUE(MatchesLang(":lang(fr-x-foobar)", "fr-Latn-FR-x-foobar"));
  EXPECT_TRUE(MatchesLang(":lang(\\*-x-foobar)", "fr-Latn-FR-x-foobar"));

  EXPECT_FALSE(MatchesLang(":lang(fr-x-foobar)", "fr"));
  EXPECT_FALSE(MatchesLang(":lang(fr-x-foobar)", "fr-FR"));
}

TEST_F(LangTest, MalformedRangesNeverMatch) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  // Valid CSS idents but invalid BCP 47 language ranges.
  // They parse successfully but will never match anything,
  // not even an element with the exact same lang attribute value.
  const char* malformed[] = {
      "en-",       "en--US", "en123", "ninechars",  "en-ninechars", "café",
      "es-España", "日本語", "en_US", "my\\.thing", "you\\&me",     "j\\ a",
  };

  for (const char* value : malformed) {
    SCOPED_TRACE(value);
    String selector = String(":lang(") + value + ")";
    EXPECT_FALSE(MatchesLangTagValue(selector, value));
  }
}

TEST_F(LangTest, ListValidAndMalformedRangesMatching) {
  ScopedCSSLangExtendedRangesForTest scoped_feature(true);

  // Malformed values do not prevent matching against others in the list.
  const char* malformed[] = {
      "en-",       "en--US", "en123", "ninechars",  "en-ninechars", "café",
      "es-España", "日本語", "en_US", "my\\.thing", "you\\&me",     "j\\ a",
  };

  for (const char* value : malformed) {
    SCOPED_TRACE(value);
    String selector = String(":lang(") + value + " , en)";
    EXPECT_TRUE(MatchesLang(selector, "en-GB"));
  }
}

}  // namespace blink
