// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_SERVICE_WORK_ITEMS_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_SERVICE_WORK_ITEMS_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/installer/util/work_item.h"

namespace remoting::installer {

// Callbacks for mocking service operations in tests without requiring SCM
// elevation.
using ServiceControlCallback = base::RepeatingCallback<bool()>;

void SetStartServiceCallbackForTesting(ServiceControlCallback callback);
void SetStopServiceCallbackForTesting(ServiceControlCallback callback);
void SetInstallOrUpdateServiceCallbackForTesting(
    ServiceControlCallback callback);
void SetUninstallServiceCallbackForTesting(ServiceControlCallback callback);

// A WorkItem that stops the Chrome Remote Desktop service if it is running.
// On rollback, restarts the service if it was running before this WorkItem
// executed.
class StopServiceWorkItem : public WorkItem {
 public:
  StopServiceWorkItem();
  StopServiceWorkItem(const StopServiceWorkItem&) = delete;
  StopServiceWorkItem& operator=(const StopServiceWorkItem&) = delete;
  ~StopServiceWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;

  bool was_running_ = false;
};

// A WorkItem that installs or updates the chromoting Windows service. On
// rollback, deletes the service if it was newly created.
class InstallOrUpdateServiceWorkItem : public WorkItem {
 public:
  InstallOrUpdateServiceWorkItem(const base::FilePath& host_binary_path,
                                 const base::FilePath& host_config_path,
                                 bool existed_previously_for_testing = false);
  InstallOrUpdateServiceWorkItem(const InstallOrUpdateServiceWorkItem&) =
      delete;
  InstallOrUpdateServiceWorkItem& operator=(
      const InstallOrUpdateServiceWorkItem&) = delete;
  ~InstallOrUpdateServiceWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;

  base::FilePath host_binary_path_;
  base::FilePath host_config_path_;
  bool existed_previously_ = false;
};

// A WorkItem that starts the Chrome Remote Desktop Windows service. On
// rollback, stops the service if started by this WorkItem.
class StartServiceWorkItem : public WorkItem {
 public:
  StartServiceWorkItem();
  StartServiceWorkItem(const StartServiceWorkItem&) = delete;
  StartServiceWorkItem& operator=(const StartServiceWorkItem&) = delete;
  ~StartServiceWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;

  bool started_by_us_ = false;
};

// A best-effort WorkItem that signals the global CrdHostUpdatePending event.
// On rollback, resets the event only if it was transitioned from unsignaled to
// signaled by this WorkItem.
class SignalUpdatePendingWorkItem : public WorkItem {
 public:
  explicit SignalUpdatePendingWorkItem(
      std::wstring event_name_for_testing = std::wstring());
  SignalUpdatePendingWorkItem(const SignalUpdatePendingWorkItem&) = delete;
  SignalUpdatePendingWorkItem& operator=(const SignalUpdatePendingWorkItem&) =
      delete;
  ~SignalUpdatePendingWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;

  std::wstring event_name_for_testing_;
  bool signaled_by_us_ = false;
};

// A WorkItem that uninstalls the Chrome Remote Desktop Windows service.
class UninstallServiceWorkItem : public WorkItem {
 public:
  UninstallServiceWorkItem();
  UninstallServiceWorkItem(const UninstallServiceWorkItem&) = delete;
  UninstallServiceWorkItem& operator=(const UninstallServiceWorkItem&) = delete;
  ~UninstallServiceWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;
};

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_SERVICE_WORK_ITEMS_H_
