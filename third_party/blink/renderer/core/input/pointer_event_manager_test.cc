// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/pointer_event_manager.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

namespace {
class CheckPointerEventListenerCallback final : public EventListener {
 public:
  static CheckPointerEventListenerCallback* Create() {
    return new CheckPointerEventListenerCallback();
  }

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
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
  CheckPointerEventListenerCallback()
      : EventListener(EventListener::kCPPEventListenerType) {}
  int mouse_event_received_count_ = 0;
  int touch_event_received_count_ = 0;
  int pen_event_received_count_ = 0;
};

class PointerEventCoordinateListenerCallback final : public EventListener {
 public:
  static PointerEventCoordinateListenerCallback* Create() {
    return new PointerEventCoordinateListenerCallback();
  }

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
    const PointerEvent* pointer_event = (PointerEvent*)event;
    last_client_x_ = pointer_event->clientX();
    last_client_y_ = pointer_event->clientY();
    last_page_x_ = pointer_event->pageX();
    last_page_y_ = pointer_event->pageY();
    last_screen_x_ = pointer_event->screenX();
    last_screen_y_ = pointer_event->screenY();
    last_width_ = pointer_event->width();
    last_height_ = pointer_event->height();
  }

  double last_client_x_ = 0;
  double last_client_y_ = 0;
  double last_page_x_ = 0;
  double last_page_y_ = 0;
  double last_screen_x_ = 0;
  double last_screen_y_ = 0;
  double last_width_ = 0;
  double last_height_ = 0;

 private:
  PointerEventCoordinateListenerCallback()
      : EventListener(EventListener::kCPPEventListenerType) {}
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
                        WebPointerProperties::Button::kLeft, 0, 0,
                        WebInputEvent::GetStaticTimeStampForTests());
    event.SetFrameScale(1);
    return event;
  }
};

TEST_F(PointerEventManagerTest, PointerCancelsOfAllTypes) {
  WebView().Resize(WebSize(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "<div draggable='true' style='width: 150px; height: 150px;'></div>"
      "</body>");
  CheckPointerEventListenerCallback* callback =
      CheckPointerEventListenerCallback::Create();
  GetDocument().body()->addEventListener(EventTypeNames::pointercancel,
                                         callback);

  WebView().HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerDown,
                             WebPointerProperties::PointerType::kTouch),
      std::vector<WebPointerEvent>(), std::vector<WebPointerEvent>()));

  WebView().HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerDown,
                             WebPointerProperties::PointerType::kPen),
      std::vector<WebPointerEvent>(), std::vector<WebPointerEvent>()));

  GetEventHandler().HandleMousePressEvent(
      CreateTestMouseEvent(WebInputEvent::kMouseDown, WebFloatPoint(100, 100)));

  ASSERT_EQ(callback->mouseEventCount(), 0);
  ASSERT_EQ(callback->touchEventCount(), 0);
  ASSERT_EQ(callback->penEventCount(), 0);

  WebView().HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerCausedUaAction,
                             WebPointerProperties::PointerType::kPen),
      std::vector<WebPointerEvent>(), std::vector<WebPointerEvent>()));
  ASSERT_EQ(callback->mouseEventCount(), 0);
  ASSERT_EQ(callback->touchEventCount(), 1);
  ASSERT_EQ(callback->penEventCount(), 1);

  WebView().HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerCausedUaAction,
                             WebPointerProperties::PointerType::kTouch),
      std::vector<WebPointerEvent>(), std::vector<WebPointerEvent>()));
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
  WebView().Resize(WebSize(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "</body>");
  WebView().SetPageScaleFactor(2);
  PointerEventCoordinateListenerCallback* callback =
      PointerEventCoordinateListenerCallback::Create();
  GetDocument().body()->addEventListener(EventTypeNames::pointerdown, callback);

  WebView().HandleInputEvent(WebCoalescedInputEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerDown,
                             WebPointerProperties::PointerType::kTouch,
                             WebFloatPoint(150, 200), WebFloatPoint(100, 50),
                             10, 10, 16, 24),
      std::vector<WebPointerEvent>(), std::vector<WebPointerEvent>()));

  ASSERT_EQ(callback->last_client_x_, 75);
  ASSERT_EQ(callback->last_client_y_, 100);
  ASSERT_EQ(callback->last_page_x_, 75);
  ASSERT_EQ(callback->last_page_y_, 100);
  ASSERT_EQ(callback->last_screen_x_, 100);
  ASSERT_EQ(callback->last_screen_y_, 50);
  ASSERT_EQ(callback->last_width_, 8);
  ASSERT_EQ(callback->last_height_, 12);
}

}  // namespace blink
