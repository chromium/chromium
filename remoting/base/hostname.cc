// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/hostname.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

// Needed for GetComputerNameExW/ComputerNameDnsFullyQualified.
#include <windows.h>
#else
#include "net/base/network_interfaces.h"
#endif

namespace remoting {

std::string GetHostname() {
#if BUILDFLAG(IS_WIN)
  wchar_t buffer[MAX_PATH] = {0};
  DWORD size = MAX_PATH;
  if (!::GetComputerNameExW(ComputerNameDnsFullyQualified, buffer, &size)) {
    PLOG(ERROR) << "GetComputerNameExW failed";
    return std::string();
  }
  std::string hostname;
  if (!base::WideToUTF8(buffer, size, &hostname)) {
    LOG(ERROR) << "Failed to convert from Wide to UTF8";
    return std::string();
  }
  return hostname;
#else
  return net::GetHostName();
#endif
}

}  // namespace remoting
