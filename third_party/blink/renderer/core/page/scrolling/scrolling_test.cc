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

#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/base/features.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/sticky_position_constraint.h"
#include "content/test/test_blink_web_unit_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

namespace {

constexpr char kHttpBaseUrl[] = "http://www.test.com/";
constexpr char kHttpsBaseUrl[] = "https://www.test.com/";

cc::Region RegionFromRects(std::initializer_list<gfx::Rect> rects) {
  cc::Region region;
  for (const auto& rect : rects) {
    region.Union(rect);
  }
  return region;
}

}  // namespace

class ScrollingTest : public testing::Test, public PaintTestConfigurations {
 public:
  ScrollingTest() {
    helper_.Initialize();
    SetPreferCompositingToLCDText(true);
    GetWebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));
    GetWebView()->MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  ~ScrollingTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void NavigateToHttp(const std::string& url_fragment) {
    frame_test_helpers::LoadFrame(GetWebView()->MainFrameImpl(),
                                  kHttpBaseUrl + url_fragment);
  }

  void NavigateToHttps(const std::string& url_fragment) {
    frame_test_helpers::LoadFrame(GetWebView()->MainFrameImpl(),
                                  kHttpsBaseUrl + url_fragment);
  }

  void LoadHTML(const std::string& html) {
    frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(), html,
                                       url_test_helpers::ToKURL("about:blank"));
  }

  void ForceFullCompositingUpdate() {
    GetWebView()->MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(kHttpBaseUrl), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  void RegisterMockedHttpsURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(kHttpsBaseUrl), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  void SetupHttpTestURL(const std::string& url_fragment) {
    RegisterMockedHttpURLLoad(url_fragment);
    NavigateToHttp(url_fragment);
    ForceFullCompositingUpdate();
  }

  void SetupHttpsTestURL(const std::string& url_fragment) {
    RegisterMockedHttpsURLLoad(url_fragment);
    NavigateToHttps(url_fragment);
    ForceFullCompositingUpdate();
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }
  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }

  frame_test_helpers::TestWebFrameWidget* GetMainFrameWidget() const {
    return helper_.GetMainFrameWidget();
  }

  PaintLayerScrollableArea* ScrollableAreaByDOMElementId(
      const char* id_value) const {
    return GetFrame()
        ->GetDocument()
        ->getElementById(AtomicString(id_value))
        ->GetLayoutBoxForScrolling()
        ->GetScrollableArea();
  }

  void LoadAhem() { helper_.LoadAhem(); }

  cc::ScrollNode* ScrollNodeForScrollableArea(
      const ScrollableArea* scrollable_area) {
    if (!scrollable_area)
      return nullptr;
    auto* property_trees = RootCcLayer()->layer_tree_host()->property_trees();
    return property_trees->scroll_tree_mutable().FindNodeFromElementId(
        scrollable_area->GetScrollElementId());
  }

  cc::ScrollNode* ScrollNodeByDOMElementId(const char* dom_id) {
    return ScrollNodeForScrollableArea(ScrollableAreaByDOMElementId(dom_id));
  }

  gfx::PointF CurrentScrollOffset(cc::ElementId element_id) const {
    return RootCcLayer()
        ->layer_tree_host()
        ->property_trees()
        ->scroll_tree()
        .current_scroll_offset(element_id);
  }

  gfx::PointF CurrentScrollOffset(const cc::ScrollNode* scroll_node) const {
    return CurrentScrollOffset(scroll_node->element_id);
  }

  cc::ScrollbarLayerBase* ScrollbarLayerForScrollNode(
      cc::ScrollNode* scroll_node,
      cc::ScrollbarOrientation orientation) {
    return blink::ScrollbarLayerForScrollNode(RootCcLayer(), scroll_node,
                                              orientation);
  }

  cc::Layer* RootCcLayer() { return GetFrame()->View()->RootCcLayer(); }

  const cc::Layer* RootCcLayer() const {
    return GetFrame()->View()->RootCcLayer();
  }

  cc::LayerTreeHost* LayerTreeHost() { return helper_.GetLayerTreeHost(); }

  const cc::Layer* FrameScrollingContentsLayer(const LocalFrame& frame) const {
    return ScrollingContentsCcLayerByScrollElementId(
        RootCcLayer(), frame.View()->LayoutViewport()->GetScrollElementId());
  }

  const cc::Layer* MainFrameScrollingContentsLayer() const {
    return FrameScrollingContentsLayer(*GetFrame());
  }

  const cc::Layer* LayerByDOMElementId(const char* dom_id) const {
    return CcLayersByDOMElementId(RootCcLayer(), dom_id)[0];
  }

  const cc::Layer* ScrollingContentsLayerByDOMElementId(
      const char* element_id) const {
    const auto* scrollable_area = ScrollableAreaByDOMElementId(element_id);
    return ScrollingContentsCcLayerByScrollElementId(
        RootCcLayer(), scrollable_area->GetScrollElementId());
  }

  void SetPreferCompositingToLCDText(bool enabled) {
    GetFrame()->GetSettings()->SetPreferCompositingToLCDTextForTesting(enabled);
  }

 private:
  void NavigateTo(const std::string& url) {
    frame_test_helpers::LoadFrame(GetWebView()->MainFrameImpl(), url);
  }

  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper helper_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollingTest);

#define ASSERT_COMPOSITED(scroll_node)                            \
  do {                                                            \
    ASSERT_TRUE(scroll_node);                                     \
    ASSERT_TRUE(scroll_node->is_composited);                      \
    EXPECT_EQ(cc::MainThreadScrollingReason::kNotScrollingOnMain, \
              scroll_node->main_thread_scrolling_reasons);        \
  } while (false)

#define ASSERT_NOT_COMPOSITED(scroll_node,                            \
                              expected_main_thread_scrolling_reasons) \
  do {                                                                \
    ASSERT_TRUE(scroll_node);                                         \
    ASSERT_FALSE(scroll_node->is_composited);                         \
    EXPECT_EQ(expected_main_thread_scrolling_reasons,                 \
              scroll_node->main_thread_scrolling_reasons);            \
  } while (false)

TEST_P(ScrollingTest, fastScrollingByDefault) {
  GetWebView()->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  ForceFullCompositingUpdate();

  // Make sure the scrolling coordinator is active.
  LocalFrameView* frame_view = GetFrame()->View();
  Page* page = GetFrame()->GetPage();
  ASSERT_TRUE(page->GetScrollingCoordinator());

  // Fast scrolling should be enabled by default.
  const auto* outer_scroll_node =
      ScrollNodeForScrollableArea(frame_view->LayoutViewport());
  ASSERT_COMPOSITED(outer_scroll_node);

  ASSERT_EQ(cc::EventListenerProperties::kNone,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kTouchStartOrMove));
  ASSERT_EQ(cc::EventListenerProperties::kNone,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kMouseWheel));

  const auto* inner_scroll_node =
      ScrollNodeForScrollableArea(&page->GetVisualViewport());
  ASSERT_COMPOSITED(inner_scroll_node);
}

TEST_P(ScrollingTest, fastFractionalScrollingDiv) {
  ScopedFractionalScrollOffsetsForTest fractional_scroll_offsets(true);

  SetupHttpTestURL("fractional-scroll-div.html");

  Document* document = GetFrame()->GetDocument();
  Element* scrollable_element =
      document->getElementById(AtomicString("scroller"));
  DCHECK(scrollable_element);

  scrollable_element->setScrollTop(1.0);
  scrollable_element->setScrollLeft(1.0);
  ForceFullCompositingUpdate();

  // Make sure the fractional scroll offset change 1.0 -> 1.2 gets propagated
  // to compositor.
  scrollable_element->setScrollTop(1.2);
  scrollable_element->setScrollLeft(1.2);
  ForceFullCompositingUpdate();

  const auto* scroll_node = ScrollNodeByDOMElementId("scroller");
  ASSERT_TRUE(scroll_node);
  ASSERT_NEAR(1.2f, CurrentScrollOffset(scroll_node).x(), 0.01f);
  ASSERT_NEAR(1.2f, CurrentScrollOffset(scroll_node).y(), 0.01f);
}

TEST_P(ScrollingTest, fastScrollingForFixedPosition) {
  SetupHttpTestURL("fixed-position.html");

  const auto* scroll_node =
      ScrollNodeForScrollableArea(GetFrame()->View()->LayoutViewport());
  ASSERT_TRUE(scroll_node);
  EXPECT_FALSE(scroll_node->main_thread_scrolling_reasons);
}

// Sticky constraints are stored on transform property tree nodes.
static cc::StickyPositionConstraint GetStickyConstraint(Element* element) {
  const auto* properties =
      element->GetLayoutObject()->FirstFragment().PaintProperties();
  DCHECK(properties);
  return *properties->StickyTranslation()->GetStickyConstraint();
}

TEST_P(ScrollingTest, fastScrollingForStickyPosition) {
  SetupHttpTestURL("sticky-position.html");

  // Sticky position should not fall back to main thread scrolling.
  const auto* scroll_node =
      ScrollNodeForScrollableArea(GetFrame()->View()->LayoutViewport());
  ASSERT_COMPOSITED(scroll_node);

  Document* document = GetFrame()->GetDocument();
  {
    Element* element = document->getElementById(AtomicString("div-tl"));
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right &&
                !constraint.is_anchored_bottom);
    EXPECT_EQ(1.f, constraint.top_offset);
    EXPECT_EQ(1.f, constraint.left_offset);
    EXPECT_EQ(gfx::RectF(100, 100, 10, 10),
              constraint.scroll_container_relative_sticky_box_rect);
    EXPECT_EQ(gfx::RectF(100, 100, 200, 200),
              constraint.scroll_container_relative_containing_block_rect);
  }
  {
    Element* element = document->getElementById(AtomicString("div-tr"));
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && !constraint.is_anchored_left &&
                constraint.is_anchored_right && !constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById(AtomicString("div-bl"));
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(!constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right && constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById(AtomicString("div-br"));
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(!constraint.is_anchored_top && !constraint.is_anchored_left &&
                constraint.is_anchored_right && constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById(AtomicString("span-tl"));
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right &&
                !constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById(AtomicString("span-tlbr"));
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                constraint.is_anchored_right && constraint.is_anchored_bottom);
    EXPECT_EQ(1.f, constraint.top_offset);
    EXPECT_EQ(1.f, constraint.left_offset);
    EXPECT_EQ(1.f, constraint.right_offset);
    EXPECT_EQ(1.f, constraint.bottom_offset);
  }
  {
    Element* element = document->getElementById(AtomicString("composited-top"));
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top);
    EXPECT_EQ(gfx::RectF(100, 110, 10, 10),
              constraint.scroll_container_relative_sticky_box_rect);
    EXPECT_EQ(gfx::RectF(100, 100, 200, 200),
              constraint.scroll_container_relative_containing_block_rect);
  }
}

TEST_P(ScrollingTest, elementPointerEventHandler) {
  LoadHTML(R"HTML(
    <div id="pointer" style="width: 100px; height: 100px;"></div>
    <script>
      pointer.addEventListener('pointerdown', function(event) {
      }, {blocking: false} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  // Pointer event handlers should not generate blocking touch action regions.
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingTest, touchEventHandler) {
  SetupHttpTestURL("touch-event-handler.html");

  ASSERT_EQ(cc::EventListenerProperties::kBlocking,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingTest, elementBlockingTouchEventHandler) {
  LoadHTML(R"HTML(
    <div id="blocking" style="width: 100px; height: 100px;"></div>
    <script>
      blocking.addEventListener('touchstart', function(event) {
      }, {passive: false} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 8, 100, 100)), region);
}

TEST_P(ScrollingTest, touchEventHandlerPassive) {
  SetupHttpTestURL("touch-event-handler-passive.html");

  ASSERT_EQ(cc::EventListenerProperties::kPassive,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingTest, elementTouchEventHandlerPassive) {
  LoadHTML(R"HTML(
    <div id="passive" style="width: 100px; height: 100px;"></div>
    <script>
      passive.addEventListener('touchstart', function(event) {
      }, {passive: true} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  // Passive event handlers should not generate blocking touch action regions.
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingTest, TouchActionRectsOnImage) {
  LoadHTML(R"HTML(
    <img id="image" style="width: 100px; height: 100px; touch-action: none;">
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 8, 100, 100)), region);
}

TEST_P(ScrollingTest, touchEventHandlerBoth) {
  SetupHttpTestURL("touch-event-handler-both.html");

  ASSERT_EQ(cc::EventListenerProperties::kBlockingAndPassive,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingTest, wheelEventHandler) {
  SetupHttpTestURL("wheel-event-handler.html");

  ASSERT_EQ(cc::EventListenerProperties::kBlocking,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kMouseWheel));
}

TEST_P(ScrollingTest, wheelEventHandlerPassive) {
  SetupHttpTestURL("wheel-event-handler-passive.html");

  ASSERT_EQ(cc::EventListenerProperties::kPassive,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kMouseWheel));
}

TEST_P(ScrollingTest, wheelEventHandlerBoth) {
  SetupHttpTestURL("wheel-event-handler-both.html");

  ASSERT_EQ(cc::EventListenerProperties::kBlockingAndPassive,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kMouseWheel));
}

TEST_P(ScrollingTest, scrollEventHandler) {
  SetupHttpTestURL("scroll-event-handler.html");

  ASSERT_TRUE(GetMainFrameWidget()->HaveScrollEventHandlers());
}

TEST_P(ScrollingTest, updateEventHandlersDuringTeardown) {
  SetupHttpTestURL("scroll-event-handler-window.html");

  // Simulate detaching the document from its DOM window. This should not
  // cause a crash when the WebViewImpl is closed by the test runner.
  GetFrame()->GetDocument()->Shutdown();
}

TEST_P(ScrollingTest, clippedBodyTest) {
  SetupHttpTestURL("clipped-body.html");

  const auto* root_scroll_layer = MainFrameScrollingContentsLayer();
  EXPECT_TRUE(
      root_scroll_layer->main_thread_scroll_hit_test_region().IsEmpty());
  EXPECT_FALSE(root_scroll_layer->non_composited_scroll_hit_test_rects());
}

TEST_P(ScrollingTest, touchAction) {
  SetupHttpTestURL("touch-action.html");

  const auto* cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kPanDown |
      TouchAction::kInternalPanXScrolls | TouchAction::kInternalNotWritable);
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 1000, 1000)), region);
}

TEST_P(ScrollingTest, touchActionRegions) {
  SetupHttpTestURL("touch-action-regions.html");

  const auto* cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanDown | TouchAction::kPanX |
      TouchAction::kInternalPanXScrolls | TouchAction::kInternalNotWritable);
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 100, 100)), region);

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanDown | TouchAction::kPanRight |
      TouchAction::kInternalPanXScrolls | TouchAction::kInternalNotWritable);
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 50, 50)), region);

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanDown | TouchAction::kInternalNotWritable);
  EXPECT_EQ(cc::Region(gfx::Rect(0, 100, 100, 100)), region);
}

TEST_P(ScrollingTest, touchActionNesting) {
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        background: blue;
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

  const auto* cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kInternalPanXScrolls |
      TouchAction::kInternalNotWritable);
  EXPECT_EQ(
      RegionFromRects({gfx::Rect(5, 5, 150, 50), gfx::Rect(5, 55, 100, 50)}),
      region);
}

TEST_P(ScrollingTest, nestedTouchActionInvalidation) {
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        background: blue;
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

  const auto* cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kInternalPanXScrolls |
      TouchAction::kInternalNotWritable);
  EXPECT_EQ(
      RegionFromRects({gfx::Rect(5, 5, 150, 50), gfx::Rect(5, 55, 100, 50)}),
      region);

  auto* scrollable =
      GetFrame()->GetDocument()->getElementById(AtomicString("scrollable"));
  scrollable->setAttribute(html_names::kStyleAttr,
                           AtomicString("touch-action: none"));
  ForceFullCompositingUpdate();
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kInternalPanXScrolls |
      TouchAction::kInternalNotWritable);
  EXPECT_TRUE(region.IsEmpty());
}

// Similar to nestedTouchActionInvalidation but tests that an ancestor with
// touch-action: pan-x and a descendant with touch-action: pan-y results in a
// touch-action rect of none for the descendant.
TEST_P(ScrollingTest, nestedTouchActionChangesUnion) {
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

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kInternalPanXScrolls |
      TouchAction::kInternalNotWritable);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 8, 150, 50)), region);
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());

  Element* ancestor =
      GetFrame()->GetDocument()->getElementById(AtomicString("ancestor"));
  ancestor->setAttribute(html_names::kStyleAttr,
                         AtomicString("touch-action: pan-y"));
  ForceFullCompositingUpdate();

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanY | TouchAction::kInternalNotWritable);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 8, 100, 100)), region);
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kInternalPanXScrolls |
      TouchAction::kInternalNotWritable);
  EXPECT_TRUE(region.IsEmpty());
  // kInternalNotWritable is set when any of the pans are allowed.
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone | TouchAction::kInternalNotWritable);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 8, 150, 50)), region);
}

TEST_P(ScrollingTest, touchActionEditableElement) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({::features::kSwipeToMoveCursor}, {});
  if (!::features::IsSwipeToMoveCursorEnabled())
    return;
  // Long text that will overflow in y-direction.
  LoadHTML(R"HTML(
    <style>
      #touchaction {
        touch-action: manipulation;
        width: 100px;
        height: 50px;
        overflow: scroll;
      }
    </style>
    <div id="touchaction" contenteditable>
      <div id="child"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();
  const auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kManipulation | TouchAction::kInternalNotWritable);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 8, 100, 50)), region);
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());

  // Make touchaction scrollable by making child overflow.
  Element* child =
      GetFrame()->GetDocument()->getElementById(AtomicString("child"));
  child->setAttribute(html_names::kStyleAttr,
                      AtomicString("width: 1000px; height: 100px;"));
  ForceFullCompositingUpdate();

  cc_layer = ScrollingContentsLayerByDOMElementId("touchaction");
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kManipulation | TouchAction::kInternalPanXScrolls |
      TouchAction::kInternalNotWritable);
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 1000, 100)), region);
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());
}

// Box shadow is not hit testable and should not be included in touch action.
TEST_P(ScrollingTest, touchActionExcludesBoxShadow) {
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

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 8, 100, 100)), region);
}

TEST_P(ScrollingTest, touchActionOnInline) {
  RegisterMockedHttpURLLoad("touch-action-on-inline.html");
  NavigateToHttp("touch-action-on-inline.html");
  LoadAhem();
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(
      RegionFromRects({gfx::Rect(8, 8, 120, 10), gfx::Rect(8, 18, 10, 40)}),
      region);
}

TEST_P(ScrollingTest, touchActionOnText) {
  RegisterMockedHttpURLLoad("touch-action-on-text.html");
  NavigateToHttp("touch-action-on-text.html");
  LoadAhem();
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(RegionFromRects({gfx::Rect(8, 8, 80, 10), gfx::Rect(8, 18, 40, 10),
                             gfx::Rect(8, 28, 160, 10)}),
            region);
}

TEST_P(ScrollingTest, touchActionWithVerticalRLWritingMode) {
  RegisterMockedHttpURLLoad("touch-action-with-vertical-rl-writing-mode.html");
  NavigateToHttp("touch-action-with-vertical-rl-writing-mode.html");
  LoadAhem();
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(
      RegionFromRects({gfx::Rect(292, 8, 20, 20), gfx::Rect(302, 28, 10, 60)}),
      region);
}

TEST_P(ScrollingTest, touchActionBlockingHandler) {
  SetupHttpTestURL("touch-action-blocking-handler.html");

  const auto* cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 100, 100)), region);

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanY | TouchAction::kInternalNotWritable);
  EXPECT_EQ(RegionFromRects(
                {gfx::Rect(0, 0, 200, 100), gfx::Rect(0, 100, 1000, 900)}),
            region);
}

TEST_P(ScrollingTest, touchActionOnScrollingElement) {
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

  // The scrolling contents layer is fully marked as pan-y.
  const auto* scrolling_contents_layer =
      ScrollingContentsLayerByDOMElementId("scrollable");
  cc::Region region =
      scrolling_contents_layer->touch_action_region().GetRegionForTouchAction(
          TouchAction::kPanY | TouchAction::kInternalNotWritable);
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_EQ(scrolling_contents_layer->bounds(), gfx::Size(100, 150));
    EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 100, 150)), region);
  } else {
    // When HitTestOpaqueness is not enabled, the scrolling contents layer
    // contains only the drawable contents due to the lack of the hit test
    // data for the whole scrolling contents.
    EXPECT_EQ(scrolling_contents_layer->bounds(), gfx::Size(50, 150));
    EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 50, 150)), region);
  }

  const auto* container_layer = LayerByDOMElementId("scrollable");
  region = container_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanY | TouchAction::kInternalNotWritable);
  EXPECT_TRUE(region.IsEmpty());
  // TODO(crbug.com/324285520): Do we need touch action data in a ScrollHitTest
  // layer?
  EXPECT_EQ(container_layer->bounds(), gfx::Size(100, 100));

  // The area of the scroller (8,8 100x100) in the main frame scrolling
  // contents layer is also marked as pan-y.
  const auto* main_frame_scrolling_layer = MainFrameScrollingContentsLayer();
  region =
      main_frame_scrolling_layer->touch_action_region().GetRegionForTouchAction(
          TouchAction::kPanY | TouchAction::kInternalNotWritable);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 8, 100, 100)), region);
}

TEST_P(ScrollingTest, IframeWindowTouchHandler) {
  LoadHTML(R"HTML(
    <iframe style="width: 275px; height: 250px; will-change: transform">
    </iframe>
  )HTML");
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

  const auto* child_cc_layer =
      FrameScrollingContentsLayer(*child_frame->GetFrame());
  cc::Region region_child_frame =
      child_cc_layer->touch_action_region().GetRegionForTouchAction(
          TouchAction::kNone);
  cc::Region region_main_frame =
      MainFrameScrollingContentsLayer()
          ->touch_action_region()
          .GetRegionForTouchAction(TouchAction::kNone);
  EXPECT_TRUE(region_main_frame.bounds().IsEmpty());
  EXPECT_FALSE(region_child_frame.bounds().IsEmpty());
  // We only check for the content size for verification as the offset is 0x0
  // due to child frame having its own composited layer.

  // Because touch action rects are painted on the scrolling contents layer,
  // the size of the rect should be equal to the entire scrolling contents area.
  EXPECT_EQ(gfx::Rect(child_cc_layer->bounds()), region_child_frame.bounds());
}

TEST_P(ScrollingTest, WindowTouchEventHandler) {
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

  auto* cc_layer = MainFrameScrollingContentsLayer();

  // The touch action region should include the entire frame, even though the
  // document is smaller than the frame.
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 320, 240)), region);
}

namespace {
class ScrollingTestMockEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override {}
};
}  // namespace

TEST_P(ScrollingTest, WindowTouchEventHandlerInvalidation) {
  LoadHTML("");
  ForceFullCompositingUpdate();

  auto* cc_layer = MainFrameScrollingContentsLayer();

  // Initially there are no touch action regions.
  auto region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());

  // Adding a blocking window event handler should create a touch action region.
  auto* listener = MakeGarbageCollected<ScrollingTestMockEventListener>();
  auto* resolved_options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  resolved_options->setPassive(false);
  GetFrame()->DomWindow()->addEventListener(event_type_names::kTouchstart,
                                            listener, resolved_options);
  ForceFullCompositingUpdate();
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_FALSE(region.IsEmpty());

  // Removing the window event handler also removes the blocking touch action
  // region.
  GetFrame()->DomWindow()->RemoveAllEventListeners();
  ForceFullCompositingUpdate();
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingTest, TouchActionChangeWithoutContent) {
  LoadHTML(R"HTML(
    <div id="blocking"
        style="will-change: transform; width: 100px; height: 100px;"></div>
  )HTML");
  ForceFullCompositingUpdate();

  // Adding a blocking window event handler should create a touch action region.
  auto* listener = MakeGarbageCollected<ScrollingTestMockEventListener>();
  auto* resolved_options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  resolved_options->setPassive(false);
  auto* target_element =
      GetFrame()->GetDocument()->getElementById(AtomicString("blocking"));
  target_element->addEventListener(event_type_names::kTouchstart, listener,
                                   resolved_options);
  ForceFullCompositingUpdate();

  const auto* cc_layer = LayerByDOMElementId("blocking");
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 100, 100)), region);
}

TEST_P(ScrollingTest, WheelEventRegion) {
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        will-change: transform;
        overflow: scroll;
      }
      #content {
        width: 1000px;
        height: 1000px;
      }
    </style>
    <div id="scrollable">
      <div id="content"></div>
    </div>
    <script>
      document.getElementById("scrollable").addEventListener('wheel', (e) => {
        e.preventDefault();
      });
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());

  cc_layer = LayerByDOMElementId("scrollable");
  region = cc_layer->wheel_event_region();
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 200, 200)), region);

  cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");
  region = cc_layer->wheel_event_region();
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 1000, 1000)), region);
}

TEST_P(ScrollingTest, WheelEventHandlerInvalidation) {
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        will-change: transform;
        overflow: scroll;
      }
      #content {
        width: 1000px;
        height: 1000px;
      }
    </style>
    <div id="scrollable">
      <div id="content"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  // Initially there are no wheel event regions.
  const auto* cc_layer = LayerByDOMElementId("scrollable");
  auto region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());

  const auto* cc_layer_content =
      ScrollingContentsLayerByDOMElementId("scrollable");
  region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());

  // Adding a blocking window event handler should create a wheel event region.
  auto* listener = MakeGarbageCollected<ScrollingTestMockEventListener>();
  auto* resolved_options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  resolved_options->setPassive(false);
  GetFrame()
      ->GetDocument()
      ->getElementById(AtomicString("scrollable"))
      ->addEventListener(event_type_names::kWheel, listener, resolved_options);
  ForceFullCompositingUpdate();
  region = cc_layer->wheel_event_region();
  EXPECT_FALSE(region.IsEmpty());
  region = cc_layer_content->wheel_event_region();
  EXPECT_FALSE(region.IsEmpty());

  // Removing the window event handler also removes the wheel event region.
  GetFrame()
      ->GetDocument()
      ->getElementById(AtomicString("scrollable"))
      ->RemoveAllEventListeners();
  ForceFullCompositingUpdate();
  region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());
  region = cc_layer_content->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingTest, WheelEventRegions) {
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        will-change: transform;
        overflow: scroll;
      }
      #content {
        width: 1000px;
        height: 1000px;
      }
      .region {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id="scrollable">
      <div id="region1" class="region"></div>
      <div id="content"></div>
      <div id="region2" class="region"></div>
    </div>
    <script>
      document.getElementById("region1").addEventListener('wheel', (e) => {
        e.preventDefault();
      });
      document.getElementById("region2").addEventListener('wheel', (e) => {
        e.preventDefault();
      });
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  auto* cc_layer = LayerByDOMElementId("scrollable");
  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());

  cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");
  region = cc_layer->wheel_event_region();

  EXPECT_EQ(RegionFromRects(
                {gfx::Rect(0, 0, 100, 100), gfx::Rect(0, 1100, 100, 100)}),
            region);
}

TEST_P(ScrollingTest, WheelEventRegionOnScrollWithoutDrawableContents) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <style>
      #noncomposited {
        width: 200px;
        height: 200px;
        overflow: auto;
        position: absolute;
        top: 50px;
      }
      #content {
        width: 100%;
        height: 1000px;
      }
      .region {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id="noncomposited">
      <div id="region" class="region"></div>
      <div id="content"></div>
    </div>
    <script>
      document.getElementById("region").addEventListener('wheel', (e) => {
        e.preventDefault();
      }, {passive: false});
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_EQ(cc::Region(gfx::Rect(8, 50, 100, 100)), region);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("noncomposited"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);

  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById(AtomicString("noncomposited"));
  DCHECK(scrollable_element);

  // Change scroll position and verify that blocking wheel handler region is
  // updated accordingly.
  scrollable_element->setScrollTop(10.0);
  ForceFullCompositingUpdate();
  region = cc_layer->wheel_event_region();
  EXPECT_EQ(cc::Region(gfx::Rect(8, 50, 100, 90)), region);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("noncomposited"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
}

TEST_P(ScrollingTest, WheelEventRegionOnScrollWithDrawableContents) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <style>
      #noncomposited {
        width: 200px;
        height: 200px;
        overflow: auto;
        position: absolute;
        top: 50px;
      }
      #content {
        width: 100%;
        height: 1000px;
        background: yellow;
      }
      .region {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id="noncomposited">
      <div id="region" class="region"></div>
      <div id="content"></div>
    </div>
    <script>
      document.getElementById("region").addEventListener('wheel', (e) => {
        e.preventDefault();
      }, {passive: false});
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_EQ(cc::Region(gfx::Rect(8, 50, 100, 100)), region);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("noncomposited"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);

  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById(AtomicString("noncomposited"));
  ASSERT_TRUE(scrollable_element);

  scrollable_element->setScrollTop(10.0);
  ForceFullCompositingUpdate();
  region = cc_layer->wheel_event_region();
  EXPECT_EQ(cc::Region(gfx::Rect(8, 50, 100, 90)), region);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("noncomposited"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
}

TEST_P(ScrollingTest, TouchActionRegionOnScrollWithoutDrawableContents) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <style>
      #noncomposited {
        width: 200px;
        height: 200px;
        overflow: auto;
        position: absolute;
        top: 50px;
      }
      #content {
        width: 100%;
        height: 1000px;
      }
      .region {
        width: 100px;
        height: 100px;
        touch-action: none;
      }
    </style>
    <div id="noncomposited">
      <div id="region" class="region"></div>
      <div id="content"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 50, 100, 100)), region);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("noncomposited"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);

  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById(AtomicString("noncomposited"));
  DCHECK(scrollable_element);

  // Change scroll position and verify that blocking wheel handler region is
  // updated accordingly.
  scrollable_element->setScrollTop(10.0);
  ForceFullCompositingUpdate();
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 50, 100, 90)), region);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("noncomposited"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
}

TEST_P(ScrollingTest, TouchActionRegionOnScrollWithDrawableContents) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <style>
      #noncomposited {
        width: 200px;
        height: 200px;
        overflow: auto;
        position: absolute;
        top: 50px;
      }
      #content {
        width: 100%;
        height: 1000px;
        background: yellow;
      }
      .region {
        width: 100px;
        height: 100px;
        touch-action: none;
      }
    </style>
    <div id="noncomposited">
      <div id="region" class="region"></div>
      <div id="content"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 50, 100, 100)), region);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("noncomposited"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);

  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById(AtomicString("noncomposited"));
  ASSERT_TRUE(scrollable_element);

  scrollable_element->setScrollTop(10.0);
  ForceFullCompositingUpdate();
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(8, 50, 100, 90)), region);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("noncomposited"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
}

TEST_P(ScrollingTest, NonCompositedMainThreadRepaintWithCaptureRegion) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <!DOCTYPE html>
    <div id="composited" style="width: 200px; height: 200px; overflow: scroll;
                                background: white">
      <div id="middle" style="width: 150px; height: 300px; overflow: scroll">
        <div id="inner" style="width: 100px; height: 400px; overflow: scroll">
          <div id="capture" style="width: 50px; height: 500px"></div>
          <div style="height: 1000px"></div>
        </div>
        <div style="height: 1000px"></div>
      </div>
    </div>
  )HTML");

  auto crop_id = base::Token::CreateRandom();
  Document& document = *GetFrame()->GetDocument();
  document.getElementById(AtomicString("capture"))
      ->SetRegionCaptureCropId(std::make_unique<RegionCaptureCropId>(crop_id));
  ForceFullCompositingUpdate();

  const cc::Layer* cc_layer =
      ScrollingContentsLayerByDOMElementId("composited");
  EXPECT_EQ(gfx::Rect(0, 0, 50, 300),
            cc_layer->capture_bounds().bounds().at(crop_id));
  ASSERT_COMPOSITED(ScrollNodeByDOMElementId("composited"));
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("middle"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("inner"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);

  document.getElementById(AtomicString("middle"))->setScrollTop(200);
  ForceFullCompositingUpdate();
  EXPECT_EQ(gfx::Rect(0, 0, 50, 200),
            cc_layer->capture_bounds().bounds().at(crop_id));
  ASSERT_COMPOSITED(ScrollNodeByDOMElementId("composited"));
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("middle"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("inner"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);

  document.getElementById(AtomicString("inner"))->setScrollTop(200);
  ForceFullCompositingUpdate();
  EXPECT_EQ(gfx::Rect(0, 0, 50, 100),
            cc_layer->capture_bounds().bounds().at(crop_id));
  ASSERT_COMPOSITED(ScrollNodeByDOMElementId("composited"));
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("middle"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("inner"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
}

TEST_P(ScrollingTest, NonCompositedMainThreadRepaintWithLayerSelection) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <!DOCTYPE html>
    <div id="composited" style="width: 200px; height: 200px; overflow: scroll;
                                background: white">
      <div id="middle" style="width: 150px; height: 300px; overflow: scroll">
        <div id="inner" style="width: 100px; height: 400px; overflow: scroll">
          <div style="height: 150px"></div>
          <div id="text">TEXT</div>
          <div style="height: 1000px"></div>
        </div>
        <div style="height: 1000px"></div>
      </div>
    </div>
  )HTML");

  Document& document = *GetFrame()->GetDocument();
  document.GetPage()->GetFocusController().SetActive(true);
  document.GetPage()->GetFocusController().SetFocused(true);
  GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SelectAllChildren(*document.getElementById(AtomicString("text")))
          .Build(),
      SetSelectionOptions());
  GetFrame()->Selection().SetHandleVisibleForTesting();
  ForceFullCompositingUpdate();

  EXPECT_EQ(gfx::Point(0, 150), LayerTreeHost()->selection().start.edge_start);
  ASSERT_COMPOSITED(ScrollNodeByDOMElementId("composited"));
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("middle"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("inner"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);

  document.getElementById(AtomicString("middle"))->setScrollTop(50);
  ForceFullCompositingUpdate();
  EXPECT_EQ(gfx::Point(0, 100), LayerTreeHost()->selection().start.edge_start);
  ASSERT_COMPOSITED(ScrollNodeByDOMElementId("composited"));
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("middle"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("inner"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);

  document.getElementById(AtomicString("inner"))->setScrollTop(50);
  ForceFullCompositingUpdate();
  EXPECT_EQ(gfx::Point(0, 50), LayerTreeHost()->selection().start.edge_start);
  ASSERT_COMPOSITED(ScrollNodeByDOMElementId("composited"));
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("middle"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  ASSERT_NOT_COMPOSITED(
      ScrollNodeByDOMElementId("inner"),
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
}

// Box shadow is not hit testable and should not be included in wheel region.
TEST_P(ScrollingTest, WheelEventRegionExcludesBoxShadow) {
  LoadHTML(R"HTML(
    <style>
      #shadow {
        width: 100px;
        height: 100px;
        box-shadow: 10px 5px 5px red;
      }
    </style>
    <div id="shadow"></div>
    <script>
      document.getElementById("shadow").addEventListener('wheel', (e) => {
        e.preventDefault();
      });
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_EQ(cc::Region(gfx::Rect(8, 8, 100, 100)), region);
}

TEST_P(ScrollingTest, IframeWindowWheelEventHandler) {
  LoadHTML(R"HTML(
    <iframe style="width: 275px; height: 250px; will-change: transform">
    </iframe>
  )HTML");
  auto* child_frame =
      To<WebLocalFrameImpl>(GetWebView()->MainFrameImpl()->FirstChild());
  frame_test_helpers::LoadHTMLString(child_frame, R"HTML(
      <p style="margin: 1000px"> Hello </p>
      <script>
        window.addEventListener('wheel', (e) => {
          e.preventDefault();
        }, {passive: false});
      </script>
    )HTML",
                                     url_test_helpers::ToKURL("about:blank"));
  ForceFullCompositingUpdate();

  const auto* child_cc_layer =
      FrameScrollingContentsLayer(*child_frame->GetFrame());
  cc::Region region_child_frame = child_cc_layer->wheel_event_region();
  cc::Region region_main_frame =
      MainFrameScrollingContentsLayer()->wheel_event_region();
  EXPECT_TRUE(region_main_frame.bounds().IsEmpty());
  EXPECT_FALSE(region_child_frame.bounds().IsEmpty());
  // We only check for the content size for verification as the offset is 0x0
  // due to child frame having its own composited layer.

  // Because blocking wheel rects are painted on the scrolling contents layer,
  // the size of the rect should be equal to the entire scrolling contents area.
  EXPECT_EQ(gfx::Rect(child_cc_layer->bounds()), region_child_frame.bounds());
}

TEST_P(ScrollingTest, WindowWheelEventHandler) {
  LoadHTML(R"HTML(
    <style>
      html { width: 200px; height: 200px; }
      body { width: 100px; height: 100px; }
    </style>
    <script>
      window.addEventListener('wheel', function(event) {
        event.preventDefault();
      }, {passive: false} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  auto* cc_layer = MainFrameScrollingContentsLayer();

  // The wheel region should include the entire frame, even though the
  // document is smaller than the frame.
  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 320, 240)), region);
}

TEST_P(ScrollingTest, WindowWheelEventHandlerInvalidation) {
  LoadHTML("");
  ForceFullCompositingUpdate();

  auto* cc_layer = MainFrameScrollingContentsLayer();

  // Initially there are no wheel event regions.
  auto region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());

  // Adding a blocking window event handler should create a wheel event region.
  auto* listener = MakeGarbageCollected<ScrollingTestMockEventListener>();
  auto* resolved_options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  resolved_options->setPassive(false);
  GetFrame()->DomWindow()->addEventListener(event_type_names::kWheel, listener,
                                            resolved_options);
  ForceFullCompositingUpdate();
  region = cc_layer->wheel_event_region();
  EXPECT_FALSE(region.IsEmpty());

  // Removing the window event handler also removes the wheel event region.
  GetFrame()->DomWindow()->RemoveAllEventListeners();
  ForceFullCompositingUpdate();
  region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingTest, WheelEventHandlerChangeWithoutContent) {
  LoadHTML(R"HTML(
    <div id="blocking"
        style="will-change: transform; width: 100px; height: 100px;"></div>
  )HTML");
  ForceFullCompositingUpdate();

  // Adding a blocking window event handler should create a wheel event region.
  auto* listener = MakeGarbageCollected<ScrollingTestMockEventListener>();
  auto* resolved_options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  resolved_options->setPassive(false);
  auto* target_element =
      GetFrame()->GetDocument()->getElementById(AtomicString("blocking"));
  target_element->addEventListener(event_type_names::kWheel, listener,
                                   resolved_options);
  ForceFullCompositingUpdate();

  const auto* cc_layer = LayerByDOMElementId("blocking");
  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 100, 100)), region);
}

// Ensure we don't crash when a plugin becomes a LayoutInline
TEST_P(ScrollingTest, PluginBecomesLayoutInline) {
  LoadHTML(R"HTML(
    <style>
      body {
        margin: 0;
        height: 3000px;
      }
    </style>
    <object id="plugin" type="application/x-webkit-test-plugin"></object>
    <script>
      document.getElementById("plugin")
              .appendChild(document.createElement("label"))
    </script>
  )HTML");

  // This test passes if it doesn't crash. We're trying to make sure
  // ScrollingCoordinator can deal with LayoutInline plugins when generating
  // MainThreadScrollHitTestRegion.
  auto* plugin = To<HTMLObjectElement>(
      GetFrame()->GetDocument()->getElementById(AtomicString("plugin")));
  ASSERT_TRUE(plugin->GetLayoutObject()->IsLayoutInline());
  ForceFullCompositingUpdate();
}

// Ensure blocking wheel event regions are correctly generated for both fixed
// and in-flow plugins that need them.
TEST_P(ScrollingTest, WheelEventRegionsForPlugins) {
  LoadHTML(R"HTML(
    <style>
      body {
        margin: 0;
        height: 3000px;
        /* Ensures the wheel hit test data doesn't conflict with this. */
        touch-action: none;
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
        left: 300px;
      }
    </style>
    <div id="fixed">
      <object id="pluginfixed" type="application/x-webkit-test-plugin"></object>
    </div>
    <object id="plugin" type="application/x-webkit-test-plugin"></object>
  )HTML");

  auto* plugin = To<HTMLObjectElement>(
      GetFrame()->GetDocument()->getElementById(AtomicString("plugin")));
  auto* plugin_fixed = To<HTMLObjectElement>(
      GetFrame()->GetDocument()->getElementById(AtomicString("pluginfixed")));
  // Wheel event regions are generated for plugins that require wheel
  // events.
  plugin->OwnedPlugin()->SetWantsWheelEvents(true);
  plugin_fixed->OwnedPlugin()->SetWantsWheelEvents(true);

  ForceFullCompositingUpdate();

  // The non-fixed plugin should create a wheel event region in the
  // scrolling contents layer of the LayoutView.
  auto* viewport_non_fast_layer = MainFrameScrollingContentsLayer();
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 300, 300)),
            viewport_non_fast_layer->wheel_event_region());

  // The fixed plugin should create a wheel event region in a fixed
  // cc::Layer.
  auto* fixed_layer = LayerByDOMElementId("fixed");
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 200, 200)),
            fixed_layer->wheel_event_region());
}

TEST_P(ScrollingTest, MainThreadScrollHitTestRegionWithBorder) {
  SetPreferCompositingToLCDText(false);
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

  auto* layer = MainFrameScrollingContentsLayer();
  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    EXPECT_TRUE(layer->main_thread_scroll_hit_test_region().IsEmpty());
    EXPECT_EQ(
        gfx::Rect(0, 0, 120, 120),
        layer->non_composited_scroll_hit_test_rects()->at(0).hit_test_rect);
  } else {
    EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 120, 120)),
              layer->main_thread_scroll_hit_test_region());
    EXPECT_TRUE(layer->non_composited_scroll_hit_test_rects()->empty());
  }
}

TEST_P(ScrollingTest, NonFastScrollableRegionWithBorderAndBorderRadius) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 0; }
      #scroller {
        height: 100px;
        width: 100px;
        overflow-y: scroll;
        border: 10px solid black;
        /* Make the box not eligible for fast scroll hit test. */
        border-radius: 5px;
      }
    </style>
    <div id="scroller">
      <div id="forcescroll" style="height: 1000px;"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* layer = MainFrameScrollingContentsLayer();
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 120, 120)),
            layer->main_thread_scroll_hit_test_region());
  EXPECT_TRUE(layer->non_composited_scroll_hit_test_rects()->empty());
}

TEST_P(ScrollingTest, FastNonCompositedScrollHitTest) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <!doctype html>
    <style>
      body { margin: 50px; }
      .scroller { width: 100px; height: 100px; overflow: scroll; }
      .content { height: 1000px; position: relative; opacity: 0.5; }
    </style>
    <!-- 50,50 100x100 -->
    <div id="standalone" class="scroller">
      <div class="content"></div>
    </div>
    <!-- 50,150 100x100 -->
    <div id="nested-parent" class="scroller">
      <div id="nested-child" class="scroller">
        <div class="content"></div>
      </div>
      <div class="content"></div>
    </div>
    <!-- 50,250 100x100 -->
    <div id="covered1" class="scroller">
      <div class="content"></div>
    </div>
    <!-- This partly covers `covered1` -->
    <div style="position: absolute;
                top: 250px; left: 0; width: 100px; height: 50px">
    </div>
    <!-- 50,350 100x100 -->
    <div id="covered2" class="scroller">
      <div class="content"></div>
    </div>
    <!-- This scroller partly covers `covered2`, opaque to hit test. -->
    <div id="covering2" class="scroller"
         style="position: absolute; top: 350px; left: 0;
                width: 100px; height: 50px">
      <div class="content"></div>
    </div>
    <!-- 50,450 100x100 -->
    <div id="covered3" class="scroller">
      <div class="content"></div>
    </div>
    <!-- This scroller partly covers `covered3`, not opaque to hit test. -->
    <div id="covering3" class="scroller"
         style="position: absolute; top: 450px; left: 0;
                width: 100px; height: 50px; border-radius: 10px">
      <div class="content"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* layer = MainFrameScrollingContentsLayer();
  const cc::Region& non_fast_region =
      layer->main_thread_scroll_hit_test_region();
  const std::vector<cc::ScrollHitTestRect>* scroll_hit_test_rects =
      layer->non_composited_scroll_hit_test_rects();
  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    cc::Region expected_non_fast_region(gfx::Rect(50, 150, 100, 400));
    EXPECT_EQ(RegionFromRects(
                  {// nested-parent, covered1, covered2, covered3.
                   // TODO(crbug.com/357905840): Ideally covered2 should be
                   // fast, but for now
                   // it's marked not fast by the background chunk of covering2.
                   gfx::Rect(50, 150, 100, 400),
                   // covering3.
                   gfx::Rect(0, 450, 100, 50)}),
              non_fast_region);
    EXPECT_EQ(2u, scroll_hit_test_rects->size());
    // standalone.
    EXPECT_EQ(gfx::Rect(50, 50, 100, 100),
              scroll_hit_test_rects->at(0).hit_test_rect);
    // covering2.
    EXPECT_EQ(gfx::Rect(0, 350, 100, 50),
              scroll_hit_test_rects->at(1).hit_test_rect);
  } else {
    EXPECT_EQ(RegionFromRects(
                  {// standalone, nested-parent, covered1, covered2, covered3.
                   gfx::Rect(50, 50, 100, 500),
                   // convering2, coverting3.
                   gfx::Rect(0, 350, 100, 50), gfx::Rect(0, 450, 100, 50)}),
              non_fast_region);
    EXPECT_TRUE(scroll_hit_test_rects->empty());
  }
}

TEST_P(ScrollingTest, ElementRegionCaptureData) {
  LoadHTML(R"HTML(
              <head>
                <style type="text/css">
                  body {
                    height: 2000px;
                  }
                  #scrollable {
                    margin-top: 50px;
                    margin-left: 50px;
                    width: 200px;
                    height: 200px;
                    overflow: scroll;
                  }
                  #content {
                    width: 1000px;
                    height: 1000px;
                  }
                </style>
              </head>

              <body>
                <div id="scrollable">
                  <div id="content"></div>
                </div>
              </body>
            )HTML");

  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById(AtomicString("scrollable"));
  Element* content_element =
      GetFrame()->GetDocument()->getElementById(AtomicString("content"));

  const RegionCaptureCropId scrollable_id(
      GUIDToToken(base::Uuid::GenerateRandomV4()));
  const RegionCaptureCropId content_id(
      GUIDToToken(base::Uuid::GenerateRandomV4()));

  scrollable_element->SetRegionCaptureCropId(
      std::make_unique<RegionCaptureCropId>(scrollable_id));
  content_element->SetRegionCaptureCropId(
      std::make_unique<RegionCaptureCropId>(content_id));
  ForceFullCompositingUpdate();

  const cc::Layer* container_layer = MainFrameScrollingContentsLayer();
  const cc::Layer* contents_layer =
      ScrollingContentsLayerByDOMElementId("scrollable");
  ASSERT_TRUE(container_layer);
  ASSERT_TRUE(contents_layer);

  const base::flat_map<viz::RegionCaptureCropId, gfx::Rect>& container_bounds =
      container_layer->capture_bounds().bounds();
  const base::flat_map<viz::RegionCaptureCropId, gfx::Rect>& contents_bounds =
      contents_layer->capture_bounds().bounds();

  EXPECT_EQ(1u, container_bounds.size());
  EXPECT_FALSE(container_bounds.begin()->first.is_zero());
  EXPECT_EQ(scrollable_id.value(), container_bounds.begin()->first);
  EXPECT_EQ((gfx::Size{200, 200}), container_bounds.begin()->second.size());

  EXPECT_EQ(1u, contents_bounds.size());
  EXPECT_FALSE(contents_bounds.begin()->first.is_zero());
  EXPECT_EQ(content_id.value(), contents_bounds.begin()->first);
  EXPECT_EQ((gfx::Rect{0, 0, 1000, 1000}), contents_bounds.begin()->second);
}

TEST_P(ScrollingTest, overflowScrolling) {
  SetupHttpTestURL("overflow-scrolling.html");

  // Verify the scroll node of the accelerated scrolling element.
  auto* scroll_node = ScrollNodeByDOMElementId("scrollable");
  ASSERT_TRUE(scroll_node);
  EXPECT_TRUE(scroll_node->user_scrollable_horizontal);
  EXPECT_TRUE(scroll_node->user_scrollable_vertical);

  EXPECT_TRUE(ScrollbarLayerForScrollNode(
      scroll_node, cc::ScrollbarOrientation::kHorizontal));
  EXPECT_TRUE(ScrollbarLayerForScrollNode(scroll_node,
                                          cc::ScrollbarOrientation::kVertical));
}

TEST_P(ScrollingTest, overflowHidden) {
  SetupHttpTestURL("overflow-hidden.html");

  // Verify the scroll node of the accelerated scrolling element.
  const auto* scroll_node = ScrollNodeByDOMElementId("unscrollable-y");
  ASSERT_TRUE(scroll_node);
  EXPECT_TRUE(scroll_node->user_scrollable_horizontal);
  EXPECT_FALSE(scroll_node->user_scrollable_vertical);

  scroll_node = ScrollNodeByDOMElementId("unscrollable-x");
  ASSERT_TRUE(scroll_node);
  EXPECT_FALSE(scroll_node->user_scrollable_horizontal);
  EXPECT_TRUE(scroll_node->user_scrollable_vertical);
}

TEST_P(ScrollingTest, iframeScrolling) {
  RegisterMockedHttpURLLoad("iframe-scrolling.html");
  RegisterMockedHttpURLLoad("iframe-scrolling-inner.html");
  NavigateToHttp("iframe-scrolling.html");
  ForceFullCompositingUpdate();

  Element* scrollable_frame =
      GetFrame()->GetDocument()->getElementById(AtomicString("scrollable"));
  ASSERT_TRUE(scrollable_frame);

  LayoutObject* layout_object = scrollable_frame->GetLayoutObject();
  ASSERT_TRUE(layout_object);
  ASSERT_TRUE(layout_object->IsLayoutEmbeddedContent());

  auto* layout_embedded_content = To<LayoutEmbeddedContent>(layout_object);
  ASSERT_TRUE(layout_embedded_content);

  LocalFrameView* inner_frame_view =
      To<LocalFrameView>(layout_embedded_content->ChildFrameView());
  ASSERT_TRUE(inner_frame_view);

  // Verify the scroll node of the accelerated scrolling iframe.
  auto* scroll_node =
      ScrollNodeForScrollableArea(inner_frame_view->LayoutViewport());
  ASSERT_TRUE(scroll_node);
  EXPECT_TRUE(ScrollbarLayerForScrollNode(
      scroll_node, cc::ScrollbarOrientation::kHorizontal));
  EXPECT_TRUE(ScrollbarLayerForScrollNode(scroll_node,
                                          cc::ScrollbarOrientation::kVertical));
}

TEST_P(ScrollingTest, rtlIframe) {
  RegisterMockedHttpURLLoad("rtl-iframe.html");
  RegisterMockedHttpURLLoad("rtl-iframe-inner.html");
  NavigateToHttp("rtl-iframe.html");
  ForceFullCompositingUpdate();

  Element* scrollable_frame =
      GetFrame()->GetDocument()->getElementById(AtomicString("scrollable"));
  ASSERT_TRUE(scrollable_frame);

  LayoutObject* layout_object = scrollable_frame->GetLayoutObject();
  ASSERT_TRUE(layout_object);
  ASSERT_TRUE(layout_object->IsLayoutEmbeddedContent());

  auto* layout_embedded_content = To<LayoutEmbeddedContent>(layout_object);
  ASSERT_TRUE(layout_embedded_content);

  LocalFrameView* inner_frame_view =
      To<LocalFrameView>(layout_embedded_content->ChildFrameView());
  ASSERT_TRUE(inner_frame_view);

  // Verify the scroll node of the accelerated scrolling iframe.
  const auto* scroll_node =
      ScrollNodeForScrollableArea(inner_frame_view->LayoutViewport());
  ASSERT_TRUE(scroll_node);

  int expected_scroll_position = 958 + (inner_frame_view->LayoutViewport()
                                                ->VerticalScrollbar()
                                                ->IsOverlayScrollbar()
                                            ? 0
                                            : 15);
  ASSERT_EQ(expected_scroll_position, CurrentScrollOffset(scroll_node).x());
}

TEST_P(ScrollingTest, setupScrollbarLayerShouldNotCrash) {
  SetupHttpTestURL("setup_scrollbar_layer_crash.html");
  // This test document setup an iframe with scrollbars, then switch to
  // an empty document by javascript.
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
TEST_P(ScrollingTest, DISABLED_setupScrollbarLayerShouldSetScrollLayerOpaque)
#else
TEST_P(ScrollingTest, setupScrollbarLayerShouldSetScrollLayerOpaque)
#endif
{
  ScopedMockOverlayScrollbars mock_overlay_scrollbar(false);

  SetupHttpTestURL("wide_document.html");

  LocalFrameView* frame_view = GetFrame()->View();
  ASSERT_TRUE(frame_view);

  auto* scroll_node = ScrollNodeForScrollableArea(frame_view->LayoutViewport());
  ASSERT_TRUE(scroll_node);

  auto* horizontal_scrollbar_layer = ScrollbarLayerForScrollNode(
      scroll_node, cc::ScrollbarOrientation::kHorizontal);
  ASSERT_TRUE(horizontal_scrollbar_layer);
  EXPECT_EQ(!frame_view->LayoutViewport()
                 ->HorizontalScrollbar()
                 ->IsOverlayScrollbar(),
            horizontal_scrollbar_layer->contents_opaque());

  EXPECT_FALSE(ScrollbarLayerForScrollNode(
      scroll_node, cc::ScrollbarOrientation::kVertical));
}

TEST_P(ScrollingTest, NestedIFramesMainThreadScrollingRegion) {
  // This page has an absolute IFRAME. It contains a scrollable child DIV
  // that's nested within an intermediate IFRAME.
  SetPreferCompositingToLCDText(false);
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
                                  /* Make the div not eligible for fast scroll
                                     hit test. */
                                  border-radius: 5px;
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
      ScrollOffset(0, 1000), mojom::blink::ScrollType::kProgrammatic);

  ForceFullCompositingUpdate();

  auto* non_fast_layer = MainFrameScrollingContentsLayer();
  EXPECT_EQ(cc::Region(gfx::Rect(0, 1200, 65, 65)),
            non_fast_layer->main_thread_scroll_hit_test_region());
  // Nested scroll is not eligible for fast non-composited scroll hit test.
  EXPECT_TRUE(non_fast_layer->non_composited_scroll_hit_test_rects()->empty());
}

// Same as above but test that the rect is correctly calculated into the fixed
// region when the containing iframe is position: fixed.
TEST_P(ScrollingTest, NestedFixedIFramesMainThreadScrollingRegion) {
  // This page has a fixed IFRAME. It contains a scrollable child DIV that's
  // nested within an intermediate IFRAME.
  SetPreferCompositingToLCDText(false);
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
              border: 20px solid blue;
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
                                  /* Make the div not eligible for fast scroll
                                     hit test. */
                                  border-radius: 5px;
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
      ScrollOffset(0, 1000), mojom::blink::ScrollType::kProgrammatic);

  ForceFullCompositingUpdate();
  auto* non_fast_layer = LayerByDOMElementId("iframe");
  EXPECT_EQ(cc::Region(gfx::Rect(20, 20, 75, 75)),
            non_fast_layer->main_thread_scroll_hit_test_region());
  // Nested scroll is not eligible for fast non-composited scroll hit test.
  EXPECT_TRUE(non_fast_layer->non_composited_scroll_hit_test_rects()->empty());
}

TEST_P(ScrollingTest, IframeCompositedScrolling) {
  LoadHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 0; }
      iframe { height: 100px; width: 100px; }
    </style>
    <iframe id="iframe1" srcdoc="<!DOCTYPE html>"></iframe>
    <iframe id="iframe2" srcdoc="
      <!DOCTYPE html>
      <style>body { height: 1000px; }</style>">
    </iframe>
  )HTML");
  ForceFullCompositingUpdate();

  // Should not have main_thread_scroll_hit_test_region or
  // non_composited_scroll_hit_test_rects on any layer.
  for (auto& layer : RootCcLayer()->children()) {
    EXPECT_TRUE(layer->main_thread_scroll_hit_test_region().IsEmpty());
    EXPECT_FALSE(layer->non_composited_scroll_hit_test_rects());
  }
}

TEST_P(ScrollingTest, IframeNonCompositedScrollingHideAndShow) {
  SetPreferCompositingToLCDText(false);
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

  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    // Should have a NonCompositedScrollHitTestRect initially.
    EXPECT_TRUE(MainFrameScrollingContentsLayer()
                    ->main_thread_scroll_hit_test_region()
                    .IsEmpty());
    EXPECT_EQ(gfx::Rect(2, 2, 100, 100),
              MainFrameScrollingContentsLayer()
                  ->non_composited_scroll_hit_test_rects()
                  ->at(0)
                  .hit_test_rect);
  } else {
    // Should have a MainThreadScrollHitTestRegion initially.
    EXPECT_EQ(cc::Region(gfx::Rect(2, 2, 100, 100)),
              MainFrameScrollingContentsLayer()
                  ->main_thread_scroll_hit_test_region());
    EXPECT_TRUE(MainFrameScrollingContentsLayer()
                    ->non_composited_scroll_hit_test_rects()
                    ->empty());
  }

  // Hiding the iframe should clear the MainThreadScrollHitTestRegion and
  // NonCompositedScrollHitTestRect.
  Element* iframe =
      GetFrame()->GetDocument()->getElementById(AtomicString("iframe"));
  iframe->setAttribute(html_names::kStyleAttr, AtomicString("display: none"));
  ForceFullCompositingUpdate();
  EXPECT_TRUE(MainFrameScrollingContentsLayer()
                  ->main_thread_scroll_hit_test_region()
                  .IsEmpty());
  EXPECT_FALSE(MainFrameScrollingContentsLayer()
                   ->non_composited_scroll_hit_test_rects());

  // Showing it again should compute the MainThreadScrollHitTestRegion or
  // NonCompositedScrollHitTestRect.
  iframe->setAttribute(html_names::kStyleAttr, g_empty_atom);
  ForceFullCompositingUpdate();
  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    EXPECT_TRUE(MainFrameScrollingContentsLayer()
                    ->main_thread_scroll_hit_test_region()
                    .IsEmpty());
    EXPECT_EQ(gfx::Rect(2, 2, 100, 100),
              MainFrameScrollingContentsLayer()
                  ->non_composited_scroll_hit_test_rects()
                  ->at(0)
                  .hit_test_rect);
  } else {
    EXPECT_EQ(cc::Region(gfx::Rect(2, 2, 100, 100)),
              MainFrameScrollingContentsLayer()
                  ->main_thread_scroll_hit_test_region());
    EXPECT_TRUE(MainFrameScrollingContentsLayer()
                    ->non_composited_scroll_hit_test_rects()
                    ->empty());
  }
}

// Same as above but use visibility: hidden instead of display: none.
TEST_P(ScrollingTest, IframeNonCompositedScrollingHideAndShowVisibility) {
  SetPreferCompositingToLCDText(false);
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

  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    // Should have a NonCompositedScrollHitTestRect initially.
    EXPECT_TRUE(MainFrameScrollingContentsLayer()
                    ->main_thread_scroll_hit_test_region()
                    .IsEmpty());
    EXPECT_EQ(gfx::Rect(2, 2, 100, 100),
              MainFrameScrollingContentsLayer()
                  ->non_composited_scroll_hit_test_rects()
                  ->at(0)
                  .hit_test_rect);
  } else {
    // Should have a MainThreadScrollHitTestRegion initially.
    EXPECT_EQ(cc::Region(gfx::Rect(2, 2, 100, 100)),
              MainFrameScrollingContentsLayer()
                  ->main_thread_scroll_hit_test_region());
    EXPECT_TRUE(MainFrameScrollingContentsLayer()
                    ->non_composited_scroll_hit_test_rects()
                    ->empty());
  }

  // Hiding the iframe should clear the MainThreadScrollHitTestRegion and
  // NonCompositedScrollHitTestRect.
  Element* iframe =
      GetFrame()->GetDocument()->getElementById(AtomicString("iframe"));
  iframe->setAttribute(html_names::kStyleAttr, AtomicString("display: none"));
  ForceFullCompositingUpdate();
  EXPECT_TRUE(MainFrameScrollingContentsLayer()
                  ->main_thread_scroll_hit_test_region()
                  .IsEmpty());
  EXPECT_FALSE(MainFrameScrollingContentsLayer()
                   ->non_composited_scroll_hit_test_rects());

  // Showing it again should compute the MainThreadScrollHitTestRegion or
  // NonCompositedScrollHitTestRect.
  iframe->setAttribute(html_names::kStyleAttr, g_empty_atom);
  ForceFullCompositingUpdate();
  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    EXPECT_TRUE(MainFrameScrollingContentsLayer()
                    ->main_thread_scroll_hit_test_region()
                    .IsEmpty());
    EXPECT_EQ(gfx::Rect(2, 2, 100, 100),
              MainFrameScrollingContentsLayer()
                  ->non_composited_scroll_hit_test_rects()
                  ->at(0)
                  .hit_test_rect);
  } else {
    EXPECT_EQ(cc::Region(gfx::Rect(2, 2, 100, 100)),
              MainFrameScrollingContentsLayer()
                  ->main_thread_scroll_hit_test_region());
    EXPECT_TRUE(MainFrameScrollingContentsLayer()
                    ->non_composited_scroll_hit_test_rects()
                    ->empty());
  }
}

// Same as above but the main frame is scrollable. This should cause the non
// fast scrollable regions to go on the outer viewport's scroll layer.
TEST_P(ScrollingTest, IframeNonCompositedScrollingHideAndShowScrollable) {
  SetPreferCompositingToLCDText(false);
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
  const auto* inner_viewport_scroll_layer =
      page->GetVisualViewport().LayerForScrolling();
  Element* iframe =
      GetFrame()->GetDocument()->getElementById(AtomicString("iframe"));

  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    // Should have a MainThreadScrollHitTestRegion initially.
    EXPECT_FALSE(MainFrameScrollingContentsLayer()
                     ->non_composited_scroll_hit_test_rects()
                     ->empty());
  } else {
    // Should have a MainThreadScrollHitTestRegion initially.
    EXPECT_FALSE(MainFrameScrollingContentsLayer()
                     ->main_thread_scroll_hit_test_region()
                     .IsEmpty());
  }

  // Ensure the visual viewport's scrolling layer didn't get a
  // MainThreadScrollHitTestRegion or NonCompositedScrollHitTestRect.
  EXPECT_TRUE(inner_viewport_scroll_layer->main_thread_scroll_hit_test_region()
                  .IsEmpty());
  EXPECT_FALSE(
      inner_viewport_scroll_layer->non_composited_scroll_hit_test_rects());

  // Hiding the iframe should clear the MainThreadScrollHitTestRegion and
  // NonCompositedScrollHitTestRect.
  iframe->setAttribute(html_names::kStyleAttr, AtomicString("display: none"));
  ForceFullCompositingUpdate();
  EXPECT_TRUE(MainFrameScrollingContentsLayer()
                  ->main_thread_scroll_hit_test_region()
                  .IsEmpty());
  EXPECT_FALSE(MainFrameScrollingContentsLayer()
                   ->non_composited_scroll_hit_test_rects());

  iframe->setAttribute(html_names::kStyleAttr, g_empty_atom);
  ForceFullCompositingUpdate();
  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    // Showing it again should compute the NonCompositedScrollHitTestRect.
    EXPECT_FALSE(MainFrameScrollingContentsLayer()
                     ->non_composited_scroll_hit_test_rects()
                     ->empty());
  } else {
    // Showing it again should compute the MainThreadScrollHitTestRegion.
    EXPECT_FALSE(MainFrameScrollingContentsLayer()
                     ->main_thread_scroll_hit_test_region()
                     .IsEmpty());
  }
}

TEST_P(ScrollingTest, IframeNonCompositedScrollingNested) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <!DOCTYPE html>
    <style>body { margin: 0; }</style>
    <iframe style="width: 1000px; height: 1000px; border: none;
                   margin-left: 51px; margin-top: 52px"
     srcdoc="
       <!DOCTYPE html>
       <style>body { margin: 50px 0; }</style>
       <div style='width: 100px; height: 100px; overflow: scroll'>
         <div style='height: 1000px'></div>
       </div>
       <iframe style='width: 211px; height: 211px; padding: 10px; border: none'
        srcdoc='
          <!DOCTYPE html>
          <style>body { margin: 0; width: 1000px; height: 1000px; }</style>
       '></iframe>
     "></iframe>
    <div style="height: 2000px"></div>
  )HTML");
  ForceFullCompositingUpdate();

  auto main_thread_region =
      MainFrameScrollingContentsLayer()->main_thread_scroll_hit_test_region();
  auto* hit_test_rects =
      MainFrameScrollingContentsLayer()->non_composited_scroll_hit_test_rects();
  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    EXPECT_TRUE(main_thread_region.IsEmpty());
    EXPECT_EQ(2u, hit_test_rects->size());
    EXPECT_EQ(gfx::Rect(51, 102, 100, 100),
              hit_test_rects->at(0).hit_test_rect);
    EXPECT_EQ(gfx::Rect(61, 212, 211, 211),
              hit_test_rects->at(1).hit_test_rect);
  } else {
    EXPECT_EQ(RegionFromRects(
                  {gfx::Rect(51, 102, 100, 100), gfx::Rect(61, 212, 211, 211)}),
              main_thread_region);
    EXPECT_TRUE(hit_test_rects->empty());
  }
}

TEST_P(ScrollingTest, IframeNonCompositedScrollingTransformed) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <!DOCTYPE html>
    <iframe style="position: absolute; left: 300px; top: 300px;
                   width: 200px; height: 200px; border: none;
                   transform: scale(2)"
     srcdoc="
       <!DOCTYPE html>
       <style>body { margin: 0; }</style>
       <iframe style='width: 120px; height: 120px; padding: 10px; border: none'
        srcdoc='
          <!DOCTYPE html>
          <style>body { margin: 0; width: 1000px; height: 1000px }</style>
        '></iframe>
     "></iframe>
    <div style="height: 2000px"></div>
  )HTML");
  ForceFullCompositingUpdate();

  EXPECT_EQ(
      cc::Region(gfx::Rect(220, 220, 240, 240)),
      MainFrameScrollingContentsLayer()->main_thread_scroll_hit_test_region());
  // The scale makes the scroller not eligible for fast non-composited scroll
  // hit test.
  EXPECT_TRUE(MainFrameScrollingContentsLayer()
                  ->non_composited_scroll_hit_test_rects()
                  ->empty());
}

TEST_P(ScrollingTest, IframeNonCompositedScrollingPageScaled) {
  GetFrame()->GetPage()->SetPageScaleFactor(2.f);
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <!DOCTYPE html>
    <iframe style="position: absolute; left: 300px; top: 300px;
                   width: 200px; height: 200px; border: none"
     srcdoc="
       <!DOCTYPE html>
       <style>body { margin: 0; }</style>
       <iframe style='width: 120px; height: 120px; padding: 10px; border: none'
        srcdoc='
          <!DOCTYPE html>
          <style>body { margin: 0; width: 1000px; height: 1000px }</style>
        '></iframe>
     "></iframe>
    <div style="height: 2000px"></div>
  )HTML");
  ForceFullCompositingUpdate();

  // cc::Layer::main_thread_scroll_hit_test_region and
  // non_composited_scroll_hit_test_rects are in layer space and are not
  // affected by the page scale.
  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    EXPECT_TRUE(MainFrameScrollingContentsLayer()
                    ->main_thread_scroll_hit_test_region()
                    .IsEmpty());
    EXPECT_EQ(gfx::Rect(310, 310, 120, 120),
              MainFrameScrollingContentsLayer()
                  ->non_composited_scroll_hit_test_rects()
                  ->at(0)
                  .hit_test_rect);
  } else {
    EXPECT_EQ(cc::Region(gfx::Rect(310, 310, 120, 120)),
              MainFrameScrollingContentsLayer()
                  ->main_thread_scroll_hit_test_region());
    EXPECT_TRUE(MainFrameScrollingContentsLayer()
                    ->non_composited_scroll_hit_test_rects()
                    ->empty());
  }
}

TEST_P(ScrollingTest, NonCompositedScrollTransformChange) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <!DOCTYPE html>
    <style>body { margin: 0; }</style>
    <div id="scroll" style="width: 222px; height: 222px; overflow: scroll;
                            transform: translateX(0)">
      <div style="height: 1000px"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    EXPECT_EQ(gfx::Rect(0, 0, 222, 222),
              MainFrameScrollingContentsLayer()
                  ->non_composited_scroll_hit_test_rects()
                  ->at(0)
                  .hit_test_rect);
  } else {
    EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 222, 222)),
              MainFrameScrollingContentsLayer()
                  ->main_thread_scroll_hit_test_region());
  }

  GetFrame()->GetDocument()->body()->SetInlineStyleProperty(
      CSSPropertyID::kPadding, "10px");
  ForceFullCompositingUpdate();
  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    EXPECT_EQ(gfx::Rect(10, 10, 222, 222),
              MainFrameScrollingContentsLayer()
                  ->non_composited_scroll_hit_test_rects()
                  ->at(0)
                  .hit_test_rect);
  } else {
    EXPECT_EQ(cc::Region(gfx::Rect(10, 10, 222, 222)),
              MainFrameScrollingContentsLayer()
                  ->main_thread_scroll_hit_test_region());
  }

  GetFrame()
      ->GetDocument()
      ->getElementById(AtomicString("scroll"))
      ->SetInlineStyleProperty(CSSPropertyID::kTransform, "translateX(100px)");
  ForceFullCompositingUpdate();
  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    EXPECT_EQ(gfx::Rect(110, 10, 222, 222),
              MainFrameScrollingContentsLayer()
                  ->non_composited_scroll_hit_test_rects()
                  ->at(0)
                  .hit_test_rect);
  } else {
    EXPECT_EQ(cc::Region(gfx::Rect(110, 10, 222, 222)),
              MainFrameScrollingContentsLayer()
                  ->main_thread_scroll_hit_test_region());
  }
}

TEST_P(ScrollingTest, ScrollOffsetClobberedBeforeCompositingUpdate) {
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

  auto* scrollable_area = ScrollableAreaByDOMElementId("container");
  ASSERT_EQ(0, scrollable_area->GetScrollOffset().y());
  const auto* scroll_node = ScrollNodeForScrollableArea(scrollable_area);

  // Simulate 100px of scroll coming from the compositor thread during a commit.
  gfx::Vector2dF compositor_delta(0, 100.f);
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.push_back(
      {scrollable_area->GetScrollElementId(), compositor_delta, std::nullopt});
  RootCcLayer()->layer_tree_host()->ApplyCompositorChanges(&commit_data);
  // The compositor offset is reflected in blink and cc scroll tree.
  gfx::PointF expected_scroll_position =
      gfx::PointAtOffsetFromOrigin(compositor_delta);
  EXPECT_EQ(expected_scroll_position, scrollable_area->ScrollPosition());
  EXPECT_EQ(expected_scroll_position, CurrentScrollOffset(scroll_node));

  // Before updating the lifecycle, set the scroll offset back to what it was
  // before the commit from the main thread.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 0),
                                   mojom::blink::ScrollType::kProgrammatic);

  // Ensure the offset is up-to-date on the cc::Layer even though, as far as
  // the main thread is concerned, it was unchanged since the last time we
  // pushed the scroll offset.
  ForceFullCompositingUpdate();
  EXPECT_EQ(gfx::PointF(), CurrentScrollOffset(scroll_node));
}

TEST_P(ScrollingTest, UpdateVisualViewportScrollLayer) {
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
  const auto* inner_viewport_scroll_node =
      ScrollNodeForScrollableArea(&page->GetVisualViewport());

  page->GetVisualViewport().SetScale(2);
  ForceFullCompositingUpdate();
  EXPECT_EQ(gfx::PointF(0, 0), CurrentScrollOffset(inner_viewport_scroll_node));

  page->GetVisualViewport().SetLocation(gfx::PointF(10, 20));
  ForceFullCompositingUpdate();
  EXPECT_EQ(gfx::PointF(10, 20),
            CurrentScrollOffset(inner_viewport_scroll_node));
}

TEST_P(ScrollingTest, NonCompositedMainThreadScrollHitTestRegion) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            body { margin: 0; }
            #composited_container {
              will-change: transform;
              border: 20px solid blue;
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

  const auto* cc_layer = LayerByDOMElementId("composited_container");
  if (RuntimeEnabledFeatures::FastNonCompositedScrollHitTestEnabled()) {
    // The non-scrolling layer should have a NonCompositedScrollHitTestRect
    // for the non-composited scroller.
    EXPECT_TRUE(cc_layer->main_thread_scroll_hit_test_region().IsEmpty());
    EXPECT_EQ(
        gfx::Rect(20, 20, 200, 200),
        cc_layer->non_composited_scroll_hit_test_rects()->at(0).hit_test_rect);
  } else {
    // The non-scrolling layer should have a MainThreadScrollHitTestRegion for
    // the non-composited scroller.
    EXPECT_EQ(cc::Region(gfx::Rect(20, 20, 200, 200)),
              cc_layer->main_thread_scroll_hit_test_region());
    EXPECT_TRUE(cc_layer->non_composited_scroll_hit_test_rects()->empty());
  }
}

TEST_P(ScrollingTest, NonCompositedResizerMainThreadScrollHitTestRegion) {
  SetPreferCompositingToLCDText(false);
  LoadHTML(R"HTML(
    <style>
      #container {
        will-change: transform;
        border: 20px solid blue;
      }
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

  auto* container_cc_layer = LayerByDOMElementId("container");
  // The non-fast scrollable region should be on the container's layer and not
  // one of the viewport scroll layers because the region should move when the
  // container moves and not when the viewport scrolls.
  auto region = container_cc_layer->main_thread_scroll_hit_test_region();
  EXPECT_EQ(cc::Region(gfx::Rect(86, 121, 14, 14)), region);
}

TEST_P(ScrollingTest, CompositedResizerMainThreadScrollHitTestRegion) {
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

  auto region =
      LayerByDOMElementId("scroller")->main_thread_scroll_hit_test_region();
  EXPECT_EQ(cc::Region(gfx::Rect(66, 66, 14, 14)), region);
}

TEST_P(ScrollingTest, TouchActionUpdatesOutsideInterestRect) {
  LoadHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #scroller {
        will-change: transform;
        width: 200px;
        height: 200px;
        background: blue;
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

  auto* touch_action =
      GetFrame()->GetDocument()->getElementById(AtomicString("touchaction"));
  touch_action->setAttribute(html_names::kStyleAttr,
                             AtomicString("touch-action: none;"));

  ForceFullCompositingUpdate();

  ScrollableAreaByDOMElementId("scroller")
      ->SetScrollOffset(ScrollOffset(0, 5100),
                        mojom::blink::ScrollType::kProgrammatic);

  ForceFullCompositingUpdate();

  auto* cc_layer = ScrollingContentsLayerByDOMElementId("scroller");
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(0, 5000, 200, 100)), region);
}

TEST_P(ScrollingTest, MainThreadScrollAndDeltaFromImplSide) {
  LoadHTML(R"HTML(
    <div id='scroller' style='overflow: scroll; width: 100px; height: 100px'>
      <div style='height: 1000px'></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* scroller =
      GetFrame()->GetDocument()->getElementById(AtomicString("scroller"));
  auto* scrollable_area = scroller->GetLayoutBox()->GetScrollableArea();
  auto element_id = scrollable_area->GetScrollElementId();

  EXPECT_EQ(gfx::PointF(), CurrentScrollOffset(element_id));

  // Simulate a direct scroll update out of document lifecycle update.
  scroller->scrollTo(0, 200);
  EXPECT_EQ(gfx::PointF(0, 200), scrollable_area->ScrollPosition());
  EXPECT_EQ(gfx::PointF(0, 200), CurrentScrollOffset(element_id));

  // Simulate the scroll update with scroll delta from impl-side at the
  // beginning of BeginMainFrame.
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.push_back(cc::CompositorCommitData::ScrollUpdateInfo(
      element_id, gfx::Vector2dF(0, 10), std::nullopt));
  RootCcLayer()->layer_tree_host()->ApplyCompositorChanges(&commit_data);
  EXPECT_EQ(gfx::PointF(0, 210), scrollable_area->ScrollPosition());
  EXPECT_EQ(gfx::PointF(0, 210), CurrentScrollOffset(element_id));
}

TEST_P(ScrollingTest, ThumbInvalidatesLayer) {
  ScopedMockOverlayScrollbars mock_overlay_scrollbar(false);
  LoadHTML(R"HTML(
    <div id='scroller' style='overflow-y: scroll; width: 100px; height: 100px'>
      <div style='height: 1000px'></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* scroll_node = ScrollNodeByDOMElementId("scroller");
  auto* layer = ScrollbarLayerForScrollNode(
      scroll_node, cc::ScrollbarOrientation::kVertical);
  // Solid color scrollbars do not repaint (see:
  // |SolidColorScrollbarLayer::SetNeedsDisplayRect|).
  if (layer->GetScrollbarLayerType() != cc::ScrollbarLayerBase::kSolidColor) {
    layer->ResetUpdateRectForTesting();
    ASSERT_TRUE(layer->update_rect().IsEmpty());

    auto* scrollable_area = ScrollableAreaByDOMElementId("scroller");
    scrollable_area->VerticalScrollbar()->SetNeedsPaintInvalidation(kThumbPart);
    EXPECT_FALSE(layer->update_rect().IsEmpty());
  }
}

class UnifiedScrollingSimTest : public SimTest, public PaintTestConfigurations {
 public:
  UnifiedScrollingSimTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    SetPreferCompositingToLCDText(false);
    WebView().MainFrameViewWidget()->Resize(gfx::Size(1000, 1000));
    WebView().MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void RunIdleTasks() {
    ThreadScheduler::Current()
        ->ToMainThreadScheduler()
        ->StartIdlePeriodForTesting();
    test::RunPendingTasks();
  }

  const cc::Layer* RootCcLayer() { return GetDocument().View()->RootCcLayer(); }

  const cc::ScrollNode* ScrollNodeForScrollableArea(
      const ScrollableArea* scrollable_area) {
    if (!scrollable_area)
      return nullptr;
    const auto* property_trees =
        RootCcLayer()->layer_tree_host()->property_trees();
    return property_trees->scroll_tree().FindNodeFromElementId(
        scrollable_area->GetScrollElementId());
  }

  PaintLayerScrollableArea* ScrollableAreaByDOMElementId(const char* id_value) {
    auto* box = MainFrame()
                    .GetFrame()
                    ->GetDocument()
                    ->getElementById(AtomicString(id_value))
                    ->GetLayoutBoxForScrolling();
    return box ? box->GetScrollableArea() : nullptr;
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(UnifiedScrollingSimTest);

// Tests that the compositor gets a scroll node for noncomposited scrollers by
// loading a page with a scroller that has an inset box-shadow, and ensuring
// that scroller generates a compositor scroll node with the proper
// noncomposited reasons set. It then removes the box-shadow property and
// ensures the compositor node updates accordingly.
TEST_P(UnifiedScrollingSimTest, ScrollNodeForNonCompositedScroller) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #noncomposited {
      width: 200px;
      height: 200px;
      overflow: auto;
      position: absolute;
      top: 300px;
      background: white;
      box-shadow: 10px 10px black inset;
    }
    #spacer {
      width: 100%;
      height: 10000px;
    }
    </style>
    <div id="noncomposited">
      <div id="spacer"></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* noncomposited_element =
      MainFrame().GetFrame()->GetDocument()->getElementById(
          AtomicString("noncomposited"));
  auto* scrollable_area =
      noncomposited_element->GetLayoutBoxForScrolling()->GetScrollableArea();
  const auto* scroll_node = ScrollNodeForScrollableArea(scrollable_area);
  ASSERT_NOT_COMPOSITED(
      scroll_node,
      RuntimeEnabledFeatures::RasterInducingScrollEnabled()
          ? cc::MainThreadScrollingReason::kNotScrollingOnMain
          : cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  EXPECT_EQ(scroll_node->element_id, scrollable_area->GetScrollElementId());

  // Now remove the box-shadow property and ensure the compositor scroll node
  // changes.
  noncomposited_element->setAttribute(html_names::kStyleAttr,
                                      AtomicString("box-shadow: none"));
  Compositor().BeginFrame();

  ASSERT_COMPOSITED(scroll_node);
  EXPECT_EQ(scroll_node->element_id, scrollable_area->GetScrollElementId());
}

// Tests that the compositor retains the scroll node for a composited scroller
// when it becomes noncomposited, and ensures the scroll node has its
// IsComposited state updated accordingly.
TEST_P(UnifiedScrollingSimTest,
       ScrollNodeForCompositedToNonCompositedScroller) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #composited {
      width: 200px;
      height: 200px;
      overflow: auto;
      position: absolute;
      top: 300px;
      background: white;
    }
    #spacer {
      width: 100%;
      height: 10000px;
    }
    </style>
    <div id="composited">
      <div id="spacer"></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* composited_element =
      MainFrame().GetFrame()->GetDocument()->getElementById(
          AtomicString("composited"));
  auto* scrollable_area =
      composited_element->GetLayoutBoxForScrolling()->GetScrollableArea();
  const auto* scroll_node = ScrollNodeForScrollableArea(scrollable_area);
  ASSERT_COMPOSITED(scroll_node);
  EXPECT_EQ(scroll_node->element_id, scrollable_area->GetScrollElementId());

  // Now add an inset box-shadow property to make the node noncomposited and
  // ensure the compositor scroll node updates accordingly.
  composited_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("box-shadow: 10px 10px black inset"));
  Compositor().BeginFrame();

  ASSERT_NOT_COMPOSITED(
      scroll_node,
      RuntimeEnabledFeatures::RasterInducingScrollEnabled()
          ? cc::MainThreadScrollingReason::kNotScrollingOnMain
          : cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  EXPECT_EQ(scroll_node->element_id, scrollable_area->GetScrollElementId());
}

// Tests that the compositor gets a scroll node for noncomposited scrollers
// embedded in an iframe, by loading a document with an iframe that has a
// scroller with an inset box shadow, and ensuring that scroller generates a
// compositor scroll node with the proper noncomposited reasons set.
TEST_P(UnifiedScrollingSimTest, ScrollNodeForEmbeddedScrollers) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #iframe {
      width: 300px;
      height: 300px;
      overflow: auto;
    }
    </style>
    <iframe id="iframe" srcdoc="
        <!DOCTYPE html>
        <style>
          body {
            background: white;
          }
          #scroller {
            width: 200px;
            height: 200px;
            overflow: auto;
            position: absolute;
            top: 50px;
            background: white;
            box-shadow: 10px 10px black inset;
          }
          #spacer {
            width: 100%;
            height: 10000px;
          }
        </style>
        <div id='scroller'>
          <div id='spacer'></div>
        </div>
        <div id='spacer'></div>">
    </iframe>
  )HTML");

  // RunIdleTasks to load the srcdoc iframe.
  RunIdleTasks();
  Compositor().BeginFrame();

  HTMLFrameOwnerElement* iframe = To<HTMLFrameOwnerElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  auto* iframe_scrollable_area =
      iframe->contentDocument()->View()->LayoutViewport();
  const auto* iframe_scroll_node =
      ScrollNodeForScrollableArea(iframe_scrollable_area);

  // The iframe itself is a composited scroller.
  ASSERT_COMPOSITED(iframe_scroll_node);
  EXPECT_EQ(iframe_scroll_node->element_id,
            iframe_scrollable_area->GetScrollElementId());

  // Ensure we have a compositor scroll node for the noncomposited subscroller.
  auto* child_scrollable_area = iframe->contentDocument()
                                    ->getElementById(AtomicString("scroller"))
                                    ->GetLayoutBoxForScrolling()
                                    ->GetScrollableArea();
  const auto* child_scroll_node =
      ScrollNodeForScrollableArea(child_scrollable_area);
  ASSERT_NOT_COMPOSITED(
      child_scroll_node,
      RuntimeEnabledFeatures::RasterInducingScrollEnabled()
          ? cc::MainThreadScrollingReason::kNotScrollingOnMain
          : cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  EXPECT_EQ(child_scroll_node->element_id,
            child_scrollable_area->GetScrollElementId());
}

// Similar to the above test, but for deeper nesting iframes to ensure we
// generate scroll nodes that are deeper than the main frame's children.
TEST_P(UnifiedScrollingSimTest, ScrollNodeForNestedEmbeddedScrollers) {
  SimRequest request("https://example.com/test.html", "text/html");
  SimRequest child_request_1("https://example.com/child1.html", "text/html");
  SimRequest child_request_2("https://example.com/child2.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    iframe {
      width: 300px;
      height: 300px;
      overflow: auto;
    }
    </style>
    <iframe id="child1" src="child1.html">
  )HTML");

  child_request_1.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    iframe {
      width: 300px;
      height: 300px;
      overflow: auto;
    }
    </style>
    <iframe id="child2" src="child2.html">
  )HTML");

  child_request_2.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scroller {
        width: 200px;
        height: 200px;
        overflow: auto;
        position: absolute;
        top: 50px;
        background: white;
        box-shadow: 10px 10px black inset;
      }
      #spacer {
        width: 100%;
        height: 10000px;
      }
    </style>
    <div id='scroller'>
      <div id='spacer'></div>
    </div>
    <div id='spacer'></div>
  )HTML");

  RunIdleTasks();
  Compositor().BeginFrame();

  HTMLFrameOwnerElement* child_iframe_1 = To<HTMLFrameOwnerElement>(
      GetDocument().getElementById(AtomicString("child1")));

  HTMLFrameOwnerElement* child_iframe_2 = To<HTMLFrameOwnerElement>(
      child_iframe_1->contentDocument()->getElementById(
          AtomicString("child2")));

  // Ensure we have a compositor scroll node for the noncomposited subscroller
  // nested in the second iframe.
  auto* child_scrollable_area = child_iframe_2->contentDocument()
                                    ->getElementById(AtomicString("scroller"))
                                    ->GetLayoutBoxForScrolling()
                                    ->GetScrollableArea();
  const auto* child_scroll_node =
      ScrollNodeForScrollableArea(child_scrollable_area);
  ASSERT_NOT_COMPOSITED(
      child_scroll_node,
      RuntimeEnabledFeatures::RasterInducingScrollEnabled()
          ? cc::MainThreadScrollingReason::kNotScrollingOnMain
          : cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  EXPECT_EQ(child_scroll_node->element_id,
            child_scrollable_area->GetScrollElementId());
}

// Tests that the compositor gets a scroll node for opacity 0 noncomposited
// scrollers by loading a page with an opacity 0 scroller that has an inset
// box-shadow, and ensuring that scroller generates a compositor scroll node
// with the proper noncomposited reasons set. The test also ensures that there
// is no scroll node for a display:none scroller, as there is no scrollable
// area.
TEST_P(UnifiedScrollingSimTest, ScrollNodeForInvisibleNonCompositedScroller) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    .noncomposited {
      width: 200px;
      height: 200px;
      overflow: auto;
      position: absolute;
      top: 300px;
      background: white;
      box-shadow: 10px 10px black inset;
    }
    #invisible {
      opacity: 0;
    }
    #displaynone {
      display: none;
    }
    #spacer {
      width: 100%;
      height: 10000px;
    }
    </style>
    <div id="invisible" class="noncomposited">
      <div id="spacer"></div>
    </div>
    <div id="displaynone" class="noncomposited">
      <div id="spacer"></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  // Ensure the opacity 0 noncomposited scrollable area generates a scroll node
  auto* invisible_scrollable_area = ScrollableAreaByDOMElementId("invisible");
  const auto* invisible_scroll_node =
      ScrollNodeForScrollableArea(invisible_scrollable_area);
  ASSERT_NOT_COMPOSITED(
      invisible_scroll_node,
      RuntimeEnabledFeatures::RasterInducingScrollEnabled()
          ? cc::MainThreadScrollingReason::kNotScrollingOnMain
          : cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  EXPECT_EQ(invisible_scroll_node->element_id,
            invisible_scrollable_area->GetScrollElementId());

  // Ensure there's no scrollable area (and therefore no scroll node) for a
  // display none scroller.
  EXPECT_EQ(nullptr, ScrollableAreaByDOMElementId("displaynone"));
}

// Tests that the compositor gets a scroll node for a non-composited (due to
// PaintLayerScrollableArea::PrefersNonCompositedScrolling()) scrollable input
// box.
TEST_P(UnifiedScrollingSimTest, ScrollNodeForInputBox) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        input {
          width: 50px;
        }
      </style>
      <input id="textinput" type="text" value="some overflowing text"/>
  )HTML");
  Compositor().BeginFrame();

  auto* scrollable_area = ScrollableAreaByDOMElementId("textinput");
  const auto* scroll_node = ScrollNodeForScrollableArea(scrollable_area);
  ASSERT_TRUE(scroll_node);
  EXPECT_EQ(cc::MainThreadScrollingReason::kPreferNonCompositedScrolling,
            scroll_node->main_thread_scrolling_reasons);
  EXPECT_FALSE(scroll_node->is_composited);
}

class ScrollingSimTest : public SimTest {
 public:
  ScrollingSimTest() = default;

  void SetUp() override {
    was_threaded_animation_enabled_ =
        content::TestBlinkWebUnitTestSupport::SetThreadedAnimationEnabled(true);

    SimTest::SetUp();
    SetPreferCompositingToLCDText(true);
    ResizeView(gfx::Size(1000, 1000));
    WebView().MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void TearDown() override {
    SimTest::TearDown();
    feature_list_.Reset();

    content::TestBlinkWebUnitTestSupport::SetThreadedAnimationEnabled(
        was_threaded_animation_enabled_);
  }

  WebGestureEvent GenerateGestureEvent(WebInputEvent::Type type,
                                       int delta_x = 0,
                                       int delta_y = 0) {
    WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                          WebInputEvent::GetStaticTimeStampForTests(),
                          WebGestureDevice::kTouchscreen);
    event.SetPositionInWidget(gfx::PointF(100, 100));
    if (type == WebInputEvent::Type::kGestureScrollUpdate) {
      event.data.scroll_update.delta_x = delta_x;
      event.data.scroll_update.delta_y = delta_y;
    } else if (type == WebInputEvent::Type::kGestureScrollBegin) {
      event.data.scroll_begin.delta_x_hint = delta_x;
      event.data.scroll_begin.delta_y_hint = delta_y;
    }
    return event;
  }

  WebCoalescedInputEvent GenerateCoalescedGestureEvent(WebInputEvent::Type type,
                                                       int delta_x = 0,
                                                       int delta_y = 0) {
    return WebCoalescedInputEvent(GenerateGestureEvent(type, delta_x, delta_y),
                                  ui::LatencyInfo());
  }

  unsigned NumObjectsNeedingLayout() {
    bool is_partial = false;
    unsigned num_objects_need_layout = 0;
    unsigned total_objects = 0;
    GetDocument().View()->CountObjectsNeedingLayout(num_objects_need_layout,
                                                    total_objects, is_partial);
    return num_objects_need_layout;
  }

  cc::LayerTreeHostImpl* GetLayerTreeHostImpl() {
    return static_cast<cc::SingleThreadProxy*>(
               GetWebFrameWidget().LayerTreeHostForTesting()->proxy())
        ->LayerTreeHostImplForTesting();
  }

  gfx::PointF GetActiveScrollOffset(PaintLayerScrollableArea* scroller) {
    return GetLayerTreeHostImpl()->GetScrollTree().current_scroll_offset(
        scroller->GetScrollElementId());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  bool was_threaded_animation_enabled_;
};

TEST_F(ScrollingSimTest, BasicScroll) {
  String kUrl = "https://example.com/test.html";
  SimRequest request(kUrl, "text/html");
  LoadURL(kUrl);

  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #s { overflow: scroll; width: 300px; height: 300px; }
      #sp { width: 600px; height: 600px; }
    </style>
    <div id=s><div id=sp>hello</div></div>
  )HTML");

  Compositor().BeginFrame();

  auto& widget = GetWebFrameWidget();
  widget.DispatchThroughCcInputHandler(
      GenerateGestureEvent(WebInputEvent::Type::kGestureScrollBegin, 0, -100));
  widget.DispatchThroughCcInputHandler(
      GenerateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, 0, -100));
  widget.DispatchThroughCcInputHandler(
      GenerateGestureEvent(WebInputEvent::Type::kGestureScrollEnd));

  Compositor().BeginFrame();

  Element* scroller = GetDocument().getElementById(AtomicString("s"));
  LayoutBox* box = To<LayoutBox>(scroller->GetLayoutObject());
  EXPECT_EQ(100, box->ScrolledContentOffset().top);
}

TEST_F(ScrollingSimTest, ImmediateCompositedScroll) {
  String kUrl = "https://example.com/test.html";
  SimRequest request(kUrl, "text/html");
  LoadURL(kUrl);

  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #s { overflow: scroll; width: 300px; height: 300px; background: white }
      #sp { width: 600px; height: 600px; }
    </style>
    <div id=s><div id=sp>hello</div></div>
  )HTML");

  Compositor().BeginFrame();
  Element* scroller = GetDocument().getElementById(AtomicString("s"));
  LayoutBox* box = To<LayoutBox>(scroller->GetLayoutObject());
  EXPECT_EQ(0, GetActiveScrollOffset(box->GetScrollableArea()).y());

  WebGestureEvent scroll_begin(
      WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  scroll_begin.SetPositionInWidget(gfx::PointF(100, 100));
  scroll_begin.data.scroll_begin.delta_y_hint = -100;

  WebGestureEvent scroll_update(
      WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  scroll_update.SetPositionInWidget(gfx::PointF(100, 100));
  scroll_update.data.scroll_update.delta_y = -100;

  WebGestureEvent scroll_end(
      WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  scroll_end.SetPositionInWidget(gfx::PointF(100, 100));

  auto& widget = GetWebFrameWidget();
  widget.DispatchThroughCcInputHandler(scroll_begin);
  widget.DispatchThroughCcInputHandler(scroll_update);
  widget.DispatchThroughCcInputHandler(scroll_end);

  // The scroll is applied immediately in the active tree.
  EXPECT_EQ(100, GetActiveScrollOffset(box->GetScrollableArea()).y());

  // Blink sees the scroll after the main thread lifecycle update.
  EXPECT_EQ(0, box->ScrolledContentOffset().top);
  Compositor().BeginFrame();
  EXPECT_EQ(100, box->ScrolledContentOffset().top);
}

TEST_F(ScrollingSimTest, CompositedScrollDeferredWithLinkedAnimation) {
  ScopedScrollTimelineForTest scroll_timeline_enabled(true);

  String kUrl = "https://example.com/test.html";
  SimRequest request(kUrl, "text/html");
  LoadURL(kUrl);

  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #s { overflow: scroll; width: 300px; height: 300px;
           background: white; position: relative; }
      #sp { width: 600px; height: 600px; }
      #align { width: 100%; height: 20px; position: absolute; background: blue;
               will-change: transform; animation: a linear 10s;
               animation-timeline: scroll(); }
      @keyframes a {
        0% { transform: translateY(0); }
        100% { transform: translateY(100px); }
      }
    </style>
    <div id=s><div id=sp><div id=align></div>hello</div></div>
  )HTML");

  Compositor().BeginFrame();

  // Slight hack: SimTest sets LayerTreeSettings::commit_to_active_tree == true,
  // so there is no pending tree, but AnimationHost doesn't understand that.
  // Simulate part of activation to get cc::ScrollTimeline::active_id_ set.
  GetLayerTreeHostImpl()
      ->mutator_host()
      ->PromoteScrollTimelinesPendingToActive();

  Element* scroller = GetDocument().getElementById(AtomicString("s"));
  LayoutBox* box = To<LayoutBox>(scroller->GetLayoutObject());

  WebGestureEvent scroll_begin(
      WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  scroll_begin.SetPositionInWidget(gfx::PointF(100, 100));
  scroll_begin.data.scroll_begin.delta_y_hint = -100;

  WebGestureEvent scroll_update(
      WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  scroll_update.SetPositionInWidget(gfx::PointF(100, 100));
  scroll_update.data.scroll_update.delta_y = -100;

  WebGestureEvent scroll_end(
      WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  scroll_end.SetPositionInWidget(gfx::PointF(100, 100));

  auto& widget = GetWebFrameWidget();
  widget.DispatchThroughCcInputHandler(scroll_begin);
  widget.DispatchThroughCcInputHandler(scroll_update);
  widget.DispatchThroughCcInputHandler(scroll_end);

  // Due to the scroll-linked animation, the scroll is NOT applied immediately
  // in the active tree. (Compare with ImmediateCompositedScroll test case.)
  EXPECT_EQ(0, GetActiveScrollOffset(box->GetScrollableArea()).y());

  // The scroll is applied to the active tree in LTHI::WillBeginImplFrame.
  Compositor().BeginFrame();
  EXPECT_EQ(100, GetActiveScrollOffset(box->GetScrollableArea()).y());
  EXPECT_EQ(100, box->ScrolledContentOffset().top);
}

TEST_F(ScrollingSimTest, CompositedStickyTracksMainRepaintScroll) {
  SetPreferCompositingToLCDText(false);

  String kUrl = "https://example.com/test.html";
  SimRequest request(kUrl, "text/html");
  LoadURL(kUrl);

  request.Complete(R"HTML(
    <style>
    .spincont { position: absolute;
                width: 10px; height: 10px; left: 50px; top: 20px; }
    .spinner { animation: spin 1s linear infinite; }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
    .scroller { position: absolute; overflow: scroll;
                left: 10px; top: 50px; width: 750px; height: 400px;
                border: 10px solid #ccc; }
    .spacer { position: absolute; width: 9000px; height: 100px; }
    .sticky { position: sticky; background: #eee;
              left: 50px; top: 50px; width: 600px; height: 200px; }
    .bluechip { position: absolute; background: blue; color: white;
                left: 100px; top: 50px; width: 200px; height: 30px; }
    </style>
    <div class="spincont"><div class="spinner">X</div></div>
    <div class="scroller">
      <div class="spacer">scrolling</div>
      <div class="sticky"><div class="bluechip">sticky?</div></div>
    </div>
  )HTML");

  Compositor().BeginFrame(0.016, /* raster */ true);
  Element* scroller = GetDocument().QuerySelector(AtomicString(".scroller"));
  LayoutBox* box = To<LayoutBox>(scroller->GetLayoutObject());
  EXPECT_EQ(0, GetActiveScrollOffset(box->GetScrollableArea()).y());

  WebGestureEvent scroll_begin(
      WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  scroll_begin.SetPositionInWidget(gfx::PointF(200, 200));
  scroll_begin.data.scroll_begin.delta_x_hint = -100;

  WebGestureEvent scroll_update(
      WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  scroll_update.SetPositionInWidget(gfx::PointF(200, 200));
  scroll_update.data.scroll_update.delta_x = -100;

  WebGestureEvent scroll_end(
      WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  scroll_end.SetPositionInWidget(gfx::PointF(200, 200));

  auto& widget = GetWebFrameWidget();
  widget.DispatchThroughCcInputHandler(scroll_begin);
  widget.DispatchThroughCcInputHandler(scroll_update);
  widget.DispatchThroughCcInputHandler(scroll_end);

  // Scroll applied immediately in the scroll tree.
  EXPECT_EQ(100, GetActiveScrollOffset(box->GetScrollableArea()).x());

  // Tick impl animation to dirty draw properties.
  static_cast<cc::SingleThreadProxy*>(
      GetWebFrameWidget().LayerTreeHostForTesting()->proxy())
      ->BeginImplFrameForTest(Compositor().LastFrameTime() +
                              base::Seconds(0.016));

  // Update draw properties.
  cc::LayerTreeHostImpl::FrameData frame;
  auto* lthi = GetLayerTreeHostImpl();
  lthi->PrepareToDraw(&frame);

  Element* sticky = GetDocument().QuerySelector(AtomicString(".sticky"));
  cc::ElementId sticky_translation = CompositorElementIdFromUniqueObjectId(
      sticky->GetLayoutObject()->UniqueId(),
      CompositorElementIdNamespace::kStickyTranslation);
  auto* transform_node = lthi->active_tree()
                             ->property_trees()
                             ->transform_tree()
                             .FindNodeFromElementId(sticky_translation);

  // Sticky translation should NOT reflect the updated scroll, since the scroll
  // is main-repainted and we haven't had a main frame yet.
  EXPECT_EQ(50, transform_node->to_parent.To2dTranslation().x());
}

TEST_F(ScrollingSimTest, ScrollTimelineActiveAtBoundary) {
  String kUrl = "https://example.com/test.html";
  SimRequest request(kUrl, "text/html");
  LoadURL(kUrl);

  request.Complete(R"HTML(
    <style>
      #s { overflow-y: scroll; width: 300px; height: 200px;
           position: relative; background: white; }
      #sp { width: 100px; height: 1000px; }
      #align { width: 100%; height: 20px; position: absolute; background: blue;
               will-change: transform; animation: a linear 10s;
               animation-timeline: scroll(); }
      @keyframes a {
        0% { transform: translateY(0); }
        100% { transform: translateY(800px); }
      }
    </style>
    <div id=s><div id=sp><div id=align></div>hello</div></div>
  )HTML");

  cc::AnimationHost* impl_host =
      static_cast<cc::AnimationHost*>(GetLayerTreeHostImpl()->mutator_host());

  // First frame: Initial commit creates the cc::Animation etc.
  Compositor().BeginFrame();

  blink::Animation* animation =
      GetDocument().getElementById(AtomicString("align"))->getAnimations()[0];
  cc::Animation* cc_animation =
      animation->GetCompositorAnimation()->CcAnimation();
  cc::ElementId element_id = cc_animation->element_id();

  gfx::KeyframeModel* keyframe_model_main =
      cc_animation->GetKeyframeModel(cc::TargetProperty::TRANSFORM);
  cc::KeyframeEffect* keyframe_effect =
      impl_host->GetElementAnimationsForElementIdForTesting(element_id)
          ->FirstKeyframeEffectForTesting();
  gfx::KeyframeModel* keyframe_model_impl =
      keyframe_effect->keyframe_models()[0].get();

  EXPECT_EQ(gfx::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            keyframe_model_impl->run_state());

  // Activate the timeline (see ScrollTimeline::IsActive), so that it will be
  // ticked during the next LTHI::Animate.
  impl_host->PromoteScrollTimelinesPendingToActive();

  // Second frame: LTHI::Animate transitions to RunState::STARTING. Pass
  // raster=true to also reach LTHI::UpdateAnimationState, which transitions
  // STARTING -> RUNNING.
  Compositor().BeginFrame(0.016, /* raster */ true);
  EXPECT_EQ(gfx::KeyframeModel::RUNNING, keyframe_model_impl->run_state());

  // Scroll to the end.
  GetDocument().getElementById(AtomicString("s"))->setScrollTop(800);

  // Third frame: LayerTreeHost::ApplyMutatorEvents dispatches
  // AnimationEvent::STARTED and resets
  // KeyframeModel::needs_synchronized_start_time_.
  Compositor().BeginFrame();
  EXPECT_EQ(gfx::KeyframeModel::RUNNING, keyframe_model_impl->run_state());

  // Verify that KeyframeModel::CalculatePhase returns ACTIVE for the case of
  // local_time == active_after_boundary_time.
  base::TimeTicks max = base::TimeTicks() + base::Seconds(100);
  EXPECT_TRUE(keyframe_model_main->HasActiveTime(max));
  EXPECT_TRUE(keyframe_model_impl->HasActiveTime(max));

  // Try reversed playbackRate, and verify that we are also ACTIVE in the case
  // local_time == before_active_boundary_time.
  animation->setPlaybackRate(-1);
  GetDocument().getElementById(AtomicString("s"))->setScrollTop(0);
  Compositor().BeginFrame(0.016, /* raster */ true);
  Compositor().BeginFrame();

  cc_animation = animation->GetCompositorAnimation()->CcAnimation();
  keyframe_model_main =
      cc_animation->GetKeyframeModel(cc::TargetProperty::TRANSFORM);
  keyframe_effect =
      impl_host->GetElementAnimationsForElementIdForTesting(element_id)
          ->FirstKeyframeEffectForTesting();
  keyframe_model_impl =
      keyframe_effect->GetKeyframeModelById(keyframe_model_main->id());

  EXPECT_EQ(gfx::KeyframeModel::RUNNING, keyframe_model_impl->run_state());
  EXPECT_TRUE(keyframe_model_main->HasActiveTime(base::TimeTicks()));
  EXPECT_TRUE(keyframe_model_impl->HasActiveTime(base::TimeTicks()));
}

// Ensure that a main thread hit test for ScrollBegin does cause layout.
TEST_F(ScrollingSimTest, ScrollLayoutTriggers) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
      #box {
        position: absolute;
      }
      body {
        height: 5000px;
      }
      </style>
      <div id='box'></div>
  )HTML");
  Compositor().BeginFrame();
  ASSERT_EQ(0u, NumObjectsNeedingLayout());

  Element* box = GetDocument().getElementById(AtomicString("box"));

  // Dirty the layout
  box->setAttribute(html_names::kStyleAttr, AtomicString("height: 10px"));
  GetDocument().UpdateStyleAndLayoutTree();
  ASSERT_NE(NumObjectsNeedingLayout(), 0u);

  // The hit test (which may be performed by a scroll begin) should cause a
  // layout to occur.
  WebView().MainFrameWidget()->HitTestResultAt(gfx::PointF(10, 10));
  EXPECT_EQ(NumObjectsNeedingLayout(), 0u);
}

// Verifies that a composited scrollbar scroll uses the target scroller
// specified by the widget input handler and does not bubble up.
TEST_F(ScrollingSimTest, CompositedScrollbarScrollDoesNotBubble) {
  String kUrl = "https://example.com/test.html";
  SimRequest request(kUrl, "text/html");
  LoadURL(kUrl);

  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #scroller {
      width: 100px;
      height: 100px;
      overflow: scroll;
    }
    .spacer {
      height: 2000px;
      width: 2000px;
    }
    </style>
    <div id="scroller"><div class="spacer">Hello, world!</div></div>
    <div class="spacer"></div>
  )HTML");

  Compositor().BeginFrame();

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  ScrollOffset max_offset = scroller->GetLayoutBoxForScrolling()
                                ->GetScrollableArea()
                                ->MaximumScrollOffset();
  // Scroll to the end. A subsequent non-latched upward gesture scroll
  // would bubble up to the root scroller; but a gesture scroll
  // generated for a composited scrollbar scroll should not bubble up.
  scroller->setScrollTop(max_offset.y());
  Compositor().BeginFrame();

  WebGestureEvent scroll_begin(WebInputEvent::Type::kGestureScrollBegin,
                               WebInputEvent::kNoModifiers,
                               WebInputEvent::GetStaticTimeStampForTests(),
                               WebGestureDevice::kScrollbar);
  // Location outside the scrolling div; input manager should accept the
  // targeted element without performing a hit test.
  scroll_begin.SetPositionInWidget(gfx::PointF(150, 150));
  scroll_begin.data.scroll_begin.main_thread_hit_tested_reasons =
      cc::MainThreadScrollingReason::kScrollbarScrolling;
  scroll_begin.data.scroll_begin.scrollable_area_element_id =
      CompositorElementIdFromUniqueObjectId(
          scroller->GetLayoutObject()->UniqueId(),
          CompositorElementIdNamespace::kScroll)
          .GetInternalValue();
  // Specify an upward scroll
  scroll_begin.data.scroll_begin.delta_y_hint = -1;
  auto& widget = GetWebFrameWidget();
  widget.DispatchThroughCcInputHandler(scroll_begin);

  WebGestureEvent scroll_update(WebInputEvent::Type::kGestureScrollUpdate,
                                WebInputEvent::kNoModifiers,
                                WebInputEvent::GetStaticTimeStampForTests(),
                                WebGestureDevice::kScrollbar);
  scroll_update.SetPositionInWidget(gfx::PointF(150, 150));
  scroll_update.data.scroll_update.delta_x = 0;
  scroll_update.data.scroll_update.delta_y = -13;
  widget.DispatchThroughCcInputHandler(scroll_update);

  Compositor().BeginFrame();

  EXPECT_EQ(GetDocument().View()->LayoutViewport()->GetScrollOffset(),
            ScrollOffset());
  EXPECT_EQ(scroller->GetLayoutBoxForScrolling()
                ->GetScrollableArea()
                ->GetScrollOffset(),
            ScrollOffset(0, max_offset.y()));
}

class ScrollingTestWithAcceleratedContext : public ScrollingTest {
 protected:
  void SetUp() override {
    auto factory = [](FakeGLES2Interface* gl)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      gl->SetIsContextLost(false);
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
    ScrollingTest::SetUp();
  }

  void TearDown() override {
    SharedGpuContext::Reset();
    ScrollingTest::TearDown();
  }

 private:
  FakeGLES2Interface gl_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollingTestWithAcceleratedContext);

TEST_P(ScrollingTestWithAcceleratedContext, CanvasTouchActionRects) {
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

  const auto* cc_layer = LayerByDOMElementId("canvas");
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(cc::Region(gfx::Rect(0, 0, 400, 400)), region);
}

}  // namespace blink
