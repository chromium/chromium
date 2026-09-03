// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_element.h"

#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/svg/svg_element_rare_data.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

class SVGTestEventListener : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override {}
};

}  // namespace

class SVGElementTest : public PageTestBase {};

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
