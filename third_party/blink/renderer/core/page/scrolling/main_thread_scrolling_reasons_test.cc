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
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class MainThreadScrollingReasonsTest : public testing::Test {
 public:
  MainThreadScrollingReasonsTest() : base_url_("http://www.test.com/") {
    helper_.InitializeWithSettings(&ConfigureSettings);
    GetWebView()->MainFrameWidget()->Resize(IntSize(320, 240));
    GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
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
    GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  void RegisterMockedHttpURLLoad(const String& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString(base_url_), test::CoreTestDataPath(), WebString(file_name));
  }

  uint32_t GetMainThreadScrollingReasons(const cc::Layer* layer) const {
    return layer->layer_tree_host()
        ->property_trees()
        ->scroll_tree.Node(layer->scroll_tree_index())
        ->main_thread_scrolling_reasons;
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

  LayoutEmbeddedContent* layout_embedded_content =
      ToLayoutEmbeddedContent(layout_object);
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
  ASSERT_TRUE(cc_scroll_layer->scrollable());
  ASSERT_TRUE(
      GetMainThreadScrollingReasons(cc_scroll_layer) &
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

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
  ASSERT_TRUE(cc_scroll_layer->scrollable());
  ASSERT_FALSE(
      GetMainThreadScrollingReasons(cc_scroll_layer) &
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

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
  ASSERT_TRUE(cc_scroll_layer->scrollable());
  ASSERT_TRUE(
      GetMainThreadScrollingReasons(cc_scroll_layer) &
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
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

  EXPECT_TRUE(
      GetViewMainThreadScrollingReasons() &
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

  // The main thread scrolling reason should be reset upon the following change.
  element->setAttribute("style", "", ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  EXPECT_FALSE(GetViewMainThreadScrollingReasons());
}

TEST_F(MainThreadScrollingReasonsTest, FastScrollingCanBeDisabledWithSetting) {
  GetWebView()->MainFrameWidget()->Resize(WebSize(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  GetWebView()->GetSettings()->SetThreadedScrollingEnabled(false);
  GetFrame()->View()->SetNeedsPaintPropertyUpdate();
  ForceFullCompositingUpdate();

  // Main scrolling should be enabled with the setting override.
  EXPECT_TRUE(GetViewMainThreadScrollingReasons());

  // Main scrolling should also propagate to inner viewport layer.
  const cc::Layer* visual_viewport_scroll_layer =
      GetFrame()->GetPage()->GetVisualViewport().LayerForScrolling();
  ASSERT_TRUE(visual_viewport_scroll_layer->scrollable());
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
  GetWebView()->MainFrameWidget()->Resize(WebSize(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  ForceFullCompositingUpdate();

  // Fast scrolling should be enabled by default.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());

  const cc::Layer* visual_viewport_scroll_layer =
      GetFrame()->GetPage()->GetVisualViewport().LayerForScrolling();
  ASSERT_TRUE(visual_viewport_scroll_layer->scrollable());
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
  ASSERT_TRUE(layout_object->IsBox());
  LayoutBox* box = ToLayoutBox(layout_object);
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
      cc::MainThreadScrollingReason::kHasOpacityAndLCDText |
      cc::MainThreadScrollingReason::kHasTransformAndLCDText |
      cc::MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText |
      cc::MainThreadScrollingReason::kIsNotStackingContextAndLCDText;

 protected:
  NonCompositedMainThreadScrollingReasonsTest() {
    RegisterMockedHttpURLLoad("two_scrollable_area.html");
    NavigateTo(base_url_ + "two_scrollable_area.html");
  }
  void TestNonCompositedReasons(const String& target, const uint32_t reason) {
    GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
    Document* document = GetFrame()->GetDocument();
    Element* container = document->getElementById("scroller1");
    container->setAttribute("class", target.Utf8().c_str(),
                            ASSERT_NO_EXCEPTION);
    ForceFullCompositingUpdate();

    PaintLayerScrollableArea* scrollable_area =
        ToLayoutBoxModelObject(container->GetLayoutObject())
            ->GetScrollableArea();
    ASSERT_TRUE(scrollable_area);
    EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
                reason);

    Element* container2 = document->getElementById("scroller2");
    PaintLayerScrollableArea* scrollable_area2 =
        ToLayoutBoxModelObject(container2->GetLayoutObject())
            ->GetScrollableArea();
    ASSERT_TRUE(scrollable_area2);
    // Different scrollable area should remain unaffected.
    EXPECT_FALSE(
        scrollable_area2->GetNonCompositedMainThreadScrollingReasons() &
        reason);

    LocalFrameView* frame_view = GetFrame()->View();
    ASSERT_TRUE(frame_view);
    EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & reason);

    // Remove attribute from the scroller 1 would lead to scroll on impl.
    container->removeAttribute("class");
    ForceFullCompositingUpdate();

    EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
                 reason);
    EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & reason);

    // Add target attribute would again lead to scroll on main thread
    container->setAttribute("class", target.Utf8().c_str(),
                            ASSERT_NO_EXCEPTION);
    ForceFullCompositingUpdate();

    EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
                reason);
    EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & reason);

    if ((reason & kLCDTextRelatedReasons) &&
        !(reason & ~kLCDTextRelatedReasons)) {
      GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
      ForceFullCompositingUpdate();
      EXPECT_FALSE(
          scrollable_area->GetNonCompositedMainThreadScrollingReasons());
      EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons());
    }
  }
};

TEST_F(NonCompositedMainThreadScrollingReasonsTest, TransparentTest) {
  TestNonCompositedReasons(
      "transparent", cc::MainThreadScrollingReason::kHasOpacityAndLCDText);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, TransformTest) {
  TestNonCompositedReasons(
      "transform", cc::MainThreadScrollingReason::kHasTransformAndLCDText);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, BackgroundNotOpaqueTest) {
  TestNonCompositedReasons(
      "background-not-opaque",
      cc::MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, ClipTest) {
  TestNonCompositedReasons(
      "clip", cc::MainThreadScrollingReason::kHasClipRelatedProperty);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, ClipPathTest) {
  uint32_t clip_reason = cc::MainThreadScrollingReason::kHasClipRelatedProperty;
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  Document* document = GetFrame()->GetDocument();
  // Test ancestor with ClipPath
  Element* element = document->body();
  element->setAttribute(html_names::kStyleAttr,
                        "clip-path:circle(115px at 20px 20px);");
  Element* container = document->getElementById("scroller1");
  ASSERT_TRUE(container);
  ForceFullCompositingUpdate();

  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
              clip_reason);

  EXPECT_FALSE(GetViewMainThreadScrollingReasons() & clip_reason);

  // Remove clip path from ancestor.
  element->removeAttribute(html_names::kStyleAttr);
  ForceFullCompositingUpdate();

  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
               clip_reason);
  EXPECT_FALSE(GetViewMainThreadScrollingReasons() & clip_reason);

  // Test descendant with ClipPath
  element = document->getElementById("content1");
  ASSERT_TRUE(element);
  element->setAttribute(html_names::kStyleAttr,
                        "clip-path:circle(115px at 20px 20px);");
  ForceFullCompositingUpdate();
  EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
              clip_reason);
  EXPECT_FALSE(GetViewMainThreadScrollingReasons() & clip_reason);

  // Remove clip path from descendant.
  element->removeAttribute(html_names::kStyleAttr);
  ForceFullCompositingUpdate();
  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
               clip_reason);
  EXPECT_FALSE(GetViewMainThreadScrollingReasons() & clip_reason);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, LCDTextEnabledTest) {
  TestNonCompositedReasons(
      "transparent", cc::MainThreadScrollingReason::kHasOpacityAndLCDText);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, BoxShadowTest) {
  TestNonCompositedReasons(
      "box-shadow",
      cc::MainThreadScrollingReason::kHasBoxShadowFromNonRootLayer);
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest, StackingContextTest) {
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);

  Document* document = GetFrame()->GetDocument();
  Element* container = document->getElementById("scroller1");
  ASSERT_TRUE(container);

  ForceFullCompositingUpdate();

  // If a scroller contains all its children, it's not a stacking context.
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
              cc::MainThreadScrollingReason::kIsNotStackingContextAndLCDText);

  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  ForceFullCompositingUpdate();
  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
               cc::MainThreadScrollingReason::kIsNotStackingContextAndLCDText);
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);

  // Adding "contain: paint" to force a stacking context leads to promotion.
  container->setAttribute("style", "contain: paint", ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons());
}

TEST_F(NonCompositedMainThreadScrollingReasonsTest,
       CompositedWithLCDTextRelatedReasonsTest) {
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

  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons());

  Element* container2 = document->getElementById("scroller2");
  ASSERT_TRUE(container2);
  container2->setAttribute("class", "composited border-radius",
                           ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();
  PaintLayerScrollableArea* scrollable_area2 =
      ToLayoutBoxModelObject(container2->GetLayoutObject())
          ->GetScrollableArea();
  ASSERT_TRUE(scrollable_area2);
  ASSERT_TRUE(scrollable_area2->UsesCompositedScrolling());
}

}  // namespace blink
