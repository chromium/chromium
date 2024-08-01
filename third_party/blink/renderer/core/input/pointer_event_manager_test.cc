// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/pointer_event_manager.h"

#include <limits>
#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/pointer_type_names.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "ui/base/ui_base_features.h"

namespace blink {

namespace {
class CheckPointerEventListenerCallback final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override {
    num_events_received_++;

    const String pointer_type =
        static_cast<PointerEvent*>(event)->pointerType();
    if (pointer_type == pointer_type_names::kMouse)
      num_type_mouse_received_++;
    else if (pointer_type == pointer_type_names::kTouch)
      num_type_touch_received_++;
    else if (pointer_type == pointer_type_names::kPen)
      num_type_pen_received_++;
  }

  int numEventsReceived() const { return num_events_received_; }

  int numTypeMouseReceived() const { return num_type_mouse_received_; }
  int numTypeTouchReceived() const { return num_type_touch_received_; }
  int numTypePenReceived() const { return num_type_pen_received_; }

 private:
  int num_events_received_ = 0;
  int num_type_mouse_received_ = 0;
  int num_type_touch_received_ = 0;
  int num_type_pen_received_ = 0;
};

class PointerEventCoordinateListenerCallback final
    : public NativeEventListener {
 public:
  static PointerEventCoordinateListenerCallback* Create() {
    return MakeGarbageCollected<PointerEventCoordinateListenerCallback>();
  }

  void Invoke(ExecutionContext*, Event* event) override {
    const PointerEvent* pointer_event = static_cast<PointerEvent*>(event);
    last_client_x_ = pointer_event->clientX();
    last_client_y_ = pointer_event->clientY();
    last_page_x_ = pointer_event->pageX();
    last_page_y_ = pointer_event->pageY();
    last_screen_x_ = pointer_event->screenX();
    last_screen_y_ = pointer_event->screenY();
    last_width_ = pointer_event->width();
    last_height_ = pointer_event->height();
    last_movement_x_ = pointer_event->movementX();
    last_movement_y_ = pointer_event->movementY();
  }

  double last_client_x_ = 0;
  double last_client_y_ = 0;
  double last_page_x_ = 0;
  double last_page_y_ = 0;
  double last_screen_x_ = 0;
  double last_screen_y_ = 0;
  double last_width_ = 0;
  double last_height_ = 0;
  double last_movement_x_ = 0;
  double last_movement_y_ = 0;
};

}  // namespace

class PointerEventManagerTest : public SimTest {
 protected:
  EventHandler& GetEventHandler() {
    return GetDocument().GetFrame()->GetEventHandler();
  }
  std::unique_ptr<WebPointerEvent> CreateTestPointerEvent(
      WebInputEvent::Type type,
      WebPointerProperties::PointerType pointer_type,
      PointerId id = 1) {
    return CreateTestPointerEvent(type, pointer_type, id, gfx::PointF(100, 100),
                                  gfx::PointF(100, 100), 0, 0, 1, 1);
  }
  std::unique_ptr<WebPointerEvent> CreateTestPointerEvent(
      WebInputEvent::Type type,
      WebPointerProperties::PointerType pointer_type,
      gfx::PointF position_in_widget,
      gfx::PointF position_in_screen,
      int movement_x,
      int movement_y,
      float width = 1,
      float height = 1) {
    return CreateTestPointerEvent(type, pointer_type, 1, position_in_widget,
                                  position_in_screen, movement_x, movement_y,
                                  width, height);
  }
  WebMouseEvent CreateTestMouseEvent(WebInputEvent::Type type,
                                     const gfx::PointF& coordinates) {
    WebMouseEvent event(type, coordinates, coordinates,
                        WebPointerProperties::Button::kLeft, 0,
                        WebInputEvent::kLeftButtonDown,
                        WebInputEvent::GetStaticTimeStampForTests());
    event.SetFrameScale(1);
    return event;
  }

 private:
  std::unique_ptr<WebPointerEvent> CreateTestPointerEvent(
      WebInputEvent::Type type,
      WebPointerProperties::PointerType pointer_type,
      PointerId id,
      gfx::PointF position_in_widget,
      gfx::PointF position_in_screen,
      int movement_x,
      int movement_y,
      float width,
      float height) {
    return std::make_unique<WebPointerEvent>(
        type,
        WebPointerProperties(
            id, pointer_type, WebPointerProperties::Button::kLeft,
            position_in_widget, position_in_screen, movement_x, movement_y),
        width, height);
  }
};

TEST_F(PointerEventManagerTest, HasPointerCapture) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "</body>");
  ASSERT_FALSE(GetDocument().body()->hasPointerCapture(4));
  ASSERT_FALSE(GetDocument().body()->hasPointerCapture(
      std::numeric_limits<PointerId>::max()));
  ASSERT_FALSE(GetDocument().body()->hasPointerCapture(0));
  ASSERT_FALSE(GetDocument().body()->hasPointerCapture(-1));
  ASSERT_FALSE(
      GetDocument().body()->hasPointerCapture(PointerEventFactory::kMouseId));

  ExceptionState exception(nullptr, v8::ExceptionContext::kOperation, "", "");

  GetEventHandler().HandleMousePressEvent(CreateTestMouseEvent(
      WebInputEvent::Type::kMouseDown, gfx::PointF(100, 100)));

  ASSERT_FALSE(
      GetDocument().body()->hasPointerCapture(PointerEventFactory::kMouseId));

  GetDocument().body()->setPointerCapture(PointerEventFactory::kMouseId,
                                          exception);
  ASSERT_TRUE(
      GetDocument().body()->hasPointerCapture(PointerEventFactory::kMouseId));

  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseEvent(WebInputEvent::Type::kMouseMove,
                           gfx::PointF(200, 200)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  ASSERT_TRUE(
      GetDocument().body()->hasPointerCapture(PointerEventFactory::kMouseId));

  GetDocument().body()->releasePointerCapture(PointerEventFactory::kMouseId,
                                              exception);
  ASSERT_FALSE(
      GetDocument().body()->hasPointerCapture(PointerEventFactory::kMouseId));
}

TEST_F(PointerEventManagerTest, PointerCancelsOfAllTypes) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "<div draggable='true' style='width: 150px; height: 150px;'></div>"
      "</body>");
  auto* callback = MakeGarbageCollected<CheckPointerEventListenerCallback>();
  GetDocument().body()->addEventListener(event_type_names::kPointercancel,
                                         callback);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerDown,
                             WebPointerProperties::PointerType::kTouch),
      {}, {}, ui::LatencyInfo()));

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerDown,
                             WebPointerProperties::PointerType::kPen),
      {}, {}, ui::LatencyInfo()));

  GetEventHandler().HandleMousePressEvent(CreateTestMouseEvent(
      WebInputEvent::Type::kMouseDown, gfx::PointF(100, 100)));

  ASSERT_EQ(callback->numTypeMouseReceived(), 0);
  ASSERT_EQ(callback->numTypeTouchReceived(), 0);
  ASSERT_EQ(callback->numTypePenReceived(), 0);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerCausedUaAction,
                             WebPointerProperties::PointerType::kPen),
      {}, {}, ui::LatencyInfo()));
  ASSERT_EQ(callback->numTypeMouseReceived(), 0);
  ASSERT_EQ(callback->numTypeTouchReceived(), 1);
  ASSERT_EQ(callback->numTypePenReceived(), 1);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerCausedUaAction,
                             WebPointerProperties::PointerType::kTouch),
      {}, {}, ui::LatencyInfo()));
  ASSERT_EQ(callback->numTypeMouseReceived(), 0);
  ASSERT_EQ(callback->numTypeTouchReceived(), 1);
  ASSERT_EQ(callback->numTypePenReceived(), 1);

  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseEvent(WebInputEvent::Type::kMouseMove,
                           gfx::PointF(200, 200)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  ASSERT_EQ(callback->numTypeMouseReceived(), 1);
  ASSERT_EQ(callback->numTypeTouchReceived(), 1);
  ASSERT_EQ(callback->numTypePenReceived(), 1);
}

// Tests that user activation in not triggered if Blink receives a pointerup
// event after a gesture scroll has started.  On a page w/o either pointer or
// touch event listeners, WidgetInputHandlerManager dispatches a kPointerup
// event to Blink after dispatching kPointerCausedUaAction to mark an
// ongoing scroll, see https://crbug.com/1313076.
TEST_F(PointerEventManagerTest, NoUserActivationWithPointerUpAfterCancel) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'></body>");

  ASSERT_FALSE(
      WebView().MainFrameWidget()->LocalRoot()->HasTransientUserActivation());

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerCausedUaAction,
                             WebPointerProperties::PointerType::kTouch),
      {}, {}, ui::LatencyInfo()));

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerUp,
                             WebPointerProperties::PointerType::kTouch),
      {}, {}, ui::LatencyInfo()));

  ASSERT_FALSE(
      WebView().MainFrameWidget()->LocalRoot()->HasTransientUserActivation());
}

TEST_F(PointerEventManagerTest, PointerCancelForNonExistentid) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "<div draggable='true' style='width: 150px; height: 150px;'></div>"
      "</body>");
  auto* callback = MakeGarbageCollected<CheckPointerEventListenerCallback>();
  GetDocument().body()->addEventListener(event_type_names::kPointercancel,
                                         callback);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerCancel,
                             WebPointerProperties::PointerType::kTouch, 100),
      {}, {}, ui::LatencyInfo()));

  ASSERT_EQ(callback->numEventsReceived(), 0);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerDown,
                             WebPointerProperties::PointerType::kTouch, 100),
      {}, {}, ui::LatencyInfo()));

  ASSERT_EQ(callback->numEventsReceived(), 0);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerCancel,
                             WebPointerProperties::PointerType::kTouch, 100),
      {}, {}, ui::LatencyInfo()));

  ASSERT_EQ(callback->numEventsReceived(), 1);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerCancel,
                             WebPointerProperties::PointerType::kTouch, 200),
      {}, {}, ui::LatencyInfo()));

  ASSERT_EQ(callback->numEventsReceived(), 1);
}

TEST_F(PointerEventManagerTest, PointerEventCoordinates) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "</body>");
  WebView().SetPageScaleFactor(2);
  PointerEventCoordinateListenerCallback* callback =
      PointerEventCoordinateListenerCallback::Create();
  GetDocument().body()->addEventListener(event_type_names::kPointerdown,
                                         callback);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerDown,
                             WebPointerProperties::PointerType::kTouch,
                             gfx::PointF(150, 200), gfx::PointF(100, 50), 10,
                             10, 16, 24),
      {}, {}, ui::LatencyInfo()));

  ASSERT_EQ(callback->last_client_x_, 75);
  ASSERT_EQ(callback->last_client_y_, 100);
  ASSERT_EQ(callback->last_page_x_, 75);
  ASSERT_EQ(callback->last_page_y_, 100);
  ASSERT_EQ(callback->last_screen_x_, 100);
  ASSERT_EQ(callback->last_screen_y_, 50);
  ASSERT_EQ(callback->last_width_, 8);
  ASSERT_EQ(callback->last_height_, 12);
  ASSERT_EQ(callback->last_movement_x_, 0);
  ASSERT_EQ(callback->last_movement_y_, 0);
}

TEST_F(PointerEventManagerTest, PointerEventMovements) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "</body>");
  PointerEventCoordinateListenerCallback* callback =
      PointerEventCoordinateListenerCallback::Create();
  GetDocument().body()->addEventListener(event_type_names::kPointermove,
                                         callback);

  {
    WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
        CreateTestPointerEvent(WebInputEvent::Type::kPointerMove,
                               WebPointerProperties::PointerType::kMouse,
                               gfx::PointF(150, 210), gfx::PointF(100, 50), 10,
                               10),
        {}, {}, ui::LatencyInfo()));
    // The first pointermove event has movement_x/y 0.
    ASSERT_EQ(callback->last_screen_x_, 100);
    ASSERT_EQ(callback->last_screen_y_, 50);
    ASSERT_EQ(callback->last_movement_x_, 0);
    ASSERT_EQ(callback->last_movement_y_, 0);

    WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
        CreateTestPointerEvent(WebInputEvent::Type::kPointerMove,
                               WebPointerProperties::PointerType::kMouse,
                               gfx::PointF(150, 200), gfx::PointF(132, 29), 10,
                               10),
        {}, {}, ui::LatencyInfo()));
    // pointermove event movement = event.screenX/Y - last_event.screenX/Y.
    ASSERT_EQ(callback->last_screen_x_, 132);
    ASSERT_EQ(callback->last_screen_y_, 29);
    ASSERT_EQ(callback->last_movement_x_, 32);
    ASSERT_EQ(callback->last_movement_y_, -21);

    WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
        CreateTestPointerEvent(WebInputEvent::Type::kPointerMove,
                               WebPointerProperties::PointerType::kMouse,
                               gfx::PointF(150, 210), gfx::PointF(113.8, 32.7),
                               10, 10),
        {}, {}, ui::LatencyInfo()));
    // fractional screen coordinates result in fractional movement.
    ASSERT_FLOAT_EQ(callback->last_screen_x_, 113.8);
    ASSERT_FLOAT_EQ(callback->last_screen_y_, 32.7);
    // TODO(eirage): These should be float value once mouse_event.idl change.
    ASSERT_FLOAT_EQ(callback->last_movement_x_, -19);
    ASSERT_FLOAT_EQ(callback->last_movement_y_, 3);
  }
}

// Test that we are not losing fractions when truncating movements.
TEST_F(PointerEventManagerTest, PointerEventSmallFractionMovements) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "</body>");
  PointerEventCoordinateListenerCallback* callback =
      PointerEventCoordinateListenerCallback::Create();
  GetDocument().body()->addEventListener(event_type_names::kPointermove,
                                         callback);

  std::unique_ptr<WebPointerEvent> pointer_event = CreateTestPointerEvent(
      WebInputEvent::Type::kPointerMove,
      WebPointerProperties::PointerType::kMouse, gfx::PointF(150, 210),
      gfx::PointF(113.8, 32.7), 0, 0);
  ASSERT_NE(nullptr, pointer_event);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_event->Clone(), ui::LatencyInfo()));
  ASSERT_FLOAT_EQ(callback->last_movement_x_, 0);
  ASSERT_FLOAT_EQ(callback->last_movement_y_, 0);

  pointer_event->SetPositionInScreen(113.4, 32.9);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_event->Clone(), ui::LatencyInfo()));
  ASSERT_FLOAT_EQ(callback->last_movement_x_, 0);
  ASSERT_FLOAT_EQ(callback->last_movement_y_, 0);

  pointer_event->SetPositionInScreen(113.0, 33.1);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_event->Clone(), ui::LatencyInfo()));
  ASSERT_FLOAT_EQ(callback->last_movement_x_, 0);
  ASSERT_FLOAT_EQ(callback->last_movement_y_, 1);

  pointer_event->SetPositionInScreen(112.6, 33.3);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(std::move(pointer_event), ui::LatencyInfo()));
  ASSERT_FLOAT_EQ(callback->last_movement_x_, -1);
  ASSERT_FLOAT_EQ(callback->last_movement_y_, 0);
}

TEST_F(PointerEventManagerTest, PointerRawUpdateMovements) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "</body>");
  PointerEventCoordinateListenerCallback* callback =
      PointerEventCoordinateListenerCallback::Create();
  GetDocument().body()->addEventListener(event_type_names::kPointermove,
                                         callback);
  GetDocument().body()->addEventListener(event_type_names::kPointerrawupdate,
                                         callback);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerRawUpdate,
                             WebPointerProperties::PointerType::kMouse,
                             gfx::PointF(150, 210), gfx::PointF(100, 50), 10,
                             10),
      {}, {}, ui::LatencyInfo()));
  // The first pointerrawupdate event has movement_x/y 0.
  ASSERT_EQ(callback->last_screen_x_, 100);
  ASSERT_EQ(callback->last_screen_y_, 50);
  ASSERT_EQ(callback->last_movement_x_, 0);
  ASSERT_EQ(callback->last_movement_y_, 0);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerRawUpdate,
                             WebPointerProperties::PointerType::kMouse,
                             gfx::PointF(150, 200), gfx::PointF(132, 29), 10,
                             10),
      {}, {}, ui::LatencyInfo()));
  // pointerrawupdate event movement = event.screenX/Y - last_event.screenX/Y.
  ASSERT_EQ(callback->last_screen_x_, 132);
  ASSERT_EQ(callback->last_screen_y_, 29);
  ASSERT_EQ(callback->last_movement_x_, 32);
  ASSERT_EQ(callback->last_movement_y_, -21);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerMove,
                             WebPointerProperties::PointerType::kMouse,
                             gfx::PointF(150, 200), gfx::PointF(144, 30), 10,
                             10),
      {}, {}, ui::LatencyInfo()));
  // First pointermove, have 0 movements.
  ASSERT_EQ(callback->last_screen_x_, 144);
  ASSERT_EQ(callback->last_screen_y_, 30);
  ASSERT_EQ(callback->last_movement_x_, 0);
  ASSERT_EQ(callback->last_movement_y_, 0);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerRawUpdate,
                             WebPointerProperties::PointerType::kMouse,
                             gfx::PointF(150, 200), gfx::PointF(142, 32), 10,
                             10),
      {}, {}, ui::LatencyInfo()));
  // pointerrawupdate event's movement is independent from pointermoves.
  ASSERT_EQ(callback->last_screen_x_, 142);
  ASSERT_EQ(callback->last_screen_y_, 32);
  ASSERT_EQ(callback->last_movement_x_, 10);
  ASSERT_EQ(callback->last_movement_y_, 3);
}

TEST_F(PointerEventManagerTest, PointerRawUpdateWithRelativeMotionEvent) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding:0px; width:400px; height:400px;'></body>");

  PointerEventCoordinateListenerCallback* callback =
      PointerEventCoordinateListenerCallback::Create();
  GetDocument().body()->addEventListener(event_type_names::kPointerrawupdate,
                                         callback);

  // Initial movement_x/y are both 0.
  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerRawUpdate,
                             WebPointerProperties::PointerType::kMouse,
                             gfx::PointF(350, 350), gfx::PointF(350, 350), 10,
                             10),
      {}, {}, ui::LatencyInfo()));
  ASSERT_EQ(callback->last_movement_x_, 0);
  ASSERT_EQ(callback->last_movement_y_, 0);

  // After moving the mouse by (+40,+20), movement_x/y have same deltas.
  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerRawUpdate,
                             WebPointerProperties::PointerType::kMouse,
                             gfx::PointF(390, 370), gfx::PointF(390, 370), 10,
                             10),
      {}, {}, ui::LatencyInfo()));
  ASSERT_EQ(callback->last_movement_x_, 40);
  ASSERT_EQ(callback->last_movement_y_, 20);

  // A relative motion event to pull the mouse pointer back towards the center
  // is not exposed to JS, so the event handler is not called and the cached
  // coordinates remain unchanged.
  std::unique_ptr<WebInputEvent> synthetic_event = CreateTestPointerEvent(
      WebInputEvent::Type::kPointerRawUpdate,
      WebPointerProperties::PointerType::kMouse, gfx::PointF(200, 200),
      gfx::PointF(200, 200), 10, 10);
  synthetic_event->SetModifiers(WebInputEvent::Modifiers::kRelativeMotionEvent);
  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      std::move(synthetic_event), {}, {}, ui::LatencyInfo()));
  ASSERT_EQ(callback->last_movement_x_, 40);
  ASSERT_EQ(callback->last_movement_y_, 20);

  // After moving the mouse by (+15,-10) from the last unexposed event,
  // movement_x/y have deltas from the last event, not the deltas from the last
  // exposed event.
  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::Type::kPointerRawUpdate,
                             WebPointerProperties::PointerType::kMouse,
                             gfx::PointF(215, 190), gfx::PointF(215, 190), 10,
                             10),
      {}, {}, ui::LatencyInfo()));
  ASSERT_EQ(callback->last_movement_x_, 15);
  ASSERT_EQ(callback->last_movement_y_, -10);
}

TEST_F(PointerEventManagerTest, PointerUnadjustedMovement) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "</body>");
  PointerEventCoordinateListenerCallback* callback =
      PointerEventCoordinateListenerCallback::Create();
  GetDocument().body()->addEventListener(event_type_names::kPointermove,
                                         callback);

  std::unique_ptr<WebPointerEvent> event = CreateTestPointerEvent(
      WebInputEvent::Type::kPointerMove,
      WebPointerProperties::PointerType::kMouse, gfx::PointF(150, 210),
      gfx::PointF(100, 50), 120, -321);
  ASSERT_NE(nullptr, event);
  event->is_raw_movement_event = true;
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(std::move(event), {}, {}, ui::LatencyInfo()));

  // If is_raw_movement_event is true, PE use the raw movement value from
  // movement_x/y.
  ASSERT_EQ(callback->last_screen_x_, 100);
  ASSERT_EQ(callback->last_screen_y_, 50);
  ASSERT_EQ(callback->last_movement_x_, 120);
  ASSERT_EQ(callback->last_movement_y_, -321);
}

using PanAction = blink::mojom::PanAction;

class PanActionWidgetInputHandlerHost
    : public frame_test_helpers::TestWidgetInputHandlerHost {
 public:
  void SetPanAction(PanAction pan_action) override { pan_action_ = pan_action; }

  void ResetPanAction() { pan_action_ = PanAction::kNone; }

  PanAction pan_action() const { return pan_action_; }

 private:
  PanAction pan_action_ = PanAction::kNone;
};

class PanActionTrackingWebFrameWidget
    : public frame_test_helpers::TestWebFrameWidget {
 public:
  using frame_test_helpers::TestWebFrameWidget::TestWebFrameWidget;

  // frame_test_helpers::TestWebFrameWidget overrides.
  frame_test_helpers::TestWidgetInputHandlerHost* GetInputHandlerHost()
      override {
    return &input_handler_host_;
  }

  void ResetPanAction() { input_handler_host_.ResetPanAction(); }

  PanAction LastPanAction() { return input_handler_host_.pan_action(); }

 private:
  PanActionWidgetInputHandlerHost input_handler_host_;
};

class PanActionPointerEventTest : public PointerEventManagerTest {
 public:
  PanActionPointerEventTest() {
    feature_list_.InitWithFeatures({blink::features::kStylusPointerAdjustment},
                                   {});
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
    return MakeGarbageCollected<PanActionTrackingWebFrameWidget>(
        pass_key, std::move(frame_widget_host), std::move(frame_widget),
        std::move(widget_host), std::move(widget), std::move(task_runner),
        frame_sink_id, hidden, never_composited, is_for_child_local_root,
        is_for_nested_main_frame, is_for_scalable_page);
  }

 protected:
  // Sets inner HTML and runs document lifecycle.
  void SetBodyInnerHTML(const String& body_content) {
    GetDocument().body()->setInnerHTML(body_content, ASSERT_NO_EXCEPTION);
    WebView().MainFrameWidget()->UpdateLifecycle(WebLifecycleUpdate::kAll,
                                                 DocumentUpdateReason::kTest);
  }

  PanActionTrackingWebFrameWidget* GetWidget() {
    return static_cast<PanActionTrackingWebFrameWidget*>(
        WebView().MainFrameWidget());
  }

  WebMouseEvent CreateTestMouseMoveEvent(
      WebPointerProperties::PointerType pointer_type,
      const gfx::PointF& coordinates) {
    WebMouseEvent event(WebInputEvent::Type::kMouseMove, coordinates,
                        coordinates, WebPointerProperties::Button::kNoButton,
                        /* click_count */ 0, /* modifiers */ 0,
                        WebInputEvent::GetStaticTimeStampForTests());
    event.pointer_type = pointer_type;
    return event;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PanActionPointerEventTest, PanActionStylusWritable) {
  ScopedStylusHandwritingForTest stylus_handwriting(true);
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <input type=text style='width: 100px; height: 100px;'>
  )HTML");

  PanActionTrackingWebFrameWidget* widget = GetWidget();

  // Pan action to be stylus writable for contenteditable with pointer as kPen.
  ASSERT_EQ(widget->LastPanAction(), PanAction::kNone);
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(50, 50)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kStylusWritable);

  // Pan action is not stylus writable when pointer type is kMouse.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kMouse,
                               gfx::PointF(50, 50)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_NE(widget->LastPanAction(), PanAction::kStylusWritable);

  // Pan action is stylus writable when pointer type is kEraser.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kEraser,
                               gfx::PointF(50, 50)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kStylusWritable);
}

TEST_F(PanActionPointerEventTest, PanActionMoveCursor) {
  ScopedStylusHandwritingForTest stylus_handwriting(true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({::features::kSwipeToMoveCursor}, {});
  if (!::features::IsSwipeToMoveCursorEnabled())
    return;

  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <input type=password style='width: 100px; height: 100px;'>
  )HTML");

  // Pan action is expected to be kMoveCursorOrScroll for password inputs.
  PanActionTrackingWebFrameWidget* widget = GetWidget();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kNone);
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(50, 50)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kMoveCursorOrScroll);
}

TEST_F(PanActionPointerEventTest, PanActionNoneAndScroll) {
  ScopedStylusHandwritingForTest stylus_handwriting(true);
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width: 100px; height: 100px; touch-action: none'/>
  )HTML");

  PanActionTrackingWebFrameWidget* widget = GetWidget();

  // Pan action set to kNone when touch action does not allow panning.
  ASSERT_EQ(widget->LastPanAction(), PanAction::kNone);
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(50, 50)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kNone);

  // Pan action to be scroll when element under pointer allows panning but does
  // not allow both swipe to move cursor and stylus writing.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("touch-action: pan"));
  widget->UpdateLifecycle(WebLifecycleUpdate::kAll,
                          DocumentUpdateReason::kTest);
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(50, 50)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kScroll);
}

TEST_F(PanActionPointerEventTest, PanActionNotSetWhenTouchActive) {
  ScopedStylusHandwritingForTest stylus_handwriting(true);
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <input type=text style='width: 100px; height: 100px;'>
  )HTML");

  std::unique_ptr<WebPointerEvent> event =
      CreateTestPointerEvent(WebInputEvent::Type::kPointerDown,
                             WebPointerProperties::PointerType::kPen,
                             gfx::PointF(50, 50), gfx::PointF(50, 50), 1, 1);
  PanActionTrackingWebFrameWidget* widget = GetWidget();

  // Send pointer down before move.
  widget->HandleInputEvent(
      WebCoalescedInputEvent(std::move(event), {}, {}, ui::LatencyInfo()));
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kNone);

  // Pan action is not updated when touch is active.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(50, 50)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kNone);
}

TEST_F(PanActionPointerEventTest, PanActionAdjustedStylusWritable) {
  ScopedStylusHandwritingForTest stylus_handwriting(true);
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <input type=text style='width: 100px; height: 100px;'>
  )HTML");

  PanActionTrackingWebFrameWidget* widget = GetWidget();

  // Pan action adjusted as stylus writable for 15px around edit area with
  // pointer as kPen.
  ASSERT_EQ(widget->LastPanAction(), PanAction::kNone);
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(110, 110)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kStylusWritable);

  // Pan action not adjusted when pointer type is kMouse.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kMouse,
                               gfx::PointF(110, 110)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_NE(widget->LastPanAction(), PanAction::kStylusWritable);

  // Pan action adjusted with pointer as kEraser.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kEraser,
                               gfx::PointF(110, 110)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kStylusWritable);
}

TEST_F(PanActionPointerEventTest, PanActionAdjustedWithTappableNodeNearby) {
  ScopedStylusHandwritingForTest stylus_handwriting(true);
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <input type=text style='width: 100px; height: 100px;'>
    <button id='button1'>Button</button>
  )HTML");

  PanActionTrackingWebFrameWidget* widget = GetWidget();

  // Pan action adjusted as stylus writable below the editable node.
  ASSERT_EQ(widget->LastPanAction(), PanAction::kNone);
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(50, 110)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kStylusWritable);

  // On a tappable node to the right, then pan action is not writable.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(110, 50)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_NE(widget->LastPanAction(), PanAction::kStylusWritable);
}

TEST_F(PanActionPointerEventTest, PanActionAdjustedWhenZoomed) {
  ScopedStylusHandwritingForTest stylus_handwriting(true);
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>
      html { zoom: 2; margin: 0; padding: 0; border: none; }
      body { margin: 0; padding: 0; border: none; }
    </style>
    <input type=text style='width: 50px; height: 50px; margin-top: 50px;'>
  )HTML");

  PanActionTrackingWebFrameWidget* widget = GetWidget();

  // Pan action adjusted as stylus writable for (15 / 2)px around edit area
  // with pointer as kPen.
  ASSERT_EQ(widget->LastPanAction(), PanAction::kNone);
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(50, 94)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kStylusWritable);

  // Pan action is stylus writable on editable node.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(50, 125)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kStylusWritable);

  // Pan action is not stylus writable outside of editable node.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(110, 225)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_NE(widget->LastPanAction(), PanAction::kStylusWritable);
}

TEST_F(PanActionPointerEventTest, PanActionSentAcrossFrames) {
  ScopedStylusHandwritingForTest stylus_handwriting(true);
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; } iframe { display: block; } </style>
    <input type=text style='width: 100px; height: 100px;' />
    <div style='margin: 0px;'>
      <iframe style='width: 500px; height: 500px;'
        srcdoc='<body style="margin: 0; height: 500px; width: 500px;
                touch-action: none"></body>'>
      </iframe>
    </div>
  )HTML");

  PanActionTrackingWebFrameWidget* widget = GetWidget();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kNone);

  // Pan action is stylus writable on editable node.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(50, 50)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kStylusWritable);

  // Pan action is none on an Iframe with touch-action set as none.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(200, 200)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kNone);

  // Pan action is set stylus writable again outside iframe.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseMoveEvent(WebPointerProperties::PointerType::kPen,
                               gfx::PointF(50, 50)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  test::RunPendingTasks();
  ASSERT_EQ(widget->LastPanAction(), PanAction::kStylusWritable);
}

}  // namespace blink
