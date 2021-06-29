// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"

#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ContainerQueryTest : public PageTestBase,
                           private ScopedCSSContainerQueriesForTest {
 public:
  ContainerQueryTest() : ScopedCSSContainerQueriesForTest(true) {}

  StyleRuleContainer* ParseAtContainer(String rule_string) {
    return DynamicTo<StyleRuleContainer>(
        css_test_helpers::ParseRule(GetDocument(), rule_string));
  }

  ContainerQuery* ParseContainerQuery(String query) {
    String rule = "@container " + query + " {}";
    StyleRuleContainer* container = ParseAtContainer(rule);
    if (!container)
      return nullptr;
    return &container->GetContainerQuery();
  }

  PhysicalAxes QueriedAxes(String query) {
    ContainerQuery* container_query = ParseContainerQuery(query);
    DCHECK(container_query);
    return container_query->QueriedAxes();
  }

  String SerializeCondition(StyleRuleContainer* container) {
    if (!container)
      return "";
    return container->GetContainerQuery().ToString();
  }

  // TODO(crbug.com/1145970): Remove this when ContainerQuery no longer
  // relies on MediaQuerySet.
  MediaQuerySet& GetMediaQuerySet(ContainerQuery& container_query) {
    return *container_query.media_queries_;
  }

  const CSSValue* ComputedValue(Element* element, String property_name) {
    CSSPropertyRef ref(property_name, GetDocument());
    DCHECK(ref.IsValid());
    return ref.GetProperty().CSSValueFromComputedStyle(
        element->ComputedStyleRef(),
        /* layout_object */ nullptr,
        /* allow_visited_style */ false);
  }

  String ComputedValueString(Element* element, String property_name) {
    if (const CSSValue* value = ComputedValue(element, property_name))
      return value->CssText();
    return g_null_atom;
  }
};

TEST_F(ContainerQueryTest, PreludeParsing) {
  // Valid:
  EXPECT_EQ(
      "(min-width: 300px)",
      SerializeCondition(ParseAtContainer("@container (min-width: 300px) {}")));
  EXPECT_EQ(
      "(max-width: 500px)",
      SerializeCondition(ParseAtContainer("@container (max-width: 500px) {}")));

  // TODO(crbug.com/1145970): The MediaQuery parser emits a "not all"
  // MediaQuery for parse failures. When ContainerQuery has its own parser,
  // it should probably return nullptr instead.

  // Invalid:
  EXPECT_EQ("not all",
            SerializeCondition(ParseAtContainer("@container 100px {}")));
  EXPECT_EQ("not all",
            SerializeCondition(ParseAtContainer("@container calc(1) {}")));
}

TEST_F(ContainerQueryTest, RuleParsing) {
  StyleRuleContainer* container = ParseAtContainer(R"CSS(
    @container (min-width: 100px) {
      div { width: 100px; }
      span { height: 100px; }
    }
  )CSS");
  ASSERT_TRUE(container);

  CSSStyleSheet* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
  auto* rule =
      DynamicTo<CSSContainerRule>(container->CreateCSSOMWrapper(sheet));
  ASSERT_TRUE(rule);
  ASSERT_EQ(2u, rule->length());

  auto* div_rule = rule->Item(0);
  ASSERT_TRUE(div_rule);
  EXPECT_EQ("div { width: 100px; }", div_rule->cssText());

  auto* span_rule = rule->Item(1);
  ASSERT_TRUE(span_rule);
  EXPECT_EQ("span { height: 100px; }", span_rule->cssText());
}

TEST_F(ContainerQueryTest, RuleCopy) {
  StyleRuleContainer* container = ParseAtContainer(R"CSS(
    @container (min-width: 100px) {
      div { width: 100px; }
    }
  )CSS");
  ASSERT_TRUE(container);

  // Copy via StyleRuleBase to test switch dispatch.
  auto* copy_base = static_cast<StyleRuleBase*>(container)->Copy();
  auto* copy = DynamicTo<StyleRuleContainer>(copy_base);
  ASSERT_TRUE(copy);

  // The StyleRuleContainer object should be copied.
  EXPECT_NE(container, copy);

  // The rules should be copied.
  auto rules = container->ChildRules();
  auto rules_copy = copy->ChildRules();
  ASSERT_EQ(1u, rules.size());
  ASSERT_EQ(1u, rules_copy.size());
  EXPECT_NE(rules[0], rules_copy[0]);

  // The ContainerQuery should be copied.
  EXPECT_NE(&container->GetContainerQuery(), &copy->GetContainerQuery());

  // The inner MediaQuerySet should be copied.
  EXPECT_NE(&GetMediaQuerySet(container->GetContainerQuery()),
            &GetMediaQuerySet(copy->GetContainerQuery()));
}

TEST_F(ContainerQueryTest, ContainerQueryEvaluation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        contain: size layout style;
        width: 500px;
        height: 500px;
      }
      #container.adjust {
        width: 600px;
      }

      div { z-index:1; }
      /* Should apply: */
      @container (min-width: 500px) {
        div { z-index:2; }
      }
      /* Should initially not apply: */
      @container (min-width: 600px) {
        div { z-index:3; }
      }
    </style>
    <div id=container>
      <div id=div></div>
    </div>
  )HTML");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  EXPECT_EQ(2, div->ComputedStyleRef().ZIndex());

  // Check that dependent elements are responsive to changes:
  Element* container = GetDocument().getElementById("container");
  ASSERT_TRUE(container);
  container->setAttribute(html_names::kClassAttr, "adjust");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(3, div->ComputedStyleRef().ZIndex());

  container->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2, div->ComputedStyleRef().ZIndex());
}

TEST_F(ContainerQueryTest, QueriedAxes) {
  auto horizontal = PhysicalAxes(kPhysicalAxisHorizontal);
  auto vertical = PhysicalAxes(kPhysicalAxisVertical);
  auto both = PhysicalAxes(kPhysicalAxisBoth);
  auto none = PhysicalAxes(kPhysicalAxisNone);

  EXPECT_EQ(horizontal, QueriedAxes("(min-width: 1px)"));
  EXPECT_EQ(horizontal, QueriedAxes("(max-width: 1px)"));
  EXPECT_EQ(horizontal, QueriedAxes("(width: 1px)"));

  EXPECT_EQ(vertical, QueriedAxes("(min-height: 1px)"));
  EXPECT_EQ(vertical, QueriedAxes("(max-height: 1px)"));
  EXPECT_EQ(vertical, QueriedAxes("(height: 1px)"));

  EXPECT_EQ(both, QueriedAxes("(width: 1px) and (height: 1px)"));
  EXPECT_EQ(both, QueriedAxes("(min-width: 1px) and (max-height: 1px)"));

  // TODO(crbug.com/1145970): We want to test the case where no axes are
  // queried (kPhysicalAxisNone). This can (for now) be achieved by using
  // some media query feature (e.g. "resolution"). Ultimately, using
  // "resolution" will not be allowed in @container: we will then need to find
  // another way to author a container query that queries no axes (or make it
  // illegal altogether).
  EXPECT_EQ(none, QueriedAxes("(resolution: 150dpi)"));
}

TEST_F(ContainerQueryTest, QueryZoom) {
  GetFrame().SetPageZoomFactor(2.0f);

  SetBodyInnerHTML(R"HTML(
    <style>
      #container1 {
        width: 100px;
        height: 200px;
        container-type: inline-size block-size;
      }
      #container2 {
        width: 200px;
        height: 400px;
        container-type: inline-size block-size;
      }
      @container (width: 100px) {
        div { --w100:1; }
      }
      @container (width: 200px) {
        div { --w200:1; }
      }
      @container (height: 200px) {
        div { --h200:1; }
      }
      @container (height: 400px) {
        div { --h400:1; }
      }
    </style>
    <div id=container1>
      <div id=target1></div>
    </div>
    <div id=container2>
      <div id=target2></div>
    </div>
  )HTML");

  Element* target1 = GetDocument().getElementById("target1");
  Element* target2 = GetDocument().getElementById("target2");
  ASSERT_TRUE(target1);
  ASSERT_TRUE(target2);

  EXPECT_TRUE(target1->ComputedStyleRef().GetVariableData("--w100"));
  EXPECT_TRUE(target1->ComputedStyleRef().GetVariableData("--h200"));
  EXPECT_FALSE(target1->ComputedStyleRef().GetVariableData("--w200"));
  EXPECT_FALSE(target1->ComputedStyleRef().GetVariableData("--h400"));

  EXPECT_FALSE(target2->ComputedStyleRef().GetVariableData("--w100"));
  EXPECT_FALSE(target2->ComputedStyleRef().GetVariableData("--h200"));
  EXPECT_TRUE(target2->ComputedStyleRef().GetVariableData("--w200"));
  EXPECT_TRUE(target2->ComputedStyleRef().GetVariableData("--h400"));
}

TEST_F(ContainerQueryTest, ContainerUnitsViewportFallback) {
  using css_test_helpers::RegisterProperty;

  ScopedCSSContainerRelativeUnitsForTest feature(true);

  RegisterProperty(GetDocument(), "--qw", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--qi", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--qh", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--qb", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--qmin", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--qmax", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--fallback-w", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--fallback-h", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--fallback-min-qi-vh", "<length>", "0px",
                   false);
  RegisterProperty(GetDocument(), "--fallback-min-qb-vw", "<length>", "0px",
                   false);
  RegisterProperty(GetDocument(), "--fallback-max-qi-vh", "<length>", "0px",
                   false);
  RegisterProperty(GetDocument(), "--fallback-max-qb-vw", "<length>", "0px",
                   false);

  SetBodyInnerHTML(R"HTML(
    <style>
      #inline, #block {
        width: 100px;
        height: 100px;
      }
      #inline {
        container-type: inline-size;
      }
      #block {
        container-type: block-size;
      }
      #inline_target, #block_target {
        --qw: 10qw;
        --qi: 10qi;
        --qh: 10qh;
        --qb: 10qb;
        --qmin: 10qmin;
        --qmax: 10qmax;
        --fallback-w: 10vw;
        --fallback-h: 10vh;
        --fallback-min-qi-vh: min(10qi, 10vh);
        --fallback-min-qb-vw: min(10qb, 10vw);
        --fallback-max-qi-vh: max(10qi, 10vh);
        --fallback-max-qb-vw: max(10qb, 10vw);
      }
    </style>
    <div id=inline>
      <div id="inline_target"></div>
    </div>
    <div id=block>
      <div id="block_target"></div>
    </div>
  )HTML");

  Element* inline_target = GetDocument().getElementById("inline_target");
  ASSERT_TRUE(inline_target);
  EXPECT_EQ(ComputedValueString(inline_target, "--qw"), "10px");
  EXPECT_EQ(ComputedValueString(inline_target, "--qi"), "10px");
  EXPECT_EQ(ComputedValueString(inline_target, "--qh"),
            ComputedValueString(inline_target, "--fallback-h"));
  EXPECT_EQ(ComputedValueString(inline_target, "--qb"),
            ComputedValueString(inline_target, "--fallback-h"));
  EXPECT_EQ(ComputedValueString(inline_target, "--qmin"),
            ComputedValueString(inline_target, "--fallback-min-qi-vh"));
  EXPECT_EQ(ComputedValueString(inline_target, "--qmax"),
            ComputedValueString(inline_target, "--fallback-max-qi-vh"));

  Element* block_target = GetDocument().getElementById("block_target");
  ASSERT_TRUE(block_target);
  EXPECT_EQ(ComputedValueString(block_target, "--qw"),
            ComputedValueString(block_target, "--fallback-w"));
  EXPECT_EQ(ComputedValueString(block_target, "--qi"),
            ComputedValueString(block_target, "--fallback-w"));
  EXPECT_EQ(ComputedValueString(block_target, "--qh"), "10px");
  EXPECT_EQ(ComputedValueString(block_target, "--qb"), "10px");
  EXPECT_EQ(ComputedValueString(block_target, "--qmin"),
            ComputedValueString(block_target, "--fallback-min-qb-vw"));
  EXPECT_EQ(ComputedValueString(block_target, "--qmax"),
            ComputedValueString(block_target, "--fallback-max-qb-vw"));
}

}  // namespace blink
