// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/message_window.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/events/keyboard_event_counter.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/events/win/keyboard_hook_monitor.h"
#include "ui/events/win/keyboard_hook_observer.h"

namespace media {
namespace {

// From the HID Usage Tables specification.
const USHORT kGenericDesktopPage = 1;
const USHORT kKeyboardUsage = 6;

std::unique_ptr<RAWINPUTDEVICE> GetRawInputDevices(HWND hwnd, DWORD flags) {
  std::unique_ptr<RAWINPUTDEVICE> device(new RAWINPUTDEVICE());
  device->dwFlags = flags;
  device->usUsagePage = kGenericDesktopPage;
  device->usUsage = kKeyboardUsage;
  device->hwndTarget = hwnd;
  return device;
}

// This is the actual implementation of event monitoring. It's separated from
// UserInputMonitorWin since it needs to be deleted on the UI thread.
class UserInputMonitorWinCore final
    : public base::CurrentThread::DestructionObserver,
      public ui::KeyboardHookObserver {
 public:
  enum EventBitMask {
    MOUSE_EVENT_MASK = 1,
    KEYBOARD_EVENT_MASK = 2,
  };

  explicit UserInputMonitorWinCore(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  UserInputMonitorWinCore(const UserInputMonitorWinCore&) = delete;
  UserInputMonitorWinCore& operator=(const UserInputMonitorWinCore&) = delete;

  ~UserInputMonitorWinCore() override;

  // DestructionObserver overrides.
  void WillDestroyCurrentMessageLoop() override;

  // KeyboardHookObserver implementation.
  void OnHookRegistered() override;
  void OnHookUnregistered() override;

  uint32_t GetKeyPressCount() const;
  void StartMonitor();
  void StartMonitorWithMapping(base::WritableSharedMemoryMapping mapping);
  void StopMonitor();

  base::WeakPtr<UserInputMonitorWinCore> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Handles WM_INPUT messages.
  LRESULT OnInput(HRAWINPUT input_handle);
  // Handles messages received by |window_|.
  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result);

  void CreateRawInputWindow();
  void DestroyRawInputWindow();

  // Task runner on which |window_| is created.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Used for sharing key press count value.
  std::unique_ptr<base::WritableSharedMemoryMapping> key_press_count_mapping_;

  // These members are only accessed on the UI thread.
  std::unique_ptr<base::win::MessageWindow> window_;
  ui::KeyboardEventCounter counter_;

  bool pause_monitoring_ = false;
  bool start_monitoring_after_hook_removed_ = false;

  base::WeakPtrFactory<UserInputMonitorWinCore> weak_ptr_factory_{this};
};

class UserInputMonitorWin : public UserInputMonitorBase {
 public:
  explicit UserInputMonitorWin(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner);

  UserInputMonitorWin(const UserInputMonitorWin&) = delete;
  UserInputMonitorWin& operator=(const UserInputMonitorWin&) = delete;

  ~UserInputMonitorWin() override;

  // Public UserInputMonitor overrides.
  uint32_t GetKeyPressCount() const override;

 private:
  // Private UserInputMonitor overrides.
  void StartKeyboardMonitoring() override;
  void StartKeyboardMonitoring(
      base::WritableSharedMemoryMapping mapping) override;
  void StopKeyboardMonitoring() override;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  raw_ptr<UserInputMonitorWinCore> core_;
};

UserInputMonitorWinCore::UserInputMonitorWinCore(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner) {
  // Register this instance with the KeyboardHookMonitor to listen for changes
  // in the KeyboardHook registration state.  Since this instance may have been
  // constructed after a hook was registered, check the current state as well.
  ui::KeyboardHookMonitor::GetInstance()->AddObserver(this);
  pause_monitoring_ = ui::KeyboardHookMonitor::GetInstance()->IsActive();
}

UserInputMonitorWinCore::~UserInputMonitorWinCore() {
  DCHECK(!window_);
  ui::KeyboardHookMonitor::GetInstance()->RemoveObserver(this);
}

void UserInputMonitorWinCore::WillDestroyCurrentMessageLoop() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  StopMonitor();
}

uint32_t UserInputMonitorWinCore::GetKeyPressCount() const {
  return counter_.GetKeyPressCount();
}

void UserInputMonitorWinCore::StartMonitor() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (pause_monitoring_) {
    start_monitoring_after_hook_removed_ = true;
    return;
  }

  CreateRawInputWindow();
}

void UserInputMonitorWinCore::StartMonitorWithMapping(
    base::WritableSharedMemoryMapping mapping) {
  StartMonitor();
  key_press_count_mapping_ =
      std::make_unique<base::WritableSharedMemoryMapping>(std::move(mapping));
}

void UserInputMonitorWinCore::StopMonitor() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  DestroyRawInputWindow();
  start_monitoring_after_hook_removed_ = false;

  key_press_count_mapping_.reset();
}

void UserInputMonitorWinCore::OnHookRegistered() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!pause_monitoring_);
  pause_monitoring_ = true;

  // Don't destroy |key_press_count_mapping_| as this is a temporary block and
  // we want to allow monitoring to continue using the same shared memory once
  // monitoring is unblocked.
  DestroyRawInputWindow();
}

void UserInputMonitorWinCore::OnHookUnregistered() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(pause_monitoring_);
  pause_monitoring_ = false;

  if (start_monitoring_after_hook_removed_) {
    start_monitoring_after_hook_removed_ = false;
    StartMonitor();
  }
}

void UserInputMonitorWinCore::CreateRawInputWindow() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (window_)
    return;

  std::unique_ptr<base::win::MessageWindow> window =
      std::make_unique<base::win::MessageWindow>();
  if (!window->Create(base::BindRepeating(
          &UserInputMonitorWinCore::HandleMessage, base::Unretained(this)))) {
    PLOG(ERROR) << "Failed to create the raw input window";
    return;
  }

  // Register to receive raw keyboard input.
  std::unique_ptr<RAWINPUTDEVICE> device(
      GetRawInputDevices(window->hwnd(), RIDEV_INPUTSINK));
  if (!RegisterRawInputDevices(device.get(), 1, sizeof(*device))) {
    PLOG(ERROR) << "RegisterRawInputDevices() failed for RIDEV_INPUTSINK";
    return;
  }

  window_ = std::move(window);
  // Start observing message loop destruction if we start monitoring the first
  // event.
  base::CurrentThread::Get()->AddDestructionObserver(this);
}

void UserInputMonitorWinCore::DestroyRawInputWindow() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (!window_)
    return;

  // Stop receiving raw input.
  std::unique_ptr<RAWINPUTDEVICE> device(
      GetRawInputDevices(nullptr, RIDEV_REMOVE));
  if (!RegisterRawInputDevices(device.get(), 1, sizeof(*device))) {
    PLOG(INFO) << "RegisterRawInputDevices() failed for RIDEV_REMOVE";
  }
  window_ = nullptr;

  // Stop observing message loop destruction if no event is being monitored.
  base::CurrentThread::Get()->RemoveDestructionObserver(this);
}

LRESULT UserInputMonitorWinCore::OnInput(HRAWINPUT input_handle) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Get the size of the input record.
  UINT size = 0;
  UINT result = GetRawInputData(
      input_handle, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(0u, result);

  // Retrieve the input record itself.
  auto buffer = base::HeapArray<uint8_t>::Uninit(size);
  RAWINPUT* input = reinterpret_cast<RAWINPUT*>(buffer.data());
  result = GetRawInputData(input_handle, RID_INPUT, buffer.data(), &size,
                           sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(size, result);

  // Notify the observer about events generated locally.
  if (input->header.dwType == RIM_TYPEKEYBOARD &&
      input->header.hDevice != NULL) {
    ui::EventType event = (input->data.keyboard.Flags & RI_KEY_BREAK)
                              ? ui::EventType::kKeyReleased
                              : ui::EventType::kKeyPressed;
    ui::KeyboardCode key_code =
        ui::KeyboardCodeForWindowsKeyCode(input->data.keyboard.VKey);
    counter_.OnKeyboardEvent(event, key_code);

    // Update count value in shared memory.
    if (key_press_count_mapping_)
      WriteKeyPressMonitorCount(*key_press_count_mapping_, GetKeyPressCount());
  }

  return DefRawInputProc(&input, 1, sizeof(RAWINPUTHEADER));
}

bool UserInputMonitorWinCore::HandleMessage(UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam,
                                            LRESULT* result) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  switch (message) {
    case WM_INPUT:
      *result = OnInput(reinterpret_cast<HRAWINPUT>(lparam));
      return true;

    default:
      return false;
  }
}

//
// Implementation of UserInputMonitorWin.
//

UserInputMonitorWin::UserInputMonitorWin(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : ui_task_runner_(ui_task_runner),
      core_(new UserInputMonitorWinCore(ui_task_runner)) {}

UserInputMonitorWin::~UserInputMonitorWin() {
  if (!ui_task_runner_->DeleteSoon(FROM_HERE, core_.get()))
    delete core_;
}

uint32_t UserInputMonitorWin::GetKeyPressCount() const {
  return core_->GetKeyPressCount();
}

void UserInputMonitorWin::StartKeyboardMonitoring() {
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UserInputMonitorWinCore::StartMonitor,
                                core_->AsWeakPtr()));
}

void UserInputMonitorWin::StartKeyboardMonitoring(
    base::WritableSharedMemoryMapping mapping) {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UserInputMonitorWinCore::StartMonitorWithMapping,
                     core_->AsWeakPtr(), std::move(mapping)));
}

void UserInputMonitorWin::StopKeyboardMonitoring() {
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UserInputMonitorWinCore::StopMonitor,
                                core_->AsWeakPtr()));
}

}  // namespace

std::unique_ptr<UserInputMonitor> UserInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return std::make_unique<UserInputMonitorWin>(ui_task_runner);
}

}  // namespace media
