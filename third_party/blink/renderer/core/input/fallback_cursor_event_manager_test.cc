// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/fallback_cursor_event_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace {

constexpr size_t kLeft = 0;
constexpr size_t kRight = 1;
constexpr size_t kUp = 2;
constexpr size_t kDown = 3;

}  //  namespace

#define ExpectLock(l, r, u, d)                                        \
  EXPECT_EQ(GetFallbackCursorChromeClient().cursor_lock_[kLeft], l);  \
  EXPECT_EQ(GetFallbackCursorChromeClient().cursor_lock_[kRight], r); \
  EXPECT_EQ(GetFallbackCursorChromeClient().cursor_lock_[kUp], u);    \
  EXPECT_EQ(GetFallbackCursorChromeClient().cursor_lock_[kDown], d);

namespace blink {

class FallbackCursorChromeClient : public RenderingTestChromeClient {
 public:
  FallbackCursorChromeClient() {}

  void FallbackCursorModeLockCursor(LocalFrame* frame,
                                    bool left,
                                    bool right,
                                    bool up,
                                    bool down) override {
    cursor_lock_[0] = left;
    cursor_lock_[1] = right;
    cursor_lock_[2] = up;
    cursor_lock_[3] = down;
  }

  void FallbackCursorModeSetCursorVisibility(LocalFrame* frame,
                                             bool visible) override {
    cursor_visible_ = visible;
  }

  bool cursor_lock_[4] = {0};
  bool cursor_visible_ = true;

 private:
  DISALLOW_COPY_AND_ASSIGN(FallbackCursorChromeClient);
};

class FallbackCursorEventManagerTest : public RenderingTest,
                                       private ScopedFallbackCursorModeForTest {
 protected:
  FallbackCursorEventManagerTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        ScopedFallbackCursorModeForTest(true),
        chrome_client_(MakeGarbageCollected<FallbackCursorChromeClient>()) {}

  ~FallbackCursorEventManagerTest() override {}

  RenderingTestChromeClient& GetChromeClient() const override {
    return *chrome_client_;
  }

  FallbackCursorChromeClient& GetFallbackCursorChromeClient() const {
    return *chrome_client_;
  }

  void TurnOnFallbackCursorMode() {
    GetDocument().GetFrame()->GetEventHandler().SetIsFallbackCursorModeOn(true);
  }

  void MouseMove(int x, int y, float scale = 1.0f) {
    WebMouseEvent event(WebInputEvent::kMouseMove, WebFloatPoint(x, y),
                        WebFloatPoint(x, y),
                        WebPointerProperties::Button::kNoButton, 0,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now());
    event.SetFrameScale(scale);
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  }

  // Simulates a mouse move at the given point in the visual viewport (i.e. the
  // coordinates relative to the Chrome window).
  // TODO(bokan): Replace all above uses with this method.
  void MouseMoveViewport(IntPoint point) {
    VisualViewport& visual_viewport =
        GetDocument().GetPage()->GetVisualViewport();
    FloatPoint root_frame_point =
        visual_viewport.ViewportToRootFrame(FloatPoint(point));

    WebMouseEvent event(WebInputEvent::kMouseMove, root_frame_point,
                        root_frame_point,
                        WebPointerProperties::Button::kNoButton, 0,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now());
    event.SetFrameScale(1.0f);
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  }

  void MouseDown(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::kMouseDown, WebFloatPoint(x, y), WebFloatPoint(x, y),
        WebPointerProperties::Button::kLeft, 0,
        WebInputEvent::Modifiers::kLeftButtonDown, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);
  }

  bool KeyBack() {
    return GetDocument()
        .GetFrame()
        ->GetEventHandler()
        .HandleFallbackCursorModeBackEvent();
  }

 private:
  Persistent<FallbackCursorChromeClient> chrome_client_;

  DISALLOW_COPY_AND_ASSIGN(FallbackCursorEventManagerTest);
};

TEST_F(FallbackCursorEventManagerTest, RootFrameNotScrollable) {
  SetBodyInnerHTML("A");
  TurnOnFallbackCursorMode();

  // Mouse move to edge.
  MouseMove(0, 0);
  ExpectLock(false, false, false, false);

  MouseMove(0, 600);
  ExpectLock(false, false, false, false);

  MouseMove(800, 0);
  ExpectLock(false, false, false, false);

  MouseMove(800, 600);
  ExpectLock(false, false, false, false);
}

TEST_F(FallbackCursorEventManagerTest, ResetOnOutOfFrame) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();

  // Move below the scroll down line.
  MouseMove(100, 500);
  ExpectLock(false, false, false, true);

  // Ensure an invalid or out-of-bounds mouse move will reset the lock.
  MouseMove(-1, -1);
  ExpectLock(false, false, false, false);

  // Ensure an invalid or out-of-bounds mouse move will reset the lock.
  MouseMove(790, 590);
  ExpectLock(false, true, false, true);

  // Ensure an invalid or out-of-bounds mouse move will reset the lock.
  MouseMove(800, 600);
  ExpectLock(false, false, false, false);
}

TEST_F(FallbackCursorEventManagerTest, MouseMoveCursorLockOnRootFrame) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();

  // Move below the scroll down line.
  MouseMove(100, 500);
  ExpectLock(false, false, false, true);

  // Move above the scroll down line.
  MouseMove(100, 400);
  ExpectLock(false, false, false, false);

  // Move to the right of scroll right line.
  MouseMove(600, 400);
  ExpectLock(false, true, false, false);
}

TEST_F(FallbackCursorEventManagerTest,
       MouseMoveCursorLockOnRootFrameWithScale) {
  const float SCALE = 0.5f;
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();

  // Move below the scroll down line.
  MouseMove(50, 250, SCALE);
  ExpectLock(false, false, false, true);

  // Move above the scroll down line.
  MouseMove(50, 200, SCALE);
  ExpectLock(false, false, false, false);

  // Move to the right of scroll right line.
  MouseMove(300, 200, SCALE);
  ExpectLock(false, true, false, false);
}

TEST_F(FallbackCursorEventManagerTest, MouseMoveCursorLockOnDiv) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    #d1 {
      height: 100px;
      width: 100px;
      overflow: auto;
    }
    </style>
    <div id='d1'>
      <div class='big'></div>
    </div>
  )HTML");
  TurnOnFallbackCursorMode();

  // Move below the scroll down line but before mouse down.
  MouseMove(50, 80);
  ExpectLock(false, false, false, false);
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // Mouse down and move lock on down.
  MouseDown(50, 80);
  Element* d1 = GetDocument().getElementById("d1");
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .fallback_cursor_event_manager_->current_node_.Get(),
            d1);
  MouseMove(50, 80);
  ExpectLock(false, false, false, true);

  // Mouse move out of div.
  MouseMove(200, 200);
  ExpectLock(false, false, false, false);
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // key back.
  MouseMove(50, 80);
  MouseDown(50, 80);
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .fallback_cursor_event_manager_->current_node_.Get(),
            d1);
  EXPECT_TRUE(KeyBack());
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);
}

TEST_F(FallbackCursorEventManagerTest, MouseMoveCursorLockOnIFrame) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    #ifr {
      height: 100px;
      width: 100px;
    }
    </style>
    <iframe id='ifr'></iframe>
  )HTML");

  SetChildFrameHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();

  // Move below the scroll down line but before mouse down.
  MouseMove(50, 80);
  ExpectLock(false, false, false, false);
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // Mouse down and move lock on down.
  MouseDown(50, 80);
  MouseMove(50, 80);
  ExpectLock(false, false, false, true);
  Node* child_frame_doc = ChildFrame().GetDocument();
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .fallback_cursor_event_manager_->current_node_.Get(),
            child_frame_doc);

  // Mouse move out of iframe.
  MouseMove(200, 200);
  ExpectLock(false, false, false, false);
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // key back.
  MouseMove(50, 80);
  MouseDown(50, 80);
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .fallback_cursor_event_manager_->current_node_.Get(),
            child_frame_doc);
  EXPECT_TRUE(KeyBack());
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);
}

TEST_F(FallbackCursorEventManagerTest, KeyBackAndMouseMove) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    #ifr {
      height: 100px;
      width: 100px;
    }
    div {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <iframe id='ifr'></iframe>
    <div></div>
  )HTML");

  SetChildFrameHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();

  // Move below the scroll down line but before mouse down.
  MouseMove(50, 80);
  ExpectLock(false, false, false, false);
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // Mouse down and move lock on down.
  MouseDown(50, 80);
  MouseMove(50, 80);
  ExpectLock(false, false, false, true);
  Node* child_frame_doc = ChildFrame().GetDocument();
  EXPECT_EQ(GetDocument()
                .GetFrame()
                ->GetEventHandler()
                .fallback_cursor_event_manager_->current_node_.Get(),
            child_frame_doc);

  // key back.
  EXPECT_TRUE(KeyBack());
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .fallback_cursor_event_manager_->current_node_);

  // Move below the scroll down line of page.
  MouseMove(100, 500);
  ExpectLock(false, false, false, true);
}

TEST_F(FallbackCursorEventManagerTest, MouseDownOnEditor) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    #editor {
      height: 100px;
      width: 100px;
    }
    </style>
    <div id='editor' contenteditable='true'>
    </div>
  )HTML");
  TurnOnFallbackCursorMode();

  MouseMove(50, 80);
  MouseDown(50, 80);

  EXPECT_EQ(GetFallbackCursorChromeClient().cursor_visible_, false);

  Element* editor = GetDocument().getElementById("editor");
  EXPECT_EQ(GetDocument().FocusedElement(), editor);

  EXPECT_TRUE(KeyBack());

  EXPECT_EQ(GetFallbackCursorChromeClient().cursor_visible_, true);
  EXPECT_FALSE(GetDocument().FocusedElement());
}

// Ensure the cursor causes correct locking and scrolling when the web page is
// zoomed in and the visual viewport is offset.
TEST_F(FallbackCursorEventManagerTest, ZoomedIn) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();
  VisualViewport& visual_viewport =
      GetDocument().GetPage()->GetVisualViewport();
  visual_viewport.SetScaleAndLocation(4, /*is_pinch_gesture_active=*/false,
                                      FloatPoint(400, 300));

  ASSERT_EQ(IntSize(800, 600), GetDocument().View()->Size());
  ASSERT_EQ(FloatSize(200, 150), visual_viewport.VisibleRect().Size());

  // Move to the center of the viewport.
  MouseMoveViewport(IntPoint(400, 300));
  ExpectLock(false, false, false, false);

  // Move below the scroll down line.
  MouseMoveViewport(IntPoint(400, 550));
  ExpectLock(false, false, false, true);

  // Move to the left of scroll left line.
  MouseMoveViewport(IntPoint(50, 300));
  ExpectLock(true, false, false, false);
}

// Ensure the cursor causes correct locking in the presence of overflow:hidden.
TEST_F(FallbackCursorEventManagerTest, AccountsForOverflowHidden) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0px;
    }
    html {
      overflow-x: hidden;
    }
    .big {
      height: 10000px;
      width: 10000px;
    }
    </style>
    <div class='big'></div>
  )HTML");
  TurnOnFallbackCursorMode();
  VisualViewport& visual_viewport =
      GetDocument().GetPage()->GetVisualViewport();

  // Start fully zoomed out.
  ASSERT_EQ(IntSize(800, 600), GetDocument().View()->Size());
  ASSERT_EQ(FloatSize(800, 600), visual_viewport.VisibleRect().Size());

  // Move to the center of the viewport.
  MouseMoveViewport(IntPoint(400, 300));
  ExpectLock(false, false, false, false);

  // Move to the right scroll region. We don't expect to lock because the visual
  // viewport has no scroll extent. The layout viewport has scroll extent but
  // is limited by overflow-x:hidden.
  MouseMoveViewport(IntPoint(750, 300));
  ExpectLock(false, false, false, false);

  // Move to the bottom scroll region. Since only overflow-x is hidden, this
  // should cause locking in the down direction.
  MouseMoveViewport(IntPoint(400, 550));
  ExpectLock(false, false, false, true);

  // Now zoom in. Make sure we can still scroll the visual viewport but not the
  // layout.
  visual_viewport.SetScaleAndLocation(4, /*is_pinch_gesture_active=*/false,
                                      FloatPoint(0, 0));
  ASSERT_EQ(IntSize(800, 600), GetDocument().View()->Size());
  ASSERT_EQ(FloatSize(200, 150), visual_viewport.VisibleRect().Size());

  // Move to the right scroll region; since the visual viewport can scroll, we
  // should expect to lock to the right.
  MouseMoveViewport(IntPoint(750, 300));
  ExpectLock(false, true, false, false);

  // Now move the visual viewport to the bottom right corner of the layout
  // viewport.
  visual_viewport.SetScaleAndLocation(4, /*is_pinch_gesture_active=*/false,
                                      FloatPoint(600, 450));

  // Move mouse to the right scroll region. Since the visual viewport is at the
  // extent, and the layout viewport isn't user scrollable, we shouldn't cause
  // locking.
  MouseMoveViewport(IntPoint(750, 350));
  ExpectLock(false, false, false, false);

  // Move the mouse to the bottom scroll region, we should expect to lock
  // because the layout viewport can scroll vertically, even though the visual
  // viewport is at the extent.
  MouseMoveViewport(IntPoint(750, 550));
  ExpectLock(false, false, false, true);

  // Move the mouse to the bottom scroll region, we should expect to lock
  // because the layout viewport can scroll vertically, even though the visual
  // viewport is at the extent.
  MouseMoveViewport(IntPoint(745, 550));
  ExpectLock(false, false, false, true);

  // Fully scroll the layout viewport to the bottom.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 100000), kProgrammaticScroll);

  // Move the mouse to the bottom of the viewport, we shouldn't lock because
  // both layout and visual are at the extent.
  MouseMoveViewport(IntPoint(740, 550));
  ExpectLock(false, false, false, false);
}

TEST_F(FallbackCursorEventManagerTest, NotInCursorMode) {
  GetPage().SetIsCursorVisible(false);
  EXPECT_FALSE(KeyBack());
}

}  // namespace blink
