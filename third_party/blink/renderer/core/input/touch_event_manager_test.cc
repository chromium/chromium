// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/touch_event_manager.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
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
                             gfx::PointF(100, 100), gfx::PointF(100, 100)),
        1, 1);
    event.SetFrameScale(1);
    return event;
  }
};

class CheckEventListenerCallback final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override {
    event_received_ = true;
  }

  bool HasReceivedEvent() const { return event_received_; }

 private:
  bool event_received_ = false;
};

TEST_F(TouchEventManagerTest, LostTouchDueToInnerIframeRemove) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <body style='padding: 0px; width: 400px; height: 400px;'>
    <iframe id='target' style='width: 200px; height: 200px;'></iframe>
    </body>
  )HTML");
  auto* callback = MakeGarbageCollected<CheckEventListenerCallback>();
  GetDocument().body()->addEventListener(event_type_names::kTouchstart,
                                         callback);

  GetEventHandler().HandlePointerEvent(
      CreateTouchPointerEvent(WebInputEvent::Type::kPointerDown),
      Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
  GetEventHandler().DispatchBufferedTouchEvents();

  GetDocument().getElementById(AtomicString("target"))->remove();

  GetEventHandler().HandlePointerEvent(
      CreateTouchPointerEvent(WebInputEvent::Type::kPointerUp),
      Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
  GetEventHandler().DispatchBufferedTouchEvents();

  GetEventHandler().HandlePointerEvent(
      CreateTouchPointerEvent(WebInputEvent::Type::kPointerDown),
      Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
  GetEventHandler().DispatchBufferedTouchEvents();

  ASSERT_TRUE(callback->HasReceivedEvent());
}

TEST_F(TouchEventManagerTest, AbosolutePosWithScrollAndZoom) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <body style='width: 1600px; height: 1600px;'>
    <input type='range' id='slideElement' value=0 style='
      position: absolute; left:100px; top:100px; width:200px; height:200px;'>
    </body>
  )HTML");
  GetDocument().GetFrame()->SetLayoutZoomFactor(2);
  Window().scrollTo(100, 100);

  GetEventHandler().HandlePointerEvent(
      CreateTouchPointerEvent(WebInputEvent::Type::kPointerDown),
      Vector<WebPointerEvent>(), Vector<WebPointerEvent>());
  GetEventHandler().DispatchBufferedTouchEvents();

  auto* input = To<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("slideElement")));
  // Allow off by 1 error because it may result in different value in some
  // platform.
  EXPECT_NEAR(23, ParseToDoubleForNumberType(input->Value()), 1);
}

}  // namespace blink
