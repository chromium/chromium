// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/mouse_event_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

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

TEST_F(MouseEventManagerTest, HoverEffectAfterNav) {
  LocalFrame* frame = MainFrame().GetFrame();

  // With this feature enabled, RecomputeMouseHoverState() fires synthetic
  // mouse events for inactive pages. The SetFocused call remains as a fallback
  // in case the feature needs to be disabled.
  // See crbug.com/385474535 for more details.
  if (!RuntimeEnabledFeatures::SyntheticMouseHoverOverInactivePageEnabled()) {
    GetPage().SetFocused(true);
  }

  // This mousemove sets last_known_mouse_position_ before we navigate.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseEvent(WebInputEvent::Type::kMouseMove,
                           gfx::PointF(20, 20)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Perform two navigations, one from the initial empty document, then another
  // to a document with a hover effect.
  {
    SimRequest request1("https://example.com/page1.html", "text/html");
    LoadURL("https://example.com/page1.html");
    request1.Complete("<html></html>");
    Compositor().BeginFrame();
  }

  SimRequest request2("https://example.com/page2.html", "text/html");
  LoadURL("https://example.com/page2.html");

  request2.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 10px; }
      #b { width: 20px; height: 20px; background: gray; }
      #b:hover { background: red; } </style>
    <div id=b></div>
  )HTML");

  // These navigations swap new documents into the existing LocalFrame.
  EXPECT_EQ(frame, MainFrame().GetFrame());

  LayoutObject* b =
      GetDocument().getElementById(AtomicString("b"))->GetLayoutObject();

  // We need the first frame to layout before we can hit test the mouse pos.
  Compositor().BeginFrame();

  // The second frame applies the hover effect. We have to force a new frame
  // using SetNeedsCommit in the test, but in production we can count on
  // ProxyImpl::NotifyReadyToCommitOnImpl to schedule it (see comments there).
  GetWebFrameWidget().LayerTreeHostForTesting()->SetNeedsCommit();
  Compositor().BeginFrame();

  Color color =
      b->Style()->VisitedDependentColor(GetCSSPropertyBackgroundColor());
  EXPECT_EQ("rgb(255, 0, 0)", color.SerializeAsCSSColor());
}

TEST_F(MouseEventManagerTest,
       RecomputeMouseHoverStateForActiveAndInactivePage) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 10px; }
      #target { width: 20px; height: 20px; background: gray; }
      #target:hover { background: red; } </style>
    <div id=target></div>
  )HTML");
  Compositor().BeginFrame();

  // Test hover over target element on active page
  GetPage().SetActive(true);

  // Set the mouse position over the target element
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseEvent(WebInputEvent::Type::kMouseMove,
                           gfx::PointF(20, 20)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  GetEventHandler().MarkHoverStateDirty();
  GetEventHandler().RecomputeMouseHoverStateIfNeeded();

  LayoutObject* target =
      GetDocument().getElementById(AtomicString("target"))->GetLayoutObject();
  Color hover_color =
      target->Style()->VisitedDependentColor(GetCSSPropertyBackgroundColor());
  // :hover pseudo-class should match when the pointer is over the element.
  EXPECT_EQ("rgb(255, 0, 0)", hover_color.SerializeAsCSSColor());

  // Move mouse position away from the target element to reset target background
  // color to gray.
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseEvent(WebInputEvent::Type::kMouseMove,
                           gfx::PointF(200, 200)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  GetEventHandler().MarkHoverStateDirty();
  GetEventHandler().RecomputeMouseHoverStateIfNeeded();
  hover_color =
      target->Style()->VisitedDependentColor(GetCSSPropertyBackgroundColor());
  EXPECT_EQ("rgb(128, 128, 128)", hover_color.SerializeAsCSSColor());

  // Move mouse back over element but with page inactive.
  GetPage().SetActive(false);
  GetEventHandler().HandleMouseMoveEvent(
      CreateTestMouseEvent(WebInputEvent::Type::kMouseMove,
                           gfx::PointF(20, 20)),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  GetEventHandler().MarkHoverStateDirty();
  GetEventHandler().RecomputeMouseHoverStateIfNeeded();
  hover_color =
      target->Style()->VisitedDependentColor(GetCSSPropertyBackgroundColor());
  // Same behavior is expected regardless of Page Active state. Per
  // W3C Pointer Events, § 4.4.6 “mouseenter”
  EXPECT_EQ("rgb(255, 0, 0)", hover_color.SerializeAsCSSColor());
}
}  // namespace blink
