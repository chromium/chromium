// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/property_tree_test_utils.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "ui/base/mojom/window_show_state.mojom-blink.h"

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

namespace {

class TouchMoveEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override { invoked_ = true; }

  bool GetInvokedStateAndReset() {
    bool invoked = invoked_;
    invoked_ = false;
    return invoked;
  }

 private:
  bool invoked_ = false;
};

}  // namespace

class WebFrameWidgetSimTest : public SimTest {};

// Tests that if a WebView is auto-resized, the associated
// WebFrameWidgetImpl requests a new viz::LocalSurfaceId to be allocated on the
// impl thread.
TEST_F(WebFrameWidgetSimTest, AutoResizeAllocatedLocalSurfaceId) {
  LoadURL("about:blank");
  // Resets CommitState::new_local_surface_id_request.
  Compositor().BeginFrame();

  viz::ParentLocalSurfaceIdAllocator allocator;

  // Enable auto-resize.
  VisualProperties visual_properties;
  visual_properties.screen_infos = display::ScreenInfos(display::ScreenInfo());
  visual_properties.auto_resize_enabled = true;
  visual_properties.min_size_for_auto_resize = gfx::Size(100, 100);
  visual_properties.max_size_for_auto_resize = gfx::Size(200, 200);
  allocator.GenerateId();
  visual_properties.local_surface_id = allocator.GetCurrentLocalSurfaceId();
  WebView().MainFrameWidget()->ApplyVisualProperties(visual_properties);
  WebView().MainFrameViewWidget()->UpdateSurfaceAndScreenInfo(
      visual_properties.local_surface_id.value(),
      visual_properties.compositor_viewport_pixel_rect,
      visual_properties.screen_infos);

  EXPECT_EQ(allocator.GetCurrentLocalSurfaceId(),
            WebView().MainFrameViewWidget()->LocalSurfaceIdFromParent());
  EXPECT_FALSE(WebView()
                   .MainFrameViewWidget()
                   ->LayerTreeHostForTesting()
                   ->new_local_surface_id_request_for_testing());

  constexpr gfx::Size size(200, 200);
  WebView().MainFrameViewWidget()->DidAutoResize(size);
  EXPECT_EQ(allocator.GetCurrentLocalSurfaceId(),
            WebView().MainFrameViewWidget()->LocalSurfaceIdFromParent());
  EXPECT_TRUE(WebView()
                  .MainFrameViewWidget()
                  ->LayerTreeHostForTesting()
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

#if BUILDFLAG(IS_ANDROID)
TEST_F(WebFrameWidgetSimTest, ForceSendMetadataOnInput) {
  const cc::LayerTreeHost* layer_tree_host =
      WebView().MainFrameViewWidget()->LayerTreeHostForTesting();
  // We should not have any force send metadata requests at start.
  EXPECT_FALSE(
      layer_tree_host->pending_commit_state()->force_send_metadata_request);
  // ShowVirtualKeyboard will trigger a text input state update.
  WebView().MainFrameViewWidget()->ShowVirtualKeyboard();
  // We should now have a force send metadata request.
  EXPECT_TRUE(
      layer_tree_host->pending_commit_state()->force_send_metadata_request);
}
#endif  // BUILDFLAG(IS_ANDROID)

// A test that forces a RemoteMainFrame to be created.
class WebFrameWidgetImplRemoteFrameSimTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    InitializeRemote();
    CHECK(static_cast<WebFrameWidgetImpl*>(LocalFrameRoot().FrameWidget())
              ->ForSubframe());
  }

  WebFrameWidgetImpl* LocalFrameRootWidget() {
    return static_cast<WebFrameWidgetImpl*>(LocalFrameRoot().FrameWidget());
  }
};

// Tests that the value of VisualProperties::is_pinch_gesture_active is
// propagated to the LayerTreeHost when properties are synced for child local
// roots.
TEST_F(WebFrameWidgetImplRemoteFrameSimTest,
       ActivePinchGestureUpdatesLayerTreeHostSubFrame) {
  cc::LayerTreeHost* layer_tree_host =
      LocalFrameRootWidget()->LayerTreeHostForTesting();
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
  VisualProperties visual_properties;
  visual_properties.screen_infos = display::ScreenInfos(display::ScreenInfo());

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
  MockHandledEventCallback(const MockHandledEventCallback&) = delete;
  MockHandledEventCallback& operator=(const MockHandledEventCallback&) = delete;
  MOCK_METHOD4_T(Run,
                 void(mojom::InputEventResultState,
                      const ui::LatencyInfo&,
                      InputHandlerProxy::DidOverscrollParams*,
                      std::optional<cc::TouchAction>));

  WidgetBaseInputHandler::HandledEventCallback GetCallback() {
    return WTF::BindOnce(&MockHandledEventCallback::HandleCallback,
                         WTF::Unretained(this));
  }

 private:
  void HandleCallback(
      mojom::InputEventResultState ack_state,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<InputHandlerProxy::DidOverscrollParams> overscroll,
      std::optional<cc::TouchAction> touch_action) {
    Run(ack_state, latency_info, overscroll.get(), touch_action);
  }
};

class MockWebFrameWidgetImpl : public frame_test_helpers::TestWebFrameWidget {
 public:
  using frame_test_helpers::TestWebFrameWidget::TestWebFrameWidget;

  MOCK_METHOD1(HandleInputEvent,
               WebInputEventResult(const WebCoalescedInputEvent&));
  MOCK_METHOD0(DispatchBufferedTouchEvents, WebInputEventResult());

  MOCK_METHOD4(ObserveGestureEventAndResult,
               void(const WebGestureEvent& gesture_event,
                    const gfx::Vector2dF& unused_delta,
                    const cc::OverscrollBehavior& overscroll_behavior,
                    bool event_processed));
};

class WebFrameWidgetImplSimTest : public SimTest {
 public:
  frame_test_helpers::TestWebFrameWidget* CreateWebFrameWidget(
      base::PassKey<WebLocalFrame> pass_key,
      CrossVariantMojoAssociatedRemote<
          mojom::blink::FrameWidgetHostInterfaceBase> frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const viz::FrameSinkId& frame_sink_id,
      bool hidden,
      bool never_composited,
      bool is_for_child_local_root,
      bool is_for_nested_main_frame,
      bool is_for_scalable_page) override {
    return MakeGarbageCollected<MockWebFrameWidgetImpl>(
        pass_key, std::move(frame_widget_host), std::move(frame_widget),
        std::move(widget_host), std::move(widget), std::move(task_runner),
        frame_sink_id, hidden, never_composited, is_for_child_local_root,
        is_for_nested_main_frame, is_for_scalable_page);
  }

  MockWebFrameWidgetImpl* MockMainFrameWidget() {
    return static_cast<MockWebFrameWidgetImpl*>(MainFrame().FrameWidget());
  }

  EventHandler& GetEventHandler() {
    return GetDocument().GetFrame()->GetEventHandler();
  }

  void SendInputEvent(const WebInputEvent& event,
                      WidgetBaseInputHandler::HandledEventCallback callback) {
    MockMainFrameWidget()->ProcessInputEventSynchronouslyForTesting(
        WebCoalescedInputEvent(event.Clone(), {}, {}, ui::LatencyInfo()),
        std::move(callback));
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  base::HistogramTester histogram_tester_;
};

TEST_F(WebFrameWidgetImplSimTest, CursorChange) {
  ui::Cursor cursor;

  frame_test_helpers::TestWebFrameWidgetHost& widget_host =
      MockMainFrameWidget()->WidgetHost();

  MockMainFrameWidget()->SetCursor(cursor);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(widget_host.CursorSetCount(), 1u);

  MockMainFrameWidget()->SetCursor(cursor);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(widget_host.CursorSetCount(), 1u);

  EXPECT_CALL(*MockMainFrameWidget(), HandleInputEvent(_))
      .WillOnce(::testing::Return(WebInputEventResult::kNotHandled));
  SendInputEvent(
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::Type::kMouseLeave),
      base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(widget_host.CursorSetCount(), 1u);

  MockMainFrameWidget()->SetCursor(cursor);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(widget_host.CursorSetCount(), 2u);
}

TEST_F(WebFrameWidgetImplSimTest, RenderWidgetInputEventUmaMetrics) {
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
TEST_F(WebFrameWidgetImplSimTest, SendElasticOverscrollForTouchpad) {
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
TEST_F(WebFrameWidgetImplSimTest, SendElasticOverscrollForTouchscreen) {
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

TEST_F(WebFrameWidgetImplSimTest, TestStartStylusWritingForInputElement) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <!doctype html>
      <body style='padding: 0px; width: 400px; height: 400px;'>
      <input type='text' id='first' style='width: 100px; height: 100px;'>
      </body>
      )HTML");
  Compositor().BeginFrame();
  Element* first =
      DynamicTo<Element>(GetDocument().getElementById(AtomicString("first")));
  WebPointerEvent event(
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties(1, WebPointerProperties::PointerType::kPen,
                           WebPointerProperties::Button::kLeft,
                           gfx::PointF(100, 100), gfx::PointF(100, 100)),
      1, 1);
  GetEventHandler().HandlePointerEvent(event, Vector<WebPointerEvent>(),
                                       Vector<WebPointerEvent>());
  EXPECT_EQ(nullptr, GetDocument().FocusedElement());
  MockMainFrameWidget()->OnStartStylusWriting(base::DoNothing());
  EXPECT_EQ(first, GetDocument().FocusedElement());
}

TEST_F(WebFrameWidgetImplSimTest,
       TestStartStylusWritingForContentEditableElement) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <!doctype html>
      <body style='padding: 0px; width: 400px; height: 400px;'>
      <div contenteditable='true' id='first' style='width: 100px; height: 100px;'></div>
      </body>
      )HTML");
  Compositor().BeginFrame();
  Element* first =
      DynamicTo<Element>(GetDocument().getElementById(AtomicString("first")));
  WebPointerEvent event(
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties(1, WebPointerProperties::PointerType::kPen,
                           WebPointerProperties::Button::kLeft,
                           gfx::PointF(100, 100), gfx::PointF(100, 100)),
      1, 1);
  GetEventHandler().HandlePointerEvent(event, Vector<WebPointerEvent>(),
                                       Vector<WebPointerEvent>());
  EXPECT_EQ(nullptr, GetDocument().FocusedElement());
  MockMainFrameWidget()->OnStartStylusWriting(base::DoNothing());
  EXPECT_EQ(first, GetDocument().FocusedElement());
}

TEST_F(WebFrameWidgetImplSimTest,
       TestStartStylusWritingForContentEditableChildElement) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <!doctype html>
      <body style='padding: 0px; width: 400px; height: 400px;'>
      <div contenteditable='true' id='first'>
      <div id='second' style='width: 100px; height: 100px;'>Hello</div>
      </div>
      </body>
      )HTML");
  Compositor().BeginFrame();
  Element* first =
      DynamicTo<Element>(GetDocument().getElementById(AtomicString("first")));
  Element* second =
      DynamicTo<Element>(GetDocument().getElementById(AtomicString("second")));
  WebPointerEvent event(
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties(1, WebPointerProperties::PointerType::kPen,
                           WebPointerProperties::Button::kLeft,
                           gfx::PointF(100, 100), gfx::PointF(100, 100)),
      1, 1);
  GetEventHandler().HandlePointerEvent(event, Vector<WebPointerEvent>(),
                                       Vector<WebPointerEvent>());
  EXPECT_EQ(second, GetEventHandler().CurrentTouchDownElement());
  EXPECT_EQ(nullptr, GetDocument().FocusedElement());
  MockMainFrameWidget()->OnStartStylusWriting(base::DoNothing());
  EXPECT_EQ(first, GetDocument().FocusedElement());
}

class NotifySwapTimesWebFrameWidgetTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();

    WebView().StopDeferringMainFrameUpdate();
    FrameWidgetBase()->UpdateCompositorViewportRect(gfx::Rect(200, 100));
    Compositor().BeginFrame();

    auto* root_layer =
        FrameWidgetBase()->LayerTreeHostForTesting()->root_layer();
    auto color_layer = cc::SolidColorLayer::Create();
    color_layer->SetBounds(gfx::Size(100, 100));
    cc::CopyProperties(root_layer, color_layer.get());
    root_layer->SetChildLayerList(cc::LayerList({color_layer}));
    color_layer->SetBackgroundColor(SkColors::kRed);
  }

  WebFrameWidgetImpl* FrameWidgetBase() {
    return static_cast<WebFrameWidgetImpl*>(MainFrame().FrameWidget());
  }

  // |swap_to_presentation| determines how long after swap should presentation
  // happen. This can be negative, positive, or zero. If zero, an invalid (null)
  // presentation time is used.
  void CompositeAndWaitForPresentation(base::TimeDelta swap_to_presentation) {
    base::RunLoop swap_run_loop;
    base::RunLoop presentation_run_loop;

    // Register callbacks for swap and presentation times.
    base::TimeTicks swap_time;
    static_cast<WebFrameWidgetImpl*>(MainFrame().FrameWidget())
        ->NotifySwapAndPresentationTimeForTesting(
            {WTF::BindOnce(
                 [](base::OnceClosure swap_quit_closure,
                    base::TimeTicks* swap_time, base::TimeTicks timestamp) {
                   CHECK(!timestamp.is_null());
                   *swap_time = timestamp;
                   std::move(swap_quit_closure).Run();
                 },
                 swap_run_loop.QuitClosure(), WTF::Unretained(&swap_time)),
             WTF::BindOnce(
                 [](base::OnceClosure presentation_quit_closure,
                    const viz::FrameTimingDetails& presentation_details) {
                   base::TimeTicks timestamp =
                       presentation_details.presentation_feedback.timestamp;
                   CHECK(!timestamp.is_null());
                   std::move(presentation_quit_closure).Run();
                 },
                 presentation_run_loop.QuitClosure())});

    // Composite and wait for the swap to complete.
    Compositor().BeginFrame(/*time_delta_in_seconds=*/0.016, /*raster=*/true);
    swap_run_loop.Run();

    // Present and wait for it to complete.
    viz::FrameTimingDetails timing_details;
    if (!swap_to_presentation.is_zero()) {
      timing_details.presentation_feedback = gfx::PresentationFeedback(
          swap_time + swap_to_presentation, base::Milliseconds(16), 0);
    }
    auto* last_frame_sink = GetWebFrameWidget().LastCreatedFrameSink();
    last_frame_sink->NotifyDidPresentCompositorFrame(1, timing_details);
    presentation_run_loop.Run();
  }
};

// Verifies that the presentation callback is called after the first successful
// presentation (skips failed presentations in between).
TEST_F(NotifySwapTimesWebFrameWidgetTest, NotifyOnSuccessfulPresentation) {
  base::HistogramTester histograms;

  constexpr base::TimeDelta swap_to_failed = base::Microseconds(2);
  constexpr base::TimeDelta failed_to_successful = base::Microseconds(3);

  base::RunLoop swap_run_loop;
  base::RunLoop presentation_run_loop;

  base::TimeTicks failed_presentation_time;
  base::TimeTicks successful_presentation_time;

  WebFrameWidgetImpl::PromiseCallbacks callbacks = {
      base::BindLambdaForTesting([&](base::TimeTicks timestamp) {
        DCHECK(!timestamp.is_null());

        // Now that the swap time is known, we can determine what
        // timestamps should we use for the failed and the subsequent
        // successful presentations.
        DCHECK(failed_presentation_time.is_null());
        failed_presentation_time = timestamp + swap_to_failed;
        DCHECK(successful_presentation_time.is_null());
        successful_presentation_time =
            failed_presentation_time + failed_to_successful;

        swap_run_loop.Quit();
      }),
      base::BindLambdaForTesting(
          [&](const viz::FrameTimingDetails& presentation_details) {
            base::TimeTicks timestamp =
                presentation_details.presentation_feedback.timestamp;
            CHECK(!timestamp.is_null());
            CHECK(!failed_presentation_time.is_null());
            CHECK(!successful_presentation_time.is_null());

            // Verify that this callback is run in response to the
            // successful presentation, not the failed one before that.
            EXPECT_NE(timestamp, failed_presentation_time);
            EXPECT_EQ(timestamp, successful_presentation_time);

            presentation_run_loop.Quit();
          })};

#if BUILDFLAG(IS_MAC)
  // Assign a ca_layer error code.
  constexpr gfx::CALayerResult ca_layer_error_code =
      gfx::kCALayerFailedTileNotCandidate;

  callbacks.core_animation_error_code_callback = base::BindLambdaForTesting(
      [&](gfx::CALayerResult core_animation_error_code) {
        // Verify that the error code received here is the same as the
        // one sent to DidPresentCompositorFrame.
        EXPECT_EQ(ca_layer_error_code, core_animation_error_code);

        presentation_run_loop.Quit();
      });
#endif

  // Register callbacks for swap and presentation times.
  static_cast<WebFrameWidgetImpl*>(MainFrame().FrameWidget())
      ->NotifySwapAndPresentationTimeForTesting(std::move(callbacks));

  // Composite and wait for the swap to complete.
  Compositor().BeginFrame(/*time_delta_in_seconds=*/0.016, /*raster=*/true);
  swap_run_loop.Run();

  // Respond with a failed presentation feedback.
  DCHECK(!failed_presentation_time.is_null());
  viz::FrameTimingDetails failed_timing_details;
  failed_timing_details.presentation_feedback = gfx::PresentationFeedback(
      failed_presentation_time, base::Milliseconds(16),
      gfx::PresentationFeedback::kFailure);
  GetWebFrameWidget().LastCreatedFrameSink()->NotifyDidPresentCompositorFrame(
      1, failed_timing_details);

  // Respond with a successful presentation feedback.
  DCHECK(!successful_presentation_time.is_null());
  viz::FrameTimingDetails successful_timing_details;
  successful_timing_details.presentation_feedback = gfx::PresentationFeedback(
      successful_presentation_time, base::Milliseconds(16), 0);
#if BUILDFLAG(IS_MAC)
  successful_timing_details.presentation_feedback.ca_layer_error_code =
      ca_layer_error_code;
#endif
  GetWebFrameWidget().LastCreatedFrameSink()->NotifyDidPresentCompositorFrame(
      2, successful_timing_details);

  // Wait for the presentation callback to be called. It should be called with
  // the timestamp of the successful presentation.
  presentation_run_loop.Run();
}

// Tests that the presentation callback is only triggered if there’s
// a successful commit to the compositor.
TEST_F(NotifySwapTimesWebFrameWidgetTest,
       ReportPresentationOnlyOnSuccessfulCommit) {
  base::HistogramTester histograms;
  constexpr base::TimeDelta delta = base::Milliseconds(16);
  constexpr base::TimeDelta delta_from_swap_time = base::Microseconds(2);

  base::RunLoop swap_run_loop;
  base::RunLoop presentation_run_loop;
  base::TimeTicks presentation_time;

  // Register callbacks for swap and presentation times.
  static_cast<WebFrameWidgetImpl*>(MainFrame().FrameWidget())
      ->NotifySwapAndPresentationTimeForTesting(
          {base::BindLambdaForTesting([&](base::TimeTicks timestamp) {
             DCHECK(!timestamp.is_null());
             DCHECK(presentation_time.is_null());

             // Set the expected presentation time after the swap takes place.
             presentation_time = timestamp + delta_from_swap_time;
             swap_run_loop.Quit();
           }),
           base::BindLambdaForTesting(
               [&](const viz::FrameTimingDetails& presentation_details) {
                 base::TimeTicks timestamp =
                     presentation_details.presentation_feedback.timestamp;
                 CHECK(!timestamp.is_null());
                 CHECK(!presentation_time.is_null());

                 // Verify that the presentation is only reported on the
                 // successful commit to the compositor.
                 EXPECT_EQ(timestamp, presentation_time);
                 presentation_run_loop.Quit();
               })});

  // Simulate a failed commit to the compositor, which should not trigger either
  // a swap or a presentation callback in response.
  auto* layer_tree_host = Compositor().LayerTreeHost();
  layer_tree_host->GetSwapPromiseManager()->BreakSwapPromises(
      cc::SwapPromise::DidNotSwapReason::COMMIT_FAILS);

  // Check that a swap callback wasn't triggered for the above failed commit.
  EXPECT_TRUE(presentation_time.is_null());

  // Composite and wait for the swap to complete successfully.
  Compositor().BeginFrame(delta.InSecondsF(), true);
  swap_run_loop.Run();

  // Make sure that the swap is completed successfully.
  EXPECT_FALSE(presentation_time.is_null());

  // Respond with a presentation feedback.
  viz::FrameTimingDetails frame_timing_details;
  frame_timing_details.presentation_feedback =
      gfx::PresentationFeedback(presentation_time, delta, 0);
  GetWebFrameWidget().LastCreatedFrameSink()->NotifyDidPresentCompositorFrame(
      1, frame_timing_details);

  // Wait for the presentation callback to be called.
  presentation_run_loop.Run();
}

// Tests that the value of VisualProperties::is_pinch_gesture_active is
// not propagated to the LayerTreeHost when properties are synced for main
// frame.
TEST_F(WebFrameWidgetSimTest, ActivePinchGestureUpdatesLayerTreeHost) {
  auto* layer_tree_host =
      WebView().MainFrameViewWidget()->LayerTreeHostForTesting();
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
  VisualProperties visual_properties;
  visual_properties.screen_infos = display::ScreenInfos(display::ScreenInfo());

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

class WebFrameWidgetInputEventsSimTest
    : public WebFrameWidgetSimTest,
      public testing::WithParamInterface<bool> {
 public:
  WebFrameWidgetInputEventsSimTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          features::kPausePagesPerBrowsingContextGroup);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kPausePagesPerBrowsingContextGroup);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebFrameWidgetInputEventsSimTest,
                         testing::Values(true, false));

// Tests that dispatch buffered touch events does not process events during
// drag and devtools handling.
TEST_P(WebFrameWidgetInputEventsSimTest, DispatchBufferedTouchEvents) {
  auto* widget = WebView().MainFrameViewWidget();

  auto* listener = MakeGarbageCollected<TouchMoveEventListener>();
  Window().addEventListener(
      event_type_names::kTouchmove, listener,
      MakeGarbageCollected<AddEventListenerOptionsResolved>());
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);

  // Send a start.
  SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);
  touch.touch_start_or_first_touch_move = true;
  widget->ProcessInputEventSynchronouslyForTesting(
      WebCoalescedInputEvent(touch.Clone(), {}, {}, ui::LatencyInfo()),
      base::DoNothing());

  // Expect listener gets called.
  touch.MovePoint(0, 10, 10);
  widget->ProcessInputEventSynchronouslyForTesting(
      WebCoalescedInputEvent(touch.Clone(), {}, {}, ui::LatencyInfo()),
      base::DoNothing());
  EXPECT_TRUE(listener->GetInvokedStateAndReset());

  const base::UnguessableToken browsing_context_group_token =
      WebView().GetPage()->BrowsingContextGroupToken();

  // Expect listener does not get called, due to devtools flag.
  touch.MovePoint(0, 12, 12);
  WebFrameWidgetImpl::SetIgnoreInputEvents(browsing_context_group_token, true);
  widget->ProcessInputEventSynchronouslyForTesting(
      WebCoalescedInputEvent(touch.Clone(), {}, {}, ui::LatencyInfo()),
      base::DoNothing());
  EXPECT_TRUE(
      WebFrameWidgetImpl::IgnoreInputEvents(browsing_context_group_token));
  EXPECT_FALSE(listener->GetInvokedStateAndReset());
  WebFrameWidgetImpl::SetIgnoreInputEvents(browsing_context_group_token, false);

  // Expect listener does not get called, due to drag.
  touch.MovePoint(0, 14, 14);
  widget->StartDragging(MainFrame().GetFrame(), WebDragData(),
                        kDragOperationCopy, SkBitmap(), gfx::Vector2d(),
                        gfx::Rect());
  widget->ProcessInputEventSynchronouslyForTesting(
      WebCoalescedInputEvent(touch.Clone(), {}, {}, ui::LatencyInfo()),
      base::DoNothing());
  EXPECT_TRUE(widget->DoingDragAndDrop());
  EXPECT_FALSE(
      WebFrameWidgetImpl::IgnoreInputEvents(browsing_context_group_token));
  EXPECT_FALSE(listener->GetInvokedStateAndReset());
}

// Tests that page scale is propagated to all remote frames controlled
// by a widget.
TEST_F(WebFrameWidgetSimTest, PropagateScaleToRemoteFrames) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <iframe style='width: 200px; height: 100px;'
        srcdoc='<iframe srcdoc="plain text"></iframe>'>
        </iframe>

      )HTML");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(WebView().MainFrame()->FirstChild());
  {
    WebFrame* grandchild = WebView().MainFrame()->FirstChild()->FirstChild();
    EXPECT_TRUE(grandchild);
    EXPECT_TRUE(grandchild->IsWebLocalFrame());
    frame_test_helpers::SwapRemoteFrame(grandchild,
                                        frame_test_helpers::CreateRemote());
  }
  auto* widget = WebView().MainFrameViewWidget();
  widget->SetPageScaleStateAndLimits(1.3f, true, 1.0f, 3.0f);
  EXPECT_EQ(
      To<WebRemoteFrameImpl>(WebView().MainFrame()->FirstChild()->FirstChild())
          ->GetFrame()
          ->GetPendingVisualPropertiesForTesting()
          .page_scale_factor,
      1.3f);
  WebView().MainFrame()->FirstChild()->FirstChild()->Detach();
}

TEST_F(WebFrameWidgetSimTest, TestLineBoundsAreEmptyBeforeFocus) {
  std::unique_ptr<ScopedReportVisibleLineBoundsForTest> enabled =
      std::make_unique<ScopedReportVisibleLineBoundsForTest>(true);
  WebView().ResizeVisualViewport(gfx::Size(1000, 1000));
  auto* widget = WebView().MainFrameViewWidget();
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: custom-font;
          src: url(https://example.com/Ahem.woff2) format("woff2");
        }
        body {
          margin: 0;
          padding: 0;
          border: 0;
        }
        .target {
          font: 10px/1 custom-font, monospace;
          margin: 0;
          padding: 0;
          border: none;
        }
      </style>
      <input type='text' id='first' class='target' />
      )HTML");
  Compositor().BeginFrame();
  // Finish font loading, and trigger invalidations.
  font_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));
  Compositor().BeginFrame();
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  Vector<gfx::Rect>& actual = widget->GetVisibleLineBoundsOnScreen();
  EXPECT_EQ(0U, actual.size());
}

TEST_F(WebFrameWidgetSimTest, TestLineBoundsAreCorrectAfterFocusChange) {
  std::unique_ptr<ScopedReportVisibleLineBoundsForTest> enabled =
      std::make_unique<ScopedReportVisibleLineBoundsForTest>(true);
  WebView().ResizeVisualViewport(gfx::Size(1000, 1000));
  auto* widget = WebView().MainFrameViewWidget();
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: custom-font;
          src: url(https://example.com/Ahem.woff2) format("woff2");
        }
        body {
          margin: 0;
          padding: 0;
          border: 0;
        }
        .target {
          font: 10px/1 custom-font, monospace;
          margin: 0;
          padding: 0;
          border: none;
        }
      </style>
      <input type='text' id='first' class='target' />
      <input type='text' id='second' class='target' />
      )HTML");
  Compositor().BeginFrame();
  // Finish font loading, and trigger invalidations.
  font_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));
  Compositor().BeginFrame();
  HTMLInputElement* first = DynamicTo<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("first")));
  HTMLInputElement* second = DynamicTo<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("second")));
  // Focus the first element and check the line bounds.
  first->SetValue("ABCD");
  first->Focus();
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  Vector<gfx::Rect> expected(Vector({gfx::Rect(0, 0, 40, 10)}));
  Vector<gfx::Rect>& actual = widget->GetVisibleLineBoundsOnScreen();
  EXPECT_EQ(expected.size(), actual.size());
  for (wtf_size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected.at(i), actual.at(i));
  }

  // Focus the second element and check the line bounds have updated.
  second->SetValue("ABCD EFGH");
  second->Focus();
  gfx::Point origin =
      second->GetBoundingClientRect()->ToEnclosingRect().origin();
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  expected = Vector({gfx::Rect(origin.x(), origin.y(), 90, 10)});
  actual = widget->GetVisibleLineBoundsOnScreen();
  EXPECT_EQ(expected.size(), actual.size());
  for (wtf_size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected.at(i), actual.at(i));
  }
}

TEST_F(WebFrameWidgetSimTest, DisplayStateMatchesWindowShowState) {
  base::test::ScopedFeatureList feature_list(
      ScopedDesktopPWAsAdditionalWindowingControlsForTest);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
        <!doctype html>
        <style>
        body {
          background-color: white;
        }
        @media (display-state: normal) {
          body {
            background-color: yellow;
          }
        }
        @media (display-state: minimized) {
          body {
            background-color: cyan;
          }
        }
        @media (display-state: maximized) {
          body {
            background-color: red;
          }
        }
        @media (display-state: fullscreen) {
          body {
            background-color: blue;
          }
        }
      </style>
      <body></body>
      )HTML");

  auto* widget = WebView().MainFrameViewWidget();
  VisualProperties visual_properties;
  visual_properties.screen_infos = display::ScreenInfos(display::ScreenInfo());

  // display-state: normal
  // Default is set in /third_party/blink/renderer/core/frame/settings.json5.
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  EXPECT_EQ(Color::FromRGB(/*yellow*/ 255, 255, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));

  WTF::Vector<std::pair<ui::mojom::blink::WindowShowState, Color>> test_cases =
      {{ui::mojom::blink::WindowShowState::kMinimized,
        Color::FromRGB(/*cyan*/ 0, 255, 255)},
       {ui::mojom::blink::WindowShowState::kMaximized,
        Color::FromRGB(/*red*/ 255, 0, 0)},
       {ui::mojom::blink::WindowShowState::kFullscreen,
        Color::FromRGB(/*blue*/ 0, 0, 255)}};

  for (const auto& [show_state, color] : test_cases) {
    visual_properties.window_show_state = show_state;
    WebView().MainFrameWidget()->ApplyVisualProperties(visual_properties);
    widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
    EXPECT_EQ(color,
              GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                  GetCSSPropertyBackgroundColor()));
  }
}

TEST_F(WebFrameWidgetSimTest, ResizableMatchesCanResize) {
  base::test::ScopedFeatureList feature_list(
      ScopedDesktopPWAsAdditionalWindowingControlsForTest);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
        <!doctype html>
        <style>
          body {
            /* This should never activate. */
            background-color: white;
          }
          @media (resizable: true) {
            body {
              background-color: yellow;
            }
          }
          @media (resizable: false) {
            body {
              background-color: cyan;
            }
          }
        </style>
        <body></body>
      )HTML");

  auto* widget = WebView().MainFrameViewWidget();
  VisualProperties visual_properties;
  visual_properties.screen_infos = display::ScreenInfos(display::ScreenInfo());

  // resizable: true
  // Default is set in /third_party/blink/renderer/core/frame/settings.json5.
  WebView().MainFrameWidget()->ApplyVisualProperties(visual_properties);
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  EXPECT_EQ(Color::FromRGB(/*yellow*/ 255, 255, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));

  // resizable: false
  visual_properties.resizable = false;
  WebView().MainFrameWidget()->ApplyVisualProperties(visual_properties);
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  EXPECT_EQ(Color::FromRGB(/*cyan*/ 0, 255, 255),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));
}

TEST_F(WebFrameWidgetSimTest, TestLineBoundsAreCorrectAfterLayoutChange) {
  std::unique_ptr<ScopedReportVisibleLineBoundsForTest> enabled =
      std::make_unique<ScopedReportVisibleLineBoundsForTest>(true);
  WebView().ResizeVisualViewport(gfx::Size(1000, 1000));
  auto* widget = WebView().MainFrameViewWidget();
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: custom-font;
          src: url(https://example.com/Ahem.woff2) format("woff2");
        }
        body {
          margin: 0;
          padding: 0;
          border: 0;
        }
        .target {
          font: 10px/1 custom-font, monospace;
          margin: 0;
          padding: 0;
          border: none;
        }
      </style>
      <div id='d' style='height: 0;'/>
      <input type='text' id='first' class='target' />
      )HTML");
  Compositor().BeginFrame();
  // Finish font loading, and trigger invalidations.
  font_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));
  Compositor().BeginFrame();
  HTMLInputElement* first = DynamicTo<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("first")));
  // Focus the element and check the line bounds.
  first->Focus();
  first->SetValue("hello world");
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  Vector<gfx::Rect>& expected = widget->GetVisibleLineBoundsOnScreen();
  // Offset each line bound by 200 pixels downwards (for after layout shift).
  for (auto& i : expected) {
    i.Offset(0, 200);
  }

  GetDocument()
      .getElementById(AtomicString("d"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("height: 200px"));
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  Vector<gfx::Rect>& actual = widget->GetVisibleLineBoundsOnScreen();
  for (wtf_size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected.at(i), actual.at(i));
  }
}

TEST_F(WebFrameWidgetSimTest, TestLineBoundsAreCorrectAfterPageScroll) {
  std::unique_ptr<ScopedReportVisibleLineBoundsForTest> enabled =
      std::make_unique<ScopedReportVisibleLineBoundsForTest>(true);
  WebView().ResizeVisualViewport(gfx::Size(1000, 1000));
  auto* widget = WebView().MainFrameViewWidget();
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: custom-font;
          src: url(https://example.com/Ahem.woff2) format("woff2");
        }
        body {
          margin: 0;
          padding: 0;
          border: 0;
          height: 150vh;
          overflow: scrollY;
        }
        .target {
          font: 10px/1 custom-font, monospace;
          margin: 0;
          padding: 0;
          border: none;
          position: absolute;
          top: 100px;
        }
      </style>
      <textarea type='text' id='first' class='target' >
          The quick brown fox jumps over the lazy dog.
      </textarea>
      )HTML");
  Compositor().BeginFrame();
  // Finish font loading, and trigger invalidations.
  font_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));
  Compositor().BeginFrame();
  HTMLTextAreaElement* first = DynamicTo<HTMLTextAreaElement>(
      GetDocument().getElementById(AtomicString("first")));
  // Focus the element and check the line bounds.
  first->Focus();
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);

  Vector<gfx::Rect> expected;
  for (auto& i : widget->GetVisibleLineBoundsOnScreen()) {
    gfx::Rect bound(i.origin(), i.size());
    bound.Offset(0, -50);
    expected.push_back(bound);
  }

  // Scroll by 50 pixels down.
  widget->FocusedLocalFrameInWidget()->View()->LayoutViewport()->ScrollBy(
      ScrollOffset(0, 50), mojom::blink::ScrollType::kUser);
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);

  // As line bounds are calculated in document coordinates, a document scroll
  // should not have any effect. Assert that they are the same as before.
  Vector<gfx::Rect>& actual = widget->GetVisibleLineBoundsOnScreen();
  for (wtf_size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected.at(i).ToString(), actual.at(i).ToString());
  }
}

TEST_F(WebFrameWidgetSimTest, TestLineBoundsAreCorrectAfterElementScroll) {
  std::unique_ptr<ScopedReportVisibleLineBoundsForTest> enabled =
      std::make_unique<ScopedReportVisibleLineBoundsForTest>(true);
  WebView().ResizeVisualViewport(gfx::Size(1000, 1000));
  auto* widget = WebView().MainFrameViewWidget();
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: custom-font;
          src: url(https://example.com/Ahem.woff2) format("woff2");
        }
        body {
          margin: 0;
          padding: 0;
          border: 0;
          height: 150vh;
          overflow: scrollY;
        }
        .target {
          font: 10px/1 custom-font, monospace;
          margin: 0;
          padding: 0;
          border: none;
          overflow-y: scroll;
          position: absolute;
          top: 150px;
        }
      </style>
      <textarea type='text' id='first' class='target' >
          The quick brown fox jumps over the lazy dog.
          The quick brown fox jumps over the lazy dog.
          The quick brown fox jumps over the lazy dog.
          The quick brown fox jumps over the lazy dog.
      </textarea>
      )HTML");
  Compositor().BeginFrame();
  // Finish font loading, and trigger invalidations.
  font_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));
  Compositor().BeginFrame();
  HTMLTextAreaElement* first = DynamicTo<HTMLTextAreaElement>(
      GetDocument().getElementById(AtomicString("first")));
  // Focus the element and check the line bounds.
  first->Focus();
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  Vector<gfx::Rect> expected;

  // Offset each line bound by 50 pixels upwards (for after a scroll down).
  for (auto& i : widget->GetVisibleLineBoundsOnScreen()) {
    gfx::Rect bound(i.origin(), i.size());
    bound.Offset(0, -50);
    expected.push_back(bound);
  }

  // Scroll element by 50 pixels down.
  GetDocument().FocusedElement()->scrollBy(0, 50);
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);

  Vector<gfx::Rect>& actual = widget->GetVisibleLineBoundsOnScreen();
  EXPECT_EQ(expected.size(), actual.size());
  for (wtf_size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected.at(i), actual.at(i));
  }
}

TEST_F(WebFrameWidgetSimTest, TestLineBoundsAreCorrectAfterCommit) {
  std::unique_ptr<ScopedReportVisibleLineBoundsForTest> enabled =
      std::make_unique<ScopedReportVisibleLineBoundsForTest>(true);
  WebView().ResizeVisualViewport(gfx::Size(1000, 1000));
  auto* widget = WebView().MainFrameViewWidget();
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: custom-font;
          src: url(https://example.com/Ahem.woff2) format("woff2");
        }
        body {
          margin: 0;
          padding: 0;
          border: 0;
          height: 150vh;
          overflow: scrollY;
        }
        .target {
          font: 10px/1 custom-font, monospace;
          margin: 0;
          padding: 0;
          border: none;
          overflow-y: scroll;
        }
      </style>
      <textarea type='text' id='first' class='target' ></textarea>
      )HTML");
  Compositor().BeginFrame();
  // Finish font loading, and trigger invalidations.
  font_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));
  Compositor().BeginFrame();
  HTMLTextAreaElement* first = DynamicTo<HTMLTextAreaElement>(
      GetDocument().getElementById(AtomicString("first")));
  // Focus the element and check the line bounds.
  first->Focus();
  gfx::Point origin =
      first->GetBoundingClientRect()->ToEnclosingRect().origin();
  String text = "hello world";
  for (wtf_size_t i = 0; i < text.length(); ++i) {
    first->SetValue(first->Value() + text[i]);
    widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
    EXPECT_EQ(1U, widget->GetVisibleLineBoundsOnScreen().size());
    EXPECT_EQ(gfx::Rect(origin.x(), origin.y(), 10 * (i + 1), 10),
              widget->GetVisibleLineBoundsOnScreen().at(0));
  }
  first->SetValue(first->Value() + "\n");
  String new_text = "goodbye world";
  for (wtf_size_t i = 0; i < new_text.length(); ++i) {
    first->SetValue(first->Value() + new_text[i]);
    widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
    EXPECT_EQ(2U, widget->GetVisibleLineBoundsOnScreen().size());
    EXPECT_EQ(gfx::Rect(origin.x(), origin.y() + 10, 10 * (i + 1), 10),
              widget->GetVisibleLineBoundsOnScreen().at(1));
  }
}

TEST_F(WebFrameWidgetSimTest, TestLineBoundsAreCorrectAfterDelete) {
  std::unique_ptr<ScopedReportVisibleLineBoundsForTest> enabled =
      std::make_unique<ScopedReportVisibleLineBoundsForTest>(true);
  WebView().ResizeVisualViewport(gfx::Size(1000, 1000));
  auto* widget = WebView().MainFrameViewWidget();
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");
  LoadURL("https://example.com/test.html");
  request.Complete(
      R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: custom-font;
          src: url(https://example.com/Ahem.woff2) format("woff2");
        }
        body {
          margin: 0;
          padding: 0;
          border: 0;
          height: 150vh;
          overflow: scrollY;
        }
        .target {
          font: 10px/1 custom-font, monospace;
          margin: 0;
          padding: 0;
          border: none;
          overflow-y: scroll;
        }
      </style>
      <textarea type='text' id='first' class='target' ></textarea>
      )HTML");
  Compositor().BeginFrame();
  // Finish font loading, and trigger invalidations.
  font_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));
  Compositor().BeginFrame();
  HTMLTextAreaElement* first = DynamicTo<HTMLTextAreaElement>(
      GetDocument().getElementById(AtomicString("first")));

  first->Focus();
  first->SetValue("hello world\rgoodbye world");
  gfx::Point origin =
      first->GetBoundingClientRect()->ToEnclosingRect().origin();

  String last_line = "goodbye world";
  for (wtf_size_t i = last_line.length() - 1; i > 0; --i) {
    widget->FocusedWebLocalFrameInWidget()->DeleteSurroundingText(1, 0);
    widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
    EXPECT_EQ(2U, widget->GetVisibleLineBoundsOnScreen().size());
    EXPECT_EQ(gfx::Rect(origin.x(), origin.y() + 10, 10 * i, 10),
              widget->GetVisibleLineBoundsOnScreen().at(1));
  }

  // Remove the last character on the second line.
  // This is outside the for loop as after this happens, there should only be 1
  // line bound.
  widget->FocusedWebLocalFrameInWidget()->DeleteSurroundingText(1, 0);
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  EXPECT_EQ(1U, widget->GetVisibleLineBoundsOnScreen().size());

  // Remove the new line character.
  widget->FocusedWebLocalFrameInWidget()->DeleteSurroundingText(1, 0);
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  EXPECT_EQ(1U, widget->GetVisibleLineBoundsOnScreen().size());

  String first_line = "hello world";
  for (wtf_size_t i = first_line.length() - 1; i > 0; --i) {
    widget->FocusedWebLocalFrameInWidget()->DeleteSurroundingText(1, 0);
    widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
    EXPECT_EQ(1U, widget->GetVisibleLineBoundsOnScreen().size());
    EXPECT_EQ(gfx::Rect(origin.x(), origin.y(), 10 * i, 10),
              widget->GetVisibleLineBoundsOnScreen().at(0));
  }

  // Remove last character
  widget->FocusedWebLocalFrameInWidget()->DeleteSurroundingText(1, 0);
  widget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  EXPECT_EQ(0U, widget->GetVisibleLineBoundsOnScreen().size());
}

TEST_F(WebFrameWidgetSimTest, TestLineBoundsInFrame) {
  std::unique_ptr<ScopedReportVisibleLineBoundsForTest> enabled =
      std::make_unique<ScopedReportVisibleLineBoundsForTest>(true);
  WebView().ResizeVisualViewport(gfx::Size(1000, 1000));
  auto* widget = WebView().MainFrameViewWidget();
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest child_frame_resource("https://example.com/child_frame.html",
                                  "text/html");
  SimSubresourceRequest child_font_resource("https://example.com/Ahem.woff2",
                                            "font/woff2");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(
      R"HTML(
        <!doctype html>
        <style>
          html, body, iframe {
            margin: 0;
            padding: 0;
            border: 0;
          }
        </style>
        <div style='height: 123px;'></div>
        <iframe src='https://example.com/child_frame.html'
                id='child_frame' width='300px' height='300px'></iframe>)HTML");
  Compositor().BeginFrame();

  child_frame_resource.Complete(
      R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: custom-font;
          src: url(https://example.com/Ahem.woff2) format("woff2");
        }
        body {
          margin: 0;
          padding: 0;
        }
        .target {
          font: 10px/1 custom-font, monospace;
          margin: 0;
          padding: 0;
          border: none;
        }
      </style>
      <div style='height: 42px;'></div>
      <input type='text' id='first' class='target' value='ABCD' />
      <script>
        first.focus();
      </script>
      )HTML");
  Compositor().BeginFrame();

  child_font_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));
  Compositor().BeginFrame();

  Vector<gfx::Rect> expected(Vector({gfx::Rect(0, /* 123+42= */ 165, 40, 10)}));
  Vector<gfx::Rect>& actual = widget->GetVisibleLineBoundsOnScreen();
  EXPECT_EQ(expected.size(), actual.size());
  for (wtf_size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected.at(i), actual.at(i));
  }
}

TEST_F(WebFrameWidgetSimTest, TestLineBoundsWithDifferentZoom) {
  std::unique_ptr<ScopedReportVisibleLineBoundsForTest> enabled =
      std::make_unique<ScopedReportVisibleLineBoundsForTest>(true);
  WebView().ResizeVisualViewport(gfx::Size(1000, 1000));
  auto* widget = WebView().MainFrameViewWidget();
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest child_frame_resource("https://example.com/child_frame.html",
                                  "text/html");
  SimSubresourceRequest child_font_resource("https://example.com/Ahem.woff2",
                                            "font/woff2");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(
      R"HTML(
        <!doctype html>
        <style>
          html, body, iframe {
            margin: 0;
            padding: 0;
            border: 0;
          }
          html {
            zoom: 1.2;
          }
        </style>
        <div style='height: 70px;'></div>
        <iframe src='https://example.com/child_frame.html'
                id='child_frame' width='300px' height='300px'></iframe>)HTML");
  Compositor().BeginFrame();

  child_frame_resource.Complete(
      R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: custom-font;
          src: url(https://example.com/Ahem.woff2) format("woff2");
        }
        html {
          zoom: 1.5;
        }
        body {
          margin: 0;
          padding: 0;
        }
        .target {
          font: 10px/1 custom-font, monospace;
          margin: 0;
          padding: 0;
          border: none;
        }
      </style>
      <div style='height: 40px;'></div>
      <input type='text' id='first' class='target' value='ABCD' />
      <script>
        first.focus();
      </script>
      )HTML");
  Compositor().BeginFrame();

  child_font_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));
  Compositor().BeginFrame();

  Vector<gfx::Rect> expected(
      Vector({gfx::Rect(0, /* 70*1.2+40*1.2*1.5= */ 156, /* 40*1.2*1.5= */ 72,
                        /* 10*1.2*1.5= */ 18)}));
  Vector<gfx::Rect>& actual = widget->GetVisibleLineBoundsOnScreen();
  EXPECT_EQ(expected.size(), actual.size());
  for (wtf_size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected.at(i), actual.at(i));
  }
}

TEST_F(WebFrameWidgetSimTest, TestLineBoundsAreClippedInSubframe) {
  std::unique_ptr<ScopedReportVisibleLineBoundsForTest> enabled =
      std::make_unique<ScopedReportVisibleLineBoundsForTest>(true);
  WebView().ResizeVisualViewport(gfx::Size(200, 200));
  auto* widget = WebView().MainFrameViewWidget();
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest child_frame_resource("https://example.com/child_frame.html",
                                  "text/html");
  SimSubresourceRequest child_font_resource("https://example.com/Ahem.woff2",
                                            "font/woff2");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(
      R"HTML(
        <!doctype html>
        <style>
          html, body, iframe {
            margin: 0;
            padding: 0;
            border: 0;
          }
        </style>
        <div style='height: 100px;'></div>
        <iframe src='https://example.com/child_frame.html'
                id='child_frame' width='200px' height='100px'></iframe>)HTML");
  Compositor().BeginFrame();

  child_frame_resource.Complete(
      R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: custom-font;
          src: url(https://example.com/Ahem.woff2) format("woff2");
        }
        body {
          margin: 0;
          padding: 0;
          zoom: 11;
        }
        .target {
          font: 10px/1 custom-font, monospace;
          margin: 0;
          padding: 0;
          border: none;
        }
      </style>
      <input type='text' id='first' class='target' value='ABCD' />
      <script>
        first.focus();
      </script>
      )HTML");
  Compositor().BeginFrame();

  child_font_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));
  Compositor().BeginFrame();

  // The expected top value is 100 because of the spacer div in the main frame.
  // The expected width is 40 * 11 = 440 but this should be clipped to the
  // screen width which is 200px.
  // The expected height is 10 * 11 = 110 but this should be clipped as to the
  // screen height of 200px - 100px for the top of the bound.
  Vector<gfx::Rect> expected(Vector({gfx::Rect(0, 100, 200, 100)}));
  Vector<gfx::Rect>& actual = widget->GetVisibleLineBoundsOnScreen();
  EXPECT_EQ(expected.size(), actual.size());
  for (wtf_size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected.at(i), actual.at(i));
  }
}

class EventHandlingWebFrameWidgetSimTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();

    WebView().StopDeferringMainFrameUpdate();
    GetWebFrameWidget().UpdateCompositorViewportRect(gfx::Rect(200, 100));
    Compositor().BeginFrame();
  }

  frame_test_helpers::TestWebFrameWidget* CreateWebFrameWidget(
      base::PassKey<WebLocalFrame> pass_key,
      CrossVariantMojoAssociatedRemote<
          mojom::blink::FrameWidgetHostInterfaceBase> frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const viz::FrameSinkId& frame_sink_id,
      bool hidden,
      bool never_composited,
      bool is_for_child_local_root,
      bool is_for_nested_main_frame,
      bool is_for_scalable_page) override {
    return MakeGarbageCollected<TestWebFrameWidget>(
        pass_key, std::move(frame_widget_host), std::move(frame_widget),
        std::move(widget_host), std::move(widget), std::move(task_runner),
        frame_sink_id, hidden, never_composited, is_for_child_local_root,
        is_for_nested_main_frame, is_for_scalable_page);
  }

 protected:
  // A test `cc::SwapPromise` implementation that can be used to track the state
  // of the swap promise.
  class TestSwapPromise : public cc::SwapPromise {
   public:
    enum class State {
      kPending,
      kResolved,
      kBroken,
      kMaxValue = kBroken,
    };

    explicit TestSwapPromise(State* state) : state_(state) {
      DCHECK(state_);
      *state_ = State::kPending;
    }

    void DidActivate() override {}

    void WillSwap(viz::CompositorFrameMetadata* metadata) override {}

    void DidSwap() override {
      DCHECK_EQ(State::kPending, *state_);
      *state_ = State::kResolved;
    }

    DidNotSwapAction DidNotSwap(DidNotSwapReason reason,
                                base::TimeTicks) override {
      DCHECK_EQ(State::kPending, *state_);
      *state_ = State::kBroken;
      return DidNotSwapAction::BREAK_PROMISE;
    }

    int64_t GetTraceId() const override { return 0; }

   private:
    State* const state_;
  };

  // A test `WebFrameWidget` implementation that fakes handling of an event.
  class TestWebFrameWidget : public frame_test_helpers::TestWebFrameWidget {
   public:
    using frame_test_helpers::TestWebFrameWidget::TestWebFrameWidget;

    WebInputEventResult HandleInputEvent(
        const WebCoalescedInputEvent& coalesced_event) override {
      if (event_causes_update_) {
        RequestUpdateIfNecessary();
      }
      return WebInputEventResult::kHandledApplication;
    }

    void set_event_causes_update(bool event_causes_update) {
      event_causes_update_ = event_causes_update;
    }

    void RequestUpdateIfNecessary() {
      if (update_requested_) {
        return;
      }

      LayerTreeHost()->SetNeedsCommit();
      update_requested_ = true;
    }

    void QueueSwapPromise(TestSwapPromise::State* state) {
      LayerTreeHost()->GetSwapPromiseManager()->QueueSwapPromise(
          std::make_unique<TestSwapPromise>(state));
    }

    void SendInputEventAndWaitForDispatch(
        std::unique_ptr<WebInputEvent> event) {
      MainThreadEventQueue* input_event_queue =
          GetWidgetInputHandlerManager()->input_event_queue();
      input_event_queue->HandleEvent(
          std::make_unique<WebCoalescedInputEvent>(std::move(event),
                                                   ui::LatencyInfo()),
          MainThreadEventQueue::DispatchType::kNonBlocking,
          mojom::blink::InputEventResultState::kSetNonBlocking,
          WebInputEventAttribution(), nullptr, base::DoNothing());
      FlushInputHandlerTasks();
    }

    void CompositeAndWaitForPresentation(SimCompositor& compositor) {
      base::RunLoop swap_run_loop;
      base::RunLoop presentation_run_loop;

      // Register callbacks for swap and presentation times.
      base::TimeTicks swap_time;
      NotifySwapAndPresentationTimeForTesting(
          {WTF::BindOnce(
               [](base::OnceClosure swap_quit_closure,
                  base::TimeTicks* swap_time, base::TimeTicks timestamp) {
                 DCHECK(!timestamp.is_null());
                 *swap_time = timestamp;
                 std::move(swap_quit_closure).Run();
               },
               swap_run_loop.QuitClosure(), WTF::Unretained(&swap_time)),
           WTF::BindOnce(
               [](base::OnceClosure presentation_quit_closure,
                  const viz::FrameTimingDetails& presentation_details) {
                 base::TimeTicks timestamp =
                     presentation_details.presentation_feedback.timestamp;
                 CHECK(!timestamp.is_null());
                 std::move(presentation_quit_closure).Run();
               },
               presentation_run_loop.QuitClosure())});

      // Composite and wait for the swap to complete.
      compositor.BeginFrame(/*time_delta_in_seconds=*/0.016, /*raster=*/true);
      swap_run_loop.Run();

      // Present and wait for it to complete.
      viz::FrameTimingDetails timing_details;
      timing_details.presentation_feedback = gfx::PresentationFeedback(
          swap_time + base::Milliseconds(2), base::Milliseconds(16), 0);
      LastCreatedFrameSink()->NotifyDidPresentCompositorFrame(1,
                                                              timing_details);
      presentation_run_loop.Run();
    }

   private:
    // Whether an update is already requested. Used to avoid calling
    // `LayerTreeHost::SetNeedsCommit()` multiple times.
    bool update_requested_ = false;

    // Whether handling of the event should end up in an update or not.
    bool event_causes_update_ = false;
  };

  TestWebFrameWidget& GetTestWebFrameWidget() {
    return static_cast<TestWebFrameWidget&>(GetWebFrameWidget());
  }
};

// Verifies that when a non-rAF-aligned event is handled without causing an
// update, swap promises will be broken.
TEST_F(EventHandlingWebFrameWidgetSimTest, NonRafAlignedEventWithoutUpdate) {
  TestSwapPromise::State swap_promise_state;
  GetTestWebFrameWidget().QueueSwapPromise(&swap_promise_state);
  EXPECT_EQ(TestSwapPromise::State::kPending, swap_promise_state);

  GetTestWebFrameWidget().set_event_causes_update(false);

  GetTestWebFrameWidget().SendInputEventAndWaitForDispatch(
      std::make_unique<WebKeyboardEvent>(
          WebInputEvent::Type::kRawKeyDown, WebInputEvent::kNoModifiers,
          WebInputEvent::GetStaticTimeStampForTests()));
  EXPECT_EQ(TestSwapPromise::State::kBroken, swap_promise_state);
}

// Verifies that when a non-rAF-aligned event is handled without causing an
// update while an update is already requested, swap promises won't be broken.
TEST_F(EventHandlingWebFrameWidgetSimTest,
       NonRafAlignedEventWithoutUpdateAfterUpdate) {
  GetTestWebFrameWidget().RequestUpdateIfNecessary();

  TestSwapPromise::State swap_promise_state;
  GetTestWebFrameWidget().QueueSwapPromise(&swap_promise_state);
  EXPECT_EQ(TestSwapPromise::State::kPending, swap_promise_state);

  GetTestWebFrameWidget().set_event_causes_update(false);

  GetTestWebFrameWidget().SendInputEventAndWaitForDispatch(
      std::make_unique<WebKeyboardEvent>(
          WebInputEvent::Type::kRawKeyDown, WebInputEvent::kNoModifiers,
          WebInputEvent::GetStaticTimeStampForTests()));
  EXPECT_EQ(TestSwapPromise::State::kPending, swap_promise_state);

  GetTestWebFrameWidget().CompositeAndWaitForPresentation(Compositor());
  EXPECT_EQ(TestSwapPromise::State::kResolved, swap_promise_state);
}

// Verifies that when a non-rAF-aligned event is handled and causes an update,
// swap promises won't be broken.
TEST_F(EventHandlingWebFrameWidgetSimTest, NonRafAlignedEventWithUpdate) {
  TestSwapPromise::State swap_promise_state;
  GetTestWebFrameWidget().QueueSwapPromise(&swap_promise_state);
  EXPECT_EQ(TestSwapPromise::State::kPending, swap_promise_state);

  GetTestWebFrameWidget().set_event_causes_update(true);

  GetTestWebFrameWidget().SendInputEventAndWaitForDispatch(
      std::make_unique<WebKeyboardEvent>(
          WebInputEvent::Type::kRawKeyDown, WebInputEvent::kNoModifiers,
          WebInputEvent::GetStaticTimeStampForTests()));
  EXPECT_EQ(TestSwapPromise::State::kPending, swap_promise_state);

  GetTestWebFrameWidget().CompositeAndWaitForPresentation(Compositor());
  EXPECT_EQ(TestSwapPromise::State::kResolved, swap_promise_state);
}

// Verifies that when a rAF-aligned event is handled without causing an update,
// swap promises won't be broken.
TEST_F(EventHandlingWebFrameWidgetSimTest, RafAlignedEventWithoutUpdate) {
  TestSwapPromise::State swap_promise_state;
  GetTestWebFrameWidget().QueueSwapPromise(&swap_promise_state);
  EXPECT_EQ(TestSwapPromise::State::kPending, swap_promise_state);

  GetTestWebFrameWidget().set_event_causes_update(false);

  GetTestWebFrameWidget().SendInputEventAndWaitForDispatch(
      std::make_unique<WebMouseEvent>(WebInputEvent::Type::kMouseMove, 0,
                                      base::TimeTicks::Now()));
  EXPECT_EQ(TestSwapPromise::State::kPending, swap_promise_state);

  GetTestWebFrameWidget().CompositeAndWaitForPresentation(Compositor());
  EXPECT_EQ(TestSwapPromise::State::kResolved, swap_promise_state);
}

// Verifies that when a rAF-aligned event is handled and causes an update, swap
// promises won't be broken.
TEST_F(EventHandlingWebFrameWidgetSimTest, RafAlignedEventWithUpdate) {
  TestSwapPromise::State swap_promise_state;
  GetTestWebFrameWidget().QueueSwapPromise(&swap_promise_state);
  EXPECT_EQ(TestSwapPromise::State::kPending, swap_promise_state);

  GetTestWebFrameWidget().set_event_causes_update(true);

  GetTestWebFrameWidget().SendInputEventAndWaitForDispatch(
      std::make_unique<WebMouseEvent>(WebInputEvent::Type::kMouseMove, 0,
                                      base::TimeTicks::Now()));
  EXPECT_EQ(TestSwapPromise::State::kPending, swap_promise_state);

  GetTestWebFrameWidget().CompositeAndWaitForPresentation(Compositor());
  EXPECT_EQ(TestSwapPromise::State::kResolved, swap_promise_state);
}

}  // namespace blink
