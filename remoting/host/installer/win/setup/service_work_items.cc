// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome Remote Desktop Windows service and host event `WorkItem`s for the
// Windows host installer.
//
// See `service_work_items.h` for the boundary between service/event `WorkItem`s
// (which belong here) and low-level Win32 SCM/event helpers (which belong in
// `service_util.{h,cc}`).

#include "remoting/host/installer/win/setup/service_work_items.h"

#include <windows.h>

#include <optional>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/service_util.h"

namespace remoting::installer {

namespace {

class DefaultServiceManager : public ServiceManager {
 public:
  ServiceStateQueryResult GetServiceState() override {
    return GetHostServiceState();
  }

  std::optional<std::wstring> GetServiceBinaryPathName() override {
    return remoting::installer::GetServiceBinaryPathName();
  }

  bool InstallOrUpdateService(const base::FilePath& host_binary_path,
                              const base::FilePath& host_config_path) override {
    return remoting::installer::InstallOrUpdateService(host_binary_path,
                                                       host_config_path);
  }

  bool SetServiceBinaryPathName(const std::wstring& binary_path_name) override {
    return remoting::installer::SetServiceBinaryPathName(binary_path_name);
  }

  bool StartService() override { return StartHostService(); }

  bool StopService() override { return StopHostService(); }

  bool UninstallService() override { return UninstallHostService(); }
};

ServiceManager* g_service_manager_override = nullptr;

ServiceManager* GetServiceManager() {
  if (g_service_manager_override) {
    return g_service_manager_override;
  }
  static base::NoDestructor<DefaultServiceManager> default_manager;
  return default_manager.get();
}

}  // namespace

void SetServiceManagerForTesting(ServiceManager* service_manager) {
  g_service_manager_override = service_manager;
}

ConditionEventExists::ConditionEventExists(std::wstring event_name_for_testing)
    : event_name_for_testing_(std::move(event_name_for_testing)) {}

ConditionEventExists::~ConditionEventExists() = default;

bool ConditionEventExists::ShouldRun() const {
  const wchar_t* name = event_name_for_testing_.empty()
                            ? kHostUpdatePendingEventName
                            : event_name_for_testing_.c_str();
  return DoesEventExist(name);
}

StopServiceWorkItem::StopServiceWorkItem() = default;

StopServiceWorkItem::~StopServiceWorkItem() = default;

bool StopServiceWorkItem::DoImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ServiceStateQueryResult state = GetServiceManager()->GetServiceState();
  if (state == ServiceStateQueryResult::kError) {
    return false;
  }
  if (state != ServiceStateQueryResult::kNotInstalled &&
      state != ServiceStateQueryResult::kStopped) {
    was_running_ = true;
    return GetServiceManager()->StopService();
  }
  return true;
}

void StopServiceWorkItem::RollbackImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (was_running_) {
    GetServiceManager()->StartService();
  }
}

InstallOrUpdateServiceWorkItem::InstallOrUpdateServiceWorkItem(
    const base::FilePath& host_binary_path,
    const base::FilePath& host_config_path)
    : host_binary_path_(host_binary_path),
      host_config_path_(host_config_path) {}

InstallOrUpdateServiceWorkItem::~InstallOrUpdateServiceWorkItem() = default;

bool InstallOrUpdateServiceWorkItem::DoImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ServiceStateQueryResult state = GetServiceManager()->GetServiceState();
  if (state != ServiceStateQueryResult::kNotInstalled) {
    // If we cannot determine whether the service already exists (e.g., due to
    // SCM or service open errors returning `kError`), default
    // `existed_previously_` to true. On a failed installation or update, it is
    // much safer to leave a service registered than to risk deleting a working
    // pre-existing host service during rollback.
    existed_previously_ = true;
    previous_binary_path_name_ =
        GetServiceManager()->GetServiceBinaryPathName().value_or(
            std::wstring());
  }

  return GetServiceManager()->InstallOrUpdateService(host_binary_path_,
                                                     host_config_path_);
}

void InstallOrUpdateServiceWorkItem::RollbackImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!existed_previously_) {
    GetServiceManager()->UninstallService();
  } else if (!previous_binary_path_name_.empty()) {
    GetServiceManager()->SetServiceBinaryPathName(previous_binary_path_name_);
  }
}

StartServiceWorkItem::StartServiceWorkItem() = default;

StartServiceWorkItem::~StartServiceWorkItem() = default;

bool StartServiceWorkItem::DoImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ServiceStateQueryResult state = GetServiceManager()->GetServiceState();
  if (state == ServiceStateQueryResult::kError ||
      state == ServiceStateQueryResult::kNotInstalled) {
    return false;
  }
  if (state == ServiceStateQueryResult::kRunning ||
      state == ServiceStateQueryResult::kStartPending) {
    // Service is already running or starting; no need to start or record
    // rollback.
    return true;
  }

  if (!GetServiceManager()->StartService()) {
    return false;
  }

  started_by_us_ = true;
  return true;
}

void StartServiceWorkItem::RollbackImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (started_by_us_) {
    GetServiceManager()->StopService();
  }
}

SignalUpdatePendingWorkItem::SignalUpdatePendingWorkItem(
    std::wstring event_name_for_testing)
    : event_name_for_testing_(std::move(event_name_for_testing)) {
  // Signaling the running daemon is best-effort: once new files and registry
  // entries have been deployed, a failure to signal the event must not abort
  // or roll back the completed update.
  set_best_effort(true);
}

SignalUpdatePendingWorkItem::~SignalUpdatePendingWorkItem() = default;

bool SignalUpdatePendingWorkItem::DoImpl() {
  const wchar_t* name = event_name_for_testing_.empty()
                            ? kHostUpdatePendingEventName
                            : event_name_for_testing_.c_str();
  base::win::ScopedHandle event(::OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE,
                                             /*bInheritHandle=*/FALSE, name));
  if (!event.is_valid()) {
    DWORD error = ::GetLastError();
    if (error == ERROR_FILE_NOT_FOUND) {
      // The host daemon is not currently running (e.g., the session ended and
      // the service stopped while the update was being staged). Nothing needs
      // to be signaled.
      return true;
    }
    PLOG(ERROR) << "Failed to open update pending event: " << name;
    return false;
  }

  // Only record `signaled_by_us_ = true` if the event was not already signaled
  // by a prior update, so a rollback of a subsequent update does not clear an
  // already-pending update signal.
  bool already_signaled =
      (::WaitForSingleObject(event.Get(), 0) == WAIT_OBJECT_0);
  if (!::SetEvent(event.Get())) {
    PLOG(ERROR) << "Failed to signal update pending event: " << name;
    return false;
  }

  if (!already_signaled) {
    signaled_by_us_ = true;
  }
  return true;
}

void SignalUpdatePendingWorkItem::RollbackImpl() {
  if (signaled_by_us_) {
    const wchar_t* name = event_name_for_testing_.empty()
                              ? kHostUpdatePendingEventName
                              : event_name_for_testing_.c_str();
    base::win::ScopedHandle event(
        ::OpenEventW(EVENT_MODIFY_STATE, /*bInheritHandle=*/FALSE, name));
    if (event.is_valid()) {
      ::ResetEvent(event.Get());
    }
  }
}

UninstallServiceWorkItem::UninstallServiceWorkItem() = default;

UninstallServiceWorkItem::~UninstallServiceWorkItem() = default;

bool UninstallServiceWorkItem::DoImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  return GetServiceManager()->UninstallService();
}

void UninstallServiceWorkItem::RollbackImpl() {
  // Best-effort uninstall rollback.
}

}  // namespace remoting::installer
