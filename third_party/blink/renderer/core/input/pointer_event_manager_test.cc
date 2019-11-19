// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/pointer_event_manager.h"

#include <limits>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {
class CheckPointerEventListenerCallback final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override {
    const String pointer_type = ((PointerEvent*)event)->pointerType();
    if (pointer_type == "mouse")
      mouse_event_received_count_++;
    else if (pointer_type == "touch")
      touch_event_received_count_++;
    else if (pointer_type == "pen")
      pen_event_received_count_++;
  }

  int mouseEventCount() const { return mouse_event_received_count_; }
  int touchEventCount() const { return touch_event_received_count_; }
  int penEventCount() const { return pen_event_received_count_; }

 private:
  int mouse_event_received_count_ = 0;
  int touch_event_received_count_ = 0;
  int pen_event_received_count_ = 0;
};

class PointerEventCoordinateListenerCallback final
    : public NativeEventListener {
 public:
  static PointerEventCoordinateListenerCallback* Create() {
    return MakeGarbageCollected<PointerEventCoordinateListenerCallback>();
  }

  void Invoke(ExecutionContext*, Event* event) override {
    const PointerEvent* pointer_event = (PointerEvent*)event;
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
  WebPointerEvent CreateTestPointerEvent(
      WebInputEvent::Type type,
      WebPointerProperties::PointerType pointer_type,
      WebFloatPoint position_in_widget = WebFloatPoint(100, 100),
      WebFloatPoint position_in_screen = WebFloatPoint(100, 100),
      int movement_x = 0,
      int movement_y = 0,
      float width = 1,
      float height = 1) {
    WebPointerEvent event(
        type,
        WebPointerProperties(
            1, pointer_type, WebPointerProperties::Button::kLeft,
            position_in_widget, position_in_screen, movement_x, movement_y),
        width, height);
    return event;
  }
  WebMouseEvent CreateTestMouseEvent(WebInputEvent::Type type,
                                     const WebFloatPoint& coordinates) {
    WebMouseEvent event(type, coordinates, coordinates,
                        WebPointerProperties::Button::kLeft, 0,
                        WebInputEvent::kLeftButtonDown,
                        WebInputEvent::GetStaticTimeStampForTests());
    event.SetFrameScale(1);
    return event;
  }
};

TEST_F(PointerEventManagerTest, HasPointerCapture) {
  WebView().MainFrameWidget()->Resize(WebSize(400, 400));
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

  ExceptionState exception(nullptr, ExceptionState::kExecutionContext, "", "");

  GetEventHandler().HandleMousePressEvent(
      CreateTestMouseEvent(WebInputEvent::kMouseDown, WebFloatPoint(100, 100)));

  ASSERT_FALSE(
      GetDocument().body()->hasPointerCapture(PointerEventFactory::kMouseId));

  GetDocument().body()->setPointerCapture(PointerEventFactory::kMouseId,
                                          exception);
  ASSERT_TRUE(
      GetDocument().body()->hasPointerCapture(PointerEventFactory::kMouseId));

  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseEvent(WebInputEvent::kMouseMove, WebFloatPoint(200, 200)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  ASSERT_TRUE(
      GetDocument().body()->hasPointerCapture(PointerEventFactory::kMouseId));

  GetDocument().body()->releasePointerCapture(PointerEventFactory::kMouseId,
                                              exception);
  ASSERT_FALSE(
      GetDocument().body()->hasPointerCapture(PointerEventFactory::kMouseId));
}

TEST_F(PointerEventManagerTest, PointerCancelsOfAllTypes) {
  WebView().MainFrameWidget()->Resize(WebSize(400, 400));
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
      CreateTestPointerEvent(WebInputEvent::kPointerDown,
                             WebPointerProperties::PointerType::kTouch),
      {}, {}));

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerDown,
                             WebPointerProperties::PointerType::kPen),
      {}, {}));

  GetEventHandler().HandleMousePressEvent(
      CreateTestMouseEvent(WebInputEvent::kMouseDown, WebFloatPoint(100, 100)));

  ASSERT_EQ(callback->mouseEventCount(), 0);
  ASSERT_EQ(callback->touchEventCount(), 0);
  ASSERT_EQ(callback->penEventCount(), 0);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerCausedUaAction,
                             WebPointerProperties::PointerType::kPen),
      {}, {}));
  ASSERT_EQ(callback->mouseEventCount(), 0);
  ASSERT_EQ(callback->touchEventCount(), 1);
  ASSERT_EQ(callback->penEventCount(), 1);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerCausedUaAction,
                             WebPointerProperties::PointerType::kTouch),
      {}, {}));
  ASSERT_EQ(callback->mouseEventCount(), 0);
  ASSERT_EQ(callback->touchEventCount(), 1);
  ASSERT_EQ(callback->penEventCount(), 1);

  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseEvent(WebInputEvent::kMouseMove, WebFloatPoint(200, 200)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  ASSERT_EQ(callback->mouseEventCount(), 1);
  ASSERT_EQ(callback->touchEventCount(), 1);
  ASSERT_EQ(callback->penEventCount(), 1);
}

TEST_F(PointerEventManagerTest, PointerEventCoordinates) {
  WebView().MainFrameWidget()->Resize(WebSize(400, 400));
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
      CreateTestPointerEvent(WebInputEvent::kPointerDown,
                             WebPointerProperties::PointerType::kTouch,
                             WebFloatPoint(150, 200), WebFloatPoint(100, 50),
                             10, 10, 16, 24),
      {}, {}));

  ASSERT_EQ(callback->last_client_x_, 75);
  ASSERT_EQ(callback->last_client_y_, 100);
  ASSERT_EQ(callback->last_page_x_, 75);
  ASSERT_EQ(callback->last_page_y_, 100);
  ASSERT_EQ(callback->last_screen_x_, 100);
  ASSERT_EQ(callback->last_screen_y_, 50);
  ASSERT_EQ(callback->last_width_, 8);
  ASSERT_EQ(callback->last_height_, 12);
  ASSERT_EQ(callback->last_movement_x_, 10);
  ASSERT_EQ(callback->last_movement_y_, 10);
}

TEST_F(PointerEventManagerTest, PointerEventMovements) {
  WebView().MainFrameWidget()->Resize(WebSize(400, 400));
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
    // Turn on the flag for test.
    ScopedConsolidatedMovementXYForTest scoped_feature(true);

    WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
        CreateTestPointerEvent(WebInputEvent::kPointerMove,
                               WebPointerProperties::PointerType::kMouse,
                               WebFloatPoint(150, 210), WebFloatPoint(100, 50),
                               10, 10),
        {}, {}));
    // The first pointermove event has movement_x/y 0.
    ASSERT_EQ(callback->last_screen_x_, 100);
    ASSERT_EQ(callback->last_screen_y_, 50);
    ASSERT_EQ(callback->last_movement_x_, 0);
    ASSERT_EQ(callback->last_movement_y_, 0);

    WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
        CreateTestPointerEvent(WebInputEvent::kPointerMove,
                               WebPointerProperties::PointerType::kMouse,
                               WebFloatPoint(150, 200), WebFloatPoint(132, 29),
                               10, 10),
        {}, {}));
    // pointermove event movement = event.screenX/Y - last_event.screenX/Y.
    ASSERT_EQ(callback->last_screen_x_, 132);
    ASSERT_EQ(callback->last_screen_y_, 29);
    ASSERT_EQ(callback->last_movement_x_, 32);
    ASSERT_EQ(callback->last_movement_y_, -21);

    WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
        CreateTestPointerEvent(WebInputEvent::kPointerMove,
                               WebPointerProperties::PointerType::kMouse,
                               WebFloatPoint(150, 210),
                               WebFloatPoint(113.8, 32.7), 10, 10),
        {}, {}));
    // fractional screen coordinates result in fractional movement.
    ASSERT_FLOAT_EQ(callback->last_screen_x_, 113.8);
    ASSERT_FLOAT_EQ(callback->last_screen_y_, 32.7);
    // TODO(eirage): These should be float value once mouse_event.idl change.
    ASSERT_FLOAT_EQ(callback->last_movement_x_, -19);
    ASSERT_FLOAT_EQ(callback->last_movement_y_, 3);
  }

  {
    // When flag is off, movementX/Y follows the value in WebPointerProperties.
    ScopedConsolidatedMovementXYForTest scoped_feature(false);

    WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
        CreateTestPointerEvent(WebInputEvent::kPointerMove,
                               WebPointerProperties::PointerType::kMouse,
                               WebFloatPoint(150, 210),
                               WebFloatPoint(100, 16.25), 1024, -8765),
        {}, {}));
    ASSERT_EQ(callback->last_screen_x_, 100);
    ASSERT_EQ(callback->last_screen_y_, 16.25);
    ASSERT_EQ(callback->last_movement_x_, 1024);
    ASSERT_EQ(callback->last_movement_y_, -8765);
  }
}

// Test that we are not losing fractions when truncating movements.
TEST_F(PointerEventManagerTest, PointerEventSmallFractionMovements) {
  WebView().MainFrameWidget()->Resize(WebSize(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "</body>");
  PointerEventCoordinateListenerCallback* callback =
      PointerEventCoordinateListenerCallback::Create();
  GetDocument().body()->addEventListener(event_type_names::kPointermove,
                                         callback);

  // Turn on the flag for test.
  ScopedConsolidatedMovementXYForTest scoped_feature(true);

  WebPointerEvent pointer_event = CreateTestPointerEvent(
      WebInputEvent::kPointerMove, WebPointerProperties::PointerType::kMouse,
      WebFloatPoint(150, 210), WebFloatPoint(113.8, 32.7), 0, 0);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_event));
  ASSERT_FLOAT_EQ(callback->last_movement_x_, 0);
  ASSERT_FLOAT_EQ(callback->last_movement_y_, 0);

  pointer_event.SetPositionInScreen(113.4, 32.9);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_event));
  ASSERT_FLOAT_EQ(callback->last_movement_x_, 0);
  ASSERT_FLOAT_EQ(callback->last_movement_y_, 0);

  pointer_event.SetPositionInScreen(113.0, 33.1);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_event));
  ASSERT_FLOAT_EQ(callback->last_movement_x_, 0);
  ASSERT_FLOAT_EQ(callback->last_movement_y_, 1);

  pointer_event.SetPositionInScreen(112.6, 33.3);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_event));
  ASSERT_FLOAT_EQ(callback->last_movement_x_, -1);
  ASSERT_FLOAT_EQ(callback->last_movement_y_, 0);
}

TEST_F(PointerEventManagerTest, PointerRawUpdateMovements) {
  WebView().MainFrameWidget()->Resize(WebSize(400, 400));
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

  // Turn on the flag for test.
  ScopedConsolidatedMovementXYForTest scoped_feature(true);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerRawUpdate,
                             WebPointerProperties::PointerType::kMouse,
                             WebFloatPoint(150, 210), WebFloatPoint(100, 50),
                             10, 10),
      {}, {}));
  // The first pointerrawupdate event has movement_x/y 0.
  ASSERT_EQ(callback->last_screen_x_, 100);
  ASSERT_EQ(callback->last_screen_y_, 50);
  ASSERT_EQ(callback->last_movement_x_, 0);
  ASSERT_EQ(callback->last_movement_y_, 0);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerRawUpdate,
                             WebPointerProperties::PointerType::kMouse,
                             WebFloatPoint(150, 200), WebFloatPoint(132, 29),
                             10, 10),
      {}, {}));
  // pointerrawupdate event movement = event.screenX/Y - last_event.screenX/Y.
  ASSERT_EQ(callback->last_screen_x_, 132);
  ASSERT_EQ(callback->last_screen_y_, 29);
  ASSERT_EQ(callback->last_movement_x_, 32);
  ASSERT_EQ(callback->last_movement_y_, -21);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerMove,
                             WebPointerProperties::PointerType::kMouse,
                             WebFloatPoint(150, 200), WebFloatPoint(144, 30),
                             10, 10),
      {}, {}));
  // First pointermove, have 0 movements.
  ASSERT_EQ(callback->last_screen_x_, 144);
  ASSERT_EQ(callback->last_screen_y_, 30);
  ASSERT_EQ(callback->last_movement_x_, 0);
  ASSERT_EQ(callback->last_movement_y_, 0);

  WebView().MainFrameWidget()->HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerRawUpdate,
                             WebPointerProperties::PointerType::kMouse,
                             WebFloatPoint(150, 200), WebFloatPoint(142, 32),
                             10, 10),
      {}, {}));
  // pointerrawupdate event's movement is independent from pointermoves.
  ASSERT_EQ(callback->last_screen_x_, 142);
  ASSERT_EQ(callback->last_screen_y_, 32);
  ASSERT_EQ(callback->last_movement_x_, 10);
  ASSERT_EQ(callback->last_movement_y_, 3);
}

TEST_F(PointerEventManagerTest, PointerUnadjustedMovement) {
  WebView().MainFrameWidget()->Resize(WebSize(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "</body>");
  PointerEventCoordinateListenerCallback* callback =
      PointerEventCoordinateListenerCallback::Create();
  GetDocument().body()->addEventListener(event_type_names::kPointermove,
                                         callback);

  WebPointerEvent event = CreateTestPointerEvent(
      WebInputEvent::kPointerMove, WebPointerProperties::PointerType::kMouse,
      WebFloatPoint(150, 210), WebFloatPoint(100, 50), 120, -321);
  event.is_raw_movement_event = true;
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, {}, {}));

  // If is_raw_movement_event is true, PE use the raw movement value from
  // movement_x/y.
  ASSERT_EQ(callback->last_screen_x_, 100);
  ASSERT_EQ(callback->last_screen_y_, 50);
  ASSERT_EQ(callback->last_movement_x_, 120);
  ASSERT_EQ(callback->last_movement_y_, -321);
}

}  // namespace blink
