// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#include <stdint.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "remoting/proto/internal.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace remoting {

namespace {

using protocol::KeyEvent;
using protocol::MouseEvent;

MouseEvent MakeMouseMoveEvent(int x, int y) {
  MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  return event;
}

MouseEvent MakeMouseButtonEvent(int x,
                                int y,
                                MouseEvent::MouseButton button,
                                bool button_down) {
  MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  event.set_button(button);
  event.set_button_down(button_down);
  return event;
}

KeyEvent MakeKeyEvent(ui::DomCode dom_code, bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(static_cast<uint32_t>(dom_code));
  event.set_pressed(pressed);
  return event;
}

class InputInjectorMacTest : public testing::Test {
 protected:
  void SetUp() override {
    injector_ = std::make_unique<InputInjectorMac>(
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner());
    injector_->SetUseCGEventMouseInjectionForTesting(true);
    injector_->SetCGEventPostFunctionForTesting(base::BindRepeating(
        &InputInjectorMacTest::OnCGEventPosted, base::Unretained(this)));
  }

  void TearDown() override { injector_.reset(); }

  base::apple::ScopedCFTypeRef<CGEventRef> InjectMouseAndTakeCGEvent(
      const MouseEvent& event) {
    injector_->InjectMouseEvent(event);
    return next_event_.Take();
  }

  base::apple::ScopedCFTypeRef<CGEventRef> InjectKeyAndTakeCGEvent(
      const KeyEvent& event) {
    injector_->InjectKeyEvent(event);
    return next_event_.Take();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<InputInjectorMac> injector_;

 private:
  void OnCGEventPosted(CGEventRef event) {
    next_event_.SetValue(base::apple::ScopedCFTypeRef<CGEventRef>(
        event, base::scoped_policy::RETAIN));
  }

  base::test::TestFuture<base::apple::ScopedCFTypeRef<CGEventRef>> next_event_;
};

TEST_F(InputInjectorMacTest, MouseMoveWhenNoButtonPressed) {
  auto cg_event = InjectMouseAndTakeCGEvent(MakeMouseMoveEvent(120, 240));
  EXPECT_EQ(CGEventGetType(cg_event.get()), kCGEventMouseMoved);
  CGPoint location = CGEventGetLocation(cg_event.get());
  EXPECT_EQ(location.x, 120);
  EXPECT_EQ(location.y, 240);
}

TEST_F(InputInjectorMacTest, LeftButtonDragSequenceUsesMatchingEventNumber) {
  auto down1 = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(100, 100, MouseEvent::BUTTON_LEFT, true));
  EXPECT_EQ(CGEventGetType(down1.get()), kCGEventLeftMouseDown);
  EXPECT_EQ(CGEventGetIntegerValueField(down1.get(), kCGMouseEventNumber), 1);
  EXPECT_EQ(CGEventGetIntegerValueField(down1.get(), kCGMouseEventClickState),
            1);

  auto drag1 = InjectMouseAndTakeCGEvent(MakeMouseMoveEvent(150, 120));
  EXPECT_EQ(CGEventGetType(drag1.get()), kCGEventLeftMouseDragged);
  EXPECT_EQ(CGEventGetIntegerValueField(drag1.get(), kCGMouseEventNumber), 1);
  CGPoint drag1_loc = CGEventGetLocation(drag1.get());
  EXPECT_EQ(drag1_loc.x, 150);
  EXPECT_EQ(drag1_loc.y, 120);

  auto drag2 = InjectMouseAndTakeCGEvent(MakeMouseMoveEvent(200, 140));
  EXPECT_EQ(CGEventGetType(drag2.get()), kCGEventLeftMouseDragged);
  EXPECT_EQ(CGEventGetIntegerValueField(drag2.get(), kCGMouseEventNumber), 1);

  auto up1 = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(200, 140, MouseEvent::BUTTON_LEFT, false));
  EXPECT_EQ(CGEventGetType(up1.get()), kCGEventLeftMouseUp);
  EXPECT_EQ(CGEventGetIntegerValueField(up1.get(), kCGMouseEventNumber), 1);

  // Movement after release should be kCGEventMouseMoved.
  auto move = InjectMouseAndTakeCGEvent(MakeMouseMoveEvent(220, 160));
  EXPECT_EQ(CGEventGetType(move.get()), kCGEventMouseMoved);

  // Next button-down increments the event number.
  auto down2 = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(220, 160, MouseEvent::BUTTON_LEFT, true));
  EXPECT_EQ(CGEventGetType(down2.get()), kCGEventLeftMouseDown);
  EXPECT_EQ(CGEventGetIntegerValueField(down2.get(), kCGMouseEventNumber), 2);

  auto drag3 = InjectMouseAndTakeCGEvent(MakeMouseMoveEvent(240, 180));
  EXPECT_EQ(CGEventGetType(drag3.get()), kCGEventLeftMouseDragged);
  EXPECT_EQ(CGEventGetIntegerValueField(drag3.get(), kCGMouseEventNumber), 2);

  auto up2 = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(240, 180, MouseEvent::BUTTON_LEFT, false));
  EXPECT_EQ(CGEventGetType(up2.get()), kCGEventLeftMouseUp);
  EXPECT_EQ(CGEventGetIntegerValueField(up2.get(), kCGMouseEventNumber), 2);
}

TEST_F(InputInjectorMacTest, RightAndMiddleButtonDragSequences) {
  // Right button drag sequence.
  auto right_down = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(50, 50, MouseEvent::BUTTON_RIGHT, true));
  EXPECT_EQ(CGEventGetType(right_down.get()), kCGEventRightMouseDown);
  EXPECT_EQ(CGEventGetIntegerValueField(right_down.get(), kCGMouseEventNumber),
            1);

  auto right_drag = InjectMouseAndTakeCGEvent(MakeMouseMoveEvent(60, 70));
  EXPECT_EQ(CGEventGetType(right_drag.get()), kCGEventRightMouseDragged);
  EXPECT_EQ(CGEventGetIntegerValueField(right_drag.get(), kCGMouseEventNumber),
            1);

  auto right_up = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(60, 70, MouseEvent::BUTTON_RIGHT, false));
  EXPECT_EQ(CGEventGetType(right_up.get()), kCGEventRightMouseUp);
  EXPECT_EQ(CGEventGetIntegerValueField(right_up.get(), kCGMouseEventNumber),
            1);

  // Middle button drag sequence.
  auto middle_down = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(80, 90, MouseEvent::BUTTON_MIDDLE, true));
  EXPECT_EQ(CGEventGetType(middle_down.get()), kCGEventOtherMouseDown);
  EXPECT_EQ(
      CGEventGetIntegerValueField(middle_down.get(), kCGMouseEventButtonNumber),
      kCGMouseButtonCenter);
  EXPECT_EQ(CGEventGetIntegerValueField(middle_down.get(), kCGMouseEventNumber),
            2);

  auto middle_drag = InjectMouseAndTakeCGEvent(MakeMouseMoveEvent(90, 100));
  EXPECT_EQ(CGEventGetType(middle_drag.get()), kCGEventOtherMouseDragged);
  EXPECT_EQ(
      CGEventGetIntegerValueField(middle_drag.get(), kCGMouseEventButtonNumber),
      kCGMouseButtonCenter);
  EXPECT_EQ(CGEventGetIntegerValueField(middle_drag.get(), kCGMouseEventNumber),
            2);

  auto middle_up = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(90, 100, MouseEvent::BUTTON_MIDDLE, false));
  EXPECT_EQ(CGEventGetType(middle_up.get()), kCGEventOtherMouseUp);
  EXPECT_EQ(CGEventGetIntegerValueField(middle_up.get(), kCGMouseEventNumber),
            2);
}

TEST_F(InputInjectorMacTest, DoubleClickAndTripleClick) {
  // First click.
  auto down1 = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(100, 100, MouseEvent::BUTTON_LEFT, true));
  EXPECT_EQ(CGEventGetIntegerValueField(down1.get(), kCGMouseEventClickState),
            1);
  auto up1 = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(100, 100, MouseEvent::BUTTON_LEFT, false));
  EXPECT_EQ(CGEventGetIntegerValueField(up1.get(), kCGMouseEventClickState), 1);

  // Second click within double-click threshold and distance.
  task_environment_.FastForwardBy(base::Milliseconds(150));
  auto down2 = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(102, 99, MouseEvent::BUTTON_LEFT, true));
  EXPECT_EQ(CGEventGetIntegerValueField(down2.get(), kCGMouseEventClickState),
            2);
  auto up2 = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(102, 99, MouseEvent::BUTTON_LEFT, false));
  EXPECT_EQ(CGEventGetIntegerValueField(up2.get(), kCGMouseEventClickState), 2);

  // Third click within threshold and distance.
  task_environment_.FastForwardBy(base::Milliseconds(150));
  auto down3 = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(101, 100, MouseEvent::BUTTON_LEFT, true));
  EXPECT_EQ(CGEventGetIntegerValueField(down3.get(), kCGMouseEventClickState),
            3);
  auto up3 = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(101, 100, MouseEvent::BUTTON_LEFT, false));
  EXPECT_EQ(CGEventGetIntegerValueField(up3.get(), kCGMouseEventClickState), 3);
}

TEST_F(InputInjectorMacTest,
       ClickStateResetsAfterTimeoutDistanceOrButtonChange) {
  auto down1 = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(100, 100, MouseEvent::BUTTON_LEFT, true));
  EXPECT_EQ(CGEventGetIntegerValueField(down1.get(), kCGMouseEventClickState),
            1);
  InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(100, 100, MouseEvent::BUTTON_LEFT, false));

  // Exceed the 500ms double-click time threshold.
  task_environment_.FastForwardBy(base::Milliseconds(600));
  auto down_after_timeout = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(100, 100, MouseEvent::BUTTON_LEFT, true));
  EXPECT_EQ(CGEventGetIntegerValueField(down_after_timeout.get(),
                                        kCGMouseEventClickState),
            1);
  InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(100, 100, MouseEvent::BUTTON_LEFT, false));

  // Click within time threshold, but far away.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  auto down_far_away = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(200, 200, MouseEvent::BUTTON_LEFT, true));
  EXPECT_EQ(
      CGEventGetIntegerValueField(down_far_away.get(), kCGMouseEventClickState),
      1);
  InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(200, 200, MouseEvent::BUTTON_LEFT, false));

  // Click within time and distance threshold, but with a different button.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  auto down_right = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(200, 200, MouseEvent::BUTTON_RIGHT, true));
  EXPECT_EQ(
      CGEventGetIntegerValueField(down_right.get(), kCGMouseEventClickState),
      1);
  InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(200, 200, MouseEvent::BUTTON_RIGHT, false));
}

TEST_F(InputInjectorMacTest, MouseEventsCarryModifierFlags) {
  constexpr uint64_t kModifierMask =
      kCGEventFlagMaskShift | kCGEventFlagMaskCommand |
      kCGEventFlagMaskControl | kCGEventFlagMaskAlternate;

  // Press Shift.
  InjectKeyAndTakeCGEvent(MakeKeyEvent(ui::DomCode::SHIFT_LEFT, true));

  auto shift_down = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(100, 100, MouseEvent::BUTTON_LEFT, true));
  EXPECT_EQ(CGEventGetFlags(shift_down.get()) & kModifierMask,
            kCGEventFlagMaskShift);

  auto shift_up = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(100, 100, MouseEvent::BUTTON_LEFT, false));
  EXPECT_EQ(CGEventGetFlags(shift_up.get()) & kModifierMask,
            kCGEventFlagMaskShift);

  // Also press Right Command while Shift is held.
  InjectKeyAndTakeCGEvent(MakeKeyEvent(ui::DomCode::META_RIGHT, true));

  auto shift_cmd_down = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(100, 100, MouseEvent::BUTTON_LEFT, true));
  EXPECT_EQ(CGEventGetFlags(shift_cmd_down.get()) & kModifierMask,
            kCGEventFlagMaskShift | kCGEventFlagMaskCommand);

  auto shift_cmd_drag = InjectMouseAndTakeCGEvent(MakeMouseMoveEvent(110, 110));
  EXPECT_EQ(CGEventGetFlags(shift_cmd_drag.get()) & kModifierMask,
            kCGEventFlagMaskShift | kCGEventFlagMaskCommand);

  auto shift_cmd_up = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(110, 110, MouseEvent::BUTTON_LEFT, false));
  EXPECT_EQ(CGEventGetFlags(shift_cmd_up.get()) & kModifierMask,
            kCGEventFlagMaskShift | kCGEventFlagMaskCommand);

  // Release Shift and Right Command.
  InjectKeyAndTakeCGEvent(MakeKeyEvent(ui::DomCode::SHIFT_LEFT, false));
  InjectKeyAndTakeCGEvent(MakeKeyEvent(ui::DomCode::META_RIGHT, false));

  auto plain_down = InjectMouseAndTakeCGEvent(
      MakeMouseButtonEvent(110, 110, MouseEvent::BUTTON_LEFT, true));
  EXPECT_EQ(CGEventGetFlags(plain_down.get()) & kModifierMask, 0u);
}

}  // namespace
}  // namespace remoting
