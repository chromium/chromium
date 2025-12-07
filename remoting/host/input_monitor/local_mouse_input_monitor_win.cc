// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "remoting/host/input_monitor/local_pointer_input_monitor.h"
#include "remoting/host/input_monitor/raw_input_handler.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

namespace {

// From the HID Usage Tables specification.
const std::uint16_t kMouseUsage = 2;

// Handles Mouse Raw Input events.  This class should be instantiated at-most
// once per process instance to prevent the events being silently dropped.
class MouseRawInputHandler : public RawInputHandler {
 public:
  explicit MouseRawInputHandler(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  MouseRawInputHandler(const MouseRawInputHandler&) = delete;
  MouseRawInputHandler& operator=(const MouseRawInputHandler&) = delete;

  ~MouseRawInputHandler() override;

  // RawInputHandler implementation.
  void OnInputEvent(const RAWINPUT& event) override;

 private:
  webrtc::DesktopVector mouse_position_;
};

MouseRawInputHandler::MouseRawInputHandler(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : RawInputHandler(ui_task_runner, kMouseUsage) {}

MouseRawInputHandler::~MouseRawInputHandler() = default;

void MouseRawInputHandler::OnInputEvent(const RAWINPUT& event) {
  // Notify the observer about mouse events generated locally. Remote (injected)
  // mouse events do not specify a device handle (based on observed behavior).
  if (event.header.dwType != RIM_TYPEMOUSE || event.header.hDevice == nullptr) {
    return;
  }

  POINT position;
  if (!GetCursorPos(&position)) {
    position.x = 0;
    position.y = 0;
  }
  webrtc::DesktopVector new_position(position.x, position.y);

  // Ignore the event if the cursor position or button states have not changed.
  // Note: If GetCursorPos fails above, we err on the safe side and treat it
  // like a movement.
  if (mouse_position_.equals(new_position) && !event.data.mouse.usButtonFlags &&
      !new_position.is_zero()) {
    return;
  }

  mouse_position_ = new_position;

  NotifyMouseMove(FROM_HERE, new_position);
}

// Note that this class does not detect touch input and so is named accordingly.
class ScopedMouseInputMonitorWin : public LocalPointerInputMonitor,
                                   public RawInputHandler::Observer {
 public:
  explicit ScopedMouseInputMonitorWin(
      MouseRawInputHandler& raw_input_handler,
      LocalInputMonitor::PointerMoveCallback on_mouse_move,
      base::OnceClosure disconnect_callback);
  ~ScopedMouseInputMonitorWin() override;

  // Observer implementation.
  void OnMouseMove(const webrtc::DesktopVector&, ui::EventType) override;
  void OnKeyboardInput(std::uint32_t usb_keycode) override;
  void OnError() override;

 private:
  base::raw_ref<MouseRawInputHandler> raw_input_handler_;

  LocalInputMonitor::PointerMoveCallback on_mouse_move_;
  base::OnceClosure disconnect_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

ScopedMouseInputMonitorWin::ScopedMouseInputMonitorWin(
    MouseRawInputHandler& raw_input_handler,
    LocalInputMonitor::PointerMoveCallback on_mouse_move,
    base::OnceClosure disconnect_callback)
    : raw_input_handler_(raw_input_handler),
      on_mouse_move_(std::move(on_mouse_move)),
      disconnect_callback_(std::move(disconnect_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  raw_input_handler_->AddObserver(this);
}

ScopedMouseInputMonitorWin::~ScopedMouseInputMonitorWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  raw_input_handler_->RemoveObserver(this);
}

void ScopedMouseInputMonitorWin::OnMouseMove(
    const webrtc::DesktopVector& position,
    ui::EventType event_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_mouse_move_.Run(position, event_type);
}

void ScopedMouseInputMonitorWin::OnKeyboardInput(uint32_t usb_keycode) {
  NOTREACHED();
}

void ScopedMouseInputMonitorWin::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(disconnect_callback_).Run();
}

}  // namespace

std::unique_ptr<LocalPointerInputMonitor> LocalPointerInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::PointerMoveCallback on_mouse_move,
    base::OnceClosure disconnect_callback) {
  // Ensure there is only one instance of MouseRawInputHandler because
  // Windows does not allow multiple windows to receive WM_INPUT messages for
  // the same raw input type in the same process.
  // Note: We reuse the initial |ui_task_runner| provided to init the handler,
  // this is reasonable as we just need a UI thread to run the core object on
  // and in practice, every invocation of this method passes the same
  // task_runner because it is retrieved via the ChromotingHostContext.
  static base::NoDestructor<MouseRawInputHandler> raw_input_handler{
      ui_task_runner};

  // Bind the callbacks to |caller_task_runner| to ensure they are executed on
  // the proper thread.
  return std::make_unique<ScopedMouseInputMonitorWin>(
      *raw_input_handler,
      base::BindPostTask(caller_task_runner, std::move(on_mouse_move)),
      base::BindPostTask(caller_task_runner, std::move(disconnect_callback)));
}

}  // namespace remoting
