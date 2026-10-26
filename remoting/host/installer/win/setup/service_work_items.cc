// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/service_work_items.h"

#include <windows.h>

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_handle.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/service_util.h"

namespace remoting::installer {

namespace {

ServiceControlCallback& GetStartServiceCallback() {
  static base::NoDestructor<ServiceControlCallback> callback;
  return *callback;
}

ServiceControlCallback& GetStopServiceCallback() {
  static base::NoDestructor<ServiceControlCallback> callback;
  return *callback;
}

ServiceControlCallback& GetInstallOrUpdateServiceCallback() {
  static base::NoDestructor<ServiceControlCallback> callback;
  return *callback;
}

ServiceControlCallback& GetUninstallServiceCallback() {
  static base::NoDestructor<ServiceControlCallback> callback;
  return *callback;
}

}  // namespace

void SetStartServiceCallbackForTesting(ServiceControlCallback callback) {
  GetStartServiceCallback() = std::move(callback);
}

void SetStopServiceCallbackForTesting(ServiceControlCallback callback) {
  GetStopServiceCallback() = std::move(callback);
}

void SetInstallOrUpdateServiceCallbackForTesting(
    ServiceControlCallback callback) {
  GetInstallOrUpdateServiceCallback() = std::move(callback);
}

void SetUninstallServiceCallbackForTesting(ServiceControlCallback callback) {
  GetUninstallServiceCallback() = std::move(callback);
}

StopServiceWorkItem::StopServiceWorkItem() = default;

StopServiceWorkItem::~StopServiceWorkItem() = default;

bool StopServiceWorkItem::DoImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // TODO(crbug.com/551615326): Introduce an SCM interface/fake so service
  // state transitions (including rollback when the service was not previously
  // running) can be tested without callback overrides.
  if (GetStopServiceCallback()) {
    was_running_ = true;
    return GetStopServiceCallback().Run();
  }

  ScopedScHandle scm(
      ::OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT));
  if (!scm.is_valid()) {
    PLOG(ERROR) << "Failed to open Service Control Manager.";
    return false;
  }

  ScopedScHandle service(
      ::OpenServiceW(scm.Get(), kWindowsServiceName, SERVICE_QUERY_STATUS));
  if (!service.is_valid()) {
    DWORD error = ::GetLastError();
    if (error == ERROR_SERVICE_DOES_NOT_EXIST) {
      return true;
    }
    PLOG(ERROR) << "Failed to open service " << kWindowsServiceName;
    return false;
  }

  SERVICE_STATUS status = {};
  if (!::QueryServiceStatus(service.Get(), &status)) {
    PLOG(ERROR) << "Failed to query service status for " << kWindowsServiceName;
    return false;
  }

  if (status.dwCurrentState != SERVICE_STOPPED) {
    was_running_ = true;
    return StopHostService();
  }

  return true;
}

void StopServiceWorkItem::RollbackImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (was_running_) {
    if (GetStartServiceCallback()) {
      GetStartServiceCallback().Run();
    } else {
      StartHostService();
    }
  }
}

InstallOrUpdateServiceWorkItem::InstallOrUpdateServiceWorkItem(
    const base::FilePath& host_binary_path,
    const base::FilePath& host_config_path,
    bool existed_previously_for_testing)
    : host_binary_path_(host_binary_path),
      host_config_path_(host_config_path),
      existed_previously_(existed_previously_for_testing) {}

InstallOrUpdateServiceWorkItem::~InstallOrUpdateServiceWorkItem() = default;

bool InstallOrUpdateServiceWorkItem::DoImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (GetInstallOrUpdateServiceCallback()) {
    return GetInstallOrUpdateServiceCallback().Run();
  }

  if (!existed_previously_) {
    ScopedScHandle scm(::OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE,
                                        SC_MANAGER_CONNECT));
    // If we cannot determine whether the service already exists (e.g., due to
    // SCM or service open errors other than `ERROR_SERVICE_DOES_NOT_EXIST`),
    // default `existed_previously_` to true. On a failed installation or
    // update, it is much safer to leave a service registered than to risk
    // deleting a working pre-existing host service during rollback.
    if (scm.is_valid()) {
      ScopedScHandle service(
          ::OpenServiceW(scm.Get(), kWindowsServiceName, SERVICE_QUERY_CONFIG));
      if (service.is_valid() ||
          ::GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST) {
        existed_previously_ = true;
      }
    } else {
      existed_previously_ = true;
    }
  }

  return InstallOrUpdateService(host_binary_path_, host_config_path_);
}

void InstallOrUpdateServiceWorkItem::RollbackImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // TODO(crbug.com/551615326): When `existed_previously_` is true, record and
  // restore the prior service configuration (`lpBinaryPathName`, etc.) on
  // rollback.
  if (!existed_previously_) {
    if (GetUninstallServiceCallback()) {
      GetUninstallServiceCallback().Run();
    } else {
      UninstallHostService();
    }
  }
}

StartServiceWorkItem::StartServiceWorkItem() = default;

StartServiceWorkItem::~StartServiceWorkItem() = default;

bool StartServiceWorkItem::DoImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (GetStartServiceCallback()) {
    if (GetStartServiceCallback().Run()) {
      started_by_us_ = true;
      return true;
    }
    return false;
  }

  ScopedScHandle scm(
      ::OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT));
  if (!scm.is_valid()) {
    PLOG(ERROR) << "Failed to open Service Control Manager.";
    return false;
  }

  ScopedScHandle service(::OpenServiceW(scm.Get(), kWindowsServiceName,
                                        SERVICE_START | SERVICE_QUERY_STATUS));
  if (!service.is_valid()) {
    PLOG(ERROR) << "Failed to open service " << kWindowsServiceName;
    return false;
  }

  SERVICE_STATUS status = {};
  if (::QueryServiceStatus(service.Get(), &status) &&
      (status.dwCurrentState == SERVICE_RUNNING ||
       status.dwCurrentState == SERVICE_START_PENDING)) {
    // Service is already running; no need to start or record rollback.
    return true;
  }

  if (!::StartServiceW(service.Get(), 0, nullptr)) {
    DWORD error = ::GetLastError();
    if (error == ERROR_SERVICE_ALREADY_RUNNING) {
      return true;
    }
    PLOG(ERROR) << "Failed to start service " << kWindowsServiceName;
    return false;
  }

  started_by_us_ = true;
  return true;
}

void StartServiceWorkItem::RollbackImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (started_by_us_) {
    if (GetStopServiceCallback()) {
      GetStopServiceCallback().Run();
    } else {
      StopHostService();
    }
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

  if (GetUninstallServiceCallback()) {
    return GetUninstallServiceCallback().Run();
  }
  return UninstallHostService();
}

void UninstallServiceWorkItem::RollbackImpl() {
  // Best-effort uninstall rollback.
}

}  // namespace remoting::installer
