// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <stddef.h>
#include <sys/select.h>
#include <unistd.h>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/current_thread.h"
#include "media/base/keyboard_event_counter.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/devices/x11/xinput_util.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gfx/x/xinput.h"

namespace media {
namespace {

// This is the actual implementation of event monitoring. It's separated from
// UserInputMonitorLinux since it needs to be deleted on the IO thread.
class UserInputMonitorLinuxCore
    : public base::SupportsWeakPtr<UserInputMonitorLinuxCore>,
      public base::CurrentThread::DestructionObserver,
      public x11::Connection::Delegate {
 public:
  explicit UserInputMonitorLinuxCore(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~UserInputMonitorLinuxCore() override;

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  // x11::Connection::Delegate:
  bool ShouldContinueStream() const override;
  void DispatchXEvent(x11::Event* event) override;

  uint32_t GetKeyPressCount() const;
  void StartMonitor();
  void StartMonitorWithMapping(base::WritableSharedMemoryMapping mapping);
  void StopMonitor();

 private:
  void OnConnectionData();

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Used for sharing key press count value.
  std::unique_ptr<base::WritableSharedMemoryMapping> key_press_count_mapping_;

  //
  // The following members should only be accessed on the IO thread.
  //
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watch_controller_;
  std::unique_ptr<x11::Connection> connection_;
  KeyboardEventCounter counter_;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitorLinuxCore);
};

class UserInputMonitorLinux : public UserInputMonitorBase {
 public:
  explicit UserInputMonitorLinux(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~UserInputMonitorLinux() override;

  // Public UserInputMonitor overrides.
  uint32_t GetKeyPressCount() const override;

 private:
  // Private UserInputMonitor overrides.
  void StartKeyboardMonitoring() override;
  void StartKeyboardMonitoring(
      base::WritableSharedMemoryMapping mapping) override;
  void StopKeyboardMonitoring() override;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  UserInputMonitorLinuxCore* core_;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitorLinux);
};

UserInputMonitorLinuxCore::UserInputMonitorLinuxCore(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : io_task_runner_(io_task_runner) {}

UserInputMonitorLinuxCore::~UserInputMonitorLinuxCore() {
  DCHECK(!connection_);
}

void UserInputMonitorLinuxCore::WillDestroyCurrentMessageLoop() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  StopMonitor();
}

bool UserInputMonitorLinuxCore::ShouldContinueStream() const {
  return true;
}

void UserInputMonitorLinuxCore::DispatchXEvent(x11::Event* event) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  auto* raw = event->As<x11::Input::RawDeviceEvent>();
  if (!raw || (raw->opcode != x11::Input::RawDeviceEvent::RawKeyPress &&
               raw->opcode != x11::Input::RawDeviceEvent::RawKeyRelease)) {
    return;
  }

  ui::EventType type = raw->opcode == x11::Input::RawDeviceEvent::RawKeyPress
                           ? ui::ET_KEY_PRESSED
                           : ui::ET_KEY_RELEASED;

  auto key_sym = connection_->KeycodeToKeysym(raw->detail, 0);
  ui::KeyboardCode key_code =
      ui::KeyboardCodeFromXKeysym(static_cast<uint32_t>(key_sym));
  counter_.OnKeyboardEvent(type, key_code);

  // Update count value in shared memory.
  if (key_press_count_mapping_)
    WriteKeyPressMonitorCount(*key_press_count_mapping_, GetKeyPressCount());
}

uint32_t UserInputMonitorLinuxCore::GetKeyPressCount() const {
  return counter_.GetKeyPressCount();
}

void UserInputMonitorLinuxCore::StartMonitor() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // TODO(https://crbug.com/1116414): support UserInputMonitorLinux on
  // Ozone/Linux.
  if (features::IsUsingOzonePlatform()) {
    NOTIMPLEMENTED_LOG_ONCE();
    StopMonitor();
    return;
  }

  if (!connection_) {
    // TODO(jamiewalch): We should pass the connection in.
    if (auto* connection = x11::Connection::Get()) {
      connection_ = x11::Connection::Get()->Clone();
    } else {
      LOG(ERROR) << "Couldn't open X connection";
      StopMonitor();
      return;
    }
  }

  if (!connection_->xinput().present()) {
    LOG(ERROR) << "X Input extension not available.";
    StopMonitor();
    return;
  }
  // Let the server know the client XInput version.
  connection_->xinput().XIQueryVersion(
      {x11::Input::major_version, x11::Input::minor_version});

  x11::Input::XIEventMask mask;
  ui::SetXinputMask(&mask, x11::Input::RawDeviceEvent::RawKeyPress);
  ui::SetXinputMask(&mask, x11::Input::RawDeviceEvent::RawKeyRelease);
  connection_->xinput().XISelectEvents(
      {connection_->default_root(),
       {{x11::Input::DeviceId::AllMaster, {mask}}}});
  connection_->Flush();

  // Register OnConnectionData() to be called every time there is something to
  // read from |connection_|.
  watch_controller_ = base::FileDescriptorWatcher::WatchReadable(
      connection_->GetFd(),
      base::BindRepeating(&UserInputMonitorLinuxCore::OnConnectionData,
                          base::Unretained(this)));

  // Start observing message loop destruction if we start monitoring the first
  // event.
  base::CurrentThread::Get()->AddDestructionObserver(this);

  // Fetch pending events if any.
  OnConnectionData();
}

void UserInputMonitorLinuxCore::StartMonitorWithMapping(
    base::WritableSharedMemoryMapping mapping) {
  StartMonitor();
  key_press_count_mapping_ =
      std::make_unique<base::WritableSharedMemoryMapping>(std::move(mapping));
}

void UserInputMonitorLinuxCore::StopMonitor() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  watch_controller_.reset();
  connection_.reset();
  key_press_count_mapping_.reset();

  // Stop observing message loop destruction if no event is being monitored.
  base::CurrentThread::Get()->RemoveDestructionObserver(this);
}

void UserInputMonitorLinuxCore::OnConnectionData() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  connection_->Dispatch(this);
}

//
// Implementation of UserInputMonitorLinux.
//

UserInputMonitorLinux::UserInputMonitorLinux(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : io_task_runner_(io_task_runner),
      core_(new UserInputMonitorLinuxCore(io_task_runner)) {}

UserInputMonitorLinux::~UserInputMonitorLinux() {
  if (!io_task_runner_->DeleteSoon(FROM_HERE, core_))
    delete core_;
}

uint32_t UserInputMonitorLinux::GetKeyPressCount() const {
  return core_->GetKeyPressCount();
}

void UserInputMonitorLinux::StartKeyboardMonitoring() {
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UserInputMonitorLinuxCore::StartMonitor,
                                core_->AsWeakPtr()));
}

void UserInputMonitorLinux::StartKeyboardMonitoring(
    base::WritableSharedMemoryMapping mapping) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UserInputMonitorLinuxCore::StartMonitorWithMapping,
                     core_->AsWeakPtr(), std::move(mapping)));
}

void UserInputMonitorLinux::StopKeyboardMonitoring() {
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UserInputMonitorLinuxCore::StopMonitor,
                                core_->AsWeakPtr()));
}

}  // namespace

std::unique_ptr<UserInputMonitor> UserInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return std::make_unique<UserInputMonitorLinux>(io_task_runner);
}

}  // namespace media
