// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_hotkey_input_monitor.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform/platform_event_source.h"

namespace remoting {

namespace {

class TestPlatformEventSourceHelper : public ui::PlatformEventSource {
 public:
  using ui::PlatformEventSource::DispatchEvent;
};

}  // namespace

class LocalHotkeyInputMonitorChromeOsTest : public testing::Test {
 public:
  LocalHotkeyInputMonitorChromeOsTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    if (!ui::PlatformEventSource::GetInstance()) {
      platform_event_source_ = ui::PlatformEventSource::CreateDefault();
    }
    monitor_ = LocalHotkeyInputMonitor::Create(
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(),
        base::BindOnce(&LocalHotkeyInputMonitorChromeOsTest::OnDisconnect,
                       base::Unretained(this)));
    FlushTaskRunner();
  }

  void TearDown() override {
    monitor_.reset();
    FlushTaskRunner();
    platform_event_source_.reset();
  }

  void OnDisconnect() { disconnect_requested_ = true; }

  bool disconnect_requested() const { return disconnect_requested_; }

  void DispatchKeyEvent(ui::EventType type,
                        ui::KeyboardCode key_code,
                        int flags = ui::EF_NONE,
                        int source_device_id = 1) {
    ui::KeyEvent event(type, key_code, flags);
    event.set_source_device_id(source_device_id);
    static_cast<TestPlatformEventSourceHelper*>(
        ui::PlatformEventSource::GetInstance())
        ->DispatchEvent(&event);
    FlushTaskRunner();
  }

  void FlushTaskRunner() {
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ui::PlatformEventSource> platform_event_source_;
  std::unique_ptr<LocalHotkeyInputMonitor> monitor_;
  bool disconnect_requested_ = false;
};

// Tests that pressing local Ctrl + Alt + Esc initiates a disconnect when event
// flags are stamped.
TEST_F(LocalHotkeyInputMonitorChromeOsTest, CtrlAltEscWithEventFlags) {
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                   ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                   /*source_device_id=*/1);
  EXPECT_TRUE(disconnect_requested());
}

// Tests that holding local Ctrl and Alt then pressing Esc initiates a
// disconnect even if event flags are stripped or missing.
TEST_F(LocalHotkeyInputMonitorChromeOsTest, LocalHeldModifiersDisconnect) {
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_CONTROL, ui::EF_NONE,
                   /*source_device_id=*/1);
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_MENU, ui::EF_NONE,
                   /*source_device_id=*/1);
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE, ui::EF_NONE,
                   /*source_device_id=*/1);
  EXPECT_TRUE(disconnect_requested());
}

// Tests that remote modifier-release events from ED_REMOTE_INPUT_DEVICE do not
// suppress the local emergency disconnect when Ctrl and Alt are held locally.
TEST_F(LocalHotkeyInputMonitorChromeOsTest,
       RemoteModifierReleaseDoesNotSuppressDisconnect) {
  // Local user physically holds Ctrl and Alt.
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_CONTROL, ui::EF_NONE,
                   /*source_device_id=*/1);
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_MENU, ui::EF_NONE,
                   /*source_device_id=*/1);

  // Attacker streams bare modifier-release events from ED_REMOTE_INPUT_DEVICE.
  DispatchKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_CONTROL, ui::EF_NONE,
                   ui::ED_REMOTE_INPUT_DEVICE);
  DispatchKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_MENU, ui::EF_NONE,
                   ui::ED_REMOTE_INPUT_DEVICE);

  // Local user presses Escape. Because Ozone/evdev modifier state may have
  // been cleared by the remote release, the event arrives with EF_NONE.
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE, ui::EF_NONE,
                   /*source_device_id=*/1);

  // Disconnect must still fire based on local physical key tracking.
  EXPECT_TRUE(disconnect_requested());
}

// Tests that remote injected Ctrl + Alt + Esc does not trigger local emergency
// disconnect.
TEST_F(LocalHotkeyInputMonitorChromeOsTest,
       RemoteInjectedHotkeysDoNotTriggerDisconnect) {
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_CONTROL, ui::EF_NONE,
                   ui::ED_REMOTE_INPUT_DEVICE);
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_MENU, ui::EF_NONE,
                   ui::ED_REMOTE_INPUT_DEVICE);
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                   ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                   ui::ED_REMOTE_INPUT_DEVICE);
  EXPECT_FALSE(disconnect_requested());
}

// Tests that releasing a local modifier stops Esc from triggering disconnect.
TEST_F(LocalHotkeyInputMonitorChromeOsTest,
       LocalModifierReleasePreventsDisconnect) {
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_CONTROL, ui::EF_NONE,
                   /*source_device_id=*/1);
  DispatchKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_CONTROL, ui::EF_NONE,
                   /*source_device_id=*/1);
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_MENU, ui::EF_NONE,
                   /*source_device_id=*/1);
  DispatchKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE, ui::EF_NONE,
                   /*source_device_id=*/1);
  EXPECT_FALSE(disconnect_requested());
}

}  // namespace remoting
