// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/spooler_win.h"

#include <windows.h>

#include <winspool.h>
#include <winsvc.h>

#include "base/logging.h"
#include "base/win/scoped_handle.h"

namespace printing::internal {

namespace {

class ScHandleTraits {
 public:
  typedef SC_HANDLE Handle;

  ScHandleTraits() = delete;
  ScHandleTraits(const ScHandleTraits&) = delete;
  ScHandleTraits& operator=(const ScHandleTraits&) = delete;

  static bool CloseHandle(SC_HANDLE handle) {
    return ::CloseServiceHandle(handle) != FALSE;
  }

  static bool IsHandleValid(SC_HANDLE handle) { return handle != nullptr; }

  static SC_HANDLE NullHandle() { return nullptr; }
};

typedef base::win::GenericScopedHandle<ScHandleTraits,
                                       base::win::DummyVerifierTraits>
    ScopedScHandle;

}  // namespace

SpoolerServiceStatus IsSpoolerRunning() {
  ScopedScHandle scm_handle(
      ::OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT));
  if (!scm_handle.IsValid()) {
    LOG(ERROR) << "Unable to connect to Windows Service Control Manager";
    return SpoolerServiceStatus::kUnknown;
  }

  ScopedScHandle sc_handle(
      ::OpenService(scm_handle.Get(), L"Spooler", SERVICE_QUERY_STATUS));
  if (!sc_handle.IsValid()) {
    LOG(ERROR) << "Unable to open Windows print spooler service";
    return SpoolerServiceStatus::kUnknown;
  }

  SERVICE_STATUS service_status;
  if (!::QueryServiceStatus(sc_handle.Get(), &service_status)) {
    LOG(ERROR) << "Unable to open Windows print spooler service";
    return SpoolerServiceStatus::kUnknown;
  }

  return service_status.dwCurrentState == SERVICE_RUNNING
             ? SpoolerServiceStatus::kRunning
             : SpoolerServiceStatus::kNotRunning;
}

}  // namespace printing::internal
