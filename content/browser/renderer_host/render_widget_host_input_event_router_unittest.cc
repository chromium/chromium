// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/host/hit_test/hit_test_query.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/test/host_frame_sink_manager_test_api.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/compositor/test/test_image_transport_factory.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/frame_connector_delegate.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_widget_targeter.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/hit_test/input_target_client.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom.h"

namespace content {

namespace {

class MockFrameConnectorDelegate : public FrameConnectorDelegate {
 public:
  MockFrameConnectorDelegate(RenderWidgetHostViewChildFrame* view,
                             RenderWidgetHostViewBase* parent_view,
                             RenderWidgetHostViewBase* root_view,
                             bool use_zoom_for_device_scale_factor)
      : FrameConnectorDelegate(use_zoom_for_device_scale_factor),
        parent_view_(parent_view),
        root_view_(root_view) {
    view_ = view;
    view_->SetFrameConnectorDelegate(this);
  }

  ~MockFrameConnectorDelegate() override {
    if (view_) {
      view_->SetFrameConnectorDelegate(nullptr);
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
  RenderWidgetHostViewBase* parent_view_;
  RenderWidgetHostViewBase* root_view_;

  DISALLOW_COPY_AND_ASSIGN(MockFrameConnectorDelegate);
};

// Used as a target for the RenderWidgetHostInputEventRouter. We record what
// events were forwarded to us in order to verify that the events are being
// routed correctly.
class TestRenderWidgetHostViewChildFrame
    : public RenderWidgetHostViewChildFrame {
 public:
  explicit TestRenderWidgetHostViewChildFrame(RenderWidgetHost* widget)
      : RenderWidgetHostViewChildFrame(widget, blink::ScreenInfo()) {
    Init();
  }
  ~TestRenderWidgetHostViewChildFrame() override = default;

  void ProcessGestureEvent(const blink::WebGestureEvent& event,
                           const ui::LatencyInfo&) override {
    last_gesture_seen_ = event.GetType();
  }

  void ProcessAckedTouchEvent(
      const TouchEventWithLatencyInfo& touch,
      blink::mojom::InputEventResultState ack_result) override {
    unique_id_for_last_touch_ack_ = touch.event.unique_touch_event_id;
  }

  blink::WebInputEvent::Type last_gesture_seen() { return last_gesture_seen_; }
  uint32_t last_id_for_touch_ack() { return unique_id_for_last_touch_ack_; }

  void Reset() { last_gesture_seen_ = blink::WebInputEvent::Type::kUndefined; }

 private:
  blink::WebInputEvent::Type last_gesture_seen_ =
      blink::WebInputEvent::Type::kUndefined;
  uint32_t unique_id_for_last_touch_ack_ = 0;
};

class StubHitTestQuery : public viz::HitTestQuery {
 public:
  StubHitTestQuery(RenderWidgetHostViewBase* hittest_result,
                   bool query_renderer)
      : hittest_result_(hittest_result), query_renderer_(query_renderer) {}
  ~StubHitTestQuery() override = default;

  viz::Target FindTargetForLocationStartingFromImpl(
      viz::EventSource event_source,
      const gfx::PointF& location,
      const viz::FrameSinkId& sink_id,
      bool is_location_relative_to_parent) const override {
    return {hittest_result_->GetFrameSinkId(), gfx::PointF(),
            viz::HitTestRegionFlags::kHitTestMouse |
                viz::HitTestRegionFlags::kHitTestTouch |
                viz::HitTestRegionFlags::kHitTestMine |
                (query_renderer_ ? viz::HitTestRegionFlags::kHitTestAsk : 0)};
  }

 private:
  const RenderWidgetHostViewBase* hittest_result_;
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
      RenderWidgetHostViewBase* target_view,
      gfx::PointF* transformed_point) override {
    return true;
  }

  void ProcessGestureEvent(const blink::WebGestureEvent& event,
                           const ui::LatencyInfo&) override {
    last_gesture_seen_ = event.GetType();
  }

  void ProcessAckedTouchEvent(
      const TouchEventWithLatencyInfo& touch,
      blink::mojom::InputEventResultState ack_result) override {
    unique_id_for_last_touch_ack_ = touch.event.unique_touch_event_id;
  }

  viz::FrameSinkId GetRootFrameSinkId() override { return GetFrameSinkId(); }

  blink::WebInputEvent::Type last_gesture_seen() { return last_gesture_seen_; }
  uint32_t last_id_for_touch_ack() { return unique_id_for_last_touch_ack_; }

  void SetHittestResult(RenderWidgetHostViewBase* result_view,
                        bool query_renderer) {
    DCHECK(GetHostFrameSinkManager());

    viz::HostFrameSinkManager::DisplayHitTestQueryMap hit_test_map;
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
    if (forward_callback_)
      std::move(forward_callback_).Run(std::move(callback));
    else if (callback)
      std::move(callback).Run(viz::FrameSinkId(), point);
  }

  base::OnceCallback<void(FrameSinkIdAtCallback)> forward_callback_;
  mojo::Receiver<viz::mojom::InputTargetClient> receiver_;
};

}  // namespace

class RenderWidgetHostInputEventRouterTest : public testing::Test {
 protected:
  RenderWidgetHostInputEventRouterTest() = default;

  RenderWidgetHostInputEventRouter* rwhier() {
    return delegate_.GetInputEventRouter();
  }

  // testing::Test:
  void SetUp() override {
    browser_context_ = std::make_unique<TestBrowserContext>();

// ImageTransportFactory doesn't exist on Android. This is needed to create
// a RenderWidgetHostViewChildFrame in the test.
#if !defined(OS_ANDROID)
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
#endif

    delegate_.CreateInputEventRouter();

    process_host_root_ =
        std::make_unique<MockRenderProcessHost>(browser_context_.get());
    agent_scheduling_group_host_root_ =
        std::make_unique<AgentSchedulingGroupHost>(*process_host_root_);
    widget_host_root_ = std::make_unique<RenderWidgetHostImpl>(
        &delegate_, *agent_scheduling_group_host_root_,
        process_host_root_->GetNextRoutingID(),
        /*hidden=*/false, std::make_unique<FrameTokenMessageQueue>());

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
    widget_host_root_->SetInputTargetClientForTesting(
        std::move(input_target_client));

    EXPECT_EQ(view_root_.get(),
              rwhier()->FindViewFromFrameSinkId(view_root_->GetFrameSinkId()));
  }

  struct ChildViewState {
    std::unique_ptr<MockRenderProcessHost> process_host;
    std::unique_ptr<AgentSchedulingGroupHost> agent_scheduling_group_host;
    std::unique_ptr<RenderWidgetHostImpl> widget_host;
    std::unique_ptr<TestRenderWidgetHostViewChildFrame> view;
    std::unique_ptr<MockFrameConnectorDelegate> frame_connector;
  };

  ChildViewState MakeChildView(RenderWidgetHostViewBase* parent_view) {
    ChildViewState child;

    child.process_host =
        std::make_unique<MockRenderProcessHost>(browser_context_.get());
    child.agent_scheduling_group_host =
        std::make_unique<AgentSchedulingGroupHost>(*child.process_host);
    child.widget_host = std::make_unique<RenderWidgetHostImpl>(
        &delegate_, *child.agent_scheduling_group_host,
        child.process_host->GetNextRoutingID(),
        /*hidden=*/false, std::make_unique<FrameTokenMessageQueue>());
    child.view = std::make_unique<TestRenderWidgetHostViewChildFrame>(
        child.widget_host.get());
    child.frame_connector = std::make_unique<MockFrameConnectorDelegate>(
        child.view.get(), parent_view, view_root_.get(),
        false /* use_zoom_for_device_scale_factor */);

    EXPECT_EQ(child.view.get(),
              rwhier()->FindViewFromFrameSinkId(child.view->GetFrameSinkId()));

    return child;
  }

  void TearDown() override {
    view_root_.reset();
    widget_host_root_.reset();
    process_host_root_.reset();
    base::RunLoop().RunUntilIdle();

#if !defined(OS_ANDROID)
    ImageTransportFactory::Terminate();
#endif
  }

  RenderWidgetHostViewBase* touch_target() { return rwhier()->touch_target_; }
  RenderWidgetHostViewBase* touchscreen_gesture_target() {
    return rwhier()->touchscreen_gesture_target_;
  }
  RenderWidgetHostViewChildFrame* bubbling_gesture_scroll_origin() {
    return rwhier()->bubbling_gesture_scroll_origin_;
  }
  RenderWidgetHostViewBase* bubbling_gesture_scroll_target() {
    return rwhier()->bubbling_gesture_scroll_target_;
  }

  void TestSendNewGestureWhileBubbling(
      TestRenderWidgetHostViewChildFrame* bubbling_origin,
      RenderWidgetHostViewBase* gesture_target,
      bool should_cancel);

  BrowserTaskEnvironment task_environment_;

  MockRenderWidgetHostDelegate delegate_;
  std::unique_ptr<BrowserContext> browser_context_;

  std::unique_ptr<MockRenderProcessHost> process_host_root_;
  std::unique_ptr<AgentSchedulingGroupHost> agent_scheduling_group_host_root_;
  std::unique_ptr<RenderWidgetHostImpl> widget_host_root_;
  std::unique_ptr<MockRootRenderWidgetHostView> view_root_;
  std::unique_ptr<MockInputTargetClient> input_target_client_root_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostInputEventRouterTest);
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

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(child.view.get(), touch_target());

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;

  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(child.view.get(), touchscreen_gesture_target());
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureTapDown,
            child.view->last_gesture_seen());
  EXPECT_NE(blink::WebInputEvent::Type::kGestureTapDown,
            view_root_->last_gesture_seen());

  touch_event.SetType(blink::WebInputEvent::Type::kTouchMove);
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStateMoved;
  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureTapCancel);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollBegin);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollUpdate);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));

  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));

  // The continuation of the touch moves should maintain their current target of
  // |child.view|, even if they move outside of that view, and into
  // |view_root_|.
  // If the target is maintained through the gesture this will pass. If instead
  // the hit testing logic is refered to, then this test will fail.
  view_root_->SetHittestResult(view_root_.get(), false);
  view_root_->Reset();
  child.view->Reset();

  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));

  EXPECT_EQ(child.view.get(), touch_target());
  EXPECT_EQ(child.view.get(), touchscreen_gesture_target());
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            child.view->last_gesture_seen());
  EXPECT_NE(blink::WebInputEvent::Type::kGestureScrollUpdate,
            view_root_->last_gesture_seen());

  touch_event.SetType(blink::WebInputEvent::Type::kTouchEnd);
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStateReleased;
  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollEnd);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));

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
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  blink::WebTouchEvent touch_end_event(
      blink::WebInputEvent::Type::kTouchEnd, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_end_event.touches_length = 1;
  touch_end_event.touches[0].state =
      blink::WebTouchPoint::State::kStateReleased;
  touch_end_event.unique_touch_event_id = 2;

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_end_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  // Make sure both touch events were added to the TEAQ.
  EXPECT_EQ(2u, rwhier()->TouchEventAckQueueLengthForTesting());

  // Now, tell the router that the child view was destroyed, and verify the
  // acks.
  rwhier()->OnRenderWidgetHostViewBaseDestroyed(child.view.get());
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
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
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
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(view_root_->last_id_for_touch_ack(), 2lu);
}

TEST_F(RenderWidgetHostInputEventRouterTest, DoNotCoalesceTouchEvents) {
  // We require the presence of a child view, otherwise targeting is short
  // circuited.
  ChildViewState child = MakeChildView(view_root_.get());

  RenderWidgetTargeter* targeter = rwhier()->GetRenderWidgetTargeterForTests();
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
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(0u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  touch_event.SetType(blink::WebInputEvent::Type::kTouchMove);
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStateMoved;
  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(1u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(2u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  touch_event.SetType(blink::WebInputEvent::Type::kTouchEnd);
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStateReleased;
  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  EXPECT_EQ(3u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());
}

TEST_F(RenderWidgetHostInputEventRouterTest, DoNotCoalesceGestureEvents) {
  // We require the presence of a child view, otherwise targeting is short
  // circuited.
  ChildViewState child = MakeChildView(view_root_.get());

  RenderWidgetTargeter* targeter = rwhier()->GetRenderWidgetTargeterForTests();
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
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(0u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(1u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  touch_event.SetType(blink::WebInputEvent::Type::kTouchEnd);
  touch_event.touches[0].state = blink::WebTouchPoint::State::kStateReleased;
  touch_event.unique_touch_event_id += 1;
  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(2u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollBegin);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(3u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollUpdate);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(4u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollUpdate);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(5u, targeter->num_requests_in_queue_for_testing());
  EXPECT_TRUE(targeter->is_request_in_flight_for_testing());

  gesture_event.SetType(blink::WebInputEvent::Type::kGestureScrollEnd);
  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));
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

void RenderWidgetHostInputEventRouterTest::TestSendNewGestureWhileBubbling(
    TestRenderWidgetHostViewChildFrame* bubbling_origin,
    RenderWidgetHostViewBase* gesture_target,
    bool should_cancel) {
  gfx::Vector2dF delta(0.f, 10.f);
  blink::WebGestureEvent scroll_begin =
      blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
          delta.x(), delta.y(), blink::WebGestureDevice::kTouchscreen);

  TestRenderWidgetHostViewChildFrame* cur_target = bubbling_origin;
  RenderWidgetHostViewBase* parent = bubbling_origin->GetParentView();
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
      parent = next_child->GetParentView();
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

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(gesture_target, touch_target());

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;

  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));
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

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(view_root_.get(), touch_target());

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;

  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));
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

  rwhier()->RouteTouchEvent(view_root_.get(), &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(view_root_.get(), touch_target());

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;

  rwhier()->RouteGestureEvent(view_root_.get(), &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));
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
  RenderWidgetTargeter* targeter = rwhier()->GetRenderWidgetTargeterForTests();
  rwhier()->RouteMouseEvent(view_root_.get(), &mouse_event,
                            ui::LatencyInfo(ui::SourceEventType::MOUSE));
  // Set middle click autoscroll in progress to true.
  rwhier()->SetAutoScrollInProgress(true);
  // Destroy the view/target, middle click autoscroll is latched to.
  rwhier()->OnRenderWidgetHostViewBaseDestroyed(child.view.get());

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

        if (callback)
          std::move(callback).Run(ret, gfx::PointF());
      },
      child.view->GetFrameSinkId(),
      &view_root_->RenderWidgetHostViewBase::weak_factory_);

  // Simulate mouse event.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  rwhier()->RouteMouseEvent(view_root_.get(), &mouse_event,
                            ui::LatencyInfo(ui::SourceEventType::MOUSE));

  // Wait for the callback.
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
