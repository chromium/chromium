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
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/events/devices/x11/xinput_util.h"
#include "ui/events/keyboard_event_counter.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/x/xinput.h"

#if defined(USE_X11)
#include "ui/base/x/x11_user_input_monitor.h"  // nogncheck
#endif

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"                     // nogncheck
#include "ui/ozone/public/ozone_platform.h"               // nogncheck
#include "ui/ozone/public/platform_user_input_monitor.h"  // nogncheck
#endif

namespace media {
namespace {

using WriteKeyPressCallback =
    base::RepeatingCallback<void(const base::WritableSharedMemoryMapping& shmem,
                                 uint32_t count)>;

// Provides a unified interface for using user input monitors of unrelated
// classes.
// TODO(crbug.com/1096425): remove this when non-Ozone path is deprecated.
class UserInputMonitorAdapter
    : public base::SupportsWeakPtr<UserInputMonitorAdapter> {
 public:
  UserInputMonitorAdapter() = default;
  UserInputMonitorAdapter(const UserInputMonitorAdapter&) = delete;
  UserInputMonitorAdapter& operator=(const UserInputMonitorAdapter&) = delete;
  virtual ~UserInputMonitorAdapter() = default;

  virtual uint32_t GetKeyPressCount() const = 0;
  virtual void StartMonitor(WriteKeyPressCallback callback) = 0;
  virtual void StartMonitorWithMapping(
      WriteKeyPressCallback callback,
      base::WritableSharedMemoryMapping mapping) = 0;
  virtual void StopMonitor() = 0;
};

// Wraps a specific user input monitor into UserInputMonitorAdapter interface.
template <typename Impl>
class UserInputMonitorLinuxCore : public UserInputMonitorAdapter {
 public:
  explicit UserInputMonitorLinuxCore(std::unique_ptr<Impl> user_input_monitor)
      : user_input_monitor_(std::move(user_input_monitor)) {}
  UserInputMonitorLinuxCore(const UserInputMonitorLinuxCore&) = delete;
  UserInputMonitorLinuxCore operator=(const UserInputMonitorLinuxCore&) =
      delete;
  ~UserInputMonitorLinuxCore() override = default;

  uint32_t GetKeyPressCount() const override {
    if (!user_input_monitor_)
      return 0;
    return user_input_monitor_->GetKeyPressCount();
  }
  void StartMonitor(WriteKeyPressCallback callback) override {
    if (!user_input_monitor_)
      return;
    user_input_monitor_->StartMonitor(callback);
  }
  void StartMonitorWithMapping(
      WriteKeyPressCallback callback,
      base::WritableSharedMemoryMapping mapping) override {
    if (!user_input_monitor_)
      return;
    user_input_monitor_->StartMonitorWithMapping(callback, std::move(mapping));
  }
  void StopMonitor() override {
    if (!user_input_monitor_)
      return;
    user_input_monitor_->StopMonitor();
  }

 private:
  std::unique_ptr<Impl> user_input_monitor_;
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
  UserInputMonitorAdapter* core_;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitorLinux);
};

UserInputMonitorAdapter* CreateUserInputMonitor(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner) {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    return new UserInputMonitorLinuxCore<ui::PlatformUserInputMonitor>(
        ui::OzonePlatform::GetInstance()->GetPlatformUserInputMonitor(
            io_task_runner));
  }
#endif
#if defined(USE_X11)
  return new UserInputMonitorLinuxCore<ui::XUserInputMonitor>(
      std::make_unique<ui::XUserInputMonitor>(io_task_runner));
#else
  NOTREACHED();
  return nullptr;
#endif
}

//
// Implementation of UserInputMonitorLinux.
//

UserInputMonitorLinux::UserInputMonitorLinux(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : io_task_runner_(io_task_runner),
      core_(CreateUserInputMonitor(io_task_runner_)) {}

UserInputMonitorLinux::~UserInputMonitorLinux() {
  if (core_ && !io_task_runner_->DeleteSoon(FROM_HERE, core_))
    delete core_;
}

uint32_t UserInputMonitorLinux::GetKeyPressCount() const {
  if (!core_)
    return 0;
  return core_->GetKeyPressCount();
}

void UserInputMonitorLinux::StartKeyboardMonitoring() {
  if (!core_)
    return;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UserInputMonitorAdapter::StartMonitor, core_->AsWeakPtr(),
                     base::BindRepeating(&WriteKeyPressMonitorCount)));
}

void UserInputMonitorLinux::StartKeyboardMonitoring(
    base::WritableSharedMemoryMapping mapping) {
  if (!core_)
    return;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UserInputMonitorAdapter::StartMonitorWithMapping, core_->AsWeakPtr(),
          base::BindRepeating(&WriteKeyPressMonitorCount), std::move(mapping)));
}

void UserInputMonitorLinux::StopKeyboardMonitoring() {
  if (!core_)
    return;
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UserInputMonitorAdapter::StopMonitor,
                                core_->AsWeakPtr()));
}

}  // namespace

std::unique_ptr<UserInputMonitor> UserInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return std::make_unique<UserInputMonitorLinux>(io_task_runner);
}

}  // namespace media
