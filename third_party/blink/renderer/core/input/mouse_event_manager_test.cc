// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/mouse_event_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

namespace {
// Long enough to ensure scroll animation should be complete.
const double kScrollAnimationDuration = 100.0;
}  // namespace

class MouseEventManagerTest : public SimTest {
 protected:
  EventHandler& GetEventHandler() {
    return GetDocument().GetFrame()->GetEventHandler();
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

  void SendKeyDown(int key) {
    WebKeyboardEvent web_event = {WebInputEvent::Type::kRawKeyDown,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests()};
    web_event.windows_key_code = key;
    KeyboardEvent* event = KeyboardEvent::Create(web_event, nullptr);
    event->SetTarget(&GetDocument());
    GetDocument().GetFrame()->GetEventHandler().DefaultKeyboardEventHandler(
        event);
  }
};

// TODO(crbug.com/1325058): Re-enable this test
TEST_F(MouseEventManagerTest, DISABLED_MousePressNodeRemoved) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        #scroller {
          overflow: auto;
          height: 100px;
        }
        #target {
          width: 100px;
          height: 100px;
          background: green;
        }
        .spacer, body {
          height: 200vh;
        }
      </style>
      <body>
        <div id="scroller">
          <div id="target"></div>
          <div class="spacer"></div>
        </div>
      </body>
      )HTML");
  Compositor().BeginFrame();
  EXPECT_FLOAT_EQ(
      GetDocument().getElementById(AtomicString("scroller"))->scrollTop(), 0.0);

  // Click on the target node to set the mouse_press_node_.
  GetEventHandler().HandleMousePressEvent(CreateTestMouseEvent(
      WebInputEvent::Type::kMouseDown, gfx::PointF(50, 50)));

  // Now remove this node.
  GetDocument().getElementById(AtomicString("target"))->remove();
  Compositor().BeginFrame();

  // Now press the down key. This should still scroll the nested scroller as it
  // was still the scroller that was clicked in.
  SendKeyDown(VKEY_DOWN);
  Compositor().ResetLastFrameTime();
  // Start scroll animation.
  Compositor().BeginFrame();
  // Jump to end of scroll animation.
  Compositor().BeginFrame(kScrollAnimationDuration);
  EXPECT_GT(GetDocument().getElementById(AtomicString("scroller"))->scrollTop(),
            0.0);
}

}  // namespace blink
