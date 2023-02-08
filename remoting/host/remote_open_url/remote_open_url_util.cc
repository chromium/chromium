// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/remote_open_url_util.h"

#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#endif

namespace remoting {

#if BUILDFLAG(IS_WIN)

#if defined(OFFICIAL_BUILD)
const wchar_t kUrlForwarderRegisteredAppName[] =
    L"Chrome Remote Desktop URL Forwarder";
#else
const wchar_t kUrlForwarderRegisteredAppName[] = L"Chromoting URL Forwarder";
#endif

const wchar_t kRegisteredApplicationsKeyName[] =
    L"SOFTWARE\\RegisteredApplications";

#endif  // BUILDFLAG(IS_WIN)

bool IsRemoteOpenUrlSupported() {
#if BUILDFLAG(IS_LINUX)
  return true;
#elif BUILDFLAG(IS_WIN)
  // The MSI installs the ProgID and capabilities into registry, but not the
  // entry in RegisteredApplications, which must be applied out of band to
  // enable the feature.
  base::win::RegKey registered_apps_key;
  LONG result = registered_apps_key.Open(
      HKEY_LOCAL_MACHINE, kRegisteredApplicationsKeyName, KEY_READ);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to determine whether URL forwarding is supported "
                  "since registry key HKLM\\"
               << kRegisteredApplicationsKeyName
               << "cannot be opened. Result: " << result;
    return false;
  }
  return registered_apps_key.HasValue(kUrlForwarderRegisteredAppName);
#else
  // Not supported on other platforms.
  return false;
#endif
}

}  // namespace remoting
