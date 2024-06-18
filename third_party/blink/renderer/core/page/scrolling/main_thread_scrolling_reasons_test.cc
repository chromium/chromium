// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

#define EXPECT_MAIN_THREAD_SCROLLING_REASON(expected, actual)             \
  EXPECT_EQ(expected, actual)                                             \
      << " expected: " << cc::MainThreadScrollingReason::AsText(expected) \
      << " actual: " << cc::MainThreadScrollingReason::AsText(actual)

#define EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(actual) \
  EXPECT_EQ(cc::MainThreadScrollingReason::kNotScrollingOnMain, actual)

class MainThreadScrollingReasonsTest : public PaintTestConfigurations,
                                       public testing::Test {
 public:
  MainThreadScrollingReasonsTest() : base_url_("http://www.test.com/") {
    helper_.Initialize();
    GetFrame()->GetSettings()->SetPreferCompositingToLCDTextForTesting(true);
    GetWebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));
    GetWebView()->MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  ~MainThreadScrollingReasonsTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void NavigateTo(const String& url) {
    frame_test_helpers::LoadFrame(GetWebView()->MainFrameImpl(), url.Utf8());
  }

  void LoadHTML(const String& html) {
    frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(),
                                       html.Utf8(),
                                       url_test_helpers::ToKURL("about:blank"));
  }

  void ForceFullCompositingUpdate() {
    GetWebView()->MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void RegisterMockedHttpURLLoad(const String& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString(base_url_), test::CoreTestDataPath(), WebString(file_name));
  }

  const cc::ScrollNode* GetScrollNode(const cc::Layer* layer) const {
    return layer->layer_tree_host()
        ->property_trees()
        ->scroll_tree()
        .FindNodeFromElementId(layer->element_id());
  }

  const cc::ScrollNode* GetScrollNode(
      const PaintLayerScrollableArea& scrollable_area) const {
    return GetFrame()
        ->View()
        ->RootCcLayer()
        ->layer_tree_host()
        ->property_trees()
        ->scroll_tree()
        .FindNodeFromElementId(scrollable_area.GetScrollElementId());
  }

  uint32_t GetMainThreadScrollingReasons(const cc::Layer* layer) const {
    return GetScrollNode(layer)->main_thread_scrolling_reasons;
  }

  uint32_t GetMainThreadScrollingReasons(
      const ScrollPaintPropertyNode& scroll) const {
    return GetFrame()
        ->View()
        ->GetPaintArtifactCompositor()
        ->GetMainThreadScrollingReasons(scroll);
  }

  uint32_t GetMainThreadScrollingReasons(
      const PaintLayerScrollableArea& scrollable_area) const {
    return GetMainThreadScrollingReasons(*scrollable_area.GetLayoutBox()
                                              ->FirstFragment()
                                              .PaintProperties()
                                              ->Scroll());
  }

  uint32_t GetViewMainThreadScrollingReasons() const {
    return GetMainThreadScrollingReasons(*GetFrame()->View()->LayoutViewport());
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }
  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }
  PaintLayerScrollableArea* GetScrollableArea(const Element& element) const {
    return To<LayoutBoxModelObject>(element.GetLayoutObject())
        ->GetScrollableArea();
  }

 protected:
  test::TaskEnvironment task_environment_;
  String base_url_;
  frame_test_helpers::WebViewHelper helper_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(MainThreadScrollingReasonsTest);

// More cases are tested in LocalFrameViewTest
// .RequiresMainThreadScrollingForBackgroundFixedAttachment.
TEST_P(MainThreadScrollingReasonsTest,
       BackgroundAttachmentFixedShouldTriggerMainThreadScroll) {
  RegisterMockedHttpURLLoad("iframe-background-attachment-fixed.html");
  RegisterMockedHttpURLLoad("iframe-background-attachment-fixed-inner.html");
  RegisterMockedHttpURLLoad("white-1x1.png");
  NavigateTo(base_url_ + "iframe-background-attachment-fixed.html");
  ForceFullCompositingUpdate();

  auto* root_layer = GetFrame()->View()->RootCcLayer();
  auto* outer_layout_view = GetFrame()->View()->GetLayoutView();
  Element* iframe =
      GetFrame()->GetDocument()->getElementById(AtomicString("iframe"));
  ASSERT_TRUE(iframe);

  LocalFrameView* inner_frame_view = To<LocalFrameView>(
      To<LayoutEmbeddedContent>(iframe->GetLayoutObject())->ChildFrameView());
  ASSERT_TRUE(inner_frame_view);
  auto* inner_layout_view = inner_frame_view->GetLayoutView();
  ASSERT_TRUE(inner_layout_view);

  auto* inner_scroll_node =
      inner_layout_view->FirstFragment().PaintProperties()->Scroll();
  ASSERT_TRUE(inner_scroll_node);
  EXPECT_MAIN_THREAD_SCROLLING_REASON(
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      GetMainThreadScrollingReasons(*inner_scroll_node));
  const cc::Layer* inner_scroll_layer = CcLayerByCcElementId(
      root_layer, inner_scroll_node->GetCompositorElementId());
  ASSERT_TRUE(inner_scroll_layer);
  EXPECT_MAIN_THREAD_SCROLLING_REASON(
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      GetMainThreadScrollingReasons(inner_scroll_layer));

  // Main thread scrolling of the inner layer doesn't affect the outer layer.
  auto* outer_scroll_node = GetFrame()
                                ->View()
                                ->GetLayoutView()
                                ->FirstFragment()
                                .PaintProperties()
                                ->Scroll();
  ASSERT_TRUE(outer_scroll_node);
  EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
      GetMainThreadScrollingReasons(*outer_scroll_node));
  const cc::Layer* outer_scroll_layer = CcLayerByCcElementId(
      root_layer, outer_scroll_node->GetCompositorElementId());
  ASSERT_TRUE(outer_scroll_layer);
  EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
      GetMainThreadScrollingReasons(outer_scroll_layer));

  // Remove fixed background-attachment should make the iframe scroll on cc.
  auto* content =
      inner_layout_view->GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(content);
  content->removeAttribute(html_names::kClassAttr);

  ForceFullCompositingUpdate();

  ASSERT_EQ(inner_scroll_node,
            inner_layout_view->FirstFragment().PaintProperties()->Scroll());
  EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
      GetMainThreadScrollingReasons(*inner_scroll_node));
  ASSERT_EQ(inner_scroll_layer,
            CcLayerByCcElementId(root_layer,
                                 inner_scroll_node->GetCompositorElementId()));
  EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
      GetMainThreadScrollingReasons(inner_scroll_layer));

  ASSERT_EQ(outer_scroll_node,
            outer_layout_view->FirstFragment().PaintProperties()->Scroll());
  EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
      GetMainThreadScrollingReasons(*outer_scroll_node));
  ASSERT_EQ(outer_scroll_layer,
            CcLayerByCcElementId(root_layer,
                                 outer_scroll_node->GetCompositorElementId()));
  EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
      GetMainThreadScrollingReasons(outer_scroll_layer));

  // Force main frame to scroll on main thread. All its descendants
  // should scroll on main thread as well.
  Element* element =
      GetFrame()->GetDocument()->getElementById(AtomicString("scrollable"));
  element->setAttribute(
      html_names::kStyleAttr,
      AtomicString(
          "background-image: url('white-1x1.png'), url('white-1x1.png');"
          "                  background-attachment: fixed, local;"));

  ForceFullCompositingUpdate();

  // Main thread scrolling of the outer layer affects the inner layer.
  ASSERT_EQ(inner_scroll_node,
            inner_layout_view->FirstFragment().PaintProperties()->Scroll());
  EXPECT_MAIN_THREAD_SCROLLING_REASON(
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      GetMainThreadScrollingReasons(*inner_scroll_node));
  ASSERT_EQ(inner_scroll_layer,
            CcLayerByCcElementId(root_layer,
                                 inner_scroll_node->GetCompositorElementId()));
  EXPECT_MAIN_THREAD_SCROLLING_REASON(
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      GetMainThreadScrollingReasons(inner_scroll_layer));

  ASSERT_EQ(outer_scroll_node,
            outer_layout_view->FirstFragment().PaintProperties()->Scroll());
  EXPECT_MAIN_THREAD_SCROLLING_REASON(
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      GetMainThreadScrollingReasons(*outer_scroll_node));
  ASSERT_EQ(outer_scroll_layer,
            CcLayerByCcElementId(root_layer,
                                 outer_scroll_node->GetCompositorElementId()));
  EXPECT_MAIN_THREAD_SCROLLING_REASON(
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      GetMainThreadScrollingReasons(outer_scroll_layer));
}

TEST_P(MainThreadScrollingReasonsTest, ReportBackgroundAttachmentFixed) {
  base::HistogramTester histogram_tester;
  std::string html = R"HTML(
    <style>
      body { width: 900px; height: 900px; }
      #bg {
        background: url('white-1x1.png') fixed, url('white-1x1.png') local;
      }
    </style>
    <div id=bg>x</div>
  )HTML";

  WebLocalFrameImpl* frame = helper_.LocalMainFrame();
  frame_test_helpers::LoadHTMLString(frame, html,
                                     url_test_helpers::ToKURL("about:blank"));

  helper_.GetLayerTreeHost()->CompositeForTest(base::TimeTicks::Now(), false,
                                               base::OnceClosure());

  auto CreateEvent = [](WebInputEvent::Type type) {
    return WebGestureEvent(type, WebInputEvent::kNoModifiers,
                           base::TimeTicks::Now(),
                           WebGestureDevice::kTouchscreen);
  };

  WebGestureEvent scroll_begin =
      CreateEvent(WebInputEvent::Type::kGestureScrollBegin);
  WebGestureEvent scroll_update =
      CreateEvent(WebInputEvent::Type::kGestureScrollUpdate);
  WebGestureEvent scroll_end =
      CreateEvent(WebInputEvent::Type::kGestureScrollEnd);

  scroll_begin.SetPositionInWidget(gfx::PointF(100, 100));
  scroll_update.SetPositionInWidget(gfx::PointF(100, 100));
  scroll_end.SetPositionInWidget(gfx::PointF(100, 100));

  scroll_update.data.scroll_update.delta_y = -100;

  auto* widget = helper_.GetMainFrameWidget();
  widget->DispatchThroughCcInputHandler(scroll_begin);
  widget->DispatchThroughCcInputHandler(scroll_update);
  widget->DispatchThroughCcInputHandler(scroll_end);

  helper_.GetLayerTreeHost()->CompositeForTest(base::TimeTicks::Now(), false,
                                               base::OnceClosure());

  uint32_t expected_reason =
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Renderer4.MainThreadGestureScrollReason2"),
      testing::ElementsAre(
          base::Bucket(
              base::HistogramBase::Sample(
                  cc::MainThreadScrollingReason::kScrollingOnMainForAnyReason),
              1),
          base::Bucket(base::HistogramBase::Sample(
                           cc::MainThreadScrollingReason::BucketIndexForTesting(
                               expected_reason)),
                       1)));
}

// Upon resizing the content size, the main thread scrolling reason
// kHasBackgroundAttachmentFixedObjects should be updated on all frames
TEST_P(MainThreadScrollingReasonsTest,
       RecalculateMainThreadScrollingReasonsUponResize) {
  GetFrame()->GetSettings()->SetPreferCompositingToLCDTextForTesting(false);
  RegisterMockedHttpURLLoad("has-non-layer-viewport-constrained-objects.html");
  RegisterMockedHttpURLLoad("white-1x1.png");
  NavigateTo(base_url_ + "has-non-layer-viewport-constrained-objects.html");
  ForceFullCompositingUpdate();

  // When the main document is not scrollable, there should be no reasons.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());

  // When the div forces the document to be scrollable, it should scroll on main
  // thread.
  Element* element =
      GetFrame()->GetDocument()->getElementById(AtomicString("scrollable"));
  element->setAttribute(html_names::kStyleAttr,
                        AtomicString("background-image: url('white-1x1.png'); "
                                     "background-attachment: fixed;"));
  ForceFullCompositingUpdate();

  EXPECT_MAIN_THREAD_SCROLLING_REASON(
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      GetViewMainThreadScrollingReasons());

  // The main thread scrolling reason should be reset upon the following change.
  element->setAttribute(html_names::kStyleAttr, g_empty_atom);
  ForceFullCompositingUpdate();

  EXPECT_FALSE(GetViewMainThreadScrollingReasons());
}

TEST_P(MainThreadScrollingReasonsTest, FastScrollingForFixedPosition) {
  RegisterMockedHttpURLLoad("fixed-position.html");
  NavigateTo(base_url_ + "fixed-position.html");
  ForceFullCompositingUpdate();

  // Fixed position should not fall back to main thread scrolling.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());
}

TEST_P(MainThreadScrollingReasonsTest, FastScrollingForStickyPosition) {
  RegisterMockedHttpURLLoad("sticky-position.html");
  NavigateTo(base_url_ + "sticky-position.html");
  ForceFullCompositingUpdate();

  // Sticky position should not fall back to main thread scrolling.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());
}

TEST_P(MainThreadScrollingReasonsTest, FastScrollingByDefault) {
  GetWebView()->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  ForceFullCompositingUpdate();

  // Fast scrolling should be enabled by default.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());

  const cc::Layer* visual_viewport_scroll_layer =
      GetFrame()->GetPage()->GetVisualViewport().LayerForScrolling();
  EXPECT_FALSE(GetMainThreadScrollingReasons(visual_viewport_scroll_layer));
}

class NonCompositedMainThreadScrollingReasonsTest
    : public MainThreadScrollingReasonsTest {
  static const uint32_t kLCDTextRelatedReasons =
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText;

 protected:
  NonCompositedMainThreadScrollingReasonsTest() {
    RegisterMockedHttpURLLoad("two_scrollable_area.html");
    NavigateTo(base_url_ + "two_scrollable_area.html");
  }

  void TestNonCompositedReasons(const char* style_class,
                                const uint32_t reason) {
    AtomicString style_class_string(style_class);
    GetFrame()->GetSettings()->SetPreferCompositingToLCDTextForTesting(false);
    Document* document = GetFrame()->GetDocument();
    Element* container = document->getElementById(AtomicString("scroller1"));
    ForceFullCompositingUpdate();

    PaintLayerScrollableArea* scrollable_area = GetScrollableArea(*container);
    ASSERT_TRUE(scrollable_area);
    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
        GetMainThreadScrollingReasons(*scrollable_area));

    container->classList().Add(style_class_string);
    ForceFullCompositingUpdate();

    ASSERT_TRUE(scrollable_area);
    EXPECT_MAIN_THREAD_SCROLLING_REASON(
        reason, GetMainThreadScrollingReasons(*scrollable_area));

    Element* container2 = document->getElementById(AtomicString("scroller2"));
    PaintLayerScrollableArea* scrollable_area2 = GetScrollableArea(*container2);
    ASSERT_TRUE(scrollable_area2);
    // Different scrollable area should remain unaffected.
    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
        GetMainThreadScrollingReasons(*scrollable_area2));

    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(GetViewMainThreadScrollingReasons());

    // Remove class from the scroller 1 would lead to scroll on impl.
    container->classList().Remove(style_class_string);
    ForceFullCompositingUpdate();

    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
        GetMainThreadScrollingReasons(*scrollable_area));
    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(GetViewMainThreadScrollingReasons());

    // Add target attribute would again lead to scroll on main thread
    container->classList().Add(style_class_string);
    ForceFullCompositingUpdate();

    EXPECT_MAIN_THREAD_SCROLLING_REASON(
        reason, GetMainThreadScrollingReasons(*scrollable_area));
    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(GetViewMainThreadScrollingReasons());

    if ((reason & kLCDTextRelatedReasons) &&
        !(reason & ~kLCDTextRelatedReasons)) {
      GetFrame()->GetSettings()->SetPreferCompositingToLCDTextForTesting(true);
      ForceFullCompositingUpdate();
      EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
          GetMainThreadScrollingReasons(*scrollable_area));
      EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
          GetViewMainThreadScrollingReasons());
    }
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(NonCompositedMainThreadScrollingReasonsTest);

TEST_P(NonCompositedMainThreadScrollingReasonsTest, TransparentTest) {
  TestNonCompositedReasons("transparent",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, TransformTest) {
  TestNonCompositedReasons("transform",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, BackgroundNotOpaqueTest) {
  TestNonCompositedReasons(
      "background-not-opaque",
      RuntimeEnabledFeatures::RasterInducingScrollEnabled()
          ? cc::MainThreadScrollingReason::kNotScrollingOnMain
          : cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest,
       CantPaintScrollingBackgroundTest) {
  TestNonCompositedReasons(
      "cant-paint-scrolling-background",
      RuntimeEnabledFeatures::RasterInducingScrollEnabled()
          ? cc::MainThreadScrollingReason::kBackgroundNeedsRepaintOnScroll
          : cc::MainThreadScrollingReason::kBackgroundNeedsRepaintOnScroll |
                cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest,
       BackgroundNeedsRepaintOnScroll) {
  TestNonCompositedReasons(
      "needs-repaint-on-scroll",
      cc::MainThreadScrollingReason::kBackgroundNeedsRepaintOnScroll);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, ClipTest) {
  TestNonCompositedReasons("clip",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, ClipPathTest) {
  TestNonCompositedReasons("clip-path",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, BoxShadowTest) {
  TestNonCompositedReasons("box-shadow",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, InsetBoxShadowTest) {
  TestNonCompositedReasons(
      "inset-box-shadow",
      RuntimeEnabledFeatures::RasterInducingScrollEnabled()
          ? cc::MainThreadScrollingReason::kNotScrollingOnMain
          : cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, StackingContextTest) {
  TestNonCompositedReasons("non-stacking-context",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, BorderRadiusTest) {
  TestNonCompositedReasons("border-radius",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest,
       ForcedComositingWithLCDRelatedReasons) {
  // With "will-change:transform" we composite elements with
  // LCDTextRelatedReasons only. For elements with other NonCompositedReasons,
  // we don't composite them.
  GetFrame()->GetSettings()->SetPreferCompositingToLCDTextForTesting(false);
  Document* document = GetFrame()->GetDocument();
  Element* container = document->getElementById(AtomicString("scroller1"));
  ASSERT_TRUE(container);
  container->setAttribute(html_names::kClassAttr,
                          AtomicString("scroller composited transparent"));
  ForceFullCompositingUpdate();

  PaintLayerScrollableArea* scrollable_area = GetScrollableArea(*container);
  ASSERT_TRUE(scrollable_area);
  EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
      GetMainThreadScrollingReasons(*scrollable_area));

  Element* container2 = document->getElementById(AtomicString("scroller2"));
  ASSERT_TRUE(container2);
  container2->setAttribute(html_names::kClassAttr,
                           AtomicString("scroller composited border-radius"));
  ForceFullCompositingUpdate();
  PaintLayerScrollableArea* scrollable_area2 = GetScrollableArea(*container2);
  ASSERT_TRUE(scrollable_area2);
  EXPECT_TRUE(GetScrollNode(*scrollable_area2)->is_composited);
}

}  // namespace blink
