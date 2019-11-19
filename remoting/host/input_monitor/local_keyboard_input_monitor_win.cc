// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_keyboard_input_monitor.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/input_monitor/local_input_monitor_win.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

namespace {

// From the HID Usage Tables specification.
const USHORT kGenericDesktopPage = 1;
const USHORT kKeyboardUsage = 6;

class KeyboardRawInputHandlerWin
    : public LocalInputMonitorWin::RawInputHandler {
 public:
  KeyboardRawInputHandlerWin(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      LocalInputMonitor::KeyPressedCallback on_key_event_callback,
      base::OnceClosure disconnect_callback);
  ~KeyboardRawInputHandlerWin() override;

  // LocalInputMonitorWin::RawInputHandler implementation.
  bool Register(HWND hwnd) override;
  void Unregister() override;
  void OnInputEvent(const RAWINPUT* input) override;
  void OnError() override;

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  LocalInputMonitor::KeyPressedCallback on_key_event_callback_;
  base::OnceClosure disconnect_callback_;

  // Tracks whether the instance is registered to receive raw input events.
  bool registered_ = false;

  DISALLOW_COPY_AND_ASSIGN(KeyboardRawInputHandlerWin);
};

KeyboardRawInputHandlerWin::KeyboardRawInputHandlerWin(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::KeyPressedCallback on_key_event_callback,
    base::OnceClosure disconnect_callback)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      on_key_event_callback_(std::move(on_key_event_callback)),
      disconnect_callback_(std::move(disconnect_callback)) {}

KeyboardRawInputHandlerWin::~KeyboardRawInputHandlerWin() {
  DCHECK(!registered_);
}

bool KeyboardRawInputHandlerWin::Register(HWND hwnd) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Register to receive raw keyboard input.
  RAWINPUTDEVICE device = {0};
  device.dwFlags = RIDEV_INPUTSINK;
  device.usUsagePage = kGenericDesktopPage;
  device.usUsage = kKeyboardUsage;
  device.hwndTarget = hwnd;
  if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
    PLOG(ERROR) << "RegisterRawInputDevices() failed";
    return false;
  }

  registered_ = true;
  return true;
}

void KeyboardRawInputHandlerWin::Unregister() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  RAWINPUTDEVICE device = {0};
  device.dwFlags = RIDEV_REMOVE;
  device.usUsagePage = kGenericDesktopPage;
  device.usUsage = kKeyboardUsage;
  device.hwndTarget = nullptr;

  // The error is harmless, ignore it.
  RegisterRawInputDevices(&device, 1, sizeof(device));
  registered_ = false;
}

void KeyboardRawInputHandlerWin::OnInputEvent(const RAWINPUT* input) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (input->header.dwType == RIM_TYPEKEYBOARD &&
      input->header.hDevice != nullptr) {
    USHORT vkey = input->data.keyboard.VKey;
    UINT scancode = MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
    uint32_t usb_keycode =
        ui::KeycodeConverter::NativeKeycodeToUsbKeycode(scancode);
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_key_event_callback_, usb_keycode));
  }
}

void KeyboardRawInputHandlerWin::OnError() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (disconnect_callback_) {
    caller_task_runner_->PostTask(FROM_HERE, std::move(disconnect_callback_));
  }
}

class LocalKeyboardInputMonitorWin : public LocalKeyboardInputMonitor {
 public:
  explicit LocalKeyboardInputMonitorWin(
      std::unique_ptr<LocalInputMonitorWin> local_input_monitor);
  ~LocalKeyboardInputMonitorWin() override;

 private:
  std::unique_ptr<LocalInputMonitorWin> local_input_monitor_;
};

LocalKeyboardInputMonitorWin::LocalKeyboardInputMonitorWin(
    std::unique_ptr<LocalInputMonitorWin> local_input_monitor)
    : local_input_monitor_(std::move(local_input_monitor)) {}

LocalKeyboardInputMonitorWin::~LocalKeyboardInputMonitorWin() = default;

}  // namespace

std::unique_ptr<LocalKeyboardInputMonitor> LocalKeyboardInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::KeyPressedCallback on_key_event_callback,
    base::OnceClosure disconnect_callback) {
  auto raw_input_handler = std::make_unique<KeyboardRawInputHandlerWin>(
      caller_task_runner, ui_task_runner, std::move(on_key_event_callback),
      std::move(disconnect_callback));

  return std::make_unique<LocalKeyboardInputMonitorWin>(
      LocalInputMonitorWin::Create(caller_task_runner, ui_task_runner,
                                   std::move(raw_input_handler)));
}

}  // namespace remoting
