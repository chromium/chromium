// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
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
    auto* evaluator = MakeGarbageCollected<ContainerQueryEvaluator>(
        width, height, contained_axes);
    return evaluator->Eval(*container_query);
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

}  // namespace blink
