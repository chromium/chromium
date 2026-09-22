// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xtest.h"

namespace remoting {

class LocalMouseInputMonitorX11Test : public testing::Test {
 public:
  LocalMouseInputMonitorX11Test()
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
      if (info.type == x11::Input::DeviceType::MasterPointer) {
        master_pointer_id_ = info.deviceid;
      } else if (info.name.ends_with("XTEST pointer")) {
        xtest_pointer_id_ = info.deviceid;
      }
    }
    ASSERT_NE(master_pointer_id_, x11::Input::DeviceId{});
    ASSERT_NE(xtest_pointer_id_, x11::Input::DeviceId{});
    ASSERT_NE(xtest_pointer_id_, kLocalPointerId);

    auto task_runner = task_environment_.GetMainThreadTaskRunner();
    monitor_ =
        LocalInputMonitor::Create(task_runner, task_runner, task_runner);
    monitor_->StartMonitoring(pointer_move_future_.GetRepeatingCallback(),
                              LocalInputMonitor::KeyPressedCallback(),
                              base::DoNothing());
    FlushTaskRunner();
    connection_->Sync();
    connection_->DispatchAll();
  }

  void TearDown() override {
    monitor_.reset();
    FlushTaskRunner();
  }

  void DispatchRawMotionEvent(x11::Input::DeviceId source_id) {
    x11::Input::RawDeviceEvent raw_event;
    raw_event.opcode = x11::Input::RawDeviceEvent::RawMotion;
    raw_event.deviceid = master_pointer_id_;
    raw_event.sourceid = source_id;

    x11::Event event(false, std::move(raw_event));
    connection_->DispatchEvent(event);
    connection_->Sync();
    connection_->DispatchAll();
    FlushTaskRunner();
  }

  void InjectXTestMotionEvent(int16_t x, int16_t y) {
    ASSERT_TRUE(connection_->xtest().present());
    connection_->xtest().FakeInput({
        .type = x11::MotionNotifyEvent::opcode,
        .detail = false,
        .root = connection_->default_root(),
        .rootX = x,
        .rootY = y,
    });
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
  // devices and the virtual XTEST devices, with no physical pointer attached.
  // Use a distinct synthetic DeviceId to represent a physical pointer when
  // dispatching RawDeviceEvents.
  static constexpr x11::Input::DeviceId kLocalPointerId{100};

  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<x11::Connection> connection_ = nullptr;
  x11::Input::DeviceId master_pointer_id_{};
  x11::Input::DeviceId xtest_pointer_id_{};
  x11::Input::DeviceId local_pointer_id_{kLocalPointerId};
  base::test::TestFuture<const webrtc::DesktopVector&, ui::EventType>
      pointer_move_future_;
  std::unique_ptr<LocalInputMonitor> monitor_;
};

TEST_F(LocalMouseInputMonitorX11Test, LocalMotionTriggersCallback) {
  DispatchRawMotionEvent(local_pointer_id_);

  ASSERT_TRUE(pointer_move_future_.Wait());
  EXPECT_EQ(pointer_move_future_.Get<1>(), ui::EventType::kMouseMoved);
}

TEST_F(LocalMouseInputMonitorX11Test,
       RemoteInjectedXTestMotionDoesNotTriggerCallback) {
  DispatchRawMotionEvent(xtest_pointer_id_);
  InjectXTestMotionEvent(100, 100);
  InjectXTestMotionEvent(101, 100);

  EXPECT_FALSE(pointer_move_future_.IsReady());

  // Subsequent local motion should still be detected.
  DispatchRawMotionEvent(local_pointer_id_);
  ASSERT_TRUE(pointer_move_future_.Wait());
  EXPECT_EQ(pointer_move_future_.Get<1>(), ui::EventType::kMouseMoved);
}

}  // namespace remoting
