// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ContainerQueryEvaluatorTest : public PageTestBase,
                                    private ScopedCSSContainerQueriesForTest {
 public:
  ContainerQueryEvaluatorTest() : ScopedCSSContainerQueriesForTest(true) {}

  ContainerQuery* ParseContainer(String query) {
    String rule = "@container " + query + " {}";
    auto* style_rule = DynamicTo<StyleRuleContainer>(
        css_test_helpers::ParseRule(GetDocument(), rule));
    if (!style_rule)
      return nullptr;
    return &style_rule->GetContainerQuery();
  }

  bool Eval(String query,
            double width,
            double height,
            PhysicalAxes contained_axes) {
    ContainerQuery* container_query = ParseContainer(query);
    DCHECK(container_query);
    auto* evaluator = MakeGarbageCollected<ContainerQueryEvaluator>();
    evaluator->ContainerChanged(
        PhysicalSize(LayoutUnit(width), LayoutUnit(height)), contained_axes);
    return evaluator->Eval(*container_query);
  }

  bool ContainerChanged(ContainerQueryEvaluator* evaluator,
                        PhysicalSize size,
                        PhysicalAxes axes) {
    return evaluator->ContainerChanged(size, axes) !=
           ContainerQueryEvaluator::Change::kNone;
  }

  const PhysicalAxes none{kPhysicalAxisNone};
  const PhysicalAxes both{kPhysicalAxisBoth};
  const PhysicalAxes horizontal{kPhysicalAxisHorizontal};
  const PhysicalAxes vertical{kPhysicalAxisVertical};
};

TEST_F(ContainerQueryEvaluatorTest, ContainmentMatch) {
  {
    String query = "(min-width: 100px)";
    EXPECT_TRUE(Eval(query, 100.0, 100.0, horizontal));
    EXPECT_TRUE(Eval(query, 100.0, 100.0, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, vertical));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, none));
    EXPECT_FALSE(Eval(query, 99.0, 100.0, horizontal));
  }

  {
    String query = "(min-height: 100px)";
    EXPECT_TRUE(Eval(query, 100.0, 100.0, vertical));
    EXPECT_TRUE(Eval(query, 100.0, 100.0, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, horizontal));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, none));
    EXPECT_FALSE(Eval(query, 100.0, 99.0, vertical));
  }

  {
    String query = "(min-width: 100px) and (min-height: 100px)";
    EXPECT_TRUE(Eval(query, 100.0, 100.0, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, vertical));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, horizontal));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, none));
    EXPECT_FALSE(Eval(query, 100.0, 99.0, both));
    EXPECT_FALSE(Eval(query, 99.0, 100.0, both));
  }
}

TEST_F(ContainerQueryEvaluatorTest, ContainerChanged) {
  PhysicalSize size_100(LayoutUnit(100), LayoutUnit(100));
  PhysicalSize size_200(LayoutUnit(200), LayoutUnit(200));

  ContainerQuery* container_query_100 = ParseContainer("(min-width: 100px)");
  ContainerQuery* container_query_200 = ParseContainer("(min-width: 200px)");
  ASSERT_TRUE(container_query_100);
  ASSERT_TRUE(container_query_200);

  auto* evaluator = MakeGarbageCollected<ContainerQueryEvaluator>();
  evaluator->ContainerChanged(size_100, horizontal);
  ASSERT_TRUE(evaluator);

  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_100));
  EXPECT_FALSE(evaluator->EvalAndAdd(*container_query_200));

  EXPECT_FALSE(ContainerChanged(evaluator, size_100, horizontal));
  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_100));
  EXPECT_FALSE(evaluator->EvalAndAdd(*container_query_200));

  EXPECT_TRUE(ContainerChanged(evaluator, size_200, horizontal));
  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_100));
  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_200));

  EXPECT_FALSE(ContainerChanged(evaluator, size_200, horizontal));
  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_100));
  EXPECT_TRUE(evaluator->EvalAndAdd(*container_query_200));

  EXPECT_TRUE(ContainerChanged(evaluator, size_200, vertical));
  EXPECT_FALSE(evaluator->EvalAndAdd(*container_query_100));
  EXPECT_FALSE(evaluator->EvalAndAdd(*container_query_200));
}

TEST_F(ContainerQueryEvaluatorTest, SizeInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        contain: size layout style;
        width: 500px;
        height: 500px;
      }
      @container (min-width: 500px) {
        div { z-index:1; }
      }
    </style>
    <div id=container>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById("container");
  ASSERT_TRUE(container);
  ASSERT_TRUE(container->GetContainerQueryEvaluator());

  {
    // Causes re-layout, but the size does not change
    container->SetInlineStyleProperty(CSSPropertyID::kFloat, "left");

    unsigned before_count = GetStyleEngine().StyleForElementCount();

    UpdateAllLifecyclePhasesForTest();

    unsigned after_count = GetStyleEngine().StyleForElementCount();

    // Only #container should be affected. In particular, we should not
    // recalc any style for <div> children of #container.
    EXPECT_EQ(1u, after_count - before_count);
  }

  {
    // The size of the container changes, but it does not matter for
    // the result of the query (min-width: 500px).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "600px");

    unsigned before_count = GetStyleEngine().StyleForElementCount();

    UpdateAllLifecyclePhasesForTest();

    unsigned after_count = GetStyleEngine().StyleForElementCount();

    // Only #container should be affected. In particular, we should not
    // recalc any style for <div> children of #container.
    EXPECT_EQ(1u, after_count - before_count);
  }
}

TEST_F(ContainerQueryEvaluatorTest, DependentQueries) {
  PhysicalSize size_100(LayoutUnit(100), LayoutUnit(100));
  PhysicalSize size_150(LayoutUnit(150), LayoutUnit(150));
  PhysicalSize size_200(LayoutUnit(200), LayoutUnit(200));
  PhysicalSize size_300(LayoutUnit(300), LayoutUnit(300));
  PhysicalSize size_400(LayoutUnit(400), LayoutUnit(400));

  ContainerQuery* query_min_200px = ParseContainer("(min-width: 200px)");
  ContainerQuery* query_max_300px = ParseContainer("(max-width: 300px)");
  ASSERT_TRUE(query_min_200px);

  auto* evaluator = MakeGarbageCollected<ContainerQueryEvaluator>();
  evaluator->ContainerChanged(size_100, horizontal);

  evaluator->EvalAndAdd(*query_min_200px);
  evaluator->EvalAndAdd(*query_max_300px);
  // Updating with the same size as we initially had should not invalidate
  // any query results.
  EXPECT_FALSE(ContainerChanged(evaluator, size_100, horizontal));

  // Makes no difference for either of (min-width: 200px), (max-width: 300px):
  EXPECT_FALSE(ContainerChanged(evaluator, size_150, horizontal));

  // (min-width: 200px) becomes true:
  EXPECT_TRUE(ContainerChanged(evaluator, size_200, horizontal));

  evaluator->EvalAndAdd(*query_min_200px);
  evaluator->EvalAndAdd(*query_max_300px);
  EXPECT_FALSE(ContainerChanged(evaluator, size_200, horizontal));

  // Makes no difference for either of (min-width: 200px), (max-width: 300px):
  EXPECT_FALSE(ContainerChanged(evaluator, size_300, horizontal));

  // (max-width: 300px) becomes false:
  EXPECT_TRUE(ContainerChanged(evaluator, size_400, horizontal));
}

TEST_F(ContainerQueryEvaluatorTest, EvaluatorDisplayNone) {
  SetBodyInnerHTML(R"HTML(
    <style>
      main {
        display: block;
        contain: size layout style;
        width: 500px;
        height: 500px;
      }
      main.none {
        display: none;
      }
      @container (min-width: 500px) {
        div { --x:test; }
      }
    </style>
    <main id=outer>
      <div>
        <main id=inner>
          <div></div>
        </main>
      </div>
    </main>
  )HTML");

  // Inner container
  Element* inner = GetDocument().getElementById("inner");
  ASSERT_TRUE(inner);
  EXPECT_TRUE(inner->GetContainerQueryEvaluator());

  inner->classList().Add("none");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(inner->GetContainerQueryEvaluator());

  inner->classList().Remove("none");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(inner->GetContainerQueryEvaluator());

  // Outer container
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_TRUE(outer);
  EXPECT_TRUE(outer->GetContainerQueryEvaluator());
  EXPECT_TRUE(inner->GetContainerQueryEvaluator());

  outer->classList().Add("none");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(outer->GetContainerQueryEvaluator());
  EXPECT_FALSE(inner->GetContainerQueryEvaluator());

  outer->classList().Remove("none");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(outer->GetContainerQueryEvaluator());
  EXPECT_TRUE(inner->GetContainerQueryEvaluator());
}

}  // namespace blink
