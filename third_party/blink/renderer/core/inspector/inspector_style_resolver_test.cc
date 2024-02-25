// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_style_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class InspectorStyleResolverTest : public testing::Test {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  test::TaskEnvironment task_environment_;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void InspectorStyleResolverTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
}

TEST_F(InspectorStyleResolverTest, DirectlyMatchedRules) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #grid {
        display: grid;
        grid-gap: 10px;
        grid-template-columns: 100px 1fr 20%;
      }
    </style>
    <div id="grid">
    </div>
  )HTML");
  Element* grid = GetDocument().getElementById(AtomicString("grid"));
  InspectorStyleResolver resolver(grid, kPseudoIdNone, g_null_atom);
  RuleIndexList* matched_rules = resolver.MatchedRules();
  // Some rules are coming for UA.
  EXPECT_EQ(matched_rules->size(), 3u);
  auto rule = matched_rules->at(2);
  EXPECT_EQ(
      "#grid { display: grid; gap: 10px; grid-template-columns: 100px 1fr 20%; "
      "}",
      rule.first->cssText());
}

TEST_F(InspectorStyleResolverTest, ParentRules) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #grid-container {
        display: inline-grid;
        grid-gap: 5px;
        grid-template-columns: 50px 1fr 10%;
      }
      #grid {
        display: grid;
        grid-gap: 10px;
        grid-template-columns: 100px 2fr 20%;
      }
    </style>
    <div id="grid-container">
      <div id="grid"></div>
    </div>
  )HTML");
  Element* grid = GetDocument().getElementById(AtomicString("grid"));
  InspectorStyleResolver resolver(grid, kPseudoIdNone, g_null_atom);
  HeapVector<Member<InspectorCSSMatchedRules>> parent_rules =
      resolver.ParentRules();
  Element* grid_container =
      GetDocument().getElementById(AtomicString("grid-container"));
  // Some rules are coming for UA.
  EXPECT_EQ(parent_rules.size(), 3u);
  // grid_container is the first parent.
  EXPECT_EQ(parent_rules.at(0)->element, grid_container);
  // Some rules are coming from UA.
  EXPECT_EQ(parent_rules.at(0)->matched_rules->size(), 3u);
  auto rule = parent_rules.at(0)->matched_rules->at(2);
  EXPECT_EQ(rule.first->cssText(),
            "#grid-container { display: inline-grid; gap: 5px; "
            "grid-template-columns: 50px 1fr 10%; }");
}

TEST_F(InspectorStyleResolverTest, HighlightPseudoInheritance) {
  ScopedHighlightInheritanceForTest highlight_inheritance(true);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #outer::selection {
        color: limegreen;
      }

      #middle::highlight(foo) {
        color: red;
      }

      #middle::highlight(bar) {
        color: orange;
      }

      #target::highlight(baz) {
        color: lightblue;
      }

      body::first-letter {
        color: yellow;
      }
    </style>
    <body>
      <div id="outer">
        <div>
          <div id="middle">
            <span id="target">target</span>
          </div>
        </div>
      </div>
    </body>
  )HTML");
  Element* target = GetDocument().getElementById(AtomicString("target"));
  Element* middle = GetDocument().getElementById(AtomicString("middle"));
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* body = GetDocument().QuerySelector(AtomicString("body"));
  InspectorStyleResolver resolver(target, kPseudoIdNone, g_null_atom);
  HeapVector<Member<InspectorCSSMatchedPseudoElements>> parent_pseudos =
      resolver.ParentPseudoElementRules();
  EXPECT_EQ(5u, parent_pseudos.size());

  // <div id="middle">
  EXPECT_EQ(middle, parent_pseudos.at(0)->element);
  EXPECT_EQ(1u, parent_pseudos.at(0)->pseudo_element_rules.size());
  EXPECT_EQ(kPseudoIdHighlight,
            parent_pseudos.at(0)->pseudo_element_rules.at(0)->pseudo_id);
  EXPECT_EQ(
      2u,
      parent_pseudos.at(0)->pseudo_element_rules.at(0)->matched_rules->size());
  EXPECT_EQ("#middle::highlight(foo) { color: red; }",
            parent_pseudos.at(0)
                ->pseudo_element_rules.at(0)
                ->matched_rules->at(0)
                .first->cssText());
  EXPECT_EQ("#middle::highlight(bar) { color: orange; }",
            parent_pseudos.at(0)
                ->pseudo_element_rules.at(0)
                ->matched_rules->at(1)
                .first->cssText());

  // <div>
  EXPECT_EQ(0u, parent_pseudos.at(1)->pseudo_element_rules.size());

  // <div id="outer">
  EXPECT_EQ(outer, parent_pseudos.at(2)->element);
  EXPECT_EQ(1u, parent_pseudos.at(2)->pseudo_element_rules.size());
  EXPECT_EQ(kPseudoIdSelection,
            parent_pseudos.at(2)->pseudo_element_rules.at(0)->pseudo_id);
  EXPECT_EQ(
      1u,
      parent_pseudos.at(2)->pseudo_element_rules.at(0)->matched_rules->size());
  EXPECT_EQ("#outer::selection { color: limegreen; }",
            parent_pseudos.at(2)
                ->pseudo_element_rules.at(0)
                ->matched_rules->at(0)
                .first->cssText());

  // <body>
  EXPECT_EQ(body, parent_pseudos.at(3)->element);
  EXPECT_EQ(0u, parent_pseudos.at(3)->pseudo_element_rules.size());

  // <html>
  EXPECT_EQ(0u, parent_pseudos.at(4)->pseudo_element_rules.size());
}

}  // namespace blink
