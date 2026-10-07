// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_element.h"

#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/svg/svg_element_rare_data.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

class SVGTestEventListener : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override {}
};

}  // namespace

class SVGElementTest : public PageTestBase {};

class SVGIncrementalStyleTest : public PageTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  SVGIncrementalStyleTest() : incremental_style_(GetParam()) {}

 private:
  ScopedSvgIncrementalStyleForTest incremental_style_;
};

INSTANTIATE_TEST_SUITE_P(All, SVGIncrementalStyleTest, testing::Bool());

TEST_P(SVGIncrementalStyleTest, IncrementalStyleInternals) {
  SetBodyInnerHTML(R"HTML(
    <svg><rect id="rect" x="10" style="opacity: 0.5"/></svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* rect = GetElementById("rect");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  style_engine.SetStatsEnabled(true);

  ASSERT_TRUE(rect->GetComputedStyle());
  EXPECT_EQ(0.5f, rect->GetComputedStyle()->Opacity());

  ClassicScript::CreateUnspecifiedScript(
      "document.getElementById('rect').style.opacity = '0.75';")
      ->RunScript(GetDocument().domWindow());
  EXPECT_EQ(kIndependentStyleChange, rect->GetStyleChangeType());
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  style_engine.RecalcStyle();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  ASSERT_TRUE(rect->GetComputedStyle());
  EXPECT_EQ(0.75f, rect->GetComputedStyle()->Opacity());
  ASSERT_TRUE(style_engine.Stats());
  EXPECT_EQ(GetParam() ? 1u : 0u,
            style_engine.Stats()->svg_presentation_attribute_styles_used);

  style_engine.Stats()->svg_presentation_attribute_styles_used = 0;
  ClassicScript::CreateUnspecifiedScript(
      "document.getElementById('rect').setAttribute('x', '30');")
      ->RunScript(GetDocument().domWindow());
  EXPECT_EQ(GetParam() ? kIndependentStyleChange : kLocalStyleChange,
            rect->GetStyleChangeType());
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  style_engine.RecalcStyle();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  ASSERT_TRUE(rect->GetComputedStyle());
  EXPECT_EQ(Length::Fixed(30), rect->GetComputedStyle()->X());
  EXPECT_EQ(GetParam() ? 1u : 0u,
            style_engine.Stats()->svg_presentation_attribute_styles_used);
}

TEST_P(SVGIncrementalStyleTest, IncrementalStyleWithAdjustedInputs) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #position { position: absolute; }
      #text, #foreign { display: inline-block; }
      .appearance { appearance: button; }
      #unchanged { display: block; }
      .relative { position: relative; }
      .relative_zindex { position: relative; z-index: 1; }
      #sticky { position: sticky; }
      #static_zindex { z-index: 1; }
      #outer_relative { position: relative; }
    </style>
    <svg><g style="text-decoration: underline">
      <rect id="position" opacity="0.5" style="opacity: 0.5"/>
      <text id="text" opacity="0.5" style="opacity: 0.5">Text</text>
      <foreignObject id="foreign" opacity="0.5" style="opacity: 0.5"/>
      <rect id="appearance" class="appearance"
            opacity="0.5" style="opacity: 0.5"/>
      <rect id="unchanged" opacity="0.5" style="opacity: 0.5"/>
      <rect id="default" opacity="0.5" style="opacity: 0.5"/>
      <rect id="relative" class="relative"
            opacity="0.5" style="opacity: 0.5"/>
      <rect id="relative_cached" class="relative"
            opacity="0.5" style="opacity: 0.5"/>
      <rect id="relative_zindex" class="relative_zindex"
            opacity="0.5" style="opacity: 0.5"/>
      <rect id="relative_zindex_cached" class="relative_zindex"
            opacity="0.5" style="opacity: 0.5"/>
      <rect id="sticky" opacity="0.5" style="opacity: 0.5"/>
      <rect id="static_zindex" opacity="0.5" style="opacity: 0.5"/>
    </g></svg>
    <svg id="outer" opacity="0.5" style="opacity: 0.5"></svg>
    <svg id="outer_relative" opacity="0.5" style="opacity: 0.5"></svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  style_engine.SetStatsEnabled(true);
  ASSERT_TRUE(style_engine.Stats());

  const struct {
    const char* id;
    bool incremental;
  } cases[] = {{"position", true},        {"text", true},
               {"foreign", true},         {"appearance", false},
               {"unchanged", true},       {"default", true},
               {"relative", true},        {"relative_cached", true},
               {"relative_zindex", true}, {"relative_zindex_cached", true},
               {"sticky", true},          {"static_zindex", true},
               {"outer", true},           {"outer_relative", true}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.id);
    Element* element = GetElementById(test.id);
    const ComputedStyle* original = element->GetComputedStyle();
    ASSERT_TRUE(original);
    EXPECT_FALSE(original->HasIncrementalStyleBlocker());
    for (float opacity : {0.75f, 0.25f}) {
      style_engine.Stats()->svg_presentation_attribute_styles_used = 0;
      element->style()->setProperty(GetDocument().GetExecutionContext(),
                                    "opacity", String::Number(opacity), "",
                                    ASSERT_NO_EXCEPTION);
      EXPECT_EQ(kIndependentStyleChange, element->GetStyleChangeType());
      GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
      style_engine.RecalcStyle();
      GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

      const ComputedStyle* updated = element->GetComputedStyle();
      ASSERT_TRUE(updated);
      EXPECT_EQ(opacity, updated->Opacity());
      EXPECT_EQ(original->Display(), updated->Display());
      EXPECT_EQ(original->BaseTextDecorationData(),
                updated->BaseTextDecorationData());
      ComputedStyleBuilder expected(*original);
      expected.SetOpacity(opacity);
      EXPECT_TRUE(*expected.TakeStyle() == *updated);
      EXPECT_EQ(GetParam() && test.incremental ? 1u : 0u,
                style_engine.Stats()->svg_presentation_attribute_styles_used);
    }
  }
}

TEST_P(SVGIncrementalStyleTest, IncrementalStyleCombinedMutation) {
  SetBodyInnerHTML(R"HTML(
    <svg><rect id="rect" x="10" style="opacity: 0.5"/></svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* rect = GetElementById("rect");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  style_engine.SetStatsEnabled(true);

  ASSERT_TRUE(rect->GetComputedStyle());
  EXPECT_EQ(0.5f, rect->GetComputedStyle()->Opacity());
  EXPECT_EQ(Length::Fixed(10), rect->GetComputedStyle()->X());

  ClassicScript::CreateUnspecifiedScript(R"JS(
    const rect = document.getElementById('rect');
    rect.style.opacity = '0.75';
    rect.setAttribute('x', '30');
  )JS")
      ->RunScript(GetDocument().domWindow());
  EXPECT_EQ(GetParam() ? kIndependentStyleChange : kLocalStyleChange,
            rect->GetStyleChangeType());
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  style_engine.RecalcStyle();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  ASSERT_TRUE(rect->GetComputedStyle());
  EXPECT_EQ(0.75f, rect->GetComputedStyle()->Opacity());
  EXPECT_EQ(Length::Fixed(30), rect->GetComputedStyle()->X());
  ASSERT_TRUE(style_engine.Stats());
  EXPECT_EQ(GetParam() ? 1u : 0u,
            style_engine.Stats()->svg_presentation_attribute_styles_used);
}

TEST_P(SVGIncrementalStyleTest, IncrementalStyleInlineStyleBeatsHint) {
  SetBodyInnerHTML(R"HTML(
    <svg><rect id="rect" x="10" style="x: 20px"/></svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* rect = GetElementById("rect");
  ASSERT_TRUE(rect->GetComputedStyle());
  ASSERT_EQ(Length::Fixed(20), rect->GetComputedStyle()->X());
  EXPECT_FALSE(rect->GetComputedStyle()->HasIncrementalStyleBlocker());
  GetDocument().GetSettings()->SetScriptEnabled(true);
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  style_engine.SetStatsEnabled(true);

  const struct {
    const char* script;
    int expected_x;
  } cases[] = {
      {"document.getElementById('rect').setAttribute('x', '30');", 20},
      {R"JS(
        document.getElementById('rect').style.x = '40px';
        document.getElementById('rect').setAttribute('x', '50');
      )JS",
       40},
      {R"JS(
        document.getElementById('rect').setAttribute('x', '60');
        document.getElementById('rect').style.x = '50px';
      )JS",
       50},
  };
  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.script);
    style_engine.Stats()->svg_presentation_attribute_styles_used = 0;
    ClassicScript::CreateUnspecifiedScript(test_case.script)
        ->RunScript(GetDocument().domWindow());
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
    style_engine.RecalcStyle();
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

    ASSERT_TRUE(rect->GetComputedStyle());
    EXPECT_EQ(Length::Fixed(test_case.expected_x),
              rect->GetComputedStyle()->X());
    EXPECT_FALSE(rect->GetComputedStyle()->HasIncrementalStyleBlocker());
    EXPECT_EQ(GetParam() ? 1u : 0u,
              style_engine.Stats()->svg_presentation_attribute_styles_used);
  }
}

TEST_P(SVGIncrementalStyleTest, IncrementalStyleHintLosingToAuthorRule) {
  SetBodyInnerHTML(R"HTML(
    <style>#rect { x: 30px }</style>
    <svg><rect id="rect" x="10"/></svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* rect = GetElementById("rect");
  ASSERT_TRUE(rect->GetComputedStyle());
  ASSERT_EQ(Length::Fixed(30), rect->GetComputedStyle()->X());
  EXPECT_EQ(GetParam(), rect->GetComputedStyle()->HasIncrementalStyleBlocker());
  GetDocument().GetSettings()->SetScriptEnabled(true);
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  style_engine.SetStatsEnabled(true);

  ClassicScript::CreateUnspecifiedScript(
      "document.getElementById('rect').setAttribute('x', '40');")
      ->RunScript(GetDocument().domWindow());
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  style_engine.RecalcStyle();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  // The author rule must keep winning; incremental styling must not overwrite
  // it.
  ASSERT_TRUE(rect->GetComputedStyle());
  EXPECT_EQ(Length::Fixed(30), rect->GetComputedStyle()->X());
  ASSERT_TRUE(style_engine.Stats());
  EXPECT_EQ(0u, style_engine.Stats()->svg_presentation_attribute_styles_used);
}

TEST_P(SVGIncrementalStyleTest, IncrementalStyleSkipsUnsupportedHint) {
  SetBodyInnerHTML(R"HTML(
    <style>rect { fill: green }</style>
    <svg><rect id="rect" x="10" fill="red"/></svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* rect = GetElementById("rect");
  ASSERT_TRUE(rect->GetComputedStyle());
  EXPECT_FALSE(rect->GetComputedStyle()->HasIncrementalStyleBlocker());
  EXPECT_EQ(Color(0, 128, 0), rect->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyFill()));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  style_engine.SetStatsEnabled(true);

  ClassicScript::CreateUnspecifiedScript(
      "document.getElementById('rect').setAttribute('x', '30');")
      ->RunScript(GetDocument().domWindow());
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  style_engine.RecalcStyle();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  ASSERT_TRUE(rect->GetComputedStyle());
  EXPECT_EQ(Length::Fixed(30), rect->GetComputedStyle()->X());
  EXPECT_EQ(Color(0, 128, 0), rect->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyFill()));
  EXPECT_EQ(GetParam() ? 1u : 0u,
            style_engine.Stats()->svg_presentation_attribute_styles_used);
}

TEST_P(SVGIncrementalStyleTest, IncrementalStyleBlockedByAnimation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes fade { from { opacity: 0.25 } to { opacity: 0.25 } }
      rect { animation: fade 100s linear infinite paused }
    </style>
    <svg><rect id="rect" x="10"/></svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* rect = GetElementById("rect");
  ASSERT_TRUE(rect->GetElementAnimations());
  ASSERT_TRUE(rect->GetComputedStyle());
  EXPECT_FALSE(rect->GetComputedStyle()->HasIncrementalStyleBlocker());
  EXPECT_EQ(Length::Fixed(10), rect->GetComputedStyle()->X());
  EXPECT_EQ(0.25f, rect->GetComputedStyle()->Opacity());
  GetDocument().GetSettings()->SetScriptEnabled(true);
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  style_engine.SetStatsEnabled(true);

  ClassicScript::CreateUnspecifiedScript(
      "document.getElementById('rect').setAttribute('x', '30');")
      ->RunScript(GetDocument().domWindow());
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  style_engine.RecalcStyle();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  ASSERT_TRUE(rect->GetComputedStyle());
  EXPECT_EQ(Length::Fixed(30), rect->GetComputedStyle()->X());
  EXPECT_EQ(0.25f, rect->GetComputedStyle()->Opacity());
  EXPECT_EQ(0u, style_engine.Stats()->svg_presentation_attribute_styles_used);
}

TEST_F(SVGElementTest, BaseComputedStyleForSMILWithContainerQueries) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #rect2 { display: none }
      @container (max-width: 200px) {
        rect, g { color: green; }
      }
      @container (min-width: 300px) {
        rect, g { background-color: red; }
      }
    </style>
    <div style="container-type: inline-size; width: 200px">
      <svg>
        <rect id="rect1" />
        <rect id="rect2" />
        <g id="g"></g>
      </svg>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* rect1 =
      To<SVGElement>(GetDocument().getElementById(AtomicString("rect1")));
  auto* rect2 =
      To<SVGElement>(GetDocument().getElementById(AtomicString("rect2")));
  auto* g = To<SVGElement>(GetDocument().getElementById(AtomicString("g")));

  auto force_needs_override_style = [](SVGElement& svg_element) {
    svg_element.EnsureSVGRareData()->SetNeedsOverrideComputedStyleUpdate();
  };

  force_needs_override_style(*rect1);
  force_needs_override_style(*rect2);
  force_needs_override_style(*g);

  const auto* rect1_style = rect1->BaseComputedStyleForSMIL();
  const auto* rect2_style = rect2->BaseComputedStyleForSMIL();
  const auto* g_style = g->BaseComputedStyleForSMIL();

  const Color green(0, 128, 0);

  EXPECT_EQ(rect1_style->VisitedDependentColor(GetCSSPropertyColor()), green);
  EXPECT_EQ(rect2_style->VisitedDependentColor(GetCSSPropertyColor()), green);
  EXPECT_EQ(g_style->VisitedDependentColor(GetCSSPropertyColor()), green);

  EXPECT_EQ(rect1_style->VisitedDependentColor(GetCSSPropertyBackgroundColor()),
            Color::kTransparent);
  EXPECT_EQ(rect2_style->VisitedDependentColor(GetCSSPropertyBackgroundColor()),
            Color::kTransparent);
  EXPECT_EQ(g_style->VisitedDependentColor(GetCSSPropertyBackgroundColor()),
            Color::kTransparent);
}

TEST_F(SVGElementTest, ContainerUnitContext) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container, #svg { container-type:size; }
      #container {
        width: 200px;
        height: 200px;
      }
      #svg {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id="container">
      <svg id="svg"></svg>
    </div>
  )HTML");

  auto* svg = To<SVGElement>(GetDocument().getElementById(AtomicString("svg")));
  const auto* value = DynamicTo<CSSPrimitiveValue>(
      css_test_helpers::ParseValue(GetDocument(), "<length>", "100cqw"));
  const auto* length =
      MakeGarbageCollected<SVGLength>(*value, SVGLengthMode::kWidth);
  EXPECT_FLOAT_EQ(200.0f, length->Value(SVGLengthContext(svg)));
}

TEST_F(SVGElementTest, SynchronizeAttributeInShadowInstances) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <svg>
      <defs>
        <rect id="target" width="10" height="10" fill="red" />
      </defs>
      <use id="use" href="#target" />
      <rect id="standalone" width="20" height="20" fill="green" />
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* target =
      To<SVGElement>(GetDocument().getElementById(AtomicString("target")));
  auto* standalone =
      To<SVGElement>(GetDocument().getElementById(AtomicString("standalone")));
  ASSERT_TRUE(target);
  ASSERT_TRUE(standalone);
  EXPECT_FALSE(target->InstancesForElement().empty());
  EXPECT_TRUE(standalone->InstancesForElement().empty());

  SVGElement* instance = target->InstancesForElement().begin()->Get();
  ASSERT_TRUE(instance);
  EXPECT_EQ(instance->getAttribute(AtomicString("fill")), "red");

  // Verify attribute synchronization with SvgInstanceSyncOptimization enabled.
  {
    ScopedSvgInstanceSyncOptimizationForTest optimization(true);
    target->setAttribute(AtomicString("fill"), AtomicString("blue"));
    EXPECT_EQ(instance->getAttribute(AtomicString("fill")), "blue");

    standalone->setAttribute(AtomicString("fill"), AtomicString("yellow"));
    EXPECT_EQ(standalone->getAttribute(AtomicString("fill")), "yellow");
    EXPECT_TRUE(standalone->InstancesForElement().empty());
  }

  // Verify attribute synchronization with killswitch disabled (fallback path).
  {
    ScopedSvgInstanceSyncOptimizationForTest optimization(false);
    target->setAttribute(AtomicString("fill"), AtomicString("purple"));
    EXPECT_EQ(instance->getAttribute(AtomicString("fill")), "purple");

    standalone->setAttribute(AtomicString("fill"), AtomicString("black"));
    EXPECT_EQ(standalone->getAttribute(AtomicString("fill")), "black");
    EXPECT_TRUE(standalone->InstancesForElement().empty());
  }
}

TEST_F(SVGElementTest, EventListenerSynchronization) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <svg>
      <defs>
        <rect id="target" width="10" height="10" />
      </defs>
      <use id="use" href="#target" />
      <rect id="standalone" width="20" height="20" />
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* target =
      To<SVGElement>(GetDocument().getElementById(AtomicString("target")));
  auto* standalone =
      To<SVGElement>(GetDocument().getElementById(AtomicString("standalone")));
  ASSERT_TRUE(target);
  ASSERT_TRUE(standalone);
  EXPECT_FALSE(target->InstancesForElement().empty());
  EXPECT_TRUE(standalone->InstancesForElement().empty());

  SVGElement* instance = target->InstancesForElement().begin()->Get();
  ASSERT_TRUE(instance);

  auto* listener = MakeGarbageCollected<SVGTestEventListener>();

  // Verify event listener propagation with SvgInstanceSyncOptimization enabled.
  {
    ScopedSvgInstanceSyncOptimizationForTest optimization(true);

    target->addEventListener(event_type_names::kClick, listener,
                             /*use_capture=*/true);
    EXPECT_TRUE(instance->HasEventListeners(event_type_names::kClick));

    target->removeEventListener(event_type_names::kClick, listener,
                                /*use_capture=*/true);
    EXPECT_FALSE(instance->HasEventListeners(event_type_names::kClick));

    // Standalone element has no instances; listener works normally.
    standalone->addEventListener(event_type_names::kClick, listener);
    EXPECT_TRUE(standalone->HasEventListeners(event_type_names::kClick));
    standalone->removeEventListener(event_type_names::kClick, listener,
                                    /*use_capture=*/false);
    EXPECT_FALSE(standalone->HasEventListeners(event_type_names::kClick));
    EXPECT_TRUE(standalone->InstancesForElement().empty());
  }

  // Verify event listener propagation with killswitch disabled (fallback path).
  {
    ScopedSvgInstanceSyncOptimizationForTest optimization(false);

    target->addEventListener(event_type_names::kClick, listener,
                             /*use_capture=*/true);
    EXPECT_TRUE(instance->HasEventListeners(event_type_names::kClick));

    target->removeEventListener(event_type_names::kClick, listener,
                                /*use_capture=*/true);
    EXPECT_FALSE(instance->HasEventListeners(event_type_names::kClick));

    standalone->addEventListener(event_type_names::kClick, listener);
    EXPECT_TRUE(standalone->HasEventListeners(event_type_names::kClick));
    standalone->removeEventListener(event_type_names::kClick, listener,
                                    /*use_capture=*/false);
    EXPECT_FALSE(standalone->HasEventListeners(event_type_names::kClick));
    EXPECT_TRUE(standalone->InstancesForElement().empty());
  }
}

}  // namespace blink
