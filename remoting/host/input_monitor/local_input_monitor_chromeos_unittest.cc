// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_input_monitor.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/aura/env.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"

namespace remoting {

namespace {

using ::testing::_;
using ::testing::StrictMock;

class TestPlatformEventSourceHelper : public ui::PlatformEventSource {
 public:
  using ui::PlatformEventSource::DispatchEvent;
};

}  // namespace

class LocalInputMonitorChromeOsTest : public testing::Test {
 public:
  LocalInputMonitorChromeOsTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    if (!aura::Env::HasInstance()) {
      env_ = aura::Env::CreateInstance();
    }
    if (!ui::PlatformEventSource::GetInstance()) {
      platform_event_source_ = ui::PlatformEventSource::CreateDefault();
    }

    auto task_runner = task_environment_.GetMainThreadTaskRunner();
    monitor_ = LocalInputMonitor::Create(task_runner, task_runner, task_runner);
    monitor_->StartMonitoringForClientSession(
        client_session_control_factory_.GetWeakPtr());
    FlushTaskRunner();
  }

  void TearDown() override {
    monitor_.reset();
    FlushTaskRunner();
    platform_event_source_.reset();
    env_.reset();
  }

  void DispatchPlatformEvent(ui::Event* event, int source_device_id = 1) {
    event->set_source_device_id(source_device_id);
    static_cast<TestPlatformEventSourceHelper*>(
        ui::PlatformEventSource::GetInstance())
        ->DispatchEvent(event);
    FlushTaskRunner();
  }

  void DispatchAuraEvent(ui::Event* event, int source_device_id = 1) {
    event->set_source_device_id(source_device_id);
    aura::Env::GetInstance()->NotifyEventObservers(*event);
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
  std::unique_ptr<aura::Env> env_;
  std::unique_ptr<ui::PlatformEventSource> platform_event_source_;
  StrictMock<MockClientSessionControl> client_session_control_;
  base::WeakPtrFactory<ClientSessionControl> client_session_control_factory_{
      &client_session_control_};
  std::unique_ptr<LocalInputMonitor> monitor_;
};

TEST_F(LocalInputMonitorChromeOsTest, MouseMoveIsReported) {
  EXPECT_CALL(client_session_control_,
              OnLocalPointerMoved(_, ui::EventType::kMouseMoved));
  ui::MouseEvent move_event(ui::EventType::kMouseMoved, gfx::Point(100, 150),
                            gfx::Point(100, 150), base::TimeTicks::Now(),
                            ui::EF_NONE, ui::EF_NONE);
  DispatchPlatformEvent(&move_event);
}

TEST_F(LocalInputMonitorChromeOsTest, MouseClickIsReported) {
  EXPECT_CALL(client_session_control_,
              OnLocalPointerMoved(_, ui::EventType::kMousePressed));
  EXPECT_CALL(client_session_control_,
              OnLocalPointerMoved(_, ui::EventType::kMouseReleased));

  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(100, 150),
                             gfx::Point(100, 150), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release_event(ui::EventType::kMouseReleased,
                               gfx::Point(100, 150), gfx::Point(100, 150),
                               base::TimeTicks::Now(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  DispatchPlatformEvent(&press_event);
  DispatchPlatformEvent(&release_event);
}

TEST_F(LocalInputMonitorChromeOsTest, MouseDragIsReported) {
  EXPECT_CALL(client_session_control_,
              OnLocalPointerMoved(_, ui::EventType::kMouseDragged));

  ui::MouseEvent drag_event(ui::EventType::kMouseMoved, gfx::Point(120, 160),
                            gfx::Point(120, 160), base::TimeTicks::Now(),
                            ui::EF_LEFT_MOUSE_BUTTON, ui::EF_NONE);
  ASSERT_EQ(drag_event.type(), ui::EventType::kMouseDragged);
  DispatchPlatformEvent(&drag_event);
}

TEST_F(LocalInputMonitorChromeOsTest, TouchMoveIsReported) {
  EXPECT_CALL(client_session_control_,
              OnLocalPointerMoved(_, ui::EventType::kTouchMoved));
  ui::TouchEvent touch_move(
      ui::EventType::kTouchMoved, gfx::Point(200, 250), base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchPlatformEvent(&touch_move);
}

TEST_F(LocalInputMonitorChromeOsTest, TouchTapIsReported) {
  EXPECT_CALL(client_session_control_,
              OnLocalPointerMoved(_, ui::EventType::kTouchPressed));
  EXPECT_CALL(client_session_control_,
              OnLocalPointerMoved(_, ui::EventType::kTouchReleased));

  ui::TouchEvent touch_press(
      ui::EventType::kTouchPressed, gfx::Point(200, 250),
      base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  ui::TouchEvent touch_release(
      ui::EventType::kTouchReleased, gfx::Point(200, 250),
      base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchPlatformEvent(&touch_press);
  DispatchPlatformEvent(&touch_release);
}

TEST_F(LocalInputMonitorChromeOsTest, PhysicalKeyPressIsReported) {
  EXPECT_CALL(client_session_control_, OnLocalKeyPressed(_));
  ui::KeyEvent key_press(ui::EventType::kKeyPressed, ui::VKEY_A,
                         ui::DomCode::US_A, ui::EF_NONE);
  DispatchPlatformEvent(&key_press);
}

TEST_F(LocalInputMonitorChromeOsTest, VirtualKeyboardKeyPressIsReported) {
  EXPECT_CALL(client_session_control_, OnLocalKeyPressed(_));
  ui::KeyEvent vk_key_press(ui::EventType::kKeyPressed, ui::VKEY_A,
                            ui::DomCode::US_A, ui::EF_NONE);
  DispatchAuraEvent(&vk_key_press);
}

TEST_F(LocalInputMonitorChromeOsTest, KeyReleaseIsIgnored) {
  ui::KeyEvent key_release(ui::EventType::kKeyReleased, ui::VKEY_A,
                           ui::DomCode::US_A, ui::EF_NONE);
  DispatchPlatformEvent(&key_release);
  DispatchAuraEvent(&key_release);
}

TEST_F(LocalInputMonitorChromeOsTest, CrdInjectedEventsAreIgnored) {
  ui::MouseEvent move_event(ui::EventType::kMouseMoved, gfx::Point(100, 150),
                            gfx::Point(100, 150), base::TimeTicks::Now(),
                            ui::EF_NONE, ui::EF_NONE);
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(100, 150),
                             gfx::Point(100, 150), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  ui::TouchEvent touch_press(
      ui::EventType::kTouchPressed, gfx::Point(200, 250),
      base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  ui::KeyEvent key_press(ui::EventType::kKeyPressed, ui::VKEY_A,
                         ui::DomCode::US_A, ui::EF_NONE);

  DispatchPlatformEvent(&move_event, ui::ED_REMOTE_INPUT_DEVICE);
  DispatchPlatformEvent(&press_event, ui::ED_REMOTE_INPUT_DEVICE);
  DispatchPlatformEvent(&touch_press, ui::ED_REMOTE_INPUT_DEVICE);
  DispatchPlatformEvent(&key_press, ui::ED_REMOTE_INPUT_DEVICE);
  DispatchAuraEvent(&key_press, ui::ED_REMOTE_INPUT_DEVICE);
}

}  // namespace remoting
