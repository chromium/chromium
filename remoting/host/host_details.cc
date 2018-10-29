// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_details.h"

#include "build/build_config.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#include "base/sys_info.h"
#else
#include "remoting/base/platform_details.h"
#endif

namespace remoting {

// Get the host Operating System Name, removing the need to check for OS
// definitions and keeps the keys used consistant.
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
  return GetOperatingSystemVersionString();
#endif
}

}  // namespace remoting
