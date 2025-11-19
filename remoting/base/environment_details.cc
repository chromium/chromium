// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/environment_details.h"

#include "base/strings/stringize_macros.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "remoting/base/version.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/linux_util.h"
#endif

namespace remoting {

std::string GetBuildVersion() {
  return STRINGIZE(VERSION);
}

// Get the Operating System Name, removing the need to check for OS definitions
// and keeps the keys used consistent.
std::string GetOperatingSystemName() {
#if BUILDFLAG(IS_WIN)
  return "Windows";
#elif BUILDFLAG(IS_APPLE)
  return "Mac";
#elif BUILDFLAG(IS_CHROMEOS)
  return "ChromeOS";
#elif BUILDFLAG(IS_LINUX)
  return "Linux";
#elif BUILDFLAG(IS_ANDROID)
  return "Android";
#else
  return "Unsupported OS";
#endif
}

// Get the Operating System Version, removing the need to check for OS
// definitions and keeps the format used consistent.
std::string GetOperatingSystemVersion() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return base::GetLinuxDistro();
#else
  return base::SysInfo::OperatingSystemVersion();
#endif
}

}  // namespace remoting
