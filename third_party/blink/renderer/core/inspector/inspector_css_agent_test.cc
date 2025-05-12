// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"

#include <algorithm>

#include "third_party/blink/renderer/core/css/css_function_rule.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_resolver.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

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
        sheets, *resolver.MatchedRules(), function_rules);
    return function_rules;
  }

  CSSFunctionRule* FindFunctionRule(
      const HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>&
          function_rules,
      const ScopedCSSName* scoped_name) {
    auto it = function_rules.find(scoped_name);
    return it != function_rules.end() ? it->value.Get() : nullptr;
  }

  CSSFunctionRule* FindFunctionRule(
      const HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>&
          function_rules,
      const char* name) {
    auto* scoped_name = MakeGarbageCollected<ScopedCSSName>(
        AtomicString(name), /*tree_scope=*/&GetDocument());
    return FindFunctionRule(function_rules, scoped_name);
  }

  String GetComputedStyle(Element* element, CSSPropertyID property_id) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    const ComputedStyle* computed_style = element->GetComputedStyle();
    const LayoutObject* layout_object = element->GetLayoutObject();
    const CSSValue* computed_value =
        property.CSSProperty::CSSValueFromComputedStyle(
            *computed_style, layout_object, /* allow_visited_style */ false,
            CSSValuePhase::kResolvedValue);
    return computed_value->CssText();
  }

  String InspectorResolvePercentageValues(Element* element,
                                          CSSPropertyID property_id,
                                          String value_str) {
    const CSSPropertyName property_name =
        CSSProperty::Get(property_id).GetCSSPropertyName();
    ScopedNullExecutionContext execution_context;
    auto* document =
        Document::CreateForTest(execution_context.GetExecutionContext());
    const CSSValue* value = css_test_helpers::ParseValue(
        *document, "<length-percentage>", value_str);
    return InspectorCSSAgent::ResolvePercentagesValues(element, property_name,
                                                       value, value_str);
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

TEST_F(InspectorCSSAgentTest, SameFunctionNamesAcrossShadows) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <style>
      @function --a() {
        result: 10px;
      }
      @function --b() {
        result: --a(); /* Outer --a() */
      }
    </style>
    <div id=host>
      <template shadowrootmode=open>
        <style>
          @function --a() {
            result: --b();
          }
          :host {
             width: --a(); /* Inner --a() */
          }
        </style>
      </template>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#host");
  EXPECT_EQ(3u, function_rules.size());

  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);
  const TreeScope* document_scope = &GetDocument();
  const TreeScope* inner_scope = host->GetShadowRoot();
  ASSERT_TRUE(inner_scope);

  CSSFunctionRule* inner_a = FindFunctionRule(
      function_rules,
      MakeGarbageCollected<ScopedCSSName>(AtomicString("--a"), inner_scope));
  CSSFunctionRule* outer_a = FindFunctionRule(
      function_rules,
      MakeGarbageCollected<ScopedCSSName>(AtomicString("--a"), document_scope));
  CSSFunctionRule* outer_b = FindFunctionRule(
      function_rules,
      MakeGarbageCollected<ScopedCSSName>(AtomicString("--b"), inner_scope));

  EXPECT_TRUE(inner_a);
  EXPECT_TRUE(outer_a);
  EXPECT_TRUE(outer_b);
  EXPECT_NE(inner_a, outer_a);

  // `function_rules` maps invocations to the corresponding invoked functions,
  // and there was no invocation of --b() in the outer scope.
  EXPECT_FALSE(FindFunctionRule(
      function_rules, MakeGarbageCollected<ScopedCSSName>(AtomicString("--b"),
                                                          document_scope)));
}

const CSSPropertyID DirectionAwareConverterTestData[] = {
    // clang-format off
  CSSPropertyID::kWidth,
  CSSPropertyID::kHeight,
  CSSPropertyID::kMarginLeft,
  CSSPropertyID::kMarginTop,
  CSSPropertyID::kMarginRight,
  CSSPropertyID::kMarginBottom,
  CSSPropertyID::kPaddingLeft,
  CSSPropertyID::kPaddingTop,
  CSSPropertyID::kPaddingRight,
  CSSPropertyID::kPaddingBottom,
  CSSPropertyID::kLeft,
  CSSPropertyID::kTop,
  CSSPropertyID::kRight,
  CSSPropertyID::kBottom,
    // clang-format on
};

class PercentageResolutionTest
    : public InspectorCSSAgentTest,
      public testing::WithParamInterface<CSSPropertyID> {};

INSTANTIATE_TEST_SUITE_P(InspectorCSSAgentTest,
                         PercentageResolutionTest,
                         testing::ValuesIn(DirectionAwareConverterTestData));

TEST_P(PercentageResolutionTest, ResolvePercentagesSimple) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #outer {
        width: 100px;
        height: 300px;
      }
    </style>
    <div id=outer>
    </div>
  )HTML");

  String value("calc(1px + 10%)");

  CSSPropertyID property_id = GetParam();
  AtomicString property_name =
      CSSProperty::Get(property_id).GetCSSPropertyName().ToAtomicString();

  StringBuilder html_string;
  html_string.Append("<div id=inner style=\"position: absolute; ");
  html_string.Append(property_name);
  html_string.Append(": ");
  html_string.Append(value);
  html_string.Append(";\"></div>");
  GetElementById("outer")->setInnerHTML(html_string.ToString());

  UpdateAllLifecyclePhasesForTest();

  Element* element = GetElementById("inner");
  String expected = GetComputedStyle(element, property_id);
  String actual = InspectorResolvePercentageValues(element, property_id, value);
  EXPECT_EQ(actual, expected);
}

TEST_F(InspectorCSSAgentTest, ResolvePercentagesSizingProperties) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #outer {
        width: 100px;
        height: 300px
      }
      #inner {
        width: calc(1px + 10%);
        height: calc(1px + 10%);
      }
    </style>
    <div id=outer>
      <div id=inner></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* element = GetElementById("inner");
  String value("calc(1px + 10%)");

  String expected_inline = GetComputedStyle(element, CSSPropertyID::kWidth);
  String expected_block = GetComputedStyle(element, CSSPropertyID::kHeight);

  EXPECT_EQ(InspectorResolvePercentageValues(element, CSSPropertyID::kMinWidth,
                                             value),
            expected_inline);
  EXPECT_EQ(InspectorResolvePercentageValues(element, CSSPropertyID::kMaxWidth,
                                             value),
            expected_inline);
  EXPECT_EQ(InspectorResolvePercentageValues(element, CSSPropertyID::kMinHeight,
                                             value),
            expected_block);
  EXPECT_EQ(InspectorResolvePercentageValues(element, CSSPropertyID::kMaxHeight,
                                             value),
            expected_block);
}

TEST_F(InspectorCSSAgentTest, ResolvePercentagesAnchorPositioning) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #cb {
        width: 300px;
        height: 150px;
        position: relative;
      }
      #anchor {
        position: absolute;
        left: 50px;
        top: 20px;
        anchor-name: --a;
        width: 50px;
        height: 50px;
      }
      #anchored {
        position-anchor: --a;
        position-area: center right;
        position: absolute;
        width: 10%;
        height: 10%;
      }
    </style>
    <div id=cb>
      <div id=anchor>
        Anchor
      </div>
      <div id=anchored>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* element = GetElementById("anchored");
  String value("10%");

  String expected_inline = GetComputedStyle(element, CSSPropertyID::kWidth);
  String expected_block = GetComputedStyle(element, CSSPropertyID::kHeight);

  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kWidth, value),
      expected_inline);
  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kWidth, value),
      "20px");
  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kHeight, value),
      expected_block);
  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kHeight, value),
      "5px");
}

TEST_F(InspectorCSSAgentTest, ResolvePercentagesDisplayTable) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .table {
        display: table;
        width: 100px;
        height: 300px;
      }
      #column1 {
        display: table-column;
      }
      #column2 {
        display: table-column;
      }
      .row {
        display: table-row;
      }
      .cell {
        display: table-cell;
      }
    </style>

    <div class="table">
        <div id="column1"></div>
        <div id="column2"></div>
        <div class="row">
          <div class="cell">row 1, cell 1</div>
          <div class="cell">row 1, cell 2</div>
        </div>
        <div class="row">
          <div class="cell">row 2, cell 1</div>
          <div class="cell">row 2, cell 2</div>
        </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* element = GetElementById("column1");
  String value("10%");

  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kWidth, value),
      value);
  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kHeight, value),
      value);
}

}  // namespace blink
