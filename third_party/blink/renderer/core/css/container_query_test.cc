// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"

#include "third_party/blink/renderer/core/animation/css/css_animation_update_scope.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ContainerQueryTest : public PageTestBase,
                           private ScopedCSSContainerQueriesForTest,
                           private ScopedLayoutNGForTest {
 public:
  ContainerQueryTest()
      : ScopedCSSContainerQueriesForTest(true), ScopedLayoutNGForTest(true) {}

  bool HasUnknown(StyleRuleContainer* rule) {
    return rule && rule->GetContainerQuery().Query().HasUnknown();
  }

  // Note that these parsing utils treat "unknown" values as parse
  // errors.
  //
  // https://drafts.csswg.org/mediaqueries-4/#evaluating

  StyleRuleContainer* ParseAtContainer(String rule_string) {
    auto* rule = DynamicTo<StyleRuleContainer>(
        css_test_helpers::ParseRule(GetDocument(), rule_string));
    return HasUnknown(rule) ? nullptr : rule;
  }

  ContainerQuery* ParseContainerQuery(String query) {
    String rule = "@container " + query + " {}";
    StyleRuleContainer* container = ParseAtContainer(rule);
    if (!container)
      return nullptr;
    return &container->GetContainerQuery();
  }

  String SerializeCondition(StyleRuleContainer* container) {
    if (!container)
      return "";
    return container->GetContainerQuery().ToString();
  }

  const MediaQueryExpNode& GetInnerQuery(ContainerQuery& container_query) {
    return container_query.Query();
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

  // Get animations count for a specific element without force-updating
  // style and layout-tree.
  size_t GetAnimationsCount(Element* element) {
    if (auto* element_animations = element->GetElementAnimations())
      return element_animations->Animations().size();
    return 0;
  }

  size_t GetOldStylesCount(String html) {
    // Creating a CSSAnimationUpdateScope prevents old styles from being
    // cleared until this function completes.
    CSSAnimationUpdateScope animation_update_scope(GetDocument());
    SetBodyInnerHTML(html);
    DCHECK(CSSAnimationUpdateScope::CurrentData());
    return CSSAnimationUpdateScope::CurrentData()->old_styles_.size();
  }
};

TEST_F(ContainerQueryTest, PreludeParsing) {
  // Valid:
  EXPECT_EQ("size(min-width: 300px)",
            SerializeCondition(
                ParseAtContainer("@container size(min-width: 300px) {}")));
  EXPECT_EQ("size(max-width: 500px)",
            SerializeCondition(
                ParseAtContainer("@container size(max-width: 500px) {}")));
  EXPECT_EQ("(not size(max-width: 500px))",
            SerializeCondition(ParseAtContainer(
                "@container (not size(max-width: 500px)) {}")));
  EXPECT_EQ(
      "(size(max-width: 500px) and size(max-height: 500px))",
      SerializeCondition(ParseAtContainer("@container (size(max-width: 500px) "
                                          "and size(max-height: 500px)) {}")));
  EXPECT_EQ(
      "(size(max-width: 500px) or size(max-height: 500px))",
      SerializeCondition(ParseAtContainer("@container (size(max-width: 500px) "
                                          "or size(max-height: 500px)) {}")));
  EXPECT_EQ("size(width < 300px)", SerializeCondition(ParseAtContainer(
                                       "@container size(width < 300px) {}")));

  // Invalid:
  EXPECT_FALSE(ParseAtContainer("@container 100px {}"));
  EXPECT_FALSE(ParseAtContainer("@container calc(1) {}"));
  EXPECT_FALSE(ParseAtContainer("@container {}"));
  EXPECT_FALSE(
      ParseAtContainer("@container size(min-width: 300px) nonsense {}"));
  EXPECT_FALSE(ParseAtContainer("@container somename not size(width) {}"));
  EXPECT_FALSE(ParseAtContainer("@container size(width) and size(height) {}"));
  EXPECT_FALSE(ParseAtContainer("@container size(width) or size(height) {}"));
}

TEST_F(ContainerQueryTest, ValidFeatures) {
  // https://drafts.csswg.org/css-contain-3/#size-container
  EXPECT_TRUE(ParseAtContainer("@container size(width) {}"));
  EXPECT_TRUE(ParseAtContainer("@container size(min-width: 0px) {}"));
  EXPECT_TRUE(ParseAtContainer("@container size(max-width: 0px) {}"));
  EXPECT_TRUE(ParseAtContainer("@container size(height) {}"));
  EXPECT_TRUE(ParseAtContainer("@container size(min-height: 0px) {}"));
  EXPECT_TRUE(ParseAtContainer("@container size(max-height: 0px) {}"));
  EXPECT_TRUE(ParseAtContainer("@container size(aspect-ratio) {}"));
  EXPECT_TRUE(ParseAtContainer("@container size(min-aspect-ratio: 1/2) {}"));
  EXPECT_TRUE(ParseAtContainer("@container size(max-aspect-ratio: 1/2) {}"));
  EXPECT_TRUE(ParseAtContainer("@container size(orientation: portrait) {}"));

  EXPECT_FALSE(ParseAtContainer("@container (grid) {}"));
  EXPECT_FALSE(ParseAtContainer("@container size(color) {}"));
  EXPECT_FALSE(ParseAtContainer("@container size(color-index) {}"));
  EXPECT_FALSE(ParseAtContainer("@container size(color-index >= 1) {}"));
  EXPECT_FALSE(ParseAtContainer("@container size(grid) {}"));
  EXPECT_FALSE(ParseAtContainer("@container size(resolution: 150dpi) {}"));
}

TEST_F(ContainerQueryTest, RuleParsing) {
  StyleRuleContainer* container = ParseAtContainer(R"CSS(
    @container size(min-width: 100px) {
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
    @container size(min-width: 100px) {
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

  // The inner MediaQueryExpNode should be copied.
  EXPECT_NE(&GetInnerQuery(container->GetContainerQuery()),
            &GetInnerQuery(copy->GetContainerQuery()));
}

TEST_F(ContainerQueryTest, ContainerQueryEvaluation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        container-type: size;
        width: 500px;
        height: 500px;
      }
      #container.adjust {
        width: 600px;
      }

      div { z-index:1; }
      /* Should apply: */
      @container size(min-width: 500px) {
        div { z-index:2; }
      }
      /* Should initially not apply: */
      @container size(min-width: 600px) {
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

TEST_F(ContainerQueryTest, QueryZoom) {
  GetFrame().SetPageZoomFactor(2.0f);

  SetBodyInnerHTML(R"HTML(
    <style>
      #container1 {
        width: 100px;
        height: 200px;
        container-type: size;
      }
      #container2 {
        width: 200px;
        height: 400px;
        container-type: size;
      }
      @container size(width: 100px) {
        div { --w100:1; }
      }
      @container size(width: 200px) {
        div { --w200:1; }
      }
      @container size(height: 200px) {
        div { --h200:1; }
      }
      @container size(height: 400px) {
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

TEST_F(ContainerQueryTest, QueryFontRelativeWithZoom) {
  GetFrame().SetPageZoomFactor(2.0f);

  SetBodyInnerHTML(R"HTML(
    <style>
      #font-root {
        font-size: 50px;
      }
      #em-container {
        width: 10em;
        container-type: inline-size;
      }
      #ex-container {
        width: 10ex;
        container-type: inline-size;
      }
      #ch-container {
        width: 10ch;
        container-type: inline-size;
      }
      @container size(width: 10em) {
        #em-target { --em:1; }
      }
      @container size(width: 10ex) {
        #ex-target { --ex:1; }
      }
      @container size(width: 10ch) {
        #ch-target { --ch:1; }
      }
    </style>
    <div id="font-root">
      <div id="em-container">
        <div id="em-target"></div>
      </div>
      <div id="ex-container">
        <div id="ex-target"></div>
      </div>
      <div id="ch-container">
        <div id="ch-target"></div>
      </div>
    </div>
  )HTML");

  Element* em_target = GetDocument().getElementById("em-target");
  Element* ex_target = GetDocument().getElementById("ex-target");
  Element* ch_target = GetDocument().getElementById("ch-target");
  ASSERT_TRUE(em_target);
  ASSERT_TRUE(ex_target);
  ASSERT_TRUE(ch_target);

  EXPECT_TRUE(em_target->ComputedStyleRef().GetVariableData("--em"));
  EXPECT_TRUE(ex_target->ComputedStyleRef().GetVariableData("--ex"));
  EXPECT_TRUE(ch_target->ComputedStyleRef().GetVariableData("--ch"));
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

TEST_F(ContainerQueryTest, OldStyleForTransitions) {
  ScopedCSSDelayedAnimationUpdatesForTest scoped(true);

  Element* target = nullptr;

  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        container: inline-size;
        width: 20px;
      }
      #target {
        height: 10px;
        transition: height steps(2, start) 100s;
      }
      @container size(width: 120px) {
        #target { height: 20px; }
      }
      @container size(width: 130px) {
        #target { height: 30px; }
      }
      @container size(width: 140px) {
        #target { height: 40px; }
      }
    </style>
    <div id=container>
      <div id=target>
      </div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById("container");
  target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  ASSERT_TRUE(container);

  EXPECT_EQ("10px", ComputedValueString(target, "height"));
  EXPECT_EQ(0u, GetAnimationsCount(target));

  LogicalAxes contained_axes(kLogicalAxisInline);

  // Simulate a style and layout pass with multiple rounds of style recalc.
  {
    CSSAnimationUpdateScope animation_update_scope(GetDocument());

    // Should transition between [10px, 20px]. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(120, -1), contained_axes);
    EXPECT_EQ("15px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Should transition between [10px, 30px]. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(130, -1), contained_axes);
    EXPECT_EQ("20px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Should transition between [10px, 40px]. (Final round).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "140px");
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ("25px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));
  }

  // CSSAnimationUpdateScope going out of scope applies the update.
  EXPECT_EQ(1u, GetAnimationsCount(target));

  // Verify that the newly-updated Animation produces the correct value.
  target->SetNeedsAnimationStyleRecalc();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("25px", ComputedValueString(target, "height"));
}

TEST_F(ContainerQueryTest, TransitionAppearingInFinalPass) {
  ScopedCSSDelayedAnimationUpdatesForTest scoped(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        container: inline-size;
        width: 20px;
      }
      #target {
        height: 10px;
      }
      @container size(width: 120px) {
        #target { height: 20px; }
      }
      @container size(width: 130px) {
        #target { height: 30px; }
      }
      @container size(width: 140px) {
        #target {
          height: 40px;
          transition: height steps(2, start) 100s;
        }
      }
    </style>
    <div id=container>
      <div id=target>
      </div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById("container");
  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  ASSERT_TRUE(container);

  EXPECT_EQ("10px", ComputedValueString(target, "height"));
  EXPECT_EQ(0u, GetAnimationsCount(target));

  LogicalAxes contained_axes(kLogicalAxisInline);

  // Simulate a style and layout pass with multiple rounds of style recalc.
  {
    CSSAnimationUpdateScope animation_update_scope(GetDocument());

    // No transition property present. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(120, -1), contained_axes);
    EXPECT_EQ("20px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Still no transition property present. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(130, -1), contained_axes);
    EXPECT_EQ("30px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Now the transition property appears for the first time. (Final round).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "140px");
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ("25px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));
  }

  // CSSAnimationUpdateScope going out of scope applies the update.
  EXPECT_EQ(1u, GetAnimationsCount(target));

  // Verify that the newly-updated Animation produces the correct value.
  target->SetNeedsAnimationStyleRecalc();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("25px", ComputedValueString(target, "height"));
}

TEST_F(ContainerQueryTest, TransitionTemporarilyAppearing) {
  ScopedCSSDelayedAnimationUpdatesForTest scoped(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        container: inline-size;
        width: 20px;
      }
      #target {
        height: 10px;
      }
      @container size(width: 120px) {
        #target { height: 20px; }
      }
      @container size(width: 130px) {
        #target {
          height: 90px;
          transition: height steps(2, start) 100s;
        }
      }
      @container size(width: 140px) {
        #target { height: 40px; }
      }
    </style>
    <div id=container>
      <div id=target>
      </div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById("container");
  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  ASSERT_TRUE(container);

  EXPECT_EQ("10px", ComputedValueString(target, "height"));
  EXPECT_EQ(0u, GetAnimationsCount(target));

  LogicalAxes contained_axes(kLogicalAxisInline);

  // Simulate a style and layout pass with multiple rounds of style recalc.
  {
    CSSAnimationUpdateScope animation_update_scope(GetDocument());

    // No transition property present yet. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(120, -1), contained_axes);
    EXPECT_EQ("20px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Transition between [10px, 90px]. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(130, -1), contained_axes);
    EXPECT_EQ("50px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // The transition property disappeared again. (Final round).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "140px");
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ("40px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));
  }

  // CSSAnimationUpdateScope going out of scope applies the update.
  // We ultimately ended up with no transition, hence we should have no
  // Animations on the element.
  EXPECT_EQ(0u, GetAnimationsCount(target));
}

TEST_F(ContainerQueryTest, RedefiningAnimations) {
  ScopedCSSDelayedAnimationUpdatesForTest scoped(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { height: 0px; }
        to { height: 100px; }
      }
      #container {
        container: inline-size;
        width: 10px;
      }
      @container size(width: 120px) {
        #target {
          animation: anim 10s -2s linear paused;
        }
      }
      @container size(width: 130px) {
        #target {
          animation: anim 10s -3s linear paused;
        }
      }
      @container size(width: 140px) {
        #target {
          animation: anim 10s -4s linear paused;
        }
      }
    </style>
    <div id=container>
      <div id=target>
      </div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById("container");
  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  ASSERT_TRUE(container);

  EXPECT_EQ("auto", ComputedValueString(target, "height"));

  LogicalAxes contained_axes(kLogicalAxisInline);

  // Simulate a style and layout pass with multiple rounds of style recalc.
  {
    CSSAnimationUpdateScope animation_update_scope(GetDocument());

    // Animation at 20%. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(120, -1), contained_axes);
    EXPECT_EQ("20px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Animation at 30%. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(130, -1), contained_axes);
    EXPECT_EQ("30px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Animation at 40%. (Final round).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "140px");
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ("40px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));
  }

  // CSSAnimationUpdateScope going out of scope applies the update.
  EXPECT_EQ(1u, GetAnimationsCount(target));

  // Verify that the newly-updated Animation produces the correct value.
  target->SetNeedsAnimationStyleRecalc();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("40px", ComputedValueString(target, "height"));
}

TEST_F(ContainerQueryTest, UnsetAnimation) {
  ScopedCSSDelayedAnimationUpdatesForTest scoped(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { height: 0px; }
        to { height: 100px; }
      }
      #container {
        container: inline-size;
        width: 10px;
      }
      #target {
        animation: anim 10s -2s linear paused;
      }
      @container size(width: 130px) {
        #target {
          animation: unset;
        }
      }
    </style>
    <div id=container>
      <div id=target>
      </div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById("container");
  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  ASSERT_TRUE(container);

  EXPECT_EQ("20px", ComputedValueString(target, "height"));
  ASSERT_EQ(1u, target->getAnimations().size());
  Animation* animation_before = target->getAnimations()[0].Get();

  LogicalAxes contained_axes(kLogicalAxisInline);

  // Simulate a style and layout pass with multiple rounds of style recalc.
  {
    CSSAnimationUpdateScope animation_update_scope(GetDocument());

    // Animation should appear to be canceled. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(130, -1), contained_axes);
    EXPECT_EQ("auto", ComputedValueString(target, "height"));
    EXPECT_EQ(1u, GetAnimationsCount(target));

    // Animation should not be canceled after all. (Final round).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "140px");
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ("20px", ComputedValueString(target, "height"));
    EXPECT_EQ(1u, GetAnimationsCount(target));
  }

  // CSSAnimationUpdateScope going out of scope applies the update.
  // (Although since we didn't cancel, there is nothing to update).
  EXPECT_EQ(1u, GetAnimationsCount(target));

  // Verify that the same Animation object is still there.
  ASSERT_EQ(1u, target->getAnimations().size());
  EXPECT_EQ(animation_before, target->getAnimations()[0].Get());

  // Animation should not be canceled.
  EXPECT_TRUE(animation_before->CurrentTimeInternal());

  // Change width such that container query matches, and cancel the animation
  // for real this time. Note that since we no longer have a
  // CSSAnimationUpdateScope above us, the CSSAnimationUpdateScope within
  // UpdateAllLifecyclePhasesForTest will apply the update.
  container->SetInlineStyleProperty(CSSPropertyID::kWidth, "130px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("auto", ComputedValueString(target, "height"));

  // *Now* animation should be canceled.
  EXPECT_FALSE(animation_before->CurrentTimeInternal());
}

TEST_F(ContainerQueryTest, OldStylesCount) {
  ScopedCSSDelayedAnimationUpdatesForTest scoped(true);

  // No container, no animation properties.
  EXPECT_EQ(0u, GetOldStylesCount(R"HTML(
    <div></div>
    <div></div>
    <div></div>
    <div></div>
  )HTML"));

  // Animation properties, but no container.
  EXPECT_EQ(0u, GetOldStylesCount(R"HTML(
    <div style="animation: anim 1s linear"></div>
  )HTML"));

  // A container, but no animation properties.
  EXPECT_EQ(0u, GetOldStylesCount(R"HTML(
    <style>
      #container {
        container-type: inline-size;
      }
    </style>
    <div id=container>
      <div></div>
      <div></div>
    </div>
  )HTML"));

  // A container and a matching container query with no animations.
  EXPECT_EQ(0u, GetOldStylesCount(R"HTML(
    <style>
      #container {
        container-type: inline-size;
        width: 100px;
      }
      @container size(width: 100px) {
        #target {
          color: green;
        }
      }
    </style>
    <div id=container>
      <div id=target></div>
      <div></div>
    </div>
  )HTML"));

  // A container and a non-matching container query with no animations.
  EXPECT_EQ(0u, GetOldStylesCount(R"HTML(
    <style>
      #container {
        container-type: inline-size;
        width: 100px;
      }
      @container size(width: 200px) {
        #target {
          color: green;
        }
      }
    </style>
    <div id=container>
      <div id=target></div>
      <div></div>
    </div>
  )HTML"));

  // #target uses animations, and depends on container query.
  //
  // In theory we could understand that the animation is not inside an
  // @container rule, but we don't do this currently.
  EXPECT_EQ(1u, GetOldStylesCount(R"HTML(
    <style>
      #container {
        container-type: inline-size;
      }
      #target {
        animation: anim 1s linear;
      }
    </style>
    <div id=container>
      <div id=target></div>
      <div></div>
    </div>
  )HTML"));

  // #target uses animations in a matching container query.
  EXPECT_EQ(1u, GetOldStylesCount(R"HTML(
    <style>
      #container {
        width: 100px;
        container-type: inline-size;
      }
      @container size(width: 100px) {
        #target {
          animation: anim 1s linear;
        }
      }
    </style>
    <div id=container>
      <div id=target></div>
      <div></div>
    </div>
  )HTML"));

  // #target uses animations in a non-matching container query.
  EXPECT_EQ(1u, GetOldStylesCount(R"HTML(
    <style>
      #container {
        width: 100px;
        container-type: inline-size;
      }
      @container size(width: 200px) {
        #target {
          animation: anim 1s linear;
        }
      }
    </style>
    <div id=container>
      <div id=target></div>
      <div></div>
    </div>
  )HTML"));
}

TEST_F(ContainerQueryTest, AllAnimationAffectingPropertiesInConditional) {
  ScopedCSSDelayedAnimationUpdatesForTest scoped(true);

  CSSPropertyID animation_affecting[] = {
      CSSPropertyID::kAll,
      CSSPropertyID::kAnimation,
      CSSPropertyID::kAnimationDelay,
      CSSPropertyID::kAnimationDirection,
      CSSPropertyID::kAnimationDuration,
      CSSPropertyID::kAnimationFillMode,
      CSSPropertyID::kAnimationIterationCount,
      CSSPropertyID::kAnimationName,
      CSSPropertyID::kAnimationPlayState,
      CSSPropertyID::kAnimationTimeline,
      CSSPropertyID::kAnimationTimingFunction,
      CSSPropertyID::kTransition,
      CSSPropertyID::kTransitionDelay,
      CSSPropertyID::kTransitionDuration,
      CSSPropertyID::kTransitionProperty,
      CSSPropertyID::kTransitionTimingFunction,
  };

  CSSPropertyID non_animation_affecting_examples[] = {
      CSSPropertyID::kColor,
      CSSPropertyID::kTop,
      CSSPropertyID::kWidth,
  };

  // Generate a snippet which which specifies property:initial in a non-
  // matching media query.
  auto generate_html = [](const CSSProperty& property) -> String {
    StringBuilder builder;
    builder.Append("<style>");
    builder.Append("#container { container-type: inline-size; }");
    builder.Append("@container size(width: 100px) {");
    builder.Append("  #target {");
    builder.Append(String::Format(
        "%s:unset;", property.GetPropertyNameString().Utf8().c_str()));
    builder.Append("  }");
    builder.Append("}");
    builder.Append("</style>");
    builder.Append("<div id=container>");
    builder.Append("  <div id=target></div>");
    builder.Append("  <div></div>");
    builder.Append("</div>");
    return builder.ToString();
  };

  for (CSSPropertyID id : animation_affecting) {
    String html = generate_html(CSSProperty::Get(id));
    SCOPED_TRACE(testing::Message() << html);
    EXPECT_EQ(1u, GetOldStylesCount(html));
  }

  for (CSSPropertyID id : non_animation_affecting_examples) {
    String html = generate_html(CSSProperty::Get(id));
    SCOPED_TRACE(testing::Message() << html);
    EXPECT_EQ(0u, GetOldStylesCount(html));
  }
}

TEST_F(ContainerQueryTest, CQDependentContentVisibilityHidden) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container { container-type: inline-size }
      @container size(min-width: 200px) {
        .locked { content-visibility: hidden }
      }
    </style>
    <div id="ancestor" style="width: 100px">
      <div id="container">
        <div id="locker"></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* ancestor = GetDocument().getElementById("ancestor");
  ancestor->SetInlineStyleProperty(CSSPropertyID::kWidth, "200px");

  Element* locker = GetDocument().getElementById("locker");
  locker->setAttribute(html_names::kClassAttr, "locked");
  locker->setInnerHTML("<span>Visible?</span>");

  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(locker->GetDisplayLockContext());
  EXPECT_TRUE(locker->GetDisplayLockContext()->IsLocked());

  EXPECT_TRUE(locker->firstChild()->GetComputedStyle())
      << "The #locker element does not get content-visibility:hidden on the "
         "first pass over its children during the lifecycle update because we "
         "do not have the container laid out at that point. This is not a spec "
         "violation since it says the work _should_ be avoided. If this "
         "expectation changes because we are able to optimize this case, that "
         "is fine too.";
}

TEST_F(ContainerQueryTest, NoContainerQueryEvaluatorWhenDisabled) {
  ScopedCSSContainerQueriesForTest scope(false);

  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        container-type: size;
      }
      @container size(min-width: 200px) {
        span { color: pink; }
      }
    </style>
    <div id="container">
      <span></span>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(
      GetDocument().getElementById("container")->GetContainerQueryEvaluator());
}

}  // namespace blink
