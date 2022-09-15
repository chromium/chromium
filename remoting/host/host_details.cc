// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_details.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/linux_util.h"
#endif

namespace remoting {

// Get the host Operating System Name, removing the need to check for OS
// definitions and keeps the keys used consistent.
std::string GetHostOperatingSystemName() {
#if BUILDFLAG(IS_WIN)
  return "Windows";
#elif BUILDFLAG(IS_APPLE)
  return "Mac";
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return "ChromeOS";
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return "Linux";
#elif BUILDFLAG(IS_ANDROID)
  return "Android";
#else
#error "Unsupported host OS"
#endif
}

// Get the host Operating System Version, removing the need to check for OS
// definitions and keeps the format used consistent.
std::string GetHostOperatingSystemVersion() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return base::GetLinuxDistro();
#else
  return base::SysInfo::OperatingSystemVersion();
#endif
}

}  // namespace remoting
