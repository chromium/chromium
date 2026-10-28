// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_keyboard_input_monitor_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <unistd.h>

#include <cstdint>
#include <optional>

#include "base/apple/scoped_cftyperef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace remoting {
namespace {

base::apple::ScopedCFTypeRef<CGEventRef> CreateKeyboardCGEvent(
    CGEventType type,
    CGKeyCode keycode,
    bool key_down,
    int64_t source_pid,
    CGEventFlags flags = 0) {
  base::apple::ScopedCFTypeRef<CGEventRef> event(
      CGEventCreateKeyboardEvent(nullptr, keycode, key_down));
  EXPECT_TRUE(event);
  CGEventSetType(event.get(), type);
  CGEventSetFlags(event.get(), flags);
  CGEventSetIntegerValueField(event.get(), kCGEventSourceUnixProcessID,
                              source_pid);
  EXPECT_EQ(
      CGEventGetIntegerValueField(event.get(), kCGEventSourceUnixProcessID),
      source_pid);
  return event;
}

TEST(LocalKeyboardInputMonitorMacTest,
     HardwareKeyDownAndKeyUpEventsAreDetected) {
  // kVK_ANSI_A has virtual keycode 0x00 and must be recognized as genuine
  // local input on normal key-down and key-up events.
  auto key_down = CreateKeyboardCGEvent(kCGEventKeyDown, kVK_ANSI_A,
                                        /*key_down=*/true, /*source_pid=*/0);
  EXPECT_EQ(GetLocalKeyboardEventUsbKeycode(kCGEventKeyDown, key_down.get()),
            static_cast<uint32_t>(ui::DomCode::US_A));

  auto key_up = CreateKeyboardCGEvent(kCGEventKeyUp, kVK_ANSI_A,
                                      /*key_down=*/false, /*source_pid=*/0);
  EXPECT_EQ(GetLocalKeyboardEventUsbKeycode(kCGEventKeyUp, key_up.get()),
            static_cast<uint32_t>(ui::DomCode::US_A));

  auto tab_down = CreateKeyboardCGEvent(kCGEventKeyDown, kVK_Tab,
                                        /*key_down=*/true, /*source_pid=*/0);
  EXPECT_EQ(GetLocalKeyboardEventUsbKeycode(kCGEventKeyDown, tab_down.get()),
            static_cast<uint32_t>(ui::DomCode::TAB));
}

TEST(LocalKeyboardInputMonitorMacTest, FlagsChangedEventsAreIgnored) {
  // When switching active applications (e.g. after remote Cmd+Tab), macOS
  // WindowServer synthesizes a kCGEventFlagsChanged event with pid == 0 and
  // keycode == 0 (kVK_ANSI_A).
  auto app_switch_sync = CreateKeyboardCGEvent(
      kCGEventFlagsChanged, kVK_ANSI_A, /*key_down=*/false,
      /*source_pid=*/0, static_cast<CGEventFlags>(0x20000000));
  EXPECT_EQ(GetLocalKeyboardEventUsbKeycode(kCGEventFlagsChanged,
                                            app_switch_sync.get()),
            std::nullopt);

  // Modifier kCGEventFlagsChanged events are also ignored; genuine local
  // keyboard activity is detected via kCGEventKeyDown and kCGEventKeyUp.
  constexpr CGKeyCode kModifierKeycodes[] = {
      kVK_Command,      kVK_RightCommand, kVK_Shift,       kVK_RightShift,
      kVK_CapsLock,     kVK_Option,       kVK_RightOption, kVK_Control,
      kVK_RightControl, kVK_Function,
  };
  for (CGKeyCode keycode : kModifierKeycodes) {
    SCOPED_TRACE(keycode);
    auto event = CreateKeyboardCGEvent(kCGEventFlagsChanged, keycode,
                                       /*key_down=*/true, /*source_pid=*/0);
    EXPECT_EQ(
        GetLocalKeyboardEventUsbKeycode(kCGEventFlagsChanged, event.get()),
        std::nullopt);
  }
}

TEST(LocalKeyboardInputMonitorMacTest,
     CrdInjectedEventsFromCurrentProcessAreIgnored) {
  const int64_t self_pid = getpid();

  auto key_down = CreateKeyboardCGEvent(kCGEventKeyDown, kVK_ANSI_A,
                                        /*key_down=*/true, self_pid);
  EXPECT_EQ(GetLocalKeyboardEventUsbKeycode(kCGEventKeyDown, key_down.get()),
            std::nullopt);

  auto key_up = CreateKeyboardCGEvent(kCGEventKeyUp, kVK_Tab,
                                      /*key_down=*/false, self_pid);
  EXPECT_EQ(GetLocalKeyboardEventUsbKeycode(kCGEventKeyUp, key_up.get()),
            std::nullopt);
}

TEST(LocalKeyboardInputMonitorMacTest,
     AssistiveSoftwareEventsFromOtherProcessAreDetected) {
  const int64_t other_pid = getpid() + 1;

  auto key_down = CreateKeyboardCGEvent(kCGEventKeyDown, kVK_Return,
                                        /*key_down=*/true, other_pid);
  EXPECT_EQ(GetLocalKeyboardEventUsbKeycode(kCGEventKeyDown, key_down.get()),
            static_cast<uint32_t>(ui::DomCode::ENTER));

  auto key_up = CreateKeyboardCGEvent(kCGEventKeyUp, kVK_Return,
                                      /*key_down=*/false, other_pid);
  EXPECT_EQ(GetLocalKeyboardEventUsbKeycode(kCGEventKeyUp, key_up.get()),
            static_cast<uint32_t>(ui::DomCode::ENTER));

  auto flags_changed = CreateKeyboardCGEvent(kCGEventFlagsChanged, kVK_Command,
                                             /*key_down=*/true, other_pid);
  EXPECT_EQ(GetLocalKeyboardEventUsbKeycode(kCGEventFlagsChanged,
                                            flags_changed.get()),
            std::nullopt);
}

TEST(LocalKeyboardInputMonitorMacTest, NonKeyboardAndNullEventsAreIgnored) {
  EXPECT_EQ(GetLocalKeyboardEventUsbKeycode(kCGEventKeyDown, nullptr),
            std::nullopt);

  auto event = CreateKeyboardCGEvent(kCGEventKeyDown, kVK_ANSI_A,
                                     /*key_down=*/true, /*source_pid=*/0);
  EXPECT_EQ(GetLocalKeyboardEventUsbKeycode(kCGEventMouseMoved, event.get()),
            std::nullopt);
}

}  // namespace
}  // namespace remoting
