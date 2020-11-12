// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

#define EXPECT_MAIN_THREAD_SCROLLING_REASON(expected, actual)             \
  EXPECT_EQ(expected, actual)                                             \
      << " expected: " << cc::MainThreadScrollingReason::AsText(expected) \
      << " actual: " << cc::MainThreadScrollingReason::AsText(actual)

#define EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(actual) \
  EXPECT_EQ(cc::MainThreadScrollingReason::kNotScrollingOnMain, actual)

class MainThreadScrollingReasonsTest : public testing::Test {
 public:
  MainThreadScrollingReasonsTest() : base_url_("http://www.test.com/") {
    helper_.InitializeWithSettings(&ConfigureSettings);
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
        ->scroll_tree.FindNodeFromElementId(layer->element_id());
  }

  bool IsScrollable(const cc::Layer* layer) const {
    return GetScrollNode(layer)->scrollable;
  }

  uint32_t GetMainThreadScrollingReasons(const cc::Layer* layer) const {
    return GetScrollNode(layer)->main_thread_scrolling_reasons;
  }

  uint32_t GetViewMainThreadScrollingReasons() const {
    const auto* scroll = GetFrame()
                             ->View()
                             ->GetLayoutView()
                             ->FirstFragment()
                             .PaintProperties()
                             ->Scroll();
    return scroll->GetMainThreadScrollingReasons();
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }
  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }
  PaintLayerScrollableArea* GetScrollableArea(const Element& element) const {
    return To<LayoutBoxModelObject>(element.GetLayoutObject())
        ->GetScrollableArea();
  }

 protected:
  String base_url_;

 private:
  static void ConfigureSettings(WebSettings* settings) {
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  frame_test_helpers::WebViewHelper helper_;
};

// More cases are tested in LocalFrameViewTest
// .RequiresMainThreadScrollingForBackgroundFixedAttachment.
TEST_F(MainThreadScrollingReasonsTest,
       BackgroundAttachmentFixedShouldTriggerMainThreadScroll) {
  RegisterMockedHttpURLLoad("iframe-background-attachment-fixed.html");
  RegisterMockedHttpURLLoad("iframe-background-attachment-fixed-inner.html");
  RegisterMockedHttpURLLoad("white-1x1.png");
  NavigateTo(base_url_ + "iframe-background-attachment-fixed.html");
  ForceFullCompositingUpdate();

  Element* iframe = GetFrame()->GetDocument()->getElementById("iframe");
  ASSERT_TRUE(iframe);

  LayoutObject* layout_object = iframe->GetLayoutObject();
  ASSERT_TRUE(layout_object);
  ASSERT_TRUE(layout_object->IsLayoutEmbeddedContent());

  auto* layout_embedded_content = To<LayoutEmbeddedContent>(layout_object);
  ASSERT_TRUE(layout_embedded_content);

  LocalFrameView* inner_frame_view =
      To<LocalFrameView>(layout_embedded_content->ChildFrameView());
  ASSERT_TRUE(inner_frame_view);

  auto* inner_layout_view = inner_frame_view->GetLayoutView();
  ASSERT_TRUE(inner_layout_view);

  PaintLayerCompositor* inner_compositor = inner_layout_view->Compositor();
  ASSERT_TRUE(inner_compositor->InCompositingMode());

  cc::Layer* cc_scroll_layer =
      inner_frame_view->LayoutViewport()->LayerForScrolling();
  ASSERT_TRUE(cc_scroll_layer);
  ASSERT_TRUE(IsScrollable(cc_scroll_layer));
  EXPECT_MAIN_THREAD_SCROLLING_REASON(
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      GetMainThreadScrollingReasons(cc_scroll_layer));

  // Remove fixed background-attachment should make the iframe
  // scroll on cc.
  auto* iframe_doc = To<HTMLIFrameElement>(iframe)->contentDocument();
  iframe = iframe_doc->getElementById("scrollable");
  ASSERT_TRUE(iframe);

  iframe->removeAttribute("class");
  ForceFullCompositingUpdate();

  layout_object = iframe->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  cc_scroll_layer =
      layout_object->GetFrameView()->LayoutViewport()->LayerForScrolling();
  ASSERT_TRUE(cc_scroll_layer);
  ASSERT_TRUE(IsScrollable(cc_scroll_layer));
  EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
      GetMainThreadScrollingReasons(cc_scroll_layer));

  // Force main frame to scroll on main thread. All its descendants
  // should scroll on main thread as well.
  Element* element = GetFrame()->GetDocument()->getElementById("scrollable");
  element->setAttribute(
      "style",
      "background-image: url('white-1x1.png'); background-attachment: fixed;",
      ASSERT_NO_EXCEPTION);

  ForceFullCompositingUpdate();

  layout_object = iframe->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  cc_scroll_layer =
      layout_object->GetFrameView()->LayoutViewport()->LayerForScrolling();
  ASSERT_TRUE(cc_scroll_layer);
  ASSERT_TRUE(IsScrollable(cc_scroll_layer));
  EXPECT_MAIN_THREAD_SCROLLING_REASON(
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      GetMainThreadScrollingReasons(cc_scroll_layer));
}

// Upon resizing the content size, the main thread scrolling reason
// kHasNonLayerViewportConstrainedObject should be updated on all frames
TEST_F(MainThreadScrollingReasonsTest,
       RecalculateMainThreadScrollingReasonsUponResize) {
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  RegisterMockedHttpURLLoad("has-non-layer-viewport-constrained-objects.html");
  RegisterMockedHttpURLLoad("white-1x1.png");
  NavigateTo(base_url_ + "has-non-layer-viewport-constrained-objects.html");
  ForceFullCompositingUpdate();

  // When the main document is not scrollable, there should be no reasons.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());

  // When the div forces the document to be scrollable, it should scroll on main
  // thread.
  Element* element = GetFrame()->GetDocument()->getElementById("scrollable");
  element->setAttribute(
      "style",
      "background-image: url('white-1x1.png'); background-attachment: fixed;",
      ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  EXPECT_MAIN_THREAD_SCROLLING_REASON(
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      GetViewMainThreadScrollingReasons());

  // The main thread scrolling reason should be reset upon the following change.
  element->setAttribute("style", "", ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  EXPECT_FALSE(GetViewMainThreadScrollingReasons());
}

TEST_F(MainThreadScrollingReasonsTest, FastScrollingCanBeDisabledWithSetting) {
  GetWebView()->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  GetWebView()->GetSettings()->SetThreadedScrollingEnabled(false);
  GetFrame()->View()->SetNeedsPaintPropertyUpdate();
  ForceFullCompositingUpdate();

  // Main scrolling should be enabled with the setting override.
  EXPECT_TRUE(GetViewMainThreadScrollingReasons());

  // Main scrolling should also propagate to inner viewport layer.
  const cc::Layer* visual_viewport_scroll_layer =
      GetFrame()->GetPage()->GetVisualViewport().LayerForScrolling();
  ASSERT_TRUE(IsScrollable(visual_viewport_scroll_layer));
  EXPECT_TRUE(GetMainThreadScrollingReasons(visual_viewport_scroll_layer));
}

TEST_F(MainThreadScrollingReasonsTest, FastScrollingForFixedPosition) {
  RegisterMockedHttpURLLoad("fixed-position.html");
  NavigateTo(base_url_ + "fixed-position.html");
  ForceFullCompositingUpdate();

  // Fixed position should not fall back to main thread scrolling.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());
}

TEST_F(MainThreadScrollingReasonsTest, FastScrollingForStickyPosition) {
  RegisterMockedHttpURLLoad("sticky-position.html");
  NavigateTo(base_url_ + "sticky-position.html");
  ForceFullCompositingUpdate();

  // Sticky position should not fall back to main thread scrolling.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());
}

TEST_F(MainThreadScrollingReasonsTest, FastScrollingByDefault) {
  GetWebView()->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  ForceFullCompositingUpdate();

  // Fast scrolling should be enabled by default.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());

  const cc::Layer* visual_viewport_scroll_layer =
      GetFrame()->GetPage()->GetVisualViewport().LayerForScrolling();
  ASSERT_TRUE(IsScrollable(visual_viewport_scroll_layer));
  EXPECT_FALSE(GetMainThreadScrollingReasons(visual_viewport_scroll_layer));
}

TEST_F(MainThreadScrollingReasonsTest,
       ScrollbarsForceMainThreadOrHaveCompositorScrollbarLayer) {
  RegisterMockedHttpURLLoad("trivial-scroller.html");
  NavigateTo(base_url_ + "trivial-scroller.html");
  ForceFullCompositingUpdate();

  Document* document = GetFrame()->GetDocument();
  Element* scrollable_element = document->getElementById("scroller");
  DCHECK(scrollable_element);

  LayoutObject* layout_object = scrollable_element->GetLayoutObject();
  auto* box = To<LayoutBox>(layout_object);
  ASSERT_TRUE(box->UsesCompositedScrolling());
  CompositedLayerMapping* composited_layer_mapping =
      box->Layer()->GetCompositedLayerMapping();
  GraphicsLayer* scrollbar_graphics_layer =
      composited_layer_mapping->LayerForVerticalScrollbar();
  ASSERT_TRUE(scrollbar_graphics_layer);

  bool has_cc_scrollbar_layer = !scrollbar_graphics_layer->DrawsContent();
  EXPECT_TRUE(
      has_cc_scrollbar_layer ||
      GetMainThreadScrollingReasons(scrollbar_graphics_layer->ContentsLayer()));
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

  void TestNonCompositedReasons(const AtomicString& style_class,
                                const uint32_t reason) {
    GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
    Document* document = GetFrame()->GetDocument();
    Element* container = document->getElementById("scroller1");
    ForceFullCompositingUpdate();

    PaintLayerScrollableArea* scrollable_area = GetScrollableArea(*container);
    ASSERT_TRUE(scrollable_area);
    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
        scrollable_area->GetNonCompositedMainThreadScrollingReasons());

    container->classList().Add(style_class);
    ForceFullCompositingUpdate();

    ASSERT_TRUE(scrollable_area);
    EXPECT_MAIN_THREAD_SCROLLING_REASON(
        reason, scrollable_area->GetNonCompositedMainThreadScrollingReasons());

    Element* container2 = document->getElementById("scroller2");
    PaintLayerScrollableArea* scrollable_area2 = GetScrollableArea(*container2);
    ASSERT_TRUE(scrollable_area2);
    // Different scrollable area should remain unaffected.
    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
        scrollable_area2->GetNonCompositedMainThreadScrollingReasons());

    LocalFrameView* frame_view = GetFrame()->View();
    ASSERT_TRUE(frame_view);
    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
        frame_view->GetMainThreadScrollingReasons());

    // Remove class from the scroller 1 would lead to scroll on impl.

    container->classList().Remove(style_class);
    ForceFullCompositingUpdate();

    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
        scrollable_area->GetNonCompositedMainThreadScrollingReasons());
    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
        frame_view->GetMainThreadScrollingReasons());

    // Add target attribute would again lead to scroll on main thread
    container->classList().Add(style_class);
    ForceFullCompositingUpdate();

    EXPECT_MAIN_THREAD_SCROLLING_REASON(
        reason, scrollable_area->GetNonCompositedMainThreadScrollingReasons());
    EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
        frame_view->GetMainThreadScrollingReasons());

    if ((reason & kLCDTextRelatedReasons) &&
        !(reason & ~kLCDTextRelatedReasons)) {
      GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
      ForceFullCompositingUpdate();
      EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
          scrollable_area->GetNonCompositedMainThreadScrollingReasons());
      EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
          frame_view->GetMainThreadScrollingReasons());
    }
  }
};

TEST_F(NonCompositedMainThreadScrollingReasonsTest, TransparentTest) {
  TestNonCompositedReasons("transparent",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, TransformTest) {
  TestNonCompositedReasons("transform",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, BackgroundNotOpaqueTest) {
  TestNonCompositedReasons(
      "background-not-opaque",
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest,
       CantPaintScrollingBackgroundTest) {
  TestNonCompositedReasons(
      "cant-paint-scrolling-background",
      cc::MainThreadScrollingReason::kCantPaintScrollingBackgroundAndLCDText);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, ClipTest) {
  TestNonCompositedReasons("clip",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, ClipPathTest) {
  TestNonCompositedReasons("clip-path",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, BoxShadowTest) {
  TestNonCompositedReasons("box-shadow",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, InsetBoxShadowTest) {
  TestNonCompositedReasons(
      "inset-box-shadow",
      cc::MainThreadScrollingReason::kCantPaintScrollingBackgroundAndLCDText);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, StackingContextTest) {
  TestNonCompositedReasons("non-stacking-context",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, BorderRadiusTest) {
  TestNonCompositedReasons("border-radius",
                           cc::MainThreadScrollingReason::kNotScrollingOnMain);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest,
       ForcedComositingWithLCDRelatedReasons) {
  // With "will-change:transform" we composite elements with
  // LCDTextRelatedReasons only. For elements with other
  // NonCompositedReasons, we don't create scrollingLayer for their
  // CompositedLayerMapping therefore they don't get composited.
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  Document* document = GetFrame()->GetDocument();
  Element* container = document->getElementById("scroller1");
  ASSERT_TRUE(container);
  container->setAttribute("class", "composited transparent",
                          ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  PaintLayerScrollableArea* scrollable_area = GetScrollableArea(*container);
  ASSERT_TRUE(scrollable_area);
  EXPECT_NO_MAIN_THREAD_SCROLLING_REASON(
      scrollable_area->GetNonCompositedMainThreadScrollingReasons());

  Element* container2 = document->getElementById("scroller2");
  ASSERT_TRUE(container2);
  container2->setAttribute("class", "composited border-radius",
                           ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();
  PaintLayerScrollableArea* scrollable_area2 = GetScrollableArea(*container2);
  ASSERT_TRUE(scrollable_area2);
  ASSERT_TRUE(scrollable_area2->UsesCompositedScrolling());
}

}  // namespace blink
