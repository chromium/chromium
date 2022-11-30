// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/normalizing_input_filter_mac.h"

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
using remoting::protocol::test::EqualsKeyEventWithNumLock;

namespace remoting {

namespace {

KeyEvent MakeKeyEvent(ui::DomCode keycode, bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(static_cast<uint32_t>(keycode));
  event.set_pressed(static_cast<int>(pressed));
  event.set_lock_states(KeyEvent::LOCK_STATES_NUMLOCK);
  return event;
}

}  // namespace

// Test CapsLock press/release.
TEST(NormalizingInputFilterMacTest, CapsLock) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    // Verifies the generated CapsLock up/down events.
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::CAPS_LOCK, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::CAPS_LOCK, false)));
  }

  // Injecting a CapsLock down event with NumLock on.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::CAPS_LOCK, true));
}

// Test without pressing command key.
TEST(NormalizingInputFilterMacTest, NoInjection) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, false)));
  }

  // C Down and C Up.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::US_C, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::US_C, false));
}

// Test pressing command key and other normal keys.
TEST(NormalizingInputFilterMacTest, CmdKey) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    // Left command key.
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, false)));

    // Right command key.
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_RIGHT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_RIGHT, false)));

    // More than one keys after CMD.
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_RIGHT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_V, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, false)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_V, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_RIGHT, false)));
  }

  // Left command key.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::US_C, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, false));

  // Right command key.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_RIGHT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::US_C, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_RIGHT, false));

  // More than one keys after CMD.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_RIGHT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::US_C, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::US_V, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_RIGHT, false));
}

// Test pressing command and special keys.
TEST(NormalizingInputFilterMacTest, SpecialKeys) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    // Command + Shift.
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::SHIFT_LEFT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::SHIFT_LEFT, false)));

    // Command + Option.
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::ALT_LEFT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::ALT_LEFT, false)));
  }

  // Command + Shift.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::SHIFT_LEFT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, false));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::SHIFT_LEFT, false));

  // Command + Option.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::ALT_LEFT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, false));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::ALT_LEFT, false));
}

// Test pressing multiple command keys.
TEST(NormalizingInputFilterMacTest, MultipleCmdKeys) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_RIGHT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_LEFT, false)));
  }

  // Test multiple CMD keys at the same time.
  // L CMD Down, C Down, R CMD Down, L CMD Up.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::US_C, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_RIGHT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_LEFT, false));
}

// Test press C key before command key.
TEST(NormalizingInputFilterMacTest, BeforeCmdKey) {
  MockInputStub stub;
  std::unique_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_RIGHT, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock(
                          ui::DomCode::META_RIGHT, false)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(ui::DomCode::US_C, false)));
  }

  // Press C before command key.
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::US_C, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_RIGHT, true));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::META_RIGHT, false));
  processor->InjectKeyEvent(MakeKeyEvent(ui::DomCode::US_C, false));
}

}  // namespace remoting
