// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"

#include <algorithm>

#include "third_party/blink/renderer/core/css/css_function_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_resolver.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class InspectorCSSAgentTest : public PageTestBase {
 public:
  HeapHashSet<Member<CSSStyleSheet>> CollectAllDocumentStyleSheets() {
    HeapVector<Member<CSSStyleSheet>> sheets;
    InspectorCSSAgent::CollectAllDocumentStyleSheets(&GetDocument(), sheets);

    HeapHashSet<Member<CSSStyleSheet>> sheets_set;
    for (Member<CSSStyleSheet>& sheet : sheets) {
      sheets_set.insert(sheet);
    }
    return sheets_set;
  }

  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
  CollectReferencedFunctionRules(const char* selector) {
    HeapHashSet<Member<CSSStyleSheet>> sheets = CollectAllDocumentStyleSheets();

    Element* e = GetDocument().querySelector(AtomicString(selector),
                                             ASSERT_NO_EXCEPTION);
    CHECK(e);
    InspectorStyleResolver resolver(e, kPseudoIdNone,
                                    /*view_transition_name=*/g_null_atom);

    HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
        function_rules;
    InspectorCSSAgent::CollectReferencedFunctionRules(
        GetDocument(), sheets, *resolver.MatchedRules(), function_rules);
    return function_rules;
  }

  CSSFunctionRule* FindFunctionRule(
      const HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>&
          function_rules,
      const char* name) {
    auto* scoped_name = MakeGarbageCollected<ScopedCSSName>(
        AtomicString(name), /*tree_scope=*/&GetDocument());
    auto it = function_rules.find(scoped_name);
    return it != function_rules.end() ? it->value.Get() : nullptr;
  }
};

TEST_F(InspectorCSSAgentTest, NoFunctions) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #e { width: 1px; }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_TRUE(function_rules.empty());
}

TEST_F(InspectorCSSAgentTest, UnreferencedFunction) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e { width: 1px; }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_TRUE(function_rules.empty());
}

TEST_F(InspectorCSSAgentTest, ElementSpecificFunctionReferences) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e1 { width: 1px; }
      #e2 { width: --a(); }
    </style>
    <div id=e1></div>
    <div id=e2></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      e1_function_rules = CollectReferencedFunctionRules("#e1");
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      e2_function_rules = CollectReferencedFunctionRules("#e2");
  EXPECT_TRUE(e1_function_rules.empty());
  EXPECT_EQ(1u, e2_function_rules.size());
  EXPECT_TRUE(FindFunctionRule(e2_function_rules, "--a"));
}

TEST_F(InspectorCSSAgentTest, MultipleFunctions_Declaration) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() { result: 1px; }
      @function --b() { result: 2px; }
      @function --c() { result: 1000px; }
      #e { width: calc(--a() + --b()); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(2u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
  EXPECT_TRUE(FindFunctionRule(function_rules, "--b"));
  EXPECT_FALSE(FindFunctionRule(function_rules, "--c"));
}

TEST_F(InspectorCSSAgentTest, KeyNameVsFunctionName) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() { result: 1px; }
      @function --b() { result: 2px; }
      @function --c() { result: 3px; }
      #e { width: calc(--a() + --b() + --c()); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  // Check that the names held by the keys correspond to names held
  // by the values:
  EXPECT_EQ(3u, function_rules.size());
  ASSERT_TRUE(FindFunctionRule(function_rules, "--a"));
  EXPECT_EQ("--a", FindFunctionRule(function_rules, "--a")->name());
  ASSERT_TRUE(FindFunctionRule(function_rules, "--b"));
  EXPECT_EQ("--b", FindFunctionRule(function_rules, "--b")->name());
  ASSERT_TRUE(FindFunctionRule(function_rules, "--c"));
  EXPECT_EQ("--c", FindFunctionRule(function_rules, "--c")->name());
}

TEST_F(InspectorCSSAgentTest, MultipleFunctions_Rules) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() { result: 1px; }
      @function --b() { result: 2px; }
      @function --c() { result: 1000px; }
      #e { width: --a(); }
      #e { height: --b(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(2u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
  EXPECT_TRUE(FindFunctionRule(function_rules, "--b"));
  EXPECT_FALSE(FindFunctionRule(function_rules, "--c"));
}

TEST_F(InspectorCSSAgentTest, FunctionsInShorthand) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e { padding: --a(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(1u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
}

TEST_F(InspectorCSSAgentTest, DashedFunctionInMedia) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() { result: 1px; }
      @media (width) {
        #e { width: --a(); }
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(1u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
}

TEST_F(InspectorCSSAgentTest, DashedFunctionNested) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e {
        & {
          width: --a();
        }
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(1u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
}

TEST_F(InspectorCSSAgentTest, TransitiveFunction) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() {
        result: --b();
      }
      @function --b() {
        result: 2px;
      }
      @function --c() {
        result: 1000px;
      }
      #e { width: --a(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(2u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
  EXPECT_TRUE(FindFunctionRule(function_rules, "--b"));
  EXPECT_FALSE(FindFunctionRule(function_rules, "--c"));
}

TEST_F(InspectorCSSAgentTest, TransitiveFunctionBranches) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() {
        @media (width > 0px) {
          --x: --b();
        }
        @media (width < -9000px) {
          /* Branch not taken, but referenced functions are still relevant. */
          --x: --c();
        }
        result: var(--x);
      }
      @function --b() {
        result: 2px;
      }
      @function --c() {
        result: 3px;
      }
      @function --d() {
        result: 3px;
      }
      #e { width: --a(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(3u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
  EXPECT_TRUE(FindFunctionRule(function_rules, "--b"));
  EXPECT_TRUE(FindFunctionRule(function_rules, "--c"));
  EXPECT_FALSE(FindFunctionRule(function_rules, "--d"));
}

TEST_F(InspectorCSSAgentTest, DashedFunctionDedup) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e { left: --a(); }
      div { top: --a(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  // There should only be one entry, despite --a() appearing twice.
  EXPECT_EQ(1u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
}

TEST_F(InspectorCSSAgentTest, DashedFunctionUnknown) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e { left: --unknown(); right: --a(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(1u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
  // The presence of a reference to a function that does not exist
  // should not cause a crash.
}

}  // namespace blink
