// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/platform_details.h"

#include "base/sys_info.h"
#include "build/build_config.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

namespace remoting {

// Get the Operating System Version, removing the need to check for OS
// definitions, to keep the format used consistent.
std::string GetOperatingSystemVersionString() {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (base::mac::IsAtLeastOS10_10()) {
    return base::SysInfo::OperatingSystemVersion();
  } else {
    // MacOS Hosts prior to 10.10 were reporting incorrect OS versions after the
    // removal of the 10.9 and lower checks back in ~M66.  Since we don't know
    // the exact version in this case, I've chosen a number that is obviously
    // not a valid MacOS OS version.  That way it will be easier to find if
    // someone is unaware of this problem and does a code search to find it.
    // See crbug.com/889259 for more context.
    return "10.9.9999";
  }
#else
  return base::SysInfo::OperatingSystemVersion();
#endif
}

}  // namespace remoting
