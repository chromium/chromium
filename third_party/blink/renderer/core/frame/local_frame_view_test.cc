// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_view.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
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

TEST_F(LocalFrameViewTest,
       SetPaintInvalidationDuringUpdateLifecyclePhasesToPrePaintClean) {
  SetBodyInnerHTML("<div id='a' style='color: blue'>A</div>");
  GetDocument().getElementById("a")->setAttribute(html_names::kStyleAttr,
                                                  "color: green");
  GetAnimationMockChromeClient().has_scheduled_animation_ = false;
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(GetAnimationMockChromeClient().has_scheduled_animation_);
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
  UpdateAllLifecyclePhasesForTest();
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
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(1, 1), mojom::blink::ScrollType::kUser);

  // Programmatic scrolling should not dismiss the tooltip, so setToolTip
  // should not be called for this invocation.
  EXPECT_CALL(GetAnimationMockChromeClient(),
              MockSetToolTip(GetDocument().GetFrame(), String(), _))
      .Times(0);
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(2, 2), mojom::blink::ScrollType::kProgrammatic);
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
  sticky->Layer()->UpdateAncestorScrollContainerLayer(nullptr);

  // This call should not crash.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 100), mojom::blink::ScrollType::kProgrammatic);
}

TEST_F(LocalFrameViewTest, UpdateLifecyclePhasesForPrintingDetachedFrame) {
  SetBodyInnerHTML("<iframe style='display: none'></iframe>");
  SetChildFrameHTML("A");

  ChildDocument().SetPrinting(Document::kPrinting);
  ChildDocument().View()->UpdateLifecyclePhasesForPrinting();

  // The following checks that the detached frame has been walked for PrePaint.
  EXPECT_EQ(DocumentLifecycle::kCompositingAssignmentsClean,
            GetDocument().Lifecycle().GetState());
  EXPECT_EQ(DocumentLifecycle::kCompositingAssignmentsClean,
            ChildDocument().Lifecycle().GetState());
  auto* child_layout_view = ChildDocument().GetLayoutView();
  EXPECT_TRUE(child_layout_view->FirstFragment().PaintProperties());
}

TEST_F(LocalFrameViewTest, CanHaveScrollbarsIfScrollingAttrEqualsNoChanged) {
  SetBodyInnerHTML("<iframe scrolling='no'></iframe>");
  EXPECT_FALSE(ChildDocument().View()->CanHaveScrollbars());

  ChildDocument().WillChangeFrameOwnerProperties(
      0, 0, mojom::blink::ScrollbarMode::kAlwaysOn, false,
      mojom::blink::ColorScheme::kLight);
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
  // Style-sheets are parser-blocking, not render-blocking when
  // BlockHTMLParserOnStyleSheets is enabled.
  ScopedBlockHTMLParserOnStyleSheetsForTest scope(false);

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
  anchor->style()->setProperty(GetDocument().GetExecutionContext(), "display",
                               "block", String(), ASSERT_NO_EXCEPTION);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

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
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  svg_resource.Finish();
}

TEST_F(LocalFrameViewTest, TogglePaintEligibility) {
  SetBodyInnerHTML("<iframe><p>Hello</p></iframe>");

  PaintTiming& parent_timing = PaintTiming::From(GetDocument());
  PaintTiming& child_timing = PaintTiming::From(ChildDocument());

  // Mainframes are unthrottled by default.
  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(parent_timing.FirstEligibleToPaint().is_null());

  GetDocument().View()->MarkFirstEligibleToPaint();
  EXPECT_FALSE(parent_timing.FirstEligibleToPaint().is_null());

  // Subframes are throttled when first loaded.
  EXPECT_TRUE(ChildDocument().View()->ShouldThrottleRenderingForTest());

  // Toggle paint elgibility to true.
  ChildDocument().OverrideIsInitialEmptyDocument();
  ChildDocument().View()->BeginLifecycleUpdates();
  ChildDocument().View()->MarkFirstEligibleToPaint();
  EXPECT_FALSE(ChildDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(child_timing.FirstEligibleToPaint().is_null());

  // Toggle paint elgibility to false.
  ChildDocument().View()->SetLifecycleUpdatesThrottledForTesting(true);
  ChildDocument().View()->MarkIneligibleToPaint();
  EXPECT_TRUE(ChildDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(child_timing.FirstEligibleToPaint().is_null());
}

TEST_F(SimTest, PaintEligibilityNoSubframe) {
  SimRequest resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  resource.Complete("<p>Hello</p>");

  PaintTiming& timing = PaintTiming::From(GetDocument());

  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(timing.FirstEligibleToPaint().is_null());

  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(timing.FirstEligibleToPaint().is_null());
}

TEST_F(SimTest, SameOriginPaintEligibility) {
  SimRequest resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  resource.Complete(R"HTML(
      <iframe id=frame top=4000px left=4000px>
        <p>Hello</p>
      </iframe>
    )HTML");

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();
  PaintTiming& frame_timing = PaintTiming::From(*frame_document);

  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRenderingForTest());

  // Same origin frames are not throttled.
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(frame_timing.FirstEligibleToPaint().is_null());

  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(frame_timing.FirstEligibleToPaint().is_null());
}

TEST_F(SimTest, CrossOriginPaintEligibility) {
  SimRequest resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  resource.Complete(R"HTML(
      <iframe id=frame srcdoc ="<p>Hello</p>" sandbox top=4000px left=4000px>
      </iframe>
    )HTML");

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();
  PaintTiming& frame_timing = PaintTiming::From(*frame_document);

  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRenderingForTest());

  // Hidden cross origin frames are throttled.
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(frame_timing.FirstEligibleToPaint().is_null());

  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(frame_timing.FirstEligibleToPaint().is_null());
}

TEST_F(SimTest, NestedCrossOriginPaintEligibility) {
  // Create a document with doubly nested iframes.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<iframe id=outer src=iframe.html></iframe>");
  frame_resource.Complete(R"HTML(
      <iframe id=inner srcdoc ="<p>Hello</p>" sandbox top=4000px left=4000px>
      </iframe>
    )HTML");

  auto* outer_frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("outer"));
  auto* outer_frame_document = outer_frame_element->contentDocument();
  PaintTiming& outer_frame_timing = PaintTiming::From(*outer_frame_document);

  auto* inner_frame_element =
      To<HTMLIFrameElement>(outer_frame_document->getElementById("inner"));
  auto* inner_frame_document = inner_frame_element->contentDocument();
  PaintTiming& inner_frame_timing = PaintTiming::From(*inner_frame_document);

  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(outer_frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(outer_frame_timing.FirstEligibleToPaint().is_null());
  EXPECT_TRUE(inner_frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(inner_frame_timing.FirstEligibleToPaint().is_null());

  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(outer_frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(outer_frame_timing.FirstEligibleToPaint().is_null());
  EXPECT_TRUE(inner_frame_document->View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(inner_frame_timing.FirstEligibleToPaint().is_null());
}

class TestLifecycleObserver
    : public GarbageCollected<TestLifecycleObserver>,
      public LocalFrameView::LifecycleNotificationObserver {
 public:
  TestLifecycleObserver() = default;

  void WillStartLifecycleUpdate(const LocalFrameView&) override {
    ++will_start_lifecycle_count_;
  }
  void DidFinishLifecycleUpdate(const LocalFrameView&) override {
    ++did_finish_lifecycle_count_;
  }

  int will_start_lifecycle_count() const { return will_start_lifecycle_count_; }
  int did_finish_lifecycle_count() const { return did_finish_lifecycle_count_; }

  // GC functions.
  void Trace(Visitor*) const override {}

 private:
  int will_start_lifecycle_count_ = 0;
  int did_finish_lifecycle_count_ = 0;
};

TEST_F(LocalFrameViewTest, LifecycleNotificationsOnlyOnFullLifecycle) {
  SetBodyInnerHTML("<div></div>");
  auto* frame_view = GetDocument().View();

  auto* observer = MakeGarbageCollected<TestLifecycleObserver>();
  frame_view->RegisterForLifecycleNotifications(observer);

  EXPECT_EQ(observer->will_start_lifecycle_count(), 0);
  EXPECT_EQ(observer->did_finish_lifecycle_count(), 0);

  frame_view->UpdateAllLifecyclePhasesExceptPaint(DocumentUpdateReason::kTest);
  EXPECT_EQ(observer->will_start_lifecycle_count(), 0);
  EXPECT_EQ(observer->did_finish_lifecycle_count(), 0);

  frame_view->UpdateLifecyclePhasesForPrinting();
  EXPECT_EQ(observer->will_start_lifecycle_count(), 0);
  EXPECT_EQ(observer->did_finish_lifecycle_count(), 0);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(observer->will_start_lifecycle_count(), 1);
  EXPECT_EQ(observer->did_finish_lifecycle_count(), 1);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(observer->will_start_lifecycle_count(), 2);
  EXPECT_EQ(observer->did_finish_lifecycle_count(), 2);

  frame_view->UnregisterFromLifecycleNotifications(observer);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(observer->will_start_lifecycle_count(), 2);
  EXPECT_EQ(observer->did_finish_lifecycle_count(), 2);
}

TEST_F(LocalFrameViewTest, StartOfLifecycleTaskRunsOnFullLifecycle) {
  SetBodyInnerHTML("<div></div>");
  auto* frame_view = GetDocument().View();

  struct TestCallback {
    void Increment() { ++calls; }
    int calls = 0;
  };

  TestCallback callback;

  frame_view->EnqueueStartOfLifecycleTask(
      base::BindOnce(&TestCallback::Increment, base::Unretained(&callback)));
  EXPECT_EQ(callback.calls, 0);

  frame_view->UpdateAllLifecyclePhasesExceptPaint(DocumentUpdateReason::kTest);
  EXPECT_EQ(callback.calls, 0);

  frame_view->UpdateLifecyclePhasesForPrinting();
  EXPECT_EQ(callback.calls, 0);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(callback.calls, 1);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(callback.calls, 1);
}
}  // namespace
}  // namespace blink
