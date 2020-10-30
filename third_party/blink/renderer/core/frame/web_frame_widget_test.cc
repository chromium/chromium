// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "cc/layers/solid_color_layer.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

using testing::_;

bool operator==(const InputHandlerProxy::DidOverscrollParams& lhs,
                const InputHandlerProxy::DidOverscrollParams& rhs) {
  return lhs.accumulated_overscroll == rhs.accumulated_overscroll &&
         lhs.latest_overscroll_delta == rhs.latest_overscroll_delta &&
         lhs.current_fling_velocity == rhs.current_fling_velocity &&
         lhs.causal_event_viewport_point == rhs.causal_event_viewport_point &&
         lhs.overscroll_behavior == rhs.overscroll_behavior;
}

class WebFrameWidgetSimTest : public SimTest {};

// Tests that if a WebView is auto-resized, the associated
// WebViewFrameWidget requests a new viz::LocalSurfaceId to be allocated on the
// impl thread.
TEST_F(WebFrameWidgetSimTest, AutoResizeAllocatedLocalSurfaceId) {
  viz::ParentLocalSurfaceIdAllocator allocator;

  // Enable auto-resize.
  VisualProperties visual_properties;
  visual_properties.auto_resize_enabled = true;
  visual_properties.min_size_for_auto_resize = gfx::Size(100, 100);
  visual_properties.max_size_for_auto_resize = gfx::Size(200, 200);
  allocator.GenerateId();
  visual_properties.local_surface_id = allocator.GetCurrentLocalSurfaceId();
  WebView().MainFrameWidget()->ApplyVisualProperties(visual_properties);
  WebView().MainFrameViewWidget()->UpdateSurfaceAndScreenInfo(
      visual_properties.local_surface_id.value(),
      visual_properties.compositor_viewport_pixel_rect,
      visual_properties.screen_info);

  EXPECT_EQ(allocator.GetCurrentLocalSurfaceId(),
            WebView().MainFrameViewWidget()->LocalSurfaceIdFromParent());
  EXPECT_FALSE(WebView()
                   .MainFrameViewWidget()
                   ->LayerTreeHost()
                   ->new_local_surface_id_request_for_testing());

  constexpr gfx::Size size(200, 200);
  static_cast<WebViewFrameWidget*>(WebView().MainFrameViewWidget())
      ->DidAutoResize(size);
  EXPECT_EQ(allocator.GetCurrentLocalSurfaceId(),
            WebView().MainFrameViewWidget()->LocalSurfaceIdFromParent());
  EXPECT_TRUE(WebView()
                  .MainFrameViewWidget()
                  ->LayerTreeHost()
                  ->new_local_surface_id_request_for_testing());
}

TEST_F(WebFrameWidgetSimTest, FrameSinkIdHitTestAPI) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <style>
      html, body {
        margin :0px;
        padding: 0px;
      }
      </style>

      <div style='background: green; padding: 100px; margin: 0px;'>
        <iframe style='width: 200px; height: 100px;'
          srcdoc='<body style="margin : 0px; height : 100px; width : 200px;">
          </body>'>
        </iframe>
      </div>

      )HTML");

  gfx::PointF point;
  viz::FrameSinkId main_frame_sink_id =
      WebView().MainFrameViewWidget()->GetFrameSinkIdAtPoint(
          gfx::PointF(10.43, 10.74), &point);
  EXPECT_EQ(WebView().MainFrameViewWidget()->GetFrameSinkId(),
            main_frame_sink_id);
  EXPECT_EQ(gfx::PointF(10.43, 10.74), point);

  // Targeting a child frame should also return the FrameSinkId for the main
  // widget.
  viz::FrameSinkId frame_sink_id =
      WebView().MainFrameViewWidget()->GetFrameSinkIdAtPoint(
          gfx::PointF(150.27, 150.25), &point);
  EXPECT_EQ(main_frame_sink_id, frame_sink_id);
  EXPECT_EQ(gfx::PointF(150.27, 150.25), point);
}

#if defined(OS_ANDROID)
TEST_F(WebFrameWidgetSimTest, ForceSendMetadataOnInput) {
  cc::LayerTreeHost* layer_tree_host =
      WebView().MainFrameViewWidget()->LayerTreeHost();
  // We should not have any force send metadata requests at start.
  EXPECT_FALSE(layer_tree_host->TakeForceSendMetadataRequest());
  // ShowVirtualKeyboard will trigger a text input state update.
  WebView().MainFrameViewWidget()->ShowVirtualKeyboard();
  // We should now have a force send metadata request.
  EXPECT_TRUE(layer_tree_host->TakeForceSendMetadataRequest());
}
#endif  // defined(OS_ANDROID)

// A test that forces a RemoteMainFrame to be created and the LocalFrameRoot
// to be a WebFrameWidgetImpl.
class WebFrameWidgetImplSimTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    InitializeRemote();
    CHECK(static_cast<WebFrameWidgetBase*>(LocalFrameRoot().FrameWidget())
              ->ForSubframe());
  }

  WebFrameWidgetImpl* LocalFrameRootWidget() {
    return static_cast<WebFrameWidgetImpl*>(LocalFrameRoot().FrameWidget());
  }
};

// Tests that the value of VisualProperties::is_pinch_gesture_active is
// propagated to the LayerTreeHost when properties are synced for child local
// roots.
TEST_F(WebFrameWidgetImplSimTest,
       ActivePinchGestureUpdatesLayerTreeHostSubFrame) {
  cc::LayerTreeHost* layer_tree_host = LocalFrameRootWidget()->LayerTreeHost();
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
  blink::VisualProperties visual_properties;

  // Sync visual properties on a child widget.
  visual_properties.is_pinch_gesture_active = true;
  LocalFrameRootWidget()->ApplyVisualProperties(visual_properties);
  // We expect the |is_pinch_gesture_active| value to propagate to the
  // LayerTreeHost for sub-frames. Since GesturePinch events are handled
  // directly in the main-frame's layer tree (and only there), information about
  // whether or not we're in a pinch gesture must be communicated separately to
  // sub-frame layer trees, via OnUpdateVisualProperties. This information
  // is required to allow sub-frame compositors to throttle rastering while
  // pinch gestures are active.
  EXPECT_TRUE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
  visual_properties.is_pinch_gesture_active = false;
  LocalFrameRootWidget()->ApplyVisualProperties(visual_properties);
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
}

const char EVENT_LISTENER_RESULT_HISTOGRAM[] = "Event.PassiveListeners";

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

// Since std::unique_ptr<InputHandlerProxy::DidOverscrollParams> isn't copyable
// we can't use the MockCallback template.
class MockHandledEventCallback {
 public:
  MockHandledEventCallback() = default;
  MOCK_METHOD4_T(Run,
                 void(mojom::InputEventResultState,
                      const ui::LatencyInfo&,
                      InputHandlerProxy::DidOverscrollParams*,
                      base::Optional<cc::TouchAction>));

  WebWidget::HandledEventCallback GetCallback() {
    return base::BindOnce(&MockHandledEventCallback::HandleCallback,
                          base::Unretained(this));
  }

 private:
  void HandleCallback(
      mojom::InputEventResultState ack_state,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<InputHandlerProxy::DidOverscrollParams> overscroll,
      base::Optional<cc::TouchAction> touch_action) {
    Run(ack_state, latency_info, overscroll.get(), touch_action);
  }

  DISALLOW_COPY_AND_ASSIGN(MockHandledEventCallback);
};

class MockWebViewFrameWidget : public WebViewFrameWidget {
 public:
  template <typename... Args>
  explicit MockWebViewFrameWidget(Args&&... args)
      : WebViewFrameWidget(std::forward<Args>(args)...) {}

  MOCK_METHOD1(HandleInputEvent,
               WebInputEventResult(const WebCoalescedInputEvent&));
  MOCK_METHOD0(DispatchBufferedTouchEvents, WebInputEventResult());

  MOCK_METHOD4(ObserveGestureEventAndResult,
               void(const WebGestureEvent& gesture_event,
                    const gfx::Vector2dF& unused_delta,
                    const cc::OverscrollBehavior& overscroll_behavior,
                    bool event_processed));

  MOCK_METHOD1(WillHandleGestureEvent, bool(const WebGestureEvent& event));
};

WebViewFrameWidget* CreateWebViewFrameWidget(
    util::PassKey<WebFrameWidget> pass_key,
    WebWidgetClient& client,
    WebViewImpl& web_view_impl,
    CrossVariantMojoAssociatedRemote<mojom::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const viz::FrameSinkId& frame_sink_id,
    bool is_for_nested_main_frame,
    bool hidden,
    bool never_composited) {
  return MakeGarbageCollected<MockWebViewFrameWidget>(
      pass_key, client, web_view_impl, std::move(frame_widget_host),
      std::move(frame_widget), std::move(widget_host), std::move(widget),
      std::move(task_runner), frame_sink_id, is_for_nested_main_frame, hidden,
      never_composited);
}

class WebViewFrameWidgetSimTest : public SimTest {
 public:
  void SetUp() override {
    InstallCreateWebViewFrameWidgetHook(CreateWebViewFrameWidget);
    SimTest::SetUp();
  }

  MockWebViewFrameWidget* MockMainFrameWidget() {
    return static_cast<MockWebViewFrameWidget*>(MainFrame().FrameWidget());
  }

  void SendInputEvent(const WebInputEvent& event,
                      WebWidget::HandledEventCallback callback) {
    MockMainFrameWidget()->ProcessInputEventSynchronouslyForTesting(
        WebCoalescedInputEvent(event.Clone(), {}, {}, ui::LatencyInfo()),
        std::move(callback));
  }
  bool OverscrollGestureEvent(const blink::WebGestureEvent& event) {
    if (event.GetType() == WebInputEvent::Type::kGestureScrollUpdate) {
      MockMainFrameWidget()->DidOverscroll(
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

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  base::HistogramTester histogram_tester_;
};

TEST_F(WebViewFrameWidgetSimTest, CursorChange) {
  ui::Cursor cursor;

  MockMainFrameWidget()->SetCursor(cursor);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebWidgetClient().CursorSetCount(), 1u);

  MockMainFrameWidget()->SetCursor(cursor);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebWidgetClient().CursorSetCount(), 1u);

  EXPECT_CALL(*MockMainFrameWidget(), HandleInputEvent(_))
      .WillOnce(::testing::Return(WebInputEventResult::kNotHandled));
  SendInputEvent(
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::Type::kMouseLeave),
      base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebWidgetClient().CursorSetCount(), 1u);

  MockMainFrameWidget()->SetCursor(cursor);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebWidgetClient().CursorSetCount(), 2u);
}

TEST_F(WebViewFrameWidgetSimTest, EventOverscroll) {
  ON_CALL(*MockMainFrameWidget(), WillHandleGestureEvent(_))
      .WillByDefault(testing::Invoke(
          this, &WebViewFrameWidgetSimTest::OverscrollGestureEvent));
  EXPECT_CALL(*MockMainFrameWidget(), HandleInputEvent(_))
      .WillRepeatedly(::testing::Return(WebInputEventResult::kNotHandled));

  WebGestureEvent scroll(WebInputEvent::Type::kGestureScrollUpdate,
                         WebInputEvent::kNoModifiers, base::TimeTicks::Now());
  scroll.SetPositionInWidget(gfx::PointF(-10, 0));
  scroll.data.scroll_update.delta_y = 10;
  MockHandledEventCallback handled_event;

  InputHandlerProxy::DidOverscrollParams expected_overscroll;
  expected_overscroll.latest_overscroll_delta = gfx::Vector2dF(0, 10);
  expected_overscroll.accumulated_overscroll = gfx::Vector2dF(0, 10);
  expected_overscroll.causal_event_viewport_point = gfx::PointF(-10, 0);
  expected_overscroll.current_fling_velocity = gfx::Vector2dF();
  // Overscroll notifications received while handling an input event should
  // be bundled with the event ack IPC.
  EXPECT_CALL(handled_event, Run(mojom::InputEventResultState::kConsumed, _,
                                 testing::Pointee(expected_overscroll), _))
      .Times(1);

  SendInputEvent(scroll, handled_event.GetCallback());
}

TEST_F(WebViewFrameWidgetSimTest, RenderWidgetInputEventUmaMetrics) {
  SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);
  touch.touch_start_or_first_touch_move = true;

  EXPECT_CALL(*MockMainFrameWidget(), HandleInputEvent(_))
      .Times(5)
      .WillRepeatedly(::testing::Return(WebInputEventResult::kNotHandled));
  EXPECT_CALL(*MockMainFrameWidget(), DispatchBufferedTouchEvents())
      .Times(5)
      .WillRepeatedly(::testing::Return(WebInputEventResult::kNotHandled));
  SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_CANCELABLE, 1);

  touch.dispatch_type = WebInputEvent::DispatchType::kEventNonBlocking;
  SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE,
                                       1);

  touch.dispatch_type =
      WebInputEvent::DispatchType::kListenersNonBlockingPassive;
  SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_PASSIVE, 1);

  touch.dispatch_type =
      WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
  SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING, 1);

  touch.MovePoint(0, 10, 10);
  touch.touch_start_or_first_touch_move = true;
  touch.dispatch_type =
      WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
  SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING, 2);

  EXPECT_CALL(*MockMainFrameWidget(), HandleInputEvent(_))
      .WillOnce(::testing::Return(WebInputEventResult::kNotHandled));
  EXPECT_CALL(*MockMainFrameWidget(), DispatchBufferedTouchEvents())
      .WillOnce(::testing::Return(WebInputEventResult::kHandledSuppressed));
  touch.dispatch_type = WebInputEvent::DispatchType::kBlocking;
  SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED, 1);

  EXPECT_CALL(*MockMainFrameWidget(), HandleInputEvent(_))
      .WillOnce(::testing::Return(WebInputEventResult::kNotHandled));
  EXPECT_CALL(*MockMainFrameWidget(), DispatchBufferedTouchEvents())
      .WillOnce(::testing::Return(WebInputEventResult::kHandledApplication));
  touch.dispatch_type = WebInputEvent::DispatchType::kBlocking;
  SendInputEvent(touch, base::DoNothing());
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED, 1);
}

// Ensures that the compositor thread gets sent the gesture event & overscroll
// amount for an overscroll initiated by a touchpad.
TEST_F(WebViewFrameWidgetSimTest, SendElasticOverscrollForTouchpad) {
  WebGestureEvent scroll(WebInputEvent::Type::kGestureScrollUpdate,
                         WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                         WebGestureDevice::kTouchpad);
  scroll.SetPositionInWidget(gfx::PointF(-10, 0));
  scroll.data.scroll_update.delta_y = 10;

  // We only really care that ObserveGestureEventAndResult was called; we
  // therefore suppress the warning for the call to
  // HandleInputEvent().
  EXPECT_CALL(*MockMainFrameWidget(), ObserveGestureEventAndResult(_, _, _, _))
      .Times(1);
  EXPECT_CALL(*MockMainFrameWidget(), HandleInputEvent(_))
      .Times(testing::AnyNumber());

  SendInputEvent(scroll, base::DoNothing());
}

// Ensures that the compositor thread gets sent the gesture event & overscroll
// amount for an overscroll initiated by a touchscreen.
TEST_F(WebViewFrameWidgetSimTest, SendElasticOverscrollForTouchscreen) {
  WebGestureEvent scroll(WebInputEvent::Type::kGestureScrollUpdate,
                         WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                         WebGestureDevice::kTouchscreen);
  scroll.SetPositionInWidget(gfx::PointF(-10, 0));
  scroll.data.scroll_update.delta_y = 10;

  // We only really care that ObserveGestureEventAndResult was called; we
  // therefore suppress the warning for the call to
  // HandleInputEvent().
  EXPECT_CALL(*MockMainFrameWidget(), ObserveGestureEventAndResult(_, _, _, _))
      .Times(1);
  EXPECT_CALL(*MockMainFrameWidget(), HandleInputEvent(_))
      .Times(testing::AnyNumber());

  SendInputEvent(scroll, base::DoNothing());
}

class NotifySwapTimesWebFrameWidgetTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();

    WebView().StopDeferringMainFrameUpdate();
    FrameWidgetBase()->UpdateCompositorViewportRect(gfx::Rect(200, 100));

    auto root_layer = cc::SolidColorLayer::Create();
    root_layer->SetBounds(gfx::Size(200, 100));
    root_layer->SetBackgroundColor(SK_ColorGREEN);
    FrameWidgetBase()->LayerTreeHost()->SetRootLayer(root_layer);

    auto color_layer = cc::SolidColorLayer::Create();
    color_layer->SetBounds(gfx::Size(100, 100));
    root_layer->AddChild(color_layer);
    color_layer->SetBackgroundColor(SK_ColorRED);
  }

  WebViewFrameWidget* FrameWidgetBase() {
    return static_cast<WebViewFrameWidget*>(MainFrame().FrameWidget());
  }

  // |swap_to_presentation| determines how long after swap should presentation
  // happen. This can be negative, positive, or zero. If zero, an invalid (null)
  // presentation time is used.
  void CompositeAndWaitForPresentation(base::TimeDelta swap_to_presentation) {
    base::RunLoop swap_run_loop;
    base::RunLoop presentation_run_loop;

    // Register callbacks for swap time and presentation time.
    base::TimeTicks swap_time;
    MainFrame().FrameWidget()->NotifySwapAndPresentationTime(
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
    Compositor().BeginFrame(/*time_delta_in_seconds=*/0.016, /*raster=*/true);
    swap_run_loop.Run();

    // Present and wait for it to complete.
    viz::FrameTimingDetails timing_details;
    if (!swap_to_presentation.is_zero()) {
      timing_details.presentation_feedback = gfx::PresentationFeedback(
          /*presentation_time=*/swap_time + swap_to_presentation,
          base::TimeDelta::FromMilliseconds(16), 0);
    }
    auto* last_frame_sink = WebWidgetClient().LastCreatedFrameSink();
    last_frame_sink->NotifyDidPresentCompositorFrame(1, timing_details);
    presentation_run_loop.Run();
  }
};

TEST_F(NotifySwapTimesWebFrameWidgetTest, PresentationTimestampValid) {
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

TEST_F(NotifySwapTimesWebFrameWidgetTest, PresentationTimestampInvalid) {
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

TEST_F(NotifySwapTimesWebFrameWidgetTest,
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

// Tests that the value of VisualProperties::is_pinch_gesture_active is
// not propagated to the LayerTreeHost when properties are synced for main
// frame.
TEST_F(WebFrameWidgetSimTest, ActivePinchGestureUpdatesLayerTreeHost) {
  auto* layer_tree_host = WebView().MainFrameViewWidget()->LayerTreeHost();
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
  blink::VisualProperties visual_properties;

  // Sync visual properties on a mainframe RenderWidget.
  visual_properties.is_pinch_gesture_active = true;
  WebView().MainFrameViewWidget()->ApplyVisualProperties(visual_properties);
  // We do not expect the |is_pinch_gesture_active| value to propagate to the
  // LayerTreeHost for the main-frame. Since GesturePinch events are handled
  // directly by the layer tree for the main frame, it already knows whether or
  // not a pinch gesture is active, and so we shouldn't propagate this
  // information to the layer tree for a main-frame's widget.
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
}

}  // namespace blink
