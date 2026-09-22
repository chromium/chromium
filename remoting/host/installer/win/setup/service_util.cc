// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/service_util.h"

#include <windows.h>

#include <array>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/installer/win/setup/installer_constants.h"

namespace remoting::installer {

namespace {

constexpr base::TimeDelta kServiceStopTimeout = base::Seconds(30);
constexpr base::TimeDelta kServicePollInterval = base::Milliseconds(250);

void ConfigureServiceProperties(SC_HANDLE service) {
  // Set the service description.
  SERVICE_DESCRIPTIONW description = {const_cast<LPWSTR>(kServiceDescription)};
  if (!::ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION,
                               &description)) {
    PLOG(WARNING) << "Failed to set service description";
  }

  // Configure recovery actions: restart after 60 seconds on 1st, 2nd, and 3rd
  // failures, resetting the failure count after 1 day (86400 seconds).
  std::array<SC_ACTION, 3> actions = {{
      {SC_ACTION_RESTART, 60000},
      {SC_ACTION_RESTART, 60000},
      {SC_ACTION_RESTART, 60000},
  }};

  SERVICE_FAILURE_ACTIONSW failure_actions = {};
  failure_actions.dwResetPeriod = 86400;
  failure_actions.cActions = static_cast<DWORD>(actions.size());
  failure_actions.lpsaActions = actions.data();

  if (!::ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS,
                               &failure_actions)) {
    PLOG(WARNING) << "Failed to set service failure recovery actions";
  }
}

}  // namespace

bool IsHostSessionActive(const wchar_t* event_name_for_testing) {
  base::win::ScopedHandle event(::OpenEventW(
      SYNCHRONIZE, /*bInheritHandle=*/FALSE, event_name_for_testing));
  if (!event.is_valid()) {
    DWORD error = ::GetLastError();
    if (error != ERROR_FILE_NOT_FOUND) {
      PLOG(WARNING) << "Failed to open session active event: "
                    << event_name_for_testing;
    }
    return false;
  }

  DWORD wait_result = ::WaitForSingleObject(event.Get(), 0);
  if (wait_result == WAIT_FAILED) {
    PLOG(ERROR) << "WaitForSingleObject failed for " << event_name_for_testing;
    return false;
  }
  return wait_result == WAIT_OBJECT_0;
}

bool SignalHostUpdatePending(const wchar_t* event_name_for_testing) {
  base::win::ScopedHandle event(::OpenEventW(
      EVENT_MODIFY_STATE, /*bInheritHandle=*/FALSE, event_name_for_testing));
  if (!event.is_valid()) {
    DWORD error = ::GetLastError();
    if (error != ERROR_FILE_NOT_FOUND) {
      PLOG(ERROR) << "Failed to open update pending event: "
                  << event_name_for_testing;
    }
    return false;
  }

  if (!::SetEvent(event.Get())) {
    PLOG(ERROR) << "Failed to signal update pending event: "
                << event_name_for_testing;
    return false;
  }

  return true;
}

bool InstallOrUpdateService(const base::FilePath& host_binary_path,
                            const base::FilePath& host_config_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ScopedScHandle scm(
      ::OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE,
                       SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
  if (!scm.is_valid()) {
    PLOG(ERROR) << "Failed to open Service Control Manager";
    return false;
  }

  base::CommandLine command_line(host_binary_path);
  command_line.AppendSwitchASCII("type", "daemon");
  command_line.AppendSwitchPath("host-config", host_config_path);
  std::wstring command_line_str = command_line.GetCommandLineString();

  // SERVICE_START is required by ChangeServiceConfig2W when configuring
  // SC_ACTION_RESTART recovery actions.
  ScopedScHandle service(::OpenServiceW(
      scm.Get(), kWindowsServiceName,
      SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_START));
  if (service.is_valid()) {
    // An empty string (L"") for lpServiceStartName configures the service to
    // run under the LocalSystem account.
    if (!::ChangeServiceConfigW(
            service.Get(), SERVICE_WIN32_OWN_PROCESS, SERVICE_NO_CHANGE,
            SERVICE_ERROR_IGNORE, command_line_str.c_str(),
            /*lpLoadOrderGroup=*/nullptr, /*lpdwTagId=*/nullptr,
            /*lpDependencies=*/nullptr, /*lpServiceStartName=*/L"",
            /*lpPassword=*/nullptr, kServiceDisplayName)) {
      PLOG(ERROR) << "Failed to update configuration for service "
                  << kWindowsServiceName;
      return false;
    }
  } else {
    if (::GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST) {
      PLOG(ERROR) << "Failed to open service " << kWindowsServiceName;
      return false;
    }

    // Passing nullptr for lpServiceStartName configures CreateServiceW to use
    // the LocalSystem account.
    service.Set(::CreateServiceW(
        scm.Get(), kWindowsServiceName, kServiceDisplayName, SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
        command_line_str.c_str(),
        /*lpLoadOrderGroup=*/nullptr, /*lpdwTagId=*/nullptr,
        /*lpDependencies=*/nullptr, /*lpServiceStartName=*/nullptr,
        /*lpPassword=*/nullptr));
    if (!service.is_valid()) {
      PLOG(ERROR) << "Failed to create service " << kWindowsServiceName;
      return false;
    }
  }

  ConfigureServiceProperties(service.Get());
  return true;
}

bool StartHostService() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ScopedScHandle scm(
      ::OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT));
  if (!scm.is_valid()) {
    PLOG(ERROR) << "Failed to open Service Control Manager";
    return false;
  }

  ScopedScHandle service(::OpenServiceW(scm.Get(), kWindowsServiceName,
                                        SERVICE_START | SERVICE_QUERY_STATUS));
  if (!service.is_valid()) {
    PLOG(ERROR) << "Failed to open service " << kWindowsServiceName;
    return false;
  }

  if (!::StartServiceW(service.Get(), 0, nullptr)) {
    DWORD error = ::GetLastError();
    if (error != ERROR_SERVICE_ALREADY_RUNNING) {
      PLOG(ERROR) << "Failed to start service " << kWindowsServiceName;
      return false;
    }
  }

  return true;
}

bool StopHostService() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ScopedScHandle scm(
      ::OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT));
  if (!scm.is_valid()) {
    PLOG(ERROR) << "Failed to open Service Control Manager";
    return false;
  }

  ScopedScHandle service(::OpenServiceW(scm.Get(), kWindowsServiceName,
                                        SERVICE_STOP | SERVICE_QUERY_STATUS));
  if (!service.is_valid()) {
    if (::GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
      return true;
    }
    PLOG(ERROR) << "Failed to open service " << kWindowsServiceName;
    return false;
  }

  SERVICE_STATUS status = {};
  bool stop_sent = false;
  if (::ControlService(service.Get(), SERVICE_CONTROL_STOP, &status)) {
    if (status.dwCurrentState == SERVICE_STOPPED) {
      return true;
    }
    stop_sent = true;
  } else {
    DWORD error = ::GetLastError();
    if (error == ERROR_SERVICE_NOT_ACTIVE) {
      return true;
    }
    if (error != ERROR_SERVICE_CANNOT_ACCEPT_CTRL) {
      PLOG(ERROR) << "Failed to stop service " << kWindowsServiceName;
      return false;
    }
  }

  base::TimeTicks deadline = base::TimeTicks::Now() + kServiceStopTimeout;
  while (base::TimeTicks::Now() < deadline) {
    if (!::QueryServiceStatus(service.Get(), &status)) {
      PLOG(ERROR) << "Failed to query service status";
      return false;
    }
    if (status.dwCurrentState == SERVICE_STOPPED) {
      return true;
    }
    // If the service was in SERVICE_START_PENDING when ControlService was first
    // called, re-send SERVICE_CONTROL_STOP once it transitions to running.
    if (!stop_sent && status.dwCurrentState != SERVICE_START_PENDING &&
        status.dwCurrentState != SERVICE_STOP_PENDING) {
      if (::ControlService(service.Get(), SERVICE_CONTROL_STOP, &status)) {
        stop_sent = true;
      } else if (::GetLastError() == ERROR_SERVICE_NOT_ACTIVE) {
        return true;
      }
    }
    base::PlatformThread::Sleep(kServicePollInterval);
  }

  LOG(ERROR) << "Timed out waiting for service " << kWindowsServiceName
             << " to stop.";
  return false;
}

bool UninstallHostService() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!StopHostService()) {
    LOG(WARNING) << "Proceeding with service deletion despite stop failure.";
  }

  ScopedScHandle scm(
      ::OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT));
  if (!scm.is_valid()) {
    PLOG(ERROR) << "Failed to open Service Control Manager";
    return false;
  }

  ScopedScHandle service(
      ::OpenServiceW(scm.Get(), kWindowsServiceName, DELETE));
  if (!service.is_valid()) {
    if (::GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
      return true;
    }
    PLOG(ERROR) << "Failed to open service " << kWindowsServiceName
                << " for deletion";
    return false;
  }

  if (!::DeleteService(service.Get())) {
    DWORD error = ::GetLastError();
    if (error != ERROR_SERVICE_MARKED_FOR_DELETE) {
      PLOG(ERROR) << "Failed to delete service " << kWindowsServiceName;
      return false;
    }
  }

  return true;
}

}  // namespace remoting::installer
