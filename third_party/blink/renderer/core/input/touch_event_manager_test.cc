// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/touch_event_manager.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class TouchEventManagerTest : public SimTest {
 protected:
  EventHandler& GetEventHandler() {
    return GetDocument().GetFrame()->GetEventHandler();
  }

  WebPointerEvent CreateTouchPointerEvent(WebInputEvent::Type type) {
    WebPointerEvent event(
        type,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             WebFloatPoint(100, 100), WebFloatPoint(100, 100)),
        1, 1);
    event.SetFrameScale(1);
    return event;
  }
};

class CheckEventListenerCallback final : public EventListener {
 public:
  static CheckEventListenerCallback* Create() {
    return new CheckEventListenerCallback();
  }
  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
    event_received_ = true;
  }

  bool HasReceivedEvent() const { return event_received_; }

 private:
  CheckEventListenerCallback()
      : EventListener(EventListener::kCPPEventListenerType) {
    event_received_ = false;
  }
  bool event_received_;
};

TEST_F(TouchEventManagerTest, LostTouchDueToInnerIframeRemove) {
  WebView().Resize(WebSize(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <body style='padding: 0px; width: 400px; height: 400px;'>
    <iframe id='target' style='width: 200px; height: 200px;'></iframe>
    </body>
  )HTML");
  CheckEventListenerCallback* callback = CheckEventListenerCallback::Create();
  GetDocument().body()->addEventListener(EventTypeNames::touchstart, callback);

  GetEventHandler().HandlePointerEvent(
      CreateTouchPointerEvent(WebInputEvent::kPointerDown),
      Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
  GetEventHandler().DispatchBufferedTouchEvents();

  GetDocument().getElementById("target")->remove();

  GetEventHandler().HandlePointerEvent(
      CreateTouchPointerEvent(WebInputEvent::kPointerUp),
      Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
  GetEventHandler().DispatchBufferedTouchEvents();

  GetEventHandler().HandlePointerEvent(
      CreateTouchPointerEvent(WebInputEvent::kPointerDown),
      Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
  GetEventHandler().DispatchBufferedTouchEvents();

  ASSERT_TRUE(callback->HasReceivedEvent());
}

}  // namespace blink
