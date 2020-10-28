// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/test/fake_render_widget_host.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_widget_delegate.h"
#include "content/test/fake_compositor_dependencies.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/public/common/widget/visual_properties.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"
#include "third_party/blink/public/mojom/page/widget.mojom-test-utils.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_external_widget.h"
#include "third_party/blink/public/web/web_external_widget_client.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;

namespace blink {

bool operator==(const InputHandlerProxy::DidOverscrollParams& lhs,
                const InputHandlerProxy::DidOverscrollParams& rhs) {
  return lhs.accumulated_overscroll == rhs.accumulated_overscroll &&
         lhs.latest_overscroll_delta == rhs.latest_overscroll_delta &&
         lhs.current_fling_velocity == rhs.current_fling_velocity &&
         lhs.causal_event_viewport_point == rhs.causal_event_viewport_point &&
         lhs.overscroll_behavior == rhs.overscroll_behavior;
}

}  // namespace blink

namespace cc {
class AnimationHost;
}

namespace content {

namespace {

const char* EVENT_LISTENER_RESULT_HISTOGRAM = "Event.PassiveListeners";

// Keep in sync with enum defined in
// RenderWidgetInputHandler::LogPassiveEventListenersUma.
enum {
  PASSIVE_LISTENER_UMA_ENUM_PASSIVE,
  PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE,
  PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED,
  PASSIVE_LISTENER_UMA_ENUM_CANCELABLE,
  PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED,
  PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING,
  PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS_DEPRECATED,
  PASSIVE_LISTENER_UMA_ENUM_COUNT
};

// Since std::unique_ptr isn't copyable we can't use the
// MockCallback template.
class MockHandledEventCallback {
 public:
  MockHandledEventCallback() = default;
  MOCK_METHOD4_T(Run,
                 void(blink::mojom::InputEventResultState,
                      const ui::LatencyInfo&,
                      blink::InputHandlerProxy::DidOverscrollParams*,
                      base::Optional<cc::TouchAction>));

  blink::WebWidget::HandledEventCallback GetCallback() {
    return base::BindOnce(&MockHandledEventCallback::HandleCallback,
                          base::Unretained(this));
  }

 private:
  void HandleCallback(
      blink::mojom::InputEventResultState ack_state,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<blink::InputHandlerProxy::DidOverscrollParams> overscroll,
      base::Optional<cc::TouchAction> touch_action) {
    Run(ack_state, latency_info, overscroll.get(), touch_action);
  }

  DISALLOW_COPY_AND_ASSIGN(MockHandledEventCallback);
};

class MockWebExternalWidgetClient : public blink::WebExternalWidgetClient {
 public:
  MockWebExternalWidgetClient() = default;

  // WebExternalWidgetClient implementation.
  MOCK_METHOD1(DidResize, void(const gfx::Size& size));
  MOCK_METHOD0(DispatchBufferedTouchEvents, blink::WebInputEventResult());
  MOCK_METHOD1(
      HandleInputEvent,
      blink::WebInputEventResult(const blink::WebCoalescedInputEvent&));
  MOCK_METHOD1(RequestNewLayerTreeFrameSink, void(LayerTreeFrameSinkCallback));
  MOCK_METHOD0(DidCommitAndDrawCompositorFrame, void());
  MOCK_METHOD1(WillHandleGestureEvent, bool(const blink::WebGestureEvent&));
  MOCK_METHOD4(DidHandleGestureScrollEvent,
               void(const blink::WebGestureEvent& gesture_event,
                    const gfx::Vector2dF& unused_delta,
                    const cc::OverscrollBehavior& overscroll_behavior,
                    bool event_processed));

  // Because we mock DispatchBufferedTouchEvents indicate we have support.
  bool SupportsBufferedTouchEvents() override { return true; }
};

std::unique_ptr<AgentSchedulingGroup> CreateAgentSchedulingGroup(
    RenderThread& render_thread) {
  mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost>
      agent_scheduling_group_host;
  ignore_result(
      agent_scheduling_group_host.InitWithNewEndpointAndPassReceiver());
  mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup>
      agent_scheduling_group_mojo;
  return std::make_unique<AgentSchedulingGroup>(
      render_thread, std::move(agent_scheduling_group_host),
      std::move(agent_scheduling_group_mojo));
}

}  // namespace

class InteractiveRenderWidget : public RenderWidget {
 public:
  explicit InteractiveRenderWidget(AgentSchedulingGroup& agent_scheduling_group,
                                   CompositorDependencies* compositor_deps)
      : RenderWidget(agent_scheduling_group,
                     ++next_routing_id_,
                     compositor_deps) {}

  void Init(blink::WebWidget* widget, const blink::ScreenInfo& screen_info) {
    Initialize(base::NullCallback(), widget, screen_info);
  }

  using RenderWidget::Close;

  void SendInputEvent(const blink::WebInputEvent& event,
                      blink::WebWidget::HandledEventCallback callback) {
    GetWebWidget()->ProcessInputEventSynchronouslyForTesting(
        blink::WebCoalescedInputEvent(event.Clone(), {}, {}, ui::LatencyInfo()),
        std::move(callback));
  }

  IPC::TestSink* sink() { return &sink_; }

  bool OverscrollGestureEvent(const blink::WebGestureEvent& event) {
    if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate) {
      GetWebWidget()->DidOverscrollForTesting(
          gfx::Vector2dF(event.data.scroll_update.delta_x,
                         event.data.scroll_update.delta_y),
          gfx::Vector2dF(event.data.scroll_update.delta_x,
                         event.data.scroll_update.delta_y),
          event.PositionInWidget(),
          gfx::Vector2dF(event.data.scroll_update.velocity_x,
                         event.data.scroll_update.velocity_y));
      return true;
    }
    return false;
  }

 protected:
  bool Send(IPC::Message* msg) override {
    sink_.OnMessageReceived(*msg);
    delete msg;
    return true;
  }

 private:
  IPC::TestSink sink_;
  static int next_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(InteractiveRenderWidget);
};

int InteractiveRenderWidget::next_routing_id_ = 0;

class RenderWidgetUnittest : public testing::Test {
 public:
  explicit RenderWidgetUnittest(bool is_for_nested_main_frame = false)
      : is_for_nested_main_frame_(is_for_nested_main_frame) {}

  void SetUp() override {
    mojo::AssociatedRemote<blink::mojom::FrameWidget> frame_widget_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
        frame_widget_receiver =
            frame_widget_remote.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> frame_widget_host;
    mojo::PendingAssociatedReceiver<blink::mojom::FrameWidgetHost>
        frame_widget_host_receiver =
            frame_widget_host.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::Widget> widget_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::Widget> widget_receiver =
        widget_remote.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::WidgetHost> widget_host;
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
        widget_host_receiver =
            widget_host.BindNewEndpointAndPassDedicatedReceiver();

    web_view_ = blink::WebView::Create(/*client=*/&web_view_client_,
                                       /*is_hidden=*/false,
                                       /*is_inside_portal=*/false,
                                       /*compositing_enabled=*/true, nullptr,
                                       mojo::NullAssociatedReceiver());
    agent_scheduling_group_ = CreateAgentSchedulingGroup(render_thread_);
    widget_ = std::make_unique<InteractiveRenderWidget>(
        *agent_scheduling_group_, &compositor_deps_);
    web_local_frame_ = blink::WebLocalFrame::CreateMainFrame(
        web_view_, &web_frame_client_, nullptr,
        base::UnguessableToken::Create(), nullptr);
    web_frame_widget_ = blink::WebFrameWidget::CreateForMainFrame(
        widget_.get(), web_local_frame_, frame_widget_host.Unbind(),
        std::move(frame_widget_receiver), widget_host.Unbind(),
        std::move(widget_receiver), is_for_nested_main_frame_);
    widget_->Init(web_frame_widget_, blink::ScreenInfo());
    web_view_->DidAttachLocalMainFrame();
  }

  void TearDown() override {
    widget_->Close(std::move(widget_));
    web_local_frame_ = nullptr;
    web_frame_widget_ = nullptr;
    web_view_ = nullptr;
    // RenderWidget::Close() posts some destruction. Don't leak them.
    base::RunLoop loop;
    compositor_deps_.GetCleanupTaskRunner()->PostTask(FROM_HERE,
                                                      loop.QuitClosure());
    loop.Run();
  }

  InteractiveRenderWidget* widget() const { return widget_.get(); }

  blink::WebFrameWidget* frame_widget() const { return web_frame_widget_; }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  cc::FakeLayerTreeFrameSink* GetFrameSink() {
    return compositor_deps_.last_created_frame_sink();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  RenderProcess render_process_;
  MockRenderThread render_thread_;
  blink::WebViewClient web_view_client_;
  blink::WebView* web_view_;
  blink::WebLocalFrame* web_local_frame_;
  blink::WebFrameWidget* web_frame_widget_;
  blink::WebLocalFrameClient web_frame_client_;
  FakeCompositorDependencies compositor_deps_;
  std::unique_ptr<AgentSchedulingGroup> agent_scheduling_group_;
  std::unique_ptr<InteractiveRenderWidget> widget_;
  base::HistogramTester histogram_tester_;
  const bool is_for_nested_main_frame_;
};

class RenderWidgetExternalWidgetUnittest : public testing::Test {
 public:
  void SetUp() override {
    mojo::PendingAssociatedRemote<blink::mojom::WidgetHost> widget_host_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::Widget> widget_receiver;
    std::tie(widget_host_remote, widget_receiver) =
        render_widget_host_.BindNewWidgetInterfaces();
    external_web_widget_ = blink::WebExternalWidget::Create(
        &mock_web_external_widget_client_, blink::WebURL(),
        std::move(widget_host_remote), std::move(widget_receiver));

    agent_scheduling_group_ = CreateAgentSchedulingGroup(render_thread_);
    widget_ = std::make_unique<InteractiveRenderWidget>(
        *agent_scheduling_group_, &compositor_deps_);
    widget_->Init(external_web_widget_.get(), blink::ScreenInfo());
  }

  void TearDown() override {
    widget_->Close(std::move(widget_));
    // RenderWidget::Close() posts some destruction. Don't leak them.
    base::RunLoop loop;
    compositor_deps_.GetCleanupTaskRunner()->PostTask(FROM_HERE,
                                                      loop.QuitClosure());
    loop.Run();
  }

  InteractiveRenderWidget* widget() const { return widget_.get(); }

  MockWebExternalWidgetClient* mock_web_external_widget_client() {
    return &mock_web_external_widget_client_;
  }

  blink::WebExternalWidget* external_web_widget() {
    return external_web_widget_.get();
  }

  FakeRenderWidgetHost* render_widget_host() { return &render_widget_host_; }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  cc::FakeLayerTreeFrameSink* GetFrameSink() {
    return compositor_deps_.last_created_frame_sink();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  RenderProcess render_process_;
  MockRenderThread render_thread_;
  FakeRenderWidgetHost render_widget_host_;
  FakeCompositorDependencies compositor_deps_;
  MockWebExternalWidgetClient mock_web_external_widget_client_;
  std::unique_ptr<blink::WebExternalWidget> external_web_widget_;
  std::unique_ptr<AgentSchedulingGroup> agent_scheduling_group_;
  std::unique_ptr<InteractiveRenderWidget> widget_;
  base::HistogramTester histogram_tester_;
};

class SetCursorInterceptor
    : public blink::mojom::WidgetHostInterceptorForTesting {
 public:
  explicit SetCursorInterceptor(FakeRenderWidgetHost* render_widget_host)
      : render_widget_host_(render_widget_host) {
    render_widget_host_->widget_host_receiver_for_testing().SwapImplForTesting(
        this);
  }
  ~SetCursorInterceptor() override = default;

  WidgetHost* GetForwardingInterface() override { return render_widget_host_; }

  void SetCursor(const ui::Cursor& cursor) override { set_cursor_count_++; }

  int set_cursor_count() { return set_cursor_count_; }

 private:
  FakeRenderWidgetHost* render_widget_host_;
  int set_cursor_count_ = 0;
};

TEST_F(RenderWidgetExternalWidgetUnittest, CursorChange) {
  ui::Cursor cursor;

  auto set_cursor_interceptor =
      std::make_unique<SetCursorInterceptor>(render_widget_host());
  widget()->GetWebWidget()->SetCursor(cursor);
  render_widget_host()->widget_host_receiver_for_testing().FlushForTesting();
  EXPECT_EQ(set_cursor_interceptor->set_cursor_count(), 1);

  widget()->GetWebWidget()->SetCursor(cursor);
  render_widget_host()->widget_host_receiver_for_testing().FlushForTesting();
  EXPECT_EQ(set_cursor_interceptor->set_cursor_count(), 1);

  EXPECT_CALL(*mock_web_external_widget_client(), HandleInputEvent(_))
      .WillOnce(::testing::Return(blink::WebInputEventResult::kNotHandled));
  widget()->SendInputEvent(blink::SyntheticWebMouseEventBuilder::Build(
                               blink::WebInputEvent::Type::kMouseLeave),
                           base::DoNothing());
  render_widget_host()->widget_host_receiver_for_testing().FlushForTesting();
  EXPECT_EQ(set_cursor_interceptor->set_cursor_count(), 1);

  widget()->GetWebWidget()->SetCursor(cursor);
  render_widget_host()->widget_host_receiver_for_testing().FlushForTesting();
  EXPECT_EQ(set_cursor_interceptor->set_cursor_count(), 2);
}

TEST_F(RenderWidgetExternalWidgetUnittest, EventOverscroll) {
  ON_CALL(*mock_web_external_widget_client(), WillHandleGestureEvent(_))
      .WillByDefault(testing::Invoke(
          widget(), &InteractiveRenderWidget::OverscrollGestureEvent));

  EXPECT_CALL(*mock_web_external_widget_client(), HandleInputEvent(_))
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  blink::WebGestureEvent scroll(
      blink::WebInputEvent::Type::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  scroll.SetPositionInWidget(gfx::PointF(-10, 0));
  scroll.data.scroll_update.delta_y = 10;
  MockHandledEventCallback handled_event;

  blink::InputHandlerProxy::DidOverscrollParams expected_overscroll;
  expected_overscroll.latest_overscroll_delta = gfx::Vector2dF(0, 10);
  expected_overscroll.accumulated_overscroll = gfx::Vector2dF(0, 10);
  expected_overscroll.causal_event_viewport_point = gfx::PointF(-10, 0);
  expected_overscroll.current_fling_velocity = gfx::Vector2dF();

  // Overscroll notifications received while handling an input event should
  // be bundled with the event ack IPC.
  EXPECT_CALL(handled_event, Run(blink::mojom::InputEventResultState::kConsumed,
                                 _, testing::Pointee(expected_overscroll), _))
      .Times(1);

  widget()->SendInputEvent(scroll, handled_event.GetCallback());
}

TEST_F(RenderWidgetExternalWidgetUnittest, RenderWidgetInputEventUmaMetrics) {
  blink::SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);
  touch.touch_start_or_first_touch_move = true;

  EXPECT_CALL(*mock_web_external_widget_client(), HandleInputEvent(_))
      .Times(5)
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  EXPECT_CALL(*mock_web_external_widget_client(), DispatchBufferedTouchEvents())
      .Times(5)
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  widget()->SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_CANCELABLE, 1);

  touch.dispatch_type = blink::WebInputEvent::DispatchType::kEventNonBlocking;
  widget()->SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE,
                                       1);

  touch.dispatch_type =
      blink::WebInputEvent::DispatchType::kListenersNonBlockingPassive;
  widget()->SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_PASSIVE, 1);

  touch.dispatch_type =
      blink::WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
  widget()->SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING, 1);

  touch.MovePoint(0, 10, 10);
  touch.touch_start_or_first_touch_move = true;
  touch.dispatch_type =
      blink::WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
  widget()->SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING, 2);

  EXPECT_CALL(*mock_web_external_widget_client(), HandleInputEvent(_))
      .WillOnce(::testing::Return(blink::WebInputEventResult::kNotHandled));
  EXPECT_CALL(*mock_web_external_widget_client(), DispatchBufferedTouchEvents())
      .WillOnce(
          ::testing::Return(blink::WebInputEventResult::kHandledSuppressed));
  touch.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  widget()->SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED, 1);

  EXPECT_CALL(*mock_web_external_widget_client(), HandleInputEvent(_))
      .WillOnce(::testing::Return(blink::WebInputEventResult::kNotHandled));
  EXPECT_CALL(*mock_web_external_widget_client(), DispatchBufferedTouchEvents())
      .WillOnce(
          ::testing::Return(blink::WebInputEventResult::kHandledApplication));
  touch.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  widget()->SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED, 1);
}

// Ensures that the compositor thread gets sent the gesture event & overscroll
// amount for an overscroll initiated by a touchpad.
TEST_F(RenderWidgetExternalWidgetUnittest, SendElasticOverscrollForTouchpad) {
  blink::WebGestureEvent scroll(
      blink::WebInputEvent::Type::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::WebGestureDevice::kTouchpad);
  scroll.SetPositionInWidget(gfx::PointF(-10, 0));
  scroll.data.scroll_update.delta_y = 10;

  // We only really care that DidHandleGestureScrollEvent was called; we
  // therefore suppress the warning for the call to
  // mock_webwidget()->HandleInputEvent().
  EXPECT_CALL(*mock_web_external_widget_client(),
              DidHandleGestureScrollEvent(_, _, _, _))
      .Times(1);
  EXPECT_CALL(*mock_web_external_widget_client(), HandleInputEvent(_))
      .Times(testing::AnyNumber());

  widget()->SendInputEvent(scroll, base::DoNothing());
}

// Ensures that the compositor thread gets sent the gesture event & overscroll
// amount for an overscroll initiated by a touchscreen.
TEST_F(RenderWidgetExternalWidgetUnittest,
       SendElasticOverscrollForTouchscreen) {
  blink::WebGestureEvent scroll(
      blink::WebInputEvent::Type::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::WebGestureDevice::kTouchscreen);
  scroll.SetPositionInWidget(gfx::PointF(-10, 0));
  scroll.data.scroll_update.delta_y = 10;

  // We only really care that DidHandleGestureScrollEvent was called; we
  // therefore suppress the warning for the call to
  // mock_webwidget()->HandleInputEvent().
  EXPECT_CALL(*mock_web_external_widget_client(),
              DidHandleGestureScrollEvent(_, _, _, _))
      .Times(1);
  EXPECT_CALL(*mock_web_external_widget_client(), HandleInputEvent(_))
      .Times(testing::AnyNumber());

  widget()->SendInputEvent(scroll, base::DoNothing());
}

class StubRenderWidgetDelegate : public RenderWidgetDelegate {
 public:
  void SetActiveForWidget(bool active) override {}
  bool SupportsMultipleWindowsForWidget() override { return true; }
  bool ShouldAckSyntheticInputImmediately() override { return true; }
  void DidCommitCompositorFrameForWidget() override {}
  void DidCompletePageScaleAnimationForWidget() override {}
  void ResizeWebWidgetForWidget(const gfx::Size& main_frame_widget_size,
                                const gfx::Size& visible_viewport_size,
                                cc::BrowserControlsParams) override {}
};

class RenderWidgetSubFrameUnittest : public RenderWidgetUnittest {
 public:
  RenderWidgetSubFrameUnittest()
      : RenderWidgetUnittest(/*is_for_nested_main_frame=*/true) {}
};

// Tests that the value of VisualProperties::is_pinch_gesture_active is
// propagated to the LayerTreeHost when properties are synced for subframes.
TEST_F(RenderWidgetSubFrameUnittest,
       ActivePinchGestureUpdatesLayerTreeHostSubFrame) {
  cc::LayerTreeHost* layer_tree_host = widget()->layer_tree_host();
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
  blink::VisualProperties visual_properties;

  // Sync visual properties on a child RenderWidget.
  visual_properties.is_pinch_gesture_active = true;
  widget()->GetWebWidget()->ApplyVisualProperties(visual_properties);
  // We expect the |is_pinch_gesture_active| value to propagate to the
  // LayerTreeHost for sub-frames. Since GesturePinch events are handled
  // directly in the main-frame's layer tree (and only there), information about
  // whether or not we're in a pinch gesture must be communicated separately to
  // sub-frame layer trees, via OnUpdateVisualProperties. This information
  // is required to allow sub-frame compositors to throttle rastering while
  // pinch gestures are active.
  EXPECT_TRUE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
  visual_properties.is_pinch_gesture_active = false;
  widget()->GetWebWidget()->ApplyVisualProperties(visual_properties);
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
}

#if defined(OS_ANDROID)
TEST_F(RenderWidgetUnittest, ForceSendMetadataOnInput) {
  cc::LayerTreeHost* layer_tree_host = widget()->layer_tree_host();
  // We should not have any force send metadata requests at start.
  EXPECT_FALSE(layer_tree_host->TakeForceSendMetadataRequest());
  // ShowVirtualKeyboard will trigger a text input state update.
  widget()->GetWebWidget()->ShowVirtualKeyboard();
  // We should now have a force send metadata request.
  EXPECT_TRUE(layer_tree_host->TakeForceSendMetadataRequest());
}
#endif  // !defined(OS_ANDROID)

class NotifySwapTimesRenderWidgetUnittest : public RenderWidgetUnittest {
 public:
  void SetUp() override {
    RenderWidgetUnittest::SetUp();

    viz::ParentLocalSurfaceIdAllocator allocator;

    // TODO(danakj): This usually happens through
    // RenderWidget::UpdateVisualProperties() and we are cutting past that for
    // some reason.
    allocator.GenerateId();
    widget()->layer_tree_host()->SetViewportRectAndScale(
        gfx::Rect(200, 100), 1.f, allocator.GetCurrentLocalSurfaceId());

    auto root_layer = cc::SolidColorLayer::Create();
    root_layer->SetBounds(gfx::Size(200, 100));
    root_layer->SetBackgroundColor(SK_ColorGREEN);
    widget()->layer_tree_host()->SetNonBlinkManagedRootLayer(root_layer);

    auto color_layer = cc::SolidColorLayer::Create();
    color_layer->SetBounds(gfx::Size(100, 100));
    root_layer->AddChild(color_layer);
    color_layer->SetBackgroundColor(SK_ColorRED);
  }

  // |swap_to_presentation| determines how long after swap should presentation
  // happen. This can be negative, positive, or zero. If zero, an invalid (null)
  // presentation time is used.
  void CompositeAndWaitForPresentation(base::TimeDelta swap_to_presentation) {
    base::RunLoop swap_run_loop;
    base::RunLoop presentation_run_loop;

    // Register callbacks for swap time and presentation time.
    base::TimeTicks swap_time;
    frame_widget()->NotifySwapAndPresentationTime(
        base::BindOnce(
            [](base::OnceClosure swap_quit_closure, base::TimeTicks* swap_time,
               blink::WebSwapResult result, base::TimeTicks timestamp) {
              DCHECK(!timestamp.is_null());
              *swap_time = timestamp;
              std::move(swap_quit_closure).Run();
            },
            swap_run_loop.QuitClosure(), &swap_time),
        base::BindOnce(
            [](base::OnceClosure presentation_quit_closure,
               blink::WebSwapResult result, base::TimeTicks timestamp) {
              DCHECK(!timestamp.is_null());
              std::move(presentation_quit_closure).Run();
            },
            presentation_run_loop.QuitClosure()));

    // Composite and wait for the swap to complete.
    widget()->layer_tree_host()->Composite(base::TimeTicks::Now(),
                                           /*raster=*/true);
    swap_run_loop.Run();

    // Present and wait for it to complete.
    viz::FrameTimingDetails timing_details;
    if (!swap_to_presentation.is_zero()) {
      timing_details.presentation_feedback = gfx::PresentationFeedback(
          /*presentation_time=*/swap_time + swap_to_presentation,
          base::TimeDelta::FromMilliseconds(16), 0);
    }
    GetFrameSink()->NotifyDidPresentCompositorFrame(1, timing_details);
    presentation_run_loop.Run();
  }
};

TEST_F(NotifySwapTimesRenderWidgetUnittest, PresentationTimestampValid) {
  base::HistogramTester histograms;

  CompositeAndWaitForPresentation(base::TimeDelta::FromMilliseconds(2));

  EXPECT_THAT(histograms.GetAllSamples(
                  "PageLoad.Internal.Renderer.PresentationTime.Valid"),
              testing::ElementsAre(base::Bucket(true, 1)));
  EXPECT_THAT(
      histograms.GetAllSamples(
          "PageLoad.Internal.Renderer.PresentationTime.DeltaFromSwapTime"),
      testing::ElementsAre(base::Bucket(2, 1)));
}

TEST_F(NotifySwapTimesRenderWidgetUnittest, PresentationTimestampInvalid) {
  base::HistogramTester histograms;

  CompositeAndWaitForPresentation(base::TimeDelta());

  EXPECT_THAT(histograms.GetAllSamples(
                  "PageLoad.Internal.Renderer.PresentationTime.Valid"),
              testing::ElementsAre(base::Bucket(false, 1)));
  EXPECT_THAT(
      histograms.GetAllSamples(
          "PageLoad.Internal.Renderer.PresentationTime.DeltaFromSwapTime"),
      testing::IsEmpty());
}

TEST_F(NotifySwapTimesRenderWidgetUnittest,
       PresentationTimestampEarlierThanSwaptime) {
  base::HistogramTester histograms;

  CompositeAndWaitForPresentation(base::TimeDelta::FromMilliseconds(-2));

  EXPECT_THAT(histograms.GetAllSamples(
                  "PageLoad.Internal.Renderer.PresentationTime.Valid"),
              testing::ElementsAre(base::Bucket(false, 1)));
  EXPECT_THAT(
      histograms.GetAllSamples(
          "PageLoad.Internal.Renderer.PresentationTime.DeltaFromSwapTime"),
      testing::IsEmpty());
}

}  // namespace content
