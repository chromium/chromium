// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/render_widget_host_input_event_router.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/input/render_widget_targeter.h"
#include "components/viz/common/hit_test/hit_test_query.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/test/host_frame_sink_manager_test_api.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/site_instance_group.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_image_transport_factory.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/hit_test/input_target_client.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom.h"

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/mojom/delegated_ink_point_renderer.mojom.h"
#endif  // defined(USE_AURA)

namespace content {

namespace {

class MockFrameConnector : public CrossProcessFrameConnector {
 public:
  MockFrameConnector(RenderWidgetHostViewChildFrame* view,
                     RenderWidgetHostViewBase* parent_view,
                     RenderWidgetHostViewBase* root_view)
      : CrossProcessFrameConnector(nullptr),
        parent_view_(parent_view),
        root_view_(root_view) {
    view_ = view;
    view_->SetFrameConnector(this);
  }

  MockFrameConnector(const MockFrameConnector&) = delete;
  MockFrameConnector& operator=(const MockFrameConnector&) = delete;

  ~MockFrameConnector() override {
    if (view_) {
      view_->SetFrameConnector(nullptr);
      view_ = nullptr;
    }
  }

  RenderWidgetHostViewBase* GetParentRenderWidgetHostView() override {
    return parent_view_;
  }

  RenderWidgetHostViewBase* GetRootRenderWidgetHostView() override {
    return root_view_;
  }

 private:
  raw_ptr<RenderWidgetHostViewBase> parent_view_ = nullptr;
  raw_ptr<RenderWidgetHostViewBase> root_view_ = nullptr;
};

class StubHitTestQuery : public viz::HitTestQuery {
 public:
  StubHitTestQuery(RenderWidgetHostViewBase* hittest_result,
                   bool query_renderer)
      : HitTestQuery(std::nullopt),
        hittest_result_(hittest_result->GetWeakPtr()),
        query_renderer_(query_renderer) {}
  ~StubHitTestQuery() override = default;

  viz::Target FindTargetForLocationStartingFromImpl(
      viz::EventSource event_source,
      const gfx::PointF& location,
      const viz::FrameSinkId& sink_id,
      bool is_location_relative_to_parent) const override {
    CHECK(hittest_result_);

    return {hittest_result_->GetFrameSinkId(), gfx::PointF(),
            viz::HitTestRegionFlags::kHitTestMouse |
                viz::HitTestRegionFlags::kHitTestTouch |
                viz::HitTestRegionFlags::kHitTestMine |
                (query_renderer_ ? viz::HitTestRegionFlags::kHitTestAsk : 0)};
  }

 private:
  base::WeakPtr<const RenderWidgetHostViewBase> hittest_result_;
  const bool query_renderer_;
};

// The RenderWidgetHostInputEventRouter uses the root RWHV for hittesting, so
// here we stub out the hittesting logic so we can control which RWHV will be
// the result of a hittest by the RWHIER. Note that since the hittesting is
// stubbed out, the event coordinates and view bounds are irrelevant for these
// tests.
class MockRootRenderWidgetHostView : public TestRenderWidgetHostView {
 public:
  MockRootRenderWidgetHostView(RenderWidgetHost* rwh)
      : TestRenderWidgetHostView(rwh) {}
  ~MockRootRenderWidgetHostView() override = default;

  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      input::RenderWidgetHostViewInput* target_view,
      gfx::PointF* transformed_point) override {
    return true;
  }

  void ProcessGestureEvent(const blink::WebGestureEvent& event,
                           const ui::LatencyInfo&) override {
    last_gesture_seen_ = event.GetType();
  }

  void ProcessAckedTouchEvent(
      const input::TouchEventWithLatencyInfo& touch,
      blink::mojom::InputEventResultState ack_result) override {
    unique_id_for_last_touch_ack_ = touch.event.unique_touch_event_id;
  }

  viz::FrameSinkId GetRootFrameSinkId() override { return GetFrameSinkId(); }

  blink::WebInputEvent::Type last_gesture_seen() { return last_gesture_seen_; }
  uint32_t last_id_for_touch_ack() { return unique_id_for_last_touch_ack_; }

  void SetHittestResult(RenderWidgetHostViewBase* result_view,
                        bool query_renderer) {
    DCHECK(GetHostFrameSinkManager());

    viz::DisplayHitTestQueryMap hit_test_map;
    hit_test_map[GetFrameSinkId()] =
        std::make_unique<StubHitTestQuery>(result_view, query_renderer);

    viz::HostFrameSinkManagerTestApi(GetHostFrameSinkManager())
        .SetDisplayHitTestQuery(std::move(hit_test_map));
  }

  void Reset() { last_gesture_seen_ = blink::WebInputEvent::Type::kUndefined; }

 private:
  blink::WebInputEvent::Type last_gesture_seen_ =
      blink::WebInputEvent::Type::kUndefined;
  uint32_t unique_id_for_last_touch_ack_ = 0;
};

class MockInputTargetClient : public viz::mojom::InputTargetClient {
 public:
  explicit MockInputTargetClient(
      mojo::PendingReceiver<viz::mojom::InputTargetClient> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}
  ~MockInputTargetClient() override = default;

  // viz::mojom::InputTargetClient:
  void FrameSinkIdAt(const gfx::PointF& point,
                     const uint64_t trace_id,
                     FrameSinkIdAtCallback callback) override {
    if (forward_callback_) {
      std::move(forward_callback_).Run(std::move(callback));
    } else if (callback) {
      std::move(callback).Run(viz::FrameSinkId(), point);
    }
  }

  base::OnceCallback<void(FrameSinkIdAtCallback)> forward_callback_;
  mojo::Receiver<viz::mojom::InputTargetClient> receiver_;
};

}  // namespace

class RenderWidgetHostInputEventRouterTest : public testing::Test {
 public:
  RenderWidgetHostInputEventRouterTest(
      const RenderWidgetHostInputEventRouterTest&) = delete;
  RenderWidgetHostInputEventRouterTest& operator=(
      const RenderWidgetHostInputEventRouterTest&) = delete;

 protected:
  RenderWidgetHostInputEventRouterTest() = default;

  input::RenderWidgetHostInputEventRouter* rwhier() {
    return delegate_->GetInputEventRouter();
  }

  // testing::Test:
  void SetUp() override {
    browser_context_ = std::make_unique<TestBrowserContext>();
    delegate_ = std::make_unique<MockRenderWidgetHostDelegate>();

// ImageTransportFactory doesn't exist on Android. This is needed to create
// a RenderWidgetHostViewChildFrame in the test.
#if !BUILDFLAG(IS_ANDROID)
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
#endif

    delegate_->CreateInputEventRouter();

    process_host_root_ =
        std::make_unique<MockRenderProcessHost>(browser_context_.get());
    site_instance_group_root_ =
        base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
            browser_context_.get(), process_host_root_.get()));
    auto routing_id = process_host_root_->GetNextRoutingID();
    widget_host_root_ = RenderWidgetHostFactory::Create(
        /*frame_tree=*/nullptr, delegate_.get(),
        RenderWidgetHostImpl::DefaultFrameSinkId(*site_instance_group_root_,
                                                 routing_id),
        site_instance_group_root_->GetSafeRef(), routing_id,
        /*hidden=*/false, /*renderer_initiated_creation=*/false);
    widget_host_root_->SetViewIsFrameSinkIdOwner(true);

    mojo::AssociatedRemote<blink::mojom::WidgetHost> blink_widget_host;
    mojo::AssociatedRemote<blink::mojom::Widget> blink_widget;
    auto blink_widget_receiver =
        blink_widget.BindNewEndpointAndPassDedicatedReceiver();
    widget_host_root_->BindWidgetInterfaces(
        blink_widget_host.BindNewEndpointAndPassDedicatedReceiver(),
        blink_widget.Unbind());

    mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> frame_widget_host;
    mojo::AssociatedRemote<blink::mojom::FrameWidget> frame_widget;
    auto frame_widget_receiver =
        frame_widget.BindNewEndpointAndPassDedicatedReceiver();
    widget_host_root_->BindFrameWidgetInterfaces(
        frame_widget_host.BindNewEndpointAndPassDedicatedReceiver(),
        frame_widget.Unbind());

    view_root_ =
        std::make_unique<MockRootRenderWidgetHostView>(widget_host_root_.get());

    // We need to set up a comm pipe, or else the targeter will crash when it
    // tries to query the renderer. It doesn't matter that the pipe isn't
    // connected on the other end, as we really don't want it to respond
    // anyways.
    mojo::Remote<viz::mojom::InputTargetClient> input_target_client;
    input_target_client_root_ = std::make_unique<MockInputTargetClient>(
        input_target_client.BindNewPipeAndPassReceiver());
    widget_host_root_->GetRenderInputRouter()->SetInputTargetClientForTesting(
        std::move(input_target_client));

    EXPECT_EQ(view_root_.get(),
              rwhier()->FindViewFromFrameSinkId(view_root_->GetFrameSinkId()));
  }

  struct ChildViewState {
    std::unique_ptr<MockRenderProcessHost> process_host;
    scoped_refptr<SiteInstanceGroup> site_instance_group;
    std::unique_ptr<RenderWidgetHostImpl> widget_host;
    std::unique_ptr<TestRenderWidgetHostViewChildFrame> view;
    std::unique_ptr<MockFrameConnector> frame_connector;

    ChildViewState() = default;
    ChildViewState(ChildViewState&&) = default;
    ~ChildViewState() { process_host->Cleanup(); }
  };

  ChildViewState MakeChildView(RenderWidgetHostViewBase* parent_view) {
    ChildViewState child;

    child.process_host =
        std::make_unique<MockRenderProcessHost>(browser_context_.get());
    child.site_instance_group =
        base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
            site_instance_group_root_.get(), child.process_host.get()));
    auto routing_id = child.process_host->GetNextRoutingID();
    child.widget_host = RenderWidgetHostFactory::Create(
        /*frame_tree=*/nullptr, delegate_.get(),
        RenderWidgetHostImpl::DefaultFrameSinkId(*child.site_instance_group,
                                                 routing_id),
        child.site_instance_group->GetSafeRef(), routing_id,
        /*hidden=*/false, /*renderer_initiated_creation=*/false);
    child.widget_host->SetViewIsFrameSinkIdOwner(true);
    child.view = std::make_unique<TestRenderWidgetHostViewChildFrame>(
        child.widget_host.get());
    child.frame_connector = std::make_unique<MockFrameConnector>(
        child.view.get(), parent_view, view_root_.get());

    EXPECT_EQ(child.view.get(),
              rwhier()->FindViewFromFrameSinkId(child.view->GetFrameSinkId()));

    return child;
  }

  void TearDown() override {
    view_root_.reset();
    widget_host_root_.reset();
    process_host_root_->Cleanup();
    site_instance_group_root_.reset();
    process_host_root_.reset();
    delegate_.reset();

    base::RunLoop().RunUntilIdle();

#if !BUILDFLAG(IS_ANDROID)
    ImageTransportFactory::Terminate();
#endif
  }

  input::RenderWidgetHostViewInput* touch_target() {
    return rwhier()->touch_target_;
  }
  input::RenderWidgetHostViewInput* touchscreen_gesture_target() {
    return rwhier()->touchscreen_gesture_target_.get();
  }
  input::RenderWidgetHostViewInput* bubbling_gesture_scroll_origin() {
    return rwhier()->bubbling_gesture_scroll_origin_;
  }
  input::RenderWidgetHostViewInput* bubbling_gesture_scroll_target() {
    return rwhier()->bubbling_gesture_scroll_target_;
  }

  void TestSendNewGestureWhileBubbling(
      TestRenderWidgetHostViewChildFrame* bubbling_origin,
      RenderWidgetHostViewBase* gesture_target,
      bool should_cancel);

  void FlushInkRenderer() { delegate_->FlushInkRenderer(); }

  BrowserTaskEnvironment task_environment_;

  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;
  std::unique_ptr<BrowserContext> browser_context_;

  std::unique_ptr<MockRenderProcessHost> process_host_root_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_root_;
  std::unique_ptr<RenderWidgetHostImpl> widget_host_root_;
  std::unique_ptr<MockRootRenderWidgetHostView> view_root_;
  std::unique_ptr<MockInputTargetClient> input_target_client_root_;
};

// Make sure that when a touch scroll crosses out of the area for a
// RenderWidgetHostView, the RenderWidgetHostInputEventRouter continues to
// route gesture events to the same RWHV until the end of the gesture.
// See crbug.com/739831
TEST_F(RenderWidgetHostInputEventRouterTest,
       DoNotChangeTargetViewDuringTouchScrollGesture) {
  ChildViewState child = MakeChildView(view_root_.get());
  // Simulate the touch and gesture events produced from scrolling on a
  // touchscreen.

  // We start the touch in the area for |child.view|.
  view_root_->SetHittestResult(child.view.get(), false);

  blink::WebTouchEvent touch_event(
      blink::WebInputEvent::Type::kTouchStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_event.touches_length = 1;
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStatePressed;
  touch_event.unique_touch_event_id = 1;

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  EXPECT_EQ(child.view.get(), touch_target());

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;

  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());
  EXPECT_EQ(child.view.get(), touchscreen_gesture_target());
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureTapDown,
            child.view->last_gesture_seen());
  EXPECT_NE(blink::WebInputEvent::Type::kGestureTapDown,
            view_root_->last_gesture_seen());

  touch_event.SetType(blink::WebInputEvent::Type::kTouchMove);
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStateMoved;
  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureTapCancel);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollBegin);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollUpdate);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());

  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());

  // The continuation of the touch moves should maintain their current target of
  // |child.view|, even if they move outside of that view, and into
  // |view_root_|.
  // If the target is maintained through the gesture this will pass. If instead
  // the hit testing logic is refered to, then this test will fail.
  view_root_->SetHittestResult(view_root_.get(), false);
  view_root_->Reset();
  child.view->Reset();

  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());

  EXPECT_EQ(child.view.get(), touch_target());
  EXPECT_EQ(child.view.get(), touchscreen_gesture_target());
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            child.view->last_gesture_seen());
  EXPECT_NE(blink::WebInputEvent::Type::kGestureScrollUpdate,
            view_root_->last_gesture_seen());

  touch_event.SetType(blink::WebInputEvent::Type::kTouchEnd);
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStateReleased;
  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollEnd);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());

  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            child.view->last_gesture_seen());
  EXPECT_NE(blink::WebInputEvent::Type::kGestureScrollEnd,
            view_root_->last_gesture_seen());
}

TEST_F(RenderWidgetHostInputEventRouterTest,
       EnsureRendererDestroyedHandlesUnAckedTouchEvents) {
  ChildViewState child = MakeChildView(view_root_.get());

  // Tell the child that it has event handlers, to prevent the touch event
  // queue in the renderer host from acking the touch events immediately.
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      /*has_touch_event_handlers=*/true, /*has_hit_testable_scrollbar=*/false);
  child.widget_host->SetHasTouchEventConsumers(
      std::move(touch_event_consumers));

  // Make sure we route touch events to child. This will cause the RWH's
  // InputRouter to IPC the event into the ether, from which it will never
  // return, which is perfect for this test.
  view_root_->SetHittestResult(child.view.get(), false);

  // Send a TouchStart/End sequence.
  blink::WebTouchEvent touch_start_event(
      blink::WebInputEvent::Type::kTouchStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_start_event.touches_length = 1;
  touch_start_event.touches[0].state =
      blink::WebTouchPoint::State::kStatePressed;
  touch_start_event.unique_touch_event_id = 1;

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_start_event,
                            ui::LatencyInfo());

  blink::WebTouchEvent touch_end_event(
      blink::WebInputEvent::Type::kTouchEnd, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_end_event.touches_length = 1;
  touch_end_event.touches[0].state =
      blink::WebTouchPoint::State::kStateReleased;
  touch_end_event.unique_touch_event_id = 2;

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_end_event,
                            ui::LatencyInfo());

  // Make sure both touch events were added to the TEAQ.
  EXPECT_EQ(2u, rwhier()->TouchEventAckQueueLengthForTesting());

  // Now, tell the router that the child view was destroyed, and verify the
  // acks.
  rwhier()->OnRenderWidgetHostViewInputDestroyed(child.view.get());
  EXPECT_EQ(view_root_->last_id_for_touch_ack(), 2lu);
  EXPECT_EQ(0u, rwhier()->TouchEventAckQueueLengthForTesting());
}

// Ensure that when RenderWidgetHostInputEventRouter receives an unexpected
// touch event, it calls the root view's method to Ack the event before
// dropping it.
TEST_F(RenderWidgetHostInputEventRouterTest, EnsureDroppedTouchEventsAreAcked) {
  // Send a touch move without a touch start.
  blink::WebTouchEvent touch_move_event(
      blink::WebInputEvent::Type::kTouchMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_move_event.touches_length = 1;
  touch_move_event.touches[0].state =
      blink::WebTouchPoint::State::kStatePressed;
  touch_move_event.unique_touch_event_id = 1;

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_move_event,
                            ui::LatencyInfo());
  EXPECT_EQ(view_root_->last_id_for_touch_ack(), 1lu);

  // Send a touch cancel without a touch start.
  blink::WebTouchEvent touch_cancel_event(
      blink::WebInputEvent::Type::kTouchCancel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_cancel_event.touches_length = 1;
  touch_cancel_event.touches[0].state =
      blink::WebTouchPoint::State::kStateCancelled;
  touch_cancel_event.unique_touch_event_id = 2;

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_cancel_event,
                            ui::LatencyInfo());
  EXPECT_EQ(view_root_->last_id_for_touch_ack(), 2lu);
}

TEST_F(RenderWidgetHostInputEventRouterTest, DoNotCoalesceTouchEvents) {
  // We require the presence of a child view, otherwise targeting is short
  // circuited.
  ChildViewState child = MakeChildView(view_root_.get());

  input::RenderWidgetTargeter* targeter =
      rwhier()->GetRenderWidgetTargeterForTests();
  view_root_->SetHittestResult(view_root_.get(), true);

  // Send TouchStart, TouchMove, TouchMove, TouchMove, TouchEnd and make sure
  // the targeter doesn't attempt to coalesce.
  blink::WebTouchEvent touch_event(
      blink::WebInputEvent::Type::kTouchStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_event.touches_length = 1;
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStatePressed;
  touch_event.unique_touch_event_id = 1;

  EXPECT_EQ(0u, targeter->num_requests_in_queue_for_testing());
  EXPECT_FALSE(targeter->is_request_in_flight_for_testing());
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  EXPECT_EQ(0u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  touch_event.SetType(blink::WebInputEvent::Type::kTouchMove);
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStateMoved;
  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  EXPECT_EQ(1u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  EXPECT_EQ(2u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  touch_event.SetType(blink::WebInputEvent::Type::kTouchEnd);
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStateReleased;
  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());

  EXPECT_EQ(3u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());
}

TEST_F(RenderWidgetHostInputEventRouterTest, DoNotCoalesceGestureEvents) {
  // We require the presence of a child view, otherwise targeting is short
  // circuited.
  ChildViewState child = MakeChildView(view_root_.get());

  input::RenderWidgetTargeter* targeter =
      rwhier()->GetRenderWidgetTargeterForTests();
  view_root_->SetHittestResult(view_root_.get(), true);

  // Send TouchStart, GestureTapDown, TouchEnd, GestureScrollBegin,
  // GestureScrollUpdate (x2), GestureScrollEnd and make sure
  // the targeter doesn't attempt to coalesce.
  blink::WebTouchEvent touch_event(
      blink::WebInputEvent::Type::kTouchStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_event.touches_length = 1;
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStatePressed;
  touch_event.unique_touch_event_id = 1;

  EXPECT_EQ(0u, targeter->num_requests_in_queue_for_testing());
  EXPECT_FALSE(targeter->is_request_in_flight_for_testing());
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  EXPECT_EQ(0u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());
  EXPECT_EQ(1u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  touch_event.SetType(blink::WebInputEvent::Type::kTouchEnd);
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStateReleased;
  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  EXPECT_EQ(2u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollBegin);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());
  EXPECT_EQ(3u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollUpdate);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());
  EXPECT_EQ(4u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollUpdate);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());
  EXPECT_EQ(5u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollEnd);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());
  EXPECT_EQ(6u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());
}

// Test that when a child view involved in scroll bubbling detaches, scroll
// bubbling is canceled.
TEST_F(RenderWidgetHostInputEventRouterTest,
       CancelScrollBubblingWhenChildDetaches) {
  gfx::Vector2dF delta(0.f, 10.f);
  blink::WebGestureEvent scroll_begin =
      blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
          delta.x(), delta.y(), blink::WebGestureDevice::kTouchscreen);

  {
    ChildViewState child = MakeChildView(view_root_.get());

    ASSERT_TRUE(rwhier()->BubbleScrollEvent(view_root_.get(), child.view.get(),
                                            scroll_begin));
    EXPECT_EQ(child.view.get(), bubbling_gesture_scroll_origin());
    EXPECT_EQ(view_root_.get(), bubbling_gesture_scroll_target());
    EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
              view_root_->last_gesture_seen());

    rwhier()->WillDetachChildView(child.view.get());
    EXPECT_EQ(nullptr, bubbling_gesture_scroll_origin());
    EXPECT_EQ(nullptr, bubbling_gesture_scroll_target());
    EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
              view_root_->last_gesture_seen());
  }

  {
    ChildViewState outer = MakeChildView(view_root_.get());
    ChildViewState inner = MakeChildView(outer.view.get());

    ASSERT_TRUE(rwhier()->BubbleScrollEvent(outer.view.get(), inner.view.get(),
                                            scroll_begin));
    EXPECT_EQ(inner.view.get(), bubbling_gesture_scroll_origin());
    EXPECT_EQ(outer.view.get(), bubbling_gesture_scroll_target());
    EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
              outer.view->last_gesture_seen());
    ASSERT_TRUE(rwhier()->BubbleScrollEvent(view_root_.get(), outer.view.get(),
                                            scroll_begin));
    EXPECT_EQ(inner.view.get(), bubbling_gesture_scroll_origin());
    EXPECT_EQ(view_root_.get(), bubbling_gesture_scroll_target());
    EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
              outer.view->last_gesture_seen());
    EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
              view_root_->last_gesture_seen());

    rwhier()->WillDetachChildView(outer.view.get());
    EXPECT_EQ(nullptr, bubbling_gesture_scroll_origin());
    EXPECT_EQ(nullptr, bubbling_gesture_scroll_target());
    EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
              view_root_->last_gesture_seen());
  }
}

// Test that when a child view that is irrelevant to any ongoing scroll
// bubbling detaches, scroll bubbling is not canceled.
TEST_F(RenderWidgetHostInputEventRouterTest,
       ContinueScrollBubblingWhenIrrelevantChildDetaches) {
  gfx::Vector2dF delta(0.f, 10.f);
  blink::WebGestureEvent scroll_begin =
      blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
          delta.x(), delta.y(), blink::WebGestureDevice::kTouchscreen);

  ChildViewState outer = MakeChildView(view_root_.get());
  ChildViewState inner = MakeChildView(outer.view.get());

  ASSERT_TRUE(rwhier()->BubbleScrollEvent(view_root_.get(), outer.view.get(),
                                          scroll_begin));
  EXPECT_EQ(outer.view.get(), bubbling_gesture_scroll_origin());
  EXPECT_EQ(view_root_.get(), bubbling_gesture_scroll_target());
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            view_root_->last_gesture_seen());

  rwhier()->WillDetachChildView(inner.view.get());
  EXPECT_EQ(outer.view.get(), bubbling_gesture_scroll_origin());
  EXPECT_EQ(view_root_.get(), bubbling_gesture_scroll_target());
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            view_root_->last_gesture_seen());
}

// Ensure filtered scroll events while a scroll bubble is in progress don't
// affect the scroll bubbling state.
TEST_F(RenderWidgetHostInputEventRouterTest,
       FilteredGestureDoesntInterruptBubbling) {
  gfx::Vector2dF delta(0.f, 10.f);
  blink::WebGestureEvent scroll_begin =
      blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
          delta.x(), delta.y(), blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_end =
      blink::SyntheticWebGestureEventBuilder::BuildScrollEnd(
          blink::WebGestureDevice::kTouchscreen);

  ChildViewState child = MakeChildView(view_root_.get());

  // Start a scroll that gets bubbled up from the child view.
  {
    ASSERT_FALSE(child.view.get()->is_scroll_sequence_bubbling_);

    child.view->GestureEventAck(
        scroll_begin, blink::mojom::InputEventResultSource::kCompositorThread,
        blink::mojom::InputEventResultState::kNotConsumed);

    EXPECT_EQ(child.view.get(), bubbling_gesture_scroll_origin());
    EXPECT_EQ(view_root_.get(), bubbling_gesture_scroll_target());
    ASSERT_TRUE(child.view->is_scroll_sequence_bubbling_);
  }

  // Simulate a debounce filtered GSE/GSB pair which looks like an ACK consumed
  // by the browser.
  {
    child.view->GestureEventAck(scroll_end,
                                blink::mojom::InputEventResultSource::kBrowser,
                                blink::mojom::InputEventResultState::kConsumed);
    child.view->GestureEventAck(scroll_begin,
                                blink::mojom::InputEventResultSource::kBrowser,
                                blink::mojom::InputEventResultState::kConsumed);

    EXPECT_EQ(child.view.get(), bubbling_gesture_scroll_origin());
    EXPECT_EQ(view_root_.get(), bubbling_gesture_scroll_target());
    EXPECT_TRUE(child.view->is_scroll_sequence_bubbling_);
  }

  // An unfiltered GSE should now clear state.
  {
    // Note: scroll end is always sent non-blocking which means the ACK comes
    // from the browser.
    child.view->GestureEventAck(scroll_end,
                                blink::mojom::InputEventResultSource::kBrowser,
                                blink::mojom::InputEventResultState::kIgnored);
    EXPECT_FALSE(child.view->is_scroll_sequence_bubbling_);
    EXPECT_EQ(bubbling_gesture_scroll_origin(), nullptr);
    EXPECT_EQ(bubbling_gesture_scroll_target(), nullptr);
  }

  // A new scroll should once again establish bubbling.
  {
    ASSERT_FALSE(child.view.get()->is_scroll_sequence_bubbling_);

    child.view->GestureEventAck(
        scroll_begin, blink::mojom::InputEventResultSource::kCompositorThread,
        blink::mojom::InputEventResultState::kNotConsumed);

    EXPECT_EQ(child.view.get(), bubbling_gesture_scroll_origin());
    EXPECT_EQ(view_root_.get(), bubbling_gesture_scroll_target());
    ASSERT_TRUE(child.view->is_scroll_sequence_bubbling_);
  }
}

void RenderWidgetHostInputEventRouterTest::TestSendNewGestureWhileBubbling(
    TestRenderWidgetHostViewChildFrame* bubbling_origin,
    RenderWidgetHostViewBase* gesture_target,
    bool should_cancel) {
  gfx::Vector2dF delta(0.f, 10.f);
  blink::WebGestureEvent scroll_begin =
      blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
          delta.x(), delta.y(), blink::WebGestureDevice::kTouchscreen);

  TestRenderWidgetHostViewChildFrame* cur_target = bubbling_origin;
  RenderWidgetHostViewBase* parent = bubbling_origin->GetParentViewInput();
  while (parent) {
    ASSERT_TRUE(rwhier()->BubbleScrollEvent(parent, cur_target, scroll_begin));
    EXPECT_EQ(bubbling_origin, bubbling_gesture_scroll_origin());
    EXPECT_EQ(parent, bubbling_gesture_scroll_target());
    if (cur_target != bubbling_origin) {
      EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
                cur_target->last_gesture_seen());
    }

    if (parent->IsRenderWidgetHostViewChildFrame()) {
      TestRenderWidgetHostViewChildFrame* next_child =
          static_cast<TestRenderWidgetHostViewChildFrame*>(parent);
      EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
                next_child->last_gesture_seen());
      cur_target = next_child;
      parent = next_child->GetParentViewInput();
    } else {
      MockRootRenderWidgetHostView* root =
          static_cast<MockRootRenderWidgetHostView*>(parent);
      EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
                root->last_gesture_seen());
      parent = nullptr;
    }
  }

  // While bubbling scroll, a new gesture is targeted to |gesture_target|.

  view_root_->SetHittestResult(gesture_target, false);

  blink::WebTouchEvent touch_event(
      blink::WebInputEvent::Type::kTouchStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_event.touches_length = 1;
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStatePressed;
  touch_event.unique_touch_event_id = 123;

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  EXPECT_EQ(gesture_target, touch_target());

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;

  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());
  EXPECT_EQ(gesture_target, touchscreen_gesture_target());

  if (should_cancel) {
    EXPECT_EQ(nullptr, bubbling_gesture_scroll_origin());
    EXPECT_EQ(nullptr, bubbling_gesture_scroll_target());
  } else {
    EXPECT_NE(nullptr, bubbling_gesture_scroll_origin());
    EXPECT_NE(nullptr, bubbling_gesture_scroll_target());
  }
}

// If we're bubbling scroll to a view and a new gesture is to be targeted to
// that view, cancel scroll bubbling, so that the view does not have multiple
// gestures happening at the same time.
TEST_F(RenderWidgetHostInputEventRouterTest,
       CancelBubblingOnNewGestureToBubblingTarget) {
  ChildViewState child = MakeChildView(view_root_.get());

  TestSendNewGestureWhileBubbling(child.view.get(), view_root_.get(), true);
}

// Like CancelBubblingOnNewGestureToBubblingTarget, but tests that we also
// cancel in the case of nested bubbling.
TEST_F(RenderWidgetHostInputEventRouterTest,
       CancelNestedBubblingOnNewGestureToBubblingTarget) {
  ChildViewState outer = MakeChildView(view_root_.get());
  ChildViewState inner = MakeChildView(outer.view.get());

  TestSendNewGestureWhileBubbling(inner.view.get(), view_root_.get(), true);
}

// If we're bubbling scroll and a new gesture is to be targeted to an
// intermediate bubbling target, cancel scroll bubbling.
TEST_F(RenderWidgetHostInputEventRouterTest,
       CancelNestedBubblingOnNewGestureToIntermediateTarget) {
  ChildViewState outer = MakeChildView(view_root_.get());
  ChildViewState inner = MakeChildView(outer.view.get());

  TestSendNewGestureWhileBubbling(inner.view.get(), outer.view.get(), true);
}

// If we're bubbling scroll, the child that is bubbling may receive a new
// gesture. Since this doesn't conflict with the bubbling, we should not
// cancel it.
TEST_F(RenderWidgetHostInputEventRouterTest,
       ContinueBubblingOnNewGestureToBubblingOrigin) {
  ChildViewState child = MakeChildView(view_root_.get());

  TestSendNewGestureWhileBubbling(child.view.get(), child.view.get(), false);
}

// If a view tries to bubble a scroll sequence while we are already bubbling
// a scroll sequence from another view, do not bubble the conflicting sequence.
TEST_F(RenderWidgetHostInputEventRouterTest, DoNotBubbleMultipleSequences) {
  gfx::Vector2dF delta(0.f, 10.f);
  blink::WebGestureEvent scroll_begin =
      blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
          delta.x(), delta.y(), blink::WebGestureDevice::kTouchscreen);

  ChildViewState outer1 = MakeChildView(view_root_.get());
  ChildViewState inner1 = MakeChildView(outer1.view.get());
  ChildViewState outer2 = MakeChildView(view_root_.get());
  ChildViewState inner2 = MakeChildView(outer2.view.get());

  ASSERT_TRUE(rwhier()->BubbleScrollEvent(outer1.view.get(), inner1.view.get(),
                                          scroll_begin));
  EXPECT_EQ(inner1.view.get(), bubbling_gesture_scroll_origin());
  EXPECT_EQ(outer1.view.get(), bubbling_gesture_scroll_target());
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            outer1.view->last_gesture_seen());

  EXPECT_FALSE(rwhier()->BubbleScrollEvent(outer2.view.get(), inner2.view.get(),
                                           scroll_begin));

  EXPECT_EQ(inner1.view.get(), bubbling_gesture_scroll_origin());
  EXPECT_EQ(outer1.view.get(), bubbling_gesture_scroll_target());
}

// If a view tries to bubble scroll and the target view has an unrelated
// gesture in progress, do not bubble the conflicting sequence.
TEST_F(RenderWidgetHostInputEventRouterTest,
       DoNotBubbleIfUnrelatedGestureInTarget) {
  gfx::Vector2dF delta(0.f, 10.f);
  blink::WebGestureEvent scroll_begin =
      blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
          delta.x(), delta.y(), blink::WebGestureDevice::kTouchscreen);

  ChildViewState child = MakeChildView(view_root_.get());

  view_root_->SetHittestResult(view_root_.get(), false);

  blink::WebTouchEvent touch_event(
      blink::WebInputEvent::Type::kTouchStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_event.touches_length = 1;
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStatePressed;
  touch_event.unique_touch_event_id = 123;

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  EXPECT_EQ(view_root_.get(), touch_target());

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;

  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());
  EXPECT_EQ(view_root_.get(), touchscreen_gesture_target());

  // Now that we have a gesture in |view_root_|, suppose that there was a
  // previous gesture in |child.view| that has resulted in a scroll which we
  // will now attempt to bubble.

  EXPECT_FALSE(rwhier()->BubbleScrollEvent(view_root_.get(), child.view.get(),
                                           scroll_begin));
  EXPECT_EQ(nullptr, bubbling_gesture_scroll_origin());
  EXPECT_EQ(nullptr, bubbling_gesture_scroll_target());
}

// Like DoNotBubbleIfUnrelatedGestureInTarget, but considers bubbling from a
// nested view.
TEST_F(RenderWidgetHostInputEventRouterTest,
       NestedDoNotBubbleIfUnrelatedGestureInTarget) {
  gfx::Vector2dF delta(0.f, 10.f);
  blink::WebGestureEvent scroll_begin =
      blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
          delta.x(), delta.y(), blink::WebGestureDevice::kTouchscreen);

  ChildViewState outer = MakeChildView(view_root_.get());
  ChildViewState inner = MakeChildView(outer.view.get());

  view_root_->SetHittestResult(view_root_.get(), false);

  blink::WebTouchEvent touch_event(
      blink::WebInputEvent::Type::kTouchStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_event.touches_length = 1;
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStatePressed;
  touch_event.unique_touch_event_id = 123;

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  EXPECT_EQ(view_root_.get(), touch_target());

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;

  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo());
  EXPECT_EQ(view_root_.get(), touchscreen_gesture_target());

  // Now that we have a gesture in |view_root_|, suppose that there was a
  // previous gesture in |inner.view| that has resulted in a scroll which we
  // will now attempt to bubble.

  // Bubbling to |outer.view| is fine, since it doesn't interfere with the
  // gesture in |view_root_|.
  ASSERT_TRUE(rwhier()->BubbleScrollEvent(outer.view.get(), inner.view.get(),
                                          scroll_begin));
  EXPECT_EQ(inner.view.get(), bubbling_gesture_scroll_origin());
  EXPECT_EQ(outer.view.get(), bubbling_gesture_scroll_target());
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            outer.view->last_gesture_seen());

  // We cannot bubble any further, as that would interfere with the gesture in
  // |view_root_|.
  EXPECT_FALSE(rwhier()->BubbleScrollEvent(view_root_.get(), outer.view.get(),
                                           scroll_begin));
  EXPECT_NE(view_root_.get(), bubbling_gesture_scroll_target());
}

// Ensure that when the RenderWidgetHostChildFrameView handling mouse events is
// rooted by GuestView which is not connected to WebContents, we return early
// and when it's connected, we do update mouse move related states in RWHIER.
TEST_F(RenderWidgetHostInputEventRouterTest,
       DoNotSendMouseLeaveEventsForDisconnectedGuestView) {
  ChildViewState child = MakeChildView(view_root_.get());

  // We start the touch in the area for |child.view|.
  view_root_->SetHittestResult(child.view.get(), false);

  blink::WebTouchEvent touch_event(
      blink::WebInputEvent::Type::kTouchStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_event.touches_length = 1;
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStatePressed;
  touch_event.unique_touch_event_id = 1;

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event, ui::LatencyInfo());
  EXPECT_EQ(child.view.get(), touch_target());

  // Need to send a new mouse event after ending the previous touch.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseLeave,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(gfx::PointF(20, 21));

  {  // Simulates GuestView not yet attached to WebContents.
    ChildViewState guest_view = MakeChildView(nullptr);
    ChildViewState child1 = MakeChildView(guest_view.view.get());
    ChildViewState child2 = MakeChildView(child1.view.get());

    // We start the input event in the area for |child1.view|.
    view_root_->SetHittestResult(child1.view.get(), false);

    rwhier()->RouteMouseEvent(view_root_.get(), &mouse_event,
                              ui::LatencyInfo());

    DCHECK_EQ(rwhier()->GetLastMouseMoveTargetForTest(), nullptr);
    DCHECK_EQ(rwhier()->GetLastMouseMoveRootViewForTest(), nullptr);
  }
  {  // Simulates GuestView attached to WebContents.
    ChildViewState guest_view = MakeChildView(view_root_.get());
    ChildViewState child1 = MakeChildView(guest_view.view.get());
    ChildViewState child2 = MakeChildView(child1.view.get());

    // We start the input event in the area for |child2.view|.
    view_root_->SetHittestResult(child1.view.get(), false);
    rwhier()->RouteMouseEvent(view_root_.get(), &mouse_event,
                              ui::LatencyInfo());

    DCHECK_EQ(rwhier()->GetLastMouseMoveTargetForTest(), child1.view.get());
    DCHECK_EQ(rwhier()->GetLastMouseMoveRootViewForTest(), view_root_.get());
  }
}

// Calling ShowContextMenuAtPoint without other events will happen when desktop
// devtools connect to a browser instance running on a mobile.  It should not
// crash.
TEST_F(RenderWidgetHostInputEventRouterTest, CanCallShowContextMenuAtPoint) {
  rwhier()->ShowContextMenuAtPoint(gfx::Point(0, 0), ui::MENU_SOURCE_MOUSE,
                                   view_root_.get());
}

// Input events get latched to a target when middle click autoscroll is in
// progress. This tests enusres that autoscroll latched target state is cleared
// when the view, input events are latched to is destroyed.
TEST_F(RenderWidgetHostInputEventRouterTest,
       EnsureAutoScrollLatchedTargetIsCleared) {
  ChildViewState child = MakeChildView(view_root_.get());

  // Simulate middle click mouse event.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kMiddle;

  view_root_->SetHittestResult(child.view.get(), false);
  input::RenderWidgetTargeter* targeter =
      rwhier()->GetRenderWidgetTargeterForTests();
  rwhier()->RouteMouseEvent(view_root_.get(), &mouse_event, ui::LatencyInfo());
  // Set middle click autoscroll in progress to true.
  rwhier()->SetAutoScrollInProgress(true);
  // Destroy the view/target, middle click autoscroll is latched to.
  rwhier()->OnRenderWidgetHostViewInputDestroyed(child.view.get());

  EXPECT_FALSE(targeter->is_auto_scroll_in_progress());
}

TEST_F(RenderWidgetHostInputEventRouterTest, QueryResultAfterChildViewDead) {
  ChildViewState child = MakeChildView(view_root_.get());

  view_root_->SetHittestResult(child.view.get(), true);

  input_target_client_root_->forward_callback_ = base::BindOnce(
      [](viz::FrameSinkId ret,
         base::WeakPtrFactory<RenderWidgetHostViewBase>* factory,
         MockInputTargetClient::FrameSinkIdAtCallback callback) {
        // Simulate destruction of the view.
        factory->InvalidateWeakPtrs();

        if (callback) {
          std::move(callback).Run(ret, gfx::PointF());
        }
      },
      child.view->GetFrameSinkId(),
      &view_root_->RenderWidgetHostViewBase::weak_factory_);

  // Simulate mouse event.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  rwhier()->RouteMouseEvent(view_root_.get(), &mouse_event, ui::LatencyInfo());

  // Wait for the callback.
  base::RunLoop().RunUntilIdle();
}

#if defined(USE_AURA)
// Mock the DelegatedInkPointRenderer to grab the delegated ink points as they
// are shipped off to viz from the browser process.
class MockDelegatedInkPointRenderer
    : public gfx::mojom::DelegatedInkPointRenderer {
 public:
  explicit MockDelegatedInkPointRenderer(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver)
      : receiver_(this, std::move(receiver)) {}

  void StoreDelegatedInkPoint(const gfx::DelegatedInkPoint& point) override {
    delegated_ink_point_ = point;
  }

  bool HasDelegatedInkPoint() { return delegated_ink_point_.has_value(); }

  gfx::DelegatedInkPoint GetDelegatedInkPoint() {
    gfx::DelegatedInkPoint point = delegated_ink_point_.value();
    delegated_ink_point_.reset();
    return point;
  }

  void ClearDelegatedInkPoint() { delegated_ink_point_.reset(); }

  void ResetPrediction() override { prediction_reset_ = true; }
  bool GetPredictionState() {
    bool state = prediction_reset_;
    prediction_reset_ = false;
    return state;
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  void ResetReceiver() { receiver_.reset(); }
  bool ReceiverIsBound() { return receiver_.is_bound(); }

 private:
  mojo::Receiver<gfx::mojom::DelegatedInkPointRenderer> receiver_;
  std::optional<gfx::DelegatedInkPoint> delegated_ink_point_;
  bool prediction_reset_ = false;
};

// MockCompositor class binds the mojo interfaces so that the ink points are
// shipped to the browser process. Uses values from the real compositor to be
// created, but a fake FrameSinkId must be used so that it hasn't already been
// registered.
class MockCompositor : public ui::Compositor {
 public:
  explicit MockCompositor(ui::Compositor* compositor)
      : ui::Compositor(viz::FrameSinkId(5, 5),
                       compositor->context_factory(),
                       compositor->task_runner(),
                       compositor->is_pixel_canvas()) {}

  void SetDelegatedInkPointRenderer(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver)
      override {
    delegated_ink_point_renderer_ =
        std::make_unique<MockDelegatedInkPointRenderer>(std::move(receiver));
  }

  MockDelegatedInkPointRenderer* delegated_ink_point_renderer() {
    return delegated_ink_point_renderer_.get();
  }

 private:
  std::unique_ptr<MockDelegatedInkPointRenderer> delegated_ink_point_renderer_;
};

enum TestEvent { kMouseEvent, kTouchEvent };
enum HoveringState { kHovering, kNotHovering };

class DelegatedInkPointTest
    : public RenderWidgetHostInputEventRouterTest,
      public testing::WithParamInterface<std::tuple<TestEvent, HoveringState>> {
 public:
  DelegatedInkPointTest() = default;

  void SetUp() override {
    RenderWidgetHostInputEventRouterTest::SetUp();

    aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>(
        ImageTransportFactory::GetInstance()->GetContextFactory());
    aura_test_helper_->SetUp();

    compositor_ = std::make_unique<MockCompositor>(
        aura_test_helper_->GetHost()->compositor());
    view_root_->SetCompositor(compositor_.get());
  }

  void TearDown() override {
    aura_test_helper_->TearDown();
    compositor_.reset();
    RenderWidgetHostInputEventRouterTest::TearDown();
  }

  TestEvent GetEventParam() { return std::get<0>(GetParam()); }
  HoveringState GetHoverParam() { return std::get<1>(GetParam()); }

  void SetInkMetadataFlagOnRenderFrameMetadata(bool delegated_ink) {
    SetInkMetadataFlagOnSpecificHost(delegated_ink, widget_host_root_.get());
  }

  void SetInkMetadataFlagOnSpecificHost(bool delegated_ink,
                                        RenderWidgetHostImpl* widget_host) {
    cc::RenderFrameMetadata metadata;
    if (delegated_ink) {
      metadata.delegated_ink_metadata = cc::DelegatedInkBrowserMetadata(
          GetHoverParam() == HoveringState::kHovering);
    }
    widget_host->render_frame_metadata_provider()
        ->SetLastRenderFrameMetadataForTest(metadata);
  }

  void SendEvent(bool match_test_hovering_state,
                 gfx::PointF point,
                 base::TimeTicks timestamp = base::TimeTicks::Now()) {
    SendEvent(match_test_hovering_state, point, timestamp,
              /*use_enter_event*/ false, /*use_exit_event*/ false);
  }

  void SendEvent(bool match_test_hovering_state,
                 const gfx::PointF& point,
                 base::TimeTicks timestamp,
                 bool use_enter_event,
                 bool use_exit_event) {
    DCHECK(!(use_enter_event && use_exit_event));

    // Hovering creates and sends ui::MouseEvents with
    // EventType::kMouse{Moved,Entered,Exited} types, so do the same here in
    // hovering scenarios.
    if (GetEventParam() == TestEvent::kTouchEvent &&
        !Hovering(match_test_hovering_state)) {
      blink::WebInputEvent::Type event_type =
          blink::WebInputEvent::Type::kTouchMove;
      blink::WebTouchPoint::State touch_state =
          blink::WebTouchPoint::State::kStateMoved;
      if (use_enter_event) {
        event_type = blink::WebInputEvent::Type::kTouchStart;
        touch_state = blink::WebTouchPoint::State::kStatePressed;
        // Set this now so that if we are going to send a enter event anyway,
        // we don't send two.
        sent_touch_press_ = true;
      }
      if (use_exit_event) {
        event_type = blink::WebInputEvent::Type::kTouchEnd;
        touch_state = blink::WebTouchPoint::State::kStateReleased;
      }

      // Touch needs a pressed event first to properly handle future move
      // events.
      SendTouchPress(point);

      blink::WebTouchEvent touch_event(
          event_type, blink::WebInputEvent::kNoModifiers, timestamp);
      touch_event.touches_length = 1;
      touch_event.touches[0].id = kPointerId;
      touch_event.touches[0].SetPositionInWidget(point);
      touch_event.touches[0].state = touch_state;
      touch_event.unique_touch_event_id = GetTouchId();

      rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                                ui::LatencyInfo());

      // Need to send a new press event after ending the previous touch.
      if (use_exit_event) {
        sent_touch_press_ = false;
      }
    } else {
      blink::WebInputEvent::Type event_type =
          blink::WebInputEvent::Type::kMouseMove;
      if (use_enter_event) {
        event_type = blink::WebInputEvent::Type::kMouseEnter;
      }
      if (use_exit_event) {
        event_type = blink::WebInputEvent::Type::kMouseLeave;
      }

      int modifiers = 0;
      if (!Hovering(match_test_hovering_state)) {
        modifiers = blink::WebInputEvent::kLeftButtonDown;
      }

      blink::WebMouseEvent mouse_event(event_type, modifiers, timestamp,
                                       kPointerId);
      mouse_event.SetPositionInWidget(point);

      rwhier()->RouteMouseEvent(view_root_.get(), &mouse_event,
                                ui::LatencyInfo());
    }
  }

  void SetDeviceScaleFactor(float dsf) {
    aura_test_helper_->GetTestScreen()->SetDeviceScaleFactor(dsf);

    // Normally, WebContentsImpl owns a ScreenChangeMonitor that observes
    // display::DisplayList changes like this and indirectly calls
    // UpdateScreenInfo via a callback.  Since there's no WebContentsImpl
    // in this unittest, make this call directly.
    view_root_->UpdateScreenInfo();
  }

  MockCompositor* compositor() { return compositor_.get(); }

  int32_t GetExpectedPointerId() const { return kPointerId; }

 private:
  void SendTouchPress(const gfx::PointF& requested_touch_location) {
    DCHECK(GetEventParam() == TestEvent::kTouchEvent);
    if (sent_touch_press_) {
      return;
    }

    // Location of the press event doesn't matter, so long as it doesn't exactly
    // match the location of the subsequent move event. If they match, then the
    // move event is dropped.
    gfx::PointF point(requested_touch_location.x() + 2.f,
                      requested_touch_location.y() + 2.f);

    // Send a TouchStart/End sequence.
    blink::WebTouchEvent press(
        blink::WebInputEvent::Type::kTouchStart,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    press.touches_length = 1;
    press.touches[0].id = kPointerId;
    press.touches[0].SetPositionInWidget(point);
    press.touches[0].state = blink::WebTouchPoint::State::kStatePressed;
    press.unique_touch_event_id = GetTouchId();

    rwhier()->RouteTouchEvent(view_root_.get(), &press, ui::LatencyInfo());
    sent_touch_press_ = true;
  }

  bool Hovering(bool match_test_hovering_state) {
    return (GetHoverParam() == HoveringState::kHovering &&
            match_test_hovering_state) ||
           (GetHoverParam() == HoveringState::kNotHovering &&
            !match_test_hovering_state);
  }

  // Unique touch id is unique per event, so always increment before providing
  // a new one.
  int GetTouchId() { return ++unique_touch_id_; }

  // Pointer id to use in these tests. It must be consistent throughout a single
  // test for some of the touch variations.
  const int32_t kPointerId = 5;

  // Touch events are ignored if a press isn't sent first, so use this to track
  // if we have already sent a touch press event yet or not.
  bool sent_touch_press_ = false;

  // Most recently used unique touch id for blink::WebTouchEvents
  int unique_touch_id_ = 0;

  // Helper for creating a compositor and setting the device scale factor.
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;

  // Mock compositor used for getting the delegated ink points that are
  // forwarded.
  std::unique_ptr<MockCompositor> compositor_;
};

struct DelegatedInkPointTestPassToString {
  std::string operator()(
      const testing::TestParamInfo<std::tuple<TestEvent, HoveringState>> type)
      const {
    std::string suffix;

    if (std::get<0>(type.param) == TestEvent::kMouseEvent) {
      suffix.append("Mouse");
    } else {
      suffix.append("Touch");
    }

    if (std::get<1>(type.param) == HoveringState::kHovering) {
      suffix.append("Hovering");
    } else {
      suffix.append("NotHovering");
    }

    return suffix;
  }
};

INSTANTIATE_TEST_SUITE_P(
    DelegatedInkTrails,
    DelegatedInkPointTest,
    testing::Combine(
        testing::Values(TestEvent::kMouseEvent, TestEvent::kTouchEvent),
        testing::Values(HoveringState::kHovering, HoveringState::kNotHovering)),
    DelegatedInkPointTestPassToString());

// Tests to confirm that input events are correctly forwarded to the UI
// Compositor when DelegatedInkTrails should be drawn, and stops forwarding when
// they no longer should be drawn.
TEST_P(DelegatedInkPointTest, EventForwardedToCompositor) {
  // First confirm that the flag is false by default and the point is not sent.
  SendEvent(true, gfx::PointF(15, 15));
  MockDelegatedInkPointRenderer* delegated_ink_point_renderer =
      compositor()->delegated_ink_point_renderer();

  EXPECT_FALSE(delegated_ink_point_renderer);

  // Then set it to true and confirm that the DelegatedInkPointRenderer is
  // initialized, the connection is made and the point makes it to the renderer.
  SetInkMetadataFlagOnRenderFrameMetadata(true);
  gfx::DelegatedInkPoint expected_point(
      gfx::PointF(10, 10), base::TimeTicks::Now(), GetExpectedPointerId());
  SendEvent(true, expected_point.point(), expected_point.timestamp());

  delegated_ink_point_renderer = compositor()->delegated_ink_point_renderer();
  EXPECT_TRUE(delegated_ink_point_renderer);
  delegated_ink_point_renderer->FlushForTesting();

  EXPECT_TRUE(delegated_ink_point_renderer->HasDelegatedInkPoint());
  gfx::DelegatedInkPoint actual_point =
      delegated_ink_point_renderer->GetDelegatedInkPoint();
  EXPECT_EQ(expected_point.point(), actual_point.point());
  EXPECT_EQ(expected_point.timestamp(), actual_point.timestamp());
  EXPECT_EQ(GetExpectedPointerId(), actual_point.pointer_id());

  // Then try changing the scale factor to confirm it affects the point
  // correctly.
  const float scale = 2.6f;
  SetDeviceScaleFactor(scale);
  gfx::PointF unscaled_point(15, 15);
  base::TimeTicks unscaled_time = base::TimeTicks::Now();

  SendEvent(true, unscaled_point, unscaled_time);
  delegated_ink_point_renderer->FlushForTesting();

  unscaled_point.Scale(scale);
  expected_point = gfx::DelegatedInkPoint(unscaled_point, unscaled_time,
                                          GetExpectedPointerId());

  EXPECT_TRUE(delegated_ink_point_renderer->HasDelegatedInkPoint());
  actual_point = delegated_ink_point_renderer->GetDelegatedInkPoint();
  EXPECT_EQ(expected_point.point(), actual_point.point());
  EXPECT_EQ(expected_point.timestamp(), actual_point.timestamp());
  EXPECT_EQ(GetExpectedPointerId(), actual_point.pointer_id());

  // Confirm that prediction is reset when the API is no longer being used and
  // |delegated_ink_metadata| is not set.
  SetInkMetadataFlagOnRenderFrameMetadata(false);

  SendEvent(true, gfx::PointF(25, 25));
  delegated_ink_point_renderer->FlushForTesting();

  EXPECT_FALSE(delegated_ink_point_renderer->HasDelegatedInkPoint());
  EXPECT_TRUE(delegated_ink_point_renderer->GetPredictionState());

  // Finally, confirm that nothing is sent after the prediction has been reset
  // when the delegated ink flag on the render frame metadata is false.
  SendEvent(true, gfx::PointF(46, 46));
  delegated_ink_point_renderer->FlushForTesting();

  EXPECT_FALSE(delegated_ink_point_renderer->HasDelegatedInkPoint());
  EXPECT_FALSE(delegated_ink_point_renderer->GetPredictionState());
}

// Confirm that the interface is rebound if the receiver disconnects.
TEST_P(DelegatedInkPointTest, MojoInterfaceReboundOnDisconnect) {
  // First make sure the connection exists.
  SetInkMetadataFlagOnRenderFrameMetadata(true);
  SendEvent(true, gfx::PointF(15, 15));

  MockDelegatedInkPointRenderer* delegated_ink_point_renderer =
      compositor()->delegated_ink_point_renderer();

  EXPECT_TRUE(delegated_ink_point_renderer);
  EXPECT_TRUE(delegated_ink_point_renderer->ReceiverIsBound());

  // Reset the receiver and flush the remote to confirm it is no longer bound.
  delegated_ink_point_renderer->ResetReceiver();
  FlushInkRenderer();

  EXPECT_FALSE(delegated_ink_point_renderer->ReceiverIsBound());

  // Confirm that it now gets reconnected correctly.
  SendEvent(true, gfx::PointF(25, 25));

  delegated_ink_point_renderer = compositor()->delegated_ink_point_renderer();

  EXPECT_TRUE(delegated_ink_point_renderer);
  EXPECT_TRUE(delegated_ink_point_renderer->ReceiverIsBound());
}

// Test to confirm that forwarding points to viz will stop and prediction is
// reset if the state of hovering differs between what is expected and the
// received points.
TEST_P(DelegatedInkPointTest, StopForwardingOnHoverStateChange) {
  // First send a point and make sure it makes it to the renderer.
  SetInkMetadataFlagOnRenderFrameMetadata(true);
  SendEvent(true, gfx::PointF(15, 15));

  MockDelegatedInkPointRenderer* delegated_ink_point_renderer =
      compositor()->delegated_ink_point_renderer();
  EXPECT_TRUE(delegated_ink_point_renderer);
  delegated_ink_point_renderer->FlushForTesting();

  EXPECT_TRUE(delegated_ink_point_renderer->HasDelegatedInkPoint());
  delegated_ink_point_renderer->ClearDelegatedInkPoint();

  // Now send a point that doesn't match the state of hovering on the metadata
  // to confirm that it isn't sent and ResetPrediction is called.
  SendEvent(false, gfx::PointF(20, 20));
  delegated_ink_point_renderer->FlushForTesting();

  EXPECT_FALSE(delegated_ink_point_renderer->HasDelegatedInkPoint());
  EXPECT_TRUE(delegated_ink_point_renderer->GetPredictionState());

  // Send another that doesn't match to confirm the end trail point is only sent
  // once.
  SendEvent(false, gfx::PointF(25, 25));
  delegated_ink_point_renderer->FlushForTesting();
  EXPECT_FALSE(delegated_ink_point_renderer->HasDelegatedInkPoint());

  // Send one that does match again to confirm that points will start sending
  // again if the hovering state starts matching again.
  SendEvent(true, gfx::PointF(30, 30));
  delegated_ink_point_renderer->FlushForTesting();

  EXPECT_TRUE(delegated_ink_point_renderer->HasDelegatedInkPoint());
  EXPECT_FALSE(delegated_ink_point_renderer->GetPredictionState());
}

// Confirm that only move events are forwarded, not enter/exit or equivalent
// events.
TEST_P(DelegatedInkPointTest, IgnoreEnterAndExitEvents) {
  // First set everything up and try forwarding a point, confirming that it is
  // sent as expected.
  SetInkMetadataFlagOnRenderFrameMetadata(true);
  gfx::DelegatedInkPoint expected_point(
      gfx::PointF(10, 10), base::TimeTicks::Now(), GetExpectedPointerId());
  SendEvent(true, expected_point.point(), expected_point.timestamp());

  MockDelegatedInkPointRenderer* delegated_ink_point_renderer =
      compositor()->delegated_ink_point_renderer();
  EXPECT_TRUE(delegated_ink_point_renderer);
  delegated_ink_point_renderer->FlushForTesting();

  EXPECT_TRUE(delegated_ink_point_renderer->HasDelegatedInkPoint());
  gfx::DelegatedInkPoint actual_point =
      delegated_ink_point_renderer->GetDelegatedInkPoint();
  EXPECT_EQ(expected_point.point(), actual_point.point());
  EXPECT_EQ(expected_point.timestamp(), actual_point.timestamp());
  EXPECT_EQ(GetExpectedPointerId(), actual_point.pointer_id());

  // Now try with an exit event.
  SendEvent(true, gfx::PointF(42, 19), base::TimeTicks::Now(),
            /*use_enter_event=*/false, /*use_exit_event=*/true);
  delegated_ink_point_renderer->FlushForTesting();
  EXPECT_FALSE(delegated_ink_point_renderer->HasDelegatedInkPoint());

  // Try sending an enter event and confirm it is not forwarded.
  SendEvent(true, gfx::PointF(12, 12), base::TimeTicks::Now(),
            /*use_enter_event=*/true, /*use_exit_event=*/false);
  delegated_ink_point_renderer->FlushForTesting();
  EXPECT_FALSE(delegated_ink_point_renderer->HasDelegatedInkPoint());

  // Finally, confirm that sending move events will work again without issue.
  expected_point = gfx::DelegatedInkPoint(
      gfx::PointF(20, 21), base::TimeTicks::Now(), GetExpectedPointerId());
  SendEvent(true, expected_point.point(), expected_point.timestamp());

  delegated_ink_point_renderer->FlushForTesting();

  EXPECT_TRUE(delegated_ink_point_renderer->HasDelegatedInkPoint());
  actual_point = delegated_ink_point_renderer->GetDelegatedInkPoint();
  EXPECT_EQ(expected_point.point(), actual_point.point());
  EXPECT_EQ(expected_point.timestamp(), actual_point.timestamp());
  EXPECT_EQ(GetExpectedPointerId(), actual_point.pointer_id());
}

// This test confirms that points can be forwarded when using delegated ink in
// a child frame, such as an OOPIF.
TEST_P(DelegatedInkPointTest, ForwardPointsToChildFrame) {
  // Make the child frame, set the delegated ink flag on it, give it a
  // compositor, and set it as the hit test result so that the input router
  // sends points to it.
  ChildViewState child = MakeChildView(view_root_.get());
  SetInkMetadataFlagOnSpecificHost(true, child.widget_host.get());
  child.view->SetCompositor(compositor());
  view_root_->SetHittestResult(child.view.get(), false);

  // Send a point and confirm that it is forwarded, meaning that it correctly
  // checked the metadata flag on the child frame's widget.
  gfx::DelegatedInkPoint expected_point(
      gfx::PointF(10, 10), base::TimeTicks::Now(), GetExpectedPointerId());
  SendEvent(true, expected_point.point(), expected_point.timestamp(), false,
            false);

  MockDelegatedInkPointRenderer* delegated_ink_point_renderer =
      compositor()->delegated_ink_point_renderer();
  EXPECT_TRUE(delegated_ink_point_renderer);
  delegated_ink_point_renderer->FlushForTesting();

  EXPECT_TRUE(delegated_ink_point_renderer->HasDelegatedInkPoint());
  gfx::DelegatedInkPoint actual_point =
      delegated_ink_point_renderer->GetDelegatedInkPoint();
  EXPECT_EQ(expected_point.point(), actual_point.point());
  EXPECT_EQ(expected_point.timestamp(), actual_point.timestamp());
  EXPECT_EQ(GetExpectedPointerId(), actual_point.pointer_id());

  // Reset's the hit test result on the root so that we don't crash on
  // destruction.
  rwhier()->OnRenderWidgetHostViewInputDestroyed(child.view.get());
  view_root_->GetCursorManager()->ViewBeingDestroyed(child.view.get());
}

#endif  // defined(USE_AURA)

}  // namespace content
