// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "remoting/host/input_monitor/local_hotkey_input_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/keysyms/keysyms.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xtest.h"

namespace remoting {

class LocalHotkeyInputMonitorX11Test : public testing::Test {
 public:
  LocalHotkeyInputMonitorX11Test()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    connection_ = x11::Connection::Get();
    ASSERT_NE(connection_, nullptr);
    ASSERT_TRUE(connection_->Ready());
    ASSERT_TRUE(connection_->xinput().present());

    auto reply =
        connection_->xinput().XIQueryDevice({x11::Input::DeviceId::All}).Sync();
    ASSERT_TRUE(reply);
    for (const auto& info : reply->infos) {
      if (info.type == x11::Input::DeviceType::MasterKeyboard) {
        master_keyboard_id_ = info.deviceid;
      } else if (info.name.ends_with("XTEST keyboard")) {
        xtest_keyboard_id_ = info.deviceid;
      }
    }
    ASSERT_NE(master_keyboard_id_, x11::Input::DeviceId{});
    ASSERT_NE(xtest_keyboard_id_, x11::Input::DeviceId{});
    ASSERT_NE(xtest_keyboard_id_, kLocalKeyboardId);

    auto task_runner = task_environment_.GetMainThreadTaskRunner();
    monitor_ =
        LocalHotkeyInputMonitor::Create(task_runner, task_runner, task_runner,
                                        disconnect_future_.GetCallback());
    FlushTaskRunner();
    connection_->Sync();
    connection_->DispatchAll();
  }

  void TearDown() override {
    monitor_.reset();
    FlushTaskRunner();
  }

  void DispatchRawKeyEvent(uint32_t keysym,
                           bool down,
                           x11::Input::DeviceId source_id) {
    x11::KeyCode keycode = connection_->KeysymToKeycode(keysym);
    ASSERT_NE(keycode, x11::KeyCode{});

    x11::Input::RawDeviceEvent raw_event;
    raw_event.opcode = down ? x11::Input::RawDeviceEvent::RawKeyPress
                            : x11::Input::RawDeviceEvent::RawKeyRelease;
    raw_event.deviceid = master_keyboard_id_;
    raw_event.sourceid = source_id;
    raw_event.detail = static_cast<uint32_t>(keycode);

    x11::Event event(false, std::move(raw_event));
    connection_->DispatchEvent(event);
    FlushTaskRunner();
  }

  void InjectXTestKeyEvent(uint32_t keysym, bool down) {
    ASSERT_TRUE(connection_->xtest().present());
    x11::KeyCode keycode = connection_->KeysymToKeycode(keysym);
    ASSERT_NE(keycode, x11::KeyCode{});

    uint8_t opcode = down ? x11::KeyEvent::Press : x11::KeyEvent::Release;
    connection_->xtest().FakeInput({opcode, static_cast<uint8_t>(keycode)});
    connection_->Sync();
    connection_->DispatchAll();
    FlushTaskRunner();
  }

  void FlushTaskRunner() {
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  // Headless Xorg (used by testing/xvfb.py on CQ bots) only creates the master
  // devices and the virtual XTEST slave devices, with no physical slave
  // keyboard attached. Use a distinct synthetic DeviceId to represent a
  // physical slave keyboard when dispatching RawDeviceEvents.
  static constexpr x11::Input::DeviceId kLocalKeyboardId{100};

  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<x11::Connection> connection_ = nullptr;
  x11::Input::DeviceId master_keyboard_id_{};
  x11::Input::DeviceId xtest_keyboard_id_{};
  x11::Input::DeviceId local_keyboard_id_{kLocalKeyboardId};
  base::test::TestFuture<void> disconnect_future_;
  std::unique_ptr<LocalHotkeyInputMonitor> monitor_;
};

TEST_F(LocalHotkeyInputMonitorX11Test, LocalCtrlAltEscTriggersDisconnect) {
  DispatchRawKeyEvent(XK_Control_L, /*down=*/true, local_keyboard_id_);
  DispatchRawKeyEvent(XK_Alt_L, /*down=*/true, local_keyboard_id_);
  DispatchRawKeyEvent(XK_Escape, /*down=*/true, local_keyboard_id_);

  EXPECT_TRUE(disconnect_future_.IsReady());
}

TEST_F(LocalHotkeyInputMonitorX11Test,
       RemoteModifierReleaseDoesNotSuppressDisconnect) {
  // Local user physically holds Control and Alt.
  DispatchRawKeyEvent(XK_Control_L, /*down=*/true, local_keyboard_id_);
  DispatchRawKeyEvent(XK_Alt_L, /*down=*/true, local_keyboard_id_);

  // Remote client streams modifier release events via the XTEST device.
  DispatchRawKeyEvent(XK_Control_L, /*down=*/false, xtest_keyboard_id_);
  DispatchRawKeyEvent(XK_Alt_L, /*down=*/false, xtest_keyboard_id_);

  // Local user presses Escape while still physically holding Control and Alt.
  DispatchRawKeyEvent(XK_Escape, /*down=*/true, local_keyboard_id_);

  EXPECT_TRUE(disconnect_future_.IsReady());
}

TEST_F(LocalHotkeyInputMonitorX11Test,
       LiveXTestModifierChurnDoesNotSuppressDisconnect) {
  // Local user physically holds Control and Alt.
  DispatchRawKeyEvent(XK_Control_L, /*down=*/true, local_keyboard_id_);
  DispatchRawKeyEvent(XK_Alt_L, /*down=*/true, local_keyboard_id_);

  // Remote client injects modifier press/release churn through live XTest.
  InjectXTestKeyEvent(XK_Control_L, /*down=*/true);
  InjectXTestKeyEvent(XK_Control_L, /*down=*/false);
  InjectXTestKeyEvent(XK_Alt_L, /*down=*/true);
  InjectXTestKeyEvent(XK_Alt_L, /*down=*/false);

  // Local user presses Escape.
  DispatchRawKeyEvent(XK_Escape, /*down=*/true, local_keyboard_id_);

  EXPECT_TRUE(disconnect_future_.IsReady());
}

TEST_F(LocalHotkeyInputMonitorX11Test,
       RemoteInjectedHotkeysDoNotTriggerDisconnect) {
  DispatchRawKeyEvent(XK_Control_L, /*down=*/true, xtest_keyboard_id_);
  DispatchRawKeyEvent(XK_Alt_L, /*down=*/true, xtest_keyboard_id_);
  DispatchRawKeyEvent(XK_Escape, /*down=*/true, xtest_keyboard_id_);

  EXPECT_FALSE(disconnect_future_.IsReady());
}

TEST_F(LocalHotkeyInputMonitorX11Test, LocalModifierReleasePreventsDisconnect) {
  DispatchRawKeyEvent(XK_Control_L, /*down=*/true, local_keyboard_id_);
  DispatchRawKeyEvent(XK_Control_L, /*down=*/false, local_keyboard_id_);
  DispatchRawKeyEvent(XK_Alt_L, /*down=*/true, local_keyboard_id_);
  DispatchRawKeyEvent(XK_Escape, /*down=*/true, local_keyboard_id_);

  EXPECT_FALSE(disconnect_future_.IsReady());
}

TEST_F(LocalHotkeyInputMonitorX11Test,
       ReleasingOneCtrlWhileOtherCtrlHeldStillTriggersDisconnect) {
  DispatchRawKeyEvent(XK_Control_L, /*down=*/true, local_keyboard_id_);
  DispatchRawKeyEvent(XK_Control_R, /*down=*/true, local_keyboard_id_);
  DispatchRawKeyEvent(XK_Control_R, /*down=*/false, local_keyboard_id_);
  DispatchRawKeyEvent(XK_Alt_L, /*down=*/true, local_keyboard_id_);
  DispatchRawKeyEvent(XK_Escape, /*down=*/true, local_keyboard_id_);

  EXPECT_TRUE(disconnect_future_.IsReady());
}

}  // namespace remoting
