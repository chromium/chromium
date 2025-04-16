// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_keyboard_input_monitor.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/input_monitor/raw_input_handler.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

namespace {

// From the HID Usage Tables specification.
const std::uint16_t kKeyboardUsage = 6;

// Handles Keyboard Raw Input events.  This class should be instantiated at-most
// once per process instance to prevent the events being silently dropped.
class KeyboardRawInputHandler : public RawInputHandler {
 public:
  explicit KeyboardRawInputHandler(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  KeyboardRawInputHandler(const KeyboardRawInputHandler&) = delete;
  KeyboardRawInputHandler& operator=(const KeyboardRawInputHandler&) = delete;

  ~KeyboardRawInputHandler() override;

  // RawInputHandler implementation.
  void OnInputEvent(const RAWINPUT& event) override;
};

KeyboardRawInputHandler::KeyboardRawInputHandler(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : RawInputHandler(ui_task_runner, kKeyboardUsage) {}

KeyboardRawInputHandler::~KeyboardRawInputHandler() = default;

void KeyboardRawInputHandler::OnInputEvent(const RAWINPUT& event) {
  if (event.header.dwType == RIM_TYPEKEYBOARD &&
      event.header.hDevice != nullptr) {
    std::uint16_t vkey = event.data.keyboard.VKey;
    std::uint32_t scancode = MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
    std::uint32_t usb_keycode =
        ui::KeycodeConverter::NativeKeycodeToUsbKeycode(scancode);

    NotifyKeyboardInput(FROM_HERE, usb_keycode);
  }
}

class ScopedKeyboardInputMonitorWin : public LocalKeyboardInputMonitor,
                                      public RawInputHandler::Observer {
 public:
  explicit ScopedKeyboardInputMonitorWin(
      KeyboardRawInputHandler& raw_input_handler,
      LocalInputMonitor::KeyPressedCallback on_key_event_callback,
      base::OnceClosure disconnect_callback);
  ~ScopedKeyboardInputMonitorWin() override;

  // Observer implementation.
  void OnMouseMove(const webrtc::DesktopVector&, ui::EventType) override;
  void OnKeyboardInput(std::uint32_t usb_keycode) override;
  void OnError() override;

 private:
  base::raw_ref<KeyboardRawInputHandler> keyboard_raw_input_handler_;

  LocalInputMonitor::KeyPressedCallback on_key_event_callback_;
  base::OnceClosure disconnect_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

ScopedKeyboardInputMonitorWin::ScopedKeyboardInputMonitorWin(
    KeyboardRawInputHandler& raw_input_handler,
    LocalInputMonitor::KeyPressedCallback on_key_event_callback,
    base::OnceClosure disconnect_callback)
    : keyboard_raw_input_handler_(raw_input_handler),
      on_key_event_callback_(std::move(on_key_event_callback)),
      disconnect_callback_(std::move(disconnect_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  keyboard_raw_input_handler_->AddObserver(this);
}

ScopedKeyboardInputMonitorWin::~ScopedKeyboardInputMonitorWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  keyboard_raw_input_handler_->RemoveObserver(this);
}

void ScopedKeyboardInputMonitorWin::OnKeyboardInput(uint32_t usb_keycode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_key_event_callback_.Run(usb_keycode);
}

void ScopedKeyboardInputMonitorWin::OnMouseMove(
    const webrtc::DesktopVector& position,
    ui::EventType event_type) {
  NOTREACHED();
}

void ScopedKeyboardInputMonitorWin::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(disconnect_callback_).Run();
}

}  // namespace

std::unique_ptr<LocalKeyboardInputMonitor> LocalKeyboardInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::KeyPressedCallback on_key_event_callback,
    base::OnceClosure disconnect_callback) {
  // Ensure there is only one instance of KeyboardRawInputHandler because
  // Windows does not allow multiple windows to receive WM_INPUT messages for
  // the same raw input type in the same process.
  // Note: We reuse the initial |ui_task_runner| provided to init the handler,
  // this is reasonable as we just need a UI thread to run the core object on
  // and in practice, every invocation of this method passes the same
  // task_runner because it is retrieved via the ChromotingHostContext.
  static base::NoDestructor<KeyboardRawInputHandler> raw_input_handler{
      ui_task_runner};

  // Bind the callbacks to |caller_task_runner| to ensure they are executed on
  // the proper thread.
  return std::make_unique<ScopedKeyboardInputMonitorWin>(
      *raw_input_handler,
      base::BindPostTask(caller_task_runner, std::move(on_key_event_callback)),
      base::BindPostTask(caller_task_runner, std::move(disconnect_callback)));
}

}  // namespace remoting
