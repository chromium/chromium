// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/test/bind_test_util.h"
#include "cc/layers/picture_layer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
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

class FrameThrottlingTest : public PaintTestConfigurations, public SimTest {
 protected:
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
    GetDocument().View()->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(FrameThrottlingTest);

TEST_P(FrameThrottlingTest, ThrottleInvisibleFrames) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe sandbox id=frame></iframe>");

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();

  // Initially both frames are visible.
  EXPECT_FALSE(GetDocument().View()->IsHiddenForThrottling());
  EXPECT_FALSE(frame_document->View()->IsHiddenForThrottling());

  // Moving the child fully outside the parent makes it invisible.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();
  EXPECT_FALSE(GetDocument().View()->IsHiddenForThrottling());
  EXPECT_TRUE(frame_document->View()->IsHiddenForThrottling());

  // A partially visible child is considered visible.
  frame_element->setAttribute(kStyleAttr,
                              "transform: translate(-50px, 0px, 0px)");
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

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();

  auto* inner_frame_element =
      To<HTMLIFrameElement>(frame_document->getElementById("innerFrame"));
  auto* inner_frame_document = inner_frame_element->contentDocument();

  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(inner_frame_document->View()->CanThrottleRendering());

  // Hidden same origin frames are not throttled.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
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

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();

  auto* inner_frame_element =
      To<HTMLIFrameElement>(frame_document->getElementById("innerFrame"));
  auto* inner_frame_document = inner_frame_element->contentDocument();

  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(inner_frame_document->View()->CanThrottleRendering());

  // Hidden cross origin frames are throttled.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
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

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();

  auto* inner_frame_element =
      To<HTMLIFrameElement>(frame_document->getElementById("innerFrame"));
  auto* inner_frame_document = inner_frame_element->contentDocument();

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      GetDocument().Lifecycle());

  // Hidden cross origin frames are throttled.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();
  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_TRUE(inner_frame_document->View()->ShouldThrottleRendering());

  // An intersection observation overrides throttling during a lifecycle update.
  inner_frame_document->View()->SetIntersectionObservationState(
      LocalFrameView::kRequired);
  {
    GetDocument().GetFrame()->View()->SetInLifecycleUpdateForTest(true);
    EXPECT_FALSE(inner_frame_document->View()->ShouldThrottleRendering());
    GetDocument().GetFrame()->View()->SetInLifecycleUpdateForTest(false);
  }

  inner_frame_document->View()->ScheduleAnimation();

  LayoutView* inner_view = inner_frame_document->View()->GetLayoutView();

  inner_view->SetNeedsLayout("test");
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    inner_view->Compositor()->SetNeedsCompositingUpdate(
        kCompositingUpdateRebuildTree);
  }
  inner_view->SetShouldDoFullPaintInvalidation(
      PaintInvalidationReason::kForTesting);
  inner_view->Layer()->SetNeedsRepaint();
  EXPECT_TRUE(inner_frame_document->View()
                  ->GetLayoutView()
                  ->ShouldDoFullPaintInvalidation());
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    inner_view->Compositor()->SetNeedsCompositingUpdate(
        kCompositingUpdateRebuildTree);
    EXPECT_EQ(kCompositingUpdateRebuildTree,
              inner_view->Compositor()->pending_update_type_);
  }
  EXPECT_TRUE(inner_view->Layer()->SelfNeedsRepaint());

  CompositeFrame();
  // The lifecycle update should only be overridden for one frame.
  EXPECT_TRUE(inner_frame_document->View()->ShouldThrottleRendering());

  EXPECT_FALSE(inner_view->NeedsLayout());
  EXPECT_TRUE(inner_frame_document->View()
                  ->GetLayoutView()
                  ->ShouldDoFullPaintInvalidation());
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(kCompositingUpdateRebuildTree,
              inner_view->Compositor()->pending_update_type_);
  }
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

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      GetDocument().Lifecycle());

  // Move both frames offscreen to make them throttled.
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* child_frame_element = To<HTMLIFrameElement>(
      frame_element->contentDocument()->getElementById("child-frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");

  CompositeFrame();

  auto* root_view = LocalFrameRoot().GetFrame()->View();
  ASSERT_FALSE(root_view->ShouldThrottleRendering());
  auto* frame_view = frame_element->contentDocument()->View();
  ASSERT_TRUE(frame_view->ShouldThrottleRendering());
  auto* child_view = child_frame_element->contentDocument()->View();
  ASSERT_TRUE(child_view->ShouldThrottleRendering());

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
  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      root_frame->GetDocument()->Lifecycle());
  auto* root_frame_view = root_frame->View();
  root_frame_view->SetNeedsLayout();
  root_frame_view->ScheduleAnimation();
  root_frame_view->SetLifecycleUpdatesThrottledForTesting(true);
  ASSERT_TRUE(root_frame->IsLocalRoot());
  ASSERT_TRUE(root_frame_view->ShouldThrottleRendering());

  auto* child_frame_document =
      To<HTMLIFrameElement>(root_frame->GetDocument()->getElementById("frame"))
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

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      GetDocument().Lifecycle());
  CompositeFrame();

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();
  // Hidden cross origin frames are throttled.
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRendering());

  // An intersection observation overrides throttling, but this is only during
  // the lifecycle.
  frame_document->View()->SetIntersectionObservationState(
      LocalFrameView::kRequired);
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRendering());
  {
    GetDocument().GetFrame()->View()->SetInLifecycleUpdateForTest(true);
    EXPECT_FALSE(frame_document->View()->ShouldThrottleRendering());
    GetDocument().GetFrame()->View()->SetInLifecycleUpdateForTest(false);
  }

  // A lifecycle update can update the throttled frame to just LayoutClean but
  // the frame should still be considered throttled outside the lifecycle
  // because it is not fully running the lifecycle.
  frame_document->View()->GetLayoutView()->SetNeedsLayout("test");
  frame_document->View()->ScheduleAnimation();
  frame_document->View()->GetLayoutView()->Layer()->SetNeedsRepaint();
  CompositeFrame();
  EXPECT_EQ(DocumentLifecycle::kLayoutClean,
            frame_document->Lifecycle().GetState());
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRendering());
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

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      GetDocument().Lifecycle());
  CompositeFrame();

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();
  // Hidden cross origin frames are throttled.
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRendering());
  // Main frame is not throttled.
  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRendering());

  unsigned throttled_count = 0;
  auto throttled_callback = base::BindLambdaForTesting(
      [&throttled_count](LocalFrameView&) { throttled_count++; });
  GetDocument().View()->ForAllThrottledLocalFrameViewsForTesting(
      throttled_callback);
  EXPECT_EQ(1u, throttled_count);
}

TEST_P(FrameThrottlingTest, HiddenCrossOriginZeroByZeroFramesAreNotThrottled) {
  // Create a document with doubly nested iframes.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame src=iframe.html></iframe>");
  frame_resource.Complete(
      "<iframe id=innerFrame width=0 height=0 sandbox></iframe>");

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();

  auto* inner_frame_element =
      To<HTMLIFrameElement>(frame_document->getElementById("innerFrame"));
  auto* inner_frame_document = inner_frame_element->contentDocument();

  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(inner_frame_document->View()->CanThrottleRendering());

  // The frame is not throttled because its dimensions are 0x0.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();
  EXPECT_FALSE(GetDocument().View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(inner_frame_document->View()->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, ThrottledLifecycleUpdate) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe sandbox id=frame></iframe>");

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();

  // Enable throttling for the child frame.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  EXPECT_EQ(DocumentLifecycle::kPaintClean,
            frame_document->Lifecycle().GetState());

  // Mutating the throttled frame followed by a beginFrame will not result in
  // a complete lifecycle update.
  // TODO(skyostil): these expectations are either wrong, or the test is
  // not exercising the code correctly. PaintClean means the entire lifecycle
  // ran.
  frame_element->setAttribute(html_names::kWidthAttr, "50");
  CompositeFrame();

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(DocumentLifecycle::kPaintClean,
              frame_document->Lifecycle().GetState());

    // A hit test will not force a complete lifecycle update.
    WebView().HitTestResultAt(gfx::PointF());
    EXPECT_EQ(DocumentLifecycle::kPaintClean,
              frame_document->Lifecycle().GetState());
  } else {
    // TODO(chrishtr): fix this test by manually resetting to
    // kVisualUpdatePending before call to CompositeFrame.
    EXPECT_EQ(DocumentLifecycle::kPaintClean,
              frame_document->Lifecycle().GetState());

    // A hit test will not force a complete lifecycle update.
    WebView().HitTestResultAt(gfx::PointF());
    EXPECT_EQ(DocumentLifecycle::kPaintClean,
              frame_document->Lifecycle().GetState());
  }
}

TEST_P(FrameThrottlingTest, UnthrottlingFrameSchedulesAnimation) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe sandbox id=frame></iframe>");

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));

  // First make the child hidden to enable throttling.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  // Then bring it back on-screen. This should schedule an animation update.
  frame_element->setAttribute(kStyleAttr, "");
  CompositeFrame();
  EXPECT_TRUE(Compositor().NeedsBeginFrame());
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

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));

  // Move the frame offscreen to throttle it.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Mutating the throttled frame should not cause an animation to be scheduled.
  frame_element->contentDocument()->documentElement()->setAttribute(
      kStyleAttr, "background: green");
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  // Move the frame back on screen to unthrottle it.
  frame_element->setAttribute(kStyleAttr, "");
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

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));

  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();

  // Change the size of a div in the throttled frame.
  auto* div_element = frame_element->contentDocument()->getElementById("div");
  div_element->setAttribute(kStyleAttr, "width: 50px");

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
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
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
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
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
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Change the background color of the frame's contents from red to green.
  frame_element->contentDocument()->body()->setAttribute(kStyleAttr,
                                                         "background: green");

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

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));

  CompositeFrame();

  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_TRUE(
      frame_element->contentDocument()->GetFrame()->IsCrossOriginToMainFrame());
  EXPECT_FALSE(frame_element->contentDocument()
                   ->View()
                   ->GetLayoutView()
                   ->NeedsPaintPropertyUpdate());

  NonThrowableExceptionState exception_state;

  // Security policy requires setting domain on both frames.
  GetDocument().setDomain(String("example.com"), exception_state);
  frame_element->contentDocument()->setDomain(String("example.com"),
                                              exception_state);

  EXPECT_FALSE(
      frame_element->contentDocument()->GetFrame()->IsCrossOriginToMainFrame());
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

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  EXPECT_TRUE(frame_document->GetFrame()->IsCrossOriginToMainFrame());
  EXPECT_FALSE(
      frame_document->View()->GetLayoutView()->NeedsPaintPropertyUpdate());

  // Set the domain of the child frame first which should be a no-op in terms of
  // cross-origin status changes.
  NonThrowableExceptionState exception_state;
  frame_element->contentDocument()->setDomain(String("example.com"),
                                              exception_state);
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  EXPECT_TRUE(frame_document->GetFrame()->IsCrossOriginToMainFrame());
  EXPECT_FALSE(
      frame_document->View()->GetLayoutView()->NeedsPaintPropertyUpdate());

  // Then change the main frame origin which needs to invalidate the newly
  // cross-origin child.
  GetDocument().setDomain(String("example.com"), exception_state);
  EXPECT_FALSE(frame_document->GetFrame()->IsCrossOriginToMainFrame());
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
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Give the frame focus and do another composite. The selection in the
  // compositor should be cleared because the frame is throttled.
  EXPECT_FALSE(Compositor().HasSelection());
  GetDocument().GetPage()->GetFocusController().SetFocusedFrame(
      frame_element->contentDocument()->GetFrame());
  GetDocument().body()->setAttribute(kStyleAttr, "background: green");
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
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Change style of the frame's content to make it in VisualUpdatePending
  // state.
  frame_element->contentDocument()->body()->setAttribute(kStyleAttr,
                                                         "background: green");
  // Change root frame's layout so that the next lifecycle update will call
  // ScrollingCoordinator::UpdateAfterPaint().
  GetDocument().body()->setAttribute(kStyleAttr, "margin: 20px");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            frame_element->contentDocument()->Lifecycle().GetState());

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      GetDocument().Lifecycle());
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
  frame_element->setAttribute(kStyleAttr, "transform: translateY(0px)");
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
  WebView().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);

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
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Change style of the frame's content to make it in VisualUpdatePending
  // state.
  frame_element->contentDocument()->body()->setAttribute(kStyleAttr,
                                                         "background: green");
  // Change root frame's layout so that the next lifecycle update will call
  // ScrollingCoordinator::UpdateAfterPaint().
  GetDocument().body()->setAttribute(kStyleAttr, "margin: 20px");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            frame_element->contentDocument()->Lifecycle().GetState());

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      GetDocument().Lifecycle());
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
  WebView().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);

  // Create a hidden frame which is throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete("<div style='height: 2000px'></div>");

  // Move the frame offscreen to throttle it.
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Change style of the frame's content to make it in VisualUpdatePending
  // state.
  frame_element->contentDocument()->body()->setAttribute(kStyleAttr,
                                                         "background: green");
  // Change root frame's layout so that the next lifecycle update will call
  // ScrollingCoordinator::UpdateAfterPaint().
  GetDocument().body()->setAttribute(kStyleAttr, "margin: 20px");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            frame_element->contentDocument()->Lifecycle().GetState());

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      GetDocument().Lifecycle());
  // This will call ScrollingCoordinator::UpdateAfterPaint() and should not
  // cause an assert failure about isAllowedToQueryCompositingState() in the
  // throttled frame.
  CompositeFrame();
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            frame_element->contentDocument()->Lifecycle().GetState());

  // Make the frame visible by changing its transform. This doesn't cause a
  // layout, but should still unthrottle the frame.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(0px)");
  CompositeFrame();  // Unthrottle the frame.

  EXPECT_FALSE(
      frame_element->contentDocument()->View()->ShouldThrottleRendering());
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
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // Make the frame visible by changing its transform. This doesn't cause a
  // layout, but should still unthrottle the frame.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(0px)");
  CompositeFrame();
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, ThrottledTopLevelEventHandlerIgnored) {
  WebView().GetSettings()->SetJavaScriptEnabled(true);
  EXPECT_EQ(0u, TouchHandlerRegionSize());

  // Create a frame which is throttled and has two different types of
  // top-level touchstart handlers.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "<iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
  frame_resource.Complete(R"HTML(
    <script>
    window.addEventListener('touchstart', function(){}, {passive: false});
    document.addEventListener('touchstart', function(){}, {passive: false});
    </script>
  )HTML");
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      GetDocument().Lifecycle());
  CompositeFrame();  // Throttle the frame.

  // In here, throttle iframe doesn't throttle the main frame.
  EXPECT_TRUE(
      frame_element->contentDocument()->View()->ShouldThrottleRendering());
  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRendering());

  // In this test, the iframe has the same origin as the main frame, so we have
  // two documents but one graphics layer tree. The test throttles the iframe
  // document only. In ScrollingCoordinator::UpdateLayerTouchActionRects, we
  // check whether the document associated with a certain grahpics layer is
  // throttled or not. Since the layers are associated with the main document
  // which is not throttled, we expect the main document to have one touch
  // handler region.
  EXPECT_EQ(1u, TouchHandlerRegionSize());

  // Unthrottling the frame makes the touch handlers active again. Note that
  // both handlers get combined into the same rectangle in the region, so
  // there is only one rectangle in total.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(0px)");
  CompositeFrame();  // Unthrottle the frame.
  EXPECT_EQ(1u, TouchHandlerRegionSize());
}

TEST_P(FrameThrottlingTest, ThrottledEventHandlerIgnored) {
  WebView().GetSettings()->SetJavaScriptEnabled(true);
  EXPECT_EQ(0u, TouchHandlerRegionSize());

  // Create a frame which is throttled and has a non-top-level touchstart
  // handler.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "<iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
  frame_resource.Complete(R"HTML(
    <div id=d>touch handler</div>
    <script>
    document.querySelector('#d').addEventListener('touchstart',
    function(){});
    </script>
  )HTML");
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      GetDocument().Lifecycle());
  CompositeFrame();  // Throttle the frame.

  // In here, throttle iframe doesn't throttle the main frame.
  EXPECT_TRUE(
      frame_element->contentDocument()->View()->ShouldThrottleRendering());
  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRendering());

  // In this test, the iframe has the same origin as the main frame, so we have
  // two documents but one graphics layer tree. The test throttles the iframe
  // document only. In ScrollingCoordinator::UpdateLayerTouchActionRects, we
  // check whether the document associated with a certain grahpics layer is
  // throttled or not. Since the layers are associated with the main document
  // which is not throttled, we expect the main document to have one touch
  // handler region.
  EXPECT_EQ(1u, TouchHandlerRegionSize());

  // Unthrottling the frame makes the touch handler active again.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(0px)");
  CompositeFrame();  // Unthrottle the frame.
  EXPECT_EQ(1u, TouchHandlerRegionSize());
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
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  ClassicScript::CreateUnspecifiedScript(
      ScriptSourceCode("document.body.innerHTML = 'throttled'"))
      ->RunScript(To<LocalDOMWindow>(frame_element->contentWindow()));
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  // The dumped contents should not include the throttled frame.
  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      GetDocument().Lifecycle());
  WebString result = WebFrameContentDumper::DumpWebViewAsText(&WebView(), 1024);
  EXPECT_NE(std::string::npos, result.Utf8().find("main"));
  EXPECT_EQ(std::string::npos, result.Utf8().find("throttled"));
}

TEST_P(FrameThrottlingTest, PaintingViaGraphicsLayerIsThrottled) {
  // GraphicsLayers are not created with CompositeAfterPaint.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  WebView().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);

  // Create a hidden frame which is throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete("throttled");

  // Before the iframe is throttled, we should create all drawing items.
  auto commands_not_throttled = CompositeFrame();
  EXPECT_EQ(6u, commands_not_throttled.DrawCount());

  // Move the frame offscreen to throttle it and make sure it is backed by a
  // graphics layer.
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr,
                              "transform: translateY(480px) translateZ(0px)");
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  // If painting of the iframe is throttled, we should only receive drawing
  // commands for the main frame.
  auto commands_throttled = Compositor().PaintFrame();
  EXPECT_EQ(5u, commands_throttled.DrawCount());
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
}

TEST_P(FrameThrottlingTest, ThrottleInnerCompositedLayer) {
  WebView().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);

  // Create a hidden frame which is throttled.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete(
      "<div id=div style='will-change: transform; background: blue'>DIV</div>");
  auto commands_not_throttled = CompositeFrame();

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* root_layer = WebView().MainFrameImpl()->GetFrameView()->RootCcLayer();
  // The inner div is composited.
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "div").size());

  // Before the iframe is throttled, we should create all drawing commands.
  unsigned full_draw_count = 7u;
  EXPECT_EQ(full_draw_count, commands_not_throttled.DrawCount());

  // Move the frame offscreen to throttle it.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  // The inner div should still be composited.
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "div").size());

  // If painting of the iframe is throttled, we should only receive drawing
  // commands for the main frame.
  auto commands_throttled = Compositor().PaintFrame();
  EXPECT_LT(commands_throttled.DrawCount(), full_draw_count);

  // Remove compositing trigger of inner_div.
  auto* inner_div = frame_element->contentDocument()->getElementById("div");
  inner_div->setAttribute(kStyleAttr, "background: yellow; overflow: hidden");
  // Do an unthrottled style and layout update, simulating the situation
  // triggered by script style/layout access.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  {
    // And a throttled full lifecycle update.
    DocumentLifecycle::AllowThrottlingScope throttling_scope(
        GetDocument().Lifecycle());
    UpdateAllLifecyclePhases();
  }
  // The inner div is no longer composited.
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "div").size());

  auto commands_throttled1 = CompositeFrame();
  EXPECT_LT(commands_throttled1.DrawCount(), full_draw_count);

  // Move the frame back on screen.
  frame_element->setAttribute(kStyleAttr, "");
  CompositeFrame();
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
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
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* child_frame_element = To<HTMLIFrameElement>(
      frame_element->contentDocument()->getElementById("child-frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  Compositor().BeginFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_TRUE(
      child_frame_element->contentDocument()->View()->CanThrottleRendering());

  // Move the frame back on screen.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(0px)");
  Compositor().BeginFrame();
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_FALSE(
      child_frame_element->contentDocument()->View()->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, SkipPaintingLayersInThrottledFrames) {
  WebView().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);

  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete(
      "<div id=div style='transform: translateZ(0); background: "
      "red'>layer</div>");
  auto commands = CompositeFrame();
  EXPECT_TRUE(commands.Contains(SimCanvas::kRect, "red"));

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  auto* frame_document = frame_element->contentDocument();
  EXPECT_EQ(DocumentLifecycle::kPaintClean,
            frame_document->Lifecycle().GetState());

  // Simulate the paint for a graphics layer being externally invalidated
  // (e.g., by video playback).
  frame_document->View()
      ->GetLayoutView()
      ->InvalidatePaintForViewAndCompositedLayers();

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
  auto* first_frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("first"));
  first_frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();
  EXPECT_TRUE(
      first_frame_element->contentDocument()->View()->CanThrottleRendering());

  // Run a animation frame callback in the second frame which mutates the
  // contents of the first frame and causes a synchronous style update. This
  // should not result in an unexpected lifecycle state even if the first
  // frame is throttled during the animation frame callback.
  auto* second_frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("second"));
  ClassicScript::CreateUnspecifiedScript(
      ScriptSourceCode(
          "window.requestAnimationFrame(function() {\n"
          "  var throttledFrame = window.parent.frames.first;\n"
          "  throttledFrame.document.documentElement.style = 'margin: 50px';\n"
          "  "
          "throttledFrame.document.querySelector('#d').getBoundingClientRect();"
          "\n"
          "});\n"))
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

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> result =
      ClassicScript::CreateUnspecifiedScript(ScriptSourceCode("window.didRaf;"))
          ->RunScriptAndReturnValue(
              To<LocalDOMWindow>(frame_element->contentWindow()));
  EXPECT_TRUE(result->IsTrue());
}

TEST_P(FrameThrottlingTest, UpdatePaintPropertiesOnUnthrottling) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frame_resource.Complete("<div id='div'>Inner</div>");
  CompositeFrame();

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();
  auto* inner_div = frame_document->getElementById("div");
  auto* inner_div_object = inner_div->GetLayoutObject();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRendering());

  frame_element->setAttribute(html_names::kStyleAttr,
                              "transform: translateY(1000px)");
  CompositeFrame();
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(inner_div_object->FirstFragment().PaintProperties());

  // Mutating the throttled frame should not cause paint property update.
  inner_div->setAttribute(html_names::kStyleAttr,
                          "transform: translateY(20px)");
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  {
    DocumentLifecycle::AllowThrottlingScope throttling_scope(
        GetDocument().Lifecycle());
    UpdateAllLifecyclePhases();
  }
  EXPECT_FALSE(inner_div_object->FirstFragment().PaintProperties());

  // Move the frame back on screen to unthrottle it.
  frame_element->setAttribute(html_names::kStyleAttr, "");
  // The first update unthrottles the frame, the second actually update layout
  // and paint properties etc.
  CompositeFrame();
  CompositeFrame();
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
  EXPECT_EQ(FloatSize(0, 20), inner_div->GetLayoutObject()
                                  ->FirstFragment()
                                  .PaintProperties()
                                  ->Transform()
                                  ->Translation2D());
}

TEST_P(FrameThrottlingTest, DisplayNoneNotThrottled) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "<style>iframe { transform: translateY(480px); }</style>"
      "<iframe sandbox id=frame></iframe>");

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();

  // Initially the frame is throttled as it is offscreen.
  CompositeFrame();
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());

  // Setting display:none unthrottles the frame.
  frame_element->setAttribute(kStyleAttr, "display: none");
  CompositeFrame();
  EXPECT_FALSE(frame_document->View()->CanThrottleRendering());
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
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* child_frame_element = To<HTMLIFrameElement>(
      frame_element->contentDocument()->getElementById("child-frame"));
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();
  EXPECT_TRUE(frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_TRUE(
      child_frame_element->contentDocument()->View()->CanThrottleRendering());

  // Setting display:none for the parent frame unthrottles the parent but not
  // the child. This behavior matches Safari.
  frame_element->setAttribute(kStyleAttr, "display: none");
  CompositeFrame();
  EXPECT_FALSE(
      frame_element->contentDocument()->View()->CanThrottleRendering());
  EXPECT_TRUE(
      child_frame_element->contentDocument()->View()->CanThrottleRendering());
}

TEST_P(FrameThrottlingTest, RebuildCompositedLayerTreeOnLayerRemoval) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  // This test verifies removal of PaintLayer due to style change will force
  // unthrottling a frame. This is because destructing PaintLayer would cause
  // CompositedLayerMapping and composited layers to be destructed and detach
  // from layer tree immediately. Layers could have dangling scroll/clip
  // parent if compositing update were omitted.
  WebView().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);

  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<iframe sandbox id='frame' src='iframe.html' style='position:relative; "
      "top:1000px;'></iframe>");
  frame_resource.Complete(R"HTML(
    <div id='scroller' style='overflow:scroll; width:300px; height:200px;'>
      <div style='height:1000px;'></div>
      <div id='sibling' style='transform:translateZ(0);'>Foo</div>
    </div>
  )HTML");

  CompositeFrame();
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  {
    DocumentLifecycle::AllowThrottlingScope throttling_scope(
        GetDocument().Lifecycle());
    EXPECT_TRUE(
        frame_element->contentDocument()->View()->ShouldThrottleRendering());
  }

  auto* scroller_element =
      frame_element->contentDocument()->getElementById("scroller");
  ASSERT_TRUE(scroller_element->GetLayoutObject()->HasLayer());
  auto* scroller_layer =
      ToLayoutBoxModelObject(scroller_element->GetLayoutObject())->Layer();
  EXPECT_TRUE(scroller_layer->NeedsCompositedScrolling());

  auto* sibling_element =
      frame_element->contentDocument()->getElementById("sibling");
  ASSERT_TRUE(sibling_element->GetLayoutObject()->HasLayer());
  auto* sibling_layer =
      ToLayoutBoxModelObject(sibling_element->GetLayoutObject())->Layer();
  auto* sibling_clm = sibling_layer->GetCompositedLayerMapping();
  ASSERT_TRUE(sibling_clm);

  scroller_element->setAttribute(kStyleAttr, "overflow:visible;");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            frame_element->contentDocument()->Lifecycle().GetState());

  // This simulates a javascript query to layout results, e.g.
  // document.body.offsetTop, which will force style & layout to be computed,
  // whether the frame is throttled or not.
  frame_element->contentDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(DocumentLifecycle::kLayoutClean,
            frame_element->contentDocument()->Lifecycle().GetState());
  {
    DocumentLifecycle::AllowThrottlingScope throttling_scope(
        GetDocument().Lifecycle());
    EXPECT_FALSE(
        frame_element->contentDocument()->View()->ShouldThrottleRendering());
  }

  CompositeFrame();
  {
    DocumentLifecycle::AllowThrottlingScope throttling_scope(
        GetDocument().Lifecycle());
    EXPECT_TRUE(
        frame_element->contentDocument()->View()->ShouldThrottleRendering());
  }
  EXPECT_EQ(DocumentLifecycle::kCompositingAssignmentsClean,
            frame_element->contentDocument()->Lifecycle().GetState());
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
  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();
  EXPECT_TRUE(frame_document->View()->CanThrottleRendering());
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRendering());

  frame_document->getElementById("div")->setAttribute(kStyleAttr,
                                                      "will-change: transform");
  GetDocument().View()->UpdateLifecycleToCompositingCleanPlusScrolling(
      DocumentUpdateReason::kTest);

  {
    // Then do a full lifecycle with throttling enabled. This should not crash.
    DocumentLifecycle::AllowThrottlingScope throttling_scope(
        GetDocument().Lifecycle());
    EXPECT_TRUE(frame_document->View()->ShouldThrottleRendering());
    UpdateAllLifecyclePhases();
  }
}

TEST_P(FrameThrottlingTest, GraphicsLayerCollection) {
  // GraphicsLayers are not created with CompositeAfterPaint.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  // The frame is initially throttled.
  main_resource.Complete(
      "<iframe id='frame' sandbox src='iframe.html'></iframe>");
  frame_resource.Complete(
      "<div id='div' style='will-change: transform'>Foo</div>");

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      GetDocument().Lifecycle());
  CompositeFrame();

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRendering());
  auto* paint_controller = GetDocument().View()->GetPaintController();
  ASSERT_NE(nullptr, paint_controller);
  auto display_item_count = paint_controller->GetDisplayItemList().size();

  // Moving the child fully outside the parent makes it invisible.
  frame_element->setAttribute(kStyleAttr, "transform: translateY(480px)");
  CompositeFrame();
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRendering());
  // Change of throttling clears paint controller, to force re-collection of
  // graphics layers in the next frame.
  EXPECT_EQ(nullptr, GetDocument().View()->GetPaintController());

  // Force a frame update. We should re-collect the graphics layers.
  GetDocument().GetPage()->Animator().ScheduleVisualUpdate(
      GetDocument().GetFrame());
  CompositeFrame();
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRendering());
  paint_controller = GetDocument().View()->GetPaintController();
  ASSERT_NE(nullptr, paint_controller);
  // We no longer collect the graphics layers of the iframe and the composited
  // content.
  EXPECT_GT(display_item_count, paint_controller->GetDisplayItemList().size());

  // Move the child back to the visible viewport.
  frame_element->setAttribute(kStyleAttr,
                              "transform: translate(-50px, 0px, 0px)");
  // Update throttling, which will schedule visual update on unthrottling of the
  // frame.
  CompositeFrame();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRendering());
  // Change of throttling clears paint controller, to force re-collection of
  // graphics layers in the next frame.
  EXPECT_EQ(nullptr, GetDocument().View()->GetPaintController());

  CompositeFrame();
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRendering());
  paint_controller = GetDocument().View()->GetPaintController();
  ASSERT_NE(nullptr, paint_controller);
  // Now we should collect all graphics layers again.
  EXPECT_EQ(display_item_count, paint_controller->GetDisplayItemList().size());
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

  ViewportIntersectionState intersection;
  intersection.main_frame_intersection = WebRect(0, 0, 100, 100);
  intersection.main_frame_viewport_size = WebSize(100, 100);
  intersection.viewport_intersection = WebRect(0, 0, 100, 100);
  LocalFrameRoot().FrameWidget()->Resize(gfx::Size(300, 200));
  LocalFrameRoot().FrameWidget()->SetRemoteViewportIntersection(intersection);

  auto* root_frame = LocalFrameRoot().GetFrame();
  auto* frame_document =
      To<HTMLIFrameElement>(root_frame->GetDocument()->getElementById("frame"))
          ->contentDocument();
  auto* frame_view = frame_document->View();
  auto* child_document =
      To<HTMLIFrameElement>(frame_document->getElementById("child-frame"))
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
  EXPECT_EQ(root_frame->RemoteViewportIntersection(), IntRect(0, 0, 100, 100));
  EXPECT_TRUE(root_frame->View()->CanThrottleRenderingForPropagation());
  EXPECT_EQ(root_frame->GetOcclusionState(),
            FrameOcclusionState::kPossiblyOccluded);
  EXPECT_TRUE(frame_view->CanThrottleRendering());
  EXPECT_TRUE(child_view->CanThrottleRendering());
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  // Simulate a trivial style change that doesn't trigger layout, compositing
  // update, but schedules layout tree update.
  frame_document->documentElement()->setAttribute(html_names::kStyleAttr,
                                                  "color: blue");
  // This is needed to reproduce crbug.com/1054644 before the fix.
  frame_view->SetNeedsPaintPropertyUpdate();

  // Show the frame without any other change.
  LocalFrameRoot().WasShown();
  LocalFrameRoot().FrameWidget()->SetRemoteViewportIntersection(intersection);
  CompositeFrame();
  EXPECT_EQ(root_frame->RemoteViewportIntersection(), IntRect(0, 0, 100, 100));
  EXPECT_FALSE(root_frame->View()->CanThrottleRenderingForPropagation());
  EXPECT_NE(root_frame->GetOcclusionState(),
            FrameOcclusionState::kPossiblyOccluded);
  EXPECT_FALSE(frame_view->CanThrottleRendering());
  // The child frame's throtting status is not updated because the parent
  // document has pending visual update.
  EXPECT_TRUE(child_view->CanThrottleRendering());

  CompositeFrame();
  EXPECT_FALSE(frame_view->CanThrottleRendering());
  // The child frame's throttling status should be updated now.
  EXPECT_FALSE(child_view->CanThrottleRendering());
}

}  // namespace blink
