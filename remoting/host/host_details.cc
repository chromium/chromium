// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_details.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#endif

namespace remoting {

// Get the host Operating System Name, removing the need to check for OS
// definitions and keeps the keys used consistent.
std::string GetHostOperatingSystemName() {
#if defined(OS_WIN)
  return "Windows";
#elif defined(OS_MACOSX)
  return "Mac";
#elif defined(OS_CHROMEOS)
  return "ChromeOS";
#elif defined(OS_LINUX)
  return "Linux";
#elif defined(OS_ANDROID)
  return "Android";
#else
#error "Unsupported host OS"
#endif
}

// Get the host Operating System Version, removing the need to check for OS
// definitions and keeps the format used consistent.
std::string GetHostOperatingSystemVersion() {
#if defined(OS_LINUX)
  return base::GetLinuxDistro();
#else
  return base::SysInfo::OperatingSystemVersion();
#endif
}

}  // namespace remoting
