// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/user_agent.h"

#include <stdint.h>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/util/webkit_version.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
#include <sys/utsname.h>
#endif

namespace content {

namespace {

#if defined(OS_ANDROID)
const base::Feature kAndroidUserAgentStringContainsBuildId{
    "AndroidUserAgentStringContainsBuildId", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

std::string GetUserAgentPlatform() {
#if defined(OS_WIN)
  return "";
#elif defined(OS_MACOSX)
  return "Macintosh; ";
#elif defined(USE_X11) || defined(USE_OZONE)
  return "X11; ";  // strange, but that's what Firefox uses
#elif defined(OS_ANDROID)
  return "Linux; ";
#elif defined(OS_FUCHSIA)
  // TODO(https://crbug.com/1010256): Sites get confused into serving mobile
  // content if we report only "Fuchsia".
  return "X11; ";
#elif defined(OS_POSIX)
  return "Unknown; ";
#endif
}

}  // namespace

std::string GetWebKitVersion() {
  return base::StringPrintf("%d.%d (%s)",
                            WEBKIT_VERSION_MAJOR,
                            WEBKIT_VERSION_MINOR,
                            WEBKIT_SVN_REVISION);
}

std::string GetWebKitRevision() {
  return WEBKIT_SVN_REVISION;
}

std::string BuildOSCpuInfo(bool include_android_build_number) {
  std::string os_cpu;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS) ||\
    defined(OS_ANDROID)
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
#endif

#if defined(OS_WIN)
  std::string architecture_token;
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->wow64_status() == base::win::OSInfo::WOW64_ENABLED) {
    architecture_token = "; WOW64";
  } else {
    base::win::OSInfo::WindowsArchitecture windows_architecture =
        os_info->GetArchitecture();
    if (windows_architecture == base::win::OSInfo::X64_ARCHITECTURE)
      architecture_token = "; Win64; x64";
    else if (windows_architecture == base::win::OSInfo::IA64_ARCHITECTURE)
      architecture_token = "; Win64; IA64";
  }
#elif defined(OS_ANDROID)
  std::string android_version_str = base::SysInfo::OperatingSystemVersion();
  std::string android_info_str = GetAndroidOSInfo(include_android_build_number);
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);

  std::string cputype;
  // special case for biarch systems
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32_t)) {
    cputype.assign("i686 (x86_64)");
  } else {
    cputype.assign(unixinfo.machine);
  }
#endif

  base::StringAppendF(&os_cpu,
#if defined(OS_WIN)
                      "Windows NT %d.%d%s", os_major_version, os_minor_version,
                      architecture_token.c_str()
#elif defined(OS_MACOSX)
                      "Intel Mac OS X %d_%d_%d", os_major_version,
                      os_minor_version, os_bugfix_version
#elif defined(OS_CHROMEOS)
                      "CrOS "
                      "%s %d.%d.%d",
                      cputype.c_str(),  // e.g. i686
                      os_major_version, os_minor_version, os_bugfix_version
#elif defined(OS_ANDROID)
                      "Android %s%s", android_version_str.c_str(),
                      android_info_str.c_str()
#elif defined(OS_FUCHSIA)
                      "Fuchsia"
#elif defined(OS_POSIX)
                      "%s %s",
                      unixinfo.sysname,  // e.g. Linux
                      cputype.c_str()    // e.g. i686
#endif
  );

  return os_cpu;
}

base::StringPiece GetFrozenUserAgent(bool mobile) {
#if defined(OS_ANDROID)
  return mobile ? frozen_user_agent_strings::kAndroidMobile
                : frozen_user_agent_strings::kAndroid;
#endif
  return frozen_user_agent_strings::kDesktop;
}

std::string BuildUserAgentFromProduct(const std::string& product) {
  std::string os_info;
  base::StringAppendF(&os_info, "%s%s", GetUserAgentPlatform().c_str(),
                      BuildOSCpuInfo(false).c_str());
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

#if defined(OS_ANDROID)
std::string BuildUserAgentFromProductAndExtraOSInfo(
    const std::string& product,
    const std::string& extra_os_info,
    bool include_android_build_number) {
  std::string os_info;
  base::StringAppendF(&os_info, "%s%s%s", GetUserAgentPlatform().c_str(),
                      BuildOSCpuInfo(include_android_build_number).c_str(),
                      extra_os_info.c_str());
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

std::string GetAndroidOSInfo(bool include_android_build_number) {
  std::string android_info_str;

  // Send information about the device.
  bool semicolon_inserted = false;
  std::string android_build_codename = base::SysInfo::GetAndroidBuildCodename();
  std::string android_device_name = base::SysInfo::HardwareModelName();
  if (!android_device_name.empty() && "REL" == android_build_codename) {
    android_info_str += "; " + android_device_name;
    semicolon_inserted = true;
  }

  // Append the build ID.
  if (base::FeatureList::IsEnabled(kAndroidUserAgentStringContainsBuildId) ||
      include_android_build_number) {
    std::string android_build_id = base::SysInfo::GetAndroidBuildID();
    if (!android_build_id.empty()) {
      if (!semicolon_inserted)
        android_info_str += ";";
      android_info_str += " Build/" + android_build_id;
    }
  }

  return android_info_str;
}
#endif  // defined(OS_ANDROID)

std::string BuildUserAgentFromOSAndProduct(const std::string& os_info,
                                           const std::string& product) {
  // Derived from Safari's UA string.
  // This is done to expose our product name in a manner that is maximally
  // compatible with Safari, we hope!!
  std::string user_agent;
  base::StringAppendF(
      &user_agent,
      "Mozilla/5.0 (%s) AppleWebKit/%d.%d (KHTML, like Gecko) %s Safari/%d.%d",
      os_info.c_str(),
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR,
      product.c_str(),
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR);
  return user_agent;
}

}  // namespace content
