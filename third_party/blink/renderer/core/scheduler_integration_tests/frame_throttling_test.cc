// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/test/test_web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/intersection_observer_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

using testing::_;

namespace blink {

using html_names::kStyleAttr;

// NOTE: This test uses <iframe sandbox> to create cross origin iframes.

// This should not conflict with the existing PaintTestConfiguration bits.
enum { kIntersectionOptimization = 1 << 20 };

class FrameThrottlingTest : public PaintTestConfigurations,
                            public SimTest,
                            private ScopedIntersectionOptimizationForTest {
 protected:
  FrameThrottlingTest()
      : ScopedIntersectionOptimizationForTest(GetParam() &
                                              kIntersectionOptimization) {}

  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(640, 480));
  }

  SimCanvas::Commands CompositeFrame() {
    auto commands = Compositor().BeginFrame();
    // Ensure intersection observer notifications get delivered.
    test::RunPendingTasks();
    return commands;
  }

  // Number of rectangles that make up the root layer's touch handler region.
  size_t TouchHandlerRegionSize() {
    const auto* frame_view = WebView().MainFrameImpl()->GetFrameView();
    return ScrollingContentsCcLayerByScrollElementId(
               frame_view->RootCcLayer(),
               frame_view->LayoutViewport()->GetScrollElementId())
        ->touch_action_region()
        .GetAllRegions()
        .GetRegionComplexity();
  }

  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }

  class EmptyEventListener final : public NativeEventListener {
   public:
    void Invoke(ExecutionContext* execution_context, Event*) override {}
  };
};

INSTANTIATE_TEST_SUITE_P(All,
                         FrameThrottlingTest,
                         ::testing::Values(PAINT_TEST_SUITE_P_VALUES,
                                           kIntersectionOptimization));

TEST_P(FrameThrottlingTest, ThrottleInvisibleFrames) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe sandbox id=frame></iframe>");

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();

  // Initially both frames are visible.
  EXPECT_FALSE(GetDocument().View()->IsHiddenForThrottling());
  EXPECT_FALSE(frame_document->View()->IsHiddenForThrottling());

  // Moving the child fully outside the parent makes it invisible.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_FALSE(GetDocument().View()->IsHiddenForThrottling());
  EXPECT_TRUE(frame_document->View()->IsHiddenForThrottling());

  // A partially visible child is considered visible.
  frame_element->setAttribute(
      kStyleAttr, AtomicString("transform: translate(-50px, 0px, 0px)"));
  CompositeFrame();
  EXPECT_FALSE(GetDocument().View()->IsHiddenForThrottling());
  EXPECT_FALSE(frame_document->View()->IsHiddenForThrottling());
}

TEST_P(FrameThrottlingTest, HiddenSameOriginFramesAreNotThrottled) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame src=iframe.html></iframe>");
  frame_resource.Complete("<iframe id=innerFrame></iframe>");

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();

  auto* inner_frame_element = To<HTMLIFrameElement>(
      frame_document->getElementById(AtomicString("innerFrame")));
  auto* inner_frame_document = inner_frame_element->contentDocument();

  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(inner_frame_document->View()->CanThrottleRendering());

  // Hidden same origin frames are not throttled.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(inner_frame_document->View()->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, HiddenCrossOriginFramesAreThrottled) {
  // Create a document with doubly nested iframes.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame src=iframe.html></iframe>");
  frame_resource.Complete("<iframe id=innerFrame sandbox></iframe>");

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();

  auto* inner_frame_element = To<HTMLIFrameElement>(
      frame_document->getElementById(AtomicString("innerFrame")));
  auto* inner_frame_document = inner_frame_element->contentDocument();

  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(inner_frame_document->View()->CanThrottleRendering());

  // Hidden cross origin frames are throttled.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_TRUE(inner_frame_document->View()->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, IntersectionObservationOverridesThrottling) {
  // Create a document with doubly nested iframes.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame src=iframe.html></iframe>");
  frame_resource.Complete("<iframe id=innerFrame sandbox></iframe>");

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();

  auto* inner_frame_element = To<HTMLIFrameElement>(
      frame_document->getElementById(AtomicString("innerFrame")));
  auto* inner_frame_document = inner_frame_element->contentDocument();

  // Hidden cross origin frames are throttled.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_TRUE(inner_frame_document->View()->ShouldThrottleRenderingForTest());

  // An intersection observation overrides throttling during a lifecycle update.
  inner_frame_document->View()->SetIntersectionObservationState(
      LocalFrameView::kRequired);
  {
    GetDocument().GetFrame()->View()->SetTargetStateForTest(
        DocumentLifecycle::kPaintClean);
    inner_frame_document->Lifecycle().EnsureStateAtMost(
        DocumentLifecycle::kVisualUpdatePending);
    EXPECT_FALSE(
        inner_frame_document->View()->ShouldThrottleRenderingForTest());
    GetDocument().GetFrame()->View()->SetTargetStateForTest(
        DocumentLifecycle::kUninitialized);
  }

  inner_frame_document->View()->ScheduleAnimation();

  LayoutView* inner_view = inner_frame_document->View()->GetLayoutView();

  inner_view->SetNeedsLayout("test");
  inner_view->SetShouldDoFullPaintInvalidation(
      PaintInvalidationReason::kLayout);
  inner_view->Layer()->SetNeedsRepaint();
  EXPECT_TRUE(inner_frame_document->View()
                  ->GetLayoutView()
                  ->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(inner_view->Layer()->SelfNeedsRepaint());

  CompositeFrame();
  // The lifecycle update should only be overridden for one frame.
  EXPECT_TRUE(inner_frame_document->View()->ShouldThrottleRenderingForTest());

  EXPECT_FALSE(inner_view->NeedsLayout());
  EXPECT_LT(inner_frame_document->Lifecycle().GetState(),
            DocumentLifecycle::kPaintClean);
  EXPECT_TRUE(inner_view->Layer()->SelfNeedsRepaint());
}

TEST_P(FrameThrottlingTest, NestedIntersectionObservationStateUpdated) {
  // Create two nested frames which are throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  SimRequest child_frame_resource("https://example.com/child-iframe.html",
                                  "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete(
      "<iframe id=child-frame sandbox src=child-iframe.html></iframe>");
  child_frame_resource.Complete("");

  // Move both frames offscreen to make them throttled.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* child_frame_element =
      To<HTMLIFrameElement>(frame_element->contentDocument()->getElementById(
          AtomicString("child-frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));

  CompositeFrame();

  auto* root_view = LocalFrameRoot().GetFrame()->View();
  ASSERT_FALSE(root_view->ShouldThrottleRenderingForTest());
  auto* frame_view = frame_element->contentDocument()->View();
  ASSERT_TRUE(frame_view->ShouldThrottleRenderingForTest());
  auto* child_view = child_frame_element->contentDocument()->View();
  ASSERT_TRUE(child_view->ShouldThrottleRenderingForTest());

  // Force |child_view| to do an intersection observation.
  child_view->SetIntersectionObservationState(LocalFrameView::kRequired);

  // Ensure all frames need a layout.
  root_view->SetNeedsLayout();
  frame_view->SetNeedsLayout();
  child_view->SetNeedsLayout();

  // Though |frame_view| is throttled, the descendant (|child_view|) should
  // still be updated which resets the intersection observation state.
  CompositeFrame();
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            root_view->GetIntersectionObservationStateForTesting());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            child_view->GetIntersectionObservationStateForTesting());
}

// This test creates a throttled local root (simulating a throttled OOPIF) and
// ensures the intersection observation state of descendants can still be
// updated.
TEST_P(FrameThrottlingTest,
       IntersectionObservationStateUpdatedWithThrottledLocalRoot) {
  SimRequest local_root_resource("https://example.com/", "text/html");
  SimRequest child_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  local_root_resource.Complete(
      "<iframe id=frame sandbox src=iframe.html></iframe>");
  child_resource.Complete("");
  CompositeFrame();

  auto* root_frame = LocalFrameRoot().GetFrame();
  auto* root_frame_view = root_frame->View();
  root_frame_view->SetNeedsLayout();
  root_frame_view->ScheduleAnimation();
  root_frame_view->SetLifecycleUpdatesThrottledForTesting(true);
  ASSERT_TRUE(root_frame->IsLocalRoot());
  ASSERT_TRUE(root_frame_view->ShouldThrottleRenderingForTest());

  auto* child_frame_document =
      To<HTMLIFrameElement>(
          root_frame->GetDocument()->getElementById(AtomicString("frame")))
          ->contentDocument();
  auto* child_frame_view = child_frame_document->View();
  // Force |child_frame_view| to do an intersection observation.
  child_frame_view->SetIntersectionObservationState(LocalFrameView::kRequired);

  // Though |root_frame_view| is throttled, the descendant (|child_frame_view|)
  // should still be updated which resets the intersection observation state.
  CompositeFrame();
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            root_frame_view->GetIntersectionObservationStateForTesting());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            child_frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(FrameThrottlingTest,
       ThrottlingOverrideOnlyAppliesDuringLifecycleUpdate) {
  // Create a document with a hidden cross-origin subframe.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <iframe id="frame" sandbox src="iframe.html"
        style="transform: translateY(480px)">
  )HTML");
  frame_resource.Complete("<!doctype html>");

  CompositeFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();
  // Hidden cross origin frames are throttled.
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRenderingForTest());

  // An intersection observation overrides throttling, but this is only during
  // the lifecycle.
  frame_document->View()->SetIntersectionObservationState(
      LocalFrameView::kRequired);
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRenderingForTest());
  {
    GetDocument().GetFrame()->View()->SetTargetStateForTest(
        DocumentLifecycle::kPaintClean);
    frame_document->Lifecycle().EnsureStateAtMost(
        DocumentLifecycle::kVisualUpdatePending);
    EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());
    GetDocument().GetFrame()->View()->SetTargetStateForTest(
        DocumentLifecycle::kUninitialized);
  }

  // A lifecycle update can update the throttled frame to just LayoutClean but
  // the frame should still be considered throttled outside the lifecycle
  // because it is not fully running the lifecycle.
  frame_document->View()->GetLayoutView()->SetNeedsLayout("test");
  frame_document->View()->ScheduleAnimation();
  frame_document->View()->GetLayoutView()->Layer()->SetNeedsRepaint();
  CompositeFrame();
  EXPECT_EQ(DocumentLifecycle::kPrePaintClean,
            frame_document->Lifecycle().GetState());
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRenderingForTest());
}

TEST_P(FrameThrottlingTest, ForAllThrottledLocalFrameViews) {
  // Create a document with a hidden cross-origin subframe.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <iframe id="frame" sandbox src="iframe.html"
        style="transform: translateY(480px)">
  )HTML");
  frame_resource.Complete("<!doctype html>");

  CompositeFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();
  // Hidden cross origin frames are throttled.
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRenderingForTest());
  // Main frame is not throttled.
  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRenderingForTest());

  LocalFrameView::AllowThrottlingScope allow_throttling(*GetDocument().View());
  unsigned throttled_count = 0;
  GetDocument().View()->ForAllThrottledLocalFrameViews(
      [&throttled_count](LocalFrameView&) { throttled_count++; });
  EXPECT_EQ(1u, throttled_count);
}

TEST_P(FrameThrottlingTest, HiddenCrossOriginDisplayNoneFramesAreThrottled) {
  // Create a document with doubly nested iframes.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame src=iframe.html></iframe>");
  frame_resource.Complete(
      "<iframe id=innerFrame style='display: none; width: 0; height: 0' "
      "sandbox></iframe>");

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();

  auto* inner_frame_element = To<HTMLIFrameElement>(
      frame_document->getElementById(AtomicString("innerFrame")));
  auto* inner_frame_document = inner_frame_element->contentDocument();

  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(inner_frame_document->View()->CanThrottleRendering());

  // The frame is throttled because its dimensions are 0x0, as per experimental
  // feature ThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  // When ThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes is enabled,
  // we will throttle the frame.
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_TRUE(inner_frame_document->View()->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, ThrottledLifecycleUpdate) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe sandbox id=frame></iframe>");

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();

  // Enable throttling for the child frame.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  EXPECT_EQ(DocumentLifecycle::kPaintClean,
            frame_document->Lifecycle().GetState());

  // Mutating the throttled frame followed by a beginFrame will not result in
  // a complete lifecycle update.
  // TODO(skyostil): these expectations are either wrong, or the test is
  // not exercising the code correctly. PaintClean means the entire lifecycle
  // ran.
  frame_element->setAttribute(html_names::kWidthAttr, AtomicString("50"));
  CompositeFrame();

  EXPECT_EQ(DocumentLifecycle::kPaintClean,
            frame_document->Lifecycle().GetState());

  // A hit test will not force a complete lifecycle update.
  WebView().MainFrameWidget()->HitTestResultAt(gfx::PointF());
  EXPECT_EQ(DocumentLifecycle::kPaintClean,
            frame_document->Lifecycle().GetState());
}

TEST_P(FrameThrottlingTest, UnthrottlingFrameSchedulesAnimation) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe sandbox id=frame></iframe>");
  CompositeFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));

  // First make the child hidden to enable throttling.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  // Then bring it back on-screen. This should schedule an animation update.
  frame_element->setAttribute(kStyleAttr, g_empty_atom);
  CompositeFrame();
  EXPECT_TRUE(Compositor().NeedsBeginFrame());
  CompositeFrame();
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
}

TEST_P(FrameThrottlingTest, ThrottledFrameCompositing) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id="container">
    <iframe id=frame sandbox src=iframe.html></iframe>
    </div>
  )HTML");
  frame_resource.Complete(R"HTML(
    <html id="inner_frame"></html>
  )HTML");

  CompositeFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_doc = frame_element->contentDocument();
  auto* frame_view = frame_doc->View();
  EXPECT_FALSE(frame_view->CanThrottleRendering());
  auto* root_layer = WebView().MainFrameImpl()->GetFrameView()->RootCcLayer();
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "container").size());
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_TRUE(CcLayerByOwnerNodeId(root_layer, frame_doc->GetDomNodeId()));
  } else {
    EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "inner_frame").size());
  }

  // First make the child hidden to enable throttling, and composite
  // the container.
  auto* container_element =
      GetDocument().getElementById(AtomicString("container"));
  container_element->setAttribute(
      kStyleAttr,
      AtomicString("will-change: transform; transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_TRUE(frame_view->CanThrottleRendering());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "container").size());
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_TRUE(CcLayerByOwnerNodeId(root_layer, frame_doc->GetDomNodeId()));
  } else {
    EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "inner_frame").size());
  }

  // Then bring it back on-screen, and decomposite container.
  container_element->setAttribute(kStyleAttr, g_empty_atom);
  CompositeFrame();
  ASSERT_TRUE(Compositor().NeedsBeginFrame());
  CompositeFrame();
  EXPECT_FALSE(frame_view->CanThrottleRendering());
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "container").size());
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_TRUE(CcLayerByOwnerNodeId(root_layer, frame_doc->GetDomNodeId()));
  } else {
    EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "inner_frame").size());
  }
}

TEST_P(FrameThrottlingTest, MutatingThrottledFrameDoesNotCauseAnimation) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete("<style> html { background: red; } </style>");

  // Check that the frame initially shows up.
  auto commands1 = CompositeFrame();
  EXPECT_TRUE(commands1.Contains(SimCanvas::kRect, "red"));

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));

  // Move the frame offscreen to throttle it.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Mutating the throttled frame should not cause an animation to be scheduled.
  frame_element->contentDocument()->documentElement()->setAttribute(
      kStyleAttr, AtomicString("background: green"));
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  // Move the frame back on screen to unthrottle it.
  frame_element->setAttribute(kStyleAttr, g_empty_atom);
  EXPECT_TRUE(Compositor().NeedsBeginFrame());

  // The first frame we composite after unthrottling won't contain the
  // frame's new contents because unthrottling happens at the end of the
  // lifecycle update. We need to do another composite to refresh the frame's
  // contents.
  auto commands2 = CompositeFrame();
  EXPECT_FALSE(commands2.Contains(SimCanvas::kRect, "green"));
  EXPECT_TRUE(Compositor().NeedsBeginFrame());

  auto commands3 = CompositeFrame();
  EXPECT_TRUE(commands3.Contains(SimCanvas::kRect, "green"));
}

TEST_P(FrameThrottlingTest, SynchronousLayoutInThrottledFrame) {
  // Create a hidden frame which is throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete("<div id=div></div>");

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));

  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();

  // Change the size of a div in the throttled frame.
  auto* div_element =
      frame_element->contentDocument()->getElementById(AtomicString("div"));
  div_element->setAttribute(kStyleAttr, AtomicString("width: 50px"));

  // Querying the width of the div should do a synchronous layout update even
  // though the frame is being throttled.
  EXPECT_EQ(50, div_element->clientWidth());
}

TEST_P(FrameThrottlingTest, UnthrottlingTriggersRepaint) {
  // Create a hidden frame which is throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete("<style> html { background: green; } </style>");

  // Move the frame offscreen to throttle it.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Scroll down to unthrottle the frame. The first frame we composite after
  // scrolling won't contain the frame yet, but will schedule another repaint.
  WebView().MainFrameImpl()->GetFrameView()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 480), mojom::blink::ScrollType::kProgrammatic);
  auto commands = CompositeFrame();
  EXPECT_FALSE(commands.Contains(SimCanvas::kRect, "green"));

  // Now the frame contents should be visible again.
  auto commands2 = CompositeFrame();
  EXPECT_TRUE(commands2.Contains(SimCanvas::kRect, "green"));
}

TEST_P(FrameThrottlingTest, UnthrottlingTriggersRepaintInCompositedChild) {
  // Create a hidden frame with a composited child layer.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      background-color: green;
      transform: translateZ(0);
    }
    </style><div></div>
  )HTML");

  // Move the frame offscreen to throttle it.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Scroll down to unthrottle the frame. The first frame we composite after
  // scrolling won't contain the frame yet, but will schedule another repaint.
  WebView().MainFrameImpl()->GetFrameView()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 480), mojom::blink::ScrollType::kProgrammatic);
  auto commands = CompositeFrame();
  EXPECT_FALSE(commands.Contains(SimCanvas::kRect, "green"));

  // Now the composited child contents should be visible again.
  auto commands2 = CompositeFrame();
  EXPECT_TRUE(commands2.Contains(SimCanvas::kRect, "green"));
}

TEST_P(FrameThrottlingTest, ChangeStyleInThrottledFrame) {
  // Create a hidden frame which is throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete("<style> html { background: red; } </style>");

  // Move the frame offscreen to throttle it.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Change the background color of the frame's contents from red to green.
  frame_element->contentDocument()->body()->setAttribute(
      kStyleAttr, AtomicString("background: green"));

  // Scroll down to unthrottle the frame.
  WebView().MainFrameImpl()->GetFrameView()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 480), mojom::blink::ScrollType::kProgrammatic);
  auto commands = CompositeFrame();
  EXPECT_FALSE(commands.Contains(SimCanvas::kRect, "red"));
  EXPECT_FALSE(commands.Contains(SimCanvas::kRect, "green"));

  // Make sure the new style shows up instead of the old one.
  auto commands2 = CompositeFrame();
  EXPECT_TRUE(commands2.Contains(SimCanvas::kRect, "green"));
}

TEST_P(FrameThrottlingTest, ChangeOriginInThrottledFrame) {
  // Create a hidden frame which is throttled.
  SimRequest main_resource("http://example.com/", "text/html");
  SimRequest frame_resource("http://sub.example.com/iframe.html", "text/html");
  LoadURL("http://example.com/");
  main_resource.Complete(
      "<iframe style='position: absolute; top: 10000px' id=frame "
      "src=http://sub.example.com/iframe.html></iframe>");
  frame_resource.Complete("");

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));

  CompositeFrame();

  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_TRUE(frame_element->contentDocument()
                  ->GetFrame()
                  ->IsCrossOriginToNearestMainFrame());
  EXPECT_FALSE(frame_element->contentDocument()
                   ->View()
                   ->GetLayoutView()
                   ->NeedsPaintPropertyUpdate());

  NonThrowableExceptionState exception_state;

  // Security policy requires setting domain on both frames.
  GetDocument().setDomain(String("example.com"), exception_state);
  frame_element->contentDocument()->setDomain(String("example.com"),
                                              exception_state);

  EXPECT_FALSE(frame_element->contentDocument()
                   ->GetFrame()
                   ->IsCrossOriginToNearestMainFrame());
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_TRUE(frame_element->contentDocument()
                  ->View()
                  ->GetLayoutView()
                  ->NeedsPaintPropertyUpdate());
}

TEST_P(FrameThrottlingTest, MainFrameOriginChangeInvalidatesDescendants) {
  // Create a hidden frame which is throttled.
  SimRequest main_resource("https://sub.example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://sub.example.com/");
  main_resource.Complete(R"HTML(
    <iframe id='frame' style='position: absolute; top: 10000px'
        src='https://example.com/iframe.html'></iframe>
  )HTML");
  frame_resource.Complete("");

  CompositeFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  EXPECT_TRUE(frame_document->GetFrame()->IsCrossOriginToNearestMainFrame());
  EXPECT_FALSE(
      frame_document->View()->GetLayoutView()->NeedsPaintPropertyUpdate());

  // Set the domain of the child frame first which should be a no-op in terms of
  // cross-origin status changes.
  NonThrowableExceptionState exception_state;
  frame_element->contentDocument()->setDomain(String("example.com"),
                                              exception_state);
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  EXPECT_TRUE(frame_document->GetFrame()->IsCrossOriginToNearestMainFrame());
  EXPECT_FALSE(
      frame_document->View()->GetLayoutView()->NeedsPaintPropertyUpdate());

  // Then change the main frame origin which needs to invalidate the newly
  // cross-origin child.
  GetDocument().setDomain(String("example.com"), exception_state);
  EXPECT_FALSE(frame_document->GetFrame()->IsCrossOriginToNearestMainFrame());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_TRUE(
      frame_document->View()->GetLayoutView()->NeedsPaintPropertyUpdate());
}

TEST_P(FrameThrottlingTest, ThrottledFrameWithFocus) {
  WebView().GetSettings()->SetJavaScriptEnabled(true);
  ScopedCompositedSelectionUpdateForTest composited_selection_update(true);

  // Create a hidden frame which is throttled and has a text selection.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "<iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
  frame_resource.Complete(
      "some text to select\n"
      "<script>\n"
      "var range = document.createRange();\n"
      "range.selectNode(document.body);\n"
      "window.getSelection().addRange(range);\n"
      "</script>\n");

  // Move the frame offscreen to throttle it.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Give the frame focus and do another composite. The selection in the
  // compositor should be cleared because the frame is throttled.
  EXPECT_FALSE(Compositor().HasSelection());
  GetDocument().GetPage()->GetFocusController().SetFocusedFrame(
      frame_element->contentDocument()->GetFrame());
  GetDocument().body()->setAttribute(kStyleAttr,
                                     AtomicString("background: green"));
  CompositeFrame();
  EXPECT_FALSE(Compositor().HasSelection());
}

TEST_P(FrameThrottlingTest, ScrollingCoordinatorShouldSkipThrottledFrame) {
  // Create a hidden frame which is throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete(R"HTML(
    <style>
      html {
        background-image: linear-gradient(red, blue);
        background-attachment: fixed;
        will-change: transform;
      }
    </style>
  )HTML");

  // Move the frame offscreen to throttle it.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Change style of the frame's content to make it in VisualUpdatePending
  // state.
  frame_element->contentDocument()->body()->setAttribute(
      kStyleAttr, AtomicString("background: green"));
  // Change root frame's layout so that the next lifecycle update will call
  // ScrollingCoordinator::UpdateAfterPaint().
  GetDocument().body()->setAttribute(kStyleAttr, AtomicString("margin: 20px"));
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            frame_element->contentDocument()->Lifecycle().GetState());

  // This will call ScrollingCoordinator::UpdateAfterPaint() and should not
  // cause assert failure about isAllowedToQueryCompositingState() in the
  // throttled frame.
  UpdateAllLifecyclePhases();
  test::RunPendingTasks();
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            frame_element->contentDocument()->Lifecycle().GetState());
  // The fixed background in the throttled sub frame should not cause main
  // thread scrolling.
  EXPECT_FALSE(
      GetDocument().View()->LayoutViewport()->ShouldScrollOnMainThread());

  // Make the frame visible by changing its transform. This doesn't cause a
  // layout, but should still unthrottle the frame.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(0px)"));
  CompositeFrame();
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  // This CompositeFrame handles the visual update scheduled when we unthrottle
  // the iframe.
  CompositeFrame();
  // The fixed background in the throttled sub frame should be considered.
  EXPECT_TRUE(frame_element->contentDocument()
                  ->View()
                  ->LayoutViewport()
                  ->ShouldScrollOnMainThread());
  EXPECT_FALSE(
      GetDocument().View()->LayoutViewport()->ShouldScrollOnMainThread());
}

TEST_P(FrameThrottlingTest, ScrollingCoordinatorShouldSkipThrottledLayer) {
  WebView().GetSettings()->SetJavaScriptEnabled(true);
  SetPreferCompositingToLCDText(true);

  // Create a hidden frame which is throttled and has a touch handler inside a
  // composited layer.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "<iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
  frame_resource.Complete(
      "<div id=div style='transform: translateZ(0)' ontouchstart='foo()'>touch "
      "handler</div>");

  // Move the frame offscreen to throttle it.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Change style of the frame's content to make it in VisualUpdatePending
  // state.
  frame_element->contentDocument()->body()->setAttribute(
      kStyleAttr, AtomicString("background: green"));
  // Change root frame's layout so that the next lifecycle update will call
  // ScrollingCoordinator::UpdateAfterPaint().
  GetDocument().body()->setAttribute(kStyleAttr, AtomicString("margin: 20px"));
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            frame_element->contentDocument()->Lifecycle().GetState());

  // This will call ScrollingCoordinator::UpdateAfterPaint() and should not
  // cause an assert failure about isAllowedToQueryCompositingState() in the
  // throttled frame.
  UpdateAllLifecyclePhases();
  test::RunPendingTasks();
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            frame_element->contentDocument()->Lifecycle().GetState());
}

TEST_P(FrameThrottlingTest,
       ScrollingCoordinatorShouldSkipCompositedThrottledFrame) {
  SetPreferCompositingToLCDText(true);

  // Create a hidden frame which is throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete("<div style='height: 2000px'></div>");

  // Move the frame offscreen to throttle it.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Change style of the frame's content to make it in VisualUpdatePending
  // state.
  frame_element->contentDocument()->body()->setAttribute(
      kStyleAttr, AtomicString("background: green"));
  // Change root frame's layout so that the next lifecycle update will call
  // ScrollingCoordinator::UpdateAfterPaint().
  GetDocument().body()->setAttribute(kStyleAttr, AtomicString("margin: 20px"));
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            frame_element->contentDocument()->Lifecycle().GetState());

  // This will call ScrollingCoordinator::UpdateAfterPaint() and should not
  // cause an assert failure about isAllowedToQueryCompositingState() in the
  // throttled frame.
  CompositeFrame();
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            frame_element->contentDocument()->Lifecycle().GetState());

  // Make the frame visible by changing its transform. This doesn't cause a
  // layout, but should still unthrottle the frame.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(0px)"));
  CompositeFrame();  // Unthrottle the frame.

  EXPECT_FALSE(frame_element->contentDocument()
                   ->View()
                   ->ShouldThrottleRenderingForTest());
  // Handle the pending visual update of the unthrottled frame.
  CompositeFrame();
  EXPECT_EQ(DocumentLifecycle::kPaintClean,
            frame_element->contentDocument()->Lifecycle().GetState());
}

TEST_P(FrameThrottlingTest, UnthrottleByTransformingWithoutLayout) {
  // Create a hidden frame which is throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete("");

  // Move the frame offscreen to throttle it.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Make the frame visible by changing its transform. This doesn't cause a
  // layout, but should still unthrottle the frame.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(0px)"));
  CompositeFrame();
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, DumpThrottledFrame) {
  WebView().GetSettings()->SetJavaScriptEnabled(true);

  // Create a frame which is throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "main <iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
  frame_resource.Complete("");
  CompositeFrame();
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  ClassicScript::CreateUnspecifiedScript(
      "document.body.innerHTML = 'throttled'")
      ->RunScript(To<LocalDOMWindow>(frame_element->contentWindow()));
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  // The dumped contents should not include the throttled frame.
  WebString result =
      TestWebFrameContentDumper::DumpWebViewAsText(&WebView(), 1024);
  EXPECT_NE(std::string::npos, result.Utf8().find("main"));
  EXPECT_EQ(std::string::npos, result.Utf8().find("throttled"));
}

TEST_P(FrameThrottlingTest, ThrottleInnerCompositedLayer) {
  SetPreferCompositingToLCDText(true);

  // Create a hidden frame which is throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete(
      "<div id=div style='will-change: transform; background: blue'>DIV</div>");
  auto commands_not_throttled = CompositeFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* root_layer = WebView().MainFrameImpl()->GetFrameView()->RootCcLayer();
  // The inner div is composited.
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "div").size());

  // Before the iframe is throttled, we should create all drawing commands.
  unsigned full_draw_count = 7u;
  EXPECT_EQ(full_draw_count, commands_not_throttled.DrawCount());

  // Move the frame offscreen to throttle it.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  // The inner div should still be composited.
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "div").size());
  // The owner document may included stale painted output for the iframe in its
  // cache; make sure it gets invalidated.
  EXPECT_FALSE(To<LayoutBoxModelObject>(frame_element->GetLayoutObject())
                   ->Layer()
                   ->IsValid());

  // If painting of the iframe is throttled, we should only receive drawing
  // commands for the main frame.
  GetDocument().View()->ScheduleAnimation();
  auto commands_throttled = Compositor().BeginFrame();
  EXPECT_LT(commands_throttled.DrawCount(), full_draw_count);

  // Remove compositing trigger of inner_div.
  auto* inner_div =
      frame_element->contentDocument()->getElementById(AtomicString("div"));
  inner_div->setAttribute(kStyleAttr,
                          AtomicString("background: yellow; overflow: hidden"));
  // Do an unthrottled style and layout update, simulating the situation
  // triggered by script style/layout access.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  // And a throttled full lifecycle update.
  UpdateAllLifecyclePhases();

  // The inner div is no longer composited.
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "div").size());

  auto commands_throttled1 = CompositeFrame();
  EXPECT_LT(commands_throttled1.DrawCount(), full_draw_count);

  // Move the frame back on screen.
  frame_element->setAttribute(kStyleAttr, g_empty_atom);
  CompositeFrame();
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_FALSE(To<LayoutBoxModelObject>(frame_element->GetLayoutObject())
                   ->Layer()
                   ->IsValid());
  auto commands_not_throttled1 = CompositeFrame();
  // The inner div is still not composited.
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "div").size());

  // After the iframe is unthrottled, we should create all drawing items.
  EXPECT_EQ(commands_not_throttled1.DrawCount(), full_draw_count);
}

TEST_P(FrameThrottlingTest, ThrottleSubtreeAtomically) {
  // Create two nested frames which are throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  SimRequest child_frame_resource("https://example.com/child-iframe.html",
                                  "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete(
      "<iframe id=child-frame sandbox src=child-iframe.html></iframe>");
  child_frame_resource.Complete("");

  // Move both frames offscreen. IntersectionObservers will run during
  // post-lifecycle steps and synchronously update throttling status.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* child_frame_element =
      To<HTMLIFrameElement>(frame_element->contentDocument()->getElementById(
          AtomicString("child-frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  Compositor().BeginFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_TRUE(
      child_frame_element->contentDocument()->View()->CanThrottleRendering());

  // Move the frame back on screen.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(0px)"));
  Compositor().BeginFrame();
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_FALSE(
      child_frame_element->contentDocument()->View()->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, SkipPaintingLayersInThrottledFrames) {
  SetPreferCompositingToLCDText(true);

  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete(
      "<div id=div style='transform: translateZ(0); background: "
      "red'>layer</div>");
  auto commands = CompositeFrame();
  EXPECT_TRUE(commands.Contains(SimCanvas::kRect, "red"));

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  auto* frame_document = frame_element->contentDocument();
  EXPECT_EQ(DocumentLifecycle::kPaintClean,
            frame_document->Lifecycle().GetState());

  // Simulate the paint for a graphics layer being externally invalidated
  // (e.g., by video playback).
  frame_document->View()
      ->GetLayoutView()
      ->InvalidatePaintForViewAndDescendants();

  // The layer inside the throttled frame should not get painted.
  auto commands2 = CompositeFrame();
  EXPECT_FALSE(commands2.Contains(SimCanvas::kRect, "red"));
}

TEST_P(FrameThrottlingTest, SynchronousLayoutInAnimationFrameCallback) {
  WebView().GetSettings()->SetJavaScriptEnabled(true);

  // Prepare a page with two cross origin frames (from the same origin so they
  // are able to access eachother).
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest first_frame_resource("https://thirdparty.com/first.html",
                                  "text/html");
  SimRequest second_frame_resource("https://thirdparty.com/second.html",
                                   "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <iframe id=first name=first
    src='https://thirdparty.com/first.html'></iframe>\n
    <iframe id=second name=second
    src='https://thirdparty.com/second.html'></iframe>
  )HTML");

  // The first frame contains just a simple div. This frame will be made
  // throttled.
  first_frame_resource.Complete("<div id=d>first frame</div>");

  // The second frame just used to execute a requestAnimationFrame callback.
  second_frame_resource.Complete("");

  // Throttle the first frame.
  auto* first_frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("first")));
  first_frame_element->setAttribute(
      kStyleAttr, AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_TRUE(
      first_frame_element->contentDocument()->View()->CanThrottleRendering());

  // Run a animation frame callback in the second frame which mutates the
  // contents of the first frame and causes a synchronous style update. This
  // should not result in an unexpected lifecycle state even if the first
  // frame is throttled during the animation frame callback.
  auto* second_frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("second")));
  ClassicScript::CreateUnspecifiedScript(
      "window.requestAnimationFrame(function() {\n"
      "  var throttledFrame = window.parent.frames.first;\n"
      "  throttledFrame.document.documentElement.style = 'margin: 50px';\n"
      "  "
      "throttledFrame.document.querySelector('#d').getBoundingClientRect();"
      "\n"
      "});\n")
      ->RunScript(To<LocalDOMWindow>(second_frame_element->contentWindow()));
  CompositeFrame();
}

TEST_P(FrameThrottlingTest, AllowOneAnimationFrame) {
  WebView().GetSettings()->SetJavaScriptEnabled(true);

  // Prepare a page with two cross origin frames (from the same origin so they
  // are able to access eachother).
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://thirdparty.com/frame.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<iframe id=frame style=\"position: fixed; top: -10000px\" "
      "src='https://thirdparty.com/frame.html'></iframe>");

  frame_resource.Complete(R"HTML(
    <script>
    window.requestAnimationFrame(() => { window.didRaf = true; });
    </script>
  )HTML");

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  v8::HandleScope scope(Window().GetIsolate());
  v8::Local<v8::Value> result =
      ClassicScript::CreateUnspecifiedScript("window.didRaf;")
          ->RunScriptAndReturnValue(
              To<LocalDOMWindow>(frame_element->contentWindow()))
          .GetSuccessValueOrEmpty();
  EXPECT_TRUE(result->IsTrue());
}

TEST_P(FrameThrottlingTest, UpdatePaintPropertiesOnUnthrottling) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete("<div id='div'>Inner</div>");
  CompositeFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();
  auto* inner_div = frame_document->getElementById(AtomicString("div"));
  auto* inner_div_object = inner_div->GetLayoutObject();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());

  frame_element->setAttribute(html_names::kStyleAttr,
                              AtomicString("transform: translateY(1000px)"));
  CompositeFrame();
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(inner_div_object->FirstFragment().PaintProperties());

  // Mutating the throttled frame should not cause paint property update.
  inner_div->setAttribute(html_names::kStyleAttr,
                          AtomicString("transform: translateY(20px)"));
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(inner_div_object->FirstFragment().PaintProperties());

  // Move the frame back on screen to unthrottle it.
  frame_element->setAttribute(html_names::kStyleAttr, g_empty_atom);
  // The first update unthrottles the frame, the second actually update layout
  // and paint properties etc.
  CompositeFrame();
  EXPECT_TRUE(frame_document->GetLayoutView()->Layer()->SelfNeedsRepaint());
  CompositeFrame();
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_EQ(gfx::Vector2dF(0, 20), inner_div->GetLayoutObject()
                                       ->FirstFragment()
                                       .PaintProperties()
                                       ->Transform()
                                       ->Get2dTranslation());
}

TEST_P(FrameThrottlingTest, DisplayNoneNotThrottled) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "<style>iframe { transform: translateY(480px); }</style>"
      "<iframe sandbox id=frame></iframe>");

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();

  // Initially the frame is throttled as it is offscreen.
  CompositeFrame();
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());

  // Setting display:none unthrottles the frame.
  frame_element->setAttribute(kStyleAttr, AtomicString("display: none"));
  CompositeFrame();
  // When ThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes is enabled,
  // we will throttle cross-origin display:none.
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, DisplayNoneChildrenRemainThrottled) {
  // Create two nested frames which are throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  SimRequest child_frame_resource("https://example.com/child-iframe.html",
                                  "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete(
      "<iframe id=child-frame sandbox src=child-iframe.html></iframe>");
  child_frame_resource.Complete("");

  // Move both frames offscreen to make them throttled.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* child_frame_element =
      To<HTMLIFrameElement>(frame_element->contentDocument()->getElementById(
          AtomicString("child-frame")));
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_TRUE(
      child_frame_element->contentDocument()->View()->CanThrottleRendering());

  // Setting display:none for the parent frame throttles the parent and also
  // the child. This behavior differs from Safari.
  frame_element->setAttribute(kStyleAttr, AtomicString("display: none"));
  CompositeFrame();
  // When ThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes is enabled,
  // the cross-origin, display:none frame will be throttled.
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_TRUE(
      child_frame_element->contentDocument()->View()->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, LifecycleUpdateAfterUnthrottledCompositingUpdate) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  // The frame is initially throttled.
  main_resource.Complete(R"HTML(
    <iframe id='frame' sandbox src='iframe.html'
        style='transform: translateY(480px)'></iframe>
  )HTML");
  frame_resource.Complete("<div id='div'>Foo</div>");

  CompositeFrame();
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRendering());

  frame_document->getElementById(AtomicString("div"))
      ->setAttribute(kStyleAttr, AtomicString("will-change: transform"));
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);

  // Then do a full lifecycle with throttling enabled. This should not crash.
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRenderingForTest());
  UpdateAllLifecyclePhases();
}

TEST_P(FrameThrottlingTest, NestedFramesInRemoteFrameHiddenAndShown) {
  InitializeRemote();

  SimRequest local_root_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  SimRequest child_frame_resource("https://example.com/child-iframe.html",
                                  "text/html");

  LoadURL("https://example.com/");
  local_root_resource.Complete(
      "<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete(
      "<iframe id=child-frame sandbox src=child-iframe.html></iframe>");
  child_frame_resource.Complete("");

  mojom::blink::ViewportIntersectionState intersection;
  intersection.main_frame_intersection = gfx::Rect(0, 0, 100, 100);
  intersection.outermost_main_frame_size = gfx::Size(100, 100);
  intersection.viewport_intersection = gfx::Rect(0, 0, 100, 100);
  LocalFrameRoot().FrameWidget()->Resize(gfx::Size(300, 200));
  static_cast<WebFrameWidgetImpl*>(LocalFrameRoot().FrameWidget())
      ->ApplyViewportIntersectionForTesting(intersection.Clone());

  auto* root_frame = LocalFrameRoot().GetFrame();
  auto* frame_document =
      To<HTMLIFrameElement>(
          root_frame->GetDocument()->getElementById(AtomicString("frame")))
          ->contentDocument();
  auto* frame_view = frame_document->View();
  auto* child_document = To<HTMLIFrameElement>(frame_document->getElementById(
                                                   AtomicString("child-frame")))
                             ->contentDocument();
  auto* child_view = child_document->View();

  CompositeFrame();
  EXPECT_FALSE(frame_view->CanThrottleRendering());
  EXPECT_FALSE(child_view->CanThrottleRendering());

  // Hide the frame without any other change. The new throttling state will not
  // be computed until the next lifecycle update; but merely hiding the frame
  // will not schedule an update, so we must force one for the purpose of
  // testing.
  LocalFrameRoot().WasHidden();
  root_frame->View()->ScheduleAnimation();
  CompositeFrame();
  EXPECT_EQ(root_frame->RemoteViewportIntersection(),
            gfx::Rect(0, 0, 100, 100));
  EXPECT_TRUE(root_frame->View()->CanThrottleRenderingForPropagation());
  EXPECT_EQ(root_frame->GetOcclusionState(),
            mojom::FrameOcclusionState::kPossiblyOccluded);
  EXPECT_TRUE(frame_view->CanThrottleRendering());
  EXPECT_TRUE(child_view->CanThrottleRendering());
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  // Simulate a trivial style change that doesn't trigger layout, compositing
  // update, but schedules layout tree update.
  frame_document->documentElement()->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("color: blue"));
  // This is needed to reproduce crbug.com/1054644 before the fix.
  frame_view->SetIntersectionObservationState(LocalFrameView::kDesired);

  // Show the frame without any other change.
  LocalFrameRoot().WasShown();
  static_cast<WebFrameWidgetImpl*>(LocalFrameRoot().FrameWidget())
      ->ApplyViewportIntersectionForTesting(intersection.Clone());
  CompositeFrame();
  EXPECT_EQ(root_frame->RemoteViewportIntersection(),
            gfx::Rect(0, 0, 100, 100));
  EXPECT_FALSE(root_frame->View()->CanThrottleRenderingForPropagation());
  EXPECT_NE(root_frame->GetOcclusionState(),
            mojom::FrameOcclusionState::kPossiblyOccluded);
  EXPECT_FALSE(frame_view->CanThrottleRendering());
  // The child frame's throtting status is not updated because the parent
  // document has pending visual update.
  EXPECT_TRUE(child_view->CanThrottleRendering());

  CompositeFrame();
  EXPECT_FALSE(frame_view->CanThrottleRendering());
  // The child frame's throttling status should be updated now.
  EXPECT_FALSE(child_view->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, LifecycleThrottledFrameNeedsRepaint) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  // The frame is initially throttled.
  main_resource.Complete("<iframe id='frame' src='iframe.html'></iframe>");
  frame_resource.Complete("<body style='background: red'></body>");

  auto commands = CompositeFrame();
  EXPECT_TRUE(commands.Contains(SimCanvas::kRect, "red"));

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();
  frame_document->View()->SetLifecycleUpdatesThrottledForTesting(true);
  GetDocument().View()->GetLayoutView()->Layer()->SetNeedsRepaint();
  GetDocument().View()->ScheduleAnimation();
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRenderingForTest());

  commands = CompositeFrame();
  // The throttled frame is omitted for paint.
  EXPECT_FALSE(commands.Contains(SimCanvas::kRect, "red"));

  frame_document->body()->setAttribute(kStyleAttr,
                                       AtomicString("background: green"));
  // Update life cycle update except paint without throttling, which will do
  // paint invalidation.
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(
      frame_document->GetLayoutView()->Layer()->SelfOrDescendantNeedsRepaint());
  // The NeedsRepaint flag doesn't propagte across frame boundary for now.
  EXPECT_FALSE(
      GetDocument().GetLayoutView()->Layer()->SelfOrDescendantNeedsRepaint());

  commands = CompositeFrame();
  EXPECT_FALSE(commands.Contains(SimCanvas::kRect, "green"));
  EXPECT_TRUE(
      frame_document->GetLayoutView()->Layer()->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(
      GetDocument().GetLayoutView()->Layer()->SelfOrDescendantNeedsRepaint());

  frame_document->View()->BeginLifecycleUpdates();
  commands = CompositeFrame();
  EXPECT_TRUE(commands.Contains(SimCanvas::kRect, "green"));
  EXPECT_FALSE(
      frame_document->GetLayoutView()->Layer()->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(
      GetDocument().GetLayoutView()->Layer()->SelfOrDescendantNeedsRepaint());
}

namespace {

class TestEventListener : public NativeEventListener {
 public:
  TestEventListener() = default;
  void Invoke(ExecutionContext*, Event*) final { count_++; }
  int GetCallCount() const { return count_; }

 private:
  int count_ = 0;
};

}  // namespace

TEST_P(FrameThrottlingTest, ThrottledIframeGetsResizeEvents) {
  WebView().GetSettings()->SetJavaScriptEnabled(true);

  // Set up child-iframe that can be throttled and make sure it still gets a
  // resize event when it loads.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://sub.example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");

  // The frame is initially throttled.
  main_resource.Complete(R"HTML(
      <iframe id="frame" src="https://sub.example.com/iframe.html"
              style="margin-top: 2000px"></iframe>
  )HTML");
  frame_resource.Complete(R"HTML(
    Hello, world!
  )HTML");

  // Load and verify throttling.
  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* listener = MakeGarbageCollected<TestEventListener>();
  frame_element->contentWindow()->addEventListener(event_type_names::kResize,
                                                   listener,
                                                   /*use_capture=*/false);
  EXPECT_EQ(listener->GetCallCount(), 0);
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_EQ(listener->GetCallCount(), 1);

  // Composite a second frame to pick up the resize.
  frame_element->SetInlineStyleProperty(CSSPropertyID::kWidth, "200px");
  // This should trigger resize event without clearing NeedsBeginMainFrame().
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  CompositeFrame();
  EXPECT_EQ(listener->GetCallCount(), 2);

  frame_element->contentWindow()->removeEventListener(event_type_names::kResize,
                                                      listener,
                                                      /*use_capture=*/false);
}

TEST_P(FrameThrottlingTest, AncestorTouchActionAndWheelEventHandlers) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  // The frame is initially throttled.
  main_resource.Complete(R"HTML(
    <div id="parent">
      <iframe id="frame" sandbox src="iframe.html"></iframe>
    </div>
  )HTML");
  frame_resource.Complete("<div id='child'></div>");
  CompositeFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();
  auto* parent = GetDocument().getElementById(AtomicString("parent"));
  auto* parent_object = parent->GetLayoutObject();
  auto* child_layout_view = frame_document->GetLayoutView();
  auto* child = frame_document->getElementById(AtomicString("child"));
  auto* child_object = child->GetLayoutObject();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(parent_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(parent_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(child_layout_view->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(child_layout_view->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(child_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(child_object->InsideBlockingWheelEventHandler());

  // Moving the child fully outside the parent makes it invisible.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRenderingForTest());

  auto* handler = MakeGarbageCollected<EmptyEventListener>();
  parent->addEventListener(event_type_names::kTouchstart, handler);
  parent->addEventListener(event_type_names::kWheel, handler);
  EXPECT_TRUE(parent_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(parent_object->BlockingWheelEventHandlerChanged());
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(parent_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(parent_object->InsideBlockingWheelEventHandler());
  // Event handler status update is pending in the throttled frame.
  EXPECT_TRUE(child_layout_view->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(child_layout_view->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(child_layout_view->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(child_layout_view->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(child_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(child_object->InsideBlockingWheelEventHandler());

  // Move the child back to the visible viewport.
  frame_element->setAttribute(
      kStyleAttr, AtomicString("transform: translate(-50px, 0px, 0px)"));
  // Update throttling, which will schedule visual update on unthrottling of the
  // frame.
  CompositeFrame();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());
  CompositeFrame();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(parent_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(parent_object->InsideBlockingWheelEventHandler());
  // Event handler status is updated in the unthrottled frame.
  EXPECT_FALSE(child_layout_view->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(child_layout_view->BlockingWheelEventHandlerChanged());
  EXPECT_TRUE(child_layout_view->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(child_layout_view->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(child_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(child_object->InsideBlockingWheelEventHandler());
}

TEST_P(FrameThrottlingTest, DescendantTouchActionAndWheelEventHandlers) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  // The frame is initially throttled.
  main_resource.Complete(R"HTML(
    <div id="parent">
      <iframe id="frame" sandbox src="iframe.html"></iframe>
    </div>
  )HTML");
  frame_resource.Complete("<div id='child'></div>");
  CompositeFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();
  auto* parent = GetDocument().getElementById(AtomicString("parent"));
  auto* parent_object = parent->GetLayoutObject();
  auto* child_layout_view = frame_document->GetLayoutView();
  auto* child = frame_document->getElementById(AtomicString("child"));
  auto* child_object = child->GetLayoutObject();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(parent_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(parent_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(child_layout_view->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(child_layout_view->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(child_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(child_object->InsideBlockingWheelEventHandler());

  // Moving the child fully outside the parent makes it invisible.
  frame_element->setAttribute(kStyleAttr,
                              AtomicString("transform: translateY(480px)"));
  CompositeFrame();
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRenderingForTest());

  auto* handler = MakeGarbageCollected<EmptyEventListener>();
  child->addEventListener(event_type_names::kTouchstart, handler);
  child->addEventListener(event_type_names::kWheel, handler);
  EXPECT_TRUE(child_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(child_object->BlockingWheelEventHandlerChanged());
  EXPECT_TRUE(
      child_layout_view->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(child_layout_view->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(parent_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(parent_object->DescendantBlockingWheelEventHandlerChanged());
  UpdateAllLifecyclePhases();
  // Event handler status update is pending in the throttled frame.
  EXPECT_TRUE(child_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(child_object->BlockingWheelEventHandlerChanged());
  EXPECT_TRUE(
      child_layout_view->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(child_layout_view->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(child_layout_view->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(child_layout_view->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(child_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(child_object->InsideBlockingWheelEventHandler());

  // Move the child back to the visible viewport.
  frame_element->setAttribute(
      kStyleAttr, AtomicString("transform: translate(-50px, 0px, 0px)"));
  // Update throttling, which will schedule visual update on unthrottling of the
  // frame.
  CompositeFrame();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());
  CompositeFrame();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());
  // Event handler status is updated in the unthrottled frame.
  EXPECT_FALSE(child_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(child_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(
      child_layout_view->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(child_layout_view->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(child_layout_view->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(child_layout_view->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(child_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(child_object->InsideBlockingWheelEventHandler());
}

namespace {

class TestResizeObserverDelegate : public ResizeObserver::Delegate {
 public:
  explicit TestResizeObserverDelegate() {}
  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    entries[0]->target()->SetInlineStyleProperty(CSSPropertyID::kWidth,
                                                 "100px");
  }
};

}  // namespace

TEST_P(FrameThrottlingTest, ForceUnthrottled) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe id="frame" sandbox src="iframe.html"
        style="border:0;transform:translateY(480px)">
  )HTML");
  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <div style="width:120px">Hello, world!</div>
  )HTML");
  CompositeFrame();
  HTMLIFrameElement* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  LocalFrameView* inner_frame_view =
      To<LocalFrameView>(frame_element->OwnedEmbeddedContentView());
  EXPECT_TRUE(inner_frame_view->ShouldThrottleRenderingForTest());

  IntersectionObserverInit* intersection_init =
      IntersectionObserverInit::Create();
  TestIntersectionObserverDelegate* intersection_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(
          *frame_element->contentDocument());
  IntersectionObserver* intersection_observer = IntersectionObserver::Create(
      intersection_init, *intersection_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver);
  intersection_observer->observe(frame_element->contentDocument()->body());

  ResizeObserver::Delegate* resize_delegate =
      MakeGarbageCollected<TestResizeObserverDelegate>();
  ResizeObserver* resize_observer =
      ResizeObserver::Create(&Window(), resize_delegate);
  resize_observer->observe(frame_element);

  // Apply style change here to ensure ResizeObserver will force a second pass
  // through the lifecycle loop on the next update.
  frame_element->SetInlineStyleProperty(CSSPropertyID::kWidth, "200px");

  // Because there is a new IntersectionObserver target, the iframe will be
  // force-unthrottled going into the lifecycle update. During the first pass
  // through the lifecycle loop, the style change will cause the ResizeObserver
  // callback to run. The ResizeObserver will dirty the iframe element by
  // setting its width to 100px. At this point, the lifecycle state of the
  // iframe will be kPrePaintClean, which will cause ShouldThrottleRendering()
  // to return true.
  //
  // Because ResizeObserver dirtied layout, there will be a second pass through
  // the main lifecycle loop. When the iframe element runs layout again,
  // setting its width to 100px, it will cause the iframe's contents to
  // overflow, so the iframe will add a horizontal scrollbar and mark its
  // LayoutView as needing paint property update. If the iframe's lifecycle
  // state is still kPrePaintClean, then it will skip pre-paint on the second
  // pass through the lifecycle loop, leaving its paint properties in a dirty
  // state (bad). If, however, the iframe's lifecycle state is reset to
  // kVisualUpdatePending prior to the second pass through the loop, then it
  // will be once again force-unthrottled, and will run lifecycle steps up
  // through pre-paint (good).
  CompositeFrame();

  EXPECT_TRUE(inner_frame_view->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(inner_frame_view->GetLayoutView()->NeedsPaintPropertyUpdate());
}

TEST_P(FrameThrottlingTest, CullRectUpdate) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  // The frame is initially throttled.
  main_resource.Complete(R"HTML(
    <style>#clip { width: 100px; height: 100px; overflow: hidden }</style>
    <div id="container" style="transform: translateY(480px)">
      <div id="clip">
        <iframe id="frame" sandbox src="iframe.html"
                style="width: 400px; height: 400px; border: none"></iframe>
      </div>
    </div>
  )HTML");
  frame_resource.Complete("");
  CompositeFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_object = frame_element->GetLayoutBox();
  auto* frame_document = frame_element->contentDocument();
  auto* child_layout_view = frame_document->GetLayoutView();

  EXPECT_TRUE(frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            frame_object->FirstFragment().GetCullRect().Rect());
  EXPECT_FALSE(child_layout_view->Layer()->NeedsCullRectUpdate());

  // Change clip. |frame_element| should update its cull rect.
  // |child_layout_view|'s cull rect update is pending.
  GetDocument()
      .getElementById(AtomicString("clip"))
      ->setAttribute(kStyleAttr, AtomicString("width: 630px"));
  CompositeFrame();
  EXPECT_EQ(gfx::Rect(0, 0, 630, 100),
            frame_object->FirstFragment().GetCullRect().Rect());
  EXPECT_TRUE(child_layout_view->Layer()->NeedsCullRectUpdate());

  // Move the frame into the visible viewport.
  GetDocument()
      .getElementById(AtomicString("container"))
      ->setAttribute(kStyleAttr, AtomicString("transform: translate(0)"));
  // Update throttling, which will schedule visual update on unthrottling of the
  // frame.
  CompositeFrame();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_EQ(gfx::Rect(0, 0, 630, 100),
            frame_object->FirstFragment().GetCullRect().Rect());
  EXPECT_TRUE(child_layout_view->Layer()->NeedsCullRectUpdate());

  // The frame is unthrottled.
  CompositeFrame();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_EQ(gfx::Rect(0, 0, 630, 100),
            frame_object->FirstFragment().GetCullRect().Rect());
  EXPECT_EQ(gfx::Rect(0, 0, 630, 100),
            child_layout_view->FirstFragment().GetCullRect().Rect());
  EXPECT_FALSE(child_layout_view->Layer()->NeedsCullRectUpdate());
}

TEST_P(FrameThrottlingTest, ClearPaintArtifactOnThrottlingLocalRoot) {
  InitializeRemote();
  LocalFrameRoot().FrameWidget()->Resize({640, 480});
  SimRequest local_root_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  local_root_resource.Complete(
      "<div style='will-change:transform'>Hello, world!</div>");
  CompositeFrame();
  LocalFrameView* view = LocalFrameRoot().GetFrame()->View();
  Element* div =
      view->GetFrame().GetDocument()->QuerySelector(AtomicString("div"));
  EXPECT_FALSE(view->GetPaintControllerPersistentDataForTesting()
                   .GetPaintArtifact()
                   .IsEmpty());

  // This emulates javascript.
  div->setAttribute(html_names::kStyleAttr, g_empty_atom);
  div->GetBoundingClientRect();
  // This emulates WebFrameWidgetImpl::UpdateRenderThrottlingStatusForSubFrame.
  view->UpdateRenderThrottlingStatus(true, false, false, true);
  // UpdateRenderThrottlingStatus should have cleared out previous paint
  // results.
  EXPECT_TRUE(view->GetPaintControllerPersistentDataForTesting()
                  .GetPaintArtifact()
                  .IsEmpty());
}

TEST_P(FrameThrottlingTest, PrintThrottledFrame) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  // The frame is initially throttled.
  main_resource.Complete(R"HTML(
    <div style="height: 2000px"></div>
    <iframe id="frame" sandbox src="iframe.html"></iframe>
  )HTML");
  frame_resource.Complete("ABC");
  CompositeFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* sub_frame = To<LocalFrame>(frame_element->ContentFrame());
  EXPECT_TRUE(sub_frame->View()->ShouldThrottleRenderingForTest());
  auto* web_frame = WebLocalFrameImpl::FromFrame(sub_frame);
  WebPrintParams print_params(gfx::SizeF(500, 500));
  web_frame->PrintBegin(print_params, blink::WebNode());
  cc::PaintRecorder recorder;
  web_frame->PrintPage(0, recorder.beginRecording());
  auto record = recorder.finishRecordingAsPicture();
  String record_string = RecordAsDebugString(record);
  EXPECT_TRUE(record_string.Contains("drawTextBlob")) << record_string.Utf8();
  web_frame->PrintEnd();
}

}  // namespace blink
