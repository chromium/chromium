// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/input/cursor_manager.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/common/features.h"
#include "components/viz/test/host_frame_sink_manager_test_api.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/input/touch_emulator_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/common/input/synthetic_smooth_scroll_gesture.h"
#include "content/common/input/synthetic_tap_gesture.h"
#include "content/common/input/synthetic_touchpad_pinch_gesture.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/mock_overscroll_observer.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-test-utils.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom-test-utils.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/display_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/quad_f.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/test/mock_overscroll_controller_delegate_aura.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_rewriter.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_preferred_scroller_style_mac.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/test/mock_overscroll_refresh_handler_android.h"
#endif

namespace content {

namespace {

// TODO(nzolghadr): We need to have a much lower tolerance across the board.
constexpr float kHitTestTolerance = 1.f;
constexpr float kHitTestLowTolerance = 0.2f;

class TestInputEventObserver : public RenderWidgetHost::InputEventObserver {
 public:
  explicit TestInputEventObserver(RenderWidgetHost* host) : host_(host) {
    host_->AddInputEventObserver(this);
  }

  TestInputEventObserver(const TestInputEventObserver&) = delete;
  TestInputEventObserver& operator=(const TestInputEventObserver&) = delete;

  ~TestInputEventObserver() override { host_->RemoveInputEventObserver(this); }

  bool EventWasReceived() const { return !events_received_.empty(); }
  void ResetEventsReceived() { events_received_.clear(); }
  blink::WebInputEvent::Type EventType() const {
    DCHECK(EventWasReceived());
    return events_received_.front();
  }
  const std::vector<blink::WebInputEvent::Type>& events_received() {
    return events_received_;
  }

  const blink::WebInputEvent& event() const { return *event_; }

  void OnInputEvent(const blink::WebInputEvent& event) override {
    events_received_.push_back(event.GetType());
    event_ = event.Clone();
  }

  const std::vector<blink::mojom::InputEventResultSource>& events_acked() {
    return events_acked_;
  }

  void OnInputEventAck(blink::mojom::InputEventResultSource source,
                       blink::mojom::InputEventResultState state,
                       const blink::WebInputEvent&) override {
    events_acked_.push_back(source);
  }

 private:
  raw_ptr<RenderWidgetHost> host_;
  std::vector<blink::WebInputEvent::Type> events_received_;
  std::vector<blink::mojom::InputEventResultSource> events_acked_;
  ui::WebScopedInputEvent event_;
};

// |position_in_widget| is in the coord space of |rwhv|.
template <typename PointType>
void SetWebEventPositions(blink::WebPointerProperties* event,
                          const PointType& position_in_widget,
                          RenderWidgetHostViewBase* rwhv,
                          RenderWidgetHostViewBase* rwhv_root) {
  event->SetPositionInWidget(gfx::PointF(position_in_widget));
  const gfx::PointF position_in_root =
      rwhv->TransformPointToRootCoordSpaceF(event->PositionInWidget());
  const gfx::PointF point_in_screen =
      position_in_root + rwhv_root->GetViewBounds().OffsetFromOrigin();
  event->SetPositionInScreen(point_in_screen.x(), point_in_screen.y());
}

// For convenience when setting the position in the space of the root RWHV.
template <typename PointType>
void SetWebEventPositions(blink::WebPointerProperties* event,
                          const PointType& position_in_widget,
                          RenderWidgetHostViewBase* rwhv_root) {
  DCHECK(!rwhv_root->IsRenderWidgetHostViewChildFrame());
  SetWebEventPositions(event, position_in_widget, rwhv_root, rwhv_root);
}

#if defined(USE_AURA)
// |event->location()| is in the coord space of |rwhv|.
void UpdateEventRootLocation(ui::LocatedEvent* event,
                             RenderWidgetHostViewBase* rwhv,
                             RenderWidgetHostViewBase* rwhv_root) {
  const gfx::Point position_in_root =
      rwhv->TransformPointToRootCoordSpace(event->location());

  gfx::Point root_location = position_in_root;
  aura::Window::ConvertPointToTarget(
      rwhv_root->GetNativeView(), rwhv_root->GetNativeView()->GetRootWindow(),
      &root_location);

  event->set_root_location(root_location);
}

// For convenience when setting the position in the space of the root RWHV.
void UpdateEventRootLocation(ui::LocatedEvent* event,
                             RenderWidgetHostViewBase* rwhv_root) {
  DCHECK(!rwhv_root->IsRenderWidgetHostViewChildFrame());
  UpdateEventRootLocation(event, rwhv_root, rwhv_root);
}
#endif  // defined(USE_AURA)

void RouteMouseEventAndWaitUntilDispatch(
    input::RenderWidgetHostInputEventRouter* router,
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* expected_target,
    blink::WebMouseEvent* event) {
  InputEventAckWaiter waiter(expected_target->GetRenderWidgetHost(),
                             event->GetType());
  router->RouteMouseEvent(root_view, event, ui::LatencyInfo());
  waiter.Wait();
}

// Dispatch |event| to the specified view using browser process hit testing.
void DispatchMouseEventAndWaitUntilDispatch(
    WebContentsImpl* web_contents,
    blink::WebMouseEvent& event,
    RenderWidgetHostViewBase* location_view,
    const gfx::PointF& location,
    RenderWidgetHostViewBase* expected_target,
    const gfx::PointF& expected_location) {
  auto* router = web_contents->GetInputEventRouter();

  RenderWidgetHostMouseEventMonitor monitor(
      expected_target->GetRenderWidgetHost());
  gfx::PointF root_location =
      location_view->TransformPointToRootCoordSpaceF(location);
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  auto* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  SetWebEventPositions(&event, root_location, root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, expected_target,
                                      &event);
  EXPECT_TRUE(monitor.EventWasReceived());
  EXPECT_NEAR(expected_location.x(), monitor.event().PositionInWidget().x(),
              kHitTestTolerance)
      << " & original location was " << location.x() << ", " << location.y()
      << " & root_location was " << root_location.x() << ", "
      << root_location.y();
  EXPECT_NEAR(expected_location.y(), monitor.event().PositionInWidget().y(),
              kHitTestTolerance);
}

// Wrapper for the above method that creates a MouseDown to send.
void DispatchMouseDownEventAndWaitUntilDispatch(
    WebContentsImpl* web_contents,
    RenderWidgetHostViewBase* location_view,
    const gfx::PointF& location,
    RenderWidgetHostViewBase* expected_target,
    const gfx::PointF& expected_location) {
  blink::WebMouseEvent down_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  down_event.button = blink::WebPointerProperties::Button::kLeft;
  down_event.click_count = 1;
  DispatchMouseEventAndWaitUntilDispatch(web_contents, down_event,
                                         location_view, location,
                                         expected_target, expected_location);
}

// Helper function that performs a surface hittest.
void SurfaceHitTestTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  DispatchMouseDownEventAndWaitUntilDispatch(web_contents, rwhv_child,
                                             gfx::PointF(5, 5), rwhv_child,
                                             gfx::PointF(5, 5));

  DispatchMouseDownEventAndWaitUntilDispatch(
      web_contents, rwhv_root, gfx::PointF(2, 2), rwhv_root, gfx::PointF(2, 2));
}

void OverlapSurfaceHitTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_content_overlap_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  gfx::PointF parent_location = gfx::PointF(5, 5);
  parent_location =
      rwhv_child->TransformPointToRootCoordSpaceF(parent_location);
  DispatchMouseDownEventAndWaitUntilDispatch(
      web_contents, rwhv_child, gfx::PointF(5, 5), rwhv_root, parent_location);

  DispatchMouseDownEventAndWaitUntilDispatch(web_contents, rwhv_child,
                                             gfx::PointF(95, 95), rwhv_child,
                                             gfx::PointF(95, 95));
}

void NonFlatTransformedSurfaceHitTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_non_flat_transformed_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  DispatchMouseDownEventAndWaitUntilDispatch(web_contents, rwhv_child,
                                             gfx::PointF(5, 5), rwhv_child,
                                             gfx::PointF(5, 5));
}

void PerspectiveTransformedSurfaceHitTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_perspective_transformed_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  RenderFrameSubmissionObserver render_frame_submission_observer(web_contents);

  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  // (90, 75) hit tests into the child frame that is positioned at (50, 50).
  // Without other transformations this should result in a translated point
  // of (40, 25), but the 45 degree 3-dimensional rotation of the frame about
  // a vertical axis skews it.
  // We can't allow DispatchMouseDownEventAndWaitUntilDispatch to compute the
  // coordinates in the root space unless browser conversions with
  // perspective transforms are first fixed. See https://crbug.com/854257.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents, rwhv_root,
                                             gfx::PointF(90, 75), rwhv_child,
                                             gfx::PointF(33, 23));
}

// Helper function that performs a surface hittest in nested frame.
void NestedSurfaceHitTestTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL(
      "a.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_EQ(site_url, parent_iframe_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            parent_iframe_node->current_frame_host()->GetSiteInstance());

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  GURL nested_site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(nested_site_url, nested_iframe_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            nested_iframe_node->current_frame_host()->GetSiteInstance());
  EXPECT_NE(parent_iframe_node->current_frame_host()->GetSiteInstance(),
            nested_iframe_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  WaitForHitTestData(nested_iframe_node->current_frame_host());

  DispatchMouseDownEventAndWaitUntilDispatch(web_contents, rwhv_nested,
                                             gfx::PointF(10, 10), rwhv_nested,
                                             gfx::PointF(10, 10));
}

void HitTestLayerSquashing(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/oopif_hit_test_layer_squashing.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  gfx::Vector2dF child_offset = rwhv_child->GetViewBounds().origin() -
                                rwhv_root->GetViewBounds().origin();
  // Send a mouse-down on #B. The main-frame should receive it.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents, rwhv_root,
                                             gfx::PointF(195, 11), rwhv_root,
                                             gfx::PointF(195, 11));
  // Send another event just below. The child-frame should receive it.
  DispatchMouseDownEventAndWaitUntilDispatch(
      web_contents, rwhv_root, gfx::PointF(195, 30), rwhv_child,
      gfx::PointF(195, 30) - child_offset);
  // Send a mouse-down on #C.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents, rwhv_root,
                                             gfx::PointF(35, 195), rwhv_root,
                                             gfx::PointF(35, 195));
  // Send a mouse-down to the right of #C so that it goes to the child frame.
  DispatchMouseDownEventAndWaitUntilDispatch(
      web_contents, rwhv_root, gfx::PointF(55, 195), rwhv_child,
      gfx::PointF(55, 195) - child_offset);
  // Send a mouse-down to the right-bottom edge of the iframe.
  DispatchMouseDownEventAndWaitUntilDispatch(
      web_contents, rwhv_root, gfx::PointF(195, 235), rwhv_child,
      gfx::PointF(195, 235) - child_offset);
}

void HitTestWatermark(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/oopif_hit_test_watermark.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  gfx::Vector2dF child_offset = rwhv_child->GetViewBounds().origin() -
                                rwhv_root->GetViewBounds().origin();
  const gfx::PointF child_location(100, 120);
  // Send a mouse-down at the center of the iframe. This should go to the
  // main-frame (since there's a translucent div on top of it).
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents, rwhv_child,
                                             child_location, rwhv_root,
                                             child_location + child_offset);

  // Set 'pointer-events: none' on the div.
  EXPECT_TRUE(ExecJs(web_contents, "W.style.pointerEvents = 'none';"));

  DispatchMouseDownEventAndWaitUntilDispatch(
      web_contents, rwhv_child, child_location, rwhv_child, child_location);
}

void HitTestNestedFramesHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://a.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(*root));

  FrameTreeNode* child_node = root->child_at(0);
  FrameTreeNode* grandchild_node = child_node->child_at(0);
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_grandchild =
      static_cast<RenderWidgetHostViewBase*>(
          grandchild_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  WaitForHitTestData(child_node->current_frame_host());
  WaitForHitTestData(grandchild_node->current_frame_host());

  // Create two points to hit test: One in the child of the main frame, and
  // one in the frame nested within that. The hit test request is sent to the
  // child's renderer.
  gfx::PointF point_in_child(1.29, 1.59);
  gfx::PointF point_in_nested_child(5.52, 5.62);
  gfx::PointF point_in_nested_child_transformed;  // Transformed into child view
                                                  // coordinate space.
  rwhv_grandchild->TransformPointToCoordSpaceForView(
      point_in_nested_child, rwhv_child, &point_in_nested_child_transformed);

  {
    base::RunLoop run_loop;
    viz::FrameSinkId received_frame_sink_id;
    gfx::PointF returned_point;
    base::OnceClosure quit_closure = run_loop.QuitClosure();
    DCHECK(child_node->current_frame_host()
               ->GetRenderWidgetHost()
               ->GetRenderInputRouter()
               ->input_target_client());
    child_node->current_frame_host()
        ->GetRenderWidgetHost()
        ->GetRenderInputRouter()
        ->input_target_client()
        ->FrameSinkIdAt(
            point_in_child, 0,
            base::BindLambdaForTesting(
                [&](const viz::FrameSinkId& id, const gfx::PointF& point) {
                  received_frame_sink_id = id;
                  returned_point = point;
                  std::move(quit_closure).Run();
                }));
    run_loop.Run();
    // |point_in_child| should hit test to the view for |child_node|.
    ASSERT_EQ(rwhv_child->GetFrameSinkId(), received_frame_sink_id);
    EXPECT_NEAR(returned_point.x(), point_in_child.x(), kHitTestLowTolerance);
    EXPECT_NEAR(returned_point.y(), point_in_child.y(), kHitTestLowTolerance);
  }

  {
    base::RunLoop run_loop;
    viz::FrameSinkId received_frame_sink_id;
    gfx::PointF returned_point;
    base::OnceClosure quit_closure = run_loop.QuitClosure();
    DCHECK(child_node->current_frame_host()
               ->GetRenderWidgetHost()
               ->GetRenderInputRouter()
               ->input_target_client());
    child_node->current_frame_host()
        ->GetRenderWidgetHost()
        ->GetRenderInputRouter()
        ->input_target_client()
        ->FrameSinkIdAt(
            point_in_nested_child_transformed, 0,
            base::BindLambdaForTesting(
                [&](const viz::FrameSinkId& id, const gfx::PointF& point) {
                  received_frame_sink_id = id;
                  returned_point = point;
                  std::move(quit_closure).Run();
                }));
    run_loop.Run();
    // |point_in_nested_child_transformed| should hit test to |rwhv_grandchild|.
    ASSERT_EQ(rwhv_grandchild->GetFrameSinkId(), received_frame_sink_id);
    EXPECT_NEAR(returned_point.x(), point_in_nested_child.x(),
                kHitTestLowTolerance);
    EXPECT_NEAR(returned_point.y(), point_in_nested_child.y(),
                kHitTestLowTolerance);
  }
}

#if defined(USE_AURA)
void HitTestRootWindowTransform(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  // Apply transform to root window to test that we respect root window
  // transform when transforming event location.
  gfx::Transform transform;
  transform.RotateAboutXAxis(180.f);
  transform.Translate(0.f,
                      -shell->window()->GetHost()->window()->bounds().height());
  shell->window()->GetHost()->SetRootTransform(transform);

  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  DispatchMouseDownEventAndWaitUntilDispatch(web_contents, rwhv_child,
                                             gfx::PointF(5, 5), rwhv_child,
                                             gfx::PointF(5, 5));

  DispatchMouseDownEventAndWaitUntilDispatch(
      web_contents, rwhv_root, gfx::PointF(2, 2), rwhv_root, gfx::PointF(2, 2));
}
#endif  // defined(USE_AURA)

#if defined(USE_AURA)
bool ConvertJSONToPoint(const std::string& str, gfx::PointF* point) {
  std::optional<base::Value> value = base::JSONReader::Read(str);
  if (!value)
    return false;
  base::Value::Dict* root = value->GetIfDict();
  if (!root)
    return false;
  std::optional<double> x = root->FindDouble("x");
  std::optional<double> y = root->FindDouble("y");
  if (!x || !y)
    return false;
  point->set_x(*x);
  point->set_y(*y);
  return true;
}

bool ConvertJSONToRect(const std::string& str, gfx::Rect* rect) {
  std::optional<base::Value> value = base::JSONReader::Read(str);
  if (!value)
    return false;
  base::Value::Dict* root = value->GetIfDict();
  if (!root)
    return false;
  std::optional<int> x = root->FindInt("x");
  if (!x)
    return false;
  std::optional<int> y = root->FindInt("y");
  if (!y)
    return false;
  std::optional<int> width = root->FindInt("width");
  if (!width)
    return false;
  std::optional<int> height = root->FindInt("height");
  if (!height)
    return false;
  rect->set_x(*x);
  rect->set_y(*y);
  rect->set_width(*width);
  rect->set_height(*height);
  return true;
}
#endif  // defined(USE_AURA)

// Class for intercepting SetMouseCapture messages being sent to a
// RenderWidgetHost. Note that this only works for RenderWidgetHosts that
// are attached to RenderFrameHosts, and not those for page popups, which
// use different bindings.
class SetMouseCaptureInterceptor
    : public base::RefCountedThreadSafe<SetMouseCaptureInterceptor>,
      public blink::mojom::WidgetInputHandlerHostInterceptorForTesting {
 public:
  SetMouseCaptureInterceptor(RenderWidgetHostImpl* host)
      : msg_received_(false),
        capturing_(false),
        host_(host),
        impl_(receiver().internal_state()->impl()),
        swapped_impl_(receiver(), this) {}

  SetMouseCaptureInterceptor(const SetMouseCaptureInterceptor&) = delete;
  SetMouseCaptureInterceptor& operator=(const SetMouseCaptureInterceptor&) =
      delete;

  bool Capturing() const { return capturing_; }

  void Wait() {
    DCHECK(!run_loop_);
    if (msg_received_) {
      msg_received_ = false;
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
    msg_received_ = false;
  }

 protected:
  // blink::mojom::WidgetInputHandlerHostInterceptorForTesting:
  blink::mojom::WidgetInputHandlerHost* GetForwardingInterface() override {
    return impl_;
  }

  void SetMouseCapture(bool capturing) override {
    capturing_ = capturing;
    msg_received_ = true;
    if (run_loop_)
      run_loop_->Quit();
    GetForwardingInterface()->SetMouseCapture(capturing);
  }

 private:
  friend class base::RefCountedThreadSafe<SetMouseCaptureInterceptor>;

  ~SetMouseCaptureInterceptor() override = default;

  mojo::Receiver<blink::mojom::WidgetInputHandlerHost>& receiver() {
    return static_cast<input::InputRouterImpl*>(host_->input_router())
        ->host_receiver_for_testing();
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  bool msg_received_;
  bool capturing_;
  raw_ptr<RenderWidgetHostImpl> host_;
  raw_ptr<blink::mojom::WidgetInputHandlerHost> impl_;
  mojo::test::ScopedSwapImplForTesting<blink::mojom::WidgetInputHandlerHost>
      swapped_impl_;
};

#if defined(USE_AURA)
// A class to allow intercepting and discarding of all system-level events
// that might otherwise cause unpredictable behaviour in tests.
class SystemEventRewriter : public ui::EventRewriter {
 public:
  SystemEventRewriter() = default;

  SystemEventRewriter(const SystemEventRewriter&) = delete;
  SystemEventRewriter& operator=(const SystemEventRewriter&) = delete;

  ~SystemEventRewriter() override = default;

 private:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override {
    return DiscardEvent(continuation);
  }
};
#endif

enum class HitTestType {
  kDrawQuad,
  kSurfaceLayer,
};

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
bool IsScreenTooSmallForPopup(const display::ScreenInfo& screen_info) {
  // Small display size will cause popup positions to be adjusted,
  // causing test failures.
  //
  // The size adjustment happens in adjustWindowRect()
  // (third_party/blink/renderer/core/html/forms/resources/pickerCommon.js
  // lines 132-133).
  static constexpr gfx::Size kMinimumScreenSize(300, 300);
  return screen_info.rect.width() < kMinimumScreenSize.width() ||
         screen_info.rect.height() < kMinimumScreenSize.height();
}
#endif

}  // namespace

class SitePerProcessHitTestBrowserTest : public SitePerProcessBrowserTestBase {
 public:
  SitePerProcessHitTestBrowserTest() {}

#if defined(USE_AURA)
  void PreRunTestOnMainThread() override {
    SitePerProcessBrowserTestBase::PreRunTestOnMainThread();
    // Disable system mouse events, which can interfere with tests.
    shell()->window()->GetHost()->AddEventRewriter(&event_rewriter_);
  }

  void PostRunTestOnMainThread() override {
    shell()->window()->GetHost()->RemoveEventRewriter(&event_rewriter_);
    SitePerProcessBrowserTestBase::PostRunTestOnMainThread();
  }
#endif

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    ui::PlatformEventSource::SetIgnoreNativePlatformEvents(true);
  }

#if defined(USE_AURA)
  SystemEventRewriter event_rewriter_;
#endif
};

//
// SitePerProcessHighDPIHitTestBrowserTest
//

class SitePerProcessHighDPIHitTestBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  const double kDeviceScaleFactor = 2.0;

  SitePerProcessHighDPIHitTestBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessHitTestBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", kDeviceScaleFactor));
  }
};

//
// SitePerProcessNonIntegerScaleFactorHitTestBrowserTest
//

class SitePerProcessNonIntegerScaleFactorHitTestBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  const double kDeviceScaleFactor = 1.5;

  SitePerProcessNonIntegerScaleFactorHitTestBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessHitTestBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", kDeviceScaleFactor));
  }
};

//
// SitePerProcessUserActivationHitTestBrowserTest
//

class SitePerProcessUserActivationHitTestBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  SitePerProcessUserActivationHitTestBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    ui::PlatformEventSource::SetIgnoreNativePlatformEvents(true);
    feature_list_.InitAndEnableFeature(
        features::kBrowserVerifiedUserActivationMouse);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Restrict to Aura to we can use routable MouseWheel event via
// RenderWidgetHostViewAura::OnScrollEvent().
#if defined(USE_AURA)
class SitePerProcessInternalsHitTestBrowserTest
    : public testing::WithParamInterface<std::tuple<float>>,
      public SitePerProcessHitTestBrowserTest {
 public:
  SitePerProcessInternalsHitTestBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessHitTestBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
    // Needed to guarantee the scrollable div we're testing with is not given
    // its own compositing layer.
    command_line->AppendSwitch(
        blink::switches::kDisablePreferCompositingToLCDText);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", std::get<0>(GetParam())));
  }
};

constexpr float kMultiScale[] = {1.f, 1.5f, 2.f};
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessInternalsHitTestBrowserTest,
                         testing::Combine(testing::ValuesIn(kMultiScale)));

// Flaky on MSAN. https://crbug.com/959924
// Flaky on Linux Wayland and Lacros. https://crbug.com/1158437
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_ScrollNestedLocalNonFastScrollableDiv \
  DISABLED_ScrollNestedLocalNonFastScrollableDiv
#else
#define MAYBE_ScrollNestedLocalNonFastScrollableDiv \
  ScrollNestedLocalNonFastScrollableDiv
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessInternalsHitTestBrowserTest,
                       MAYBE_ScrollNestedLocalNonFastScrollableDiv) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);

  GURL site_url(embedded_test_server()->GetURL(
      "b.com", "/tall_page_with_local_iframe.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(parent_iframe_node, site_url));

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  WaitForHitTestData(nested_iframe_node->current_frame_host());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  static constexpr char kGetElementLocationScriptFmt[] =
      "var rect = "
      "document.getElementById('%s').getBoundingClientRect();\n"
      "var point = {\n"
      "  x: rect.left,\n"
      "  y: rect.top\n"
      "};\n"
      "JSON.stringify(point);";

  // Since the nested local b-frame shares the RenderWidgetHostViewChildFrame
  // with the parent frame, we need to query element offsets in both documents
  // before converting to root space coordinates for the wheel event.
  gfx::PointF nested_point_f;
  ConvertJSONToPoint(
      EvalJs(nested_iframe_node->current_frame_host(),
             base::StringPrintf(kGetElementLocationScriptFmt, "scrollable_div"))
          .ExtractString(),
      &nested_point_f);

  gfx::PointF parent_offset_f;
  ConvertJSONToPoint(
      EvalJs(parent_iframe_node->current_frame_host(),
             base::StringPrintf(kGetElementLocationScriptFmt, "nested_frame"))
          .ExtractString(),
      &parent_offset_f);

  // Compute location for wheel event.
  gfx::PointF point_f(parent_offset_f.x() + nested_point_f.x() + 5.f,
                      parent_offset_f.y() + nested_point_f.y() + 5.f);

  RenderWidgetHostViewChildFrame* rwhv_nested =
      static_cast<RenderWidgetHostViewChildFrame*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());
  point_f = rwhv_nested->TransformPointToRootCoordSpaceF(point_f);

  RenderWidgetHostViewAura* rwhv_root = static_cast<RenderWidgetHostViewAura*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  gfx::PointF nested_in_parent;
  rwhv_root->TransformPointToCoordSpaceForView(
      point_f,
      parent_iframe_node->current_frame_host()
          ->GetRenderWidgetHost()
          ->GetView(),
      &nested_in_parent);

  // Get original scroll position.
  double div_scroll_top_start =
      EvalJs(nested_iframe_node->current_frame_host(),
             "document.getElementById('scrollable_div').scrollTop;")
          .ExtractDouble();
  EXPECT_EQ(0.0, div_scroll_top_start);

  // Wait until renderer's compositor thread is synced. Otherwise the non fast
  // scrollable regions won't be set when the event arrives.
  MainThreadFrameObserver observer(rwhv_nested->GetRenderWidgetHost());
  observer.Wait();

  // Send a wheel to scroll the div.
  gfx::Point location(point_f.x(), point_f.y());
  ui::ScrollEvent scroll_event(
      ui::EventType::kScroll, location, ui::EventTimeForNow(), 0, 0,
      -ui::MouseWheelEvent::kWheelDelta, 0, ui::MouseWheelEvent::kWheelDelta,
      2);  // This must be '2' or it gets silently
           // dropped.
  UpdateEventRootLocation(&scroll_event, rwhv_root);

  InputEventAckWaiter ack_observer(
      parent_iframe_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollUpdate);
  rwhv_root->OnScrollEvent(&scroll_event);
  ack_observer.Wait();

  // Wait until renderer's main thread is synced.
  observer.Wait();

  // Verify the div scrolled.
  EXPECT_NE(div_scroll_top_start,
            EvalJs(nested_iframe_node->current_frame_host(),
                   "document.getElementById('scrollable_div').scrollTop;"));
}

// TODO(crbug.com/41457695): disabled because tests are flaky
IN_PROC_BROWSER_TEST_P(SitePerProcessInternalsHitTestBrowserTest,
                       DISABLED_NestedLocalNonFastScrollableDivCoordsAreLocal) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);

  GURL site_url(embedded_test_server()->GetURL(
      "b.com", "/tall_page_with_local_iframe.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(parent_iframe_node, site_url));

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  WaitForHitTestData(nested_iframe_node->current_frame_host());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  static constexpr char kGetElementLocationScriptFmt[] =
      "var rect = "
      "document.getElementById('%s').getBoundingClientRect();\n"
      "var point = {\n"
      "  x: rect.left,\n"
      "  y: rect.top\n"
      "};\n"
      "JSON.stringify(point);";

  // Since the nested local b-frame shares the RenderWidgetHostViewChildFrame
  // with the parent frame, we need to query element offsets in both documents
  // before converting to root space coordinates for the wheel event.
  gfx::PointF nested_point_f;
  ConvertJSONToPoint(
      EvalJs(nested_iframe_node->current_frame_host(),
             base::StringPrintf(kGetElementLocationScriptFmt, "scrollable_div"))
          .ExtractString(),
      &nested_point_f);

  EXPECT_EQ(
      1,
      EvalJs(parent_iframe_node->current_frame_host(),
             "window.internals.markGestureScrollRegionDirty(document);\n"
             "window.internals.forceCompositingUpdate(document);\n"
             "var rects = window.internals.nonFastScrollableRects(document);\n"
             "rects.length;"));
  gfx::Rect non_fast_scrollable_rect_before_scroll;
  ConvertJSONToRect(EvalJs(parent_iframe_node->current_frame_host(),
                           "var rect = {\n"
                           "  x: rects[0].left,\n"
                           "  y: rects[0].top,\n"
                           "  width: rects[0].width,\n"
                           "  height: rects[0].height\n"
                           "};\n"
                           "JSON.stringify(rect);")
                        .ExtractString(),
                    &non_fast_scrollable_rect_before_scroll);

  gfx::PointF parent_offset_f;
  ConvertJSONToPoint(
      EvalJs(parent_iframe_node->current_frame_host(),
             base::StringPrintf(kGetElementLocationScriptFmt, "nested_frame"))
          .ExtractString(),
      &parent_offset_f);

  // Compute location for wheel event to scroll the parent with respect to the
  // mainframe.
  gfx::PointF point_f(parent_offset_f.x() + 1.f, parent_offset_f.y() + 1.f);

  RenderWidgetHostViewChildFrame* rwhv_parent =
      static_cast<RenderWidgetHostViewChildFrame*>(
          parent_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());
  point_f = rwhv_parent->TransformPointToRootCoordSpaceF(point_f);

  RenderWidgetHostViewAura* rwhv_root = static_cast<RenderWidgetHostViewAura*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  gfx::PointF nested_in_parent;
  rwhv_root->TransformPointToCoordSpaceForView(
      point_f,
      parent_iframe_node->current_frame_host()
          ->GetRenderWidgetHost()
          ->GetView(),
      &nested_in_parent);

  // Get original scroll position.
  double div_scroll_top_start = EvalJs(parent_iframe_node->current_frame_host(),
                                       "document.body.scrollTop;")
                                    .ExtractDouble();
  EXPECT_EQ(0.0, div_scroll_top_start);

  // Send a wheel to scroll the parent containing the div.
  gfx::Point location(point_f.x(), point_f.y());
  ui::ScrollEvent scroll_event(
      ui::EventType::kScroll, location, ui::EventTimeForNow(), 0, 0,
      -ui::MouseWheelEvent::kWheelDelta, 0, ui::MouseWheelEvent::kWheelDelta,
      2);  // This must be '2' or it gets silently
           // dropped.
  UpdateEventRootLocation(&scroll_event, rwhv_root);

  InputEventAckWaiter ack_observer(
      parent_iframe_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollUpdate);
  rwhv_root->OnScrollEvent(&scroll_event);
  ack_observer.Wait();

  MainThreadFrameObserver thread_observer(rwhv_parent->GetRenderWidgetHost());
  thread_observer.Wait();

  // Check compositor layers.
  // We expect the nested OOPIF to not have any compositor layers.
  EXPECT_EQ(std::string(),
            EvalJs(nested_iframe_node->current_frame_host(),
                   "window.internals.layerTreeAsText(document);"));

  // Verify the div scrolled.
  EXPECT_NE(div_scroll_top_start,
            EvalJs(parent_iframe_node->current_frame_host(),
                   "document.body.scrollTop;"));

  // Verify the non-fast scrollable region rect is the same, even though the
  // parent scroll isn't.
  EXPECT_EQ(
      1, EvalJs(parent_iframe_node->current_frame_host(),
                "window.internals.markGestureScrollRegionDirty(document);"
                "window.internals.forceCompositingUpdate(document);"
                "var rects = window.internals.nonFastScrollableRects(document);"
                "rects.length;"));
  gfx::Rect non_fast_scrollable_rect_after_scroll;
  ConvertJSONToRect(EvalJs(parent_iframe_node->current_frame_host(),
                           "var rect = {"
                           "  x: rects[0].left,"
                           "  y: rects[0].top,"
                           "  width: rects[0].width,"
                           "  height: rects[0].height"
                           "};"
                           "JSON.stringify(rect);")
                        .ExtractString(),
                    &non_fast_scrollable_rect_after_scroll);
  EXPECT_EQ(non_fast_scrollable_rect_before_scroll,
            non_fast_scrollable_rect_after_scroll);
}
#endif  // defined(USE_AURA)

// Tests that wheel scroll bubbling gets cancelled when the wheel target view
// gets destroyed in the middle of a wheel scroll seqeunce. This happens in
// cases like overscroll navigation from inside an oopif.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       CancelWheelScrollBubblingOnWheelTargetDeletion) {
  ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
      0);
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, iframe_node->current_url());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  RenderWidgetHostViewBase* child_rwhv = static_cast<RenderWidgetHostViewBase*>(
      iframe_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  input::RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();

  WaitForHitTestData(iframe_node->current_frame_host());

  InputEventAckWaiter scroll_begin_observer(
      root->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollBegin);
  InputEventAckWaiter scroll_end_observer(
      root->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollEnd);

  // Scroll the iframe upward, scroll events get bubbled up to the root.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gfx::Rect bounds = child_rwhv->GetViewBounds();
  float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;
  gfx::Point position_in_widget(
      base::ClampCeil((bounds.x() - root_view->GetViewBounds().x() + 5) *
                      scale_factor),
      base::ClampCeil((bounds.y() - root_view->GetViewBounds().y() + 5) *
                      scale_factor));
  SetWebEventPositions(&scroll_event, position_in_widget, root_view);
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = 5.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  router->RouteMouseWheelEvent(root_view, &scroll_event, ui::LatencyInfo());
  scroll_begin_observer.Wait();

  // Now destroy the child_rwhv, scroll bubbling stops and a GSE gets sent to
  // the root_view.
  RenderProcessHost* rph =
      iframe_node->current_frame_host()->GetSiteInstance()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      rph, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(rph->Shutdown(0));
  crash_observer.Wait();
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  scroll_event.dispatch_type =
      blink::WebInputEvent::DispatchType::kEventNonBlocking;
  router->RouteMouseWheelEvent(root_view, &scroll_event, ui::LatencyInfo());
  scroll_end_observer.Wait();
}

// Ensure that the positions of touch events sent to cross-process subframes
// account for any change in the position of the subframe during the scroll
// sequence.
// Before the issue fix, we record the transform for root to subframe coordinate
// space and reuse it in the sequence. It is wrong if the subframe moved in the
// sequence. In this test, the point passed to subframe at the touch end (scroll
// end) would be wrong because the subframe moved in scroll.
// Suppose the offset of subframe in rootframe is (0, 0) in the test, the touch
// start position in root is (15, 15) same in subframe, then move to (15, 10)
// in rootframe and subframe it caused subframe scroll down for 5px, then touch
// release in (15, 10) same as the touch move in root frame. Before the fix the
// touch end would pass (15, 10) to subframe which should be (15, 15) in
// subframe.
// https://crbug.com/959848: Flaky on Linux MSAN bots
// https://crbug.com/959924: Flaky on Android MSAN bots
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#define MAYBE_TouchAndGestureEventPositionChange \
  DISABLED_TouchAndGestureEventPositionChange
#else
#define MAYBE_TouchAndGestureEventPositionChange \
  TouchAndGestureEventPositionChange
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       MAYBE_TouchAndGestureEventPositionChange) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_tall_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  auto* root_rwhv = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  RenderWidgetHostViewChildFrame* child_rwhv =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(0)->current_frame_host()->GetView());
  WaitForHitTestData(root->child_at(0)->current_frame_host());

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  const float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;

  auto await_touch_event_with_position = base::BindRepeating(
      [](blink::WebInputEvent::Type expected_type,
         RenderWidgetHostViewBase* rwhv, gfx::PointF expected_position,
         gfx::PointF expected_position_in_root,
         blink::mojom::InputEventResultSource,
         blink::mojom::InputEventResultState,
         const blink::WebInputEvent& event) {
        if (event.GetType() != expected_type)
          return false;

        const auto& touch_event =
            static_cast<const blink::WebTouchEvent&>(event);
        const gfx::PointF root_point = rwhv->TransformPointToRootCoordSpaceF(
            touch_event.touches[0].PositionInWidget());

        EXPECT_NEAR(touch_event.touches[0].PositionInWidget().x(),
                    expected_position.x(), 1.0f);
        EXPECT_NEAR(touch_event.touches[0].PositionInWidget().y(),
                    expected_position.y(), 1.0f);
        EXPECT_NEAR(root_point.x(), expected_position_in_root.x(), 1.0f);
        EXPECT_NEAR(root_point.y(), expected_position_in_root.y(), 1.0f);
        return true;
      });

  auto await_gesture_event_with_position = base::BindRepeating(
      [](blink::WebInputEvent::Type expected_type,
         RenderWidgetHostViewBase* rwhv, gfx::PointF expected_position,
         gfx::PointF expected_position_in_root,
         blink::mojom::InputEventResultSource,
         blink::mojom::InputEventResultState,
         const blink::WebInputEvent& event) {
        if (event.GetType() != expected_type)
          return false;

        const auto& gesture_event =
            static_cast<const blink::WebGestureEvent&>(event);
        const gfx::PointF root_point = rwhv->TransformPointToRootCoordSpaceF(
            gesture_event.PositionInWidget());

        EXPECT_NEAR(gesture_event.PositionInWidget().x(), expected_position.x(),
                    1.0f);
        EXPECT_NEAR(gesture_event.PositionInWidget().y(), expected_position.y(),
                    1.0f);
        EXPECT_NEAR(root_point.x(), expected_position_in_root.x(), 1.0f);
        EXPECT_NEAR(root_point.y(), expected_position_in_root.y(), 1.0f);
        return true;
      });

  MainThreadFrameObserver thread_observer(root_rwhv->GetRenderWidgetHost());

  gfx::PointF touch_start_point_in_child(15, 15);
  gfx::PointF touch_move_point_in_child(15, 10);

  gfx::PointF touch_start_point =
      child_rwhv->TransformPointToRootCoordSpaceF(touch_start_point_in_child);
  gfx::PointF touch_move_point =
      child_rwhv->TransformPointToRootCoordSpaceF(touch_move_point_in_child);

  // Touch start
  {
    blink::WebTouchEvent touch_start_event(
        blink::WebInputEvent::Type::kTouchStart,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    touch_start_event.touches_length = 1;
    touch_start_event.touches[0].state =
        blink::WebTouchPoint::State::kStatePressed;
    touch_start_event.touches[0].SetPositionInWidget(touch_start_point);
    touch_start_event.unique_touch_event_id = 1;

    InputEventAckWaiter await_begin_in_child(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_touch_event_with_position,
                            blink::WebInputEvent::Type::kTouchStart, child_rwhv,
                            touch_start_point_in_child, touch_start_point));

    router->RouteTouchEvent(root_rwhv, &touch_start_event, ui::LatencyInfo());

    await_begin_in_child.Wait();

    blink::WebGestureEvent gesture_tap_event(
        blink::WebInputEvent::Type::kGestureTapDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchscreen);
    gesture_tap_event.unique_touch_event_id = 1;
    gesture_tap_event.SetPositionInWidget(touch_start_point);
    InputEventAckWaiter await_tap_in_child(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_gesture_event_with_position,
                            blink::WebInputEvent::Type::kGestureTapDown,
                            child_rwhv, touch_start_point_in_child,
                            touch_start_point));
    router->RouteGestureEvent(root_rwhv, &gesture_tap_event, ui::LatencyInfo());
    await_tap_in_child.Wait();
  }

  // Touch move
  {
    blink::WebTouchEvent touch_move_event(
        blink::WebInputEvent::Type::kTouchMove,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    touch_move_event.touches_length = 1;
    touch_move_event.touches[0].state =
        blink::WebTouchPoint::State::kStateMoved;
    touch_move_event.touches[0].SetPositionInWidget(touch_move_point);
    touch_move_event.unique_touch_event_id = 2;
    InputEventAckWaiter await_move_in_child(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_touch_event_with_position,
                            blink::WebInputEvent::Type::kTouchMove, child_rwhv,
                            touch_move_point_in_child, touch_move_point));
    router->RouteTouchEvent(root_rwhv, &touch_move_event, ui::LatencyInfo());
    await_move_in_child.Wait();
  }

  // Gesture Begin and update
  {
    blink::WebGestureEvent gesture_scroll_begin(
        blink::WebGestureEvent::Type::kGestureScrollBegin,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchscreen);
    gesture_scroll_begin.unique_touch_event_id = 2;
    gesture_scroll_begin.data.scroll_begin.delta_hint_units =
        ui::ScrollGranularity::kScrollByPrecisePixel;
    gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0.f;
    gesture_scroll_begin.data.scroll_begin.delta_y_hint = -5.f * scale_factor;
    gesture_scroll_begin.SetPositionInWidget(touch_start_point);

    blink::WebGestureEvent gesture_scroll_update(
        blink::WebGestureEvent::Type::kGestureScrollUpdate,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchscreen);
    gesture_scroll_update.unique_touch_event_id = 2;
    gesture_scroll_update.data.scroll_update.delta_units =
        ui::ScrollGranularity::kScrollByPrecisePixel;
    gesture_scroll_update.data.scroll_update.delta_x = 0.f;
    gesture_scroll_update.data.scroll_update.delta_y = -5.f * scale_factor;
    gesture_scroll_update.SetPositionInWidget(touch_start_point);

    InputEventAckWaiter await_begin_in_child(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_gesture_event_with_position,
                            blink::WebInputEvent::Type::kGestureScrollBegin,
                            child_rwhv, touch_start_point_in_child,
                            touch_start_point));
    InputEventAckWaiter await_update_in_child(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_gesture_event_with_position,
                            blink::WebInputEvent::Type::kGestureScrollUpdate,
                            child_rwhv, touch_start_point_in_child,
                            touch_start_point));
    InputEventAckWaiter await_update_in_root(
        root_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_gesture_event_with_position,
                            blink::WebInputEvent::Type::kGestureScrollUpdate,
                            root_rwhv, touch_start_point, touch_start_point));

    router->RouteGestureEvent(root_rwhv, &gesture_scroll_begin,
                              ui::LatencyInfo());
    await_begin_in_child.Wait();
    router->RouteGestureEvent(root_rwhv, &gesture_scroll_update,
                              ui::LatencyInfo());
    await_update_in_child.Wait();
    await_update_in_root.Wait();
    thread_observer.Wait();
  }

  // Touch end & Scroll end
  {
    blink::WebTouchEvent touch_end_event(
        blink::WebInputEvent::Type::kTouchEnd,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    touch_end_event.touches_length = 1;
    touch_end_event.touches[0].state =
        blink::WebTouchPoint::State::kStateReleased;
    touch_end_event.touches[0].SetPositionInWidget(touch_move_point);
    touch_end_event.unique_touch_event_id = 3;
    InputEventAckWaiter await_end_in_child(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_touch_event_with_position,
                            blink::WebInputEvent::Type::kTouchEnd, child_rwhv,
                            touch_start_point_in_child, touch_move_point));
    router->RouteTouchEvent(root_rwhv, &touch_end_event, ui::LatencyInfo());
    await_end_in_child.Wait();

    blink::WebGestureEvent gesture_scroll_end(
        blink::WebGestureEvent::Type::kGestureScrollEnd,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchscreen);
    gesture_scroll_end.unique_touch_event_id = 3;
    gesture_scroll_end.data.scroll_end.delta_units =
        ui::ScrollGranularity::kScrollByPrecisePixel;
    gesture_scroll_end.SetPositionInWidget(touch_move_point);

    InputEventAckWaiter await_scroll_end_in_child(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_gesture_event_with_position,
                            blink::WebInputEvent::Type::kGestureScrollEnd,
                            child_rwhv, touch_start_point_in_child,
                            touch_move_point));
    router->RouteGestureEvent(root_rwhv, &gesture_scroll_end,
                              ui::LatencyInfo());
    await_scroll_end_in_child.Wait();

    thread_observer.Wait();
  }
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       CSSTransformedIframeTouchEventCoordinates) {
  GURL url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_scaled_frame.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  FrameTreeNode* root_frame_tree_node =
      web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root_frame_tree_node->child_count());
  FrameTreeNode* child_frame_tree_node = root_frame_tree_node->child_at(0);
  GURL child_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(child_url, child_frame_tree_node->current_url());

  auto* root_rwhv = static_cast<RenderWidgetHostViewBase*>(
      root_frame_tree_node->current_frame_host()
          ->GetRenderWidgetHost()
          ->GetView());
  auto* child_rwhv = static_cast<RenderWidgetHostViewBase*>(
      child_frame_tree_node->current_frame_host()
          ->GetRenderWidgetHost()
          ->GetView());

  WaitForHitTestData(child_frame_tree_node->current_frame_host());

  const float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;

  // Some basic tests on the transforms between child and root. These assume
  // a CSS scale of 0.5 on the child, though should be robust to placement of
  // the iframe.
  float kScaleTolerance = 0.0001f;
  gfx::Transform transform_to_child;
  ASSERT_TRUE(
      root_rwhv->GetTransformToViewCoordSpace(child_rwhv, &transform_to_child));
  EXPECT_TRUE(transform_to_child.IsScaleOrTranslation());
  EXPECT_NEAR(2.f / scale_factor, transform_to_child.rc(0, 0), kScaleTolerance);
  EXPECT_NEAR(2.f / scale_factor, transform_to_child.rc(1, 1), kScaleTolerance);

  gfx::PointF child_origin =
      child_rwhv->TransformPointToRootCoordSpaceF(gfx::PointF());

  gfx::Transform transform_from_child;
  ASSERT_TRUE(child_rwhv->GetTransformToViewCoordSpace(root_rwhv,
                                                       &transform_from_child));
  EXPECT_TRUE(transform_from_child.IsScaleOrTranslation());
  EXPECT_NEAR(0.5f * scale_factor, transform_from_child.rc(0, 0),
              kScaleTolerance);
  EXPECT_NEAR(0.5f * scale_factor, transform_from_child.rc(1, 1),
              kScaleTolerance);
  EXPECT_EQ(child_origin.x(), transform_from_child.rc(0, 3));
  EXPECT_EQ(child_origin.y(), transform_from_child.rc(1, 3));

  gfx::Transform transform_child_to_child =
      transform_from_child * transform_to_child;
  // If the scale factor is 1.f, then this multiplication of the transform with
  // its inverse will be exact, and IsIdentity will indicate that. However, if
  // the scale is an arbitrary float (as on Android), then we instead compare
  // element by element using EXPECT_NEAR.
  if (scale_factor == 1.f) {
    EXPECT_TRUE(transform_child_to_child.IsIdentity());
  } else {
    const float kTolerance = 0.001f;
    const int kDim = 4;
    for (int row = 0; row < kDim; ++row) {
      for (int col = 0; col < kDim; ++col) {
        EXPECT_NEAR(row == col ? 1.f : 0.f,
                    transform_child_to_child.rc(row, col), kTolerance);
      }
    }
  }

  gfx::Transform transform_root_to_root;
  ASSERT_TRUE(root_rwhv->GetTransformToViewCoordSpace(root_rwhv,
                                                      &transform_root_to_root));
  EXPECT_TRUE(transform_root_to_root.IsIdentity());

  // Select two points inside child, one for the touch start and a different
  // one for a touch move.
  gfx::PointF touch_start_point_in_child(6, 6);
  gfx::PointF touch_move_point_in_child(10, 10);

  gfx::PointF touch_start_point =
      child_rwhv->TransformPointToRootCoordSpaceF(touch_start_point_in_child);
  gfx::PointF touch_move_point =
      child_rwhv->TransformPointToRootCoordSpaceF(touch_move_point_in_child);

  // Install InputEventObserver on child, and collect the three events.
  TestInputEventObserver child_event_observer(
      child_rwhv->GetRenderWidgetHost());
  InputEventAckWaiter child_touch_start_waiter(
      child_rwhv->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kTouchStart);
  InputEventAckWaiter child_touch_move_waiter(
      child_rwhv->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kTouchMove);
  InputEventAckWaiter child_touch_end_waiter(
      child_rwhv->GetRenderWidgetHost(), blink::WebInputEvent::Type::kTouchEnd);

  // Send events and verify each one was sent to the child with correctly
  // transformed event coordinates.
  auto* router = web_contents()->GetInputEventRouter();
  const float kCoordinateTolerance = 0.1f;

  // TouchStart.
  blink::WebTouchEvent touch_start_event(
      blink::WebInputEvent::Type::kTouchStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_start_event.touches_length = 1;
  touch_start_event.touches[0].state =
      blink::WebTouchPoint::State::kStatePressed;
  SetWebEventPositions(&touch_start_event.touches[0], touch_start_point,
                       root_rwhv);
  touch_start_event.unique_touch_event_id = 1;
  router->RouteTouchEvent(root_rwhv, &touch_start_event, ui::LatencyInfo());
  child_touch_start_waiter.Wait();

  ASSERT_EQ(1U, child_event_observer.events_received().size());
  ASSERT_EQ(blink::WebInputEvent::Type::kTouchStart,
            child_event_observer.event().GetType());
  const blink::WebTouchEvent& touch_start_event_received =
      static_cast<const blink::WebTouchEvent&>(child_event_observer.event());
  EXPECT_NEAR(touch_start_point_in_child.x(),
              touch_start_event_received.touches[0].PositionInWidget().x(),
              kCoordinateTolerance);
  EXPECT_NEAR(touch_start_point_in_child.y(),
              touch_start_event_received.touches[0].PositionInWidget().y(),
              kCoordinateTolerance);

  // TouchMove.
  blink::WebTouchEvent touch_move_event(
      blink::WebInputEvent::Type::kTouchMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_move_event.touches_length = 1;
  touch_move_event.touches[0].state = blink::WebTouchPoint::State::kStateMoved;
  SetWebEventPositions(&touch_move_event.touches[0], touch_move_point,
                       root_rwhv);
  touch_move_event.unique_touch_event_id = 2;
  router->RouteTouchEvent(root_rwhv, &touch_move_event, ui::LatencyInfo());
  child_touch_move_waiter.Wait();

  ASSERT_EQ(2U, child_event_observer.events_received().size());
  ASSERT_EQ(blink::WebInputEvent::Type::kTouchMove,
            child_event_observer.event().GetType());
  const blink::WebTouchEvent& touch_move_event_received =
      static_cast<const blink::WebTouchEvent&>(child_event_observer.event());
  EXPECT_NEAR(touch_move_point_in_child.x(),
              touch_move_event_received.touches[0].PositionInWidget().x(),
              kCoordinateTolerance);
  EXPECT_NEAR(touch_move_point_in_child.y(),
              touch_move_event_received.touches[0].PositionInWidget().y(),
              kCoordinateTolerance);

  // TouchEnd.
  blink::WebTouchEvent touch_end_event(
      blink::WebInputEvent::Type::kTouchEnd, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_end_event.touches_length = 1;
  touch_end_event.touches[0].state =
      blink::WebTouchPoint::State::kStateReleased;
  SetWebEventPositions(&touch_end_event.touches[0], touch_move_point,
                       root_rwhv);
  touch_end_event.unique_touch_event_id = 3;
  router->RouteTouchEvent(root_rwhv, &touch_end_event, ui::LatencyInfo());
  child_touch_end_waiter.Wait();

  ASSERT_EQ(3U, child_event_observer.events_received().size());
  ASSERT_EQ(blink::WebInputEvent::Type::kTouchEnd,
            child_event_observer.event().GetType());
  const blink::WebTouchEvent& touch_end_event_received =
      static_cast<const blink::WebTouchEvent&>(child_event_observer.event());
  EXPECT_NEAR(touch_move_point_in_child.x(),
              touch_end_event_received.touches[0].PositionInWidget().x(),
              kCoordinateTolerance);
  EXPECT_NEAR(touch_move_point_in_child.y(),
              touch_end_event_received.touches[0].PositionInWidget().y(),
              kCoordinateTolerance);
}

// When a scroll event is bubbled, ensure that the bubbled event's coordinates
// are correctly updated to the ancestor's coordinate space. In particular,
// ensure that the transformation considers CSS scaling of the child where
// simply applying the ancestor's offset does not produce the correct
// coordinates in the ancestor's coordinate space.
// See https://crbug.com/817392
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       BubbledScrollEventsTransformedCorrectly) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_scaled_frame.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, iframe_node->current_url());

  RenderWidgetHostViewBase* root_rwhv = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  input::RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();

  WaitForHitTestData(iframe_node->current_frame_host());

  const float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;
  // Due to the CSS scaling of the iframe, the position in the child view's
  // coordinates is (96, 96) and not (48, 48) (or approximately these values
  // if there's rounding due to the scale factor).
  const gfx::Point position_in_root(base::ClampCeil(150 * scale_factor),
                                    base::ClampCeil(150 * scale_factor));

  auto expect_gsb_with_position =
      base::BindRepeating([](const gfx::Point& expected_position,
                             blink::mojom::InputEventResultSource,
                             blink::mojom::InputEventResultState,
                             const blink::WebInputEvent& event) {
        if (event.GetType() != blink::WebInputEvent::Type::kGestureScrollBegin)
          return false;

        const blink::WebGestureEvent& gesture_event =
            static_cast<const blink::WebGestureEvent&>(event);
        EXPECT_NEAR(expected_position.x(), gesture_event.PositionInWidget().x(),
                    kHitTestTolerance);
        EXPECT_NEAR(expected_position.y(), gesture_event.PositionInWidget().y(),
                    kHitTestTolerance);
        return true;
      });

  InputEventAckWaiter root_scroll_begin_observer(
      root_rwhv->GetRenderWidgetHost(),
      base::BindRepeating(expect_gsb_with_position, position_in_root));

  // Scroll the iframe upward, scroll events get bubbled up to the root.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&scroll_event, position_in_root, root_rwhv);
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = 5.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;

  router->RouteMouseWheelEvent(root_rwhv, &scroll_event, ui::LatencyInfo());

  root_scroll_begin_observer.Wait();
}

namespace {

// Waits until an event of the given type has been sent to the given
// RenderWidgetHost.
class OutgoingEventWaiter : public RenderWidgetHost::InputEventObserver {
 public:
  explicit OutgoingEventWaiter(RenderWidgetHostImpl* rwh,
                               blink::WebInputEvent::Type type)
      : rwh_(rwh->GetWeakPtr()), type_(type) {
    rwh->AddInputEventObserver(this);
  }

  ~OutgoingEventWaiter() override {
    if (rwh_)
      rwh_->RemoveInputEventObserver(this);
  }

  void OnInputEvent(const blink::WebInputEvent& event) override {
    if (event.GetType() == type_) {
      seen_event_ = true;
      if (quit_closure_)
        std::move(quit_closure_).Run();
    }
  }

  void Wait() {
    if (!seen_event_) {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

 private:
  base::WeakPtr<RenderWidgetHostImpl> rwh_;
  const blink::WebInputEvent::Type type_;
  bool seen_event_ = false;
  base::OnceClosure quit_closure_;
};

// Fails the test if an event of the given type is sent to the given
// RenderWidgetHost.
class BadInputEventObserver : public RenderWidgetHost::InputEventObserver {
 public:
  explicit BadInputEventObserver(RenderWidgetHostImpl* rwh,
                                 blink::WebInputEvent::Type type)
      : rwh_(rwh->GetWeakPtr()), type_(type) {
    rwh->AddInputEventObserver(this);
  }

  ~BadInputEventObserver() override {
    if (rwh_)
      rwh_->RemoveInputEventObserver(this);
  }

  void OnInputEvent(const blink::WebInputEvent& event) override {
    EXPECT_NE(type_, event.GetType())
        << "Unexpected " << blink::WebInputEvent::GetName(event.GetType());
  }

 private:
  base::WeakPtr<RenderWidgetHostImpl> rwh_;
  const blink::WebInputEvent::Type type_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       ScrollBubblingTargetWithUnrelatedGesture) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* parent_iframe_node = root->child_at(0);
  ASSERT_EQ(1U, parent_iframe_node->child_count());

  GURL nested_frame_url(embedded_test_server()->GetURL(
      "baz.com", "/page_with_touch_start_janking_main_thread.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(parent_iframe_node->child_at(0),
                                        nested_frame_url));

  RenderWidgetHostViewBase* root_rwhv = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewChildFrame* rwhv_parent =
      static_cast<RenderWidgetHostViewChildFrame*>(
          parent_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());
  RenderWidgetHostViewChildFrame* rwhv_nested =
      static_cast<RenderWidgetHostViewChildFrame*>(
          parent_iframe_node->child_at(0)
              ->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  input::RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();

  WaitForHitTestData(parent_iframe_node->child_at(0)->current_frame_host());

  OutgoingEventWaiter outgoing_touch_end_waiter(
      static_cast<RenderWidgetHostImpl*>(rwhv_nested->GetRenderWidgetHost()),
      blink::WebInputEvent::Type::kTouchEnd);
  InputEventAckWaiter scroll_end_at_parent(
      rwhv_parent->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollEnd);
  BadInputEventObserver no_scroll_bubbling_to_root(
      static_cast<RenderWidgetHostImpl*>(root_rwhv->GetRenderWidgetHost()),
      blink::WebInputEvent::Type::kGestureScrollBegin);

  MainThreadFrameObserver synchronize_threads(
      rwhv_nested->GetRenderWidgetHost());
  synchronize_threads.Wait();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = content::mojom::GestureSourceType::kTouchInput;
  const gfx::PointF location_in_widget(25, 25);
  const gfx::PointF location_in_root =
      rwhv_nested->TransformPointToRootCoordSpaceF(location_in_widget);
  params.anchor = location_in_root;
  params.distances.push_back(gfx::Vector2d(0, 100));
  params.prevent_fling = false;
  RenderWidgetHostImpl* root_widget_host =
      static_cast<RenderWidgetHostImpl*>(root_rwhv->GetRenderWidgetHost());
  auto dont_care_on_complete = base::DoNothing();
  root_widget_host->QueueSyntheticGesture(
      std::make_unique<SyntheticSmoothScrollGesture>(params),
      std::move(dont_care_on_complete));

  outgoing_touch_end_waiter.Wait();

  // We are now waiting for the touch events to be acked from the nested OOPIF
  // which will result in a scroll gesture that will bubble from the nested
  // frame. Meanwhile, we start a new gesture in the main frame.

  const gfx::PointF point_in_root(1, 1);
  blink::WebTouchEvent touch_event(
      blink::WebInputEvent::Type::kTouchStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_event.touches_length = 1;
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStatePressed;
  SetWebEventPositions(&touch_event.touches[0], point_in_root, root_rwhv);
  touch_event.unique_touch_event_id = 1;
  InputEventAckWaiter root_touch_waiter(
      root_rwhv->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kTouchStart);
  router->RouteTouchEvent(root_rwhv, &touch_event, ui::LatencyInfo());
  root_touch_waiter.Wait();

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  router->RouteGestureEvent(root_rwhv, &gesture_event, ui::LatencyInfo());

  scroll_end_at_parent.Wait();
  // By this point, the parent frame attempted to bubble scroll to the main
  // frame. |no_scroll_bubbling_to_root| checks that the bubbling stopped at
  // the parent.
}

class SitePerProcessEmulatedTouchBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  enum TestType {
    ScrollBubbling,
    PinchGoesToMainFrame,
    TouchActionBubbling,
    ShowPressHasTouchID
  };

  ~SitePerProcessEmulatedTouchBrowserTest() override {}

  void RunTest(TestType test_type) {
    std::string url;
    if (test_type == TouchActionBubbling)
      url = "/frame_tree/page_with_pany_frame.html";
    else
      url = "/frame_tree/page_with_positioned_frame.html";
    GURL main_url(embedded_test_server()->GetURL(url));
    ASSERT_TRUE(NavigateToURL(shell(), main_url));

    // It is safe to obtain the root frame tree node here, as it doesn't change.
    FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
    ASSERT_EQ(1U, root->child_count());

    FrameTreeNode* iframe_node = root->child_at(0);
    GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
    EXPECT_EQ(site_url, iframe_node->current_url());

    RenderWidgetHostViewBase* root_rwhv =
        static_cast<RenderWidgetHostViewBase*>(
            root->current_frame_host()->GetRenderWidgetHost()->GetView());
    RenderWidgetHostViewBase* child_rwhv =
        static_cast<RenderWidgetHostViewBase*>(iframe_node->current_frame_host()
                                                   ->GetRenderWidgetHost()
                                                   ->GetView());

    input::RenderWidgetHostInputEventRouter* router =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetInputEventRouter();

    WaitForHitTestData(iframe_node->current_frame_host());

    auto expect_gesture_with_position =
        base::BindRepeating([](blink::WebInputEvent::Type expected_type,
                               const gfx::Point& expected_position,
                               blink::mojom::InputEventResultSource,
                               blink::mojom::InputEventResultState,
                               const blink::WebInputEvent& event) {
          if (event.GetType() != expected_type)
            return false;

          const blink::WebGestureEvent& gesture_event =
              static_cast<const blink::WebGestureEvent&>(event);
          EXPECT_NEAR(expected_position.x(),
                      gesture_event.PositionInWidget().x(), kHitTestTolerance);
          EXPECT_NEAR(expected_position.y(),
                      gesture_event.PositionInWidget().y(), kHitTestTolerance);
          EXPECT_EQ(blink::WebGestureDevice::kTouchscreen,
                    gesture_event.SourceDevice());
          // We expect all gesture events to have non-zero ids otherwise they
          // can force hit-testing in RenderWidgetHostInputEventRouter even
          // when it's unnecessary.
          EXPECT_NE(0U, gesture_event.unique_touch_event_id);
          return true;
        });

    blink::WebInputEvent::Type expected_gesture_type;
    switch (test_type) {
      case ScrollBubbling:
      case TouchActionBubbling:
        expected_gesture_type = blink::WebInputEvent::Type::kGestureScrollBegin;
        break;
      case PinchGoesToMainFrame:
        expected_gesture_type = blink::WebInputEvent::Type::kGesturePinchBegin;
        break;
      case ShowPressHasTouchID:
        expected_gesture_type = blink::WebInputEvent::Type::kGestureShowPress;
        break;
      default:
        ASSERT_TRUE(false);
    }

#if BUILDFLAG(IS_WIN)
    {
      gfx::Rect view_bounds = root_rwhv->GetViewBounds();
      LOG(ERROR) << "Root view bounds = (" << view_bounds.x() << ","
                 << view_bounds.y() << ") " << view_bounds.width() << " x "
                 << view_bounds.height();
    }
#endif

    gfx::Point position_in_child(5, 5);
    InputEventAckWaiter child_gesture_event_observer(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(expect_gesture_with_position, expected_gesture_type,
                            position_in_child));

    gfx::Point position_in_root =
        child_rwhv->TransformPointToRootCoordSpace(position_in_child);
    InputEventAckWaiter root_gesture_event_observer(
        root_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(expect_gesture_with_position, expected_gesture_type,
                            position_in_root));

    // Enable touch emulation.
    auto* touch_emulator =
        root_rwhv->host()->GetTouchEmulator(/*create_if_necessary=*/true);
    ASSERT_TRUE(touch_emulator);
    touch_emulator->Enable(input::TouchEmulator::Mode::kEmulatingTouchFromMouse,
                           ui::GestureProviderConfigType::CURRENT_PLATFORM);

    // Create mouse events to emulate touch scroll. Since the page has no touch
    // handlers, these events will be converted into a gesture scroll sequence.
    blink::WebMouseEvent mouse_move_event =
        blink::SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::Type::kMouseMove, position_in_root.x(),
            position_in_root.y(), 0);
    mouse_move_event.SetTimeStamp(ui::EventTimeForNow());

    int mouse_modifier = (test_type == PinchGoesToMainFrame)
                             ? blink::WebInputEvent::kShiftKey
                             : 0;
    mouse_modifier |= blink::WebInputEvent::kLeftButtonDown;
    blink::WebMouseEvent mouse_down_event =
        blink::SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::Type::kMouseDown, position_in_root.x(),
            position_in_root.y(), mouse_modifier);
    mouse_down_event.button = blink::WebMouseEvent::Button::kLeft;
    mouse_down_event.SetTimeStamp(ui::EventTimeForNow());

    blink::WebMouseEvent mouse_drag_event =
        blink::SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::Type::kMouseMove, position_in_root.x(),
            position_in_root.y() + 20, mouse_modifier);
    mouse_drag_event.SetTimeStamp(ui::EventTimeForNow());
    mouse_drag_event.button = blink::WebMouseEvent::Button::kLeft;

    blink::WebMouseEvent mouse_up_event =
        blink::SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::Type::kMouseUp, position_in_root.x(),
            position_in_root.y() + 20, mouse_modifier);
    mouse_up_event.button = blink::WebMouseEvent::Button::kLeft;
    mouse_up_event.SetTimeStamp(ui::EventTimeForNow());

    // Send mouse events and wait for GesturePinchBegin.
    router->RouteMouseEvent(root_rwhv, &mouse_move_event, ui::LatencyInfo());
    router->RouteMouseEvent(root_rwhv, &mouse_down_event, ui::LatencyInfo());
    if (test_type == ShowPressHasTouchID) {
      // Wait for child to receive GestureShowPress. If this test fails, it
      // will either DCHECK or time out.
      child_gesture_event_observer.Wait();
      return;
    }
    router->RouteMouseEvent(root_rwhv, &mouse_drag_event, ui::LatencyInfo());
    router->RouteMouseEvent(root_rwhv, &mouse_up_event, ui::LatencyInfo());

    if (test_type == ScrollBubbling || test_type == TouchActionBubbling) {
      // Verify child receives GestureScrollBegin.
      child_gesture_event_observer.Wait();
    }

    // Verify the root receives the GesturePinchBegin or GestureScrollBegin,
    // depending on |test_type|.
    root_gesture_event_observer.Wait();

    // Wait for all remaining input events to be processed by root_rwhv
    RunUntilInputProcessed(root_rwhv->GetRenderWidgetHost());

    // Shut down.
    touch_emulator->Disable();
  }
};

IN_PROC_BROWSER_TEST_F(SitePerProcessEmulatedTouchBrowserTest,
                       EmulatedTouchShowPressHasTouchID) {
  RunTest(ShowPressHasTouchID);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessEmulatedTouchBrowserTest,
                       EmulatedTouchScrollBubbles) {
  RunTest(ScrollBubbling);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessEmulatedTouchBrowserTest,
                       EmulatedTouchPinchGoesToMainFrame) {
  RunTest(PinchGoesToMainFrame);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessEmulatedTouchBrowserTest,
                       EmulatedGestureScrollBubbles) {
  RunTest(TouchActionBubbling);
}

// Regression test for https://crbug.com/851644. The test passes as long as it
// doesn't crash.
// Touch action ack timeout is enabled on Android only.
#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       TouchActionAckTimeout) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_janky_frame.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  GURL frame_url(embedded_test_server()->GetURL(
      "baz.com", "/page_with_touch_start_janking_main_thread.html"));
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewChildFrame* rwhv_child =
      static_cast<RenderWidgetHostViewChildFrame*>(
          child_frame_host->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_frame_host);

  // Compute the point so that the gesture event can target the child frame.
  const gfx::Rect root_bounds = rwhv_root->GetViewBounds();
  const gfx::Rect child_bounds = rwhv_child->GetViewBounds();
  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());
  const float page_scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;
  const gfx::PointF point_in_child(
      (child_bounds.x() - root_bounds.x() + 25) * page_scale_factor,
      (child_bounds.y() - root_bounds.y() + 25) * page_scale_factor);

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = content::mojom::GestureSourceType::kTouchInput;
  params.anchor = gfx::PointF(point_in_child.x(), point_in_child.y());
  params.distances.push_back(gfx::Vector2dF(0, -10));
  // The JS jank from the "page_with_touch_start_janking_main_thread.html"
  // causes the touch ack timeout. Set the speed high so that the gesture can be
  // completed quickly and so does this test.
  params.speed_in_pixels_s = 100000;
  std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));

  InputEventAckWaiter ack_observer(
      child_frame_host->GetRenderWidgetHost(),
      base::BindRepeating([](blink::mojom::InputEventResultSource source,
                             blink::mojom::InputEventResultState state,
                             const blink::WebInputEvent& event) {
        return event.GetType() ==
               blink::WebGestureEvent::Type::kGestureScrollEnd;
      }));
  ack_observer.Reset();

  RenderWidgetHostImpl* render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture), base::BindOnce([](SyntheticGesture::Result result) {
        EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
      }));
  ack_observer.Wait();
}
#endif  // BUILDFLAG(IS_ANDROID)

#if defined(USE_AURA) || BUILDFLAG(IS_ANDROID)

// When unconsumed scrolls in a child bubble to the root and start an
// overscroll gesture, the subsequent gesture scroll update events should be
// consumed by the root. The child should not be able to scroll during the
// overscroll gesture.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       RootConsumesScrollDuringOverscrollGesture) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);

#if defined(USE_AURA)
  // The child must be horizontally scrollable.
  GURL child_url(embedded_test_server()->GetURL("b.com", "/wide_page.html"));
#elif BUILDFLAG(IS_ANDROID)
  // The child must be vertically scrollable.
  GURL child_url(embedded_test_server()->GetURL("b.com", "/tall_page.html"));
#endif
  EXPECT_TRUE(NavigateToURLFromRenderer(child_node, child_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewChildFrame* rwhv_child =
      static_cast<RenderWidgetHostViewChildFrame*>(
          child_node->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderFrameSubmissionObserver child_render_frame_submission_observer(
      child_node);

  WaitForHitTestData(child_node->current_frame_host());

  const cc::RenderFrameMetadata& last_root_metadata =
      render_frame_submission_observer.LastRenderFrameMetadata();
  const cc::RenderFrameMetadata& last_child_metadata =
      child_render_frame_submission_observer.LastRenderFrameMetadata();

  ASSERT_TRUE(last_root_metadata.is_scroll_offset_at_top);
  ASSERT_TRUE(last_child_metadata.is_scroll_offset_at_top);

  input::RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();

  {
    // Set up the RenderWidgetHostInputEventRouter to send the gesture stream
    // to the child.
    const gfx::Rect root_bounds = rwhv_root->GetViewBounds();
    const gfx::Rect child_bounds = rwhv_child->GetViewBounds();
    const float page_scale_factor =
        render_frame_submission_observer.LastRenderFrameMetadata()
            .page_scale_factor;
    const gfx::PointF point_in_root(
        (child_bounds.x() - root_bounds.x() + 10) * page_scale_factor,
        (child_bounds.y() - root_bounds.y() + 10) * page_scale_factor);

    blink::WebTouchEvent touch_event(
        blink::WebInputEvent::Type::kTouchStart,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    touch_event.touches_length = 1;
    touch_event.touches[0].state = blink::WebTouchPoint::State::kStatePressed;
    SetWebEventPositions(&touch_event.touches[0], point_in_root, rwhv_root);
    touch_event.unique_touch_event_id = 1;
    InputEventAckWaiter waiter(rwhv_child->GetRenderWidgetHost(),
                               blink::WebInputEvent::Type::kTouchStart);
    router->RouteTouchEvent(rwhv_root, &touch_event, ui::LatencyInfo());
    // With async hit testing, make sure the target for the initial TouchStart
    // is resolved before sending the rest of the stream.
    waiter.Wait();

    blink::WebGestureEvent gesture_event(
        blink::WebInputEvent::Type::kGestureTapDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchscreen);
    gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
    router->RouteGestureEvent(rwhv_root, &gesture_event, ui::LatencyInfo());
  }

#if defined(USE_AURA)
  RenderWidgetHostViewAura* rwhva =
      static_cast<RenderWidgetHostViewAura*>(rwhv_root);
  std::unique_ptr<MockOverscrollControllerDelegateAura>
      mock_overscroll_delegate =
          std::make_unique<MockOverscrollControllerDelegateAura>(rwhva);
  rwhva->overscroll_controller()->set_delegate(
      mock_overscroll_delegate->GetWeakPtr());
  MockOverscrollObserver* mock_overscroll_observer =
      mock_overscroll_delegate.get();
#elif BUILDFLAG(IS_ANDROID)
  RenderWidgetHostViewAndroid* rwhv_android =
      static_cast<RenderWidgetHostViewAndroid*>(rwhv_root);
  std::unique_ptr<MockOverscrollRefreshHandlerAndroid> mock_overscroll_handler =
      std::make_unique<MockOverscrollRefreshHandlerAndroid>();
  rwhv_android->SetOverscrollControllerForTesting(
      mock_overscroll_handler.get());
  MockOverscrollObserver* mock_overscroll_observer =
      mock_overscroll_handler.get();
#endif  // defined(USE_AURA)

  InputEventAckWaiter gesture_begin_observer_child(
      child_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollBegin);
  InputEventAckWaiter gesture_end_observer_child(
      child_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollEnd);

#if defined(USE_AURA)
  const float overscroll_threshold =
      OverscrollConfig::kStartTouchscreenThresholdDips;
#elif BUILDFLAG(IS_ANDROID)
  const float overscroll_threshold = 0.f;
#endif

  // First we need our scroll to initiate an overscroll gesture in the root
  // via unconsumed scrolls in the child.
  blink::WebGestureEvent gesture_scroll_begin(
      blink::WebGestureEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_scroll_begin.unique_touch_event_id = 1;
  gesture_scroll_begin.data.scroll_begin.delta_hint_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0.f;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = 0.f;
#if defined(USE_AURA)
  // For aura, we scroll horizontally to activate an overscroll navigation.
  gesture_scroll_begin.data.scroll_begin.delta_x_hint =
      overscroll_threshold + 1;
#elif BUILDFLAG(IS_ANDROID)
  // For android, we scroll vertically to activate pull-to-refresh.
  gesture_scroll_begin.data.scroll_begin.delta_y_hint =
      overscroll_threshold + 1;
#endif
  router->RouteGestureEvent(rwhv_root, &gesture_scroll_begin,
                            ui::LatencyInfo());

  // Make sure the child is indeed receiving the gesture stream.
  gesture_begin_observer_child.Wait();

  blink::WebGestureEvent gesture_scroll_update(
      blink::WebGestureEvent::Type::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_scroll_update.unique_touch_event_id = 1;
  gesture_scroll_update.data.scroll_update.delta_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  gesture_scroll_update.data.scroll_update.delta_x = 0.f;
  gesture_scroll_update.data.scroll_update.delta_y = 0.f;
#if defined(USE_AURA)
  float* delta = &gesture_scroll_update.data.scroll_update.delta_x;
#elif BUILDFLAG(IS_ANDROID)
  float* delta = &gesture_scroll_update.data.scroll_update.delta_y;
#endif
  *delta = overscroll_threshold + 1;
  mock_overscroll_observer->Reset();
  // This will bring us into an overscroll gesture.
  router->RouteGestureEvent(rwhv_root, &gesture_scroll_update,
                            ui::LatencyInfo());
  // Note that in addition to verifying that we get the overscroll update, it
  // is necessary to wait before sending the next event to prevent our multiple
  // GestureScrollUpdates from being coalesced.
  mock_overscroll_observer->WaitForUpdate();

  // This scroll is in the same direction and so it will contribute to the
  // overscroll.
  *delta = 10.0f;
  mock_overscroll_observer->Reset();
  router->RouteGestureEvent(rwhv_root, &gesture_scroll_update,
                            ui::LatencyInfo());
  mock_overscroll_observer->WaitForUpdate();

  // Now we reverse direction. The child could scroll in this direction, but
  // since we're in an overscroll gesture, the root should consume it.
  *delta = -5.0f;
  mock_overscroll_observer->Reset();
  router->RouteGestureEvent(rwhv_root, &gesture_scroll_update,
                            ui::LatencyInfo());
  mock_overscroll_observer->WaitForUpdate();

  blink::WebGestureEvent gesture_scroll_end(
      blink::WebGestureEvent::Type::kGestureScrollEnd,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_scroll_end.unique_touch_event_id = 1;
  gesture_scroll_end.data.scroll_end.delta_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  mock_overscroll_observer->Reset();
  router->RouteGestureEvent(rwhv_root, &gesture_scroll_end, ui::LatencyInfo());
  mock_overscroll_observer->WaitForEnd();

  // Ensure that the method of providing the child's scroll events to the root
  // does not leave the child in an invalid state.
  gesture_end_observer_child.Wait();
}
#endif  // defined(USE_AURA) || BUILDFLAG(IS_ANDROID)

// Test that an EventType::kScroll event sent to an out-of-process iframe
// correctly results in a scroll. This is only handled by
// RenderWidgetHostViewAura and is needed for trackpad scrolling on Chromebooks.
#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest, ScrollEventToOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewAura* rwhv_parent =
      static_cast<RenderWidgetHostViewAura*>(
          root->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  // Create listener for input events.
  TestInputEventObserver child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  // Send a ui::ScrollEvent that will hit test to the child frame.
  InputEventAckWaiter waiter(
      child_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kMouseWheel);
  ui::ScrollEvent scroll_event(ui::EventType::kScroll, gfx::Point(75, 75),
                               ui::EventTimeForNow(), ui::EF_NONE, 0,
                               10,     // Offsets
                               0, 10,  // Offset ordinals
                               2);
  UpdateEventRootLocation(&scroll_event, rwhv_parent);
  rwhv_parent->OnScrollEvent(&scroll_event);
  waiter.Wait();

  // Verify that this a mouse wheel event was sent to the child frame renderer.
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_TRUE(base::Contains(child_frame_monitor.events_received(),
                             blink::WebInputEvent::Type::kMouseWheel));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       InputEventRouterWheelCoalesceTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewAura* rwhv_parent =
      static_cast<RenderWidgetHostViewAura*>(
          root->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  // Create listener for input events.
  TestInputEventObserver child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());
  InputEventAckWaiter waiter(
      child_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kMouseWheel);

  // Send a mouse wheel event to child.
  blink::WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&wheel_event, gfx::Point(75, 75), rwhv_parent);
  wheel_event.delta_x = 10;
  wheel_event.delta_y = 20;
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  router->RouteMouseWheelEvent(rwhv_parent, &wheel_event, ui::LatencyInfo());

  // Send more mouse wheel events to the child. Since we are waiting for the
  // async targeting on the first event, these new mouse wheel events should
  // be coalesced properly.
  blink::WebMouseWheelEvent wheel_event1(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&wheel_event1, gfx::Point(70, 70), rwhv_parent);
  wheel_event1.delta_x = 12;
  wheel_event1.delta_y = 22;
  wheel_event1.phase = blink::WebMouseWheelEvent::kPhaseChanged;
  router->RouteMouseWheelEvent(rwhv_parent, &wheel_event1, ui::LatencyInfo());

  blink::WebMouseWheelEvent wheel_event2(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&wheel_event2, gfx::Point(65, 65), rwhv_parent);
  wheel_event2.delta_x = 14;
  wheel_event2.delta_y = 24;
  wheel_event2.phase = blink::WebMouseWheelEvent::kPhaseChanged;
  router->RouteMouseWheelEvent(rwhv_parent, &wheel_event2, ui::LatencyInfo());

  // Since we are targeting child, event dispatch should not happen
  // synchronously. Validate that the expected target does not receive the
  // event immediately.
  waiter.Wait();
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_EQ(child_frame_monitor.EventType(),
            blink::WebInputEvent::Type::kMouseWheel);

  // Check if the two mouse-wheel update events are coalesced correctly.
  const auto& gesture_event =
      static_cast<const blink::WebGestureEvent&>(child_frame_monitor.event());
  EXPECT_EQ(26 /* wheel_event1.delta_x + wheel_event2.delta_x */,
            gesture_event.data.scroll_update.delta_x);
  EXPECT_EQ(46 /* wheel_event1.delta_y + wheel_event2.delta_y */,
            gesture_event.data.scroll_update.delta_y);
}
#endif  // defined(USE_AURA)

// Test that mouse events are being routed to the correct RenderWidgetHostView
// based on coordinates.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest, SurfaceHitTestTest) {
  SurfaceHitTestTestHelper(shell(), embedded_test_server());
}

// Same test as above, but runs in high-dpi mode.
#if BUILDFLAG(IS_ANDROID)
// High DPI browser tests are not needed on Android, and confuse some of the
// coordinate calculations. Android uses fixed device scale factor.
#define MAYBE_SurfaceHitTestTest DISABLED_SurfaceHitTestTest
#else
#define MAYBE_SurfaceHitTestTest SurfaceHitTestTest
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIHitTestBrowserTest,
                       MAYBE_SurfaceHitTestTest) {
  SurfaceHitTestTestHelper(shell(), embedded_test_server());
}

// Test that mouse events are being routed to the correct RenderWidgetHostView
// when there are nested out-of-process iframes.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       NestedSurfaceHitTestTest) {
  NestedSurfaceHitTestTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIHitTestBrowserTest,
                       NestedSurfaceHitTestTest) {
  NestedSurfaceHitTestTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       NonFlatTransformedSurfaceHitTestTest) {
  NonFlatTransformedSurfaceHitTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIHitTestBrowserTest,
                       NonFlatTransformedSurfaceHitTestTest) {
  NonFlatTransformedSurfaceHitTestHelper(shell(), embedded_test_server());
}

// TODO(kenrb): Running this test on Android bots has slight discrepancies in
// transformed event coordinates when we do manual calculation of expected
// values. We can't rely on browser side transformation because it is broken
// for perspective transforms. See https://crbug.com/854247.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PerspectiveTransformedSurfaceHitTestTest \
  DISABLED_PerspectiveTransformedSurfaceHitTestTest
#else
#define MAYBE_PerspectiveTransformedSurfaceHitTestTest \
  PerspectiveTransformedSurfaceHitTestTest
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       MAYBE_PerspectiveTransformedSurfaceHitTestTest) {
  PerspectiveTransformedSurfaceHitTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIHitTestBrowserTest,
                       MAYBE_PerspectiveTransformedSurfaceHitTestTest) {
  PerspectiveTransformedSurfaceHitTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIHitTestBrowserTest,
                       OverlapSurfaceHitTestTest) {
  OverlapSurfaceHitTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       OverlapSurfaceHitTestTest) {
  OverlapSurfaceHitTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       HitTestLayerSquashing) {
  HitTestLayerSquashing(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIHitTestBrowserTest,
                       HitTestLayerSquashing) {
  HitTestLayerSquashing(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest, HitTestWatermark) {
  HitTestWatermark(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIHitTestBrowserTest,
                       HitTestWatermark) {
  HitTestWatermark(shell(), embedded_test_server());
}

#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest, RootWindowTransform) {
  HitTestRootWindowTransform(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIHitTestBrowserTest,
                       RootWindowTransform) {
  HitTestRootWindowTransform(shell(), embedded_test_server());
}
#endif  // defined(USE_AURA)

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       HitTestStaleDataDeletedView) {
  // Have two iframes to avoid going to short circuit path during the second
  // targeting.
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_iframes.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());

  FrameTreeNode* child_node1 = root->child_at(0);
  GURL site_url1(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  EXPECT_EQ(site_url1, child_node1->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node1->current_frame_host()->GetSiteInstance());

  FrameTreeNode* child_node2 = root->child_at(1);
  GURL site_url2(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url2, child_node2->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node2->current_frame_host()->GetSiteInstance());

  RenderWidgetHostImpl* root_rwh = static_cast<RenderWidgetHostImpl*>(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostViewBase* rwhv_parent =
      static_cast<RenderWidgetHostViewBase*>(root_rwh->GetView());
  RenderWidgetHostViewBase* rwhv_child2 =
      static_cast<RenderWidgetHostViewBase*>(
          child_node2->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node1->current_frame_host());
  WaitForHitTestData(child_node2->current_frame_host());

  const gfx::PointF child_location(50, 50);
  gfx::PointF parent_location =
      rwhv_child2->TransformPointToRootCoordSpaceF(child_location);
  // Send a mouse-down at the center of the child2. This should go to the
  // child2.
  DispatchMouseDownEventAndWaitUntilDispatch(
      web_contents, rwhv_parent, parent_location, rwhv_child2, child_location);

  // Remove the iframe from the page. Add an infinite loop at the end so that
  // renderer wouldn't submit updated hit-test data.
  FrameDeletedObserver delete_observer(child_node2->current_frame_host());
  ExecuteScriptAsync(
      root,
      "document.body.removeChild(document.getElementsByName('frame2')[0]);"
      "while(true) {}");
  delete_observer.Wait();
  EXPECT_EQ(1U, root->child_count());

  // The synchronous targeting for the same location should now find the
  // root-view as the target (and require async-targeting), since child2 has
  // been removed. We cannot actually attempt to dispatch the event though,
  // since it would try to do asynchronous targeting by asking the root-view,
  // whose main-thread is blocked because of the infinite-loop in the injected
  // javascript above.
  blink::WebMouseEvent down_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  down_event.button = blink::WebPointerProperties::Button::kLeft;
  down_event.click_count = 1;
  SetWebEventPositions(&down_event, parent_location, rwhv_parent);
  auto result = web_contents->GetInputEventRouter()->FindTargetSynchronously(
      rwhv_parent, down_event);
  EXPECT_EQ(result.view, rwhv_parent);
  // There is only one child frame, we can find the target frame and are sure
  // there are no other possible targets, in this case, we dispatch the event
  // immediately without asynchronously querying the root-view.
  EXPECT_FALSE(result.should_query_view);
  EXPECT_EQ(result.target_location.value(), parent_location);
}

// This test tests that browser process hittesting ignores frames with
// pointer-events: none.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       SurfaceHitTestPointerEventsNoneChanged) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame_pointer-events_none.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());

  FrameTreeNode* child_node1 = root->child_at(0);
  FrameTreeNode* child_node2 = root->child_at(1);

  GURL site_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node2->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node2->current_frame_host()->GetSiteInstance());

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node1->current_frame_host()->GetRenderWidgetHost());

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  // This is to make sure that the hit_test_data is clean before running the
  // hit_test_data_change_observer below.
  WaitForHitTestData(child_node1->current_frame_host());
  WaitForHitTestData(child_node2->current_frame_host());

  // Target input event to child1 frame.
  blink::WebMouseEvent child_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  child_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&child_event, gfx::Point(75, 75), root_view);
  child_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  InputEventAckWaiter waiter(root->current_frame_host()->GetRenderWidgetHost(),
                             blink::WebInputEvent::Type::kMouseDown);
  router->RouteMouseEvent(root_view, &child_event, ui::LatencyInfo());
  waiter.Wait();

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().x(),
              kHitTestTolerance);
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().y(),
              kHitTestTolerance);
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());

  HitTestRegionObserver hit_test_data_change_observer(
      root_view->GetRootFrameSinkId());
  hit_test_data_change_observer.WaitForHitTestData();

  // Remove pointer-events: none property from iframe to check that it can claim
  // the input event now.
  EXPECT_TRUE(ExecJs(web_contents(),
                     "setTimeout(function() {\n"
                     "  document.getElementsByTagName('iframe')[0].style."
                     "      pointerEvents = 'auto';\n"
                     "}, 100);"));
  ASSERT_EQ(2U, root->child_count());

  MainThreadFrameObserver observer(
      root->current_frame_host()->GetRenderWidgetHost());
  observer.Wait();

  hit_test_data_change_observer.WaitForHitTestDataChange();

  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  InputEventAckWaiter child_waiter(
      child_node1->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kMouseDown);
  router->RouteMouseEvent(root_view, &child_event, ui::LatencyInfo());
  child_waiter.Wait();

  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_NEAR(23, child_frame_monitor.event().PositionInWidget().x(),
              kHitTestTolerance);
  EXPECT_NEAR(23, child_frame_monitor.event().PositionInWidget().y(),
              kHitTestTolerance);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       PointerEventsNoneWithNestedSameOriginIFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_same_origin_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* child_node = root->child_at(0);
  FrameTreeNode* grandchild_node = child_node->child_at(0);

  // This is to make sure that the hit_test_data is clean before running the
  // hit_test_data_change_observer.
  WaitForHitTestData(child_node->current_frame_host());
  WaitForHitTestData(grandchild_node->current_frame_host());

  HitTestRegionObserver hit_test_data_change_observer(
      root_view->GetRootFrameSinkId());
  hit_test_data_change_observer.WaitForHitTestData();

  EXPECT_TRUE(ExecJs(web_contents(),
                     "document.getElementById('wrapper').style."
                     "pointerEvents = 'none';"));

  hit_test_data_change_observer.WaitForHitTestDataChange();

  MainThreadFrameObserver observer(
      root->current_frame_host()->GetRenderWidgetHost());
  observer.Wait();

  // ------------------------
  // root    50px
  //     ---------------------
  //     |child  50px        |
  // 50px|    -------------- |
  //     |50px| grand_child ||
  //     |    |             ||
  //     |    |-------------||
  //     ---------------------

  // DispatchMouseDownEventAndWaitUntilDispatch will make sure the mouse event
  // goes to the right frame. Create a listener for the grandchild to verify
  // that it does not receive the event. No need to create one for the child
  // because root and child are on the same process.
  RenderWidgetHostMouseEventMonitor grandchild_frame_monitor(
      grandchild_node->current_frame_host()->GetRenderWidgetHost());

  // Since child has pointer-events: none, (125, 125) should be claimed by root.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents(), root_view,
                                             gfx::PointF(125, 125), root_view,
                                             gfx::PointF(125, 125));
  EXPECT_FALSE(grandchild_frame_monitor.EventWasReceived());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       PointerEventsNoneWithNestedOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://a.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* child_node = root->child_at(0);
  FrameTreeNode* grandchild_node = child_node->child_at(0);

  // This is to make sure that the hit_test_data is clean before running the
  // hit_test_data_change_observer.
  WaitForHitTestData(child_node->current_frame_host());
  WaitForHitTestData(grandchild_node->current_frame_host());

  HitTestRegionObserver hit_test_data_change_observer(
      root_view->GetRootFrameSinkId());
  hit_test_data_change_observer.WaitForHitTestData();

  EXPECT_TRUE(ExecJs(web_contents(),
                     "document.getElementsByTagName('iframe')[0].style."
                     "pointerEvents = 'none';"));

  hit_test_data_change_observer.WaitForHitTestDataChange();

  MainThreadFrameObserver observer(
      root->current_frame_host()->GetRenderWidgetHost());
  observer.Wait();

  // ------------------------
  // root    50px
  //     ---------------------
  //     |child  50px        |
  // 50px|    -------------- |
  //     |50px| grand_child ||
  //     |    |             ||
  //     |    |-------------||
  //     ---------------------

  // DispatchMouseDownEventAndWaitUntilDispatch will make sure the mouse event
  // goes to the right frame. Create a listener for the child to verify that it
  // does not receive the event.
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  // Since child has pointer-events: none, (125, 125) should be claimed by root.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents(), root_view,
                                             gfx::PointF(125, 125), root_view,
                                             gfx::PointF(125, 125));
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
}

// This test tests that browser process can successfully hit test on nested
// OOPIFs that are partially occluded by main frame elements.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       HitTestNestedOccludedOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_nested_frames_and_occluding_div.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* parent = root->child_at(0);

  GURL site_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_EQ(site_url, parent->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            parent->current_frame_host()->GetSiteInstance());

  ASSERT_EQ(1U, parent->child_count());
  FrameTreeNode* child = parent->child_at(0);
  GURL child_site_url(
      embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(child_site_url, child->current_url());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child->current_frame_host());

  // Target input event to the overlapping region of main frame's div and child
  // frame.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents, root_view,
                                             gfx::PointF(75, 75), root_view,
                                             gfx::PointF(75, 75));

  // Target input event to the non overlapping region of child frame.
  // The div has a bound of (0, 0, 100, 100) with a border-radius of 5px, so
  // point (99, 99) should not hit test the div but reach the nested child
  // frame.
  // The parent frame and child frame both have a default offset of (2, 2) and
  // child frame's top and left properties are set to be (50, 50), so there is
  // an offset of (54, 54) in total.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents, root_view,
                                             gfx::PointF(99, 99), child_view,
                                             gfx::PointF(45, 45));
}

// Verify that an event is properly retargeted to the main frame when an
// asynchronous hit test to the child frame times out.
// TODO(crbug.com/40806028) Flaky on all platforms
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       DISABLED_AsynchronousHitTestChildTimeout) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_busy_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  WaitForHitTestData(child_node->current_frame_host());

  // Shorten the timeout for purposes of this test.
  router->GetRenderWidgetTargeterForTests()
      ->set_async_hit_test_timeout_delay_for_testing(base::TimeDelta());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_TRUE(ExecJs(child_node, "lookBusy();"));

  // Target input event to child frame. It should get delivered to the main
  // frame instead because the child frame main thread is non-responsive.
  blink::WebMouseEvent child_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  child_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&child_event, gfx::Point(75, 75), root_view);
  child_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &child_event);

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().x(),
              kHitTestTolerance);
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().y(),
              kHitTestTolerance);
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
}

// Verify that asynchronous hit test immediately handle
// when target client disconnects.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       AsynchronousHitTestChildDisconnectClient) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_busy_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  WaitForHitTestData(child_node->current_frame_host());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  // Target input event to child frame. It should get delivered to the main
  // frame instead because the child frame main thread is non-responsive.
  blink::WebMouseEvent child_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  child_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&child_event, gfx::Point(75, 75), root_view);
  child_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  {
    InputEventAckWaiter waiter(root_view->GetRenderWidgetHost(),
                               child_event.GetType());
    router->RouteMouseEvent(root_view, &child_event, ui::LatencyInfo());
    // Raise error for call disconnect handler.
    static_cast<RenderWidgetHostImpl*>(
        root->current_frame_host()->GetRenderWidgetHost())
        ->GetRenderInputRouter()
        ->input_target_client()
        .internal_state()
        ->RaiseError();
    waiter.Wait();
  }

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().x(),
              kHitTestTolerance);
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().y(),
              kHitTestTolerance);
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
}

// Tooltips aren't used on Android, so no need to compile/run this test in that
// case.
#if !BUILDFLAG(IS_ANDROID)
class TooltipMonitor : public RenderWidgetHostViewBase::TooltipObserver {
 public:
  explicit TooltipMonitor(RenderWidgetHostViewBase* rwhv)
      : run_loop_(new base::RunLoop) {
    DCHECK(rwhv);
    rwhv->SetTooltipObserverForTesting(this);
  }

  TooltipMonitor(const TooltipMonitor&) = delete;
  TooltipMonitor& operator=(const TooltipMonitor&) = delete;

  ~TooltipMonitor() override {}

  void Reset() {
    run_loop_ = std::make_unique<base::RunLoop>();
    tooltips_received_.clear();
  }

  void OnTooltipTextUpdated(const std::u16string& tooltip_text) override {
    tooltips_received_.push_back(tooltip_text);
    if (tooltip_text == tooltip_text_wanted_ && run_loop_->running())
      run_loop_->Quit();
  }

  void WaitUntil(const std::u16string& tooltip_text) {
    tooltip_text_wanted_ = tooltip_text;
    if (base::Contains(tooltips_received_, tooltip_text))
      return;
    run_loop_->Run();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  std::u16string tooltip_text_wanted_;
  std::vector<std::u16string> tooltips_received_;
};  // class TooltipMonitor

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       CrossProcessTooltipTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* b_node = root->child_at(0);

  RenderWidgetHostViewBase* rwhv_a = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_b = static_cast<RenderWidgetHostViewBase*>(
      b_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  TooltipMonitor tooltip_monitor(rwhv_a);

  WaitForHitTestData(b_node->current_frame_host());

  // Make sure the point_in_a_frame value is outside the default 8px margin
  // for the body element.
  gfx::Point point_in_a_frame(10, 10);
  gfx::Point point_in_b_frame =
      rwhv_b->TransformPointToRootCoordSpace(gfx::Point(25, 25));

  // Create listeners for mouse events. These are used to verify that the
  // RenderWidgetHostInputEventRouter is generating MouseLeave, etc for
  // the right renderers.
  RenderWidgetHostMouseEventMonitor a_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor b_frame_monitor(
      b_node->current_frame_host()->GetRenderWidgetHost());

  // Add tooltip text to both the body and the iframe in A.
  std::string script =
      "body = document.body.setAttribute('title', 'body_tooltip');\n"
      "iframe = document.getElementsByTagName('iframe')[0];\n"
      "iframe.setAttribute('title','iframe_for_b');";
  EXPECT_TRUE(ExecJs(root->current_frame_host(), script));

  // Send mouse events to both A and B.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  auto* router = web_contents()->GetInputEventRouter();

  // Alternate mouse moves between main frame and the cross-process iframe to
  // test that the tool tip in the iframe can override the one set by the main
  // frame renderer, even on a second entry into the iframe.
  gfx::Point current_point;
  for (int iteration = 0; iteration < 2; ++iteration) {
    // The following is a bit of a hack to prevent hitting the same
    // position/node check in ChromeClient::SetToolTip().
    current_point = point_in_a_frame;
    current_point.Offset(iteration, iteration);
    SetWebEventPositions(&mouse_event, current_point, rwhv_a);
    RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_a, &mouse_event);
    EXPECT_TRUE(a_frame_monitor.EventWasReceived());
    a_frame_monitor.ResetEventReceived();
    // B will receive a mouseLeave on all but the first iteration.
    EXPECT_EQ(iteration != 0, b_frame_monitor.EventWasReceived());
    b_frame_monitor.ResetEventReceived();

    tooltip_monitor.WaitUntil(u"body_tooltip");
    tooltip_monitor.Reset();

    // Next send a MouseMove to B frame, and A should receive a MouseMove event.
    current_point = point_in_b_frame;
    current_point.Offset(iteration, iteration);
    SetWebEventPositions(&mouse_event, current_point, rwhv_a);
    RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_b, &mouse_event);
    EXPECT_TRUE(a_frame_monitor.EventWasReceived());
    EXPECT_EQ(a_frame_monitor.event().GetType(),
              blink::WebInputEvent::Type::kMouseMove);
    a_frame_monitor.ResetEventReceived();
    EXPECT_TRUE(b_frame_monitor.EventWasReceived());
    b_frame_monitor.ResetEventReceived();
    tooltip_monitor.WaitUntil(std::u16string());
    tooltip_monitor.Reset();
  }

  rwhv_a->SetTooltipObserverForTesting(nullptr);
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// The following test ensures that we don't get a crash if a tooltip is
// triggered on Android. This test is nearly identical to
// SitePerProcessHitTestBrowserTest.CrossProcessTooltipTestAndroid, except
// it omits the tooltip monitor, and all dereferences of GetCursorManager().
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       CrossProcessTooltipTestAndroid) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* b_node = root->child_at(0);

  RenderWidgetHostViewBase* rwhv_a = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_b = static_cast<RenderWidgetHostViewBase*>(
      b_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // On Android we don't expect GetCursorManager() to return anything other
  // than nullptr. If it did, this test would be unnecessary.
  DCHECK(!rwhv_a->GetCursorManager());

  WaitForHitTestData(b_node->current_frame_host());

  // Make sure the point_in_a_frame value is outside the default 8px margin
  // for the body element.
  gfx::Point point_in_a_frame(10, 10);
  gfx::Point point_in_b_frame =
      rwhv_b->TransformPointToRootCoordSpace(gfx::Point(25, 25));

  // Create listeners for mouse events. These are used to verify that the
  // RenderWidgetHostInputEventRouter is generating MouseLeave, etc for
  // the right renderers.
  RenderWidgetHostMouseEventMonitor a_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor b_frame_monitor(
      b_node->current_frame_host()->GetRenderWidgetHost());

  // Add tooltip text to both the body and the iframe in A.
  std::string script_a =
      "body = document.body.setAttribute('title', 'body_a_tooltip');\n"
      "iframe = document.getElementsByTagName('iframe')[0];\n"
      "iframe.setAttribute('title','iframe_for_b');";
  EXPECT_TRUE(ExecJs(root->current_frame_host(), script_a));
  std::string script_b =
      "body = document.body.setAttribute('title', 'body_b_tooltip');";
  EXPECT_TRUE(ExecJs(b_node->current_frame_host(), script_b));

  // Send mouse events to both A and B.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  auto* router = web_contents()->GetInputEventRouter();

  // Alternate mouse moves between main frame and the cross-process iframe to
  // test that the tool tip in the iframe can override the one set by the main
  // frame renderer, even on a second entry into the iframe.
  gfx::Point current_point;
  for (int iteration = 0; iteration < 2; ++iteration) {
    // The following is a bit of a hack to prevent hitting the same
    // position/node check in ChromeClient::SetToolTip().
    current_point = point_in_a_frame;
    current_point.Offset(iteration, iteration);
    SetWebEventPositions(&mouse_event, current_point, rwhv_a);
    RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_a, &mouse_event);
    EXPECT_TRUE(a_frame_monitor.EventWasReceived());
    a_frame_monitor.ResetEventReceived();
    // B will receive a mouseLeave on all but the first iteration.
    EXPECT_EQ(iteration != 0, b_frame_monitor.EventWasReceived());
    b_frame_monitor.ResetEventReceived();

    // Next send a MouseMove to B frame, and A should receive a MouseMove event.
    current_point = point_in_b_frame;
    current_point.Offset(iteration, iteration);
    SetWebEventPositions(&mouse_event, current_point, rwhv_a);
    RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_b, &mouse_event);
    EXPECT_TRUE(a_frame_monitor.EventWasReceived());
    EXPECT_EQ(a_frame_monitor.event().GetType(),
              blink::WebInputEvent::Type::kMouseMove);
    a_frame_monitor.ResetEventReceived();
    EXPECT_TRUE(b_frame_monitor.EventWasReceived());
    b_frame_monitor.ResetEventReceived();
  }

  // This is an (arbitrary) delay to allow the test to crash if it's going to.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_max_timeout());
  run_loop.Run();
}
#endif  // BUILDFLAG(IS_ANDROID)

// This test verifies that MouseEnter and MouseLeave events fire correctly
// when the mouse cursor moves between processes.
// Flaky (timeout): https://crbug.com/1006635, crbug.com/334105909.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_CrossProcessMouseEnterAndLeaveTest \
  DISABLED_CrossProcessMouseEnterAndLeaveTest
#else
#define MAYBE_CrossProcessMouseEnterAndLeaveTest \
  CrossProcessMouseEnterAndLeaveTest
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       MAYBE_CrossProcessMouseEnterAndLeaveTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c(d))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site B ------- proxies for A C D\n"
      "   +--Site C ------- proxies for A B D\n"
      "        +--Site D -- proxies for A B C\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/\n"
      "      D = http://d.com/",
      DepictFrameTree(root));

  FrameTreeNode* b_node = root->child_at(0);
  FrameTreeNode* c_node = root->child_at(1);
  FrameTreeNode* d_node = c_node->child_at(0);

  RenderWidgetHostViewBase* rwhv_a = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_b = static_cast<RenderWidgetHostViewBase*>(
      b_node->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_d = static_cast<RenderWidgetHostViewBase*>(
      d_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // Verifying surfaces are ready in B and D are sufficient, since other
  // surfaces contain at least one of them.
  WaitForHitTestData(b_node->current_frame_host());
  WaitForHitTestData(d_node->current_frame_host());

  // Create listeners for mouse events. These are used to verify that the
  // RenderWidgetHostInputEventRouter is generating MouseLeave, etc for
  // the right renderers.
  RenderWidgetHostMouseEventMonitor root_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor a_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor b_frame_monitor(
      b_node->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor c_frame_monitor(
      c_node->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor d_frame_monitor(
      d_node->current_frame_host()->GetRenderWidgetHost());

  float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;

  // Get the view bounds of the child iframe, which should account for the
  // relative offset of its direct parent within the root frame, for use in
  // targeting the input event.
  gfx::Rect a_bounds = rwhv_a->GetViewBounds();
  gfx::Rect b_bounds = rwhv_b->GetViewBounds();
  gfx::Rect d_bounds = rwhv_d->GetViewBounds();

  gfx::Point point_in_a_frame(2, 2);
  gfx::Point point_in_b_frame(
      base::ClampCeil((b_bounds.x() - a_bounds.x() + 25) * scale_factor),
      base::ClampCeil((b_bounds.y() - a_bounds.y() + 25) * scale_factor));
  gfx::Point point_in_d_frame(
      base::ClampCeil((d_bounds.x() - a_bounds.x() + 25) * scale_factor),
      base::ClampCeil((d_bounds.y() - a_bounds.y() + 25) * scale_factor));

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&mouse_event, point_in_a_frame, rwhv_a);

  auto* router = web_contents()->GetInputEventRouter();

  // Send an initial MouseMove to the root view, which shouldn't affect the
  // other renderers.
  RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_a, &mouse_event);
  EXPECT_TRUE(a_frame_monitor.EventWasReceived());
  a_frame_monitor.ResetEventReceived();
  EXPECT_FALSE(b_frame_monitor.EventWasReceived());
  EXPECT_FALSE(c_frame_monitor.EventWasReceived());
  EXPECT_FALSE(d_frame_monitor.EventWasReceived());

  // Next send a MouseMove to B frame, which shouldn't affect C or D but
  // A should receive a MouseMove event.
  SetWebEventPositions(&mouse_event, point_in_b_frame, rwhv_a);
  RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_b, &mouse_event);
  EXPECT_TRUE(a_frame_monitor.EventWasReceived());
  EXPECT_EQ(a_frame_monitor.event().GetType(),
            blink::WebInputEvent::Type::kMouseMove);
  a_frame_monitor.ResetEventReceived();
  EXPECT_TRUE(b_frame_monitor.EventWasReceived());
  b_frame_monitor.ResetEventReceived();
  EXPECT_FALSE(c_frame_monitor.EventWasReceived());
  EXPECT_FALSE(d_frame_monitor.EventWasReceived());

  // Next send a MouseMove to D frame, which should have side effects in every
  // other RenderWidgetHostView.
  SetWebEventPositions(&mouse_event, point_in_d_frame, rwhv_a);
  RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_d, &mouse_event);
  EXPECT_TRUE(a_frame_monitor.EventWasReceived());
  EXPECT_EQ(a_frame_monitor.event().GetType(),
            blink::WebInputEvent::Type::kMouseMove);
  EXPECT_TRUE(b_frame_monitor.EventWasReceived());
  EXPECT_EQ(b_frame_monitor.event().GetType(),
            blink::WebInputEvent::Type::kMouseLeave);
  EXPECT_TRUE(c_frame_monitor.EventWasReceived());
  EXPECT_EQ(c_frame_monitor.event().GetType(),
            blink::WebInputEvent::Type::kMouseMove);
  EXPECT_TRUE(d_frame_monitor.EventWasReceived());
}

// Verify that when mouse capture is released after dragging to a cross-process
// frame, a special MouseMove is sent to the new frame to cause the cursor
// to update.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       CrossProcessMouseMoveAfterCaptureRelease) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();
  scoped_refptr<SetMouseCaptureInterceptor> child_interceptor =
      new SetMouseCaptureInterceptor(static_cast<RenderWidgetHostImpl*>(
          child_node->current_frame_host()->GetRenderWidgetHost()));

  // Send MouseDown to child frame to initiate capture.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents(), child_view,
                                             gfx::PointF(5.0, 5.0), child_view,
                                             gfx::PointF(5.0, 5.0));

  child_interceptor->Wait();
  EXPECT_TRUE(child_interceptor->Capturing());

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  // Send MouseUp to location over parent frame, which should still go to
  // the child frame, but the parent frame should receive a MouseMove with
  // the kRelativeMotionEvent modifier set.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&mouse_event, gfx::Point(2, 2), root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, child_view,
                                      &mouse_event);
  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_TRUE(main_frame_monitor.event().GetModifiers() &
              blink::WebInputEvent::Modifiers::kRelativeMotionEvent);
}

// Verify that a click gaining mouse capture and then releasing over the same
// frame does *not* generate an extra MouseMove as if it had moved to a
// different RenderWidgetHostView, even when there are nested cross-process
// frames and there is an obstruction over the parent frame.
// Regression test for https://crbug.com/1021508.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       NoCrossProcessMouseMoveAfterCaptureRelease) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      DepictFrameTree(root));

  // Add a colored div over the B iframe to create the preconditions for the
  // iframe's HitTestRegion to have kHitTestAsk set.
  std::string script =
      "var newDiv = document.createElement('div');"
      "newDiv.style.position = 'relative';"
      "newDiv.style.height = '3px';"
      "newDiv.style.width = '300px';"
      "newDiv.style.top = '-20px';"
      "newDiv.style.left = '10px';"
      "newDiv.style.background = 'green';"
      "document.body.appendChild(newDiv)";
  EXPECT_TRUE(ExecJs(root, script));

  // B_node corresponds to the child of the main frame in Site B, C_node
  // corresponds to the child of the B frame.
  FrameTreeNode* B_node = root->child_at(0);
  FrameTreeNode* C_node = B_node->child_at(0);

  RenderWidgetHostViewBase* C_view = static_cast<RenderWidgetHostViewBase*>(
      C_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(C_node->current_frame_host());

  scoped_refptr<SetMouseCaptureInterceptor> C_interceptor =
      new SetMouseCaptureInterceptor(static_cast<RenderWidgetHostImpl*>(
          C_node->current_frame_host()->GetRenderWidgetHost()));

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor B_frame_monitor(
      B_node->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor C_frame_monitor(
      C_node->current_frame_host()->GetRenderWidgetHost());

  // Send MouseDown to C frame to initiate capture.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents(), C_view,
                                             gfx::PointF(5.0, 5.0), C_view,
                                             gfx::PointF(5.0, 5.0));

  C_interceptor->Wait();
  EXPECT_TRUE(C_interceptor->Capturing());

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(B_frame_monitor.EventWasReceived());
  EXPECT_TRUE(C_frame_monitor.EventWasReceived());
  main_frame_monitor.ResetEventReceived();
  B_frame_monitor.ResetEventReceived();
  C_frame_monitor.ResetEventReceived();

  // Send MouseUp to same location, which should still go to the C frame and
  // also release capture. No other frames should receive mouse events.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event, C_view,
                                         gfx::PointF(5.0, 5.0), C_view,
                                         gfx::PointF(5.0, 5.0));
  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(B_frame_monitor.EventWasReceived());
  EXPECT_TRUE(C_frame_monitor.EventWasReceived());
}

// Verify that mouse capture works on a RenderWidgetHostView level.
// This test checks that a MouseDown triggers mouse capture when it hits
// a scrollbar thumb or a subframe, and does not trigger mouse
// capture if it hits an element in the main frame.
// Flaky, https://crbug.com/1269160
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_CrossProcessMouseCapture DISABLED_CrossProcessMouseCapture
#else
#define MAYBE_CrossProcessMouseCapture CrossProcessMouseCapture
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       MAYBE_CrossProcessMouseCapture) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_large_scrollable_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  ASSERT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;

  // Get the view bounds of the child iframe, which should account for the
  // relative offset of its direct parent within the root frame, for use in
  // targeting the input event.
  gfx::Rect bounds = rwhv_child->GetViewBounds();
  int child_frame_target_x = base::ClampCeil(
      (bounds.x() - root_view->GetViewBounds().x() + 5) * scale_factor);
  int child_frame_target_y = base::ClampCeil(
      (bounds.y() - root_view->GetViewBounds().y() + 5) * scale_factor);

  scoped_refptr<SetMouseCaptureInterceptor> child_interceptor =
      new SetMouseCaptureInterceptor(static_cast<RenderWidgetHostImpl*>(
          child_node->current_frame_host()->GetRenderWidgetHost()));

  // Target MouseDown to child frame.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&mouse_event,
                       gfx::Point(child_frame_target_x, child_frame_target_y),
                       root_view);
  mouse_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, rwhv_child,
                                      &mouse_event);

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());

  // Wait for the mouse capture message.
  child_interceptor->Wait();
  EXPECT_TRUE(child_interceptor->Capturing());
  // Yield the thread, in order to let the capture message be processed by its
  // actual handler.
  base::RunLoop().RunUntilIdle();

  // Target MouseMove at main frame. The child frame is now capturing input,
  // so it should receive the event instead.
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseMove);
  mouse_event.SetModifiers(blink::WebInputEvent::kLeftButtonDown);
  SetWebEventPositions(&mouse_event, gfx::Point(1, 1), root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, rwhv_child,
                                      &mouse_event);

  // Dispatch twice because the router generates an extra MouseLeave for the
  // main frame.
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, rwhv_child,
                                      &mouse_event);
  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());

  // MouseUp releases capture.
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  mouse_event.SetModifiers(blink::WebInputEvent::kNoModifiers);
  SetWebEventPositions(&mouse_event, gfx::Point(1, 1), root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, rwhv_child,
                                      &mouse_event);

  child_interceptor->Wait();
  EXPECT_FALSE(child_interceptor->Capturing());

  // Targeting a MouseDown to the main frame should not initiate capture.
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseDown);
  mouse_event.SetModifiers(blink::WebInputEvent::kLeftButtonDown);
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&mouse_event, gfx::Point(1, 1), root_view);
  mouse_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());

  // Target MouseMove at child frame. Without capture, this should be
  // dispatched to the child frame.
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseMove);
  SetWebEventPositions(&mouse_event,
                       gfx::Point(child_frame_target_x, child_frame_target_y),
                       root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, rwhv_child,
                                      &mouse_event);

  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  // Again, twice because of the transition MouseMove sent to the main
  // frame.
  RouteMouseEventAndWaitUntilDispatch(router, root_view, rwhv_child,
                                      &mouse_event);
  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_FALSE(child_interceptor->Capturing());

  // No release capture events since the capture statu doesn't change.
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  mouse_event.SetModifiers(blink::WebInputEvent::kNoModifiers);
  SetWebEventPositions(&mouse_event,
                       gfx::Point(child_frame_target_x, child_frame_target_y),
                       root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, rwhv_child,
                                      &mouse_event);

  EXPECT_FALSE(child_interceptor->Capturing());
  base::RunLoop().RunUntilIdle();

// Targeting a scrollbar with a click doesn't work on Mac or Android.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
  scoped_refptr<SetMouseCaptureInterceptor> root_interceptor =
      new SetMouseCaptureInterceptor(static_cast<RenderWidgetHostImpl*>(
          root->current_frame_host()->GetRenderWidgetHost()));

  // Now send a MouseDown to target the thumb part of the scroll bar, which
  // should initiate mouse capture for the main frame.
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseDown);
  mouse_event.SetModifiers(blink::WebInputEvent::kLeftButtonDown);
  SetWebEventPositions(&mouse_event, gfx::Point(100, 105), root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);
  EXPECT_TRUE(main_frame_monitor.EventWasReceived());

  // Wait for the mouse capture message.
  root_interceptor->Wait();
  EXPECT_TRUE(root_interceptor->Capturing());
  base::RunLoop().RunUntilIdle();

  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  // Now that the main frame is capturing, a MouseMove targeted to the child
  // frame should be received by the main frame.
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseMove);
  SetWebEventPositions(&mouse_event,
                       gfx::Point(child_frame_target_x, child_frame_target_y),
                       root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);
  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());

  // A MouseUp sent anywhere should cancel the mouse capture.
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  mouse_event.SetModifiers(blink::WebInputEvent::kNoModifiers);
  SetWebEventPositions(&mouse_event,
                       gfx::Point(child_frame_target_x, child_frame_target_y),
                       root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);

  root_interceptor->Wait();
  EXPECT_FALSE(root_interceptor->Capturing());
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       MouseCaptureOnDragSelection) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  ASSERT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  scoped_refptr<SetMouseCaptureInterceptor> interceptor =
      new SetMouseCaptureInterceptor(static_cast<RenderWidgetHostImpl*>(
          child_node->current_frame_host()->GetRenderWidgetHost()));

  // Target MouseDown to child frame.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, gfx::PointF(15.0, 5.0),
                                         rwhv_child, gfx::PointF(15.0, 5.0));

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  // Wait for the mouse capture message.
  interceptor->Wait();
  EXPECT_TRUE(interceptor->Capturing());

  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  // Target MouseMove to child frame to start drag. This should cause the
  // child to start capturing mouse input.
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseMove);
  mouse_event.SetModifiers(blink::WebInputEvent::kLeftButtonDown);
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, gfx::PointF(5.0, 5.0),
                                         rwhv_child, gfx::PointF(5.0, 5.0));

  // Dispatch twice because the router generates an extra MouseLeave for the
  // main frame.
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, gfx::PointF(5.0, 5.0),
                                         rwhv_child, gfx::PointF(5.0, 5.0));

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  EXPECT_TRUE(interceptor->Capturing());

  // Yield the thread, in order to let the capture message be processed by its
  // actual handler.
  {
    base::RunLoop loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  // Now that the child frame is capturing, a MouseMove targeted to the main
  // frame should be received by the child frame.
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, gfx::PointF(-25.0, -25.0),
                                         rwhv_child, gfx::PointF(-25.0, -25.0));
  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  // A MouseUp sent anywhere should cancel the mouse capture.
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  mouse_event.SetModifiers(0);
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, gfx::PointF(-25.0, -25.0),
                                         rwhv_child, gfx::PointF(-25.0, -25.0));

  interceptor->Wait();
  EXPECT_FALSE(interceptor->Capturing());
}

// Verify that upon MouseUp, the coordinate transform cached from the previous
// MouseDown event is applied.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       CacheCoordinateTransformUponMouseDown) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_perspective_transformed_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  scoped_refptr<SetMouseCaptureInterceptor> interceptor =
      new SetMouseCaptureInterceptor(static_cast<RenderWidgetHostImpl*>(
          child_node->current_frame_host()->GetRenderWidgetHost()));

  // Target MouseDown to child frame.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  gfx::PointF click_point(15.0, 5.0);
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, click_point, rwhv_child,
                                         click_point);

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  // Wait for the mouse capture message.
  interceptor->Wait();
  EXPECT_TRUE(interceptor->Capturing());

  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  auto mouse_down_transform = web_contents()
                                  ->GetInputEventRouter()
                                  ->mouse_down_post_transformed_coordinate_;

  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  mouse_event.SetModifiers(0);

  auto* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  SetWebEventPositions(&mouse_event,
                       rwhv_child->TransformPointToRootCoordSpaceF(click_point),
                       root_view);

  auto result = web_contents()->GetInputEventRouter()->FindTargetSynchronously(
      root_view, mouse_event);
  EXPECT_EQ(result.target_location.value(), mouse_down_transform);
}

// Verify that when a divider within a frameset is clicked, mouse capture is
// initiated.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       MouseCaptureOnFramesetResize) {
  GURL main_url(embedded_test_server()->GetURL("/page_with_frameset.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderWidgetHost* widget_host =
      root->current_frame_host()->GetRenderWidgetHost();
  RenderWidgetHostViewBase* rwhv_root =
      static_cast<RenderWidgetHostViewBase*>(widget_host->GetView());

  scoped_refptr<SetMouseCaptureInterceptor> interceptor =
      new SetMouseCaptureInterceptor(
          static_cast<RenderWidgetHostImpl*>(widget_host));

  WaitForHitTestData(root->current_frame_host());

  gfx::PointF click_point =
      gfx::PointF(rwhv_root->GetViewBounds().width() / 2, 20);

  // Click on the divider bar that initiates resize.
  DispatchMouseDownEventAndWaitUntilDispatch(
      web_contents(), rwhv_root, click_point, rwhv_root, click_point);

  // Wait for the mouse capture message.
  interceptor->Wait();
  EXPECT_TRUE(interceptor->Capturing());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       CrossProcessMousePointerCapture) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_iframe_in_div.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_node = root->child_at(0);
  ASSERT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));

  ASSERT_TRUE(ExecJs(root,
                     " document.addEventListener('pointerdown', (e) => {"
                     "  e.target.setPointerCapture(e.pointerId);"
                     "});"));

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  WaitForHitTestData(child_node->current_frame_host());

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  scoped_refptr<SetMouseCaptureInterceptor> root_interceptor =
      new SetMouseCaptureInterceptor(static_cast<RenderWidgetHostImpl*>(
          root->current_frame_host()->GetRenderWidgetHost()));

  // Target MouseDown to main frame.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.SetModifiers(blink::WebInputEvent::kLeftButtonDown);
  mouse_event.pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  SetWebEventPositions(&mouse_event, gfx::Point(1, 1), root_view);
  mouse_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
  // Wait for the mouse capture message.
  root_interceptor->Wait();
  EXPECT_TRUE(root_interceptor->Capturing());
  base::RunLoop().RunUntilIdle();

  // Target MouseMove at child frame. The main frame is now capturing input,
  // so it should receive the event instead.
  float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;
  gfx::Rect bounds = child_view->GetViewBounds();
  int child_frame_target_x = base::ClampCeil(
      (bounds.x() - root_view->GetViewBounds().x() + 5) * scale_factor);
  int child_frame_target_y = base::ClampCeil(
      (bounds.y() - root_view->GetViewBounds().y() + 5) * scale_factor);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseMove);
  mouse_event.SetModifiers(blink::WebInputEvent::kLeftButtonDown);

  SetWebEventPositions(&mouse_event,
                       gfx::Point(child_frame_target_x, child_frame_target_y),
                       root_view);

  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());

  // Add script to release capture and send a mouse move to triger it.
  ASSERT_TRUE(ExecJs(root,
                     " document.addEventListener('pointermove', (e) => {"
                     "  e.target.releasePointerCapture(e.pointerId);"
                     "});"));
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());

  // Mouse capture should be released now.
  root_interceptor->Wait();
  EXPECT_FALSE(root_interceptor->Capturing());

  // Next move event should route to child frame.
  RouteMouseEventAndWaitUntilDispatch(router, root_view, child_view,
                                      &mouse_event);
  // Dispatch twice because the router generates an extra MouseLeave for the
  // main frame.
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, child_view,
                                      &mouse_event);
  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
}

// There are no cursors on Android.
#if !BUILDFLAG(IS_ANDROID)
namespace {

// Intercepts SetCursor calls. The caller has to guarantee that
// `render_widget_host` lives at least as long as SetCursorInterceptor.
class SetCursorInterceptor
    : public blink::mojom::WidgetHostInterceptorForTesting {
 public:
  explicit SetCursorInterceptor(RenderWidgetHostImpl* render_widget_host)
      : render_widget_host_(render_widget_host),
        swapped_impl_(render_widget_host_->widget_host_receiver_for_testing(),
                      this) {}

  ~SetCursorInterceptor() override = default;

  WidgetHost* GetForwardingInterface() override { return render_widget_host_; }

  void SetCursor(const ui::Cursor& cursor) override {
    GetForwardingInterface()->SetCursor(cursor);
    cursor_ = cursor;
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  std::optional<ui::Cursor> cursor() const { return cursor_; }

 private:
  base::RunLoop run_loop_;
  raw_ptr<RenderWidgetHostImpl> render_widget_host_;
  std::optional<ui::Cursor> cursor_;
  mojo::test::ScopedSwapImplForTesting<blink::mojom::WidgetHost> swapped_impl_;
};

// Verify that we receive a mouse cursor update message when we mouse over
// a text field contained in an out-of-process iframe.
void CursorUpdateReceivedFromCrossSiteIframeHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));

  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  FrameTreeNode* child_node = root->child_at(0);
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  WaitForHitTestData(child_node->current_frame_host());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostImpl* rwh_child =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  RenderWidgetHostViewBase* child_view =
      static_cast<RenderWidgetHostViewBase*>(rwh_child->GetView());

  // Intercept SetCursor messages.
  auto set_cursor_interceptor =
      std::make_unique<SetCursorInterceptor>(rwh_child);

  // This should only return nullptr on Android.
  EXPECT_TRUE(root_view->GetCursorManager());

  ui::Cursor cursor;
  EXPECT_FALSE(
      root_view->GetCursorManager()->GetCursorForTesting(root_view, cursor));
  EXPECT_FALSE(
      root_view->GetCursorManager()->GetCursorForTesting(child_view, cursor));

  // Send a MouseMove to the subframe. The frame contains text, and moving the
  // mouse over it should cause the renderer to send a mouse cursor update.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&mouse_event, gfx::Point(60, 60), root_view);
  auto* router = web_contents->GetInputEventRouter();
  RenderWidgetHostMouseEventMonitor child_monitor(
      child_view->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor root_monitor(
      root_view->GetRenderWidgetHost());
  RouteMouseEventAndWaitUntilDispatch(router, root_view, child_view,
                                      &mouse_event);
  // The child_view should receive a mouse-move event.
  EXPECT_TRUE(child_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::Type::kMouseMove,
            child_monitor.event().GetType());
  EXPECT_NEAR(8, child_monitor.event().PositionInWidget().x(),
              kHitTestTolerance);
  EXPECT_NEAR(8, child_monitor.event().PositionInWidget().y(),
              kHitTestTolerance);

  // The root_view should also receive a mouse-move event.
  EXPECT_TRUE(root_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::Type::kMouseMove,
            root_monitor.event().GetType());
  EXPECT_EQ(60, root_monitor.event().PositionInWidget().x());
  EXPECT_EQ(60, root_monitor.event().PositionInWidget().y());

  // SetCursorInterceptor::Wait() implicitly tests whether we receive a
  // blink.mojom.WidgetHost SetCursor message from the renderer process,
  // because it does does not return otherwise.
  set_cursor_interceptor->Wait();

  // The root_view receives a mouse-move event on top of the iframe, which does
  // not send a cursor update.
  EXPECT_FALSE(
      root_view->GetCursorManager()->GetCursorForTesting(root_view, cursor));
  EXPECT_TRUE(
      root_view->GetCursorManager()->GetCursorForTesting(child_view, cursor));
  // Since this moused over a text box, this should not be the default cursor.
  EXPECT_EQ(cursor.type(), ui::mojom::CursorType::kIBeam);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       CursorUpdateReceivedFromCrossSiteIframe) {
  CursorUpdateReceivedFromCrossSiteIframeHelper(shell(),
                                                embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIHitTestBrowserTest,
                       CursorUpdateReceivedFromCrossSiteIframe) {
  CursorUpdateReceivedFromCrossSiteIframeHelper(shell(),
                                                embedded_test_server());
}

// Regression test for https://crbug.com/1099276. An OOPIF at a negative offset
// from the main document should not allow large cursors to intersect browser
// UI.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       LargeCursorRemovedInOffsetOOPIF) {
  GURL url(R"(data:text/html,
    <iframe id='iframe'
            style ='position:absolute; top: -100px'
            width=1000px height=1000px>
    </iframe>)");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // The large-cursor.html document has a custom cursor that is 120x120 with a
  // hotspot on the bottom right corner.
  NavigateIframeToURL(shell()->web_contents(), "iframe",
                      embedded_test_server()->GetURL("/large-cursor.html"));

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  FrameTreeNode* child_node = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  WaitForHitTestData(child_node->current_frame_host());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostImpl* rwh_child =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  RenderWidgetHostViewBase* child_view =
      static_cast<RenderWidgetHostViewBase*>(rwh_child->GetView());

  auto* router = web_contents->GetInputEventRouter();
  RenderWidgetHostMouseEventMonitor child_monitor(
      child_view->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor root_monitor(
      root_view->GetRenderWidgetHost());

  // A cursor with enough room in the root view to fully display without
  // blocking native UI should be shown.
  {
    blink::WebMouseEvent mouse_event(
        blink::WebInputEvent::Type::kMouseMove,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    SetWebEventPositions(&mouse_event, gfx::Point(300, 300), root_view);
    auto set_cursor_interceptor =
        std::make_unique<SetCursorInterceptor>(rwh_child);
    RouteMouseEventAndWaitUntilDispatch(router, root_view, child_view,
                                        &mouse_event);
    set_cursor_interceptor->Wait();
    EXPECT_TRUE(set_cursor_interceptor->cursor().has_value());
    EXPECT_EQ(120, set_cursor_interceptor->cursor()->custom_bitmap().width());
    EXPECT_EQ(120, set_cursor_interceptor->cursor()->custom_bitmap().height());
  }
  // A cursor without enough room to be fully enclosed within the root view
  // should not be shown, even if the iframe is at an offset. The default cursor
  // should be shown instead.
  {
    blink::WebMouseEvent mouse_event(
        blink::WebInputEvent::Type::kMouseMove,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    SetWebEventPositions(&mouse_event, gfx::Point(300, 115), root_view);
    auto set_cursor_interceptor =
        std::make_unique<SetCursorInterceptor>(rwh_child);
    RouteMouseEventAndWaitUntilDispatch(router, root_view, child_view,
                                        &mouse_event);
    // We should see a new cursor come in that replaces the large one.
    set_cursor_interceptor->Wait();
    EXPECT_TRUE(set_cursor_interceptor->cursor().has_value());
    EXPECT_EQ(ui::mojom::CursorType::kPointer,
              set_cursor_interceptor->cursor()->type());
  }
}

// Regression test for https://crbug.com/1454515. An OOPIF
// scrolled away from the main document should not allow
// large cursors to intersect browser UI.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       LargeCursorRemovedInScrolledOOPIF) {
  GURL url(R"(data:text/html,
    <iframe id='iframe'
            style ='position:absolute; top: 0px'
            width=1000px height=1000px>
    </iframe>)");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // The large-cursor.html document has a custom cursor that is 120x120 with a
  // hotspot on the bottom right corner.
  NavigateIframeToURL(shell()->web_contents(), "iframe",
                      embedded_test_server()->GetURL("/large-cursor.html"));

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  FrameTreeNode* child_node = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  WaitForHitTestData(child_node->current_frame_host());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostImpl* rwh_child =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  RenderWidgetHostViewBase* child_view =
      static_cast<RenderWidgetHostViewBase*>(rwh_child->GetView());

  auto* router = web_contents->GetInputEventRouter();

  // Scroll the main frame.
  gfx::Rect initial_child_view_bounds = child_view->GetViewBounds();
  EXPECT_TRUE(ExecJs(root, "window.scrollTo(0, 10);"));
  // Wait until the OOPIF positions have been updated in the browser process.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return initial_child_view_bounds.y() ==
           child_view->GetViewBounds().y() + 10;
  }));

  // A cursor should not be shown when the main frame is scrolled
  // and the iframe is outside the root view's visible viewport.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&mouse_event, gfx::Point(300, 115), root_view);
  auto set_cursor_interceptor =
      std::make_unique<SetCursorInterceptor>(rwh_child);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, child_view,
                                      &mouse_event);
  // We should see a new cursor come in that replaces the large one.
  set_cursor_interceptor->Wait();
  EXPECT_TRUE(set_cursor_interceptor->cursor().has_value());
  EXPECT_EQ(ui::mojom::CursorType::kPointer,
            set_cursor_interceptor->cursor()->type());
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if defined(USE_AURA)
// Browser process hit testing is not implemented on Android, and these tests
// require Aura for RenderWidgetHostViewAura::OnTouchEvent().
// https://crbug.com/491334

// Ensure that scroll events can be cancelled with a wheel handler.
// https://crbug.com/698195

class SitePerProcessMouseWheelHitTestBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  SitePerProcessMouseWheelHitTestBrowserTest() : rwhv_root_(nullptr) {}

  void SetupWheelAndScrollHandlers(content::RenderFrameHostImpl* rfh) {
    // Set up event handlers. The wheel event handler calls prevent default on
    // alternate events, so only every other wheel generates a scroll. The fact
    // that any scroll events fire is dependent on the event going to the main
    // thread, which requires the nonFastScrollableRegion be set correctly
    // on the compositor.
    std::string script =
        "wheel_count = 0;"
        "function wheel_handler(e) {"
        "  wheel_count++;"
        "  if (wheel_count % 2 == 0)"
        "    e.preventDefault();\n"
        "  domAutomationController.send('wheel: ' + wheel_count);"
        "}"
        "function scroll_handler(e) {"
        "  domAutomationController.send('scroll: ' + wheel_count);"
        "}"
        "scroll_div = document.getElementById('scrollable_div');"
        "scroll_div.addEventListener('wheel', wheel_handler);"
        "scroll_div.addEventListener('scroll', scroll_handler);"
        "document.body.style.background = 'black';";

    content::DOMMessageQueue msg_queue(rfh);
    std::string reply;
    EXPECT_TRUE(ExecJs(rfh, script));

    // Wait until renderer's compositor thread is synced. Otherwise the event
    // handler won't be installed when the event arrives.
    {
      MainThreadFrameObserver observer(rfh->GetRenderWidgetHost());
      observer.Wait();
    }
  }

  void SendMouseWheel(gfx::Point location) {
    DCHECK(rwhv_root_);
    ui::ScrollEvent scroll_event(
        ui::EventType::kScroll, location, ui::EventTimeForNow(), 0, 0,
        -ui::MouseWheelEvent::kWheelDelta, 0, ui::MouseWheelEvent::kWheelDelta,
        2);  // This must be '2' or it gets silently
             // dropped.
    UpdateEventRootLocation(&scroll_event, rwhv_root_);
    rwhv_root_->OnScrollEvent(&scroll_event);
  }

  void set_rwhv_root(RenderWidgetHostViewAura* rwhv_root) {
    rwhv_root_ = rwhv_root;
  }

  void RunTest(gfx::Point pos, RenderWidgetHostViewBase* expected_target) {
    content::DOMMessageQueue msg_queue(web_contents());
    std::string reply;

    auto* rwhv_root = static_cast<RenderWidgetHostViewAura*>(
        web_contents()->GetRenderWidgetHostView());
    set_rwhv_root(rwhv_root);

    // Set the wheel scroll latching timeout to a large value to make sure
    // that the timer doesn't expire for the duration of the test.
    rwhv_root->event_handler()->set_mouse_wheel_wheel_phase_handler_timeout(
        TestTimeouts::action_max_timeout());

    InputEventAckWaiter waiter(expected_target->GetRenderWidgetHost(),
                               blink::WebInputEvent::Type::kMouseWheel);
    SendMouseWheel(pos);
    waiter.Wait();

    // Expect both wheel and scroll handlers to fire.
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"wheel: 1\"", reply);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"scroll: 1\"", reply);

    SendMouseWheel(pos);

    // Even though even number events are prevented by default since the first
    // wheel event is not prevented by default, the rest of the wheel events
    // will be handled nonblocking and the scroll will happen.
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"wheel: 2\"", reply);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"scroll: 2\"", reply);

    SendMouseWheel(pos);

    // Odd number of wheels, expect both wheel and scroll handlers to fire
    // again.
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"wheel: 3\"", reply);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"scroll: 3\"", reply);
  }

 private:
  raw_ptr<RenderWidgetHostViewAura, DanglingUntriaged> rwhv_root_;
};

// Fails on Windows official build, see // https://crbug.com/800822
#if BUILDFLAG(IS_WIN)
#define MAYBE_MultipleSubframeWheelEventsOnMainThread \
  DISABLED_MultipleSubframeWheelEventsOnMainThread
#else
#define MAYBE_MultipleSubframeWheelEventsOnMainThread \
  MultipleSubframeWheelEventsOnMainThread
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessMouseWheelHitTestBrowserTest,
                       MAYBE_MultipleSubframeWheelEventsOnMainThread) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_two_positioned_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());

  GURL frame_url(embedded_test_server()->GetURL(
      "b.com", "/page_with_scrollable_div.html"));
  // To test for https://bugs.chromium.org/p/chromium/issues/detail?id=820232
  // it's important that both subframes are in the same renderer process, so
  // we load the same URL in each case.
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), frame_url));

  for (int frame_index = 0; frame_index < 2; frame_index++) {
    // Synchronize with the child and parent renderers to guarantee that the
    // surface information required for event hit testing is ready.
    RenderWidgetHostViewBase* child_rwhv =
        static_cast<RenderWidgetHostViewBase*>(
            root->child_at(frame_index)->current_frame_host()->GetView());

    WaitForHitTestData(root->child_at(frame_index)->current_frame_host());

    content::RenderFrameHostImpl* child =
        root->child_at(frame_index)->current_frame_host();
    SetupWheelAndScrollHandlers(child);

    gfx::Rect bounds = child_rwhv->GetViewBounds();
    gfx::Point pos(bounds.x() + 10, bounds.y() + 10);

    RunTest(pos, child_rwhv);
  }
}

// Verifies that test in SubframeWheelEventsOnMainThread also makes sense for
// the same page loaded in the mainframe.
// Fails on Windows official build, see // https://crbug.com/800822
#if BUILDFLAG(IS_WIN)
#define MAYBE_MainframeWheelEventsOnMainThread \
  DISABLED_MainframeWheelEventsOnMainThread
#else
#define MAYBE_MainframeWheelEventsOnMainThread MainframeWheelEventsOnMainThread
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessMouseWheelHitTestBrowserTest,
                       MAYBE_MainframeWheelEventsOnMainThread) {
  GURL main_url(
      embedded_test_server()->GetURL("/page_with_scrollable_div.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  content::RenderFrameHostImpl* rfhi = root->current_frame_host();
  SetupWheelAndScrollHandlers(rfhi);

  gfx::Point pos(10, 10);

  RunTest(pos, rfhi->GetRenderWidgetHost()->GetView());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMouseWheelHitTestBrowserTest,
                       InputEventRouterWheelTargetTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  auto* rwhv_root = static_cast<RenderWidgetHostViewAura*>(
      web_contents()->GetRenderWidgetHostView());
  set_rwhv_root(rwhv_root);

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(embedded_test_server()->GetURL(
      "b.com", "/page_with_scrollable_div.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  RenderWidgetHostViewBase* child_rwhv = static_cast<RenderWidgetHostViewBase*>(
      root->child_at(0)->current_frame_host()->GetView());
  WaitForHitTestData(root->child_at(0)->current_frame_host());

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  // Send a mouse wheel event to child.
  gfx::Rect bounds = child_rwhv->GetViewBounds();
  gfx::Point pos(bounds.x() + 10, bounds.y() + 10);
  InputEventAckWaiter waiter(child_rwhv->GetRenderWidgetHost(),
                             blink::WebInputEvent::Type::kMouseWheel);
  SendMouseWheel(pos);
  waiter.Wait();

  EXPECT_EQ(child_rwhv, router->wheel_target_);

  // Send a mouse wheel event to the main frame. It will be still routed to
  // child till the end of current scrolling sequence. Since wheel scroll
  // latching is enabled by default, we always do sync targeting so
  // InputEventAckWaiter is not needed here.
  TestInputEventObserver child_frame_monitor(child_rwhv->GetRenderWidgetHost());
  SendMouseWheel(pos);
  EXPECT_EQ(child_rwhv, router->wheel_target_);

  // Verify that this a mouse wheel event was sent to the child frame renderer.
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_TRUE(base::Contains(child_frame_monitor.events_received(),
                             blink::WebInputEvent::Type::kMouseWheel));

  // Kill the wheel target view process. This must reset the wheel_target_.
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_EQ(nullptr, router->wheel_target_);
}

// Ensure that the positions of mouse wheel events sent to cross-process
// subframes account for any change in the position of the subframe during the
// scroll sequence.
// TODO(crbug.com/40663303): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(SitePerProcessMouseWheelHitTestBrowserTest,
                       DISABLED_MouseWheelEventPositionChange) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_tall_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  auto* rwhv_root = static_cast<RenderWidgetHostViewAura*>(
      web_contents()->GetRenderWidgetHostView());
  set_rwhv_root(rwhv_root);

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  RenderWidgetHostViewChildFrame* child_rwhv =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(0)->current_frame_host()->GetView());
  WaitForHitTestData(root->child_at(0)->current_frame_host());

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  auto await_gesture_event_with_position = base::BindRepeating(
      [](blink::WebInputEvent::Type expected_type,
         RenderWidgetHostViewBase* rwhv, gfx::PointF expected_position,
         gfx::PointF expected_position_in_root,
         blink::mojom::InputEventResultSource,
         blink::mojom::InputEventResultState,
         const blink::WebInputEvent& event) {
        if (event.GetType() != expected_type)
          return false;

        const auto& gesture_event =
            static_cast<const blink::WebGestureEvent&>(event);
        const gfx::PointF root_point = rwhv->TransformPointToRootCoordSpaceF(
            gesture_event.PositionInWidget());

        EXPECT_FLOAT_EQ(gesture_event.PositionInWidget().x(),
                        expected_position.x());
        EXPECT_FLOAT_EQ(gesture_event.PositionInWidget().y(),
                        expected_position.y());
        EXPECT_FLOAT_EQ(root_point.x(), expected_position_in_root.x());
        EXPECT_FLOAT_EQ(root_point.y(), expected_position_in_root.y());
        return true;
      });
  MainThreadFrameObserver thread_observer(rwhv_root->GetRenderWidgetHost());

  // Send a mouse wheel begin event to child.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gfx::Point child_point_in_root(90, 90);
  SetWebEventPositions(&scroll_event, child_point_in_root, rwhv_root);
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = -20.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;

  {
    InputEventAckWaiter await_begin_in_child(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_gesture_event_with_position,
                            blink::WebInputEvent::Type::kGestureScrollBegin,
                            child_rwhv, gfx::PointF(38, 38),
                            gfx::PointF(child_point_in_root)));
    InputEventAckWaiter await_update_in_child(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_gesture_event_with_position,
                            blink::WebInputEvent::Type::kGestureScrollUpdate,
                            child_rwhv, gfx::PointF(38, 38),
                            gfx::PointF(child_point_in_root)));
    InputEventAckWaiter await_update_in_root(
        rwhv_root->GetRenderWidgetHost(),
        base::BindRepeating(await_gesture_event_with_position,
                            blink::WebInputEvent::Type::kGestureScrollUpdate,
                            rwhv_root, gfx::PointF(child_point_in_root),
                            gfx::PointF(child_point_in_root)));
    router->RouteMouseWheelEvent(rwhv_root, &scroll_event, ui::LatencyInfo());
    await_begin_in_child.Wait();
    await_update_in_child.Wait();
    await_update_in_root.Wait();
    thread_observer.Wait();
  }

  // Send mouse wheel update event to child.
  {
    scroll_event.phase = blink::WebMouseWheelEvent::kPhaseChanged;
    InputEventAckWaiter await_update_in_child(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_gesture_event_with_position,
                            blink::WebInputEvent::Type::kGestureScrollUpdate,
                            child_rwhv, gfx::PointF(38, 58),
                            gfx::PointF(child_point_in_root)));
    InputEventAckWaiter await_update_in_root(
        rwhv_root->GetRenderWidgetHost(),
        base::BindRepeating(await_gesture_event_with_position,
                            blink::WebInputEvent::Type::kGestureScrollUpdate,
                            rwhv_root, gfx::PointF(child_point_in_root),
                            gfx::PointF(child_point_in_root)));
    router->RouteMouseWheelEvent(rwhv_root, &scroll_event, ui::LatencyInfo());
    await_update_in_child.Wait();
    await_update_in_root.Wait();
    thread_observer.Wait();
  }

#if !BUILDFLAG(IS_WIN)
  {
    ui::ScrollEvent fling_start(ui::EventType::kScrollFlingStart,
                                child_point_in_root, ui::EventTimeForNow(), 0,
                                10, 0, 10, 0, 1);
    UpdateEventRootLocation(&fling_start, rwhv_root);

    InputEventAckWaiter await_fling_start_in_child(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(await_gesture_event_with_position,
                            blink::WebInputEvent::Type::kGestureFlingStart,
                            child_rwhv, gfx::PointF(38, 78),
                            gfx::PointF(child_point_in_root)));
    rwhv_root->OnScrollEvent(&fling_start);
    await_fling_start_in_child.Wait();
    thread_observer.Wait();
  }
#endif
}

// Ensure that a cross-process subframe with a touch-handler can receive touch
// events.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       SubframeTouchEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_touch_handler.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  WaitForHitTestData(root->child_at(0)->current_frame_host());

  // There's no intrinsic reason the following values can't be equal, but they
  // aren't at present, and if they become the same this test will need to be
  // updated to accommodate.
  EXPECT_NE(cc::TouchAction::kAuto, cc::TouchAction::kNone);

  // Verify the child's input router is initially not set. The TouchStart event
  // will trigger TouchAction::kNone being sent back to the browser.
  RenderWidgetHostImpl* child_render_widget_host =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  EXPECT_FALSE(child_render_widget_host->input_router()
                   ->AllowedTouchAction()
                   .has_value());

  InputEventAckWaiter waiter(child_render_widget_host,
                             blink::WebInputEvent::Type::kTouchStart);

  // Simulate touch event to sub-frame.
  gfx::Point child_center(150, 150);
  auto* rwhv = static_cast<RenderWidgetHostViewAura*>(
      contents->GetRenderWidgetHostView());

  // Wait until renderer's compositor thread is synced.
  {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }

  ui::TouchEvent touch_event(ui::EventType::kTouchPressed, child_center,
                             ui::EventTimeForNow(),
                             ui::PointerDetails(ui::EventPointerType::kTouch,
                                                /* pointer_id*/ 0,
                                                /* radius_x */ 30.0f,
                                                /* radius_y */ 30.0f,
                                                /* force */ 0.0f));
  UpdateEventRootLocation(&touch_event, rwhv);
  rwhv->OnTouchEvent(&touch_event);
  waiter.Wait();
  {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }

  // Verify touch handler in subframe was invoked.
  EXPECT_EQ("touchstart", EvalJs(root->child_at(0), "getLastTouchEvent();"));

  // Verify the presence of the touch handler in the child frame correctly
  // propagates touch-action:none information back to the child's input router.
  EXPECT_EQ(cc::TouchAction::kNone,
            child_render_widget_host->input_router()->AllowedTouchAction());
}

// This test verifies that the test in
// SitePerProcessHitTestBrowserTest.SubframeTouchEventRouting also works
// properly for the main frame. Prior to the CL in which this test is
// introduced, use of MainThreadFrameObserver in SubframeTouchEventRouting was
// not necessary since the touch events were handled on the main thread. Now
// they are handled on the compositor thread, hence the need to synchronize.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       MainframeTouchEventRouting) {
  GURL main_url(
      embedded_test_server()->GetURL("/page_with_touch_handler.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();

  // Synchronize with the renderers to guarantee that the
  // surface information required for event hit testing is ready.
  auto* rwhv = static_cast<RenderWidgetHostViewAura*>(
      contents->GetRenderWidgetHostView());

  // There's no intrinsic reason the following values can't be equal, but they
  // aren't at present, and if they become the same this test will need to be
  // updated to accommodate.
  EXPECT_NE(cc::TouchAction::kAuto, cc::TouchAction::kNone);

  // Verify the main frame's input router is initially not set. The
  // TouchStart event will trigger TouchAction::kNone being sent back to the
  // browser.
  RenderWidgetHostImpl* render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();
  EXPECT_FALSE(
      render_widget_host->input_router()->AllowedTouchAction().has_value());

  // Simulate touch event to sub-frame.
  gfx::Point frame_center(150, 150);

  // Wait until renderer's compositor thread is synced.
  {
    auto observer =
        std::make_unique<MainThreadFrameObserver>(render_widget_host);
    observer->Wait();
  }

  ui::TouchEvent touch_event(ui::EventType::kTouchPressed, frame_center,
                             ui::EventTimeForNow(),
                             ui::PointerDetails(ui::EventPointerType::kTouch,
                                                /* pointer_id*/ 0,
                                                /* radius_x */ 30.0f,
                                                /* radius_y */ 30.0f,
                                                /* force */ 0.0f));
  UpdateEventRootLocation(&touch_event, rwhv);
  rwhv->OnTouchEvent(&touch_event);
  {
    auto observer =
        std::make_unique<MainThreadFrameObserver>(render_widget_host);
    observer->Wait();
  }

  // Verify touch handler in subframe was invoked.
  EXPECT_EQ("touchstart", EvalJs(root, "getLastTouchEvent();"));

  // Verify the presence of the touch handler in the child frame correctly
  // propagates touch-action:none information back to the child's input router.
  EXPECT_EQ(cc::TouchAction::kNone,
            render_widget_host->input_router()->AllowedTouchAction());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       SubframeGestureEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_click_handler.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  WaitForHitTestData(child_frame_host);

  // There have been no GestureTaps sent yet.
  {
    EXPECT_EQ("0 clicks received",
              EvalJs(child_frame_host, "getClickStatus();"));
  }

  // Simulate touch sequence to send GestureTap to sub-frame.
  SyntheticTapGestureParams params;
  params.gesture_source_type = content::mojom::GestureSourceType::kTouchInput;
  gfx::Point center(150, 150);
  params.position = gfx::PointF(center.x(), center.y());
  params.duration_ms = 100;
  std::unique_ptr<SyntheticTapGesture> gesture(new SyntheticTapGesture(params));

  RenderWidgetHostImpl* render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();
  InputEventAckWaiter ack_waiter(child_frame_host->GetRenderWidgetHost(),
                                 blink::WebInputEvent::Type::kGestureTap);

  render_widget_host->QueueSyntheticGesture(
      std::move(gesture), base::BindOnce([](SyntheticGesture::Result result) {
        EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
      }));

  // We must wait for the kGestureTap ack to come back before querying the click
  // handler in the subframe.
  ack_waiter.Wait();

  // Verify click handler in subframe was invoked
  {
    EXPECT_EQ("1 click received",
              EvalJs(child_frame_host, "getClickStatus();"));
  }
}

namespace {

// Defined here to be close to
// SitePerProcessHitTestBrowserTest.InputEventRouterGestureTargetQueueTest.
// Will wait for RenderWidgetHost's compositor thread to sync if one is given.
// Returns the unique_touch_id of the TouchStart.
uint32_t SendTouchTapWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& touch_point,
    raw_ptr<input::RenderWidgetHostViewInput>& router_touch_target,
    RenderWidgetHostViewBase* expected_target,
    RenderWidgetHostImpl* child_render_widget_host) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);
  if (child_render_widget_host != nullptr) {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }
  ui::TouchEvent touch_event_pressed(
      ui::EventType::kTouchPressed, touch_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch,
                         /* pointer_id*/ 0,
                         /* radius_x */ 30.0f,
                         /* radius_y */ 30.0f,
                         /* force */ 0.0f));
  UpdateEventRootLocation(&touch_event_pressed, root_view_aura);
  InputEventAckWaiter waiter(expected_target->GetRenderWidgetHost(),
                             blink::WebInputEvent::Type::kTouchStart);
  root_view_aura->OnTouchEvent(&touch_event_pressed);
  if (child_render_widget_host != nullptr) {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }
  waiter.Wait();
  EXPECT_EQ(expected_target, router_touch_target);
  ui::TouchEvent touch_event_released(
      ui::EventType::kTouchReleased, touch_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch,
                         /* pointer_id*/ 0,
                         /* radius_x */ 30.0f,
                         /* radius_y */ 30.0f,
                         /* force */ 0.0f));
  UpdateEventRootLocation(&touch_event_released, root_view_aura);
  root_view_aura->OnTouchEvent(&touch_event_released);
  if (child_render_widget_host != nullptr) {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }
  EXPECT_EQ(nullptr, router_touch_target);
  return touch_event_pressed.unique_event_id();
}

void SendGestureTapSequenceWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& gesture_point,
    base::WeakPtr<input::RenderWidgetHostViewInput>& router_gesture_target,
    const RenderWidgetHostViewBase* expected_target,
    const uint32_t unique_touch_event_id) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);

  ui::GestureEventDetails gesture_begin_details(ui::EventType::kGestureBegin);
  gesture_begin_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_begin_event(
      gesture_point.x(), gesture_point.y(), 0, ui::EventTimeForNow(),
      gesture_begin_details, unique_touch_event_id);
  UpdateEventRootLocation(&gesture_begin_event, root_view_aura);
  root_view_aura->OnGestureEvent(&gesture_begin_event);

  ui::GestureEventDetails gesture_tap_down_details(
      ui::EventType::kGestureTapDown);
  gesture_tap_down_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_tap_down_event(
      gesture_point.x(), gesture_point.y(), 0, ui::EventTimeForNow(),
      gesture_tap_down_details, unique_touch_event_id);
  UpdateEventRootLocation(&gesture_tap_down_event, root_view_aura);
  root_view_aura->OnGestureEvent(&gesture_tap_down_event);
  EXPECT_EQ(expected_target, router_gesture_target.get());

  ui::GestureEventDetails gesture_show_press_details(
      ui::EventType::kGestureShowPress);
  gesture_show_press_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_show_press_event(
      gesture_point.x(), gesture_point.y(), 0, ui::EventTimeForNow(),
      gesture_show_press_details, unique_touch_event_id);
  UpdateEventRootLocation(&gesture_show_press_event, root_view_aura);
  root_view_aura->OnGestureEvent(&gesture_show_press_event);
  EXPECT_EQ(expected_target, router_gesture_target.get());

  ui::GestureEventDetails gesture_tap_details(ui::EventType::kGestureTap);
  gesture_tap_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  gesture_tap_details.set_tap_count(1);
  ui::GestureEvent gesture_tap_event(gesture_point.x(), gesture_point.y(), 0,
                                     ui::EventTimeForNow(), gesture_tap_details,
                                     unique_touch_event_id);
  UpdateEventRootLocation(&gesture_tap_event, root_view_aura);
  root_view_aura->OnGestureEvent(&gesture_tap_event);
  EXPECT_EQ(nullptr, router_gesture_target.get());

  ui::GestureEventDetails gesture_end_details(ui::EventType::kGestureEnd);
  gesture_end_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_end_event(gesture_point.x(), gesture_point.y(), 0,
                                     ui::EventTimeForNow(), gesture_end_details,
                                     unique_touch_event_id);
  UpdateEventRootLocation(&gesture_end_event, root_view_aura);
  root_view_aura->OnGestureEvent(&gesture_end_event);
  EXPECT_EQ(nullptr, router_gesture_target.get());
}

void SendTouchpadPinchSequenceWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& gesture_point,
    raw_ptr<input::RenderWidgetHostViewInput>& router_touchpad_gesture_target,
    RenderWidgetHostViewBase* expected_target) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);

  ui::GestureEventDetails pinch_begin_details(
      ui::EventType::kGesturePinchBegin);
  pinch_begin_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent pinch_begin(gesture_point.x(), gesture_point.y(), 0,
                               ui::EventTimeForNow(), pinch_begin_details);
  UpdateEventRootLocation(&pinch_begin, root_view_aura);
  TestInputEventObserver target_monitor(expected_target->GetRenderWidgetHost());
  InputEventAckWaiter waiter(expected_target->GetRenderWidgetHost(),
                             blink::WebInputEvent::Type::kGesturePinchBegin);
  root_view_aura->OnGestureEvent(&pinch_begin);
  // If the expected target is not the root, then we should be doing async
  // targeting first. So event dispatch should not happen synchronously.
  // Validate that the expected target does not receive the event immediately in
  // such cases.
  waiter.Wait();
  EXPECT_TRUE(target_monitor.EventWasReceived());
  EXPECT_EQ(expected_target, router_touchpad_gesture_target);
  target_monitor.ResetEventsReceived();

  ui::GestureEventDetails pinch_update_details(
      ui::EventType::kGesturePinchUpdate);
  pinch_update_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  pinch_update_details.set_scale(1.23);
  ui::GestureEvent pinch_update(gesture_point.x(), gesture_point.y(), 0,
                                ui::EventTimeForNow(), pinch_update_details);
  UpdateEventRootLocation(&pinch_update, root_view_aura);
  root_view_aura->OnGestureEvent(&pinch_update);
  EXPECT_EQ(expected_target, router_touchpad_gesture_target);
  EXPECT_TRUE(target_monitor.EventWasReceived());
  EXPECT_EQ(target_monitor.EventType(),
            blink::WebInputEvent::Type::kGesturePinchUpdate);
  target_monitor.ResetEventsReceived();

  ui::GestureEventDetails pinch_end_details(ui::EventType::kGesturePinchEnd);
  pinch_end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent pinch_end(gesture_point.x(), gesture_point.y(), 0,
                             ui::EventTimeForNow(), pinch_end_details);
  UpdateEventRootLocation(&pinch_end, root_view_aura);
  root_view_aura->OnGestureEvent(&pinch_end);
  EXPECT_TRUE(target_monitor.EventWasReceived());
  EXPECT_EQ(target_monitor.EventType(),
            blink::WebInputEvent::Type::kGesturePinchEnd);
  EXPECT_EQ(nullptr, router_touchpad_gesture_target);
}

#if !BUILDFLAG(IS_WIN)
// Sending touchpad fling events is not supported on Windows.
void SendTouchpadFlingSequenceWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& gesture_point,
    raw_ptr<input::RenderWidgetHostViewInput>& router_wheel_target,
    RenderWidgetHostViewBase* expected_target) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);

  ui::ScrollEvent scroll_begin(ui::EventType::kScroll, gesture_point,
                               ui::EventTimeForNow(), 0, 1, 0, 1, 0, 2);
  UpdateEventRootLocation(&scroll_begin, root_view_aura);
  root_view_aura->OnScrollEvent(&scroll_begin);

  ui::ScrollEvent fling_start(ui::EventType::kScrollFlingStart, gesture_point,
                              ui::EventTimeForNow(), 0, 1, 0, 1, 0, 1);
  UpdateEventRootLocation(&fling_start, root_view_aura);
  TestInputEventObserver target_monitor(expected_target->GetRenderWidgetHost());
  InputEventAckWaiter fling_start_waiter(
      expected_target->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureFlingStart);
  InputMsgWatcher gestrue_scroll_end_waiter(
      expected_target->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollEnd);
  root_view_aura->OnScrollEvent(&fling_start);
  // If the expected target is not the root, then we should be doing async
  // targeting first. So event dispatch should not happen synchronously.
  // Validate that the expected target does not receive the event immediately in
  // such cases.
  fling_start_waiter.Wait();
  EXPECT_TRUE(target_monitor.EventWasReceived());
  EXPECT_EQ(expected_target, router_wheel_target);
  target_monitor.ResetEventsReceived();

  // Send a GFC event, the fling_controller will process the GFC and stop the
  // fling by generating a wheel event with phaseEnded. The
  // mouse_wheel_event_queue will process the wheel event and generate a GSE.
  InputEventAckWaiter fling_cancel_waiter(
      expected_target->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureFlingCancel);
  ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel, gesture_point,
                               ui::EventTimeForNow(), 0, 1, 0, 1, 0, 1);
  UpdateEventRootLocation(&fling_cancel, root_view_aura);
  root_view_aura->OnScrollEvent(&fling_cancel);
  // Since the fling velocity is small, sometimes the fling is over before
  // sending the GFC event.
  gestrue_scroll_end_waiter.GetAckStateWaitIfNecessary();
  fling_cancel_waiter.Wait();
}
#endif  // !BUILDFLAG(IS_WIN)

}  // anonymous namespace

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       InputEventRouterGestureTargetMapTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_click_handler.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  auto* child_frame_host = root->child_at(0)->current_frame_host();
  auto* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_frame_host->GetView());

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  WaitForHitTestData(child_frame_host);

  // All touches & gestures are sent to the main frame's view, and should be
  // routed appropriately from there.
  auto* rwhv_parent = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());

  input::RenderWidgetHostInputEventRouter* router =
      contents->GetInputEventRouter();
  EXPECT_TRUE(router->touchscreen_gesture_target_map_.empty());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_);

  // Send touch sequence to main-frame.
  gfx::Point main_frame_point(25, 25);
  uint32_t firstId = SendTouchTapWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touch_target_, rwhv_parent,
      nullptr);
  EXPECT_EQ(1u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_);

  // Send touch sequence to child.
  gfx::Point child_center(150, 150);
  uint32_t secondId = SendTouchTapWithExpectedTarget(
      rwhv_parent, child_center, router->touch_target_, rwhv_child, nullptr);
  EXPECT_EQ(2u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_);

  // Send another touch sequence to main frame.
  uint32_t thirdId = SendTouchTapWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touch_target_, rwhv_parent,
      nullptr);
  EXPECT_EQ(3u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_);

  // Send Gestures to clear GestureTargetQueue.

  // The first touch sequence should generate a GestureTapDown, sent to the
  // main frame.
  SendGestureTapSequenceWithExpectedTarget(rwhv_parent, main_frame_point,
                                           router->touchscreen_gesture_target_,
                                           rwhv_parent, firstId);
  EXPECT_EQ(2u, router->touchscreen_gesture_target_map_.size());

  // The second touch sequence should generate a GestureTapDown, sent to the
  // child frame.
  SendGestureTapSequenceWithExpectedTarget(rwhv_parent, child_center,
                                           router->touchscreen_gesture_target_,
                                           rwhv_child, secondId);
  EXPECT_EQ(1u, router->touchscreen_gesture_target_map_.size());

  // The third touch sequence should generate a GestureTapDown, sent to the
  // main frame.
  SendGestureTapSequenceWithExpectedTarget(rwhv_parent, main_frame_point,
                                           router->touchscreen_gesture_target_,
                                           rwhv_parent, thirdId);
  EXPECT_EQ(0u, router->touchscreen_gesture_target_map_.size());
}

// TODO: Flaking test crbug.com/802827
#if BUILDFLAG(IS_WIN)
#define MAYBE_InputEventRouterGesturePreventDefaultTargetMapTest \
  DISABLED_InputEventRouterGesturePreventDefaultTargetMapTest
#else
#define MAYBE_InputEventRouterGesturePreventDefaultTargetMapTest \
  InputEventRouterGesturePreventDefaultTargetMapTest
#endif
#if defined(USE_AURA) || BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(
    SitePerProcessHitTestBrowserTest,
    MAYBE_InputEventRouterGesturePreventDefaultTargetMapTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(embedded_test_server()->GetURL(
      "b.com", "/page_with_touch_start_default_prevented.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));

  auto* child_frame_host = root->child_at(0)->current_frame_host();
  RenderWidgetHostImpl* child_render_widget_host =
      child_frame_host->GetRenderWidgetHost();
  auto* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_frame_host->GetView());

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  WaitForHitTestData(child_frame_host);

  // All touches & gestures are sent to the main frame's view, and should be
  // routed appropriately from there.
  auto* rwhv_parent = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());

  input::RenderWidgetHostInputEventRouter* router =
      contents->GetInputEventRouter();
  EXPECT_TRUE(router->touchscreen_gesture_target_map_.empty());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_);

  // Send touch sequence to main-frame.
  gfx::Point main_frame_point(25, 25);
  uint32_t firstId = SendTouchTapWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touch_target_, rwhv_parent,
      child_render_widget_host);
  EXPECT_EQ(1u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_);

  // Send touch sequence to child.
  gfx::Point child_center(150, 150);
  SendTouchTapWithExpectedTarget(rwhv_parent, child_center,
                                 router->touch_target_, rwhv_child,
                                 child_render_widget_host);
  EXPECT_EQ(1u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_);

  // Send another touch sequence to main frame.
  uint32_t thirdId = SendTouchTapWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touch_target_, rwhv_parent,
      child_render_widget_host);
  EXPECT_EQ(2u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_);

  // Send Gestures to clear GestureTargetQueue.

  // The first touch sequence should generate a GestureTapDown, sent to the
  // main frame.
  SendGestureTapSequenceWithExpectedTarget(rwhv_parent, main_frame_point,
                                           router->touchscreen_gesture_target_,
                                           rwhv_parent, firstId);
  EXPECT_EQ(1u, router->touchscreen_gesture_target_map_.size());

  // The third touch sequence should generate a GestureTapDown, sent to the
  // main frame.
  SendGestureTapSequenceWithExpectedTarget(rwhv_parent, main_frame_point,
                                           router->touchscreen_gesture_target_,
                                           rwhv_parent, thirdId);
  EXPECT_EQ(0u, router->touchscreen_gesture_target_map_.size());
}
#endif  // defined(USE_AURA) || BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       InputEventRouterTouchpadGestureTargetTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_click_handler.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  auto* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_frame_host->GetView());
  WaitForHitTestData(child_frame_host);

  // All touches & gestures are sent to the main frame's view, and should be
  // routed appropriately from there.
  auto* rwhv_parent = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());

  input::RenderWidgetHostInputEventRouter* router =
      contents->GetInputEventRouter();
  EXPECT_EQ(nullptr, router->touchpad_gesture_target_);

  // TODO(crbug.com/40578618): If we send multiple touchpad pinch sequences to
  // separate views and the timing of the acks are such that the begin ack of
  // the second sequence arrives in the root before the end ack of the first
  // sequence, we would produce an invalid gesture event sequence. For now, we
  // wait for the root to receive the end ack before sending a pinch sequence to
  // a different view. The root view should preserve validity of input event
  // sequences when processing acks from multiple views, so that waiting here is
  // not necessary.
  auto wait_for_pinch_sequence_end = base::BindRepeating(
      [](RenderWidgetHost* rwh) {
        InputEventAckWaiter pinch_end_observer(
            rwh, base::BindRepeating([](blink::mojom::InputEventResultSource,
                                        blink::mojom::InputEventResultState,
                                        const blink::WebInputEvent& event) {
              return event.GetType() ==
                         blink::WebGestureEvent::Type::kGesturePinchEnd &&
                     !static_cast<const blink::WebGestureEvent&>(event)
                          .NeedsWheelEvent();
            }));
        pinch_end_observer.Wait();
      },
      rwhv_parent->GetRenderWidgetHost());

  gfx::Point main_frame_point(25, 25);
  gfx::Point child_center(150, 150);

  // Send touchpad pinch sequence to main-frame.
  SendTouchpadPinchSequenceWithExpectedTarget(rwhv_parent, main_frame_point,
                                              router->touchpad_gesture_target_,
                                              rwhv_parent);

  wait_for_pinch_sequence_end.Run();

  // Send touchpad pinch sequence to child.
  SendTouchpadPinchSequenceWithExpectedTarget(
      rwhv_parent, child_center, router->touchpad_gesture_target_, rwhv_child);

  wait_for_pinch_sequence_end.Run();

  // Send another touchpad pinch sequence to main frame.
  SendTouchpadPinchSequenceWithExpectedTarget(rwhv_parent, main_frame_point,
                                              router->touchpad_gesture_target_,
                                              rwhv_parent);

#if !BUILDFLAG(IS_WIN)
  // Sending touchpad fling events is not supported on Windows.

  // Send touchpad fling sequence to main-frame.
  SendTouchpadFlingSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->wheel_target_, rwhv_parent);

  // Send touchpad fling sequence to child.
  SendTouchpadFlingSequenceWithExpectedTarget(
      rwhv_parent, child_center, router->wheel_target_, rwhv_child);

  // Send another touchpad fling sequence to main frame.
  SendTouchpadFlingSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->wheel_target_, rwhv_parent);
#endif
}

// Test that performing a touchpad pinch over an OOPIF offers the synthetic
// wheel events to the child and causes the page scale factor to change for
// the main frame (given that the child did not consume the wheel).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/41449850): Flaky on multiple platforms.
#define MAYBE_TouchpadPinchOverOOPIF DISABLED_TouchpadPinchOverOOPIF
#else
#define MAYBE_TouchpadPinchOverOOPIF TouchpadPinchOverOOPIF
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       MAYBE_TouchpadPinchOverOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_wheel_handler.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  auto* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_frame_host->GetView());
  WaitForHitTestData(child_frame_host);

  auto* rwhv_parent = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());

  input::RenderWidgetHostInputEventRouter* router =
      contents->GetInputEventRouter();
  EXPECT_EQ(nullptr, router->touchpad_gesture_target_);

  const float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;
  const gfx::Point point_in_child(base::ClampCeil(100 * scale_factor),
                                  base::ClampCeil(100 * scale_factor));

  content::TestPageScaleObserver scale_observer(shell()->web_contents());
  SendTouchpadPinchSequenceWithExpectedTarget(rwhv_parent, point_in_child,
                                              router->touchpad_gesture_target_,
                                              rwhv_child);

  // Ensure the child frame saw the wheel event.
  ASSERT_EQ(false, EvalJs(child_frame_host,
                          "handlerPromise.then(function(e) {"
                          "  return e.defaultPrevented;"
                          "});"));

  scale_observer.WaitForPageScaleUpdate();
}

#endif  // defined(USE_AURA)

// Test that we can still perform a touchpad pinch gesture in the absence of viz
// hit test data without crashing.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       TouchpadPinchWhenMissingHitTestDataDoesNotCrash) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  // Even though we're sending the events to the root, we need an OOPIF so
  // that hit testing doesn't short circuit.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Clobber the real hit test data once it comes in.
  WaitForHitTestData(root->current_frame_host());
  ASSERT_TRUE(GetHostFrameSinkManager());
  viz::DisplayHitTestQueryMap empty_hit_test_map;
  viz::HostFrameSinkManagerTestApi(GetHostFrameSinkManager())
      .SetDisplayHitTestQuery(std::move(empty_hit_test_map));

  const gfx::PointF point_in_root(1, 1);
  SyntheticPinchGestureParams params;
  params.gesture_source_type =
      content::mojom::GestureSourceType::kTouchpadInput;
  params.scale_factor = 1.2f;
  params.anchor = point_in_root;

  auto pinch_gesture = std::make_unique<SyntheticTouchpadPinchGesture>(params);
  RenderWidgetHostImpl* render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();

  base::RunLoop run_loop;
  render_widget_host->QueueSyntheticGesture(
      std::move(pinch_gesture),
      base::BindOnce(
          [](base::OnceClosure quit_closure, SyntheticGesture::Result result) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

// Tests that performing a touchpad double-tap zoom over an OOPIF offers the
// synthetic wheel event to the child.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/41449850): Flaky on multiple platforms.
#define MAYBE_TouchpadDoubleTapZoomOverOOPIF \
  DISABLED_TouchpadDoubleTapZoomOverOOPIF
#else
#define MAYBE_TouchpadDoubleTapZoomOverOOPIF TouchpadDoubleTapZoomOverOOPIF
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       MAYBE_TouchpadDoubleTapZoomOverOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();

  blink::web_pref::WebPreferences prefs = contents->GetOrCreateWebPreferences();
  prefs.double_tap_to_zoom_enabled = true;
  contents->SetWebPreferences(prefs);

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_wheel_handler.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  WaitForHitTestData(child_frame_host);

  auto* root_view = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child_frame_host->GetRenderWidgetHost()->GetView());

  const float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;
  gfx::PointF point_in_screen(child_view->GetViewBounds().CenterPoint());
  point_in_screen.Scale(scale_factor);
  // It might seem weird to not also scale the root_view's view bounds, but
  // since the origin should be unaffected by page scale we don't need to.
  const gfx::PointF root_location(
      point_in_screen - root_view->GetViewBounds().OffsetFromOrigin());

  input::RenderWidgetHostInputEventRouter* router =
      contents->GetInputEventRouter();

  blink::WebGestureEvent double_tap_zoom(
      blink::WebInputEvent::Type::kGestureDoubleTap,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchpad);
  double_tap_zoom.SetPositionInWidget(root_location);
  double_tap_zoom.SetPositionInScreen(point_in_screen);
  double_tap_zoom.data.tap.tap_count = 1;
  double_tap_zoom.SetNeedsWheelEvent(true);

  content::TestPageScaleObserver scale_observer(shell()->web_contents());

  router->RouteGestureEvent(root_view, &double_tap_zoom, ui::LatencyInfo());

  // Ensure the child frame saw the wheel event.
  EXPECT_EQ(false, EvalJs(child_frame_host,
                          "handlerPromise.then(function(e) {"
                          "  return e.defaultPrevented;"
                          "});"));

  // TODO(mcnee): Support double-tap zoom gesture for OOPIFs. For now, we
  // only test that any scale change still happens in the main frame when
  // the double tap is performed over the OOPIF. Once this works with OOPIFs,
  // we should be able to test that the new scale is based on the target
  // rect of the element in the OOPIF. https://crbug.com/758348
  scale_observer.WaitForPageScaleUpdate();
}

// A WebContentsDelegate to capture ContextMenu creation events.
class ContextMenuObserverDelegate : public WebContentsDelegate {
 public:
  ContextMenuObserverDelegate()
      : context_menu_created_(false),
        message_loop_runner_(new MessageLoopRunner) {}

  ContextMenuObserverDelegate(const ContextMenuObserverDelegate&) = delete;
  ContextMenuObserverDelegate& operator=(const ContextMenuObserverDelegate&) =
      delete;

  ~ContextMenuObserverDelegate() override {}

  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override {
    context_menu_created_ = true;
    menu_params_ = params;
    message_loop_runner_->Quit();
    return true;
  }

  ContextMenuParams getParams() { return menu_params_; }

  void Wait() {
    if (!context_menu_created_)
      message_loop_runner_->Run();
    context_menu_created_ = false;
  }

 private:
  bool context_menu_created_;
  ContextMenuParams menu_params_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

// Helper function to run the CreateContextMenuTest in either normal
// or high DPI mode.
void CreateContextMenuTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // Ensure that the child process renderer is ready to have input events
  // routed to it. This happens when the browser process has received
  // updated compositor surfaces from both renderer processes.
  WaitForHitTestData(child_node->current_frame_host());

  // A WebContentsDelegate to listen for the ShowContextMenu message.
  ContextMenuObserverDelegate context_menu_delegate;
  shell->web_contents()->SetDelegate(&context_menu_delegate);

  input::RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell->web_contents())
          ->GetInputEventRouter();

  float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;

  gfx::Rect root_bounds = root_view->GetViewBounds();
  gfx::Rect bounds = rwhv_child->GetViewBounds();

  gfx::Point point(
      base::ClampCeil((bounds.x() - root_bounds.x() + 5) * scale_factor),
      base::ClampCeil((bounds.y() - root_bounds.y() + 5) * scale_factor));

  // Target right-click event to child frame.
  blink::WebMouseEvent click_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  click_event.button = blink::WebPointerProperties::Button::kRight;
  SetWebEventPositions(&click_event, point, root_view);
  click_event.click_count = 1;
  router->RouteMouseEvent(root_view, &click_event, ui::LatencyInfo());

  // We also need a MouseUp event, needed by Windows.
  click_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  SetWebEventPositions(&click_event, point, root_view);
  router->RouteMouseEvent(root_view, &click_event, ui::LatencyInfo());

  context_menu_delegate.Wait();

  ContextMenuParams params = context_menu_delegate.getParams();

  EXPECT_NEAR(point.x(), params.x, kHitTestTolerance);
  EXPECT_NEAR(point.y(), params.y, kHitTestTolerance);
}

#if BUILDFLAG(IS_ANDROID)
// High DPI tests don't work properly on Android, which has fixed scale factor.
#define MAYBE_CreateContextMenuTest DISABLED_CreateContextMenuTest
#else
#define MAYBE_CreateContextMenuTest CreateContextMenuTest
#endif

// Test that a mouse right-click to an out-of-process iframe causes a context
// menu to be generated with the correct screen position.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       MAYBE_CreateContextMenuTest) {
  CreateContextMenuTestHelper(shell(), embedded_test_server());
}

// Test that a mouse right-click to an out-of-process iframe causes a context
// menu to be generated with the correct screen position on a screen with
// non-default scale factor.
IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIHitTestBrowserTest,
                       MAYBE_CreateContextMenuTest) {
  CreateContextMenuTestHelper(shell(), embedded_test_server());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// The Popup menu test often times out on linux. https://crbug.com/1111402
#define MAYBE_PopupMenuTest DISABLED_PopupMenuTest
#else
#define MAYBE_PopupMenuTest PopupMenuTest
#endif

// Test that clicking a select element in an out-of-process iframe creates
// a popup menu in the correct position.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest, MAYBE_PopupMenuTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "baz.com", "/site_isolation/page-with-select.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child_node, site_url));

  web_contents()->SendScreenRects();

  WaitForHitTestData(child_node->current_frame_host());

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  std::optional<ShowPopupWidgetWaiter> popup_waiter(
      std::in_place, web_contents(), child_node->current_frame_host());

  // Target left-click event to child frame.
  blink::WebMouseEvent click_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  click_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&click_event, gfx::Point(15, 15), rwhv_root);
  click_event.click_count = 1;
  rwhv_child->ProcessMouseEvent(click_event, ui::LatencyInfo());

  // Dismiss the popup.
  SetWebEventPositions(&click_event, gfx::Point(1, 1), rwhv_root);
  rwhv_child->ProcessMouseEvent(click_event, ui::LatencyInfo());

  display::ScreenInfo screen_info =
      shell()->web_contents()->GetRenderWidgetHostView()->GetScreenInfo();

  popup_waiter->Wait();
  gfx::Rect popup_rect = popup_waiter->last_initial_rect();
  popup_rect =
      gfx::ScaleToRoundedRect(popup_rect, 1 / screen_info.device_scale_factor);
#if BUILDFLAG(IS_MAC)
  // On Mac and Android we receive the coordinates before they are transformed,
  // so they are still relative to the out-of-process iframe origin.
  int expected_x = base::ClampRound(9 / screen_info.device_scale_factor);
  int expected_y = base::ClampRound(9 / screen_info.device_scale_factor);
  EXPECT_EQ(popup_rect.x(), expected_x);
  EXPECT_EQ(popup_rect.y(), expected_y);
#elif BUILDFLAG(IS_ANDROID)
  // Android doesn't seem to care about the device_scale_factor.
  EXPECT_EQ(popup_rect.x(), 9);
  EXPECT_EQ(popup_rect.y(), 9);
#else
  if (!IsScreenTooSmallForPopup(screen_info)) {
    EXPECT_EQ(popup_rect.x() - rwhv_root->GetViewBounds().x(), 354);
    EXPECT_EQ(popup_rect.y() - rwhv_root->GetViewBounds().y(), 94);
  }
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Verify click-and-drag selection of popups still works on Linux with
  // OOPIFs enabled. This is only necessary to test on Aura because Mac and
  // Android use native widgets. Windows does not support this as UI
  // convention (it requires separate clicks to open the menu and select an
  // option). See https://crbug.com/703191.
  int process_id = child_node->current_frame_host()->GetProcess()->GetID();
  popup_waiter.emplace(web_contents(), child_node->current_frame_host());
  input::RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();
  // Re-open the select element.
  SetWebEventPositions(&click_event, gfx::Point(360, 90), rwhv_root);
  click_event.click_count = 1;
  router->RouteMouseEvent(rwhv_root, &click_event, ui::LatencyInfo());

  popup_waiter->Wait();

  RenderWidgetHostViewAura* popup_view = static_cast<RenderWidgetHostViewAura*>(
      RenderWidgetHost::FromID(process_id, popup_waiter->last_routing_id())
          ->GetView());
  EXPECT_TRUE(popup_view);

  RenderWidgetHostMouseEventMonitor popup_monitor(
      popup_view->GetRenderWidgetHost());

  // Next send a mouse up directly targeting the first option, simulating a
  // drag. This requires a ui::MouseEvent because it tests behavior that is
  // above RWH input event routing.
  ui::MouseEvent mouse_up_event(ui::EventType::kMouseReleased,
                                gfx::Point(10, 5), gfx::Point(10, 5),
                                ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                                ui::EF_LEFT_MOUSE_BUTTON);
  UpdateEventRootLocation(&mouse_up_event, rwhv_root);
  popup_view->OnMouseEvent(&mouse_up_event);

  // This verifies that the popup actually received the event, and it wasn't
  // diverted to a different RenderWidgetHostView due to mouse capture.
  EXPECT_TRUE(popup_monitor.EventWasReceived());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  // There are posted tasks that must be run before the test shuts down, lest
  // they access deleted state.
  RunPostedTasks();
}

// Test that clicking a select element in a nested out-of-process iframe creates
// a popup menu in the correct position, even if the top-level page repositions
// its out-of-process iframe. This verifies that screen positioning information
// is propagating down the frame tree correctly.
// On Android the reported menu coordinates are relative to the OOPIF, and its
// screen position is computed later, so this test isn't relevant.
// Flaky on all other platforms: https://crbug.com/1074248
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       DISABLED_NestedPopupMenuTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  web_contents()->SendScreenRects();

  // For clarity, we are labeling the frame tree nodes as:
  //  - root_node
  //   \-> b_node (out-of-process from root and c_node)
  //     \-> c_node (out-of-process from root and b_node)

  content::TestNavigationObserver navigation_observer(shell()->web_contents());
  FrameTreeNode* b_node = root->child_at(0);
  FrameTreeNode* c_node = b_node->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "baz.com", "/site_isolation/page-with-select.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(c_node, site_url));

  RenderWidgetHostViewBase* rwhv_c_node =
      static_cast<RenderWidgetHostViewBase*>(
          c_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            c_node->current_frame_host()->GetSiteInstance());

  std::optional<ShowPopupWidgetWaiter> popup_waiter(
      std::in_place, web_contents(), c_node->current_frame_host());

  WaitForHitTestData(c_node->current_frame_host());

  // Target left-click event to child frame.
  blink::WebMouseEvent click_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  click_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&click_event, gfx::Point(15, 15), rwhv_root);
  click_event.click_count = 1;
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  // Prompt the WebContents to dismiss the popup by clicking elsewhere.
  SetWebEventPositions(&click_event, gfx::Point(1, 1), rwhv_root);
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  popup_waiter->Wait();

  gfx::Rect popup_rect = popup_waiter->last_initial_rect();

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(popup_rect.x(), 9);
  EXPECT_EQ(popup_rect.y(), 9);
#else
  display::ScreenInfo screen_info =
      shell()->web_contents()->GetRenderWidgetHostView()->GetScreenInfo();
  if (!IsScreenTooSmallForPopup(screen_info)) {
    EXPECT_EQ(popup_rect.x() - rwhv_root->GetViewBounds().x(), 354);
    EXPECT_EQ(popup_rect.y() - rwhv_root->GetViewBounds().y(), 154);
  }
#endif

  // Save the screen rect for b_node. Since it updates asynchronously from
  // the script command that changes it, we need to wait for it to change
  // before attempting to create the popup widget again.
  gfx::Rect last_b_node_bounds_rect =
      b_node->current_frame_host()->GetView()->GetViewBounds();

  std::string script =
      "var iframe = document.querySelector('iframe');"
      "iframe.style.position = 'absolute';"
      "iframe.style.left = 150;"
      "iframe.style.top = 150;";
  EXPECT_TRUE(ExecJs(root, script));

  popup_waiter.emplace(web_contents(), c_node->current_frame_host());

  // Wait for b_node's screen rect to get updated. There doesn't seem to be any
  // better way to find out when this happens.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !(last_b_node_bounds_rect.x() ==
                 b_node->current_frame_host()->GetView()->GetViewBounds().x() &&
             last_b_node_bounds_rect.y() ==
                 b_node->current_frame_host()->GetView()->GetViewBounds().y());
  }));

  click_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&click_event, gfx::Point(15, 15), rwhv_root);
  click_event.click_count = 1;
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  SetWebEventPositions(&click_event, gfx::Point(1, 1), rwhv_root);
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  popup_waiter->Wait();

  popup_rect = popup_waiter->last_initial_rect();

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(popup_rect.x(), 9);
  EXPECT_EQ(popup_rect.y(), 9);
#else
  if (!IsScreenTooSmallForPopup(screen_info)) {
    EXPECT_EQ(popup_rect.x() - rwhv_root->GetViewBounds().x(), 203);
    EXPECT_EQ(popup_rect.y() - rwhv_root->GetViewBounds().y(), 248);
  }
#endif

  // There are posted tasks that must be run before the test shuts down, lest
  // they access deleted state.
  RunPostedTasks();
}

// Verify that scrolling the main frame correctly updates the position to
// a nested child frame. See issue https://crbug.com/878703 for more
// information.
// On Mac and Android, the reported menu coordinates are relative to the
// OOPIF, and its screen position is computed later, so this test isn't
// relevant on those platforms.
// This has been disabled on CastOS due to flakiness per crbug.com/1074249.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CASTOS)
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       ScrolledNestedPopupMenuTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_tall_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_node = root->child_at(0);

  GURL child_url(embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child_node, child_url));

  FrameTreeNode* grandchild_node = child_node->child_at(0);

  RenderProcessHost* rph = grandchild_node->current_frame_host()->GetProcess();
  RenderProcessHostWatcher watcher(
      rph, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  GURL grandchild_url(embedded_test_server()->GetURL(
      "c.com", "/site_isolation/page-with-select.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(grandchild_node, grandchild_url));

  // This is to make sure that the navigation is completed and the previous
  // RenderProcessHost is destroyed.
  watcher.Wait();

  WaitForHitTestData(grandchild_node->current_frame_host());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_grandchild =
      static_cast<RenderWidgetHostViewBase*>(
          grandchild_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  std::optional<ShowPopupWidgetWaiter> popup_waiter(
      std::in_place, web_contents(), grandchild_node->current_frame_host());

  // Target left-click event to the select element in the innermost frame.
  DispatchMouseDownEventAndWaitUntilDispatch(
      web_contents(), rwhv_grandchild, gfx::PointF(15, 15), rwhv_grandchild,
      gfx::PointF(15, 15));

  // Prompt the WebContents to dismiss the popup by clicking elsewhere.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents(), rwhv_grandchild,
                                             gfx::PointF(2, 2), rwhv_grandchild,
                                             gfx::PointF(2, 2));
  popup_waiter->Wait();

  // This test isn't verifying correctness of these coordinates, this is just
  // to ensure that they change after scroll.
  gfx::Rect unscrolled_popup_rect = popup_waiter->last_initial_rect();
  gfx::Rect initial_grandchild_view_bounds = rwhv_grandchild->GetViewBounds();

  // Scroll the main frame.
  EXPECT_TRUE(ExecJs(root, "window.scrollTo(0, 20);"));

  // Wait until the OOPIF positions have been updated in the browser process.
  // TODO(crbug.com/41492111): Using `base::test::RunUntil` makes the test flaky
  // on `linux-chromeos-rel`.
  while (true) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
    if (initial_grandchild_view_bounds.y() ==
        rwhv_grandchild->GetViewBounds().y() + 20)
      break;
  }

  popup_waiter.emplace(web_contents(), grandchild_node->current_frame_host());
  // This sends the message directly to the rwhv_grandchild, avoiding using
  // the helper methods, to avert a race condition with the surfaces or
  // HitTestRegions needing to update post-scroll. The event won't hit test
  // correctly if it gets sent before a fresh compositor frame is received.
  blink::WebMouseEvent down_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  down_event.button = blink::WebPointerProperties::Button::kLeft;
  down_event.click_count = 1;
  down_event.SetPositionInWidget(15, 15);
  rwhv_grandchild->ProcessMouseEvent(down_event, ui::LatencyInfo());

  // Dismiss the popup again. This time there is no need to worry about
  // compositor frame updates because it is sufficient to send the click to
  // the root frame.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents(), rwhv_root,
                                             gfx::PointF(1, 1), rwhv_root,
                                             gfx::PointF(1, 1));
  popup_waiter->Wait();
  EXPECT_EQ(unscrolled_popup_rect.y(),
            popup_waiter->last_initial_rect().y() + 20);

  // There are posted tasks that must be run before the test shuts down, lest
  // they access deleted state.
  RunPostedTasks();
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CASTOS)

// On Mac and Android, the reported menu coordinates are relative to the OOPIF,
// and its screen position is computed later, so this test isn't relevant on
// those platforms. This has been disabled on CastOS due to flakiness per
// crbug.com/1074249. (This test is based on the one above which is disabled
// on CastOS for this reason).
//
// Tests that a <select>'s visibility is correctly computed and thus shows the
// popup when clicked.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CASTOS)
// TODO(crbug.com/40252258): Test is flaky on every platform.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       DISABLED_ScrolledMainFrameSelectInLongIframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_tall_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_node = root->child_at(0);

  RenderProcessHost* rph = child_node->current_frame_host()->GetProcess();
  RenderProcessHostWatcher watcher(
      rph, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  GURL child_url(embedded_test_server()->GetURL(
      "b.com", "/site_isolation/page-with-select.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child_node, child_url));

  // This is to make sure that the navigation is completed and the previous
  // RenderProcessHost is destroyed.
  watcher.Wait();

  EXPECT_TRUE(ExecJs(child_node,
                     "document.querySelector('select').style.top = '700px';"));

  WaitForHitTestData(child_node->current_frame_host());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  HitTestRegionObserver hit_test_data_change_observer(
      rwhv_child->GetRootFrameSinkId());
  hit_test_data_change_observer.WaitForHitTestData();

  // Scroll the main frame so that the <select> is visible on screen. The
  // element is at (9,700) of the iframe document and the iframe is at (50,50)
  // of the main document.
  EXPECT_TRUE(ExecJs(root, "window.scrollTo(0, 740);"));

  hit_test_data_change_observer.WaitForHitTestDataChange();

  ShowPopupWidgetWaiter popup_waiter(web_contents(),
                                     child_node->current_frame_host());

  // Left click the <select> element inside the iframe.
  DispatchMouseDownEventAndWaitUntilDispatch(web_contents(), rwhv_child,
                                             gfx::PointF(15, 710), rwhv_child,
                                             gfx::PointF(15, 710));

  // Ensure the popup is requested. This test fails if this timesouts.
  popup_waiter.Wait();
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CASTOS)

#if defined(USE_AURA)
class SitePerProcessGestureHitTestBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  SitePerProcessGestureHitTestBrowserTest() {}

  SitePerProcessGestureHitTestBrowserTest(
      const SitePerProcessGestureHitTestBrowserTest&) = delete;
  SitePerProcessGestureHitTestBrowserTest& operator=(
      const SitePerProcessGestureHitTestBrowserTest&) = delete;

  // This functions simulates a sequence of events that are typical of a
  // gesture pinch at |position|. We need this since machinery in the event
  // codepath will require GesturePinch* to be enclosed in
  // GestureScrollBegin/End, and since RenderWidgetHostInputEventRouter needs
  // both the preceding touch events, as well as GestureTapDown, in order to
  // correctly target the subsequent gesture event stream. The minimum stream
  // required to trigger the correct behaviours is represented here, but could
  // be expanded to include additional events such as one or more
  // GestureScrollUpdate and GesturePinchUpdate events.
  void SendPinchBeginEndSequence(RenderWidgetHostViewAura* rwhva,
                                 const gfx::Point& position,
                                 RenderWidgetHost* expected_target_rwh) {
    DCHECK(rwhva);
    // Use full version of constructor with radius, angle and force since it
    // will crash in the renderer otherwise.
    ui::TouchEvent touch_pressed(
        ui::EventType::kTouchPressed, position, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::kTouch,
                           /* pointer_id*/ 0,
                           /* radius_x */ 1.0f,
                           /* radius_y */ 1.0f,
                           /* force */ 1.0f));
    UpdateEventRootLocation(&touch_pressed, rwhva);
    InputEventAckWaiter waiter(expected_target_rwh,
                               blink::WebInputEvent::Type::kTouchStart);
    rwhva->OnTouchEvent(&touch_pressed);
    waiter.Wait();

    ui::GestureEventDetails gesture_tap_down_details(
        ui::EventType::kGestureTapDown);
    gesture_tap_down_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_tap_down(
        position.x(), position.y(), 0, ui::EventTimeForNow(),
        gesture_tap_down_details, touch_pressed.unique_event_id());
    UpdateEventRootLocation(&gesture_tap_down, rwhva);
    rwhva->OnGestureEvent(&gesture_tap_down);

    ui::GestureEventDetails gesture_scroll_begin_details(
        ui::EventType::kGestureScrollBegin);
    gesture_scroll_begin_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    gesture_scroll_begin_details.set_touch_points(2);
    ui::GestureEvent gesture_scroll_begin(
        position.x(), position.y(), 0, ui::EventTimeForNow(),
        gesture_scroll_begin_details, touch_pressed.unique_event_id());
    UpdateEventRootLocation(&gesture_scroll_begin, rwhva);
    rwhva->OnGestureEvent(&gesture_scroll_begin);

    ui::GestureEventDetails gesture_pinch_begin_details(
        ui::EventType::kGesturePinchBegin);
    gesture_pinch_begin_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_pinch_begin(
        position.x(), position.y(), 0, ui::EventTimeForNow(),
        gesture_pinch_begin_details, touch_pressed.unique_event_id());
    UpdateEventRootLocation(&gesture_pinch_begin, rwhva);
    rwhva->OnGestureEvent(&gesture_pinch_begin);

    ui::GestureEventDetails gesture_pinch_end_details(
        ui::EventType::kGesturePinchEnd);
    gesture_pinch_end_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_pinch_end(
        position.x(), position.y(), 0, ui::EventTimeForNow(),
        gesture_pinch_end_details, touch_pressed.unique_event_id());
    UpdateEventRootLocation(&gesture_pinch_end, rwhva);
    rwhva->OnGestureEvent(&gesture_pinch_end);

    ui::GestureEventDetails gesture_scroll_end_details(
        ui::EventType::kGestureScrollEnd);
    gesture_scroll_end_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_scroll_end(
        position.x(), position.y(), 0, ui::EventTimeForNow(),
        gesture_scroll_end_details, touch_pressed.unique_event_id());
    UpdateEventRootLocation(&gesture_scroll_end, rwhva);
    rwhva->OnGestureEvent(&gesture_scroll_end);

    // TouchActionFilter is reset when a touch event sequence ends, so in order
    // to preserve the touch action set by TouchStart, we end release touch
    // after pinch gestures.
    ui::TouchEvent touch_released(
        ui::EventType::kTouchReleased, position, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::kTouch,
                           /* pointer_id*/ 0,
                           /* radius_x */ 1.0f,
                           /* radius_y */ 1.0f,
                           /* force */ 1.0f));
    InputEventAckWaiter touch_released_waiter(
        expected_target_rwh, blink::WebInputEvent::Type::kTouchEnd);
    rwhva->OnTouchEvent(&touch_released);
    touch_released_waiter.Wait();
  }

  void SetupRootAndChild() {
    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(b)"));
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    FrameTreeNode* root_node =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetPrimaryFrameTree()
            .root();
    FrameTreeNode* child_node = root_node->child_at(0);

    rwhv_child_ = static_cast<RenderWidgetHostViewBase*>(
        child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

    rwhva_root_ = static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());

    WaitForHitTestData(child_node->current_frame_host());

    rwhi_child_ = child_node->current_frame_host()->GetRenderWidgetHost();
    rwhi_root_ = root_node->current_frame_host()->GetRenderWidgetHost();
  }

  void SubframeGesturePinchTestHelper(const std::string& url,
                                      bool reset_root_touch_action) {
    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(b)"));

    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    // It is safe to obtain the root frame tree node here, as it doesn't change.
    FrameTreeNode* root_node =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetPrimaryFrameTree()
            .root();
    ASSERT_EQ(1U, root_node->child_count());

    FrameTreeNode* child_node = root_node->child_at(0);
    GURL b_url(embedded_test_server()->GetURL("b.com", url));
    EXPECT_TRUE(NavigateToURLFromRenderer(child_node, b_url));

    ASSERT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/",
        DepictFrameTree(root_node));

    rwhv_child_ = static_cast<RenderWidgetHostViewBase*>(
        child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

    rwhva_root_ = static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());

    WaitForHitTestData(child_node->current_frame_host());

    MainThreadFrameObserver observer(rwhv_child_->GetRenderWidgetHost());
    observer.Wait();

    rwhi_child_ = child_node->current_frame_host()->GetRenderWidgetHost();
    rwhi_root_ = root_node->current_frame_host()->GetRenderWidgetHost();

    TestInputEventObserver root_frame_monitor(rwhi_root_);
    TestInputEventObserver child_frame_monitor(rwhi_child_);

    gfx::Rect bounds = rwhv_child_->GetViewBounds();
    bounds.Offset(gfx::Point() - rwhva_root_->GetViewBounds().origin());

    // The pinch gesture will always sent to the root frame even if the fingers
    // are targeting the iframe. In this case, the test should not crash.
    if (reset_root_touch_action) {
      static_cast<input::InputRouterImpl*>(
          static_cast<RenderWidgetHostImpl*>(rwhva_root_->GetRenderWidgetHost())
              ->input_router())
          ->ForceResetTouchActionForTest();
    }
    SendPinchBeginEndSequence(rwhva_root_, bounds.CenterPoint(), rwhi_child_);

    if (reset_root_touch_action)
      return;

    // Verify that root-RWHI gets nothing.
    EXPECT_FALSE(root_frame_monitor.EventWasReceived());
    // Verify that child-RWHI gets TS/GTD/GSB/GPB/GPE/GSE/TE.
    EXPECT_EQ(blink::WebInputEvent::Type::kTouchStart,
              child_frame_monitor.events_received()[0]);
    EXPECT_EQ(blink::WebInputEvent::Type::kGestureTapDown,
              child_frame_monitor.events_received()[1]);
    EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
              child_frame_monitor.events_received()[2]);
    EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchBegin,
              child_frame_monitor.events_received()[3]);
    EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchEnd,
              child_frame_monitor.events_received()[4]);
    EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
              child_frame_monitor.events_received()[5]);
    EXPECT_EQ(blink::WebInputEvent::Type::kTouchEnd,
              child_frame_monitor.events_received()[6]);

    // Verify that the pinch gestures are consumed by browser.
    EXPECT_EQ(blink::mojom::InputEventResultSource::kBrowser,
              child_frame_monitor.events_acked()[3]);
    EXPECT_EQ(blink::mojom::InputEventResultSource::kBrowser,
              child_frame_monitor.events_acked()[4]);
  }

 protected:
  raw_ptr<RenderWidgetHostViewBase, DanglingUntriaged> rwhv_child_;
  raw_ptr<RenderWidgetHostViewAura, DanglingUntriaged> rwhva_root_;
  raw_ptr<RenderWidgetHostImpl, DanglingUntriaged> rwhi_child_;
  raw_ptr<RenderWidgetHostImpl, DanglingUntriaged> rwhi_root_;
};

IN_PROC_BROWSER_TEST_F(SitePerProcessGestureHitTestBrowserTest,
                       SubframeGesturePinchGoesToMainFrame) {
  SetupRootAndChild();

  TestInputEventObserver root_frame_monitor(rwhi_root_);
  TestInputEventObserver child_frame_monitor(rwhi_child_);

  // Need child rect in main frame coords.
  gfx::Rect bounds = rwhv_child_->GetViewBounds();
  bounds.Offset(gfx::Point() - rwhva_root_->GetViewBounds().origin());
  SendPinchBeginEndSequence(rwhva_root_, bounds.CenterPoint(), rwhi_child_);

  // Verify root-RWHI gets GSB/GPB/GPE/GSE.
  EXPECT_TRUE(root_frame_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            root_frame_monitor.events_received()[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchBegin,
            root_frame_monitor.events_received()[1]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchEnd,
            root_frame_monitor.events_received()[2]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            root_frame_monitor.events_received()[3]);

  // Verify child-RWHI gets TS/TE, GTD/GSB/GSE.
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchStart,
            child_frame_monitor.events_received()[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureTapDown,
            child_frame_monitor.events_received()[1]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            child_frame_monitor.events_received()[2]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            child_frame_monitor.events_received()[3]);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchEnd,
            child_frame_monitor.events_received()[4]);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessGestureHitTestBrowserTest,
                       MainframeGesturePinchGoesToMainFrame) {
  SetupRootAndChild();

  TestInputEventObserver root_frame_monitor(rwhi_root_);
  TestInputEventObserver child_frame_monitor(rwhi_child_);

  // Need child rect in main frame coords.
  gfx::Rect bounds = rwhv_child_->GetViewBounds();
  bounds.Offset(gfx::Point() - rwhva_root_->GetViewBounds().origin());

  gfx::Point main_frame_point(bounds.origin());
  main_frame_point += gfx::Vector2d(-5, -5);
  SendPinchBeginEndSequence(rwhva_root_, main_frame_point, rwhi_root_);

  // Verify root-RWHI gets TS/TE/GTD/GSB/GPB/GPE/GSE.
  EXPECT_TRUE(root_frame_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchStart,
            root_frame_monitor.events_received()[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureTapDown,
            root_frame_monitor.events_received()[1]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            root_frame_monitor.events_received()[2]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchBegin,
            root_frame_monitor.events_received()[3]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchEnd,
            root_frame_monitor.events_received()[4]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            root_frame_monitor.events_received()[5]);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchEnd,
            root_frame_monitor.events_received()[6]);

  // Verify child-RWHI gets no events.
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessGestureHitTestBrowserTest,
                       SubframeGesturePinchDeniedBySubframeTouchAction) {
  SubframeGesturePinchTestHelper("/div_with_touch_action_none.html", false);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessGestureHitTestBrowserTest,
                       SubframeGesturePinchNoCrash) {
  SubframeGesturePinchTestHelper("/div_with_touch_action_auto.html", true);
}
#endif  // defined(USE_AURA)

// Android uses fixed scale factor, which makes this test unnecessary.
// MacOSX does not have fractional device scales.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
#define MAYBE_MouseClickWithNonIntegerScaleFactor \
  DISABLED_MouseClickWithNonIntegerScaleFactor
#else
#define MAYBE_MouseClickWithNonIntegerScaleFactor \
  MouseClickWithNonIntegerScaleFactor
#endif
// Test that MouseDown and MouseUp to the same coordinates do not result in
// different coordinates after routing. See bug https://crbug.com/670253.
IN_PROC_BROWSER_TEST_F(SitePerProcessNonIntegerScaleFactorHitTestBrowserTest,
                       MAYBE_MouseClickWithNonIntegerScaleFactor) {
  GURL initial_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  RenderWidgetHostViewBase* rwhv = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  input::RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();

  // Create listener for input events.
  RenderWidgetHostMouseEventMonitor event_monitor(
      root->current_frame_host()->GetRenderWidgetHost());

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&mouse_event, gfx::Point(75, 75), rwhv);
  mouse_event.click_count = 1;
  event_monitor.ResetEventReceived();
  router->RouteMouseEvent(rwhv, &mouse_event, ui::LatencyInfo());

  EXPECT_TRUE(event_monitor.EventWasReceived());
  gfx::Point mouse_down_coords =
      gfx::Point(event_monitor.event().PositionInWidget().x(),
                 event_monitor.event().PositionInWidget().y());
  event_monitor.ResetEventReceived();

  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  SetWebEventPositions(&mouse_event, gfx::Point(75, 75), rwhv);
  router->RouteMouseEvent(rwhv, &mouse_event, ui::LatencyInfo());

  EXPECT_TRUE(event_monitor.EventWasReceived());
  EXPECT_EQ(mouse_down_coords.x(),
            event_monitor.event().PositionInWidget().x());
  // The transform from browser to renderer is (2, 35) in DIP. When we
  // scale that to pixels, it's (3, 53). Note that 35 * 1.5 should be 52.5,
  // so we already lost precision there in the transform from draw quad.
  EXPECT_NEAR(mouse_down_coords.y(),
              event_monitor.event().PositionInWidget().y(), kHitTestTolerance);
}

// MacOSX does not have fractional device scales.
// Linux/Lacros started failing after Wayland window configuration fixes have
// landed. TODO(crbug.com/40832051): Re-enable once the test issue is addressed.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_NestedSurfaceHitTestTest DISABLED_NestedSurfaceHitTestTest
#else
#define MAYBE_NestedSurfaceHitTestTest NestedSurfaceHitTestTest
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessNonIntegerScaleFactorHitTestBrowserTest,
                       MAYBE_NestedSurfaceHitTestTest) {
  NestedSurfaceHitTestTestHelper(shell(), embedded_test_server());
}

// Verify RenderWidgetHostInputEventRouter can successfully hit test
// a MouseEvent and route it to a clipped OOPIF.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest, HitTestClippedFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_clipped_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* child_node = root->child_at(0);
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());
  WaitForHitTestData(child_node->current_frame_host());

  RenderWidgetHostMouseEventMonitor root_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  gfx::PointF point_in_root(25, 25);
  gfx::PointF point_in_child(100, 100);

  blink::WebMouseEvent down_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  down_event.button = blink::WebPointerProperties::Button::kLeft;
  down_event.click_count = 1;
  SetWebEventPositions(&down_event, point_in_root, rwhv_root);

  blink::WebMouseEvent up_event(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  up_event.button = blink::WebPointerProperties::Button::kLeft;
  up_event.click_count = 1;
  SetWebEventPositions(&up_event, point_in_root, rwhv_root);

  // Target at root.
  RouteMouseEventAndWaitUntilDispatch(router, rwhv_root, rwhv_root,
                                      &down_event);
  EXPECT_TRUE(root_monitor.EventWasReceived());
  EXPECT_FALSE(child_monitor.EventWasReceived());
  EXPECT_NEAR(25, root_monitor.event().PositionInWidget().x(),
              kHitTestTolerance);
  EXPECT_NEAR(25, root_monitor.event().PositionInWidget().y(),
              kHitTestTolerance);

  root_monitor.ResetEventReceived();
  child_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, rwhv_root, rwhv_root, &up_event);
  EXPECT_TRUE(root_monitor.EventWasReceived());
  EXPECT_FALSE(child_monitor.EventWasReceived());
  EXPECT_NEAR(25, root_monitor.event().PositionInWidget().x(),
              kHitTestTolerance);
  EXPECT_NEAR(25, root_monitor.event().PositionInWidget().y(),
              kHitTestTolerance);

  // Target at child.
  root_monitor.ResetEventReceived();
  child_monitor.ResetEventReceived();
  SetWebEventPositions(&down_event, point_in_child, rwhv_root);
  SetWebEventPositions(&up_event, point_in_child, rwhv_root);
  RouteMouseEventAndWaitUntilDispatch(router, rwhv_root, rwhv_child,
                                      &down_event);
  // In surface layer hit testing, we should not query client asynchronously.
  EXPECT_FALSE(root_monitor.EventWasReceived());
  EXPECT_TRUE(child_monitor.EventWasReceived());
  EXPECT_NEAR(90, child_monitor.event().PositionInWidget().x(),
              kHitTestTolerance);
  EXPECT_NEAR(100, child_monitor.event().PositionInWidget().y(),
              kHitTestTolerance);

  root_monitor.ResetEventReceived();
  child_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, rwhv_root, rwhv_child, &up_event);
  // We should reuse the target for mouse up.
  EXPECT_FALSE(root_monitor.EventWasReceived());
  EXPECT_TRUE(child_monitor.EventWasReceived());
  EXPECT_TRUE(child_monitor.EventWasReceived());
  EXPECT_NEAR(90, child_monitor.event().PositionInWidget().x(),
              kHitTestTolerance);
  EXPECT_NEAR(100, child_monitor.event().PositionInWidget().y(),
              kHitTestTolerance);
}

// Verify InputTargetClient works within an OOPIF process.
IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest, HitTestNestedFrames) {
  HitTestNestedFramesHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestBrowserTest,
                       HitTestOOPIFWithPaddingAndBorder) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/oopif_with_padding_and_border.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  RenderWidgetHostViewBase* rwhv_parent =
      static_cast<RenderWidgetHostViewBase*>(
          root->current_frame_host()->GetRenderWidgetHost()->GetView());

  FrameTreeNode* child_node = root->child_at(0);
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestData(child_node->current_frame_host());

  // Layout of the loaded page:
  //
  //  |(0, 0)----------------------------|
  //  |             border               |
  //  |    |(20, 20)----------------|    |
  //  |    |        padding         |    |
  //  |    |    |(40, 40) -------|  |    |
  //  |    |    |                |  |    |
  //  |    |    |  content box   |  |    |
  //  |    |    |                |  |    |
  //  |    |    |----------------|  |    |
  //  |    |                        |    |
  //  |----|------------------------|----|(280, 280)
  //
  // Clicks on the padding or border should be handled by the root while
  // clicks on the content box should be handled by the iframe.

  const gfx::PointF child_origin =
      rwhv_child->TransformPointToRootCoordSpaceF(gfx::PointF());
  {
    gfx::PointF point_in_border = child_origin + gfx::Vector2dF(-30, -30);
    base::RunLoop run_loop;
    viz::FrameSinkId received_frame_sink_id;
    root->current_frame_host()
        ->GetRenderWidgetHost()
        ->GetRenderInputRouter()
        ->input_target_client()
        ->FrameSinkIdAt(
            point_in_border, 0,
            base::BindLambdaForTesting(
                [&](const viz::FrameSinkId& id, const gfx::PointF& point) {
                  received_frame_sink_id = id;
                  run_loop.Quit();
                }));
    run_loop.Run();
    EXPECT_EQ(rwhv_parent->GetFrameSinkId(), received_frame_sink_id);
  }

  {
    gfx::PointF point_in_padding = child_origin + gfx::Vector2dF(-10, -10);
    base::RunLoop run_loop;
    viz::FrameSinkId received_frame_sink_id;
    root->current_frame_host()
        ->GetRenderWidgetHost()
        ->GetRenderInputRouter()
        ->input_target_client()
        ->FrameSinkIdAt(
            point_in_padding, 0,
            base::BindLambdaForTesting(
                [&](const viz::FrameSinkId& id, const gfx::PointF& point) {
                  received_frame_sink_id = id;
                  run_loop.Quit();
                }));
    run_loop.Run();
    EXPECT_EQ(rwhv_parent->GetFrameSinkId(), received_frame_sink_id);
  }

  {
    gfx::PointF point_in_content_box = child_origin + gfx::Vector2dF(10, 10);
    base::RunLoop run_loop;
    viz::FrameSinkId received_frame_sink_id;
    root->current_frame_host()
        ->GetRenderWidgetHost()
        ->GetRenderInputRouter()
        ->input_target_client()
        ->FrameSinkIdAt(
            point_in_content_box, 0,
            base::BindLambdaForTesting(
                [&](const viz::FrameSinkId& id, const gfx::PointF& point) {
                  received_frame_sink_id = id;
                  run_loop.Quit();
                }));
    run_loop.Run();
    EXPECT_EQ(rwhv_child->GetFrameSinkId(), received_frame_sink_id);
  }
}

// TODO(crbug.com/40804367): This flakes badly on debug & sanitizer
// builds on almost all platforms, and on Mac and Android.
IN_PROC_BROWSER_TEST_F(SitePerProcessUserActivationHitTestBrowserTest,
                       DISABLED_RenderWidgetUserActivationStateTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  ASSERT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://foo.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  WaitForHitTestData(child->current_frame_host());

  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetRenderWidgetHost()->GetView());

  // Send a mouse down event to main frame.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();

  gfx::PointF click_point(10, 10);
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event, rwhv_root,
                                         click_point, rwhv_root, click_point);
  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  base::RunLoop().RunUntilIdle();

  // Wait for root frame gets activated.
  while (!root->HasTransientUserActivation()) {
    base::RunLoop loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
  // Child frame doesn't have user activation.
  EXPECT_FALSE(child->HasTransientUserActivation());
  // Root frame's pending activation state has been cleared by activation.
  EXPECT_FALSE(root->current_frame_host()
                   ->GetRenderWidgetHost()
                   ->RemovePendingUserActivationIfAvailable());

  // Clear the activation state.
  root->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kClearActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  // Send a mouse down to child frame.
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseDown);
  child_frame_monitor.ResetEventReceived();
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, click_point, rwhv_child,
                                         click_point);
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  base::RunLoop().RunUntilIdle();

  // Wait for child frame to get activated.
  while (!child->HasTransientUserActivation()) {
    base::RunLoop loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
  // With UAV2, ancestor frames get activated too.
  EXPECT_TRUE(root->HasTransientUserActivation());
  // Both child frame and root frame don't have allowed_activation state
  EXPECT_FALSE(root->current_frame_host()
                   ->GetRenderWidgetHost()
                   ->RemovePendingUserActivationIfAvailable());
  EXPECT_FALSE(child->current_frame_host()
                   ->GetRenderWidgetHost()
                   ->RemovePendingUserActivationIfAvailable());
}

class SitePerProcessHitTestDataGenerationBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  SitePerProcessHitTestDataGenerationBrowserTest() {}

 protected:
  // Load the page |host_name| and retrieve the hit test data from HitTestQuery.
  std::vector<viz::AggregatedHitTestRegion> SetupAndGetHitTestData(
      const std::string& host_name) {
    GURL main_url(embedded_test_server()->GetURL(host_name));
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();

    RenderWidgetHostViewBase* rwhv_root =
        static_cast<RenderWidgetHostViewBase*>(
            root->current_frame_host()->GetRenderWidgetHost()->GetView());

    for (unsigned i = 0; i < root->child_count(); i++) {
        WaitForHitTestData(root->child_at(i)->current_frame_host());
    }

    HitTestRegionObserver observer(rwhv_root->GetRootFrameSinkId());
    observer.WaitForHitTestData();

    device_scale_factor_ = rwhv_root->GetDeviceScaleFactor();
    DCHECK_GT(device_scale_factor_, 0);

    return observer.GetHitTestData();
  }

  float current_device_scale_factor() const { return device_scale_factor_; }

  gfx::QuadF TransformRectToQuadF(const gfx::Rect& rect,
                                  const gfx::Transform& transform,
                                  bool use_scale_factor = true) {
    gfx::Rect scaled_rect =
        use_scale_factor ? gfx::ScaleToEnclosingRect(rect, device_scale_factor_,
                                                     device_scale_factor_)
                         : rect;
    return transform.MapQuad(gfx::QuadF(gfx::RectF(scaled_rect)));
  }

  gfx::QuadF TransformRectToQuadF(
      const viz::AggregatedHitTestRegion& hit_test_region) {
    return TransformRectToQuadF(hit_test_region.rect, hit_test_region.transform,
                                false);
  }

  bool ApproximatelyEqual(const gfx::PointF& p1, const gfx::PointF& p2) const {
    return std::abs(p1.x() - p2.x()) <= 1 && std::abs(p1.y() - p2.y()) <= 1;
  }

  bool ApproximatelyEqual(const gfx::QuadF& quad,
                          const gfx::QuadF& other) const {
    return ApproximatelyEqual(quad.p1(), other.p1()) &&
           ApproximatelyEqual(quad.p2(), other.p2()) &&
           ApproximatelyEqual(quad.p3(), other.p3()) &&
           ApproximatelyEqual(quad.p4(), other.p4());
  }

  gfx::Rect AxisAlignedLayoutRectFromHitTest(
      const viz::AggregatedHitTestRegion& hit_test_region) {
    DCHECK(hit_test_region.transform.Preserves2dAxisAlignment());
    return hit_test_region.transform.MapRect(hit_test_region.rect);
  }

 public:
  static const uint32_t kFastHitTestFlags;
  static const uint32_t kSlowHitTestFlags;
  float device_scale_factor_;
};

const uint32_t
    SitePerProcessHitTestDataGenerationBrowserTest::kFastHitTestFlags =
        viz::HitTestRegionFlags::kHitTestMine |
        viz::HitTestRegionFlags::kHitTestChildSurface |
        viz::HitTestRegionFlags::kHitTestMouse |
        viz::HitTestRegionFlags::kHitTestTouch;

const uint32_t
    SitePerProcessHitTestDataGenerationBrowserTest::kSlowHitTestFlags =
        SitePerProcessHitTestDataGenerationBrowserTest::kFastHitTestFlags |
        viz::HitTestRegionFlags::kHitTestAsk;

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestDataGenerationBrowserTest,
                       TransformedOOPIF) {
  auto hit_test_data =
      SetupAndGetHitTestData("/frame_tree/page_with_transformed_iframe.html");
  float device_scale_factor = current_device_scale_factor();

  // Compute screen space transform for iframe element.
  gfx::Transform expected_transform;
  gfx::Transform translate;
  expected_transform.RotateAboutZAxis(-45);
  translate.Translate(-100 * device_scale_factor, -100 * device_scale_factor);
  expected_transform.PreConcat(translate);

  DCHECK(hit_test_data.size() >= 3);
  // The iframe element in main page is transformed and also clips the content
  // of the subframe, so we expect to do slow path hit testing in this case.
  EXPECT_TRUE(ApproximatelyEqual(
      TransformRectToQuadF(gfx::Rect(100, 100), expected_transform),
      TransformRectToQuadF(hit_test_data[2])));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestDataGenerationBrowserTest,
                       ClippedOOPIFFastPath) {
  auto hit_test_data =
      SetupAndGetHitTestData("/frame_tree/page_with_clipped_iframe.html");
  float device_scale_factor = current_device_scale_factor();
  gfx::Transform expected_transform;
  gfx::Rect original_region(200, 200);
  gfx::Rect expected_transformed_region = gfx::ScaleToEnclosingRect(
      original_region, device_scale_factor, device_scale_factor);

  uint32_t expected_flags = kFastHitTestFlags;
  // Clip2 has overflow: visible property, so it does not apply clip to iframe.
  // Clip1 and clip3 all preserve 2d axis alignment, so we should allow fast
  // path hit testing for the iframe in V2 hit testing.
  expected_transformed_region = gfx::ScaleToEnclosingRect(
      gfx::Rect(100, 100), device_scale_factor, device_scale_factor);

  // Apart from the iframe, it also contains data for root and main frame.
  DCHECK(hit_test_data.size() >= 3);
  EXPECT_TRUE(expected_transformed_region.ApproximatelyEqual(
      AxisAlignedLayoutRectFromHitTest(hit_test_data[2]),
      base::ClampRound(device_scale_factor) + 2));
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform));
  EXPECT_EQ(expected_flags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestDataGenerationBrowserTest,
                       RotatedClippedOOPIF) {
  auto hit_test_data = SetupAndGetHitTestData(
      "/frame_tree/page_with_rotated_clipped_iframe.html");
  float device_scale_factor = current_device_scale_factor();
  // +-Root
  // +---clip1
  // +-----clip2 rotateZ(45)
  // +-------clip3 rotateZ(-45)
  // +---------iframe
  //
  // +----------------300px--------------+
  // |\                                  |
  // |  \                                |
  // |    \                             100px
  // |- x --\                            |
  // |     /                             |
  // +-----------------------------------+
  //
  // Clipped region: x=100/sqrt(2), y=100.
  gfx::Transform expected_transform;
  gfx::Rect expected_region = gfx::ScaleToEnclosingRect(
      gfx::Rect(100 / 1.414, 100), device_scale_factor, device_scale_factor);

  // Compute screen space transform for iframe element, since clip2 is rotated
  // and also clips the iframe, we expect to do slow path hit test on the
  // iframe.
  DCHECK(hit_test_data.size() >= 3);
  EXPECT_TRUE(expected_region.ApproximatelyEqual(hit_test_data[2].rect,
                                                 1 + device_scale_factor));
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestDataGenerationBrowserTest,
                       ClippedRotatedOOPIF) {
  auto hit_test_data = SetupAndGetHitTestData(
      "/frame_tree/page_with_clipped_rotated_iframe.html");
  float device_scale_factor = current_device_scale_factor();
  // +-Root
  // +---clip1
  // +---------iframe rotateZ(45deg)
  //
  // There are actually 2 clips applied to surface layer, in root space they
  // are:
  // bounding box of clip1: rect 0, 0 300x100, transform = identity;
  // bounding box of iframe itself: rect -100*sqrt(2), 0 200*sqrt(2)x200*sqrt(2)
  // transform: rotateZ(45).
  // In root space the two clips accumulates to:
  //   rect 0, 0 100*sqrt(2)x100, transform=identity
  // Transform this to layer's local space, the clip rect is:
  //   rect 0, -100/sqrt(2) (100+100/sqrt(2))x(100/sqrt(2))
  // So the intersected visible layer rect is:
  //   rect 0, 0, (100+100/sqrt(2)), 100/sqrt(2).
  // +----------------300px--------------+
  // |\                                  |
  // |  \                                |
  // |    \x                            100px
  // |   /  \                            |
  // | /y     \                          |
  // +-----------------------------------+
  gfx::Transform expected_transform;
  expected_transform.RotateAboutZAxis(-45);
    // The clip tree built by BlinkGenPropertyTrees is different from that build
    // by cc. While it does not affect correctness of hit testing, the hit test
    // region with kHitTestAsk will have a different size due to the change of
    // accumulated clips.
  gfx::Rect expected_region1 = gfx::ScaleToEnclosingRect(
      gfx::Rect(200, 100 / 1.414f), device_scale_factor, device_scale_factor);
  gfx::Rect expected_region2 =
      gfx::ScaleToEnclosingRect(gfx::Rect(100 + 100 / 1.414f, 100 / 1.414f),
                                device_scale_factor, device_scale_factor);

  // Since iframe is clipped into an octagon, we expect to do slow path hit
  // test on the iframe.
  DCHECK(hit_test_data.size() >= 3);
  EXPECT_TRUE(expected_region1.ApproximatelyEqual(hit_test_data[2].rect,
                                                  1 + device_scale_factor) ||
              expected_region2.ApproximatelyEqual(hit_test_data[2].rect,
                                                  1 + device_scale_factor));
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestDataGenerationBrowserTest,
                       ClipPathOOPIF) {
  auto hit_test_data =
      SetupAndGetHitTestData("/frame_tree/page_with_clip_path_iframe.html");
  float device_scale_factor = current_device_scale_factor();
  gfx::Transform expected_transform;
  gfx::Rect expected_region1 = gfx::ScaleToEnclosingRect(
      gfx::Rect(100, 100), device_scale_factor, device_scale_factor);
  gfx::Rect expected_region2 = gfx::ScaleToEnclosingRect(
      gfx::Rect(80, 80), device_scale_factor, device_scale_factor);

  // Since iframe is clipped into an irregular quadrilateral, we expect to do
  // slow path hit test on the iframe.
  DCHECK(hit_test_data.size() >= 3);
  // When BlinkGenPropertyTrees is enabled, the visible rect calculated for the
  // OOPIF is different to that when BlinkGenPropertyTrees is disabled. So the
  // test is considered passed if either of the regions equals to hit test
  // region.
  EXPECT_TRUE(expected_region1.ApproximatelyEqual(hit_test_data[2].rect,
                                                  1 + device_scale_factor) ||
              expected_region2.ApproximatelyEqual(hit_test_data[2].rect,
                                                  1 + device_scale_factor));
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestDataGenerationBrowserTest,
                       OverlappedOOPIF) {
  auto hit_test_data =
      SetupAndGetHitTestData("/frame_tree/page_with_overlapped_iframes.html");
  float device_scale_factor = current_device_scale_factor();
  gfx::Transform expected_transform1;
  gfx::Transform expected_transform2;
  expected_transform2.PostTranslate(-100 * device_scale_factor, 0);

  gfx::Rect expected_region = gfx::ScaleToEnclosingRect(
      gfx::Rect(100, 100), device_scale_factor, device_scale_factor);

  // Since iframe is occluded by a div in parent frame, we expect to do slow hit
  // test.
  DCHECK(hit_test_data.size() >= 4);
  EXPECT_EQ(expected_region.ToString(), hit_test_data[3].rect.ToString());
  EXPECT_TRUE(
      expected_transform1.ApproximatelyEqual(hit_test_data[3].transform));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[3].flags);
  EXPECT_EQ(expected_region.ToString(), hit_test_data[2].rect.ToString());
  EXPECT_TRUE(
      expected_transform2.ApproximatelyEqual(hit_test_data[2].transform));
  EXPECT_EQ(kFastHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestDataGenerationBrowserTest,
                       MaskedOOPIF) {
  auto hit_test_data =
      SetupAndGetHitTestData("/frame_tree/page_with_masked_iframe.html");
  float device_scale_factor = current_device_scale_factor();
  gfx::Transform expected_transform;
  gfx::Rect expected_region = gfx::ScaleToEnclosingRect(
      gfx::Rect(200, 200), device_scale_factor, device_scale_factor);

  // Since iframe clipped by clip-path and has a mask layer, we expect to do
  // slow path hit testing.
  DCHECK(hit_test_data.size() >= 3);
  EXPECT_EQ(expected_region.ToString(), hit_test_data[2].rect.ToString());
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestDataGenerationBrowserTest,
                       AncestorMaskedOOPIF) {
  auto hit_test_data = SetupAndGetHitTestData(
      "/frame_tree/page_with_ancestor_masked_iframe.html");
  float device_scale_factor = current_device_scale_factor();
  gfx::Transform expected_transform;
  gfx::Rect expected_region = gfx::ScaleToEnclosingRect(
      gfx::Rect(100, 100), device_scale_factor, device_scale_factor);

  // Since iframe clipped by clip-path and has a mask layer, we expect to do
  // slow path hit testing.
  DCHECK(hit_test_data.size() >= 3);
  EXPECT_EQ(expected_region.ToString(), hit_test_data[2].rect.ToString());
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestDataGenerationBrowserTest,
                       PointerEventsNoneOOPIF) {
  auto hit_test_data = SetupAndGetHitTestData(
      "/frame_tree/page_with_positioned_frame_pointer-events_none.html");
  float device_scale_factor = current_device_scale_factor();
  gfx::Transform expected_transform;
  gfx::Rect expected_region = gfx::ScaleToEnclosingRect(
      gfx::Rect(1, 1), device_scale_factor, device_scale_factor);
  expected_transform.Translate(-2 * device_scale_factor,
                               -2 * device_scale_factor);
  gfx::Rect expected_region2 = gfx::ScaleToEnclosingRect(
      gfx::Rect(100, 100), device_scale_factor, device_scale_factor);
  gfx::Transform expected_transform2;
  expected_transform2.Translate(-52 * device_scale_factor,
                                -52 * device_scale_factor);

  // We submit hit test region for OOPIFs with pointer-events: none, and mark
  // them as kHitTestIgnore.
  uint32_t flags = kFastHitTestFlags;

  DCHECK(hit_test_data.size() == 4);
  EXPECT_EQ(expected_region2.ToString(), hit_test_data[3].rect.ToString());
  EXPECT_TRUE(
      expected_transform2.ApproximatelyEqual(hit_test_data[3].transform));
  EXPECT_EQ(flags | viz::HitTestRegionFlags::kHitTestIgnore,
            hit_test_data[3].flags);

  EXPECT_EQ(expected_region.ToString(), hit_test_data[2].rect.ToString());
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform));
  EXPECT_EQ(flags, hit_test_data[2].flags);

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  ASSERT_EQ(2U, root->child_count());
  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  HitTestRegionObserver hit_test_data_change_observer(
      rwhv_root->GetRootFrameSinkId());
  hit_test_data_change_observer.WaitForHitTestData();

  // Check that an update on the css property can trigger an update in submitted
  // hit test data.
  EXPECT_TRUE(ExecJs(web_contents(),
                     "document.getElementsByTagName('iframe')[0].style."
                     "pointerEvents = 'auto';\n"));
  MainThreadFrameObserver observer(
      root->current_frame_host()->GetRenderWidgetHost());
  observer.Wait();

  hit_test_data_change_observer.WaitForHitTestDataChange();
  hit_test_data = hit_test_data_change_observer.GetHitTestData();

  ASSERT_EQ(4u, hit_test_data.size());
  EXPECT_EQ(expected_region.ToString(), hit_test_data[2].rect.ToString());
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform));
  EXPECT_EQ(kFastHitTestFlags, hit_test_data[2].flags);

  EXPECT_EQ(expected_region2.ToString(), hit_test_data[3].rect.ToString());
  EXPECT_TRUE(
      expected_transform2.ApproximatelyEqual(hit_test_data[3].transform));
  // Hit test region with pointer-events: none is marked as kHitTestIgnore. The
  // JavaScript above sets the element's pointer-events to 'auto' therefore
  // kHitTestIgnore should be removed from the flag.
  EXPECT_EQ(kFastHitTestFlags, hit_test_data[3].flags);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessHitTestDataGenerationBrowserTest,
                       OccludedOOPIF) {
  auto hit_test_data =
      SetupAndGetHitTestData("/frame_tree/page_with_occluded_iframes.html");
  float device_scale_factor = current_device_scale_factor();
  gfx::Transform expected_transform1;
  gfx::Transform expected_transform2;
  expected_transform2.Translate(-110 * device_scale_factor, 0);

  // We should not skip OOPIFs that are occluded by parent frame elements, since
  // in cc an element's bound may not be its hit test area.
  DCHECK(hit_test_data.size() == 4);
  EXPECT_TRUE(ApproximatelyEqual(
      TransformRectToQuadF(gfx::Rect(100, 100), expected_transform1),
      TransformRectToQuadF(hit_test_data[3])));
  if (base::FeatureList::IsEnabled(blink::features::kHitTestOpaqueness)) {
    // The iframe is behind a pointer-events:none div. Because the div is
    // transparent to hit test, the iframe surface can handle hit tests
    // directly as if the div didn't exist.
    EXPECT_EQ(kFastHitTestFlags, hit_test_data[3].flags);
  } else {
    EXPECT_EQ(kSlowHitTestFlags, hit_test_data[3].flags);
  }

  EXPECT_TRUE(ApproximatelyEqual(
      TransformRectToQuadF(gfx::Rect(100, 100), expected_transform2),
      TransformRectToQuadF(hit_test_data[2])));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

#if defined(USE_AURA)
using SitePerProcessDelegatedInkBrowserTest = SitePerProcessHitTestBrowserTest;

// Test confirms that a point hitting an OOPIF that is requesting delegated ink
// trails results in the metadata being correctly sent to the child's
// RenderWidgetHost and is usable for sending delegated ink points.
// TODO(crbug.com/40835227): Fix and enable the test on Fuchsia.
// TODO(crbug.com/40935254): flaky on ChromeOS
// TODO(http://b/331190208): Test failing on Linux
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_MetadataAndPointGoThroughOOPIF \
  DISABLED_MetadataAndPointGoThroughOOPIF
#else
#define MAYBE_MetadataAndPointGoThroughOOPIF MetadataAndPointGoThroughOOPIF
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessDelegatedInkBrowserTest,
                       MAYBE_MetadataAndPointGoThroughOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);

  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Make sure the child frame is indeed a OOPIF
  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());

  EXPECT_TRUE(ExecJs(child->current_frame_host(), R"(
      let presenter = null;
      navigator.ink.requestPresenter().then(e => { presenter = e; });
      let style = { color: 'green', diameter: 21 };

      window.addEventListener('pointermove' , evt => {
        presenter.updateInkTrailStartPoint(evt, style);
        document.write('Force a new frame so that an updated ' +
        'RenderFrameMetadata is sent to the browser process.');
      });
      )"));

  RenderWidgetHostImpl* root_rwh =
      root->current_frame_host()->GetRenderWidgetHost();
  RenderWidgetHostImpl* child_rwh =
      child->current_frame_host()->GetRenderWidgetHost();

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(root_rwh);
  RenderWidgetHostMouseEventMonitor child_frame_monitor(child_rwh);

  WaitForHitTestData(child->current_frame_host());

  RenderWidgetHostViewBase* root_view =
      static_cast<RenderWidgetHostViewBase*>(root_rwh->GetView());
  RenderWidgetHostViewBase* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_rwh->GetView());

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  EXPECT_FALSE(web_contents()->IsDelegatedInkRendererBoundForTest());

  // Target MouseMove to child frame.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&mouse_event, gfx::Point(55, 55), root_view);

  RouteMouseEventAndWaitUntilDispatch(router, root_view, rwhv_child,
                                      &mouse_event);

  // Dispatch twice because the router generates an extra MouseLeave for the
  // main frame.
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  RouteMouseEventAndWaitUntilDispatch(router, root_view, rwhv_child,
                                      &mouse_event);
  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());

  RunUntilInputProcessed(root_rwh);

  // Confirm that the metadata is what we expect and accessible from the child's
  // RenderWidgetHost.
  const cc::RenderFrameMetadata& last_metadata =
      child_rwh->render_frame_metadata_provider()->LastRenderFrameMetadata();
  EXPECT_TRUE(last_metadata.delegated_ink_metadata.has_value());
  EXPECT_TRUE(
      last_metadata.delegated_ink_metadata.value().delegated_ink_is_hovering);

  // Send one more mouse move event and confirm that it causes the forwarding
  // to occur, which will result in the |delegated_ink_point_renderer_| mojom
  // remote being bound.
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  SetWebEventPositions(&mouse_event, gfx::Point(57, 57), root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, rwhv_child,
                                      &mouse_event);

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_TRUE(web_contents()->IsDelegatedInkRendererBoundForTest());
}
#endif  // USE_AURA

}  // namespace content
