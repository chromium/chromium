// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "remoting/client/input/normalizing_input_filter_win.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using remoting::protocol::InputStub;
using remoting::protocol::KeyEvent;
using remoting::protocol::MockInputStub;
using remoting::protocol::MouseEvent;

namespace remoting {

namespace {

const unsigned int kUsbKeyA = 0x070004;
const unsigned int kUsbLeftControl = 0x0700e0;
const unsigned int kUsbRightAlt = 0x0700e6;

// A hardcoded value used to verify |lock_states| is preserved.
static const uint32_t kTestLockStates = protocol::KeyEvent::LOCK_STATES_NUMLOCK;

MATCHER_P2(EqualsKeyEvent, usb_keycode, pressed, "") {
  return arg.usb_keycode() == static_cast<uint32_t>(usb_keycode) &&
         arg.pressed() == pressed && arg.lock_states() == kTestLockStates;
}

KeyEvent MakeKeyEvent(uint32_t keycode, bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(keycode);
  event.set_pressed(pressed);
  event.set_lock_states(kTestLockStates);
  return event;
}

void PressAndReleaseKey(InputStub* input_stub, uint32_t keycode) {
  input_stub->InjectKeyEvent(MakeKeyEvent(keycode, true));
  input_stub->InjectKeyEvent(MakeKeyEvent(keycode, false));
}

MATCHER_P2(EqualsMouseMoveEvent, x, y, "") {
  return arg.x() == x && arg.y() == y;
}

static MouseEvent MakeMouseMoveEvent(int x, int y) {
  MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  return event;
}

}  // namespace

// Test press/release of LeftControl, RightAlt, then LeftControl again.
TEST(NormalizingInputFilterWinTest, PressReleaseSequence) {
  MockInputStub stub;
  NormalizingInputFilterWin processor(&stub);

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbLeftControl, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbLeftControl, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbLeftControl, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbLeftControl, false)));
  }

  // Inject press & release events.
  PressAndReleaseKey(&processor, kUsbLeftControl);
  PressAndReleaseKey(&processor, kUsbRightAlt);
  PressAndReleaseKey(&processor, kUsbLeftControl);
}

// Test LeftControl key repeat causes it to be treated as LeftControl.
TEST(NormalizingInputFilterWinTest, LeftControlRepeats) {
  MockInputStub stub;
  NormalizingInputFilterWin processor(&stub);

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbLeftControl, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbLeftControl, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbLeftControl, false)));
  }

  // Inject a press and repeats for LeftControl, and verify that the repeats
  // result in press events.
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, true));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, true));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, false));
}

// Test LeftControl key pressed while pressing & releasing a printable key and
// then RightAlt treats it as LeftControl, not AltGr combo.
TEST(NormalizingInputFilterWinTest, LeftControlAndPrintableKey) {
  MockInputStub stub;
  NormalizingInputFilterWin processor(&stub);

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbLeftControl, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbKeyA, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbKeyA, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbLeftControl, false)));
  }

  // Inject a press for LeftControl, press and release a character key, then
  // press RightAlt, and finally release LeftControl.
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, true));
  PressAndReleaseKey(&processor, kUsbKeyA);
  PressAndReleaseKey(&processor, kUsbRightAlt);
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, false));
}

// Test LeftControl press followed by RightAlt press results in
// it being interpreted as AltGr, so only RightAlt events generated.
// Also press a printable key while AltGr is down to verify that doesn't
// confuse things.
TEST(NormalizingInputFilterWinTest, AltGr) {
  MockInputStub stub;
  NormalizingInputFilterWin processor(&stub);

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbKeyA, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbKeyA, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, false)));
  }

  // Hold LeftControl and then RightAlt, then release.
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, true));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbRightAlt, true));
  PressAndReleaseKey(&processor, kUsbKeyA);
  processor.InjectKeyEvent(MakeKeyEvent(kUsbRightAlt, false));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, false));
}

// Test LeftControl press followed by RightAlt press, and repeats results in
// it being interpreted as AltGr, so only RightAlt events generated.
TEST(NormalizingInputFilterWinTest, AltGrRepeats) {
  MockInputStub stub;
  NormalizingInputFilterWin processor(&stub);

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, false)));
  }

  // Hold LeftControl and then RightAlt, repeat, then release. Sequence
  // reflects the order of events generated by Windows.
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, true));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbRightAlt, true));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, true));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbRightAlt, true));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbRightAlt, false));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, false));
}

// Test LeftControl held during mouse event treats it as LeftControl.
TEST(NormalizingInputFilterWinTest, LeftControlAndMouseEvent) {
  MockInputStub stub;
  NormalizingInputFilterWin processor(&stub);

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbLeftControl, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseMoveEvent(0, 0)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbLeftControl, false)));
  }

  // Hold the left Control while moving the mouse.
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, true));
  processor.InjectMouseEvent(MakeMouseMoveEvent(0, 0));
  PressAndReleaseKey(&processor, kUsbRightAlt);
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, false));
}

// Test LeftControl & RightAlt held during mouse event treats it as AltGr.
TEST(NormalizingInputFilterWinTest, AltGrAndMouseEvent) {
  MockInputStub stub;
  NormalizingInputFilterWin processor(&stub);

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseMoveEvent(0, 0)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEvent(kUsbRightAlt, false)));
  }

  // Hold the left Control while moving the mouse.
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, true));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbRightAlt, true));
  processor.InjectMouseEvent(MakeMouseMoveEvent(0, 0));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbRightAlt, false));
  processor.InjectKeyEvent(MakeKeyEvent(kUsbLeftControl, false));
}

}  // namespace remoting
