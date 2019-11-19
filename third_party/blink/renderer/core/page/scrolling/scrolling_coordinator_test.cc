/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"

#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_and_scale_set.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/sticky_position_constraint.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator_context.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class ScrollingCoordinatorTest : public testing::Test,
                                 public testing::WithParamInterface<bool> {
 public:
  ScrollingCoordinatorTest() : base_url_("http://www.test.com/") {
    helper_.Initialize(nullptr, nullptr, nullptr, &ConfigureSettings);
    GetWebView()->MainFrameWidget()->Resize(IntSize(320, 240));
    GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  ~ScrollingCoordinatorTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void NavigateTo(const std::string& url) {
    frame_test_helpers::LoadFrame(GetWebView()->MainFrameImpl(), url);
  }

  void LoadHTML(const std::string& html) {
    frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(), html,
                                       url_test_helpers::ToKURL("about:blank"));
  }

  void ForceFullCompositingUpdate() {
    GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }
  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }
  frame_test_helpers::TestWebWidgetClient* GetWidgetClient() const {
    return helper_.GetWebWidgetClient();
  }

  void LoadAhem() { helper_.LoadAhem(); }

  bool HasMainThreadScrollingReasons(const cc::Layer* layer) const {
    return layer->layer_tree_host()
        ->property_trees()
        ->scroll_tree.Node(layer->scroll_tree_index())
        ->main_thread_scrolling_reasons;
  }

 protected:
  std::string base_url_;

 private:
  static void ConfigureSettings(WebSettings* settings) {
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  frame_test_helpers::WebViewHelper helper_;
};

INSTANTIATE_TEST_SUITE_P(All, ScrollingCoordinatorTest, testing::Bool());

TEST_P(ScrollingCoordinatorTest, fastScrollingByDefault) {
  GetWebView()->MainFrameWidget()->Resize(WebSize(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  ForceFullCompositingUpdate();

  // Make sure the scrolling coordinator is active.
  LocalFrameView* frame_view = GetFrame()->View();
  Page* page = GetFrame()->GetPage();
  ASSERT_TRUE(page->GetScrollingCoordinator());
  ASSERT_TRUE(page->GetScrollingCoordinator()->CoordinatesScrollingForFrameView(
      frame_view));

  // Fast scrolling should be enabled by default.
  const cc::Layer* root_scroll_layer =
      GetFrame()->View()->LayoutViewport()->LayerForScrolling();
  EXPECT_FALSE(HasMainThreadScrollingReasons(root_scroll_layer));
  EXPECT_TRUE(root_scroll_layer->scrollable());

  ASSERT_EQ(cc::EventListenerProperties::kNone,
            GetWidgetClient()->EventListenerProperties(
                cc::EventListenerClass::kTouchStartOrMove));
  ASSERT_EQ(cc::EventListenerProperties::kNone,
            GetWidgetClient()->EventListenerProperties(
                cc::EventListenerClass::kMouseWheel));

  const cc::Layer* inner_viewport_scroll_layer =
      page->GetVisualViewport().LayerForScrolling();
  EXPECT_FALSE(HasMainThreadScrollingReasons(inner_viewport_scroll_layer));
  EXPECT_TRUE(inner_viewport_scroll_layer->scrollable());
}

TEST_P(ScrollingCoordinatorTest, fastFractionalScrollingDiv) {
  RegisterMockedHttpURLLoad("fractional-scroll-div.html");
  NavigateTo(base_url_ + "fractional-scroll-div.html");
  ForceFullCompositingUpdate();

  Document* document = GetFrame()->GetDocument();
  Element* scrollable_element = document->getElementById("scroller");
  DCHECK(scrollable_element);

  scrollable_element->setScrollTop(1.0);
  scrollable_element->setScrollLeft(1.0);
  ForceFullCompositingUpdate();

  // Make sure the fractional scroll offset change 1.0 -> 1.2 gets propagated
  // to compositor.
  scrollable_element->setScrollTop(1.2);
  scrollable_element->setScrollLeft(1.2);
  ForceFullCompositingUpdate();

  LayoutObject* layout_object = scrollable_element->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  LayoutBox* box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->UsesCompositedScrolling());
  CompositedLayerMapping* composited_layer_mapping =
      box->Layer()->GetCompositedLayerMapping();
  ASSERT_TRUE(composited_layer_mapping->HasScrollingLayer());
  DCHECK(composited_layer_mapping->ScrollingContentsLayer());
  cc::Layer* cc_scroll_layer =
      composited_layer_mapping->ScrollingContentsLayer()->CcLayer();
  ASSERT_TRUE(cc_scroll_layer);
  ASSERT_NEAR(1.2f, cc_scroll_layer->CurrentScrollOffset().x(), 0.01f);
  ASSERT_NEAR(1.2f, cc_scroll_layer->CurrentScrollOffset().y(), 0.01f);
}

TEST_P(ScrollingCoordinatorTest, fastScrollingForFixedPosition) {
  RegisterMockedHttpURLLoad("fixed-position.html");
  NavigateTo(base_url_ + "fixed-position.html");
  ForceFullCompositingUpdate();

  const cc::Layer* root_scroll_layer =
      GetFrame()->View()->LayoutViewport()->LayerForScrolling();
  EXPECT_FALSE(HasMainThreadScrollingReasons(root_scroll_layer));
}

// Sticky constraints are stored on transform property tree nodes.
static cc::StickyPositionConstraint GetStickyConstraint(Element* element) {
  const auto* properties =
      element->GetLayoutObject()->FirstFragment().PaintProperties();
  DCHECK(properties);
  return *properties->StickyTranslation()->GetStickyConstraint();
}

TEST_P(ScrollingCoordinatorTest, fastScrollingForStickyPosition) {
  RegisterMockedHttpURLLoad("sticky-position.html");
  NavigateTo(base_url_ + "sticky-position.html");
  ForceFullCompositingUpdate();

  // Sticky position should not fall back to main thread scrolling.
  const cc::Layer* root_scroll_layer =
      GetFrame()->View()->LayoutViewport()->LayerForScrolling();
  EXPECT_FALSE(HasMainThreadScrollingReasons(root_scroll_layer));

  Document* document = GetFrame()->GetDocument();
  {
    Element* element = document->getElementById("div-tl");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right &&
                !constraint.is_anchored_bottom);
    EXPECT_EQ(1.f, constraint.top_offset);
    EXPECT_EQ(1.f, constraint.left_offset);
    EXPECT_EQ(gfx::Rect(100, 100, 10, 10),
              constraint.scroll_container_relative_sticky_box_rect);
    EXPECT_EQ(gfx::Rect(100, 100, 200, 200),
              constraint.scroll_container_relative_containing_block_rect);
  }
  {
    Element* element = document->getElementById("div-tr");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && !constraint.is_anchored_left &&
                constraint.is_anchored_right && !constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("div-bl");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(!constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right && constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("div-br");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(!constraint.is_anchored_top && !constraint.is_anchored_left &&
                constraint.is_anchored_right && constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("span-tl");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right &&
                !constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("span-tlbr");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                constraint.is_anchored_right && constraint.is_anchored_bottom);
    EXPECT_EQ(1.f, constraint.top_offset);
    EXPECT_EQ(1.f, constraint.left_offset);
    EXPECT_EQ(1.f, constraint.right_offset);
    EXPECT_EQ(1.f, constraint.bottom_offset);
  }
  {
    Element* element = document->getElementById("composited-top");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top);
    EXPECT_EQ(gfx::Rect(100, 110, 10, 10),
              constraint.scroll_container_relative_sticky_box_rect);
    EXPECT_EQ(gfx::Rect(100, 100, 200, 200),
              constraint.scroll_container_relative_containing_block_rect);
  }
}

TEST_P(ScrollingCoordinatorTest, elementPointerEventHandler) {
  LoadHTML(R"HTML(
    <div id="pointer" style="width: 100px; height: 100px;"></div>
    <script>
      pointer.addEventListener('pointerdown', function(event) {
      }, {blocking: false} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  auto* layout_view = GetFrame()->View()->GetLayoutView();
  auto* mapping = layout_view->Layer()->GetCompositedLayerMapping();
  GraphicsLayer* graphics_layer = mapping->ScrollingContentsLayer();
  cc::Layer* cc_layer = graphics_layer->CcLayer();

  // Pointer event handlers should not generate blocking touch action regions.
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingCoordinatorTest, touchEventHandler) {
  RegisterMockedHttpURLLoad("touch-event-handler.html");
  NavigateTo(base_url_ + "touch-event-handler.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kBlocking,
            GetWidgetClient()->EventListenerProperties(
                cc::EventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingCoordinatorTest, elementBlockingTouchEventHandler) {
  LoadHTML(R"HTML(
    <div id="blocking" style="width: 100px; height: 100px;"></div>
    <script>
      blocking.addEventListener('touchstart', function(event) {
      }, {passive: false} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  auto* layout_view = GetFrame()->View()->GetLayoutView();
  auto* mapping = layout_view->Layer()->GetCompositedLayerMapping();
  cc::Layer* cc_layer = mapping->ScrollingContentsLayer()->CcLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 100, 100));
}

TEST_P(ScrollingCoordinatorTest, touchEventHandlerPassive) {
  RegisterMockedHttpURLLoad("touch-event-handler-passive.html");
  NavigateTo(base_url_ + "touch-event-handler-passive.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kPassive,
            GetWidgetClient()->EventListenerProperties(
                cc::EventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingCoordinatorTest, elementTouchEventHandlerPassive) {
  LoadHTML(R"HTML(
    <div id="passive" style="width: 100px; height: 100px;"></div>
    <script>
      passive.addEventListener('touchstart', function(event) {
      }, {passive: true} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  auto* layout_view = GetFrame()->View()->GetLayoutView();
  auto* mapping = layout_view->Layer()->GetCompositedLayerMapping();
  GraphicsLayer* graphics_layer = mapping->ScrollingContentsLayer();
  cc::Layer* cc_layer = graphics_layer->CcLayer();

  // Passive event handlers should not generate blocking touch action regions.
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingCoordinatorTest, TouchActionRectsOnImage) {
  LoadHTML(R"HTML(
    <img id="image" style="width: 100px; height: 100px; touch-action: none;">
  )HTML");
  ForceFullCompositingUpdate();

  auto* layout_view = GetFrame()->View()->GetLayoutView();
  auto* mapping = layout_view->Layer()->GetCompositedLayerMapping();
  cc::Layer* cc_layer = mapping->ScrollingContentsLayer()->CcLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 100, 100));
}

TEST_P(ScrollingCoordinatorTest, touchEventHandlerBoth) {
  RegisterMockedHttpURLLoad("touch-event-handler-both.html");
  NavigateTo(base_url_ + "touch-event-handler-both.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kBlockingAndPassive,
            GetWidgetClient()->EventListenerProperties(
                cc::EventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingCoordinatorTest, wheelEventHandler) {
  RegisterMockedHttpURLLoad("wheel-event-handler.html");
  NavigateTo(base_url_ + "wheel-event-handler.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kBlocking,
            GetWidgetClient()->EventListenerProperties(
                cc::EventListenerClass::kMouseWheel));
}

TEST_P(ScrollingCoordinatorTest, wheelEventHandlerPassive) {
  RegisterMockedHttpURLLoad("wheel-event-handler-passive.html");
  NavigateTo(base_url_ + "wheel-event-handler-passive.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kPassive,
            GetWidgetClient()->EventListenerProperties(
                cc::EventListenerClass::kMouseWheel));
}

TEST_P(ScrollingCoordinatorTest, wheelEventHandlerBoth) {
  RegisterMockedHttpURLLoad("wheel-event-handler-both.html");
  NavigateTo(base_url_ + "wheel-event-handler-both.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kBlockingAndPassive,
            GetWidgetClient()->EventListenerProperties(
                cc::EventListenerClass::kMouseWheel));
}

TEST_P(ScrollingCoordinatorTest, scrollEventHandler) {
  RegisterMockedHttpURLLoad("scroll-event-handler.html");
  NavigateTo(base_url_ + "scroll-event-handler.html");
  ForceFullCompositingUpdate();

  ASSERT_TRUE(GetWidgetClient()->HaveScrollEventHandlers());
}

TEST_P(ScrollingCoordinatorTest, updateEventHandlersDuringTeardown) {
  RegisterMockedHttpURLLoad("scroll-event-handler-window.html");
  NavigateTo(base_url_ + "scroll-event-handler-window.html");
  ForceFullCompositingUpdate();

  // Simulate detaching the document from its DOM window. This should not
  // cause a crash when the WebViewImpl is closed by the test runner.
  GetFrame()->GetDocument()->Shutdown();
}

TEST_P(ScrollingCoordinatorTest, clippedBodyTest) {
  RegisterMockedHttpURLLoad("clipped-body.html");
  NavigateTo(base_url_ + "clipped-body.html");
  ForceFullCompositingUpdate();

  const auto* root_scroll_layer =
      GetFrame()->View()->LayoutViewport()->LayerForScrolling();
  EXPECT_TRUE(root_scroll_layer->non_fast_scrollable_region().IsEmpty());
}

TEST_P(ScrollingCoordinatorTest, touchAction) {
  RegisterMockedHttpURLLoad("touch-action.html");
  NavigateTo(base_url_ + "touch-action.html");
  ForceFullCompositingUpdate();

  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById("scrollable");
  LayoutBox* box = ToLayoutBox(scrollable_element->GetLayoutObject());
  ASSERT_TRUE(box->UsesCompositedScrolling());
  ASSERT_EQ(kPaintsIntoOwnBacking, box->Layer()->GetCompositingState());

  auto* composited_layer_mapping = box->Layer()->GetCompositedLayerMapping();
  auto* graphics_layer = composited_layer_mapping->ScrollingContentsLayer();
  cc::Layer* cc_layer = graphics_layer->CcLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanX | TouchAction::kTouchActionPanDown);
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 1000, 1000));
}

TEST_P(ScrollingCoordinatorTest, touchActionRegions) {
  RegisterMockedHttpURLLoad("touch-action-regions.html");
  NavigateTo(base_url_ + "touch-action-regions.html");
  ForceFullCompositingUpdate();

  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById("scrollable");
  LayoutBox* box = ToLayoutBox(scrollable_element->GetLayoutObject());
  ASSERT_TRUE(box->UsesCompositedScrolling());
  ASSERT_EQ(kPaintsIntoOwnBacking, box->Layer()->GetCompositingState());

  auto* composited_layer_mapping = box->Layer()->GetCompositedLayerMapping();
  auto* graphics_layer = composited_layer_mapping->ScrollingContentsLayer();
  cc::Layer* cc_layer = graphics_layer->CcLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanDown | TouchAction::kTouchActionPanX);
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 100, 100));

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanDown | TouchAction::kTouchActionPanRight);
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 50, 50));

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanDown);
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 100, 100, 100));
}

TEST_P(ScrollingCoordinatorTest, touchActionNesting) {
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        overflow: scroll;
      }
      #touchaction {
        touch-action: pan-x;
        width: 100px;
        height: 100px;
        margin: 5px;
      }
      #child {
        width: 150px;
        height: 50px;
      }
    </style>
    <div id="scrollable">
      <div id="touchaction">
        <div id="child"></div>
      </div>
      <div id="forcescroll" style="width: 1000px; height: 1000px;"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* scrollable = GetFrame()->GetDocument()->getElementById("scrollable");
  auto* box = ToLayoutBox(scrollable->GetLayoutObject());
  auto* composited_layer_mapping = box->Layer()->GetCompositedLayerMapping();
  auto* graphics_layer = composited_layer_mapping->ScrollingContentsLayer();
  cc::Layer* cc_layer = graphics_layer->CcLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanX);
  EXPECT_EQ(region.GetRegionComplexity(), 2);
  EXPECT_EQ(region.bounds(), gfx::Rect(5, 5, 150, 100));
}

TEST_P(ScrollingCoordinatorTest, nestedTouchActionInvalidation) {
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        overflow: scroll;
      }
      #touchaction {
        touch-action: pan-x;
        width: 100px;
        height: 100px;
        margin: 5px;
      }
      #child {
        width: 150px;
        height: 50px;
      }
    </style>
    <div id="scrollable">
      <div id="touchaction">
        <div id="child"></div>
      </div>
      <div id="forcescroll" style="width: 1000px; height: 1000px;"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* scrollable = GetFrame()->GetDocument()->getElementById("scrollable");
  auto* box = ToLayoutBox(scrollable->GetLayoutObject());
  auto* composited_layer_mapping = box->Layer()->GetCompositedLayerMapping();
  auto* graphics_layer = composited_layer_mapping->ScrollingContentsLayer();
  cc::Layer* cc_layer = graphics_layer->CcLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanX);
  EXPECT_EQ(region.GetRegionComplexity(), 2);
  EXPECT_EQ(region.bounds(), gfx::Rect(5, 5, 150, 100));

  scrollable->setAttribute("style", "touch-action: none", ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanX);
  EXPECT_TRUE(region.IsEmpty());
}

// Similar to nestedTouchActionInvalidation but tests that an ancestor with
// touch-action: pan-x and a descendant with touch-action: pan-y results in a
// touch-action rect of none for the descendant.
TEST_P(ScrollingCoordinatorTest, nestedTouchActionChangesUnion) {
  LoadHTML(R"HTML(
    <style>
      #ancestor {
        width: 100px;
        height: 100px;
      }
      #child {
        touch-action: pan-x;
        width: 150px;
        height: 50px;
      }
    </style>
    <div id="ancestor">
      <div id="child"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* layout_view = GetFrame()->View()->GetLayoutView();
  auto* mapping = layout_view->Layer()->GetCompositedLayerMapping();
  GraphicsLayer* graphics_layer = mapping->ScrollingContentsLayer();
  cc::Layer* cc_layer = graphics_layer->CcLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanX);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 150, 50));
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_TRUE(region.IsEmpty());

  Element* ancestor = GetFrame()->GetDocument()->getElementById("ancestor");
  ancestor->setAttribute("style", "touch-action: pan-y", ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanY);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 100, 100));
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanX);
  EXPECT_TRUE(region.IsEmpty());
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 150, 50));
}

// Box shadow is not hit testable and should not be included in touch action.
TEST_P(ScrollingCoordinatorTest, touchActionExcludesBoxShadow) {
  LoadHTML(R"HTML(
    <style>
      #shadow {
        width: 100px;
        height: 100px;
        touch-action: none;
        box-shadow: 10px 5px 5px red;
      }
    </style>
    <div id="shadow"></div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* layout_view = GetFrame()->View()->GetLayoutView();
  auto* mapping = layout_view->Layer()->GetCompositedLayerMapping();
  GraphicsLayer* graphics_layer = mapping->ScrollingContentsLayer();
  cc::Layer* cc_layer = graphics_layer->CcLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 100, 100));
}

TEST_P(ScrollingCoordinatorTest, touchActionOnInline) {
  RegisterMockedHttpURLLoad("touch-action-on-inline.html");
  NavigateTo(base_url_ + "touch-action-on-inline.html");
  LoadAhem();
  ForceFullCompositingUpdate();

  auto* layout_view = GetFrame()->View()->GetLayoutView();
  auto* mapping = layout_view->Layer()->GetCompositedLayerMapping();
  GraphicsLayer* graphics_layer = mapping->ScrollingContentsLayer();
  cc::Layer* cc_layer = graphics_layer->CcLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 80, 50));
}

TEST_P(ScrollingCoordinatorTest, touchActionWithVerticalRLWritingMode) {
  RegisterMockedHttpURLLoad("touch-action-with-vertical-rl-writing-mode.html");
  NavigateTo(base_url_ + "touch-action-with-vertical-rl-writing-mode.html");
  LoadAhem();
  ForceFullCompositingUpdate();

  auto* layout_view = GetFrame()->View()->GetLayoutView();
  auto* mapping = layout_view->Layer()->GetCompositedLayerMapping();
  GraphicsLayer* graphics_layer = mapping->ScrollingContentsLayer();
  cc::Layer* cc_layer = graphics_layer->CcLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(292, 8, 20, 80));
}

TEST_P(ScrollingCoordinatorTest, touchActionBlockingHandler) {
  RegisterMockedHttpURLLoad("touch-action-blocking-handler.html");
  NavigateTo(base_url_ + "touch-action-blocking-handler.html");
  ForceFullCompositingUpdate();

  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById("scrollable");
  LayoutBox* box = ToLayoutBox(scrollable_element->GetLayoutObject());
  ASSERT_TRUE(box->UsesCompositedScrolling());
  ASSERT_EQ(kPaintsIntoOwnBacking, box->Layer()->GetCompositingState());

  auto* composited_layer_mapping = box->Layer()->GetCompositedLayerMapping();
  auto* graphics_layer = composited_layer_mapping->ScrollingContentsLayer();
  cc::Layer* cc_layer = graphics_layer->CcLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 100, 100));

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanY);
  EXPECT_EQ(region.GetRegionComplexity(), 2);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 1000, 1000));
}

TEST_P(ScrollingCoordinatorTest, touchActionOnScrollingElement) {
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 100px;
        height: 100px;
        overflow: scroll;
        touch-action: pan-y;
      }
      #child {
        width: 50px;
        height: 150px;
      }
    </style>
    <div id="scrollable">
      <div id="child"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById("scrollable");
  LayoutBox* box = ToLayoutBox(scrollable_element->GetLayoutObject());
  auto* composited_layer_mapping = box->Layer()->GetCompositedLayerMapping();

  // The outer layer (not scrollable) will be fully marked as pan-y (100x100)
  // and the scrollable layer will only have the contents marked as pan-y
  // (50x150).
  auto* scrolling_contents_layer =
      composited_layer_mapping->ScrollingContentsLayer()->CcLayer();
  cc::Region region =
      scrolling_contents_layer->touch_action_region().GetRegionForTouchAction(
          TouchAction::kTouchActionPanY);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 50, 150));

  auto* non_scrolling_layer =
      composited_layer_mapping->MainGraphicsLayer()->CcLayer();
  region = non_scrolling_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionPanY);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 100, 100));
}

TEST_P(ScrollingCoordinatorTest, IframeWindowTouchHandler) {
  LoadHTML(
      R"(<iframe style="width: 275px; height: 250px;"></iframe>)");
  auto* child_frame =
      To<WebLocalFrameImpl>(GetWebView()->MainFrameImpl()->FirstChild());
  frame_test_helpers::LoadHTMLString(child_frame, R"HTML(
      <p style="margin: 1000px"> Hello </p>
      <script>
        window.addEventListener('touchstart', (e) => {
          e.preventDefault();
        }, {passive: false});
      </script>
    )HTML",
                                     url_test_helpers::ToKURL("about:blank"));
  ForceFullCompositingUpdate();

  PaintLayer* paint_layer_child_frame =
      child_frame->GetFrame()->GetDocument()->GetLayoutView()->Layer();
  auto* child_mapping = paint_layer_child_frame->GetCompositedLayerMapping();
  // Touch action regions are stored on the layer that draws the background.
  auto* child_graphics_layer = child_mapping->ScrollingContentsLayer();

  cc::Region region_child_frame =
      child_graphics_layer->CcLayer()
          ->touch_action_region()
          .GetRegionForTouchAction(TouchAction::kTouchActionNone);
  PaintLayer* paint_layer_main_frame = GetWebView()
                                           ->MainFrameImpl()
                                           ->GetFrame()
                                           ->GetDocument()
                                           ->GetLayoutView()
                                           ->Layer();
  cc::Region region_main_frame =
      paint_layer_main_frame
          ->EnclosingLayerForPaintInvalidationCrossingFrameBoundaries()
          ->GraphicsLayerBacking(&paint_layer_main_frame->GetLayoutObject())
          ->CcLayer()
          ->touch_action_region()
          .GetRegionForTouchAction(TouchAction::kTouchActionNone);
  EXPECT_TRUE(region_main_frame.bounds().IsEmpty());
  EXPECT_FALSE(region_child_frame.bounds().IsEmpty());
  // We only check for the content size for verification as the offset is 0x0
  // due to child frame having its own composited layer.

  // Because touch action rects are painted on the scrolling contents layer,
  // the size of the rect should be equal to the entire scrolling contents area.
  EXPECT_EQ(child_graphics_layer->Size(),
            gfx::Size(region_child_frame.bounds().size()));
}

TEST_P(ScrollingCoordinatorTest, WindowTouchEventHandler) {
  LoadHTML(R"HTML(
    <style>
      html { width: 200px; height: 200px; }
      body { width: 100px; height: 100px; }
    </style>
    <script>
      window.addEventListener('touchstart', function(event) {
        event.preventDefault();
      }, {passive: false} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  auto* layout_view = GetFrame()->View()->GetLayoutView();
  auto* mapping = layout_view->Layer()->GetCompositedLayerMapping();
  // Touch action regions are stored on the layer that draws the background.
  auto* graphics_layer = mapping->ScrollingContentsLayer();

  // The touch action region should include the entire frame, even though the
  // document is smaller than the frame.
  cc::Region region =
      graphics_layer->CcLayer()->touch_action_region().GetRegionForTouchAction(
          TouchAction::kTouchActionNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 320, 240));
}

namespace {
class ScrollingCoordinatorMockEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override {}
};
}  // namespace

TEST_P(ScrollingCoordinatorTest, WindowTouchEventHandlerInvalidation) {
  LoadHTML("");
  ForceFullCompositingUpdate();

  auto* layout_view = GetFrame()->View()->GetLayoutView();
  auto* mapping = layout_view->Layer()->GetCompositedLayerMapping();
  // Touch action regions are stored on the layer that draws the background.
  auto* graphics_layer = mapping->ScrollingContentsLayer();
  auto* cc_layer = graphics_layer->CcLayer();

  // Initially there are no touch action regions.
  auto region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_TRUE(region.IsEmpty());

  // Adding a blocking window event handler should create a touch action region.
  auto* listener =
      MakeGarbageCollected<ScrollingCoordinatorMockEventListener>();
  auto* resolved_options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  resolved_options->setPassive(false);
  GetFrame()->DomWindow()->addEventListener(event_type_names::kTouchstart,
                                            listener, resolved_options);
  ForceFullCompositingUpdate();
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_FALSE(region.IsEmpty());

  // Removing the window event handler also removes the blocking touch action
  // region.
  GetFrame()->DomWindow()->RemoveAllEventListeners();
  ForceFullCompositingUpdate();
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_TRUE(region.IsEmpty());
}

// Ensure we don't crash when a plugin becomes a LayoutInline
TEST_P(ScrollingCoordinatorTest, PluginBecomesLayoutInline) {
  LoadHTML(R"HTML(
    <style>
      body {
        margin: 0;
        height: 3000px;
      }
    </style>
    <object id="plugin" type="appilcation/x-webkit-test-plugin"></object>
    <script>
      document.getElementById("plugin")
              .appendChild(document.createElement("label"))
    </script>
  )HTML");

  // This test passes if it doesn't crash. We're trying to make sure
  // ScrollingCoordinator can deal with LayoutInline plugins when generating
  // NonFastScrollableRegions.
  HTMLObjectElement* plugin =
      ToHTMLObjectElement(GetFrame()->GetDocument()->getElementById("plugin"));
  ASSERT_TRUE(plugin->GetLayoutObject()->IsLayoutInline());
  ForceFullCompositingUpdate();
}

// Ensure NonFastScrollableRegions are correctly generated for both fixed and
// in-flow plugins that need them.
TEST_P(ScrollingCoordinatorTest, NonFastScrollableRegionsForPlugins) {
  LoadHTML(R"HTML(
    <style>
      body {
        margin: 0;
        height: 3000px;
      }
      #plugin {
        width: 300px;
        height: 300px;
      }
      #pluginfixed {
        width: 200px;
        height: 200px;
      }
      #fixed {
        position: fixed;
        top: 500px;
      }
    </style>
    <div id="fixed">
      <object id="pluginfixed" type="application/x-webkit-test-plugin"></object>
    </div>
    <object id="plugin" type="application/x-webkit-test-plugin"></object>
  )HTML");

  HTMLObjectElement* plugin =
      ToHTMLObjectElement(GetFrame()->GetDocument()->getElementById("plugin"));
  HTMLObjectElement* plugin_fixed = ToHTMLObjectElement(
      GetFrame()->GetDocument()->getElementById("pluginfixed"));
  // NonFastScrollableRegions are generated for plugins that require wheel
  // events.
  plugin->OwnedPlugin()->SetWantsWheelEvents(true);
  plugin_fixed->OwnedPlugin()->SetWantsWheelEvents(true);

  ForceFullCompositingUpdate();

  // The non-fixed plugin should create a non-fast scrollable region in the
  // scrolling contents layer of the LayoutView.
  auto* layout_viewport = GetFrame()->View()->LayoutViewport();
  auto* mapping = layout_viewport->Layer()->GetCompositedLayerMapping();
  auto* viewport_non_fast_layer = mapping->ScrollingContentsLayer()->CcLayer();
  EXPECT_EQ(viewport_non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(0, 0, 300, 300));

  // The fixed plugin should create a non-fast scrollable region in a fixed
  // cc::Layer.
  auto* fixed = GetFrame()->GetDocument()->getElementById("fixed");
  auto* fixed_object = ToLayoutBox(fixed->GetLayoutObject());
  auto* fixed_graphics_layer =
      fixed_object->EnclosingLayer()->GraphicsLayerBacking(fixed_object);
  EXPECT_EQ(
      fixed_graphics_layer->CcLayer()->non_fast_scrollable_region().bounds(),
      gfx::Rect(0, 0, 200, 200));
}

TEST_P(ScrollingCoordinatorTest, NonFastScrollableRegionWithBorder) {
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            body { margin: 0; }
            #scroller {
              height: 100px;
              width: 100px;
              overflow-y: scroll;
              border: 10px solid black;
            }
          </style>
          <div id="scroller">
            <div id="forcescroll" style="height: 1000px;"></div>
          </div>
      )HTML");
  ForceFullCompositingUpdate();

  auto* non_fast_layer =
      GetFrame()->View()->LayoutViewport()->LayerForScrolling();
  EXPECT_EQ(non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(0, 0, 120, 120));
}

TEST_P(ScrollingCoordinatorTest, overflowScrolling) {
  RegisterMockedHttpURLLoad("overflow-scrolling.html");
  NavigateTo(base_url_ + "overflow-scrolling.html");
  ForceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the cc::Layer.
  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById("scrollable");
  DCHECK(scrollable_element);

  LayoutObject* layout_object = scrollable_element->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  ASSERT_TRUE(layout_object->HasLayer());

  LayoutBox* box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->UsesCompositedScrolling());
  ASSERT_EQ(kPaintsIntoOwnBacking, box->Layer()->GetCompositingState());

  CompositedLayerMapping* composited_layer_mapping =
      box->Layer()->GetCompositedLayerMapping();
  ASSERT_TRUE(composited_layer_mapping->HasScrollingLayer());
  DCHECK(composited_layer_mapping->ScrollingContentsLayer());

  cc::Layer* cc_scroll_layer =
      composited_layer_mapping->ScrollingContentsLayer()->CcLayer();
  ASSERT_TRUE(cc_scroll_layer->scrollable());
  ASSERT_TRUE(cc_scroll_layer->GetUserScrollableHorizontal());
  ASSERT_TRUE(cc_scroll_layer->GetUserScrollableVertical());

  // Now verify we've attached cc scrollbar layers onto the scrollbar graphics
  // layers.
  ASSERT_TRUE(composited_layer_mapping->LayerForHorizontalScrollbar());
  ASSERT_TRUE(composited_layer_mapping->LayerForHorizontalScrollbar()
                  ->HasContentsLayer());
  ASSERT_TRUE(composited_layer_mapping->LayerForVerticalScrollbar());
  ASSERT_TRUE(composited_layer_mapping->LayerForVerticalScrollbar()
                  ->HasContentsLayer());
}

TEST_P(ScrollingCoordinatorTest, overflowHidden) {
  RegisterMockedHttpURLLoad("overflow-hidden.html");
  NavigateTo(base_url_ + "overflow-hidden.html");
  ForceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the cc::Layer.
  Element* overflow_element =
      GetFrame()->GetDocument()->getElementById("unscrollable-y");
  DCHECK(overflow_element);

  LayoutObject* layout_object = overflow_element->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  ASSERT_TRUE(layout_object->HasLayer());

  LayoutBox* box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->UsesCompositedScrolling());
  ASSERT_EQ(kPaintsIntoOwnBacking, box->Layer()->GetCompositingState());

  CompositedLayerMapping* composited_layer_mapping =
      box->Layer()->GetCompositedLayerMapping();
  ASSERT_TRUE(composited_layer_mapping->HasScrollingLayer());
  DCHECK(composited_layer_mapping->ScrollingContentsLayer());

  cc::Layer* cc_scroll_layer =
      composited_layer_mapping->ScrollingContentsLayer()->CcLayer();
  ASSERT_TRUE(cc_scroll_layer->scrollable());
  ASSERT_TRUE(cc_scroll_layer->GetUserScrollableHorizontal());
  ASSERT_FALSE(cc_scroll_layer->GetUserScrollableVertical());

  overflow_element =
      GetFrame()->GetDocument()->getElementById("unscrollable-x");
  DCHECK(overflow_element);

  layout_object = overflow_element->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  ASSERT_TRUE(layout_object->HasLayer());

  box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->GetScrollableArea()->UsesCompositedScrolling());
  ASSERT_EQ(kPaintsIntoOwnBacking, box->Layer()->GetCompositingState());

  composited_layer_mapping = box->Layer()->GetCompositedLayerMapping();
  ASSERT_TRUE(composited_layer_mapping->HasScrollingLayer());
  DCHECK(composited_layer_mapping->ScrollingContentsLayer());

  cc_scroll_layer =
      composited_layer_mapping->ScrollingContentsLayer()->CcLayer();
  ASSERT_TRUE(cc_scroll_layer->scrollable());
  ASSERT_FALSE(cc_scroll_layer->GetUserScrollableHorizontal());
  ASSERT_TRUE(cc_scroll_layer->GetUserScrollableVertical());
}

TEST_P(ScrollingCoordinatorTest, iframeScrolling) {
  RegisterMockedHttpURLLoad("iframe-scrolling.html");
  RegisterMockedHttpURLLoad("iframe-scrolling-inner.html");
  NavigateTo(base_url_ + "iframe-scrolling.html");
  ForceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the cc::Layer.
  Element* scrollable_frame =
      GetFrame()->GetDocument()->getElementById("scrollable");
  ASSERT_TRUE(scrollable_frame);

  LayoutObject* layout_object = scrollable_frame->GetLayoutObject();
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

  EXPECT_TRUE(
      inner_frame_view->LayoutViewport()->LayerForHorizontalScrollbar());
  EXPECT_TRUE(inner_frame_view->LayoutViewport()->LayerForVerticalScrollbar());
}

TEST_P(ScrollingCoordinatorTest, rtlIframe) {
  RegisterMockedHttpURLLoad("rtl-iframe.html");
  RegisterMockedHttpURLLoad("rtl-iframe-inner.html");
  NavigateTo(base_url_ + "rtl-iframe.html");
  ForceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the cc::Layer.
  Element* scrollable_frame =
      GetFrame()->GetDocument()->getElementById("scrollable");
  ASSERT_TRUE(scrollable_frame);

  LayoutObject* layout_object = scrollable_frame->GetLayoutObject();
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

  int expected_scroll_position = 958 + (inner_frame_view->LayoutViewport()
                                                ->VerticalScrollbar()
                                                ->IsOverlayScrollbar()
                                            ? 0
                                            : 15);
  ASSERT_EQ(expected_scroll_position,
            cc_scroll_layer->CurrentScrollOffset().x());
}

TEST_P(ScrollingCoordinatorTest, setupScrollbarLayerShouldNotCrash) {
  RegisterMockedHttpURLLoad("setup_scrollbar_layer_crash.html");
  NavigateTo(base_url_ + "setup_scrollbar_layer_crash.html");
  ForceFullCompositingUpdate();
  // This test document setup an iframe with scrollbars, then switch to
  // an empty document by javascript.
}

#if defined(OS_MACOSX) || defined(OS_ANDROID)
TEST_P(ScrollingCoordinatorTest,
       DISABLED_setupScrollbarLayerShouldSetScrollLayerOpaque)
#else
TEST_P(ScrollingCoordinatorTest, setupScrollbarLayerShouldSetScrollLayerOpaque)
#endif
{
  RegisterMockedHttpURLLoad("wide_document.html");
  NavigateTo(base_url_ + "wide_document.html");
  ForceFullCompositingUpdate();

  LocalFrameView* frame_view = GetFrame()->View();
  ASSERT_TRUE(frame_view);

  GraphicsLayer* scrollbar_graphics_layer =
      frame_view->LayoutViewport()->GraphicsLayerForHorizontalScrollbar();
  ASSERT_TRUE(scrollbar_graphics_layer);

  cc::Layer* platform_layer = scrollbar_graphics_layer->CcLayer();
  ASSERT_TRUE(platform_layer);

  cc::Layer* contents_layer = scrollbar_graphics_layer->ContentsLayer();
  EXPECT_EQ(contents_layer,
            frame_view->LayoutViewport()->LayerForHorizontalScrollbar());
  ASSERT_TRUE(contents_layer);

  // After ScrollableAreaScrollbarLayerDidChange(),
  // if the main frame's scrollbar_layer is opaque,
  // contents_layer should be opaque too.
  ASSERT_EQ(platform_layer->contents_opaque(),
            contents_layer->contents_opaque());
}

// LocalFrameView::FrameIsScrollableDidChange is used as a dirty bit and is
// set to clean in ScrollingCoordinator::UpdateAfterPaint. This test ensures
// that the dirty bit is set and unset properly.
TEST_P(ScrollingCoordinatorTest, FrameIsScrollableDidChange) {
  LoadHTML(R"HTML(
    <div id='bg' style='background: red; width: 10px; height: 10px;'></div>
    <div id='forcescroll' style='height: 5000px;'></div>
  )HTML");

  // Initially there is a change but that goes away after a compositing update.
  EXPECT_TRUE(GetFrame()->View()->FrameIsScrollableDidChange());
  ForceFullCompositingUpdate();
  EXPECT_FALSE(GetFrame()->View()->FrameIsScrollableDidChange());

  // A change to background color should not change the frame's scrollability.
  auto* background = GetFrame()->GetDocument()->getElementById("bg");
  background->removeAttribute(html_names::kStyleAttr);
  EXPECT_FALSE(GetFrame()->View()->FrameIsScrollableDidChange());

  ForceFullCompositingUpdate();

  // Making the frame not scroll should change the frame's scrollability.
  auto* forcescroll = GetFrame()->GetDocument()->getElementById("forcescroll");
  forcescroll->removeAttribute(html_names::kStyleAttr);
  GetFrame()->View()->UpdateLifecycleToLayoutClean();
  EXPECT_TRUE(GetFrame()->View()->FrameIsScrollableDidChange());

  ForceFullCompositingUpdate();
  EXPECT_FALSE(GetFrame()->View()->FrameIsScrollableDidChange());
}

TEST_P(ScrollingCoordinatorTest, NestedIFramesMainThreadScrollingRegion) {
  // This page has an absolute IFRAME. It contains a scrollable child DIV
  // that's nested within an intermediate IFRAME.
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            #spacer {
              height: 10000px;
            }
            iframe {
              position: absolute;
              top: 1200px;
              left: 0px;
              width: 200px;
              height: 200px;
              border: 0;
            }

          </style>
          <div id="spacer"></div>
          <iframe srcdoc="
              <!DOCTYPE html>
              <style>
                body { margin: 0; }
                iframe { width: 100px; height: 100px; border: 0; }
              </style>
              <iframe srcdoc='<!DOCTYPE html>
                              <style>
                                body { margin: 0; }
                                div {
                                  width: 65px;
                                  height: 65px;
                                  overflow: auto;
                                }
                                p {
                                  width: 300px;
                                  height: 300px;
                                }
                              </style>
                              <div>
                                <p></p>
                              </div>'>
              </iframe>">
          </iframe>
      )HTML");

  ForceFullCompositingUpdate();

  // Scroll the frame to ensure the rect is in the correct coordinate space.
  GetFrame()->GetDocument()->View()->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 1000), kProgrammaticScroll);

  ForceFullCompositingUpdate();

  auto* layout_viewport = GetFrame()->View()->LayoutViewport();
  auto* mapping = layout_viewport->Layer()->GetCompositedLayerMapping();
  auto* non_fast_layer = mapping->ScrollingContentsLayer()->CcLayer();
  EXPECT_EQ(non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(0, 1200, 65, 65));
}

// Same as above but test that the rect is correctly calculated into the fixed
// region when the containing iframe is position: fixed.
TEST_P(ScrollingCoordinatorTest, NestedFixedIFramesMainThreadScrollingRegion) {
  // This page has a fixed IFRAME. It contains a scrollable child DIV that's
  // nested within an intermediate IFRAME.
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            #spacer {
              height: 10000px;
            }
            #iframe {
              position: fixed;
              top: 20px;
              left: 0px;
              width: 200px;
              height: 200px;
              border: 0;
            }

          </style>
          <div id="spacer"></div>
          <iframe id="iframe" srcdoc="
              <!DOCTYPE html>
              <style>
                body { margin: 0; }
                iframe { width: 100px; height: 100px; border: 0; }
              </style>
              <iframe srcdoc='<!DOCTYPE html>
                              <style>
                                body { margin: 0; }
                                div {
                                  width: 75px;
                                  height: 75px;
                                  overflow: auto;
                                }
                                p {
                                  width: 300px;
                                  height: 300px;
                                }
                              </style>
                              <div>
                                <p></p>
                              </div>'>
              </iframe>">
          </iframe>
      )HTML");

  ForceFullCompositingUpdate();

  // Scroll the frame to ensure the rect is in the correct coordinate space.
  GetFrame()->GetDocument()->View()->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 1000), kProgrammaticScroll);

  ForceFullCompositingUpdate();
  auto* outer_iframe = GetFrame()->GetDocument()->getElementById("iframe");
  auto* outer_iframe_box = ToLayoutBox(outer_iframe->GetLayoutObject());
  auto* mapping = outer_iframe_box->Layer()->GetCompositedLayerMapping();
  auto* non_fast_layer = mapping->MainGraphicsLayer()->CcLayer();
  EXPECT_EQ(non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(0, 0, 75, 75));
}

TEST_P(ScrollingCoordinatorTest, IframeCompositedScrollingHideAndShow) {
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            body {
              margin: 0;
            }
            iframe {
              height: 100px;
              width: 100px;
            }
          </style>
          <iframe id="iframe" srcdoc="
              <!DOCTYPE html>
              <style>
                body {height: 1000px;}
              </style>"></iframe>
      )HTML");

  ForceFullCompositingUpdate();

  cc::Layer* non_fast_layer =
      GetFrame()->View()->LayoutViewport()->LayerForScrolling();

  // Should have a NFSR initially.
  EXPECT_EQ(non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(2, 2, 100, 100));

  // Hiding the iframe should clear the NFSR.
  Element* iframe = GetFrame()->GetDocument()->getElementById("iframe");
  iframe->setAttribute(html_names::kStyleAttr, "display: none");
  ForceFullCompositingUpdate();
  EXPECT_TRUE(non_fast_layer->non_fast_scrollable_region().bounds().IsEmpty());

  // Showing it again should compute the NFSR.
  iframe->setAttribute(html_names::kStyleAttr, "");
  ForceFullCompositingUpdate();
  EXPECT_EQ(non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(2, 2, 100, 100));
}

// Same as above but the main frame is scrollable. This should cause the non
// fast scrollable regions to go on the outer viewport's scroll layer.
TEST_P(ScrollingCoordinatorTest,
       IframeCompositedScrollingHideAndShowScrollable) {
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            body {
              height: 1000px;
              margin: 0;
            }
            iframe {
              height: 100px;
              width: 100px;
            }
          </style>
          <iframe id="iframe" srcdoc="
              <!DOCTYPE html>
              <style>
                body {height: 1000px;}
              </style>"></iframe>
      )HTML");

  ForceFullCompositingUpdate();

  Page* page = GetFrame()->GetPage();
  cc::Layer* inner_viewport_scroll_layer =
      page->GetVisualViewport().LayerForScrolling();
  Element* iframe = GetFrame()->GetDocument()->getElementById("iframe");

  cc::Layer* outer_viewport_scroll_layer =
      GetFrame()->View()->LayoutViewport()->LayerForScrolling();

  // Should have a NFSR initially.
  ForceFullCompositingUpdate();
  EXPECT_FALSE(outer_viewport_scroll_layer->non_fast_scrollable_region()
                   .bounds()
                   .IsEmpty());

  // Ensure the visual viewport's scrolling layer didn't get an NFSR.
  EXPECT_TRUE(inner_viewport_scroll_layer->non_fast_scrollable_region()
                  .bounds()
                  .IsEmpty());

  // Hiding the iframe should clear the NFSR.
  iframe->setAttribute(html_names::kStyleAttr, "display: none");
  ForceFullCompositingUpdate();
  EXPECT_TRUE(outer_viewport_scroll_layer->non_fast_scrollable_region()
                  .bounds()
                  .IsEmpty());

  // Showing it again should compute the NFSR.
  iframe->setAttribute(html_names::kStyleAttr, "");
  ForceFullCompositingUpdate();
  EXPECT_FALSE(outer_viewport_scroll_layer->non_fast_scrollable_region()
                   .bounds()
                   .IsEmpty());
}

TEST_P(ScrollingCoordinatorTest, ScrollOffsetClobberedBeforeCompositingUpdate) {
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            #container {
              width: 300px;
              height: 300px;
              overflow: auto;
              will-change: transform;
            }
            #spacer {
              height: 1000px;
            }
          </style>
          <div id="container">
            <div id="spacer"></div>
          </div>
      )HTML");
  ForceFullCompositingUpdate();

  Element* container = GetFrame()->GetDocument()->getElementById("container");
  ScrollableArea* scroller =
      ToLayoutBox(container->GetLayoutObject())->GetScrollableArea();
  cc::Layer* cc_layer = scroller->LayerForScrolling();

  ASSERT_EQ(0, scroller->GetScrollOffset().Height());

  // Simulate 100px of scroll coming from the compositor thread during a commit.
  gfx::ScrollOffset compositor_delta(0, 100.f);
  cc::ScrollAndScaleSet scroll_and_scale_set;
  scroll_and_scale_set.scrolls.push_back(
      {scroller->GetCompositorElementId(), compositor_delta});
  cc_layer->layer_tree_host()->ApplyScrollAndScale(&scroll_and_scale_set);
  EXPECT_EQ(compositor_delta.y(), scroller->GetScrollOffset().Height());
  EXPECT_EQ(compositor_delta, cc_layer->CurrentScrollOffset());

  // Before updating compositing or the lifecycle, set the scroll offset back
  // to what it was before the commit from the main thread.
  scroller->SetScrollOffset(ScrollOffset(0, 0), kProgrammaticScroll);

  // Ensure the offset is up-to-date on the cc::Layer even though, as far as
  // the main thread is concerned, it was unchanged since the last time we
  // pushed the scroll offset.
  ForceFullCompositingUpdate();
  EXPECT_EQ(gfx::ScrollOffset(), cc_layer->CurrentScrollOffset());
}

TEST_P(ScrollingCoordinatorTest, UpdateVisualViewportScrollLayer) {
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            #box {
              width: 300px;
              height: 1000px;
              background-color: red;
            }
          </style>
          <div id="box">
          </div>
      )HTML");
  ForceFullCompositingUpdate();

  Page* page = GetFrame()->GetPage();
  cc::Layer* inner_viewport_scroll_layer =
      page->GetVisualViewport().LayerForScrolling();

  page->GetVisualViewport().SetScale(2);

  EXPECT_EQ(gfx::ScrollOffset(0, 0),
            inner_viewport_scroll_layer->CurrentScrollOffset());

  page->GetVisualViewport().SetLocation(FloatPoint(10, 20));

  EXPECT_EQ(gfx::ScrollOffset(10, 20),
            inner_viewport_scroll_layer->CurrentScrollOffset());
}

TEST_P(ScrollingCoordinatorTest, UpdateUMAMetricUpdated) {
  HistogramTester histogram_tester;
  LoadHTML(R"HTML(
    <div id='bg' style='background: blue;'></div>
    <div id='scroller' style='overflow: scroll; width: 10px; height: 10px;'>
      <div id='forcescroll' style='height: 1000px;'></div>
    </div>
  )HTML");

  // The initial counts should be zero.
  histogram_tester.ExpectTotalCount("Blink.ScrollingCoordinator.UpdateTime", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PreFCP", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PostFCP", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.AggregatedPreFCP", 0);

  // After an initial compositing update, we should have one scrolling update
  // recorded as PreFCP.
  ForceFullCompositingUpdate();
  histogram_tester.ExpectTotalCount("Blink.ScrollingCoordinator.UpdateTime", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PreFCP", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PostFCP", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.AggregatedPreFCP", 0);

  // An update with no scrolling changes should not cause a scrolling update.
  ForceFullCompositingUpdate();
  histogram_tester.ExpectTotalCount("Blink.ScrollingCoordinator.UpdateTime", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PreFCP", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PostFCP", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.AggregatedPreFCP", 0);

  // A change to background color does not need to cause a scrolling update but,
  // because hit test display items paint, we also cause a scrolling coordinator
  // update when the background paints. Also render some text to get past FCP.
  auto* background = GetFrame()->GetDocument()->getElementById("bg");
  background->removeAttribute(html_names::kStyleAttr);
  background->SetInnerHTMLFromString("Some Text");
  ForceFullCompositingUpdate();
  histogram_tester.ExpectTotalCount("Blink.ScrollingCoordinator.UpdateTime", 2);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PreFCP", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PostFCP", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.AggregatedPreFCP", 1);

  // Removing a scrollable area should cause a scrolling update.
  auto* scroller = GetFrame()->GetDocument()->getElementById("scroller");
  scroller->removeAttribute(html_names::kStyleAttr);
  ForceFullCompositingUpdate();
  histogram_tester.ExpectTotalCount("Blink.ScrollingCoordinator.UpdateTime", 3);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PreFCP", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.PostFCP", 2);
  histogram_tester.ExpectTotalCount(
      "Blink.ScrollingCoordinator.UpdateTime.AggregatedPreFCP", 1);
}

TEST_P(ScrollingCoordinatorTest, NonCompositedNonFastScrollableRegion) {
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            body { margin: 0; }
            #composited_container {
              width: 220px;
              height: 220px;
              will-change: transform;
            }
            #scroller {
              height: 200px;
              width: 200px;
              overflow-y: scroll;
            }
          </style>
          <div id="composited_container">
            <div id="scroller">
              <div id="forcescroll" style="height: 1000px;"></div>
            </div>
          </div>
      )HTML");
  ForceFullCompositingUpdate();

  auto* container =
      GetFrame()->GetDocument()->getElementById("composited_container");
  auto* layer = ToLayoutBox(container->GetLayoutObject())->Layer();
  auto* mapping = layer->GetCompositedLayerMapping();
  // The non-scrolling graphics layer should have a non-scrolling region for the
  // non-composited scroller.
  cc::Layer* cc_layer = mapping->MainGraphicsLayer()->CcLayer();
  auto region = cc_layer->non_fast_scrollable_region();
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 200, 200));
}

TEST_P(ScrollingCoordinatorTest, NonCompositedResizerNonFastScrollableRegion) {
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
    <style>
      #container { will-change: transform; }
      #scroller {
        width: 80px;
        height: 80px;
        resize: both;
        overflow-y: scroll;
      }
    </style>
    <div id="container">
      <div id="offset" style="height: 35px;"></div>
      <div id="scroller"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* container_element =
      GetFrame()->GetDocument()->getElementById("container");
  auto* container = ToLayoutBox(container_element->GetLayoutObject());
  auto* container_graphics_layer =
      container->EnclosingLayer()->GraphicsLayerBacking(container);
  // The non-fast scrollable region should be on the container's graphics layer
  // and not one of the viewport scroll layers because the region should move
  // when the container moves and not when the viewport scrolls.
  auto region =
      container_graphics_layer->CcLayer()->non_fast_scrollable_region();
  EXPECT_EQ(region.bounds(), gfx::Rect(66, 101, 14, 14));
}

TEST_P(ScrollingCoordinatorTest, CompositedResizerNonFastScrollableRegion) {
  LoadHTML(R"HTML(
    <style>
      #container { will-change: transform; }
      #scroller {
        will-change: transform;
        width: 80px;
        height: 80px;
        resize: both;
        overflow-y: scroll;
      }
    </style>
    <div id="container">
      <div id="offset" style="height: 35px;"></div>
      <div id="scroller"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* scroller_element =
      GetFrame()->GetDocument()->getElementById("scroller");
  auto* scroller = ToLayoutBox(scroller_element->GetLayoutObject());
  auto* scroll_corner_graphics_layer =
      scroller->GetScrollableArea()->LayerForScrollCorner();
  auto region = scroll_corner_graphics_layer->non_fast_scrollable_region();
  EXPECT_EQ(region.bounds(), gfx::Rect(-7, -7, 14, 14));
}

TEST_P(ScrollingCoordinatorTest, TouchActionUpdatesOutsideInterestRect) {
  LoadHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #scroller {
        will-change: transform;
        width: 200px;
        height: 200px;
        overflow-y: scroll;
      }
      .spacer {
        height: 1000px;
      }
      #touchaction {
        height: 100px;
        background: yellow;
      }
    </style>
    <div id="scroller">
      <div class="spacer"></div>
      <div class="spacer"></div>
      <div class="spacer"></div>
      <div class="spacer"></div>
      <div class="spacer"></div>
      <div id="touchaction">This should not scroll via touch.</div>
    </div>
  )HTML");

  ForceFullCompositingUpdate();

  auto* touch_action = GetFrame()->GetDocument()->getElementById("touchaction");
  touch_action->setAttribute(html_names::kStyleAttr, "touch-action: none;");

  ForceFullCompositingUpdate();

  auto* scroller = GetFrame()->GetDocument()->getElementById("scroller");
  scroller->GetScrollableArea()->SetScrollOffset(ScrollOffset(0, 5100),
                                                 kProgrammaticScroll);

  ForceFullCompositingUpdate();

  auto* scroller_box = ToLayoutBox(scroller->GetLayoutObject());
  auto* mapping = scroller_box->Layer()->GetCompositedLayerMapping();
  auto* cc_layer = mapping->ScrollingContentsLayer()->CcLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 5000, 200, 100));
}

class ScrollingCoordinatorTestWithAcceleratedContext
    : public ScrollingCoordinatorTest {
 public:
  ScrollingCoordinatorTestWithAcceleratedContext()
      : ScrollingCoordinatorTest() {}

 protected:
  void SetUp() override {
    auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      gl->SetIsContextLost(false);
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
    ScrollingCoordinatorTest::SetUp();
  }

  void TearDown() override {
    SharedGpuContext::ResetForTesting();
    ScrollingCoordinatorTest::TearDown();
  }

 private:
  FakeGLES2Interface gl_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ScrollingCoordinatorTestWithAcceleratedContext,
                         testing::Bool());

TEST_P(ScrollingCoordinatorTestWithAcceleratedContext, CanvasTouchActionRects) {
  LoadHTML(R"HTML(
    <canvas id="canvas" style="touch-action: none; will-change: transform;">
    <script>
      var canvas = document.getElementById("canvas");
      var ctx = canvas.getContext("2d");
      canvas.width = 400;
      canvas.height = 400;
      ctx.fillStyle = 'lightgrey';
      ctx.fillRect(0, 0, 400, 400);
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  Element* canvas = GetFrame()->GetDocument()->getElementById("canvas");
  auto* canvas_box = ToLayoutBox(canvas->GetLayoutObject());
  auto* mapping = canvas_box->Layer()->GetCompositedLayerMapping();
  cc::Layer* cc_layer = mapping->MainGraphicsLayer()->CcLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kTouchActionNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 400, 400));
}

}  // namespace blink
