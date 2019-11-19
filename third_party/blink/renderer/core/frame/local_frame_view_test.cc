// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_view.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using blink::test::RunPendingTasks;
using testing::_;
using testing::AnyNumber;

namespace blink {
namespace {

class AnimationMockChromeClient : public RenderingTestChromeClient {
 public:
  AnimationMockChromeClient() : has_scheduled_animation_(false) {}

  // ChromeClient
  MOCK_METHOD2(AttachRootGraphicsLayer,
               void(GraphicsLayer*, LocalFrame* localRoot));
  MOCK_METHOD3(MockSetToolTip, void(LocalFrame*, const String&, TextDirection));
  void SetToolTip(LocalFrame& frame,
                  const String& tooltip_text,
                  TextDirection dir) override {
    MockSetToolTip(&frame, tooltip_text, dir);
  }

  void ScheduleAnimation(const LocalFrameView*,
                         base::TimeDelta = base::TimeDelta()) override {
    has_scheduled_animation_ = true;
  }
  bool has_scheduled_animation_;
};

class LocalFrameViewTest : public RenderingTest {
 protected:
  LocalFrameViewTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        chrome_client_(MakeGarbageCollected<AnimationMockChromeClient>()) {
    EXPECT_CALL(GetAnimationMockChromeClient(), AttachRootGraphicsLayer(_, _))
        .Times(AnyNumber());
  }

  ~LocalFrameViewTest() override {
    testing::Mock::VerifyAndClearExpectations(&GetAnimationMockChromeClient());
  }

  RenderingTestChromeClient& GetChromeClient() const override {
    return *chrome_client_;
  }

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  AnimationMockChromeClient& GetAnimationMockChromeClient() const {
    return *chrome_client_;
  }

 private:
  Persistent<AnimationMockChromeClient> chrome_client_;
};

TEST_F(LocalFrameViewTest, SetPaintInvalidationDuringUpdateAllLifecyclePhases) {
  SetBodyInnerHTML("<div id='a' style='color: blue'>A</div>");
  GetDocument().getElementById("a")->setAttribute(html_names::kStyleAttr,
                                                  "color: green");
  GetAnimationMockChromeClient().has_scheduled_animation_ = false;
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetAnimationMockChromeClient().has_scheduled_animation_);
}

TEST_F(LocalFrameViewTest, SetPaintInvalidationOutOfUpdateAllLifecyclePhases) {
  SetBodyInnerHTML("<div id='a' style='color: blue'>A</div>");
  GetAnimationMockChromeClient().has_scheduled_animation_ = false;
  GetDocument()
      .getElementById("a")
      ->GetLayoutObject()
      ->SetShouldDoFullPaintInvalidation();
  EXPECT_TRUE(GetAnimationMockChromeClient().has_scheduled_animation_);
  GetAnimationMockChromeClient().has_scheduled_animation_ = false;
  GetDocument()
      .getElementById("a")
      ->GetLayoutObject()
      ->SetShouldDoFullPaintInvalidation();
  EXPECT_TRUE(GetAnimationMockChromeClient().has_scheduled_animation_);
  GetAnimationMockChromeClient().has_scheduled_animation_ = false;
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetAnimationMockChromeClient().has_scheduled_animation_);
}

// If we don't hide the tooltip on scroll, it can negatively impact scrolling
// performance. See crbug.com/586852 for details.
TEST_F(LocalFrameViewTest, HideTooltipWhenScrollPositionChanges) {
  SetBodyInnerHTML("<div style='width:1000px;height:1000px'></div>");

  EXPECT_CALL(GetAnimationMockChromeClient(),
              MockSetToolTip(GetDocument().GetFrame(), String(), _));
  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(1, 1),
                                                          kUserScroll);

  // Programmatic scrolling should not dismiss the tooltip, so setToolTip
  // should not be called for this invocation.
  EXPECT_CALL(GetAnimationMockChromeClient(),
              MockSetToolTip(GetDocument().GetFrame(), String(), _))
      .Times(0);
  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(2, 2),
                                                          kProgrammaticScroll);
}

// NoOverflowInIncrementVisuallyNonEmptyPixelCount tests fail if the number of
// pixels is calculated in 32-bit integer, because 65536 * 65536 would become 0
// if it was calculated in 32-bit and thus it would be considered as empty.
TEST_F(LocalFrameViewTest, NoOverflowInIncrementVisuallyNonEmptyPixelCount) {
  EXPECT_FALSE(GetDocument().View()->IsVisuallyNonEmpty());
  GetDocument().View()->IncrementVisuallyNonEmptyPixelCount(
      IntSize(65536, 65536));
  EXPECT_TRUE(GetDocument().View()->IsVisuallyNonEmpty());
}

// This test addresses http://crbug.com/696173, in which a call to
// LocalFrameView::UpdateLayersAndCompositingAfterScrollIfNeeded during layout
// caused a crash as the code was incorrectly assuming that the ancestor
// overflow layer would always be valid.
TEST_F(LocalFrameViewTest,
       ViewportConstrainedObjectsHandledCorrectlyDuringLayout) {
  SetBodyInnerHTML(R"HTML(
    <style>.container { height: 200%; }
    #sticky { position: sticky; top: 0; height: 50px; }</style>
    <div class='container'><div id='sticky'></div></div>
  )HTML");

  LayoutBoxModelObject* sticky = ToLayoutBoxModelObject(
      GetDocument().getElementById("sticky")->GetLayoutObject());

  // Deliberately invalidate the ancestor overflow layer. This approximates
  // http://crbug.com/696173, in which the ancestor overflow layer can be null
  // during layout.
  sticky->Layer()->UpdateAncestorOverflowLayer(nullptr);

  // This call should not crash.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 100),
                                                          kProgrammaticScroll);
}

TEST_F(LocalFrameViewTest, UpdateLifecyclePhasesForPrintingDetachedFrame) {
  SetBodyInnerHTML("<iframe style='display: none'></iframe>");
  SetChildFrameHTML("A");

  ChildDocument().SetPrinting(Document::kPrinting);
  ChildDocument().View()->UpdateLifecyclePhasesForPrinting();

  // The following checks that the detached frame has been walked for PrePaint.
  EXPECT_EQ(DocumentLifecycle::kPrePaintClean,
            GetDocument().Lifecycle().GetState());
  EXPECT_EQ(DocumentLifecycle::kPrePaintClean,
            ChildDocument().Lifecycle().GetState());
  auto* child_layout_view = ChildDocument().GetLayoutView();
  EXPECT_TRUE(child_layout_view->FirstFragment().PaintProperties());
}

TEST_F(LocalFrameViewTest, CanHaveScrollbarsIfScrollingAttrEqualsNoChanged) {
  SetBodyInnerHTML("<iframe scrolling='no'></iframe>");
  EXPECT_FALSE(ChildDocument().View()->CanHaveScrollbars());

  ChildDocument().WillChangeFrameOwnerProperties(0, 0, ScrollbarMode::kAlwaysOn,
                                                 false);
  EXPECT_TRUE(ChildDocument().View()->CanHaveScrollbars());
}

TEST_F(LocalFrameViewTest,
       MainThreadScrollingForBackgroundFixedAttachmentWithCompositing) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);

  SetBodyInnerHTML(R"HTML(
    <style>
      .fixed-background {
        background: linear-gradient(blue, red) fixed;
      }
    </style>
    <div id="div" style="width: 5000px; height: 5000px"></div>
  )HTML");

  auto* frame_view = GetDocument().View();
  EXPECT_EQ(0u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_FALSE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());

  Element* body = GetDocument().body();
  Element* html = GetDocument().documentElement();
  Element* div = GetDocument().getElementById("div");

  // Only body has fixed background. No main thread scrolling.
  body->setAttribute(html_names::kClassAttr, "fixed-background");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_FALSE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());

  // Both body and div have fixed background. Requires main thread scrolling.
  div->setAttribute(html_names::kClassAttr, "fixed-background");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_TRUE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());

  // Only div has fixed background. Requires main thread scrolling.
  body->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_TRUE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());

  // Only html has fixed background. No main thread scrolling.
  div->removeAttribute(html_names::kClassAttr);
  html->setAttribute(html_names::kClassAttr, "fixed-background");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_FALSE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());

  // Both html and body have fixed background. Requires main thread scrolling.
  body->setAttribute(html_names::kClassAttr, "fixed-background");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_TRUE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());
}

TEST_F(LocalFrameViewTest,
       MainThreadScrollingForBackgroundFixedAttachmentWithoutCompositing) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .fixed-background {
        background: linear-gradient(blue, red) fixed;
      }
    </style>
    <div id="div" style="width: 5000px; height: 5000px"></div>
  )HTML");

  auto* frame_view = GetDocument().View();
  EXPECT_EQ(0u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_FALSE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());

  Element* body = GetDocument().body();
  Element* html = GetDocument().documentElement();
  Element* div = GetDocument().getElementById("div");

  // When not prefer compositing, we use main thread scrolling when there is
  // any object with fixed-attachment background.
  body->setAttribute(html_names::kClassAttr, "fixed-background");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_TRUE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());

  div->setAttribute(html_names::kClassAttr, "fixed-background");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_TRUE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());

  body->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_TRUE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());

  div->removeAttribute(html_names::kClassAttr);
  html->setAttribute(html_names::kClassAttr, "fixed-background");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_TRUE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());

  body->setAttribute(html_names::kClassAttr, "fixed-background");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2u, frame_view->BackgroundAttachmentFixedObjects().size());
  EXPECT_TRUE(
      frame_view->RequiresMainThreadScrollingForBackgroundAttachmentFixed());
}

// Ensure the fragment navigation "scroll into view and focus" behavior doesn't
// activate synchronously while rendering is blocked waiting on a stylesheet.
// See https://crbug.com/851338.
TEST_F(SimTest, FragmentNavChangesFocusWhileRenderingBlocked) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_resource("https://example.com/sheet.css",
                                     "text/css");
  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <link rel="stylesheet" type="text/css" href="sheet.css">
      <a id="anchorlink" href="#bottom">Link to bottom of the page</a>
      <div style="height: 1000px;"></div>
      <input id="bottom">Bottom of the page</input>
    )HTML");

  ScrollableArea* viewport = GetDocument().View()->LayoutViewport();
  ASSERT_EQ(ScrollOffset(), viewport->GetScrollOffset());

  // We're still waiting on the stylesheet to load so the load event shouldn't
  // yet dispatch and rendering is deferred.
  ASSERT_FALSE(GetDocument().HaveRenderBlockingResourcesLoaded());
  EXPECT_FALSE(GetDocument().IsLoadCompleted());

  // Click on the anchor element. This will cause a synchronous same-document
  //  navigation.
  auto* anchor =
      To<HTMLAnchorElement>(GetDocument().getElementById("anchorlink"));
  anchor->click();

  // Even though the navigation is synchronous, the active element shouldn't be
  // changed.
  EXPECT_EQ(GetDocument().body(), GetDocument().ActiveElement())
      << "Active element changed while rendering is blocked";
  EXPECT_EQ(ScrollOffset(), viewport->GetScrollOffset())
      << "Scroll offset changed while rendering is blocked";

  // Force a layout.
  anchor->style()->setProperty(&GetDocument(), "display", "block", String(),
                               ASSERT_NO_EXCEPTION);
  GetDocument().UpdateStyleAndLayout();

  EXPECT_EQ(GetDocument().body(), GetDocument().ActiveElement())
      << "Active element changed due to layout while rendering is blocked";
  EXPECT_EQ(ScrollOffset(), viewport->GetScrollOffset())
      << "Scroll offset changed due to layout while rendering is blocked";

  // Complete the CSS stylesheet load so the document can finish loading. The
  // fragment should be activated at that point.
  css_resource.Complete("");
  RunPendingTasks();
  Compositor().BeginFrame();
  ASSERT_TRUE(GetDocument().IsLoadCompleted());
  EXPECT_EQ(GetDocument().getElementById("bottom"),
            GetDocument().ActiveElement())
      << "Active element wasn't changed after load completed.";
  EXPECT_NE(ScrollOffset(), viewport->GetScrollOffset())
      << "Scroll offset wasn't changed after load completed.";
}

TEST_F(SimTest, ForcedLayoutWithIncompleteSVGChildFrame) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest svg_resource("https://example.com/file.svg", "image/svg+xml");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <object data="file.svg"></object>
    )HTML");

  // Write the SVG document so that there is something to layout, but don't let
  // the resource finish loading.
  svg_resource.Write(R"SVG(
      <svg xmlns="http://www.w3.org/2000/svg"></svg>
    )SVG");

  // Mark the top-level document for layout and then force layout. This will
  // cause the layout tree in the <object> object to be built.
  GetDocument().View()->SetNeedsLayout();
  GetDocument().UpdateStyleAndLayout();

  svg_resource.Finish();
}

}  // namespace
}  // namespace blink
