// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/normalizing_input_filter_cros.h"

#include <stdint.h>

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"

using ::testing::InSequence;
using remoting::protocol::InputStub;
using remoting::protocol::KeyEvent;
using remoting::protocol::MockInputStub;
using remoting::protocol::MouseEvent;
using remoting::protocol::test::EqualsKeyEvent;
using remoting::protocol::test::EqualsKeyEventWithNumLock;
using remoting::protocol::test::EqualsMouseButtonEvent;
using remoting::protocol::test::EqualsMouseMoveEvent;

namespace remoting {

namespace {

KeyEvent MakeKeyEvent(ui::DomCode keycode, bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(static_cast<uint32_t>(keycode));
  event.set_pressed(pressed);
  event.set_lock_states(protocol::KeyEvent::LOCK_STATES_NUMLOCK);
  return event;
}

void PressAndReleaseKey(InputStub* input_stub, ui::DomCode keycode) {
  input_stub->InjectKeyEvent(MakeKeyEvent(keycode, true));
  input_stub->InjectKeyEvent(MakeKeyEvent(keycode, false));
}

static MouseEvent MakeMouseMoveEvent(int x, int y) {
  MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  return event;
}

static MouseEvent MakeMouseButtonEvent(MouseEvent::MouseButton button,
                                       bool button_down) {
  MouseEvent event;
  event.set_button(button);
  event.set_button_down(button_down);
  return event;
}

}  // namespace

// Test OSKey press/release.
TEST(NormalizingInputFilterCrosTest, PressReleaseOsKey) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, false)));

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_RIGHT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_RIGHT, false)));
  }

  // Inject press & release events for left & right OSKeys.
  PressAndReleaseKey(processor.get(), ui::DomCode::META_LEFT);
  PressAndReleaseKey(processor.get(), ui::DomCode::META_RIGHT);
}

// Test OSKey key repeat switches it to "modifying" mode.
TEST(NormalizingInputFilterCrosTest, OSKeyRepeats) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, true)));
  }

  // Inject a press and repeats for the left OSKey, but don't release it, and
  // verify that the repeats result in press events.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
}

// Test OSKey press followed by function key press and release results in
// just the function key events.
TEST(NormalizingInputFilterCrosTest, FunctionKey) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(ui::DomCode::F1, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::F1, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
  PressAndReleaseKey(processor.get(), ui::DomCode::F1);
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, false));
}

// Test OSKey press followed by extended key press and release results in
// just the function key events.
TEST(NormalizingInputFilterCrosTest, ExtendedKey) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::INSERT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::INSERT, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
  PressAndReleaseKey(processor.get(), ui::DomCode::INSERT);
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, false));
}

// Test OSKey press followed by non-function, non-extended key press and release
// results in normal-looking sequence. We use the Tab key arbitrarily for this
// test.
TEST(NormalizingInputFilterCrosTest, OtherKey) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::TAB, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::TAB, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
  PressAndReleaseKey(processor.get(), ui::DomCode::TAB);
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, false));
}

// Test OSKey press followed by extended key press, then normal key press
// results in OSKey switching to modifying mode for the normal key.
TEST(NormalizingInputFilterCrosTest, ExtendedThenOtherKey) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::INSERT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::INSERT, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::TAB, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::TAB, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
  PressAndReleaseKey(processor.get(), ui::DomCode::INSERT);
  PressAndReleaseKey(processor.get(), ui::DomCode::TAB);
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, false));
}

// Test OSKey press followed by mouse event puts the OSKey into modifying mode.
TEST(NormalizingInputFilterCrosTest, MouseEvent) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseMoveEvent(0, 0)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
  processor->InjectMouseEvent(MakeMouseMoveEvent(0, 0));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, false));
}

// Test left alt + right click is remapped to left alt + left click.
TEST(NormalizingInputFilterCrosTest, LeftAltClick) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::ALT_LEFT, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_LEFT, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_LEFT, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::ALT_LEFT, false)));
  }

  // Hold the left alt key while left-clicking. ChromeOS will rewrite this as
  // Alt+RightClick
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::ALT_LEFT, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, false));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::ALT_LEFT, false));
}

// Test that right alt + right click is unchanged.
TEST(NormalizingInputFilterCrosTest, RightAltClick) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::ALT_RIGHT, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_RIGHT, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_RIGHT, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::ALT_RIGHT, false)));
  }

  // Hold the right alt key while left-clicking. ChromeOS will rewrite this as
  // Alt+RightClick
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::ALT_RIGHT, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, false));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::ALT_RIGHT, false));
}

// Test that the Alt-key remapping for Up and Down is not applied.
TEST(NormalizingInputFilterCrosTest, UndoAltPlusArrowRemapping) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ALT_LEFT, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ARROW_UP, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ARROW_UP, false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ARROW_DOWN, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ARROW_DOWN, false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::BACKSPACE, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::BACKSPACE, false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ALT_LEFT, false)));

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ALT_RIGHT, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ARROW_UP, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ARROW_UP, false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ARROW_DOWN, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ARROW_DOWN, false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::BACKSPACE, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::BACKSPACE, false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEvent(ui::DomCode::ALT_RIGHT, false)));
  }

  // Hold the left Alt key while pressing & releasing the PgUp, PgDown and
  // Delete keys. This simulates the mapping that ChromeOS applies if the Up,
  // Down and Backspace keys are pressed, respectively, while the Alt key is
  // held.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::ALT_LEFT, true));
  PressAndReleaseKey(processor.get(), ui::DomCode::PAGE_UP);
  PressAndReleaseKey(processor.get(), ui::DomCode::PAGE_DOWN);
  PressAndReleaseKey(processor.get(), ui::DomCode::DEL);
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::ALT_LEFT, false));

  // Repeat the test for the right Alt key.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::ALT_RIGHT, true));
  PressAndReleaseKey(processor.get(), ui::DomCode::PAGE_UP);
  PressAndReleaseKey(processor.get(), ui::DomCode::PAGE_DOWN);
  PressAndReleaseKey(processor.get(), ui::DomCode::DEL);
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::ALT_RIGHT, false));
}

}  // namespace remoting
