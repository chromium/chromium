// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/raw_input_handler.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/win/message_window.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"

namespace remoting {

namespace {
// From the HID Usage Tables specification.
const std::uint16_t kGenericDesktopPage = 1;

constexpr auto kBecameNonEmpty =
    RawInputHandler::ObserverList::AddObserverResult::kBecameNonEmpty;

constexpr auto kWasOrBecameEmpty =
    RawInputHandler::ObserverList::RemoveObserverResult::kWasOrBecameEmpty;
}  // namespace

class RawInputHandler::Core final {
 public:
  using OnInputEventCallback =
      base::RepeatingCallback<void(const RAWINPUT& event)>;
  using OnErrorCallback =
      base::OnceCallback<void(const base::Location& from_here)>;

  Core(std::uint16_t hid_usage,
       OnInputEventCallback on_input_event_callback,
       OnErrorCallback on_error_callback);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core();

  void StartListening();
  void StopListening();

 private:
  // Registers interest in receiving raw input events.
  bool Register();

  // Unregisters raw input listener.
  void Unregister();

  // Handles WM_INPUT messages.
  LRESULT OnInput(HRAWINPUT input_handle);

  // Handles messages received by |window_|.
  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result);

  // Used to receive raw input.
  std::unique_ptr<base::win::MessageWindow> window_;

  std::uint16_t hid_usage_;
  OnInputEventCallback on_input_event_callback_;
  OnErrorCallback on_error_callback_;
  bool registered_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

RawInputHandler::Core::Core(std::uint16_t hid_usage,
                            OnInputEventCallback on_input_event_callback,
                            OnErrorCallback on_error_callback)
    : hid_usage_(hid_usage),
      on_input_event_callback_(std::move(on_input_event_callback)),
      on_error_callback_(std::move(on_error_callback)) {}

RawInputHandler::Core::~Core() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!registered_);
}

void RawInputHandler::Core::StartListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  window_ = std::make_unique<base::win::MessageWindow>();
  if (!window_->Create(
          base::BindRepeating(&Core::HandleMessage, base::Unretained(this)))) {
    PLOG(ERROR) << "Failed to create the raw input window";
    window_.reset();

    // If the local input cannot be monitored, then bad things can happen like
    // the remote user taking over the session.
    std::move(on_error_callback_).Run(FROM_HERE);
  }
}

void RawInputHandler::Core::StopListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Stop receiving raw input.
  if (window_) {
    Unregister();
  }

  window_.reset();
}

LRESULT RawInputHandler::Core::OnInput(HRAWINPUT input_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get the size of the input record.
  std::uint32_t size = 0;
  std::uint32_t result = GetRawInputData(input_handle, RID_INPUT, nullptr,
                                         &size, sizeof(RAWINPUTHEADER));
  if (result == static_cast<std::uint32_t>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }

  // Retrieve the input record itself.
  auto buffer = base::HeapArray<char>::Uninit(size);
  RAWINPUT* input = reinterpret_cast<RAWINPUT*>(buffer.data());
  result = GetRawInputData(input_handle, RID_INPUT, buffer.data(), &size,
                           sizeof(RAWINPUTHEADER));
  if (result == static_cast<std::uint32_t>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }

  on_input_event_callback_.Run(*input);

  return DefRawInputProc(&input, 1, sizeof(RAWINPUTHEADER));
}

bool RawInputHandler::Core::HandleMessage(UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam,
                                          LRESULT* result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (message) {
    case WM_CREATE: {
      if (Register()) {
        *result = 0;
      } else {
        *result = -1;
        std::move(on_error_callback_).Run(FROM_HERE);
      }
      return true;
    }

    case WM_INPUT:
      *result = OnInput(reinterpret_cast<HRAWINPUT>(lparam));
      return true;

    default:
      return false;
  }
}

bool RawInputHandler::Core::Register() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Register to receive raw keyboard input.
  RAWINPUTDEVICE device = {0};
  device.dwFlags = RIDEV_INPUTSINK;
  device.usUsagePage = kGenericDesktopPage;
  device.usUsage = hid_usage_;
  device.hwndTarget = window_->hwnd();
  if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
    PLOG(ERROR) << "RegisterRawInputDevices() failed";
    return false;
  }

  registered_ = true;
  return true;
}

void RawInputHandler::Core::Unregister() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RAWINPUTDEVICE device = {0};
  device.dwFlags = RIDEV_REMOVE;
  device.usUsagePage = kGenericDesktopPage;
  device.usUsage = hid_usage_;
  device.hwndTarget = nullptr;

  // The error is harmless, ignore it.
  RegisterRawInputDevices(&device, 1, sizeof(device));
  registered_ = false;
}

RawInputHandler::RawInputHandler(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    std::uint16_t hid_usage) {
  auto current_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  core_ = base::SequenceBound<Core>(
      ui_task_runner, hid_usage,
      base::BindPostTask(current_task_runner,
                         base::BindRepeating(&RawInputHandler::OnInputEvent,
                                             weak_factory_.GetWeakPtr())),
      base::BindPostTask(current_task_runner,
                         base::BindOnce(&RawInputHandler::NotifyError,
                                        weak_factory_.GetWeakPtr())));
}

RawInputHandler::~RawInputHandler() {
  observer_list_->AssertEmpty();
}

void RawInputHandler::AddObserver(Observer* observer) {
  if (observer_list_->AddObserver(observer) == kBecameNonEmpty) {
    StartListening();
  }
}

void RawInputHandler::RemoveObserver(Observer* observer) {
  if (observer_list_->RemoveObserver(observer) == kWasOrBecameEmpty) {
    StopListening();
  }
}

void RawInputHandler::StartListening() {
  core_.AsyncCall(&Core::StartListening);
}

void RawInputHandler::StopListening() {
  core_.AsyncCall(&Core::StopListening);
}

void RawInputHandler::NotifyMouseMove(
    const base::Location& from_here,
    const webrtc::DesktopVector& new_position) {
  observer_list_->Notify(from_here, &Observer::OnMouseMove, new_position,
                         ui::EventType::kMouseMoved);
}

void RawInputHandler::NotifyKeyboardInput(const base::Location& from_here,
                                          std::uint32_t usb_keycode) {
  observer_list_->Notify(from_here, &Observer::OnKeyboardInput, usb_keycode);
}

void RawInputHandler::NotifyError(const base::Location& from_here) {
  observer_list_->Notify(from_here, &Observer::OnError);
}

}  // namespace remoting
