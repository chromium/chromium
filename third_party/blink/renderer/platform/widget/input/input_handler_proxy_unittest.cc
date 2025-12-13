// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/input_handler_proxy.h"

#include <memory>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/trace_event_analyzer.h"
#include "base/test/trace_test_utils.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/events_metrics_manager.h"
#include "cc/test/mock_input_handler.h"
#include "components/input/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/renderer/platform/widget/input/compositor_thread_event_queue.h"
#include "third_party/blink/renderer/platform/widget/input/event_with_callback.h"
#include "third_party/blink/renderer/platform/widget/input/input_handler_proxy.h"
#include "third_party/blink/renderer/platform/widget/input/input_handler_proxy_client.h"
#include "third_party/blink/renderer/platform/widget/input/mock_input_handler_proxy_client.h"
#include "third_party/blink/renderer/platform/widget/input/scroll_predictor.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/latency/latency_info.h"

using cc::InputHandler;
using cc::ScrollBeginThreadState;
using cc::TouchAction;
using testing::_;
using testing::AllOf;
using testing::DoAll;
using testing::Eq;
using testing::Field;
using testing::Mock;
using testing::NiceMock;
using testing::Property;
using testing::Return;
using testing::SetArgPointee;
using testing::SizeIs;
using testing::StrictMock;

namespace blink {
namespace test {

namespace {

MATCHER_P(WheelEventsMatch, expected, "") {
  return WheelEventsMatch(arg, expected);
}

std::unique_ptr<WebInputEvent> CreateGestureScrollPinch(
    WebInputEvent::Type type,
    WebGestureDevice source_device,
    base::TimeTicks event_time,
    float delta_y_or_scale = 0,
    int x = 0,
    int y = 0) {
  auto gesture = std::make_unique<WebGestureEvent>(
      type, WebInputEvent::kNoModifiers, event_time, source_device);
  if (type == WebInputEvent::Type::kGestureScrollUpdate) {
    gesture->data.scroll_update.delta_y = delta_y_or_scale;
  } else if (type == WebInputEvent::Type::kGesturePinchUpdate) {
    gesture->data.pinch_update.scale = delta_y_or_scale;
    gesture->SetPositionInWidget(gfx::PointF(x, y));
  }
  return gesture;
}

class MockSynchronousInputHandler : public SynchronousInputHandler {
 public:
  MOCK_METHOD6(UpdateRootLayerState,
               void(const gfx::PointF& total_scroll_offset,
                    const gfx::PointF& max_scroll_offset,
                    const gfx::SizeF& scrollable_size,
                    float page_scale_factor,
                    float min_page_scale_factor,
                    float max_page_scale_factor));
};

WebTouchPoint CreateWebTouchPoint(WebTouchPoint::State state,
                                  float x,
                                  float y) {
  WebTouchPoint point;
  point.state = state;
  point.SetPositionInScreen(x, y);
  point.SetPositionInWidget(x, y);
  return point;
}

const cc::InputHandler::ScrollStatus kImplThreadScrollState{
    cc::InputHandler::ScrollThread::kScrollOnImplThread};

const cc::InputHandler::ScrollStatus kRequiresMainThreadHitTestState{
    cc::InputHandler::ScrollThread::kScrollOnImplThread,
    /*main_thread_hit_test_reasons*/
    cc::MainThreadScrollingReason::kMainThreadScrollHitTestRegion};

constexpr auto kSampleMainThreadScrollingReason =
    cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;

const cc::InputHandler::ScrollStatus kScrollIgnoredScrollState{
    cc::InputHandler::ScrollThread::kScrollIgnored};

}  // namespace

class TestInputHandlerProxy : public InputHandlerProxy {
 public:
  TestInputHandlerProxy(cc::InputHandler& input_handler,
                        InputHandlerProxyClient* client)
      : InputHandlerProxy(input_handler, client) {}
  void RecordScrollBeginForTest(WebGestureDevice device, uint32_t reasons) {
    RecordScrollBegin(device,
                      reasons & cc::MainThreadScrollingReason::kHitTestReasons,
                      reasons & cc::MainThreadScrollingReason::kRepaintReasons);
  }

  MOCK_METHOD0(SetNeedsAnimateInput, void());

  EventDisposition HitTestTouchEventForTest(
      const WebTouchEvent& touch_event,
      bool* is_touching_scrolling_layer,
      cc::TouchAction* allowed_touch_action) {
    return HitTestTouchEvent(touch_event, is_touching_scrolling_layer,
                             allowed_touch_action);
  }

  EventDisposition HandleMouseWheelForTest(
      const WebMouseWheelEvent& wheel_event) {
    return HandleMouseWheel(wheel_event);
  }

  // This is needed because the tests can't directly call
  // DispatchQueuedInputEvents since it is private.
  void DispatchQueuedInputEventsHelper() { DispatchQueuedInputEvents(true); }
};

// Whether or not the input handler says that the viewport is scrolling the
// root scroller or a child.
enum class ScrollerType { kRoot, kChild };

// Whether or not to setup a synchronous input handler. This simulates the mode
// that WebView runs in.
enum class HandlerType { kNormal, kSynchronous };

class InputHandlerProxyTest : public testing::Test,
                              public testing::WithParamInterface<
                                  std::tuple<ScrollerType, HandlerType>> {
  ScrollerType GetScrollerType() { return std::get<0>(GetParam()); }
  HandlerType GetHandlerType() { return std::get<1>(GetParam()); }

 public:
  InputHandlerProxyTest() {
    input_handler_ = std::make_unique<TestInputHandlerProxy>(
        mock_input_handler_, &mock_client_);
    scroll_result_did_scroll_.did_scroll = true;
    scroll_result_did_not_scroll_.did_scroll = false;

    if (GetHandlerType() == HandlerType::kSynchronous) {
      EXPECT_CALL(mock_input_handler_,
                  RequestUpdateForSynchronousInputHandler())
          .Times(1);
      input_handler_->SetSynchronousInputHandler(
          &mock_synchronous_input_handler_);
    }

    mock_input_handler_.set_is_scrolling_root(
        GetHandlerType() == HandlerType::kSynchronous &&
        GetScrollerType() == ScrollerType::kRoot);

    // Set a default device so tests don't always have to set this.
    gesture_.SetSourceDevice(WebGestureDevice::kTouchpad);

    input_handler_->set_event_attribution_enabled(false);
  }

  virtual ~InputHandlerProxyTest() = default;

// This is defined as a macro so the line numbers can be traced back to the
// correct spot when it fails.
#define EXPECT_SET_NEEDS_ANIMATE_INPUT(times)                              \
  do {                                                                     \
    EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(times); \
  } while (false)

// This is defined as a macro because when an expectation is not satisfied the
// only output you get out of gmock is the line number that set the expectation.
#define VERIFY_AND_RESET_MOCKS()                                     \
  do {                                                               \
    testing::Mock::VerifyAndClearExpectations(&mock_input_handler_); \
    testing::Mock::VerifyAndClearExpectations(                       \
        &mock_synchronous_input_handler_);                           \
    testing::Mock::VerifyAndClearExpectations(&mock_client_);        \
  } while (false)

  void Animate(base::TimeTicks time) { input_handler_->Animate(time); }

  void SetSmoothScrollEnabled(bool value) {}

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  void GestureScrollStarted();
  void GestureScrollIgnored();
  void FlingAndSnap();

  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::StrictMock<cc::MockInputHandler> mock_input_handler_;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler_;
  std::unique_ptr<TestInputHandlerProxy> input_handler_;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client_;
  WebGestureEvent gesture_;
  InputHandlerProxy::EventDisposition expected_disposition_ =
      InputHandlerProxy::DID_HANDLE;
  base::HistogramTester histogram_tester_;
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  cc::InputHandlerScrollResult scroll_result_did_not_scroll_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The helper basically returns the EventDisposition that is returned by
// RouteToTypeSpecificHandler. This is done by passing in a callback when
// calling HandleInputEventWithLatencyInfo. By design, DispatchSingleInputEvent
// will then call this callback with the disposition returned by
// RouteToTypeSpecificHandler and that is what gets returned by this helper.
InputHandlerProxy::EventDisposition HandleInputEventWithLatencyInfo(
    TestInputHandlerProxy* input_handler,
    const WebInputEvent& event) {
  std::unique_ptr<WebCoalescedInputEvent> scoped_input_event =
      std::make_unique<WebCoalescedInputEvent>(event.Clone(),
                                               ui::LatencyInfo());
  InputHandlerProxy::EventDisposition event_disposition =
      InputHandlerProxy::DID_NOT_HANDLE;
  input_handler->HandleInputEventWithLatencyInfo(
      std::move(scoped_input_event), nullptr,
      base::BindLambdaForTesting(
          [&event_disposition](
              InputHandlerProxy::EventDisposition disposition,
              std::unique_ptr<blink::WebCoalescedInputEvent> event,
              std::unique_ptr<InputHandlerProxy::DidOverscrollParams> callback,
              const WebInputEventAttribution& attribution,
              std::unique_ptr<cc::EventMetrics> metrics) {
            event_disposition = disposition;
          }));
  return event_disposition;
}

// This helper forces the CompositorThreadEventQueue to be flushed.
InputHandlerProxy::EventDisposition HandleInputEventAndFlushEventQueue(
    testing::StrictMock<cc::MockInputHandler>& mock_input_handler,
    TestInputHandlerProxy* input_handler,
    const WebInputEvent& event) {
  EXPECT_CALL(mock_input_handler, SetNeedsAnimateInput())
      .Times(testing::AnyNumber());

  std::unique_ptr<WebCoalescedInputEvent> scoped_input_event =
      std::make_unique<WebCoalescedInputEvent>(event.Clone(),
                                               ui::LatencyInfo());
  InputHandlerProxy::EventDisposition event_disposition =
      InputHandlerProxy::DID_NOT_HANDLE;
  input_handler->HandleInputEventWithLatencyInfo(
      std::move(scoped_input_event), nullptr,
      base::BindLambdaForTesting(
          [&event_disposition](
              InputHandlerProxy::EventDisposition disposition,
              std::unique_ptr<blink::WebCoalescedInputEvent> event,
              std::unique_ptr<InputHandlerProxy::DidOverscrollParams> callback,
              const WebInputEventAttribution& attribution,
              std::unique_ptr<cc::EventMetrics> metrics) {
            event_disposition = disposition;
          }));

  input_handler->DispatchQueuedInputEventsHelper();
  return event_disposition;
}

class InputHandlerProxyEventQueueTest
    : public testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  InputHandlerProxyEventQueueTest() = default;

  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (std::get<0>(GetParam())) {
      enabled_features.push_back(
          {input::features::kUpdateScrollPredictorInputMapping, {}});
    } else {
      disabled_features.push_back(
          input::features::kUpdateScrollPredictorInputMapping);
    }

    if (std::get<1>(GetParam())) {
      enabled_features.push_back(
          {features::kRefactorCompositorThreadEventQueue, {}});
    } else {
      disabled_features.push_back(
          features::kRefactorCompositorThreadEventQueue);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);

    input_handler_proxy_ = std::make_unique<TestInputHandlerProxy>(
        mock_input_handler_, &mock_client_);

    SetScrollPredictionEnabled(true);
  }

  ~InputHandlerProxyEventQueueTest() override = default;

  bool IsUpdateScrollPredictorInputMappingEnabled() const {
    return std::get<0>(GetParam());
  }

  bool IsRefactorCompositorThreadEventQueueEnabled() const {
    return std::get<1>(GetParam());
  }

  void HandleGestureEvent(WebInputEvent::Type type,
                          float delta_y_or_scale = 0,
                          int x = 0,
                          int y = 0) {
    HandleGestureEventWithSourceDevice(type, WebGestureDevice::kTouchscreen,
                                       delta_y_or_scale, x, y);
  }

  void HandleGestureEventWithSourceDevice(WebInputEvent::Type type,
                                          WebGestureDevice source_device,
                                          float delta_y_or_scale = 0,
                                          int x = 0,
                                          int y = 0) {
    InjectInputEvent(CreateGestureScrollPinch(
        type, source_device, input_handler_proxy_->tick_clock_->NowTicks(),
        delta_y_or_scale, x, y));
  }

  void InjectInputEvent(std::unique_ptr<WebInputEvent> event) {
    input_handler_proxy_->HandleInputEventWithLatencyInfo(
        std::make_unique<WebCoalescedInputEvent>(std::move(event),
                                                 ui::LatencyInfo()),
        nullptr,
        base::BindOnce(
            &InputHandlerProxyEventQueueTest::DidHandleInputEventAndOverscroll,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void HandleMouseEvent(WebInputEvent::Type type, int x = 0, int y = 0) {
    WebMouseEvent mouse_event(type, WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests());

    mouse_event.SetPositionInWidget(gfx::PointF(x, y));
    mouse_event.button = WebMouseEvent::Button::kLeft;
    HandleInputEventWithLatencyInfo(input_handler_proxy_.get(), mouse_event);
  }

  void DidHandleInputEventAndOverscroll(
      InputHandlerProxy::EventDisposition event_disposition,
      std::unique_ptr<WebCoalescedInputEvent> input_event,
      std::unique_ptr<InputHandlerProxy::DidOverscrollParams> overscroll_params,
      const WebInputEventAttribution& attribution,
      std::unique_ptr<cc::EventMetrics> metrics) {
    event_disposition_recorder_.push_back(event_disposition);
    latency_info_recorder_.push_back(input_event->latency_info());
  }

  base::circular_deque<std::unique_ptr<EventWithCallback>>& event_queue() {
    return input_handler_proxy_->compositor_event_queue_->queue_;
  }

  void SetInputHandlerProxyTickClockForTesting(
      const base::TickClock* tick_clock) {
    input_handler_proxy_->SetTickClockForTesting(tick_clock);
  }

  void DeliverInputForBeginFrame(
      base::TimeTicks frame_time = base::TimeTicks(),
      viz::BeginFrameArgs::BeginFrameArgsType begin_frame_args_type =
          viz::BeginFrameArgs::NORMAL) {
    constexpr base::TimeDelta interval = base::Milliseconds(16);
    if (frame_time.is_null()) {
      frame_time = WebInputEvent::GetStaticTimeStampForTests() +
                   (next_begin_frame_number_ -
                    viz::BeginFrameArgs::kStartingFrameNumber) *
                       interval;
    }
    input_handler_proxy_->DeliverInputForBeginFrame(viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, frame_time,
        frame_time + interval, interval, begin_frame_args_type));
  }

  void DeliverInputForHighLatencyMode() {
    input_handler_proxy_->DeliverInputForHighLatencyMode();
  }

  void SetScrollPredictionEnabled(bool enabled) {
    input_handler_proxy_->scroll_predictor_ =
        enabled ? std::make_unique<ScrollPredictor>() : nullptr;
  }

  std::unique_ptr<ui::InputPredictor::InputData>
  GestureScrollEventPredictionAvailable() {
    return input_handler_proxy_->scroll_predictor_->predictor_
        ->GeneratePrediction(WebInputEvent::GetStaticTimeStampForTests());
  }

  base::TimeTicks NowTimestampForEvents() {
    return input_handler_proxy_->tick_clock_->NowTicks();
  }

  TestInputHandlerProxy* GetInputHandlerProxy() {
    return input_handler_proxy_.get();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<cc::MockInputHandler> mock_input_handler_;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client_;
  std::unique_ptr<TestInputHandlerProxy> input_handler_proxy_;
  std::vector<InputHandlerProxy::EventDisposition> event_disposition_recorder_;
  std::vector<ui::LatencyInfo> latency_info_recorder_;

  uint64_t next_begin_frame_number_ = viz::BeginFrameArgs::kStartingFrameNumber;
  base::test::ScopedFeatureList scoped_feature_list_;

  base::WeakPtrFactory<InputHandlerProxyEventQueueTest> weak_ptr_factory_{this};
};

// Tests that changing source devices mid gesture scroll is handled gracefully.
// For example, when a touch scroll is in progress and the user initiates a
// scrollbar scroll before the touch scroll has had a chance to dispatch a GSE.
TEST_P(InputHandlerProxyTest, NestedGestureBasedScrollsDifferentSourceDevice) {
  // Touchpad initiates a scroll.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(ui::ScrollInputType::kWheel,
                        cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  gesture_.SetSourceDevice(WebGestureDevice::kTouchpad);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  // Before ScrollEnd for touchpad is fired, user starts a thumb drag. This is
  // expected to immediately end the touchpad scroll.
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(ui::ScrollInputType::kWheel))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(ui::ScrollInputType::kScrollbar,
                        cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(gfx::PointF(0, 20));
  mouse_event.button = WebMouseEvent::Button::kLeft;

  cc::InputHandlerPointerResult pointer_down_result;
  pointer_down_result.type = cc::PointerResultType::kScrollbarScroll;
  pointer_down_result.scroll_delta = gfx::Vector2dF(0, 1);
  EXPECT_CALL(mock_input_handler_, HitTest(_))
      .WillOnce(testing::Return(pointer_down_result.type));
  EXPECT_CALL(mock_input_handler_, MouseDown(_, _))
      .WillOnce(testing::Return(pointer_down_result));

  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), mouse_event));

  VERIFY_AND_RESET_MOCKS();

  // Touchpad GSE comes in while a scrollbar drag is in progress. This is
  // expected to be dropped because a scrollbar scroll is currently active.
  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.SetSourceDevice(WebGestureDevice::kTouchpad);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(ui::ScrollInputType::kWheel))
      .Times(1);
  EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  VERIFY_AND_RESET_MOCKS();

  // The GSE from the scrollbar needs to be handled.
  EXPECT_CALL(mock_input_handler_,
              RecordScrollEnd(ui::ScrollInputType::kScrollbar))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(1);
  cc::InputHandlerPointerResult pointer_up_result;
  pointer_up_result.type = cc::PointerResultType::kScrollbarScroll;
  EXPECT_CALL(mock_input_handler_, MouseUp(_))
      .WillOnce(testing::Return(pointer_up_result));
  mouse_event.SetType(WebInputEvent::Type::kMouseUp);
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), mouse_event));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelNoListener) {
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));

  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelPassiveListener) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));

  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelBlockingListener) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(true));

  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelBlockingAndPassiveListener) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(true));
  // We will not call GetEventListenerProperties because we early out when we
  // hit blocking region.
  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelEventOutsideBlockingListener) {
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Gt(10))))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Le(10))))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillRepeatedly(testing::Return(cc::EventListenerProperties::kBlocking));

  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  wheel.SetPositionInScreen(0, 5);
  wheel.SetPositionInWidget(0, 5);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest,
       MouseWheelEventOutsideBlockingListenerWithPassiveListener) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING;
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Gt(10))))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Le(10))))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillRepeatedly(
          testing::Return(cc::EventListenerProperties::kBlockingAndPassive));

  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  wheel.SetPositionInScreen(0, 5);
  wheel.SetPositionInWidget(0, 5);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelEventMayBeginPhaseNoListener) {
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillRepeatedly(testing::Return(cc::EventListenerProperties::kNone));

  {
    WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                             WebInputEvent::kControlKey,
                             WebInputEvent::GetStaticTimeStampForTests());
    wheel.phase = WebMouseWheelEvent::kPhaseMayBegin;
    wheel.dispatch_type = WebInputEvent::DispatchType::kBlocking;
    EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
              HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  }

  {
    WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                             WebInputEvent::kControlKey,
                             WebInputEvent::GetStaticTimeStampForTests());
    wheel.phase = WebMouseWheelEvent::kPhaseBegan;
    wheel.dispatch_type = WebInputEvent::DispatchType::kBlocking;
    EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
              HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  }

  {
    WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                             WebInputEvent::kControlKey,
                             WebInputEvent::GetStaticTimeStampForTests());
    wheel.phase = WebMouseWheelEvent::kPhaseEnded;
    wheel.dispatch_type = WebInputEvent::DispatchType::kEventNonBlocking;
    EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
              HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  }

  {
    WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                             WebInputEvent::kControlKey,
                             WebInputEvent::GetStaticTimeStampForTests());
    wheel.phase = WebMouseWheelEvent::kPhaseMayBegin;
    wheel.dispatch_type = WebInputEvent::DispatchType::kEventNonBlocking;
    EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
              HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  }

  VERIFY_AND_RESET_MOCKS();
}

// Tests that changing source devices when an animated scroll is in progress
// ends the current scroll offset animation and ensures that a new one gets
// created.
TEST_P(InputHandlerProxyTest, ScrollbarScrollEndOnDeviceChange) {
  // A scrollbar scroll begins.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(ui::ScrollInputType::kScrollbar,
                        cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(gfx::PointF(0, 20));
  mouse_event.button = WebMouseEvent::Button::kLeft;
  cc::InputHandlerPointerResult pointer_down_result;
  pointer_down_result.type = cc::PointerResultType::kScrollbarScroll;
  pointer_down_result.scroll_delta = gfx::Vector2dF(0, 1);
  EXPECT_CALL(mock_input_handler_, HitTest(_))
      .WillOnce(testing::Return(pointer_down_result.type));
  EXPECT_CALL(mock_input_handler_, MouseDown(_, _))
      .WillOnce(testing::Return(pointer_down_result));
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), mouse_event));

  EXPECT_EQ(input_handler_->currently_active_gesture_device(),
            WebGestureDevice::kScrollbar);
  VERIFY_AND_RESET_MOCKS();

  // A mousewheel tick takes place before the scrollbar scroll ends.
  EXPECT_CALL(mock_input_handler_,
              RecordScrollEnd(ui::ScrollInputType::kScrollbar))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(ui::ScrollInputType::kWheel,
                        cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  gesture_.SetSourceDevice(WebGestureDevice::kTouchpad);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  EXPECT_EQ(input_handler_->currently_active_gesture_device(),
            WebGestureDevice::kTouchpad);

  VERIFY_AND_RESET_MOCKS();

  // Mousewheel GSE is then fired and the mousewheel scroll ends.
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(ui::ScrollInputType::kWheel))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.SetSourceDevice(WebGestureDevice::kTouchpad);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Mouse up gets ignored as the scrollbar scroll already ended before the
  // mousewheel tick took place.
  EXPECT_CALL(mock_input_handler_,
              RecordScrollEnd(ui::ScrollInputType::kScrollbar))
      .Times(1);
  mouse_event.SetType(WebInputEvent::Type::kMouseUp);
  cc::InputHandlerPointerResult pointer_up_result;
  pointer_up_result.type = cc::PointerResultType::kScrollbarScroll;
  EXPECT_CALL(mock_input_handler_, MouseUp(_))
      .WillOnce(testing::Return(pointer_up_result));
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), mouse_event));
  VERIFY_AND_RESET_MOCKS();
}

void InputHandlerProxyTest::GestureScrollStarted() {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  // The event should not be marked as handled if scrolling is not possible.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  // Mark the event as handled if scroll happens.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}
TEST_P(InputHandlerProxyTest, GestureScrollStarted) {
  GestureScrollStarted();
}

TEST_P(InputHandlerProxyTest, GestureScrollIgnored) {
  // We shouldn't handle the GestureScrollBegin.
  // Instead, we should get a DROP_EVENT result, indicating
  // that we could determine that there's nothing that could scroll or otherwise
  // react to this gesture sequence and thus we should drop the whole gesture
  // sequence on the floor, except for the ScrollEnd.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kScrollIgnoredScrollState));
  EXPECT_CALL(mock_input_handler_, RecordScrollBegin(_, _)).Times(0);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  // GSB is dropped and not sent to the main thread, GSE shouldn't get sent to
  // the main thread, either.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(0);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureScrollByPage) {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  gesture_.data.scroll_begin.delta_hint_units =
      ui::ScrollGranularity::kScrollByPage;
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));

  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = 1;
  gesture_.data.scroll_update.delta_units =
      ui::ScrollGranularity::kScrollByPage;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(_)).Times(1);
  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureScrollBeginThatTargetViewport) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, RootScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  gesture_.data.scroll_begin.target_viewport = true;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}

void InputHandlerProxyTest::FlingAndSnap() {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  // The event should be dropped if InputHandler decides to snap.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  gesture_.data.scroll_update.inertial_phase =
      WebGestureEvent::InertialPhaseState::kMomentum;
  EXPECT_CALL(mock_input_handler_,
              GetSnapFlingInfoAndSetAnimatingSnapTarget(_, _, _, _))
      .WillOnce(DoAll(testing::SetArgPointee<2>(gfx::PointF(0, 0)),
                      testing::SetArgPointee<3>(gfx::PointF(0, 100)),
                      testing::Return(true)));
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, SnapFlingIgnoresFollowingGSUAndGSE) {
  FlingAndSnap();
  // The next GestureScrollUpdate should also be ignored, and will not ask for
  // snap position.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;

  EXPECT_CALL(mock_input_handler_,
              GetSnapFlingInfoAndSetAnimatingSnapTarget(_, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(0);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  VERIFY_AND_RESET_MOCKS();

  // The GestureScrollEnd should also be ignored.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.data.scroll_end.inertial_phase =
      WebGestureEvent::InertialPhaseState::kMomentum;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(_)).Times(0);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GesturePinch) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchBegin);
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin(_, _));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 1.5;
  gesture_.SetPositionInWidget(gfx::PointF(7, 13));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(1.5, gfx::Point(7, 13)));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 0.5;
  gesture_.SetPositionInWidget(gfx::PointF(9, 6));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(.5, gfx::Point(9, 6)));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchEnd);
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point(9, 6)));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest,
       GestureScrollOnImplThreadFlagClearedAfterScrollEnd) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  // |gesture_scroll_on_impl_thread_| should be false once a GestureScrollEnd
  // gets handled.
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest,
       BeginScrollWhenGestureScrollOnImplThreadFlagIsSet) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, HitTestTouchEventNonNullTouchAction) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Rect::x, testing::Eq(0)), _))
      .WillOnce([](const gfx::Rect&, cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kMax;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::kNoHandler;
      });

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Rect::x, testing::Gt(0)), _))
      .WillOnce(
          [](const gfx::Rect&, cc::TouchAction* touch_action) {
            *touch_action = cc::TouchAction::kPanUp;
            return cc::InputHandler::TouchStartOrMoveEventListenerType::
                kHandlerOnScrollingLayer;
          });
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  touch.touches[1] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  touch.touches[2] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, -10, 10);

  bool is_touching_scrolling_layer;
  cc::TouchAction allowed_touch_action = cc::TouchAction::kAuto;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HitTestTouchEventForTest(
                touch, &is_touching_scrolling_layer, &allowed_touch_action));
  EXPECT_TRUE(is_touching_scrolling_layer);
  EXPECT_EQ(allowed_touch_action, cc::TouchAction::kPanUp);
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyEventQueueTest, DeliverInputForDeadlineIsScheduled) {
  if (IsUpdateScrollPredictorInputMappingEnabled()) {
    return;
  }

  constexpr float kDeadlineRatio = 0.333;
  constexpr base::TimeDelta kTimeBetweenEvents = base::Milliseconds(2);
  constexpr float kSchedulerSlack = 0.75;

  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);
  input_handler_proxy_->SetScrollEventDispatchMode(
      cc::InputHandlerClient::ScrollEventDispatchMode::
          kUseScrollPredictorForDeadline,
      kDeadlineRatio);

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  // ScrollUpdate should be called twice after regular scroll update gestures
  // and once after a synthetic prediction.
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(3);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  DeliverInputForBeginFrame();

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, 5);
  DeliverInputForBeginFrame();
  auto result = GestureScrollEventPredictionAvailable();
  EXPECT_FALSE(result);

  // Predictor needs at least 2 ms of delta between events to make a prediction.
  tick_clock.Advance(kTimeBetweenEvents);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  DeliverInputForBeginFrame();
  result = GestureScrollEventPredictionAvailable();
  EXPECT_TRUE(result);

  // DeliverInputForBeginFrame will start a timer to run DeliverInputForDeadline
  // around the deadline ratio.
  DeliverInputForBeginFrame(tick_clock.NowTicks());

  tick_clock.Advance(viz::BeginFrameArgs::DefaultInterval() * kDeadlineRatio *
                     kSchedulerSlack);
  task_environment_.FastForwardBy(kTimeBetweenEvents +
                                  viz::BeginFrameArgs::DefaultInterval() *
                                      kDeadlineRatio * kSchedulerSlack);

  result = GestureScrollEventPredictionAvailable();
  EXPECT_TRUE(result);
}

// Tests that multiple mousedown(s) on scrollbar are handled gracefully and
// don't fail any DCHECK(s).
TEST_P(InputHandlerProxyEventQueueTest,
       NestedGestureBasedScrollsSameSourceDevice) {
  cc::InputHandlerPointerResult pointer_down_result;
  pointer_down_result.type = cc::PointerResultType::kScrollbarScroll;
  pointer_down_result.scroll_delta = gfx::Vector2dF(0, 1);
  cc::InputHandlerPointerResult pointer_up_result;
  pointer_up_result.type = cc::PointerResultType::kScrollbarScroll;

  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
      .Times(testing::AnyNumber());
  EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
      .Times(testing::AnyNumber());

  // We expect two full gestures to be processed. The .Times(2) expectation
  // correctly verifies this without being brittle about the exact dispatch
  // order.
  EXPECT_CALL(mock_input_handler_, HitTest(_))
      .Times(2)
      .WillRepeatedly(testing::Return(pointer_down_result.type));
  EXPECT_CALL(mock_input_handler_, MouseDown(_, _))
      .Times(2)
      .WillRepeatedly(testing::Return(pointer_down_result));
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .Times(2)
      .WillRepeatedly(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(2);
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(2);
  EXPECT_CALL(mock_input_handler_, MouseUp(_))
      .Times(2)
      .WillRepeatedly(testing::Return(pointer_up_result));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(2);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(2);

  // --- Event Queuing ---
  HandleMouseEvent(WebInputEvent::Type::kMouseDown);
  HandleMouseEvent(WebInputEvent::Type::kMouseUp);
  HandleMouseEvent(WebInputEvent::Type::kMouseDown);
  HandleMouseEvent(WebInputEvent::Type::kMouseUp);

  input_handler_proxy_->DispatchQueuedInputEventsHelper();
}

// Tests that the allowed touch action is correctly set when a touch is made
// non-blocking due to an ongoing fling. https://crbug.com/1048098.
TEST_P(InputHandlerProxyEventQueueTest, AckTouchActionNonBlockingForFling) {
  // Add a mock clock to control event and frame timing.
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);

  // Simulate starting a compositor scroll and then flinging. This is setup for
  // the real checks below.
  {
    float delta = 10;

    // ScrollBegin
    {
      EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
          .WillOnce(Return(kImplThreadScrollState));
      EXPECT_CALL(
          mock_input_handler_,
          RecordScrollBegin(_, ScrollBeginThreadState::kScrollingOnCompositor))
          .Times(1);
      if (!::features::IsCCSlimmingEnabled()) {
        EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
            .Times(1)
            .WillOnce(testing::Return(cc::ElementId()));
      }

      HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin, delta);
      Mock::VerifyAndClearExpectations(&mock_input_handler_);
    }

    // ScrollUpdate
    {
      EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
      if (!::features::IsCCSlimmingEnabled()) {
        EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
            .Times(1)
            .WillOnce(testing::Return(cc::ElementId()));
      }
      EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);

      HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, delta);
      // Advance time and provide it to DeliverInputForBeginFrame to ensure the
      // GSU is processed now.
      tick_clock.Advance(base::Milliseconds(8));
      DeliverInputForBeginFrame(tick_clock.NowTicks());
      Mock::VerifyAndClearExpectations(&mock_input_handler_);
    }

    // Start a fling - ScrollUpdate with momentum
    {
      cc::InputHandlerScrollResult scroll_result_did_scroll;
      scroll_result_did_scroll.did_scroll = true;
      EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _))
          .WillOnce(Return(scroll_result_did_scroll));
      EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
      EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
          .Times(::features::IsCCSlimmingEnabled() ? 1 : 2)
          .WillRepeatedly(testing::Return(cc::ElementId()));
      EXPECT_CALL(mock_input_handler_,
                  GetSnapFlingInfoAndSetAnimatingSnapTarget(_, _, _, _))
          .WillOnce(Return(false));

      auto gsu_fling = CreateGestureScrollPinch(
          WebInputEvent::Type::kGestureScrollUpdate,
          WebGestureDevice::kTouchscreen, NowTimestampForEvents(), delta,
          /*x=*/0, /*y=*/0);
      static_cast<WebGestureEvent*>(gsu_fling.get())
          ->data.scroll_update.inertial_phase =
          WebGestureEvent::InertialPhaseState::kMomentum;
      InjectInputEvent(std::move(gsu_fling));
      // Advance time and provide it to DeliverInputForBeginFrame
      // to ensure the fling state is set before the next event.
      tick_clock.Advance(base::Milliseconds(8));
      DeliverInputForBeginFrame(tick_clock.NowTicks());
    }
  }

  // We're now in an active gesture fling. Simulate the user touching down on
  // the screen. If this touch hits a blocking region (e.g. touch-action or a
  // non-passive touchstart listener), we won't actually treat it as blocking;
  // because of the ongoing fling it will be treated as non blocking. However,
  // we also have to ensure that the allowed_touch_action reported is also kAuto
  // so that the browser knows that it shouldn't wait for an ACK with an allowed
  // touch-action before dispatching more scrolls.
  {
    // Simulate hitting a blocking region on the scrolling layer, as if there
    // was a non-passive touchstart handler.
    EXPECT_CALL(mock_input_handler_,
                EventListenerTypeForTouchStartOrMoveAt(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(TouchAction::kNone),
                        Return(InputHandler::TouchStartOrMoveEventListenerType::
                                   kHandlerOnScrollingLayer)));

    std::unique_ptr<WebTouchEvent> touch_start =
        std::make_unique<WebTouchEvent>(
            WebInputEvent::Type::kTouchStart, WebInputEvent::kNoModifiers,
            WebInputEvent::GetStaticTimeStampForTests());
    touch_start->touches_length = 1;
    touch_start->touch_start_or_first_touch_move = true;
    touch_start->touches[0] =
        CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);

    // This is the call this test is checking: we expect that the client will
    // report the touch as non-blocking and also that the allowed touch action
    // matches the non blocking expectation (i.e. all touches are allowed).
    EXPECT_CALL(mock_client_, SetAllowedTouchAction(TouchAction::kAuto))
        .WillOnce(Return());
    EXPECT_CALL(mock_input_handler_, SetIsHandlingTouchSequence(true));

    InjectInputEvent(std::move(touch_start));
  }
}

// Tests that correct values for the
// Event.ScrollJank.EmptyGestureScrollUpdateFrame histogram are recorded.
TEST_P(InputHandlerProxyEventQueueTest, EmptyGestureScrollUpdateHistogram) {
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  cc::InputHandlerScrollResult scroll_result_did_not_scroll_;
  scroll_result_did_not_scroll_.did_scroll = false;

  base::HistogramTester histogram_tester;
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);

  input_handler_proxy_->SetScrollEventDispatchMode(
      cc::InputHandlerClient::ScrollEventDispatchMode::
          kDispatchScrollEventsUntilDeadline,
      0.333);

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  // If input mapping and queue refactoring are disabled, the first update is
  // dispatched and the following ones are enqueued and then dispatched. The
  // mock will return 'did not scroll' for the first two and 'did scroll' for
  // the third one.
  if (!IsRefactorCompositorThreadEventQueueEnabled() ||
      !IsUpdateScrollPredictorInputMappingEnabled()) {
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _))
        .WillOnce(testing::Return(scroll_result_did_not_scroll_))
        .WillOnce(testing::Return(scroll_result_did_not_scroll_))
        .WillOnce(testing::Return(scroll_result_did_scroll_));
  }

  // If input mapping and queue refactoring are enabled, all updates are
  // enqueued and then dispatched. The mock will return 'did not scroll' for the
  // first one and 'did scroll' for the second one.
  if (IsRefactorCompositorThreadEventQueueEnabled() &&
      IsUpdateScrollPredictorInputMappingEnabled()) {
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _))
        .WillOnce(testing::Return(scroll_result_did_not_scroll_))
        .WillOnce(testing::Return(scroll_result_did_scroll_));
  }

  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(2);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  DeliverInputForBeginFrame();

  if (!IsRefactorCompositorThreadEventQueueEnabled() ||
      !IsUpdateScrollPredictorInputMappingEnabled()) {
    // The first (empty) scroll update will be dispatched immediately.
    HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, 0);
  }

  // This is the only update in the queue and the mock returned 'did not
  // scroll', so the histogram value should be false.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, 0);
  DeliverInputForBeginFrame();
  GetInputHandlerProxy()->DispatchQueuedInputEventsHelper();

  histogram_tester.ExpectBucketCount(
      "Event.ScrollJank.EmptyGestureScrollUpdateFrame", false, 1);

  // This is the only update in the queue and the mock returned 'did scroll', so
  // the histogram value should be true.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, 0);
  DeliverInputForBeginFrame();
  GetInputHandlerProxy()->DispatchQueuedInputEventsHelper();

  histogram_tester.ExpectBucketCount(
      "Event.ScrollJank.EmptyGestureScrollUpdateFrame", true, 1);
}

// Verifies that when the `filter_out_empty_updates` parameter for the
// SendEmptyGestureScrollUpdate feature is set to true, empty
// GestureScrollUpdates are discarded on the renderer side.
TEST_P(InputHandlerProxyEventQueueTest, FilterOutEmptyUpdates) {
  base::test::ScopedFeatureList feature_list;
  std::vector<base::test::FeatureRefAndParams> enabled_features;

  // Always enable the empty update filtering.
  enabled_features.push_back({::features::kSendEmptyGestureScrollUpdate,
                              {{"filter_out_empty_updates", "true"}}});

  if (IsUpdateScrollPredictorInputMappingEnabled()) {
    // kUpdateScrollPredictorInputMapping is ENABLED. Disable synthetic
    // prediction for this test.
    enabled_features.push_back(
        {input::features::kUpdateScrollPredictorInputMapping,
         {{"generate_synthetic_scroll", "false"}}});
  }

  feature_list.InitWithFeaturesAndParameters(enabled_features,
                                             /* disabled_features */ {});
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);

  input_handler_proxy_->SetScrollEventDispatchMode(
      cc::InputHandlerClient::ScrollEventDispatchMode::
          kDispatchScrollEventsUntilDeadline,
      0.333);

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(2);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  DeliverInputForBeginFrame();

  // The first (empty) scroll update will be discarded on the renderer side.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, 0);
  DeliverInputForBeginFrame();
  auto result = GestureScrollEventPredictionAvailable();
  EXPECT_FALSE(result);

  // Predictor needs at least 2 ms of delta between events to make a prediction.
  tick_clock.Advance(base::Milliseconds(2));

  // The second (non-empty) scroll update will be sent to the predictor,
  // enqueued and then dispatched.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  DeliverInputForBeginFrame();

  // Since the first update was not sent to the predictor, prediction is not
  // available.
  result = GestureScrollEventPredictionAvailable();
  EXPECT_FALSE(result);

  // Predictor needs at least 2 ms of delta between events to make a prediction.
  tick_clock.Advance(base::Milliseconds(2));

  // The third (non-empty) scroll update will be sent to the predictor,
  // enqueued and then dispatched.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -5);

  // There are two events with > 2ms between them, so prediction should be
  // available.
  result = GestureScrollEventPredictionAvailable();
  EXPECT_TRUE(result);
}

// Verifies that when the `filter_out_empty_updates` parameter for the
// SendEmptyGestureScrollUpdate feature is set to true, empty
// GestureScrollUpdates are discarded on the renderer side even if
// UpdateScrollPredictorInputMapping is also enabled.
TEST_P(InputHandlerProxyEventQueueTest,
       FilterOutEmptyUpdatesWhenInputPredictorMappingEnabled) {
  base::test::ScopedFeatureList feature_list;
  std::vector<base::test::FeatureRefAndParams> enabled_features;

  // Always enable the empty update filtering.
  enabled_features.push_back({
      ::features::kSendEmptyGestureScrollUpdate,
      {{"filter_out_empty_updates", "true"}},
  });

  feature_list.InitWithFeaturesAndParameters(enabled_features,
                                             /* disabled_features */ {});
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);

  input_handler_proxy_->SetScrollEventDispatchMode(
      cc::InputHandlerClient::ScrollEventDispatchMode::
          kDispatchScrollEventsUntilDeadline,
      0.333);

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  if (IsRefactorCompositorThreadEventQueueEnabled() &&
      IsUpdateScrollPredictorInputMappingEnabled()) {
    EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  }
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  DeliverInputForBeginFrame();

  // The first (empty) scroll update will be discarded on the renderer side.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, 0);
  DeliverInputForBeginFrame();

  // The second (non-empty) scroll update will be enqueued if predictor input
  // mapping and compositor queue refactoring are enabled. Otherwise, the update
  // will de dispatched.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);

  if (IsRefactorCompositorThreadEventQueueEnabled() &&
      IsUpdateScrollPredictorInputMappingEnabled()) {
    EXPECT_EQ(1ul, event_queue().size());
  } else {
    EXPECT_EQ(0ul, event_queue().size());
  }
  GetInputHandlerProxy()->DispatchQueuedInputEventsHelper();
  EXPECT_EQ(0ul, event_queue().size());
}

TEST_P(InputHandlerProxyTest, HitTestTouchEventNullTouchAction) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Rect::x, testing::Eq(0)), _))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::kNoHandler));

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Rect::x, testing::Gt(0)), _))
      .WillOnce(
          testing::Return(cc::InputHandler::TouchStartOrMoveEventListenerType::
                              kHandlerOnScrollingLayer));
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::Type::kTouchMove,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.touches_length = 3;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  touch.touches[1] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  touch.touches[2] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, -10, 10);

  bool is_touching_scrolling_layer;
  cc::TouchAction* allowed_touch_action = nullptr;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HitTestTouchEventForTest(
                touch, &is_touching_scrolling_layer, allowed_touch_action));
  EXPECT_TRUE(is_touching_scrolling_layer);
  EXPECT_TRUE(!allowed_touch_action);
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MultiTouchPointHitTestNegative) {
  // None of the three touch points fall in the touch region. So the event
  // should be dropped.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchEndOrCancel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .Times(2)
      .WillRepeatedly([](const gfx::Rect&, cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kPanUp;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::kNoHandler;
      });
  EXPECT_CALL(mock_client_, SetAllowedTouchAction(cc::TouchAction::kPanUp))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStateStationary, 0, 0);
  touch.touches[1] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  touch.touches[2] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MultiTouchPointHitTestPositive) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Rect::x, testing::Eq(0)), _))
      .WillOnce([](const gfx::Rect&, cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kAuto;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::kNoHandler;
      });
  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Rect::x, testing::Gt(0)), _))
      .WillOnce(
          [](const gfx::Rect&, cc::TouchAction* touch_action) {
            *touch_action = cc::TouchAction::kPanY;
            return cc::InputHandler::TouchStartOrMoveEventListenerType::
                kHandlerOnScrollingLayer;
          });
  EXPECT_CALL(mock_client_, SetAllowedTouchAction(cc::TouchAction::kPanY))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock_input_handler_, SetIsHandlingTouchSequence(true));
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  touch.touches[1] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  touch.touches[2] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MultiTouchPointHitTestPassivePositive) {
  // One of the touch points is not on a touch-region. So the event should be
  // sent to the impl thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillRepeatedly(testing::Return(cc::EventListenerProperties::kPassive));
  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .Times(3)
      .WillOnce([](const gfx::Rect&, cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kPanRight;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::kNoHandler;
      })
      .WillRepeatedly([](const gfx::Rect&, cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kPanX;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::kNoHandler;
      });
  EXPECT_CALL(mock_client_, SetAllowedTouchAction(cc::TouchAction::kPanRight))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock_input_handler_, SetIsHandlingTouchSequence(true));

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  touch.touches[1] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  touch.touches[2] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, TouchTrackingEndsOnCancel) {
  // One of the touch points is not on a touch-region. So the event should be
  // sent to the impl thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillRepeatedly(testing::Return(cc::EventListenerProperties::kPassive));
  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .Times(1)
      .WillOnce([](const gfx::Rect&, cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kPanRight;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::kNoHandler;
      });
  EXPECT_CALL(mock_client_, SetAllowedTouchAction(cc::TouchAction::kPanRight))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock_input_handler_, SetIsHandlingTouchSequence(true));
  EXPECT_CALL(mock_input_handler_, HitTest(_))
      .WillOnce(testing::Return(cc::PointerResultType::kUnhandled));

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));

  VERIFY_AND_RESET_MOCKS();

  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, HitTest(_))
      .WillOnce(testing::Return(cc::PointerResultType::kUnhandled));

  WebMouseEvent mouse_down(WebInputEvent::Type::kMouseDown,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests());
  mouse_down.button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), mouse_down));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, SetIsHandlingTouchSequence(false));
  WebTouchEvent touch_cancel(WebInputEvent::Type::kTouchCancel,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());

  touch_cancel.unique_touch_event_id = 2;
  EXPECT_EQ(expected_disposition_, HandleInputEventWithLatencyInfo(
                                       input_handler_.get(), touch_cancel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, TouchStartPassiveAndTouchEndBlocking) {
  // The touch start is not in a touch-region but there is a touch end handler
  // so to maintain targeting we need to dispatch the touch start as
  // non-blocking but drop all touch moves.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchEndOrCancel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kBlocking));
  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .WillOnce([](const gfx::Rect&, cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kNone;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::kNoHandler;
      });
  EXPECT_CALL(mock_client_, SetAllowedTouchAction(cc::TouchAction::kNone))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock_input_handler_, SetIsHandlingTouchSequence(true));

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  touch.unique_touch_event_id = 1;
  touch.touches_length = 1;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  touch.touch_start_or_first_touch_move = true;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));

  touch.SetType(WebInputEvent::Type::kTouchMove);
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = false;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, TouchMoveBlockingAddedAfterPassiveTouchStart) {
  // The touch start is not in a touch-region but there is a touch end handler
  // so to maintain targeting we need to dispatch the touch start as
  // non-blocking but drop all touch moves.
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));
  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::kNoHandler));
  EXPECT_CALL(mock_client_, SetAllowedTouchAction(_))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock_input_handler_, HitTest(_))
      .WillOnce(testing::Return(cc::PointerResultType::kUnhandled));
  EXPECT_CALL(mock_input_handler_, SetIsHandlingTouchSequence(true));

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));

  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::kHandler));
  EXPECT_CALL(mock_client_, SetAllowedTouchAction(_))
      .WillOnce(testing::Return());

  touch.SetType(WebInputEvent::Type::kTouchMove);
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStateMoved, 10, 10);
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, UpdateBrowserControlsState) {
  VERIFY_AND_RESET_MOCKS();
  EXPECT_CALL(
      mock_input_handler_,
      UpdateBrowserControlsState(cc::BrowserControlsState::kShown,
                                 cc::BrowserControlsState::kBoth, true, _))
      .Times(1);

  input_handler_->UpdateBrowserControlsState(cc::BrowserControlsState::kShown,
                                             cc::BrowserControlsState::kBoth,
                                             true, std::nullopt);
  VERIFY_AND_RESET_MOCKS();
}

class UnifiedScrollingInputHandlerProxyTest : public testing::Test {
 public:
  using ElementId = cc::ElementId;
  using EventDisposition = InputHandlerProxy::EventDisposition;
  using EventDispositionCallback = InputHandlerProxy::EventDispositionCallback;
  using LatencyInfo = ui::LatencyInfo;
  using ScrollGranularity = ui::ScrollGranularity;
  using ScrollState = cc::ScrollState;
  using ReturnedDisposition = std::optional<EventDisposition>;

  UnifiedScrollingInputHandlerProxyTest()
      : input_handler_proxy_(mock_input_handler_, &mock_client_) {}

  std::unique_ptr<WebCoalescedInputEvent> ScrollBegin() {
    auto gsb = std::make_unique<WebGestureEvent>(
        WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
        TimeForInputEvents(), WebGestureDevice::kTouchpad);
    gsb->data.scroll_begin.scrollable_area_element_id = 0;
    gsb->data.scroll_begin.main_thread_hit_tested_reasons =
        cc::MainThreadScrollingReason::kNotScrollingOnMain;
    gsb->data.scroll_begin.delta_x_hint = 0;
    gsb->data.scroll_begin.delta_y_hint = 10;
    gsb->data.scroll_begin.pointer_count = 0;

    LatencyInfo unused;
    return std::make_unique<WebCoalescedInputEvent>(std::move(gsb), unused);
  }

  std::unique_ptr<WebCoalescedInputEvent> ScrollUpdate() {
    auto gsu = std::make_unique<WebGestureEvent>(
        WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
        TimeForInputEvents(), WebGestureDevice::kTouchpad);
    gsu->data.scroll_update.delta_x = 0;
    gsu->data.scroll_update.delta_y = 10;

    LatencyInfo unused;
    return std::make_unique<WebCoalescedInputEvent>(std::move(gsu), unused);
  }

  std::unique_ptr<WebCoalescedInputEvent> ScrollEnd() {
    auto gse = std::make_unique<WebGestureEvent>(
        WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
        TimeForInputEvents(), WebGestureDevice::kTouchpad);

    LatencyInfo unused;
    return std::make_unique<WebCoalescedInputEvent>(std::move(gse), unused);
  }

  void DispatchEvent(std::unique_ptr<blink::WebCoalescedInputEvent> event,
                     ReturnedDisposition* out_disposition = nullptr) {
    input_handler_proxy_.HandleInputEventWithLatencyInfo(
        std::move(event), nullptr, BindEventHandledCallback(out_disposition));
  }

  void ContinueScrollBeginAfterMainThreadHitTest(
      std::unique_ptr<WebCoalescedInputEvent> event,
      cc::ElementId hit_test_result,
      ReturnedDisposition* out_disposition = nullptr) {
    input_handler_proxy_.ContinueScrollBeginAfterMainThreadHitTest(
        std::move(event), nullptr, BindEventHandledCallback(out_disposition),
        hit_test_result);
  }

  bool MainThreadHitTestInProgress() const {
    return input_handler_proxy_.scroll_begin_main_thread_hit_test_reasons_ !=
           cc::MainThreadScrollingReason::kNotScrollingOnMain;
  }

  void BeginFrame() {
    constexpr base::TimeDelta interval = base::Milliseconds(16);
    base::TimeTicks frame_time =
        TimeForInputEvents() +
        (next_begin_frame_number_ - viz::BeginFrameArgs::kStartingFrameNumber) *
            interval;
    input_handler_proxy_.DeliverInputForBeginFrame(viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, frame_time,
        frame_time + interval, interval, viz::BeginFrameArgs::NORMAL));
  }

  cc::InputHandlerScrollResult DidScrollResult() const {
    cc::InputHandlerScrollResult result;
    result.did_scroll = true;
    return result;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<cc::MockInputHandler> mock_input_handler_;
  NiceMock<MockInputHandlerProxyClient> mock_client_;

 private:
  void EventHandledCallback(
      ReturnedDisposition* out_disposition,
      EventDisposition event_disposition,
      std::unique_ptr<WebCoalescedInputEvent> input_event,
      std::unique_ptr<InputHandlerProxy::DidOverscrollParams> overscroll_params,
      const WebInputEventAttribution& attribution,
      std::unique_ptr<cc::EventMetrics> metrics) {
    if (out_disposition)
      *out_disposition = event_disposition;
  }

  EventDispositionCallback BindEventHandledCallback(
      ReturnedDisposition* out_disposition = nullptr) {
    return base::BindOnce(
        &UnifiedScrollingInputHandlerProxyTest::EventHandledCallback,
        weak_ptr_factory_.GetWeakPtr(), out_disposition);
  }

  base::TimeTicks TimeForInputEvents() const {
    return WebInputEvent::GetStaticTimeStampForTests();
  }

  InputHandlerProxy input_handler_proxy_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestTickClock tick_clock_;
  uint64_t next_begin_frame_number_ =
      viz::BeginFrameArgs::kStartingFrameNumber + 1;
  base::WeakPtrFactory<UnifiedScrollingInputHandlerProxyTest> weak_ptr_factory_{
      this};
};

// Test that when a main thread hit test is requested, the InputHandlerProxy
// starts queueing incoming gesture event and the compositor queue is blocked
// until the hit test is satisfied.
TEST_F(UnifiedScrollingInputHandlerProxyTest, MainThreadHitTestRequired) {
  // The hit testing state shouldn't be entered until one is actually requested.
  EXPECT_FALSE(MainThreadHitTestInProgress());

  // Inject a GSB that returns RequiresMainThreadHitTest.
  {
    EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
        .WillOnce(Return(kRequiresMainThreadHitTestState));

    ReturnedDisposition disposition;
    DispatchEvent(ScrollBegin(), &disposition);

    EXPECT_TRUE(MainThreadHitTestInProgress());
    EXPECT_EQ(InputHandlerProxy::REQUIRES_MAIN_THREAD_HIT_TEST, *disposition);

    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }

  ReturnedDisposition gsu1_disposition;
  ReturnedDisposition gsu2_disposition;

  // Now inject a GSU. This should be queued.
  {
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(0);

    DispatchEvent(ScrollUpdate(), &gsu1_disposition);
    EXPECT_FALSE(gsu1_disposition);

    // Ensure the queue is blocked; a BeginFrame doesn't cause event dispatch.
    BeginFrame();
    EXPECT_FALSE(gsu1_disposition);

    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }

  // Inject a second GSU; it should be coalesced and also queued.
  {
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(0);

    DispatchEvent(ScrollUpdate(), &gsu2_disposition);
    EXPECT_FALSE(gsu2_disposition);

    // Ensure the queue is blocked.
    BeginFrame();
    EXPECT_FALSE(gsu2_disposition);

    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }

  EXPECT_TRUE(MainThreadHitTestInProgress());

  // The hit test reply arrives. Ensure we call ScrollBegin and unblock the
  // queue.
  {
    EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
        .WillOnce(Return(kImplThreadScrollState));

    // Additionally, the queue should be flushed by
    // ContinueScrollBeginAfterMainThreadHitTest so that the GSUs dispatched
    // earlier will now handled.
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _))
        .WillOnce(Return(DidScrollResult()));

    // Ensure we don't spurriously call ScrollEnd (because we think we're
    // already in a scroll from the first GSB).
    EXPECT_CALL(mock_input_handler_, ScrollEnd(_)).Times(0);

    ReturnedDisposition disposition;
    constexpr ElementId kHitTestResult(12345);
    ContinueScrollBeginAfterMainThreadHitTest(ScrollBegin(), kHitTestResult,
                                              &disposition);

    // The ScrollBegin should have been immediately re-injected and queue
    // flushed.
    EXPECT_FALSE(MainThreadHitTestInProgress());
    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *disposition);
    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *gsu1_disposition);
    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *gsu2_disposition);

    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }

  // Injecting a new GSU should cause queueing and dispatching as usual.
  {
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _))
        .WillOnce(Return(DidScrollResult()));

    ReturnedDisposition disposition;
    DispatchEvent(ScrollUpdate(), &disposition);
    EXPECT_FALSE(disposition);

    BeginFrame();
    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *disposition);

    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }

  // Finish the scroll.
  {
    EXPECT_CALL(mock_input_handler_, ScrollEnd(_)).Times(1);
    ReturnedDisposition disposition;
    DispatchEvent(ScrollEnd(), &disposition);
    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *disposition);
    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }

  EXPECT_FALSE(MainThreadHitTestInProgress());
}

// Test to ensure that a main thread hit test sets the correct flags on the
// re-injected GestureScrollBegin.
TEST_F(UnifiedScrollingInputHandlerProxyTest, MainThreadHitTestEvent) {
  // Inject a GSB that returns RequiresMainThreadHitTest.
  {
    // Ensure that by default we don't set a target. The
    // |is_main_thread_hit_tested| property should default to false.
    EXPECT_CALL(
        mock_input_handler_,
        ScrollBegin(
            AllOf(Property(&ScrollState::target_element_id, Eq(ElementId())),
                  Property(
                      &ScrollState::main_thread_hit_tested_reasons,
                      Eq(cc::MainThreadScrollingReason::kNotScrollingOnMain))),
            _))
        .WillOnce(Return(kRequiresMainThreadHitTestState));
    DispatchEvent(ScrollBegin());
    ASSERT_TRUE(MainThreadHitTestInProgress());
    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }

  // The hit test reply arrives. Ensure we call ScrollBegin with the ElementId
  // from the hit test and the main_thread
  {
    const ElementId kHitTestResult(12345);

    EXPECT_CALL(
        mock_input_handler_,
        ScrollBegin(
            AllOf(Property(&ScrollState::target_element_id, Eq(kHitTestResult)),
                  Property(&ScrollState::main_thread_hit_tested_reasons,
                           Eq(cc::MainThreadScrollingReason::
                                  kMainThreadScrollHitTestRegion))),
            _))
        .Times(1);

    ContinueScrollBeginAfterMainThreadHitTest(ScrollBegin(), kHitTestResult);
    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }
}

// Test to ensure that a main thread hit test counts the correct number of
// scrolls for metrics.
TEST_F(UnifiedScrollingInputHandlerProxyTest, MainThreadHitTestMetrics) {
  // Inject a GSB that returns RequiresMainThreadHitTest followed by a GSU and
  // a GSE.
  {
    EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
        .WillOnce(Return(kRequiresMainThreadHitTestState))
        .WillOnce(Return(kImplThreadScrollState));
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);
    EXPECT_CALL(mock_input_handler_, ScrollEnd(_)).Times(1);

    // The record begin/end should be called exactly once.
    EXPECT_CALL(mock_input_handler_, RecordScrollBegin(_, _)).Times(1);
    EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);

    DispatchEvent(ScrollBegin());
    EXPECT_TRUE(MainThreadHitTestInProgress());
    DispatchEvent(ScrollUpdate());
    DispatchEvent(ScrollEnd());

    // Hit test reply.
    constexpr ElementId kHitTestResult(12345);
    ContinueScrollBeginAfterMainThreadHitTest(ScrollBegin(), kHitTestResult);
    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }

  // Ensure we don't record either a begin or an end if the hit test fails.
  {
    EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
        .WillOnce(Return(kRequiresMainThreadHitTestState));
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(0);
    EXPECT_CALL(mock_input_handler_, ScrollEnd(_)).Times(0);

    EXPECT_CALL(mock_input_handler_, RecordScrollBegin(_, _)).Times(0);
    EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(0);

    DispatchEvent(ScrollBegin());
    EXPECT_TRUE(MainThreadHitTestInProgress());
    DispatchEvent(ScrollUpdate());
    DispatchEvent(ScrollEnd());

    // Hit test reply failed.
    constexpr ElementId kHitTestResult;
    ASSERT_FALSE(kHitTestResult);

    ContinueScrollBeginAfterMainThreadHitTest(ScrollBegin(), kHitTestResult);
    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }
}

// Test the case where a main thread hit test is in progress on the main thread
// and a GSE and new GSB arrive.
TEST_F(UnifiedScrollingInputHandlerProxyTest,
       ScrollEndAndBeginsDuringMainThreadHitTest) {
  ReturnedDisposition gsb1_disposition;
  ReturnedDisposition gsu1_disposition;
  ReturnedDisposition gse1_disposition;
  ReturnedDisposition gsb2_disposition;
  ReturnedDisposition gsu2_disposition;
  ReturnedDisposition gse2_disposition;

  // Inject a GSB that returns RequiresMainThreadHitTest followed by a GSU and
  // GSE that get queued.
  {
    EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
        .WillOnce(Return(kRequiresMainThreadHitTestState));
    DispatchEvent(ScrollBegin(), &gsb1_disposition);
    ASSERT_TRUE(MainThreadHitTestInProgress());
    ASSERT_EQ(InputHandlerProxy::REQUIRES_MAIN_THREAD_HIT_TEST,
              *gsb1_disposition);

    DispatchEvent(ScrollUpdate(), &gsu1_disposition);
    DispatchEvent(ScrollEnd(), &gse1_disposition);

    // The queue is blocked so none of the events should be processed.
    BeginFrame();

    ASSERT_FALSE(gsu1_disposition);
    ASSERT_FALSE(gse1_disposition);
  }

  // Inject another group of GSB, GSU, GSE. They should all be queued.
  {
    DispatchEvent(ScrollBegin(), &gsb2_disposition);
    DispatchEvent(ScrollUpdate(), &gsu2_disposition);
    DispatchEvent(ScrollEnd(), &gse2_disposition);

    // The queue is blocked so none of the events should be processed.
    BeginFrame();

    EXPECT_FALSE(gsb2_disposition);
    EXPECT_FALSE(gsu2_disposition);
    EXPECT_FALSE(gse2_disposition);
  }

  ASSERT_TRUE(MainThreadHitTestInProgress());

  // The hit test reply arrives. Ensure we call ScrollBegin and unblock the
  // queue.
  {
    EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
        .Times(2)
        .WillRepeatedly(Return(kImplThreadScrollState));
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _))
        .Times(2)
        .WillRepeatedly(Return(DidScrollResult()));
    EXPECT_CALL(mock_input_handler_, ScrollEnd(_)).Times(2);

    ReturnedDisposition disposition;
    constexpr ElementId kHitTestResult(12345);
    ContinueScrollBeginAfterMainThreadHitTest(ScrollBegin(), kHitTestResult,
                                              &disposition);

    // The ScrollBegin should have been immediately re-injected and queue
    // flushed.
    EXPECT_FALSE(MainThreadHitTestInProgress());
    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *disposition);
    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *gsu1_disposition);
    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *gse1_disposition);

    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *gsb2_disposition);
    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *gsu2_disposition);
    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *gse2_disposition);

    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }
}

// Test the case where a main thread hit test returns a null element_id. In
// this case we should reset the state and unblock the queue.
TEST_F(UnifiedScrollingInputHandlerProxyTest, MainThreadHitTestFailed) {
  ReturnedDisposition gsu1_disposition;

  // Inject a GSB that returns RequiresMainThreadHitTest.
  {
    EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
        .WillOnce(Return(kRequiresMainThreadHitTestState));
    DispatchEvent(ScrollBegin());
    DispatchEvent(ScrollUpdate(), &gsu1_disposition);
    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }

  // The hit test reply arrives with an invalid ElementId. We shouldn't call
  // ScrollBegin nor ScrollUpdate. Both should be dropped without reaching the
  // input handler.
  {
    EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _)).Times(0);
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(0);
    EXPECT_CALL(mock_input_handler_, ScrollEnd(_)).Times(0);

    constexpr ElementId kHitTestResult;
    ASSERT_FALSE(kHitTestResult);

    ReturnedDisposition gsb_disposition;
    ContinueScrollBeginAfterMainThreadHitTest(ScrollBegin(), kHitTestResult,
                                              &gsb_disposition);

    EXPECT_EQ(InputHandlerProxy::DROP_EVENT, *gsb_disposition);
    EXPECT_EQ(InputHandlerProxy::DROP_EVENT, *gsu1_disposition);
    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }

  // Send a new GSU, ensure it's dropped without queueing since there's no
  // scroll in progress.
  {
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(0);

    ReturnedDisposition disposition;
    DispatchEvent(ScrollUpdate(), &disposition);
    EXPECT_EQ(InputHandlerProxy::DROP_EVENT, *disposition);
    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }

  // Ensure there's no left-over bad state by sending a new GSB+GSU which
  // should be handled by the input handler immediately. A following GSU should
  // be queued and dispatched at BeginFrame.
  {
    EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
        .WillOnce(Return(kImplThreadScrollState));
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _))
        .WillOnce(Return(DidScrollResult()))
        .WillOnce(Return(DidScrollResult()));

    // Note: The first GSU after a GSB is dispatched immediately without
    // queueing.
    ReturnedDisposition disposition;
    DispatchEvent(ScrollBegin(), &disposition);
    DispatchEvent(ScrollUpdate());

    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *disposition);
    disposition = std::nullopt;

    DispatchEvent(ScrollUpdate(), &disposition);
    EXPECT_FALSE(disposition);

    BeginFrame();
    EXPECT_EQ(InputHandlerProxy::DID_HANDLE, *disposition);
    Mock::VerifyAndClearExpectations(&mock_input_handler_);
  }
}

TEST(SynchronousInputHandlerProxyTest, StartupShutdown) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing::StrictMock<cc::MockInputHandler> mock_input_handler;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler;
  InputHandlerProxy proxy(mock_input_handler, &mock_client);

  // When adding a SynchronousInputHandler, immediately request an
  // UpdateRootLayerStateForSynchronousInputHandler() call.
  EXPECT_CALL(mock_input_handler, RequestUpdateForSynchronousInputHandler())
      .Times(1);
  proxy.SetSynchronousInputHandler(&mock_synchronous_input_handler);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);

  EXPECT_CALL(mock_input_handler, RequestUpdateForSynchronousInputHandler())
      .Times(0);
  proxy.SetSynchronousInputHandler(nullptr);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);
}

TEST(SynchronousInputHandlerProxyTest, UpdateRootLayerState) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing::NiceMock<cc::MockInputHandler> mock_input_handler;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler;
  InputHandlerProxy proxy(mock_input_handler, &mock_client);

  proxy.SetSynchronousInputHandler(&mock_synchronous_input_handler);

  // When adding a SynchronousInputHandler, immediately request an
  // UpdateRootLayerStateForSynchronousInputHandler() call.
  EXPECT_CALL(mock_synchronous_input_handler,
              UpdateRootLayerState(gfx::PointF(1, 2), gfx::PointF(3, 4),
                                   gfx::SizeF(5, 6), 7, 8, 9))
      .Times(1);
  proxy.UpdateRootLayerStateForSynchronousInputHandler(
      gfx::PointF(1, 2), gfx::PointF(3, 4), gfx::SizeF(5, 6), 7, 8, 9);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);
}

TEST(SynchronousInputHandlerProxyTest, SetOffset) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing::NiceMock<cc::MockInputHandler> mock_input_handler;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler;
  InputHandlerProxy proxy(mock_input_handler, &mock_client);

  proxy.SetSynchronousInputHandler(&mock_synchronous_input_handler);

  EXPECT_CALL(mock_input_handler,
              SetSynchronousInputHandlerRootScrollOffset(gfx::PointF(5, 6)));
  proxy.SynchronouslySetRootScrollOffset(gfx::PointF(5, 6));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);
}

TEST_P(InputHandlerProxyEventQueueTest,
       MouseEventOnScrollbarInitiatesGestureScroll) {
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
      .Times(2)
      .WillRepeatedly(testing::Return(cc::ElementId()));

  // Test mousedown on the scrollbar. Expect to get GSB and GSU.
  cc::InputHandlerPointerResult pointer_down_result;
  pointer_down_result.type = cc::PointerResultType::kScrollbarScroll;
  pointer_down_result.scroll_delta = gfx::Vector2dF(0, 1);
  EXPECT_CALL(mock_input_handler_, HitTest(_))
      .WillOnce(testing::Return(pointer_down_result.type));
  EXPECT_CALL(mock_input_handler_, MouseDown(_, _))
      .WillOnce(testing::Return(pointer_down_result));
  HandleMouseEvent(WebInputEvent::Type::kMouseDown);
  EXPECT_EQ(2ul, event_queue().size());
  EXPECT_EQ(event_queue()[0]->event().GetType(),
            WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_TRUE(static_cast<const WebGestureEvent&>(event_queue()[0]->event())
                  .data.scroll_begin.synthetic);
  EXPECT_EQ(event_queue()[1]->event().GetType(),
            WebInputEvent::Type::kGestureScrollUpdate);
  cc::InputHandlerPointerResult pointer_up_result;
  pointer_up_result.type = cc::PointerResultType::kScrollbarScroll;
  EXPECT_CALL(mock_input_handler_, MouseUp(_))
      .WillOnce(testing::Return(pointer_up_result));
  // Test mouseup on the scrollbar. Expect to get GSE.
  HandleMouseEvent(WebInputEvent::Type::kMouseUp);
  EXPECT_EQ(3ul, event_queue().size());
  EXPECT_EQ(event_queue()[2]->event().GetType(),
            WebInputEvent::Type::kGestureScrollEnd);
}

TEST_P(InputHandlerProxyEventQueueTest, VSyncAlignedGestureScroll) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(1)
        .WillOnce(testing::Return(cc::ElementId()));
  }

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);

  // GestureScrollBegin will be processed immediately.
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[0]);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);

  // First GestureScrollUpdate will be queued.
  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -40);

  // The event queue size varies based on the RefactorCompositorThreadEventQueue
  // feature, which affects event coalescing and queueing, thus influencing
  // UpdateScrollPredictorInputMapping behavior.
  if (IsRefactorCompositorThreadEventQueueEnabled()) {
    // Second GestureScrollUpdate will be queued without coalescing yet.
    EXPECT_EQ(2ul, event_queue().size());
  } else {
    EXPECT_EQ(1ul, event_queue().size());
  }
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(0);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  // The event queue size varies based on the
  // RefactorCompositorThreadEventQueue feature, which affects event
  // coalescing and queueing, thus influencing
  // UpdateScrollPredictorInputMapping behavior.
  if (IsRefactorCompositorThreadEventQueueEnabled()) {
    EXPECT_EQ(3ul, event_queue().size());
  } else {
    EXPECT_EQ(2ul, event_queue().size());
  }
  EXPECT_EQ(1ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The two GSUs should be coalesced into one with delta_y = -60.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(
          testing::Property(&cc::ScrollState::delta_y, testing::Eq(60)), _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(2)
        .WillRepeatedly(testing::Return(cc::ElementId()));
  }

  // Dispatch all queued events.
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  GetInputHandlerProxy()->DispatchQueuedInputEventsHelper();
  EXPECT_EQ(0ul, event_queue().size());
  // Should run callbacks for every original event.
  EXPECT_EQ(4ul, event_disposition_recorder_.size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[1]);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[2]);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[3]);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(UNDEFINED_SANITIZER)
// Flaky under sanitizers and in other "slow" bot configs:
// https://crbug.com/1029250
#define MAYBE_VSyncAlignedGestureScrollPinchScroll \
  DISABLED_VSyncAlignedGestureScrollPinchScroll
#else
#define MAYBE_VSyncAlignedGestureScrollPinchScroll \
  VSyncAlignedGestureScrollPinchScroll
#endif

TEST_P(InputHandlerProxyEventQueueTest,
       MAYBE_VSyncAlignedGestureScrollPinchScroll) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  // Start scroll in the first frame.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(
          testing::Property(&cc::ScrollState::delta_y, testing::Eq(20)), _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(2)
        .WillRepeatedly(testing::Return(cc::ElementId()));
  }

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);

  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  GetInputHandlerProxy()->DispatchQueuedInputEventsHelper();

  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Continue scroll in the second frame, pinch, then start another scroll.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(
          testing::Property(&cc::ScrollState::delta_y, testing::Eq(30)), _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(
          testing::Property(&cc::ScrollState::delta_y, testing::Eq(75)), _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(2);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(8)
        .WillRepeatedly(testing::Return(cc::ElementId()));
  }
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin(_, _));
  // Two |GesturePinchUpdate| will be coalesced.
  EXPECT_CALL(mock_input_handler_,
              PinchGestureUpdate(0.7f, gfx::Point(13, 17)));
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point()));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(2);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 1.4f, 13, 17);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 0.5f, 13, 17);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -70);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -5);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  // The event queue size varies based on the RefactorCompositorThreadEventQueue
  // feature, which affects event coalescing and queueing, thus influencing
  // UpdateScrollPredictorInputMapping behavior.
  if (IsRefactorCompositorThreadEventQueueEnabled()) {
    EXPECT_EQ(10ul, event_queue().size());
  } else {
    EXPECT_EQ(8ul, event_queue().size());
  }
  EXPECT_EQ(2ul, event_disposition_recorder_.size());

  GetInputHandlerProxy()->DispatchQueuedInputEventsHelper();

  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(12ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_P(InputHandlerProxyEventQueueTest, VSyncAlignedQueueingTime) {
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);

  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(3)
        .WillRepeatedly(testing::Return(cc::ElementId()));
  }
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  tick_clock.Advance(base::Microseconds(10));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  tick_clock.Advance(base::Microseconds(40));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -40);
  tick_clock.Advance(base::Microseconds(20));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  tick_clock.Advance(base::Microseconds(10));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  // Dispatch all queued events.
  tick_clock.Advance(base::Microseconds(70));
  GetInputHandlerProxy()->DispatchQueuedInputEventsHelper();
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(5ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_P(InputHandlerProxyEventQueueTest, VSyncAlignedCoalesceScrollAndPinch) {
  cc::InputHandlerScrollResult scroll_result_did_scroll;
  scroll_result_did_scroll.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
      .Times(testing::AnyNumber());
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(1)
        .WillOnce(testing::Return(cc::ElementId()));
  }

  // Since we queue two complete gestures, we expect two of each of these calls.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .Times(2)
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(2);
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin(_, _)).Times(2);

  // Expect one coalesced scroll update for each gesture, with the correct
  // delta.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(
          testing::Property(&cc::ScrollState::delta_y, testing::Eq(35)), _))
      .WillOnce(testing::Return(scroll_result_did_scroll));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(
          testing::Property(&cc::ScrollState::delta_y, testing::Eq(85)), _))
      .WillOnce(testing::Return(scroll_result_did_scroll));

  // Expect one pinch update for each gesture.
  EXPECT_CALL(mock_input_handler_,
              PinchGestureUpdate(testing::FloatEq(2.0f), gfx::Point(13, 10)));
  EXPECT_CALL(mock_input_handler_,
              PinchGestureUpdate(testing::FloatEq(0.5f), gfx::Point(3, 30)));

  // Expect a pinch end for each gesture.
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point())).Times(2);

  // Expect a scroll end for each gesture.
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(2);
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(2);

  // GSUs and GPUs in one sequence should be coalesced into 1 GSU and 1 GPU.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -7);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 2.0f, 13, 10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -6);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 0.2f, 2, 20);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 10.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 0.25f, 3, 30);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  GetInputHandlerProxy()->DispatchQueuedInputEventsHelper();
}

TEST_P(InputHandlerProxyEventQueueTest, VSyncAlignedCoalesceTouchpadPinch) {
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin(_, _));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(1)
        .WillOnce(testing::Return(cc::ElementId()));
  }

  // Enqueue a GesturePinchBegin.
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGesturePinchBegin,
                                     WebGestureDevice::kTouchpad);
  // Enqueue a GesturePinchUpdate.
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGesturePinchUpdate,
                                     WebGestureDevice::kTouchpad, 1.1f, 10, 20);
  // The second update should coalesce with the first.
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGesturePinchUpdate,
                                     WebGestureDevice::kTouchpad, 1.1f, 10, 20);
  // The third update has a different anchor so it should not be coalesced.
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGesturePinchUpdate,
                                     WebGestureDevice::kTouchpad, 1.1f, 11, 21);
  // Enqueue a GesturePinchEnd.
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGesturePinchEnd,
                                     WebGestureDevice::kTouchpad);

  // The event queue size varies based on the RefactorCompositorThreadEventQueue
  // feature, which affects event coalescing and queueing, thus influencing
  // UpdateScrollPredictorInputMapping behavior.
  if (IsRefactorCompositorThreadEventQueueEnabled()) {
    // All the events are simply queued.
    EXPECT_EQ(4ul, event_queue().size());
  } else {
    EXPECT_EQ(3ul, event_queue().size());
  }
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(1.21f, _));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(1.1f, _));
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(_));

  GetInputHandlerProxy()->DispatchQueuedInputEventsHelper();
}

TEST_P(InputHandlerProxyEventQueueTest, OriginalEventsTracing) {
  base::test::TracingEnvironment tracing_environment;
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillRepeatedly(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(2);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
      .Times(::testing::AtLeast(1));
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(9)
        .WillRepeatedly(testing::Return(cc::ElementId()));
  }
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true))
      .Times(::testing::AtLeast(1));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(2);

  EXPECT_CALL(mock_input_handler_, PinchGestureBegin(_, _));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(_, _));
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(_));

  trace_analyzer::Start("input");
  // Simulate scroll.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -40);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  // Simulate scroll and pinch.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 10.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 2.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  // Advance the clock and deliver another frame to process the second gesture.
  tick_clock.Advance(base::Milliseconds(8));
  DeliverInputForBeginFrame(tick_clock.NowTicks());
  EXPECT_TRUE(event_queue().empty());

  // Retrieve tracing data.
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector begin_events;
  trace_analyzer::Query begin_query = trace_analyzer::Query::EventPhaseIs(
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN);
  analyzer->FindEvents(begin_query, &begin_events);

  trace_analyzer::TraceEventVector end_events;
  analyzer->FindEvents(
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_NESTABLE_ASYNC_END),
      &end_events);

  // The number of trace events depends on whether the
  // RefactorCompositorThreadEventQueue feature is enabled, as it changes how
  // events are processed and traced, impacting the behavior of
  // UpdateScrollPredictorInputMapping.
  if (IsRefactorCompositorThreadEventQueueEnabled()) {
    EXPECT_EQ(12ul, begin_events.size());
    EXPECT_EQ(12ul, end_events.size());
  } else {
    EXPECT_EQ(7ul, begin_events.size());
    EXPECT_EQ(7ul, end_events.size());
  }

  // Filter for only the events that were dispatched.
  trace_analyzer::TraceEventVector dispatched_events;
  for (const auto* event : begin_events) {
    std::string result;
    if (event->GetArgAsString("result", &result) && result == "dispatched") {
      dispatched_events.push_back(event);
    }
  }

  // We expect 7 events to be dispatched after coalescing.
  ASSERT_EQ(7ul, dispatched_events.size());

  // First scroll sequence: GSU (coalesced), GSE
  EXPECT_EQ("kGestureScrollUpdate",
            dispatched_events[0]->GetKnownArgAsString("type"));
  EXPECT_EQ(3, dispatched_events[0]->GetKnownArgAsInt("coalesced_count"));
  EXPECT_EQ("kGestureScrollEnd",
            dispatched_events[1]->GetKnownArgAsString("type"));

  // Second scroll and pinch sequence.
  EXPECT_EQ("{kGestureScrollBegin, kGestureTypeFirst}",
            dispatched_events[2]->GetKnownArgAsString("type"));
  EXPECT_EQ("{kGesturePinchBegin, kGesturePinchTypeFirst}",
            dispatched_events[3]->GetKnownArgAsString("type"));
  // Original scroll and pinch updates will be stored in the coalesced
  // PinchUpdate of the <ScrollUpdate, PinchUpdate> pair.
  // The ScrollUpdate of the pair doesn't carry original events and won't be
  // traced.
  EXPECT_EQ("{kGesturePinchUpdate, kGesturePinchTypeLast}",
            dispatched_events[4]->GetKnownArgAsString("type"));
  EXPECT_EQ(4, dispatched_events[4]->GetKnownArgAsInt("coalesced_count"));
  EXPECT_EQ("kGesturePinchEnd",
            dispatched_events[5]->GetKnownArgAsString("type"));
  EXPECT_EQ("kGestureScrollEnd",
            dispatched_events[6]->GetKnownArgAsString("type"));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_P(InputHandlerProxyEventQueueTest, TouchpadGestureScrollEndFlushQueue) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillRepeatedly(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(2);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true))
      .Times(::testing::AtLeast(1));
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(2)
        .WillRepeatedly(testing::Return(cc::ElementId()));
  }

  // Simulate scroll.
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollBegin,
                                     WebGestureDevice::kTouchpad);
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollUpdate,
                                     WebGestureDevice::kTouchpad, -20);

  // Both GSB and the first GSU will be dispatched immediately since the first
  // GSU has blocking wheel event source.
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());

  // The rest of the GSU events will get queued since they have non-blocking
  // wheel event source.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
      .Times(::testing::AtLeast(1));
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(4)
        .WillRepeatedly(testing::Return(cc::ElementId()));
  }
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollUpdate,
                                     WebGestureDevice::kTouchpad, -20);
  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());

  // Touchpad GSE will flush the queue.
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollEnd,
                                     WebGestureDevice::kTouchpad);

  EXPECT_EQ(0ul, event_queue().size());
  // GSB, GSU(with blocking wheel source), GSU(with non-blocking wheel
  // source), and GSE are the sent events.
  EXPECT_EQ(4ul, event_disposition_recorder_.size());

  EXPECT_FALSE(
      input_handler_proxy_->gesture_scroll_on_impl_thread_for_testing());

  // Starting a new scroll sequence should have the same behavior (namely that
  // the first scroll update is not queued but immediately dispatched).
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollBegin,
                                     WebGestureDevice::kTouchpad);
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollUpdate,
                                     WebGestureDevice::kTouchpad, -20);

  // Both GSB and the first GSU must be dispatched immediately since the first
  // GSU has blocking wheel event source.
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(6ul, event_disposition_recorder_.size());
}

TEST_P(InputHandlerProxyEventQueueTest, CoalescedLatencyInfo) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(3)
        .WillRepeatedly(testing::Return(cc::ElementId()));
  }
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -40);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);
  GetInputHandlerProxy()->DispatchQueuedInputEventsHelper();

  EXPECT_EQ(0ul, event_queue().size());
  // Should run callbacks for every original events.
  EXPECT_EQ(5ul, event_disposition_recorder_.size());
  EXPECT_EQ(5ul, latency_info_recorder_.size());
  EXPECT_EQ(false, latency_info_recorder_[1].coalesced());
  // Coalesced events should have latency set to coalesced.
  EXPECT_EQ(true, latency_info_recorder_[2].coalesced());
  EXPECT_EQ(true, latency_info_recorder_[3].coalesced());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_P(InputHandlerProxyEventQueueTest, ScrollPredictorTest) {
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);

  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(2);
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(2)
        .WillRepeatedly(testing::Return(cc::ElementId()));
  }
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));

  // No prediction when start with a GSB.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  // Advance the clock and explicitly provide the frame time to ensure the
  // event is processed in this frame, respecting the new `sample_time` logic.
  tick_clock.Advance(base::Milliseconds(8));
  DeliverInputForBeginFrame(tick_clock.NowTicks());
  EXPECT_FALSE(GestureScrollEventPredictionAvailable());

  // Test predictor returns last GSU delta.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  tick_clock.Advance(base::Milliseconds(8));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -15);
  // Advance the clock and deliver the frame with an explicit time.
  tick_clock.Advance(base::Milliseconds(8));
  DeliverInputForBeginFrame(tick_clock.NowTicks());
  auto result = GestureScrollEventPredictionAvailable();
  EXPECT_TRUE(result);
  EXPECT_NE(0, result->pos.y());
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Predictor has been reset after a new GSB.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(2)
        .WillRepeatedly(testing::Return(cc::ElementId()));
  }
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(_)).Times(1);
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  tick_clock.Advance(base::Milliseconds(8));
  DeliverInputForBeginFrame(tick_clock.NowTicks());
  EXPECT_FALSE(GestureScrollEventPredictionAvailable());
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

// Test deliver input w/o prediction enabled.
TEST_P(InputHandlerProxyEventQueueTest, DeliverInputWithHighLatencyMode) {
  SetScrollPredictionEnabled(false);

  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(2);
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_))
        .Times(3)
        .WillRepeatedly(testing::Return(cc::ElementId()));
  }
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  DeliverInputForBeginFrame();
  // 3 queued event be delivered.
  EXPECT_EQ(3ul, event_disposition_recorder_.size());
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_.back());

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  DeliverInputForHighLatencyMode();
  // 2 queued event be delivered.
  EXPECT_EQ(5ul, event_disposition_recorder_.size());
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_.back());

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_P(InputHandlerProxyEventQueueTest, KeyEventAttribution) {
  WebKeyboardEvent key(WebInputEvent::Type::kKeyDown,
                       WebInputEvent::kNoModifiers,
                       WebInputEvent::GetStaticTimeStampForTests());

  EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_)).Times(0);

  WebInputEventAttribution attribution =
      input_handler_proxy_->PerformEventAttribution(key);
  EXPECT_EQ(attribution.type(), WebInputEventAttribution::kFocusedFrame);
  EXPECT_EQ(attribution.target_frame_id(), cc::ElementId());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_P(InputHandlerProxyEventQueueTest, MouseEventAttribution) {
  WebMouseEvent mouse_down(WebInputEvent::Type::kMouseDown,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests());

  EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(gfx::PointF(0, 0)))
      .Times(1)
      .WillOnce(testing::Return(cc::ElementId(0xDEADBEEF)));

  WebInputEventAttribution attribution =
      input_handler_proxy_->PerformEventAttribution(mouse_down);
  EXPECT_EQ(attribution.type(), WebInputEventAttribution::kTargetedFrame);
  EXPECT_EQ(attribution.target_frame_id(), cc::ElementId(0xDEADBEEF));
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_P(InputHandlerProxyEventQueueTest, MouseWheelEventAttribution) {
  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests());

  EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(gfx::PointF(0, 0)))
      .Times(1)
      .WillOnce(testing::Return(cc::ElementId(0xDEADBEEF)));

  WebInputEventAttribution attribution =
      input_handler_proxy_->PerformEventAttribution(wheel);
  EXPECT_EQ(attribution.type(), WebInputEventAttribution::kTargetedFrame);
  EXPECT_EQ(attribution.target_frame_id(), cc::ElementId(0xDEADBEEF));
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

// Verify that the first point in a touch event is used for performing event
// attribution.
TEST_P(InputHandlerProxyEventQueueTest, TouchEventAttribution) {
  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  touch.touches[1] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  touch.touches[2] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, -10, 10);

  EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(gfx::PointF(0, 0)))
      .Times(1)
      .WillOnce(testing::Return(cc::ElementId(0xDEADBEEF)));

  WebInputEventAttribution attribution =
      input_handler_proxy_->PerformEventAttribution(touch);
  EXPECT_EQ(attribution.type(), WebInputEventAttribution::kTargetedFrame);
  EXPECT_EQ(attribution.target_frame_id(), cc::ElementId(0xDEADBEEF));
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_P(InputHandlerProxyEventQueueTest, GestureEventAttribution) {
  WebGestureEvent gesture(WebInputEvent::Type::kGestureTap,
                          WebInputEvent::kNoModifiers,
                          WebInputEvent::GetStaticTimeStampForTests());
  gesture.SetPositionInWidget(gfx::PointF(10, 10));

  EXPECT_CALL(mock_input_handler_,
              FindFrameElementIdAtPoint(gfx::PointF(10, 10)))
      .Times(1)
      .WillOnce(testing::Return(cc::ElementId(0xDEADBEEF)));
  WebInputEventAttribution attribution =
      input_handler_proxy_->PerformEventAttribution(gesture);
  EXPECT_EQ(attribution.type(), WebInputEventAttribution::kTargetedFrame);
  EXPECT_EQ(attribution.target_frame_id(), cc::ElementId(0xDEADBEEF));
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

// Tests that when we are only dispatching events until the deadline, that input
// arriving after the deadline is enqueued. As well any MISSED BeginFrames
// arriving after the deadline do not dispatch any enqueued input events.
TEST_P(InputHandlerProxyEventQueueTest, QueueInputForLateBeginFrameArgs) {
  // This test verifies the legacy deadline-based dispatch
  // mode, which is superseded by the kUpdateScrollPredictorInputMapping
  // feature. It should only run when the new feature is disabled.
  if (IsUpdateScrollPredictorInputMappingEnabled()) {
    return;
  }

  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);
  input_handler_proxy_->SetScrollEventDispatchMode(
      cc::InputHandlerClient::ScrollEventDispatchMode::
          kDispatchScrollEventsUntilDeadline,
      0.333);

  // ScrollBegin should idenfity the target element, and the event should be
  // processed immediately without being queued.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(ui::ScrollInputType::kTouchscreen,
                        cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_)).Times(1);
  }
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollBegin,
                                     WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // When a BeginFrame is received, if the queue is empty we should dispatch
  // any incoming scroll event immediately. As long as it is before the
  // deadline.
  DeliverInputForBeginFrame(tick_clock.NowTicks());
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_)).Times(1);
  }
  EXPECT_CALL(mock_input_handler_, ScrollUpdate).Times(1);
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollUpdate,
                                     WebGestureDevice::kTouchscreen, -20);
  EXPECT_EQ(0ul, event_queue().size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // When a BeginFrame is received, and the input arrives after the deadline,
  // the event should be enqueued. We should signal that we still require
  // BeginFrames by calling SetNeedsAnimateInput.
  constexpr base::TimeDelta interval = base::Milliseconds(16);
  tick_clock.Advance(interval);
  DeliverInputForBeginFrame(tick_clock.NowTicks());
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollUpdate).Times(0);
  constexpr base::TimeDelta after_deadline = interval * 0.4f;
  tick_clock.Advance(after_deadline);
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollUpdate,
                                     WebGestureDevice::kTouchscreen, -20);
  EXPECT_EQ(1ul, event_queue().size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // When a BeginFrame is received, that is of viz::BeginFrameArgs::MISSED, and
  // arrives after the deadline. We should not process the queue.
  tick_clock.Advance(interval);
  const base::TimeTicks missed_frame_time = tick_clock.NowTicks();
  tick_clock.Advance(after_deadline);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  DeliverInputForBeginFrame(missed_frame_time, viz::BeginFrameArgs::MISSED);
  EXPECT_EQ(1ul, event_queue().size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // When a regular BeginFrame arrives we resume processing the queue.
  if (!::features::IsCCSlimmingEnabled()) {
    EXPECT_CALL(mock_input_handler_, FindFrameElementIdAtPoint(_)).Times(1);
  }
  EXPECT_CALL(mock_input_handler_, ScrollUpdate);
  DeliverInputForBeginFrame(tick_clock.NowTicks());
  EXPECT_EQ(0ul, event_queue().size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

class InputHandlerProxyMainThreadScrollingReasonTest
    : public InputHandlerProxyTest {
 public:
  enum TestEventType {
    kTouch,
    kMouseWheel,
  };

  InputHandlerProxyMainThreadScrollingReasonTest() : InputHandlerProxyTest() {}
  ~InputHandlerProxyMainThreadScrollingReasonTest() = default;

  void SetupEvents(TestEventType type) {
    touch_start_ = WebTouchEvent(WebInputEvent::Type::kTouchStart,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());
    touch_end_ = WebTouchEvent(WebInputEvent::Type::kTouchEnd,
                               WebInputEvent::kNoModifiers,
                               WebInputEvent::GetStaticTimeStampForTests());
    wheel_event_ = WebMouseWheelEvent(
        WebInputEvent::Type::kMouseWheel, WebInputEvent::kControlKey,
        WebInputEvent::GetStaticTimeStampForTests());
    gesture_scroll_begin_ = WebGestureEvent(
        WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        type == TestEventType::kMouseWheel ? WebGestureDevice::kTouchpad
                                           : WebGestureDevice::kTouchscreen);
    gesture_scroll_end_ = WebGestureEvent(
        WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        type == TestEventType::kMouseWheel ? WebGestureDevice::kTouchpad
                                           : WebGestureDevice::kTouchscreen);
    touch_start_.touches_length = 1;
    touch_start_.touch_start_or_first_touch_move = true;
    touch_start_.touches[0] =
        CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);

    touch_end_.touches_length = 1;
  }

  base::HistogramBase::Sample32 GetBucketSample(uint32_t reason) {
    uint32_t bucket = 0;
    while (reason >>= 1)
      bucket++;
    DCHECK_NE(bucket, 0u);
    return bucket;
  }

 protected:
  WebTouchEvent touch_start_;
  WebTouchEvent touch_end_;
  WebMouseWheelEvent wheel_event_;
  WebGestureEvent gesture_scroll_begin_;
  WebGestureEvent gesture_scroll_end_;
};

// Bucket 0: non-main-thread scrolls
// Bucket 1: main-thread scrolls for any reason.
#define EXPECT_NON_MAIN_THREAD_GESTURE_SCROLL_SAMPLE()         \
  EXPECT_THAT(histogram_tester().GetAllSamples(                \
                  "Renderer4.MainThreadGestureScrollReason2"), \
              testing::ElementsAre(base::Bucket(0, 1)))
#define EXPECT_NON_MAIN_THREAD_WHEEL_SCROLL_SAMPLE()         \
  EXPECT_THAT(histogram_tester().GetAllSamples(              \
                  "Renderer4.MainThreadWheelScrollReason2"), \
              testing::ElementsAre(base::Bucket(0, 1)))
#define EXPECT_MAIN_THREAD_GESTURE_SCROLL_SAMPLE(reason)       \
  EXPECT_THAT(histogram_tester().GetAllSamples(                \
                  "Renderer4.MainThreadGestureScrollReason2"), \
              testing::ElementsAre(base::Bucket(1, 1),         \
                                   base::Bucket(GetBucketSample(reason), 1)))
#define EXPECT_MAIN_THREAD_WHEEL_SCROLL_SAMPLE(reason)       \
  EXPECT_THAT(histogram_tester().GetAllSamples(              \
                  "Renderer4.MainThreadWheelScrollReason2"), \
              testing::ElementsAre(base::Bucket(1, 1),       \
                                   base::Bucket(GetBucketSample(reason), 1)))
#define EXPECT_MAIN_THREAD_WHEEL_SCROLL_SAMPLE_2(reason1, reason2)            \
  EXPECT_THAT(histogram_tester().GetAllSamples(                               \
                  "Renderer4.MainThreadWheelScrollReason2"),                  \
              testing::ElementsAre(base::Bucket(1, 1),                        \
                                   base::Bucket(GetBucketSample(reason1), 1), \
                                   base::Bucket(GetBucketSample(reason2), 1)))

// Tests GetBucketSample() returns the corresponding values defined in
// enums.xml, to ensure correctness of the tests using the function.
TEST_P(InputHandlerProxyMainThreadScrollingReasonTest, ReasonToBucket) {
  EXPECT_EQ(2, GetBucketSample(kSampleMainThreadScrollingReason));
  EXPECT_EQ(14, GetBucketSample(
                    cc::MainThreadScrollingReason::kTouchEventHandlerRegion));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       GestureScrollNotScrollOnMain) {
  // Touch start with passive event listener.
  SetupEvents(TestEventType::kTouch);

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Rect::x, testing::Gt(0)), _))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::kNoHandler));
  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));
  EXPECT_CALL(mock_client_, SetAllowedTouchAction(_))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock_input_handler_, HitTest(_))
      .WillOnce(testing::Return(cc::PointerResultType::kUnhandled));
  EXPECT_CALL(mock_input_handler_, SetIsHandlingTouchSequence(true));

  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), touch_start_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(
      expected_disposition_,
      HandleInputEventAndFlushEventQueue(
          mock_input_handler_, input_handler_.get(), gesture_scroll_begin_));

  EXPECT_NON_MAIN_THREAD_GESTURE_SCROLL_SAMPLE();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(
      expected_disposition_,
      HandleInputEventAndFlushEventQueue(
          mock_input_handler_, input_handler_.get(), gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       GestureScrollTouchEventHandlerRegion) {
  // The touch event hits a touch event handler that is acked from the
  // compositor thread.
  SetupEvents(TestEventType::kTouch);

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Rect::x, testing::Gt(0)), _))
      .WillOnce(
          testing::Return(cc::InputHandler::TouchStartOrMoveEventListenerType::
                              kHandlerOnScrollingLayer));
  EXPECT_CALL(mock_client_, SetAllowedTouchAction(_))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock_input_handler_, HitTest(_))
      .WillOnce(testing::Return(cc::PointerResultType::kUnhandled));
  EXPECT_CALL(mock_input_handler_, SetIsHandlingTouchSequence(true));

  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), touch_start_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(
      expected_disposition_,
      HandleInputEventAndFlushEventQueue(
          mock_input_handler_, input_handler_.get(), gesture_scroll_begin_));

  EXPECT_NON_MAIN_THREAD_GESTURE_SCROLL_SAMPLE();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(
      expected_disposition_,
      HandleInputEventAndFlushEventQueue(
          mock_input_handler_, input_handler_.get(), gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       ImplHandled_MainThreadHitTest) {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.data.scroll_begin.scrollable_area_element_id = 1;
  gesture_.data.scroll_begin.main_thread_hit_tested_reasons =
      cc::MainThreadScrollingReason::kMainThreadScrollHitTestRegion;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(
          _, cc::ScrollBeginThreadState::kScrollingOnCompositorBlockedOnMain))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_MAIN_THREAD_WHEEL_SCROLL_SAMPLE(
      cc::MainThreadScrollingReason::kMainThreadScrollHitTestRegion);
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       ImplHandled_MainThreadRepaint) {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  cc::InputHandler::ScrollStatus scroll_status = kImplThreadScrollState;
  scroll_status.main_thread_repaint_reasons =
      cc::MainThreadScrollingReason::kPreferNonCompositedScrolling;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(scroll_status));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_MAIN_THREAD_WHEEL_SCROLL_SAMPLE(
      cc::MainThreadScrollingReason::kPreferNonCompositedScrolling);
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest, WheelScrollHistogram) {
  // Firstly check if input handler can correctly record main thread scrolling
  // reasons.
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);
  input_handler_->RecordScrollBeginForTest(
      WebGestureDevice::kTouchpad,
      kSampleMainThreadScrollingReason |
          cc::MainThreadScrollingReason::kPreferNonCompositedScrolling);

  EXPECT_MAIN_THREAD_WHEEL_SCROLL_SAMPLE_2(
      kSampleMainThreadScrollingReason,
      cc::MainThreadScrollingReason::kPreferNonCompositedScrolling);
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       WheelScrollNotScrollingOnMain) {
  // Even if a scroller is composited, we still need to record its main thread
  // scrolling reason if it is blocked on a main thread event handler.
  SetupEvents(TestEventType::kMouseWheel);

  // We can scroll on impl for an wheel event with passive event listener.
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_, HandleInputEventWithLatencyInfo(
                                       input_handler_.get(), wheel_event_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_begin_));

  EXPECT_NON_MAIN_THREAD_WHEEL_SCROLL_SAMPLE();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       WheelScrollWheelEventHandlerRegion) {
  // Wheel event with blocking event listener. If there is a wheel event handler
  // at the point, we do not need to call GetEventListenerProperties since it
  // indicates kBlocking.
  SetupEvents(TestEventType::kMouseWheel);
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(true));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_, HandleInputEventWithLatencyInfo(
                                       input_handler_.get(), wheel_event_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(
          _, cc::ScrollBeginThreadState::kScrollingOnCompositorBlockedOnMain))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_begin_));

  EXPECT_MAIN_THREAD_WHEEL_SCROLL_SAMPLE(
      cc::MainThreadScrollingReason::kWheelEventHandlerRegion);

  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_end_));
}

class InputHandlerProxyTouchScrollbarTest : public InputHandlerProxyTest {
 public:
  void SetupEvents() {
    touch_start_ = WebTouchEvent(WebInputEvent::Type::kTouchStart,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());
    touch_end_ = WebTouchEvent(WebInputEvent::Type::kTouchEnd,
                               WebInputEvent::kNoModifiers,
                               WebInputEvent::GetStaticTimeStampForTests());
    touch_start_.touches_length = 1;
    touch_start_.touch_start_or_first_touch_move = true;
    touch_start_.touches[0] =
        CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);

    touch_end_.touches_length = 1;
  }

 protected:
  WebTouchEvent touch_start_;
  WebTouchEvent touch_end_;
};

TEST_P(InputHandlerProxyTouchScrollbarTest,
       TouchOnScrollbarIsHandledByCompositorThread) {
  // The touch event hits a touch event handler that is acked from the
  // compositor thread.
  SetupEvents();
  cc::InputHandlerPointerResult pointer_down_result;
  pointer_down_result.type = cc::PointerResultType::kScrollbarScroll;
  pointer_down_result.scroll_delta = gfx::Vector2dF(0, 1);
  cc::InputHandlerPointerResult pointer_up_result;
  pointer_up_result.type = cc::PointerResultType::kScrollbarScroll;

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Rect::x, testing::Eq(10)), _))
      .WillOnce([](const gfx::Rect&, cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kAuto;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::kNoHandler;
      });
  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));

  EXPECT_CALL(mock_client_, SetAllowedTouchAction(_))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock_input_handler_, HitTest(_))
      .WillOnce(testing::Return(pointer_down_result.type));
  EXPECT_CALL(mock_input_handler_, MouseDown(_, _))
      .WillOnce(testing::Return(pointer_down_result));
  EXPECT_CALL(mock_input_handler_, SetIsHandlingTouchSequence(true));
  cc::InputHandlerScrollResult scroll_result_did_scroll;
  scroll_result_did_scroll.did_scroll = true;
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;

  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(ui::ScrollInputType::kScrollbar,
                        cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll));
  EXPECT_CALL(mock_input_handler_, MouseUp(_))
      .WillOnce(testing::Return(pointer_up_result));

  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), touch_start_));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), touch_end_));
}

const auto kTestCombinations = testing::Combine(
    testing::Values(ScrollerType::kRoot, ScrollerType::kChild),
    testing::Values(HandlerType::kNormal, HandlerType::kSynchronous));

const auto kSuffixGenerator =
    [](const testing::TestParamInfo<std::tuple<ScrollerType, HandlerType>>&
           info) {
      std::string name = std::get<1>(info.param) == HandlerType::kSynchronous
                             ? "Synchronous"
                             : "";
      name += std::get<0>(info.param) == ScrollerType::kRoot ? "Root" : "Child";
      return name;
    };

INSTANTIATE_TEST_SUITE_P(All,
                         InputHandlerProxyTest,
                         kTestCombinations,
                         kSuffixGenerator);

TEST_P(InputHandlerProxyEventQueueTest, FutureEventDispatch) {
  bool update_scroll_predictor = IsUpdateScrollPredictorInputMappingEnabled();
  bool refactor_queue = IsRefactorCompositorThreadEventQueueEnabled();

  // The kUpdateScrollPredictorInputMapping feature depends on
  // kRefactorCompositorThreadEventQueue. So, the case where
  // update_scroll_predictor is true and refactor_queue is false is invalid.
  if (update_scroll_predictor && !refactor_queue) {
    return;
  }

  // Setup
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);
  constexpr base::TimeDelta kInterval = base::Milliseconds(16);

  // 1. Start a scroll
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // 2. Deliver a BeginFrame to set up the timing
  DeliverInputForBeginFrame(tick_clock.NowTicks());

  // 3. Queue an event "in the future" (e.g., 50ms from now)
  base::TimeTicks future_event_time =
      tick_clock.NowTicks() + base::Milliseconds(50);
  auto future_event = CreateGestureScrollPinch(
      WebInputEvent::Type::kGestureScrollUpdate, WebGestureDevice::kTouchscreen,
      future_event_time, -10);

  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  InjectInputEvent(std::move(future_event));
  EXPECT_EQ(1ul, event_queue().size());
  Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // 4. Advance time to the next VSync
  tick_clock.Advance(kInterval);

  // 5. Deliver the FIRST BeginFrame after queuing.
  if (!update_scroll_predictor) {
    // Without kUpdateScrollPredictorInputMapping, future timestamp doesn't
    // cause deferral.
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);
    DeliverInputForBeginFrame(tick_clock.NowTicks());
    EXPECT_EQ(0ul, event_queue().size());
  } else {
    // With kUpdateScrollPredictorInputMapping, the future event is deferred.
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(0);
    DeliverInputForBeginFrame(tick_clock.NowTicks());
    EXPECT_EQ(1ul, event_queue().size());
  }
  Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // 6. Advance time to the SECOND VSync
  tick_clock.Advance(kInterval);

  // 7. Deliver the SECOND BeginFrame.
  if (!update_scroll_predictor) {
    // Already dispatched.
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(0);
  } else {
    // Both features on: Dispatched from backlog.
    EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);
  }
  DeliverInputForBeginFrame(tick_clock.NowTicks());
  EXPECT_EQ(0ul, event_queue().size());
  Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    InputHandlerProxyEventQueueTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      return base::StringPrintf(
          "%s_%s",
          std::get<0>(info.param)
              ? "UpdateScrollPredictorInputMapping_Enabled"
              : "UpdateScrollPredictorInputMapping_Disabled",
          std::get<1>(info.param)
              ? "RefactorCompositorThreadEventQueue_Enabled"
              : "RefactorCompositorThreadEventQueue_Disabled");
    });

INSTANTIATE_TEST_SUITE_P(All,
                         InputHandlerProxyMainThreadScrollingReasonTest,
                         kTestCombinations,
                         kSuffixGenerator);

INSTANTIATE_TEST_SUITE_P(All,
                         InputHandlerProxyTouchScrollbarTest,
                         kTestCombinations,
                         kSuffixGenerator);

// Ideally, we would mock `GetScopedEventMetricsMonitor` on
// `cc::MockInputHandler` instead. However, that would cause more than 100 tests
// in this file would fail because they use
// `testing::StrictMock<MockInputHandler>`.
class MockInputHandlerWithEventMetricsManager : public cc::MockInputHandler {
 public:
  std::unique_ptr<cc::EventsMetricsManager::ScopedMonitor>
  GetScopedEventMetricsMonitor(
      cc::EventsMetricsManager::ScopedMonitor::DoneCallback done_callback)
      override {
    return events_metrics_manager.GetScopedMonitor(std::move(done_callback));
  }

  cc::EventsMetricsManager events_metrics_manager;
};

class InputHandlerProxyEventMetricsTest : public testing::Test {
 public:
  InputHandlerProxyEventMetricsTest()
      : input_handler_proxy_(mock_input_handler_, &mock_client_) {
    tick_clock_.SetNowTicks(base::TimeTicks::Now());
    input_handler_proxy_.SetTickClockForTesting(&tick_clock_);
  }

  ~InputHandlerProxyEventMetricsTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockInputHandlerWithEventMetricsManager mock_input_handler_;
  MockInputHandlerProxyClient mock_client_;
  base::SimpleTestTickClock tick_clock_;
  TestInputHandlerProxy input_handler_proxy_;
};

struct InputHandlerProxyScrollEventMetricsTestCase {
  WebGestureEvent::InertialPhaseState inertial_phase;
  bool is_inertial;
  cc::EventMetrics::EventType expected_event_type;
  std::string test_name;
};

class InputHandlerProxyScrollEventMetricsTest
    : public InputHandlerProxyEventMetricsTest,
      public testing::WithParamInterface<
          InputHandlerProxyScrollEventMetricsTestCase> {};

TEST_P(InputHandlerProxyScrollEventMetricsTest, SavesScrollEndMetrics) {
  constexpr base::TimeDelta kInterval = base::Milliseconds(16);
  const InputHandlerProxyScrollEventMetricsTestCase& param = GetParam();

  // Inject a gesture scroll begin first so that `InputHandlerProxy` would set
  // `InputHandlerProxy::currently_active_gesture_device()` and consequently
  // queue inputs instead of dispatching them immediately.
  input_handler_proxy_.HandleInputEventWithLatencyInfo(
      std::make_unique<WebCoalescedInputEvent>(
          std::make_unique<WebGestureEvent>(
              WebInputEvent::Type::kGestureScrollBegin,
              WebInputEvent::kNoModifiers,
              WebInputEvent::GetStaticTimeStampForTests(),
              WebGestureDevice::kTouchscreen),
          ui::LatencyInfo()),
      /* metrics= */ nullptr, /* callback= */ base::DoNothing());

  // Send begin frame 1.
  base::TimeTicks frame1_ts = tick_clock_.NowTicks();
  input_handler_proxy_.DeliverInputForBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, /* source_id= */ 0, /* sequence_number= */ 41,
      frame1_ts, frame1_ts + kInterval, kInterval,
      viz::BeginFrameArgs::NORMAL));

  // Inject the gesture scroll update. `InputHandlerProxy` will enqueue it.
  tick_clock_.Advance(base::Microseconds(10));
  base::TimeTicks timestamp = tick_clock_.NowTicks();
  tick_clock_.Advance(base::Microseconds(10));
  base::TimeTicks arrived_in_browser_main_timestamp = tick_clock_.NowTicks();
  tick_clock_.Advance(base::Microseconds(10));
  std::unique_ptr<WebGestureEvent> gesture_event =
      std::make_unique<WebGestureEvent>(
          WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
          WebInputEvent::GetStaticTimeStampForTests(),
          WebGestureDevice::kTouchscreen);
  gesture_event->data.scroll_end.inertial_phase = param.inertial_phase;
  std::unique_ptr<cc::EventMetrics> metrics =
      cc::ScrollEventMetrics::CreateForTesting(
          ui::EventType::kGestureScrollEnd, ui::ScrollInputType::kTouchscreen,
          param.is_inertial, timestamp, arrived_in_browser_main_timestamp,
          &tick_clock_);
  input_handler_proxy_.HandleInputEventWithLatencyInfo(
      std::make_unique<WebCoalescedInputEvent>(std::move(gesture_event),
                                               ui::LatencyInfo()),
      std::move(metrics),
      /* callback= */ base::DoNothing());

  // Send begin frame 2, which dispatches the gesture scroll update.
  base::TimeTicks frame2_ts = frame1_ts + kInterval;
  tick_clock_.SetNowTicks(frame2_ts);
  input_handler_proxy_.DeliverInputForBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, /* source_id= */ 0, /* sequence_number= */ 42,
      frame1_ts, frame1_ts + kInterval, kInterval,
      viz::BeginFrameArgs::NORMAL));

  cc::EventMetrics::List saved_metrics =
      mock_input_handler_.events_metrics_manager.TakeSavedEventsMetrics();
  EXPECT_THAT(saved_metrics, SizeIs(1u));
  EXPECT_EQ(saved_metrics[0]->type(), param.expected_event_type);
  EXPECT_EQ(
      saved_metrics[0]->AsScroll()->begin_frame_args().frame_id.sequence_number,
      41u);
  EXPECT_EQ(
      saved_metrics[0]->AsScroll()->dispatch_args().frame_id.sequence_number,
      42u);
}

INSTANTIATE_TEST_SUITE_P(
    InputHandlerProxyScrollEventMetricsTest,
    InputHandlerProxyScrollEventMetricsTest,
    testing::ValuesIn<InputHandlerProxyScrollEventMetricsTestCase>(
        {{.inertial_phase = WebGestureEvent::InertialPhaseState::kNonMomentum,
          .is_inertial = false,
          .expected_event_type = cc::EventMetrics::EventType::kGestureScrollEnd,
          .test_name = "RegularScroll"},
         {.inertial_phase = WebGestureEvent::InertialPhaseState::kMomentum,
          .is_inertial = true,
          .expected_event_type =
              cc::EventMetrics::EventType::kInertialGestureScrollEnd,
          .test_name = "InertialScroll"}}),
    [](const testing::TestParamInfo<
        InputHandlerProxyScrollEventMetricsTest::ParamType>& info) {
      return info.param.test_name;
    });

struct InputHandlerProxyScrollUpdateEventMetricsTestCase {
  WebGestureEvent::InertialPhaseState inertial_phase;
  bool is_inertial;
  cc::ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type;
  cc::EventMetrics::EventType expected_event_type;
  std::string test_name;
};

class InputHandlerProxyScrollUpdateEventMetricsTest
    : public InputHandlerProxyEventMetricsTest,
      public testing::WithParamInterface<
          InputHandlerProxyScrollUpdateEventMetricsTestCase> {};

TEST_P(InputHandlerProxyScrollUpdateEventMetricsTest,
       SavesScrollUpdateMetrics) {
  constexpr base::TimeDelta kInterval = base::Milliseconds(16);
  const InputHandlerProxyScrollUpdateEventMetricsTestCase& param = GetParam();

  // Inject a gesture scroll begin first so that `InputHandlerProxy` would set
  // `InputHandlerProxy::currently_active_gesture_device()` and consequently
  // queue inputs instead of dispatching them immediately.
  input_handler_proxy_.HandleInputEventWithLatencyInfo(
      std::make_unique<WebCoalescedInputEvent>(
          std::make_unique<WebGestureEvent>(
              WebInputEvent::Type::kGestureScrollBegin,
              WebInputEvent::kNoModifiers,
              WebInputEvent::GetStaticTimeStampForTests(),
              WebGestureDevice::kTouchscreen),
          ui::LatencyInfo()),
      /* metrics= */ nullptr, /* callback= */ base::DoNothing());

  // Send begin frame 1.
  base::TimeTicks frame1_ts = tick_clock_.NowTicks();
  input_handler_proxy_.DeliverInputForBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, /* source_id= */ 0, /* sequence_number= */ 41,
      frame1_ts, frame1_ts + kInterval, kInterval,
      viz::BeginFrameArgs::NORMAL));

  // Inject the gesture scroll update. `InputHandlerProxy` will enqueue it.
  tick_clock_.Advance(base::Microseconds(10));
  base::TimeTicks timestamp = tick_clock_.NowTicks();
  tick_clock_.Advance(base::Microseconds(10));
  base::TimeTicks arrived_in_browser_main_timestamp = tick_clock_.NowTicks();
  tick_clock_.Advance(base::Microseconds(10));
  std::unique_ptr<WebGestureEvent> gesture_event =
      std::make_unique<WebGestureEvent>(
          WebInputEvent::Type::kGestureScrollUpdate,
          WebInputEvent::kNoModifiers,
          WebInputEvent::GetStaticTimeStampForTests(),
          WebGestureDevice::kTouchscreen);
  gesture_event->data.scroll_update.inertial_phase = param.inertial_phase;
  std::unique_ptr<cc::EventMetrics> metrics =
      cc::ScrollUpdateEventMetrics::CreateForTesting(
          ui::EventType::kGestureScrollUpdate,
          ui::ScrollInputType::kTouchscreen, param.is_inertial,
          param.scroll_update_type,
          /* delta= */ 1.0f, timestamp, arrived_in_browser_main_timestamp,
          &tick_clock_, /* trace_id= */ std::nullopt);
  input_handler_proxy_.HandleInputEventWithLatencyInfo(
      std::make_unique<WebCoalescedInputEvent>(std::move(gesture_event),
                                               ui::LatencyInfo()),
      std::move(metrics),
      /* callback= */ base::DoNothing());

  // Send begin frame 2, which dispatches the gesture scroll update.
  base::TimeTicks frame2_ts = frame1_ts + kInterval;
  tick_clock_.SetNowTicks(frame2_ts);
  input_handler_proxy_.DeliverInputForBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, /* source_id= */ 0, /* sequence_number= */ 42,
      frame1_ts, frame1_ts + kInterval, kInterval,
      viz::BeginFrameArgs::NORMAL));

  cc::EventMetrics::List saved_metrics =
      mock_input_handler_.events_metrics_manager.TakeSavedEventsMetrics();
  EXPECT_THAT(saved_metrics, SizeIs(1u));
  EXPECT_EQ(saved_metrics[0]->type(), param.expected_event_type);
  EXPECT_EQ(
      saved_metrics[0]->AsScroll()->begin_frame_args().frame_id.sequence_number,
      41u);
  EXPECT_EQ(
      saved_metrics[0]->AsScroll()->dispatch_args().frame_id.sequence_number,
      42u);
}

INSTANTIATE_TEST_SUITE_P(
    InputHandlerProxyScrollUpdateEventMetricsTest,
    InputHandlerProxyScrollUpdateEventMetricsTest,
    testing::ValuesIn<InputHandlerProxyScrollUpdateEventMetricsTestCase>(
        {{.inertial_phase = WebGestureEvent::InertialPhaseState::kNonMomentum,
          .is_inertial = false,
          .scroll_update_type =
              cc::ScrollUpdateEventMetrics::ScrollUpdateType::kStarted,
          .expected_event_type =
              cc::EventMetrics::EventType::kFirstGestureScrollUpdate,
          .test_name = "FirstScroll"},
         {.inertial_phase = WebGestureEvent::InertialPhaseState::kNonMomentum,
          .is_inertial = false,
          .scroll_update_type =
              cc::ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
          .expected_event_type =
              cc::EventMetrics::EventType::kGestureScrollUpdate,
          .test_name = "RegularScroll"},
         {.inertial_phase = WebGestureEvent::InertialPhaseState::kMomentum,
          .is_inertial = true,
          .scroll_update_type =
              cc::ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
          .expected_event_type =
              cc::EventMetrics::EventType::kInertialGestureScrollUpdate,
          .test_name = "InertialScroll"}}),
    [](const testing::TestParamInfo<
        InputHandlerProxyScrollUpdateEventMetricsTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace test
}  // namespace blink
