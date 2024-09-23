// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"

#include <optional>

#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/post_style_update_scope.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

class ContainerQueryTest : public PageTestBase {
 public:
  bool HasUnknown(StyleRuleContainer* rule) {
    return rule && rule->GetContainerQuery().Query().HasUnknown();
  }

  enum class UnknownHandling {
    // No special handling of "unknown" values.
    kAllow,
    // Treats "unknown" values as parse errors.
    kError
  };

  StyleRuleContainer* ParseAtContainer(
      String rule_string,
      UnknownHandling unknown_handling = UnknownHandling::kError) {
    auto* rule = DynamicTo<StyleRuleContainer>(
        css_test_helpers::ParseRule(GetDocument(), rule_string));
    if ((unknown_handling == UnknownHandling::kError) && HasUnknown(rule)) {
      return nullptr;
    }
    return rule;
  }

  ContainerQuery* ParseContainerQuery(
      String query,
      UnknownHandling unknown_handling = UnknownHandling::kError) {
    String rule = "@container " + query + " {}";
    StyleRuleContainer* container = ParseAtContainer(rule, unknown_handling);
    if (!container) {
      return nullptr;
    }
    return &container->GetContainerQuery();
  }

  std::optional<MediaQueryExpNode::FeatureFlags> FeatureFlagsFrom(
      String query_string) {
    ContainerQuery* query =
        ParseContainerQuery(query_string, UnknownHandling::kAllow);
    if (!query) {
      return std::nullopt;
    }
    return GetInnerQuery(*query).CollectFeatureFlags();
  }

  ContainerSelector ContainerSelectorFrom(String query_string) {
    ContainerQuery* query =
        ParseContainerQuery(query_string, UnknownHandling::kAllow);
    if (!query) {
      return ContainerSelector();
    }
    return ContainerSelector(g_null_atom, GetInnerQuery(*query));
  }

  String SerializeCondition(StyleRuleContainer* container) {
    if (!container) {
      return "";
    }
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
        /* allow_visited_style */ false, CSSValuePhase::kComputedValue);
  }

  String ComputedValueString(Element* element, String property_name) {
    if (const CSSValue* value = ComputedValue(element, property_name)) {
      return value->CssText();
    }
    return g_null_atom;
  }

  // Get animations count for a specific element without force-updating
  // style and layout-tree.
  size_t GetAnimationsCount(Element* element) {
    if (auto* element_animations = element->GetElementAnimations()) {
      return element_animations->Animations().size();
    }
    return 0;
  }

  size_t GetOldStylesCount(String html) {
    // Creating a PostStyleUpdateScope prevents old styles from being cleared
    // until this function completes.
    PostStyleUpdateScope post_style_update_scope(GetDocument());
    SetBodyInnerHTML(html);
    DCHECK(PostStyleUpdateScope::CurrentAnimationData());
    size_t old_styles_count =
        PostStyleUpdateScope::CurrentAnimationData()->old_styles_.size();
    // We don't care about the effects of this Apply call, except that it
    // silences a DCHECK in ~PostStyleUpdateScope.
    post_style_update_scope.Apply();
    return old_styles_count;
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
  EXPECT_EQ("(not (max-width: 500px))",
            SerializeCondition(
                ParseAtContainer("@container (not (max-width: 500px)) {}")));
  EXPECT_EQ(
      "((max-width: 500px) and (max-height: 500px))",
      SerializeCondition(ParseAtContainer("@container ((max-width: 500px) "
                                          "and (max-height: 500px)) {}")));
  EXPECT_EQ(
      "((max-width: 500px) or (max-height: 500px))",
      SerializeCondition(ParseAtContainer("@container ((max-width: 500px) "
                                          "or (max-height: 500px)) {}")));
  EXPECT_EQ(
      "(width < 300px)",
      SerializeCondition(ParseAtContainer("@container (width < 300px) {}")));

  EXPECT_EQ("somename not (width)", SerializeCondition(ParseAtContainer(
                                        "@container somename not (width) {}")));

  EXPECT_EQ("(width) and (height)", SerializeCondition(ParseAtContainer(
                                        "@container (width) and (height) {}")));

  EXPECT_EQ("(width) or (height)", SerializeCondition(ParseAtContainer(
                                       "@container (width) or (height) {}")));

  EXPECT_EQ("test_name (width) or (height)",
            SerializeCondition(ParseAtContainer(
                "@container test_name (width) or (height) {}")));

  EXPECT_EQ("test_name ((max-width: 500px) or (max-height: 500px))",
            SerializeCondition(
                ParseAtContainer("@container test_name ((max-width: 500px) "
                                 "or (max-height: 500px)) {}")));

  // Invalid:
  EXPECT_FALSE(ParseAtContainer("@container test_name {}"));
  EXPECT_FALSE(ParseAtContainer("@container 100px {}"));
  EXPECT_FALSE(ParseAtContainer("@container calc(1) {}"));
  EXPECT_FALSE(ParseAtContainer("@container {}"));
  EXPECT_FALSE(ParseAtContainer("@container (min-width: 300px) nonsense {}"));
  EXPECT_FALSE(ParseAtContainer("@container size(width) {}"));
}

TEST_F(ContainerQueryTest, ValidFeatures) {
  // https://drafts.csswg.org/css-contain-3/#size-container
  EXPECT_TRUE(ParseAtContainer("@container (width) {}"));
  EXPECT_TRUE(ParseAtContainer("@container (min-width: 0px) {}"));
  EXPECT_TRUE(ParseAtContainer("@container (max-width: 0px) {}"));
  EXPECT_TRUE(ParseAtContainer("@container (height) {}"));
  EXPECT_TRUE(ParseAtContainer("@container (min-height: 0px) {}"));
  EXPECT_TRUE(ParseAtContainer("@container (max-height: 0px) {}"));
  EXPECT_TRUE(ParseAtContainer("@container (aspect-ratio) {}"));
  EXPECT_TRUE(ParseAtContainer("@container (min-aspect-ratio: 1/2) {}"));
  EXPECT_TRUE(ParseAtContainer("@container (max-aspect-ratio: 1/2) {}"));
  EXPECT_TRUE(ParseAtContainer("@container (orientation: portrait) {}"));
  EXPECT_TRUE(
      ParseAtContainer("@container test_name (orientation: portrait) {}"));

  EXPECT_FALSE(ParseAtContainer("@container (color) {}"));
  EXPECT_FALSE(ParseAtContainer("@container test_name (color) {}"));
  EXPECT_FALSE(ParseAtContainer("@container (color-index) {}"));
  EXPECT_FALSE(ParseAtContainer("@container (color-index >= 1) {}"));
  EXPECT_FALSE(ParseAtContainer("@container (grid) {}"));
  EXPECT_FALSE(ParseAtContainer("@container (resolution: 150dpi) {}"));
  EXPECT_FALSE(ParseAtContainer("@container (resolution: calc(6x / 3)) {}"));
  EXPECT_FALSE(ParseAtContainer("@container size(width) {}"));
  EXPECT_FALSE(ParseAtContainer("@container test_name size(width) {}"));
}

TEST_F(ContainerQueryTest, FeatureFlags) {
  EXPECT_EQ(MediaQueryExpNode::kFeatureUnknown,
            FeatureFlagsFrom("(width: 100gil)"));
  EXPECT_EQ(MediaQueryExpNode::kFeatureWidth,
            FeatureFlagsFrom("(width: 100px)"));
  EXPECT_EQ(MediaQueryExpNode::kFeatureWidth,
            FeatureFlagsFrom("test_name (width: 100px)"));
  EXPECT_EQ(MediaQueryExpNode::kFeatureHeight,
            FeatureFlagsFrom("(height < 100px)"));
  EXPECT_EQ(MediaQueryExpNode::kFeatureInlineSize,
            FeatureFlagsFrom("(100px >= inline-size)"));
  EXPECT_EQ(MediaQueryExpNode::kFeatureBlockSize,
            FeatureFlagsFrom("(100px = block-size)"));
  EXPECT_EQ(static_cast<MediaQueryExpNode::FeatureFlags>(
                MediaQueryExpNode::kFeatureWidth |
                MediaQueryExpNode::kFeatureBlockSize),
            FeatureFlagsFrom("((width) and (100px = block-size))"));
  EXPECT_EQ(static_cast<MediaQueryExpNode::FeatureFlags>(
                MediaQueryExpNode::kFeatureUnknown |
                MediaQueryExpNode::kFeatureBlockSize),
            FeatureFlagsFrom("((unknown) and (100px = block-size))"));
  EXPECT_EQ(
      static_cast<MediaQueryExpNode::FeatureFlags>(
          MediaQueryExpNode::kFeatureWidth | MediaQueryExpNode::kFeatureHeight |
          MediaQueryExpNode::kFeatureInlineSize),
      FeatureFlagsFrom("((width) or (height) or (inline-size))"));
  EXPECT_EQ(MediaQueryExpNode::kFeatureWidth,
            FeatureFlagsFrom("((width: 100px))"));
  EXPECT_EQ(MediaQueryExpNode::kFeatureWidth,
            FeatureFlagsFrom("(not (width: 100px))"));
}

TEST_F(ContainerQueryTest, ImplicitContainerSelector) {
  ContainerSelector width = ContainerSelectorFrom("(width: 100px)");
  EXPECT_EQ(kContainerTypeInlineSize, width.Type(WritingMode::kHorizontalTb));
  EXPECT_EQ(kContainerTypeBlockSize, width.Type(WritingMode::kVerticalRl));

  ContainerSelector height = ContainerSelectorFrom("(height: 100px)");
  EXPECT_EQ(kContainerTypeBlockSize, height.Type(WritingMode::kHorizontalTb));
  EXPECT_EQ(kContainerTypeInlineSize, height.Type(WritingMode::kVerticalRl));

  ContainerSelector inline_size = ContainerSelectorFrom("(inline-size: 100px)");
  EXPECT_EQ(kContainerTypeInlineSize,
            inline_size.Type(WritingMode::kHorizontalTb));
  EXPECT_EQ(kContainerTypeInlineSize,
            inline_size.Type(WritingMode::kVerticalRl));

  ContainerSelector block_size = ContainerSelectorFrom("(block-size: 100px)");
  EXPECT_EQ(kContainerTypeBlockSize,
            block_size.Type(WritingMode::kHorizontalTb));
  EXPECT_EQ(kContainerTypeBlockSize, block_size.Type(WritingMode::kVerticalRl));

  ContainerSelector width_height =
      ContainerSelectorFrom("((width: 100px) or (height: 100px))");
  EXPECT_EQ((kContainerTypeInlineSize | kContainerTypeBlockSize),
            width_height.Type(WritingMode::kHorizontalTb));
  EXPECT_EQ((kContainerTypeInlineSize | kContainerTypeBlockSize),
            width_height.Type(WritingMode::kVerticalRl));

  ContainerSelector inline_block_size =
      ContainerSelectorFrom("((inline-size: 100px) or (block-size: 100px))");
  EXPECT_EQ((kContainerTypeInlineSize | kContainerTypeBlockSize),
            inline_block_size.Type(WritingMode::kHorizontalTb));
  EXPECT_EQ((kContainerTypeInlineSize | kContainerTypeBlockSize),
            inline_block_size.Type(WritingMode::kVerticalRl));

  ContainerSelector aspect_ratio = ContainerSelectorFrom("(aspect-ratio: 1/2)");
  EXPECT_EQ((kContainerTypeInlineSize | kContainerTypeBlockSize),
            aspect_ratio.Type(WritingMode::kHorizontalTb));
  EXPECT_EQ((kContainerTypeInlineSize | kContainerTypeBlockSize),
            aspect_ratio.Type(WritingMode::kVerticalRl));

  ContainerSelector orientation =
      ContainerSelectorFrom("(orientation: portrait)");
  EXPECT_EQ((kContainerTypeInlineSize | kContainerTypeBlockSize),
            orientation.Type(WritingMode::kHorizontalTb));
  EXPECT_EQ((kContainerTypeInlineSize | kContainerTypeBlockSize),
            orientation.Type(WritingMode::kVerticalRl));
}

TEST_F(ContainerQueryTest, ScrollStateContainerSelector) {
  ContainerSelector stuck_right =
      ContainerSelectorFrom("scroll-state(stuck: right)");
  EXPECT_EQ(kContainerTypeScrollState,
            stuck_right.Type(WritingMode::kHorizontalTb));

  ContainerSelector stuck_and_style =
      ContainerSelectorFrom("scroll-state(stuck: right) and style(--foo: bar)");
  EXPECT_EQ(kContainerTypeScrollState,
            stuck_and_style.Type(WritingMode::kHorizontalTb));

  ContainerSelector stuck_and_inline_size = ContainerSelectorFrom(
      "scroll-state(stuck: block-end) or (inline-size > 10px)");
  EXPECT_EQ((kContainerTypeScrollState | kContainerTypeInlineSize),
            stuck_and_inline_size.Type(WritingMode::kHorizontalTb));

  ContainerSelector stuck_and_block_size =
      ContainerSelectorFrom("scroll-state(stuck: block-end) and (height)");
  EXPECT_EQ((kContainerTypeScrollState | kContainerTypeBlockSize),
            stuck_and_block_size.Type(WritingMode::kHorizontalTb));
}

TEST_F(ContainerQueryTest, RuleParsing) {
  StyleRuleContainer* container = ParseAtContainer(R"CSS(
    @container test_name (min-width: 100px) {
      div { width: 100px; }
      span { height: 100px; }
    }
  )CSS");
  ASSERT_TRUE(container);
  ASSERT_EQ("test_name", container->GetContainerQuery().Selector().Name());

  CSSStyleSheet* sheet = css_test_helpers::CreateStyleSheet(GetDocument());
  auto* rule = DynamicTo<CSSContainerRule>(
      container->CreateCSSOMWrapper(/*position_hint=*/0, sheet));
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
  auto& rules = container->ChildRules();
  auto& rules_copy = copy->ChildRules();
  ASSERT_EQ(1u, rules.size());
  ASSERT_EQ(1u, rules_copy.size());
  EXPECT_NE(rules[0], rules_copy[0]);

  // The ContainerQuery should be copied.
  EXPECT_NE(&container->GetContainerQuery(), &copy->GetContainerQuery());

  // The inner MediaQueryExpNode is immutable, and does not need to be copied.
  EXPECT_EQ(&GetInnerQuery(container->GetContainerQuery()),
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
  Element* div = GetDocument().getElementById(AtomicString("div"));
  ASSERT_TRUE(div);
  EXPECT_EQ(2, div->ComputedStyleRef().ZIndex());

  // Check that dependent elements are responsive to changes:
  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_TRUE(container);
  container->setAttribute(html_names::kClassAttr, AtomicString("adjust"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(3, div->ComputedStyleRef().ZIndex());

  container->setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2, div->ComputedStyleRef().ZIndex());
}

TEST_F(ContainerQueryTest, QueryZoom) {
  GetFrame().SetLayoutZoomFactor(2.0f);

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

  Element* target1 = GetDocument().getElementById(AtomicString("target1"));
  Element* target2 = GetDocument().getElementById(AtomicString("target2"));
  ASSERT_TRUE(target1);
  ASSERT_TRUE(target2);

  EXPECT_TRUE(
      target1->ComputedStyleRef().GetVariableData(AtomicString("--w100")));
  EXPECT_TRUE(
      target1->ComputedStyleRef().GetVariableData(AtomicString("--h200")));
  EXPECT_FALSE(
      target1->ComputedStyleRef().GetVariableData(AtomicString("--w200")));
  EXPECT_FALSE(
      target1->ComputedStyleRef().GetVariableData(AtomicString("--h400")));

  EXPECT_FALSE(
      target2->ComputedStyleRef().GetVariableData(AtomicString("--w100")));
  EXPECT_FALSE(
      target2->ComputedStyleRef().GetVariableData(AtomicString("--h200")));
  EXPECT_TRUE(
      target2->ComputedStyleRef().GetVariableData(AtomicString("--w200")));
  EXPECT_TRUE(
      target2->ComputedStyleRef().GetVariableData(AtomicString("--h400")));
}

TEST_F(ContainerQueryTest, QueryFontRelativeWithZoom) {
  GetFrame().SetLayoutZoomFactor(2.0f);

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
      @container (width: 10em) {
        #em-target { --em:1; }
      }
      @container (width: 10ex) {
        #ex-target { --ex:1; }
      }
      @container (width: 10ch) {
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

  Element* em_target = GetDocument().getElementById(AtomicString("em-target"));
  Element* ex_target = GetDocument().getElementById(AtomicString("ex-target"));
  Element* ch_target = GetDocument().getElementById(AtomicString("ch-target"));
  ASSERT_TRUE(em_target);
  ASSERT_TRUE(ex_target);
  ASSERT_TRUE(ch_target);

  EXPECT_TRUE(
      em_target->ComputedStyleRef().GetVariableData(AtomicString("--em")));
  EXPECT_TRUE(
      ex_target->ComputedStyleRef().GetVariableData(AtomicString("--ex")));
  EXPECT_TRUE(
      ch_target->ComputedStyleRef().GetVariableData(AtomicString("--ch")));
}

TEST_F(ContainerQueryTest, ContainerUnitsViewportFallback) {
  using css_test_helpers::RegisterProperty;

  RegisterProperty(GetDocument(), "--cqw", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--cqi", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--cqh", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--cqb", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--cqmin", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--cqmax", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--fallback-h", "<length>", "0px", false);
  RegisterProperty(GetDocument(), "--fallback-min-cqi-vh", "<length>", "0px",
                   false);
  RegisterProperty(GetDocument(), "--fallback-max-cqi-vh", "<length>", "0px",
                   false);

  SetBodyInnerHTML(R"HTML(
    <style>
      #inline, #size {
        width: 100px;
        height: 100px;
      }
      #inline {
        container-type: inline-size;
      }
      #size {
        container-type: size;
      }
      #inline_target, #size_target {
        --cqw: 10cqw;
        --cqi: 10cqi;
        --cqh: 10cqh;
        --cqb: 10cqb;
        --cqmin: 10cqmin;
        --cqmax: 10cqmax;
        --fallback-h: 10vh;
        --fallback-min-cqi-vh: min(10cqi, 10vh);
        --fallback-max-cqi-vh: max(10cqi, 10vh);
      }
    </style>
    <div id=inline>
      <div id="inline_target"></div>
    </div>
    <div id=size>
      <div id="size_target"></div>
    </div>
  )HTML");

  Element* inline_target =
      GetDocument().getElementById(AtomicString("inline_target"));
  ASSERT_TRUE(inline_target);
  EXPECT_EQ(ComputedValueString(inline_target, "--cqw"), "10px");
  EXPECT_EQ(ComputedValueString(inline_target, "--cqi"), "10px");
  EXPECT_EQ(ComputedValueString(inline_target, "--cqh"),
            ComputedValueString(inline_target, "--fallback-h"));
  EXPECT_EQ(ComputedValueString(inline_target, "--cqb"),
            ComputedValueString(inline_target, "--fallback-h"));
  EXPECT_EQ(ComputedValueString(inline_target, "--cqmin"),
            ComputedValueString(inline_target, "--fallback-min-cqi-vh"));
  EXPECT_EQ(ComputedValueString(inline_target, "--cqmax"),
            ComputedValueString(inline_target, "--fallback-max-cqi-vh"));

  Element* size_target =
      GetDocument().getElementById(AtomicString("size_target"));
  ASSERT_TRUE(size_target);
  EXPECT_EQ(ComputedValueString(size_target, "--cqw"), "10px");
  EXPECT_EQ(ComputedValueString(size_target, "--cqi"), "10px");
  EXPECT_EQ(ComputedValueString(size_target, "--cqh"), "10px");
  EXPECT_EQ(ComputedValueString(size_target, "--cqb"), "10px");
  EXPECT_EQ(ComputedValueString(size_target, "--cqmin"), "10px");
  EXPECT_EQ(ComputedValueString(size_target, "--cqmax"), "10px");
}

TEST_F(ContainerQueryTest, OldStyleForTransitions) {
  Element* target = nullptr;

  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        container-type: inline-size;
        width: 20px;
      }
      #target {
        height: 10px;
        transition: height steps(2, start) 100s;
      }
      @container (width: 120px) {
        #target { height: 20px; }
      }
      @container (width: 130px) {
        #target { height: 30px; }
      }
      @container (width: 140px) {
        #target { height: 40px; }
      }
    </style>
    <div id=container>
      <div id=target>
      </div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById(AtomicString("container"));
  target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  ASSERT_TRUE(container);

  EXPECT_EQ("10px", ComputedValueString(target, "height"));
  EXPECT_EQ(0u, GetAnimationsCount(target));

  // Simulate a style and layout pass with multiple rounds of style recalc.
  {
    PostStyleUpdateScope post_style_update_scope(GetDocument());

    // Should transition between [10px, 20px]. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(120, -1), kLogicalAxesInline);
    EXPECT_EQ("15px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Should transition between [10px, 30px]. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(130, -1), kLogicalAxesInline);
    EXPECT_EQ("20px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Should transition between [10px, 40px]. (Final round).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "140px");
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ("25px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    EXPECT_FALSE(post_style_update_scope.Apply());
  }

  // Animation count should be updated after PostStyleUpdateScope::Apply.
  EXPECT_EQ(1u, GetAnimationsCount(target));

  // Verify that the newly-updated Animation produces the correct value.
  target->SetNeedsAnimationStyleRecalc();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("25px", ComputedValueString(target, "height"));
}

TEST_F(ContainerQueryTest, TransitionAppearingInFinalPass) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        container-type: inline-size;
        width: 20px;
      }
      #target {
        height: 10px;
      }
      @container (width: 120px) {
        #target { height: 20px; }
      }
      @container (width: 130px) {
        #target { height: 30px; }
      }
      @container (width: 140px) {
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

  Element* container = GetDocument().getElementById(AtomicString("container"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  ASSERT_TRUE(container);

  EXPECT_EQ("10px", ComputedValueString(target, "height"));
  EXPECT_EQ(0u, GetAnimationsCount(target));

  // Simulate a style and layout pass with multiple rounds of style recalc.
  {
    PostStyleUpdateScope post_style_update_scope(GetDocument());

    // No transition property present. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(120, -1), kLogicalAxesInline);
    EXPECT_EQ("20px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Still no transition property present. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(130, -1), kLogicalAxesInline);
    EXPECT_EQ("30px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Now the transition property appears for the first time. (Final round).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "140px");
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ("25px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    EXPECT_FALSE(post_style_update_scope.Apply());
  }

  // Animation count should be updated after PostStyleUpdateScope::Apply.
  EXPECT_EQ(1u, GetAnimationsCount(target));

  // Verify that the newly-updated Animation produces the correct value.
  target->SetNeedsAnimationStyleRecalc();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("25px", ComputedValueString(target, "height"));
}

TEST_F(ContainerQueryTest, TransitionTemporarilyAppearing) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        container-type: inline-size;
        width: 20px;
      }
      #target {
        height: 10px;
      }
      @container (width: 120px) {
        #target { height: 20px; }
      }
      @container (width: 130px) {
        #target {
          height: 90px;
          transition: height steps(2, start) 100s;
        }
      }
      @container (width: 140px) {
        #target { height: 40px; }
      }
    </style>
    <div id=container>
      <div id=target>
      </div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById(AtomicString("container"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  ASSERT_TRUE(container);

  EXPECT_EQ("10px", ComputedValueString(target, "height"));
  EXPECT_EQ(0u, GetAnimationsCount(target));

  // Simulate a style and layout pass with multiple rounds of style recalc.
  {
    PostStyleUpdateScope post_style_update_scope(GetDocument());

    // No transition property present yet. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(120, -1), kLogicalAxesInline);
    EXPECT_EQ("20px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Transition between [10px, 90px]. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(130, -1), kLogicalAxesInline);
    EXPECT_EQ("50px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // The transition property disappeared again. (Final round).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "140px");
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ("40px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    EXPECT_FALSE(post_style_update_scope.Apply());
  }

  // Animation count should be updated after PostStyleUpdateScope::Apply.
  // We ultimately ended up with no transition, hence we should have no
  // Animations on the element.
  EXPECT_EQ(0u, GetAnimationsCount(target));
}

TEST_F(ContainerQueryTest, RedefiningAnimations) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { height: 0px; }
        to { height: 100px; }
      }
      #container {
        container-type: inline-size;
        width: 10px;
      }
      @container (width: 120px) {
        #target {
          animation: anim 10s -2s linear paused;
        }
      }
      @container (width: 130px) {
        #target {
          animation: anim 10s -3s linear paused;
        }
      }
      @container (width: 140px) {
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

  Element* container = GetDocument().getElementById(AtomicString("container"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  ASSERT_TRUE(container);

  EXPECT_EQ("auto", ComputedValueString(target, "height"));

  // Simulate a style and layout pass with multiple rounds of style recalc.
  {
    PostStyleUpdateScope post_style_update_scope(GetDocument());

    // Animation at 20%. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(120, -1), kLogicalAxesInline);
    EXPECT_EQ("20px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Animation at 30%. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(130, -1), kLogicalAxesInline);
    EXPECT_EQ("30px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    // Animation at 40%. (Final round).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "140px");
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ("40px", ComputedValueString(target, "height"));
    EXPECT_EQ(0u, GetAnimationsCount(target));

    EXPECT_FALSE(post_style_update_scope.Apply());
  }

  // Animation count should be updated after PostStyleUpdateScope::Apply.
  EXPECT_EQ(1u, GetAnimationsCount(target));

  // Verify that the newly-updated Animation produces the correct value.
  target->SetNeedsAnimationStyleRecalc();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("40px", ComputedValueString(target, "height"));
}

TEST_F(ContainerQueryTest, UnsetAnimation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { height: 0px; }
        to { height: 100px; }
      }
      #container {
        container-type: inline-size;
        width: 10px;
      }
      #target {
        animation: anim 10s -2s linear paused;
      }
      @container (width: 130px) {
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

  Element* container = GetDocument().getElementById(AtomicString("container"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  ASSERT_TRUE(container);

  EXPECT_EQ("20px", ComputedValueString(target, "height"));
  ASSERT_EQ(1u, target->getAnimations().size());
  Animation* animation_before = target->getAnimations()[0].Get();

  // Simulate a style and layout pass with multiple rounds of style recalc.
  {
    PostStyleUpdateScope post_style_update_scope(GetDocument());

    // Animation should appear to be canceled. (Intermediate round).
    GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
        *container, LogicalSize(130, -1), kLogicalAxesInline);
    EXPECT_EQ("auto", ComputedValueString(target, "height"));
    EXPECT_EQ(1u, GetAnimationsCount(target));

    // Animation should not be canceled after all. (Final round).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "140px");
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ("20px", ComputedValueString(target, "height"));
    EXPECT_EQ(1u, GetAnimationsCount(target));

    EXPECT_FALSE(post_style_update_scope.Apply());
  }

  // Animation count should be updated after PostStyleUpdateScope::Apply.
  // (Although since we didn't cancel, there is nothing to update).
  EXPECT_EQ(1u, GetAnimationsCount(target));

  // Verify that the same Animation object is still there.
  ASSERT_EQ(1u, target->getAnimations().size());
  EXPECT_EQ(animation_before, target->getAnimations()[0].Get());

  // Animation should not be canceled.
  EXPECT_TRUE(animation_before->CurrentTimeInternal());

  // Change width such that container query matches, and cancel the animation
  // for real this time. Note that since we no longer have a
  // PostStyleUpdateScope above us, the PostStyleUpdateScope within
  // UpdateAllLifecyclePhasesForTest will apply the update.
  container->SetInlineStyleProperty(CSSPropertyID::kWidth, "130px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("auto", ComputedValueString(target, "height"));

  // *Now* animation should be canceled.
  EXPECT_FALSE(animation_before->CurrentTimeInternal());
}

TEST_F(ContainerQueryTest, OldStylesCount) {
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
      @container (width: 100px) {
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
      @container (width: 200px) {
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
      @container (width: 100px) {
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
      @container (width: 200px) {
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
    builder.Append("@container (width: 100px) {");
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
      @container (min-width: 200px) {
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

  Element* ancestor = GetDocument().getElementById(AtomicString("ancestor"));
  ancestor->SetInlineStyleProperty(CSSPropertyID::kWidth, "200px");

  Element* locker = GetDocument().getElementById(AtomicString("locker"));
  locker->setAttribute(html_names::kClassAttr, AtomicString("locked"));
  locker->setInnerHTML("<span>Visible?</span>");

  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(locker->GetDisplayLockContext());
  EXPECT_TRUE(locker->GetDisplayLockContext()->IsLocked());

  EXPECT_TRUE(locker->firstElementChild()->GetComputedStyle())
      << "The #locker element does not get content-visibility:hidden on the "
         "first pass over its children during the lifecycle update because we "
         "do not have the container laid out at that point. This is not a spec "
         "violation since it says the work _should_ be avoided. If this "
         "expectation changes because we are able to optimize this case, that "
         "is fine too.";
}

TEST_F(ContainerQueryTest, QueryViewportDependency) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        container-type: size;
      }
      @container (width: 200px) {
        #target1 { color: pink; }
      }
      @container (width: 100vw) {
        #target2 { color: pink; }
      }
      @container (width: 100svw) {
        #target3 { color: pink; }
      }
      @container (width: 100dvw) {
        #target4 { color: pink; }
      }
    </style>
    <div id="container">
      <span id=target1></span>
      <span id=target2></span>
      <span id=target3></span>
      <span id=target4></span>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* target1 = GetDocument().getElementById(AtomicString("target1"));
  Element* target2 = GetDocument().getElementById(AtomicString("target2"));
  Element* target3 = GetDocument().getElementById(AtomicString("target3"));
  Element* target4 = GetDocument().getElementById(AtomicString("target4"));

  ASSERT_TRUE(target1);
  ASSERT_TRUE(target2);
  ASSERT_TRUE(target3);
  ASSERT_TRUE(target4);

  EXPECT_FALSE(target1->ComputedStyleRef().HasStaticViewportUnits());
  EXPECT_FALSE(target1->ComputedStyleRef().HasDynamicViewportUnits());

  EXPECT_TRUE(target2->ComputedStyleRef().HasStaticViewportUnits());
  EXPECT_FALSE(target2->ComputedStyleRef().HasDynamicViewportUnits());

  EXPECT_TRUE(target3->ComputedStyleRef().HasStaticViewportUnits());
  EXPECT_FALSE(target3->ComputedStyleRef().HasDynamicViewportUnits());

  EXPECT_FALSE(target4->ComputedStyleRef().HasStaticViewportUnits());
  EXPECT_TRUE(target4->ComputedStyleRef().HasDynamicViewportUnits());
}

TEST_F(ContainerQueryTest, TreeScopedReferenceUserOrigin) {
  StyleSheetKey user_sheet_key("user_sheet");
  auto* parsed_user_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  parsed_user_sheet->ParseString(R"HTML(
      @container author-container (width >= 0) {
        div > span {
          z-index: 13;
        }
      }
      .user_container {
        container: user-container / inline-size;
      }
  )HTML");
  GetStyleEngine().InjectSheet(user_sheet_key, parsed_user_sheet,
                               WebCssOrigin::kUser);

  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <style>
      @container user-container (width >= 0) {
        div > span {
          z-index: 17;
        }
      }
      .author_container {
        container: author-container / inline-size;
      }
    </style>
    <div class="author_container">
      <span id="author_target"></span>
    </div>
    <div class="user_container">
      <span id="user_target"></span>
    </div>
    <div id="host">
      <template shadowrootmode="open">
        <style>
          @container user-container (width >= 0) {
            div > span {
              z-index: 29;
            }
          }
          .author_container {
            container: author-container / inline-size;
          }
        </style>
        <div class="author_container">
          <span id="shadow_author_target"></span>
        </div>
        <div class="user_container">
          <span id="shadow_user_target"></span>
        </div>
      </template>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* author_target = GetElementById("author_target");
  Element* user_target = GetElementById("user_target");
  ShadowRoot* shadow_root = GetElementById("host")->GetShadowRoot();
  Element* shadow_author_target =
      shadow_root->getElementById(AtomicString("shadow_author_target"));
  Element* shadow_user_target =
      shadow_root->getElementById(AtomicString("shadow_user_target"));

  EXPECT_EQ(author_target->ComputedStyleRef().ZIndex(), 13);
  EXPECT_EQ(shadow_author_target->ComputedStyleRef().ZIndex(), 13);
  EXPECT_EQ(user_target->ComputedStyleRef().ZIndex(), 17);
  EXPECT_EQ(shadow_user_target->ComputedStyleRef().ZIndex(), 29);
}

}  // namespace blink
