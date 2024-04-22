// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_user_input_monitor.h"

namespace media {
namespace {

using WriteKeyPressCallback =
    base::RepeatingCallback<void(const base::WritableSharedMemoryMapping& shmem,
                                 uint32_t count)>;

// Provides a unified interface for using user input monitors of unrelated
// classes.
// TODO(crbug.com/40136193): remove this when non-Ozone path is deprecated.
class UserInputMonitorAdapter {
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
  virtual base::WeakPtr<UserInputMonitorAdapter> AsWeakPtr() = 0;
};

// Wraps a specific user input monitor into UserInputMonitorAdapter interface.
template <typename Impl>
class UserInputMonitorLinuxCore final : public UserInputMonitorAdapter {
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
  base::WeakPtr<UserInputMonitorAdapter> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<Impl> user_input_monitor_;
  base::WeakPtrFactory<UserInputMonitorAdapter> weak_ptr_factory_{this};
};

class UserInputMonitorLinux : public UserInputMonitorBase {
 public:
  explicit UserInputMonitorLinux(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  UserInputMonitorLinux(const UserInputMonitorLinux&) = delete;
  UserInputMonitorLinux& operator=(const UserInputMonitorLinux&) = delete;

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
  raw_ptr<UserInputMonitorAdapter> core_;
};

UserInputMonitorAdapter* CreateUserInputMonitor(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner) {
  return new UserInputMonitorLinuxCore<ui::PlatformUserInputMonitor>(
      ui::OzonePlatform::GetInstance()->GetPlatformUserInputMonitor(
          io_task_runner));
}

//
// Implementation of UserInputMonitorLinux.
//

UserInputMonitorLinux::UserInputMonitorLinux(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : io_task_runner_(io_task_runner),
      core_(CreateUserInputMonitor(io_task_runner_)) {}

UserInputMonitorLinux::~UserInputMonitorLinux() {
  if (!io_task_runner_->DeleteSoon(FROM_HERE, core_.get()))
    core_.ClearAndDelete();
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
