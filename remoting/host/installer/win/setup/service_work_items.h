// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_SERVICE_WORK_ITEMS_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_SERVICE_WORK_ITEMS_H_

// Chrome Remote Desktop Windows service and host event `WorkItem`s for the
// Windows host installer.
//
// What belongs here:
// - `ServiceManager`, `ConditionEventExists`, and `WorkItem` subclasses
//   (`StopServiceWorkItem`, `InstallOrUpdateServiceWorkItem`,
//   `StartServiceWorkItem`, `SignalUpdatePendingWorkItem`,
//   `UninstallServiceWorkItem`) and their test injection seams.
//
// What does NOT belong here:
// - Low-level Win32 Service Control Manager (SCM) or kernel event helper
//   implementations. Put those in `service_util.{h,cc}`.

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "chrome/installer/util/work_item.h"
#include "remoting/host/installer/win/setup/service_util.h"

namespace remoting::installer {

// Interface for interacting with the Windows Service Control Manager (SCM),
// allowing service WorkItems and state transitions to be tested without SCM
// elevation.
class ServiceManager {
 public:
  virtual ~ServiceManager() = default;

  virtual ServiceStateQueryResult GetServiceState() = 0;
  virtual std::optional<std::wstring> GetServiceBinaryPathName() = 0;
  virtual bool InstallOrUpdateService(
      const base::FilePath& host_binary_path,
      const base::FilePath& host_config_path) = 0;
  virtual bool SetServiceBinaryPathName(
      const std::wstring& binary_path_name) = 0;
  virtual bool StartService() = 0;
  virtual bool StopService() = 0;
  virtual bool UninstallService() = 0;
};

// Overrides the default SCM-backed `ServiceManager` for testing. Passing
// `nullptr` restores the default implementation.
void SetServiceManagerForTesting(ServiceManager* service_manager);

// A Condition that returns true if the specified kernel event exists and can be
// opened. Defaults to `kHostUpdatePendingEventName`.
class ConditionEventExists : public WorkItem::Condition {
 public:
  explicit ConditionEventExists(
      std::wstring event_name_for_testing = std::wstring());
  ConditionEventExists(const ConditionEventExists&) = delete;
  ConditionEventExists& operator=(const ConditionEventExists&) = delete;
  ~ConditionEventExists() override;

  bool ShouldRun() const override;

 private:
  std::wstring event_name_for_testing_;
};

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
// rollback, deletes the service if it was newly created, or restores its
// previous `lpBinaryPathName` if it existed previously.
class InstallOrUpdateServiceWorkItem : public WorkItem {
 public:
  InstallOrUpdateServiceWorkItem(const base::FilePath& host_binary_path,
                                 const base::FilePath& host_config_path);
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
  std::wstring previous_binary_path_name_;
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
