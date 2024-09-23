// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/user_agent.h"

#include <stdint.h>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/util/chromium_git_revision.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "ui/base/device_form_factor.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
#include <sys/utsname.h>
#endif

namespace content {

namespace {

const char kFrozenUserAgentTemplate[] =
    "Mozilla/5.0 (%s) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s.0.0.0 "
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    "%s"
#endif
    "Safari/537.36";

std::string GetUserAgentPlatform() {
#if BUILDFLAG(IS_WIN)
  return "";
#elif BUILDFLAG(IS_MAC)
  return "Macintosh; ";
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return "X11; ";  // strange, but that's what Firefox uses
#elif BUILDFLAG(IS_ANDROID)
  return "Linux; ";
#elif BUILDFLAG(IS_FUCHSIA)
  return "";
#elif BUILDFLAG(IS_IOS)
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
             ? "iPad; "
             : "iPhone; ";
#else
#error Unsupported platform
#endif
}

std::string GetUnifiedPlatform() {
#if BUILDFLAG(IS_ANDROID)
  return "Linux; Android 10; K";
#elif BUILDFLAG(IS_CHROMEOS)
  return "X11; CrOS x86_64 14541.0.0";
#elif BUILDFLAG(IS_MAC)
  return "Macintosh; Intel Mac OS X 10_15_7";
#elif BUILDFLAG(IS_WIN)
  return "Windows NT 10.0; Win64; x64";
#elif BUILDFLAG(IS_FUCHSIA)
  return "Fuchsia";
#elif BUILDFLAG(IS_LINUX)
  return "X11; Linux x86_64";
#elif BUILDFLAG(IS_IOS)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return "iPad; CPU iPad OS 14_0 like Mac OS X";
  }
  return "iPhone; CPU iPhone OS 14_0 like Mac OS X";
#else
#error Unsupported platform
#endif
}

}  // namespace

std::string GetUnifiedPlatformForTesting() {
  return GetUnifiedPlatform();
}

// Inaccurately named for historical reasons
std::string GetWebKitVersion() {
  return base::StringPrintf("537.36 (%s)", CHROMIUM_GIT_REVISION);
}

std::string GetChromiumGitRevision() {
  return CHROMIUM_GIT_REVISION;
}

std::string BuildCpuInfo() {
  std::string cpuinfo;

#if BUILDFLAG(IS_MAC)
  cpuinfo = "Intel";
#elif BUILDFLAG(IS_IOS)
  cpuinfo = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
                ? "iPad"
                : "iPhone";
#elif BUILDFLAG(IS_WIN)
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->IsWowX86OnAMD64()) {
    cpuinfo = "WOW64";
  } else {
    base::win::OSInfo::WindowsArchitecture windows_architecture =
        os_info->GetArchitecture();
    if (windows_architecture == base::win::OSInfo::X64_ARCHITECTURE)
      cpuinfo = "Win64; x64";
    else if (windows_architecture == base::win::OSInfo::IA64_ARCHITECTURE)
      cpuinfo = "Win64; IA64";
  }
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);

  // special case for biarch systems
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32_t)) {
    cpuinfo.assign("i686 (x86_64)");
  } else {
    cpuinfo.assign(unixinfo.machine);
  }
#endif

  return cpuinfo;
}

// Return the CPU architecture in Windows/Mac/POSIX/Fuchsia and the empty string
// on Android or if unknown.
std::string GetCpuArchitecture() {
#if BUILDFLAG(IS_WIN)
  base::win::OSInfo::WindowsArchitecture windows_architecture =
      base::win::OSInfo::GetInstance()->GetArchitecture();
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  // When running a Chrome x86_64 (AMD64) build on an ARM64 device,
  // the OS lies and returns 0x9 (PROCESSOR_ARCHITECTURE_AMD64)
  // for wProcessorArchitecture.
  if (windows_architecture == base::win::OSInfo::ARM64_ARCHITECTURE ||
      os_info->IsWowX86OnARM64() || os_info->IsWowAMD64OnARM64()) {
    return "arm";
  } else if ((windows_architecture == base::win::OSInfo::X86_ARCHITECTURE) ||
             (windows_architecture == base::win::OSInfo::X64_ARCHITECTURE)) {
    return "x86";
  }
#elif BUILDFLAG(IS_MAC)
  base::mac::CPUType cpu_type = base::mac::GetCPUType();
  if (cpu_type == base::mac::CPUType::kIntel) {
    return "x86";
  } else if (cpu_type == base::mac::CPUType::kArm ||
             cpu_type == base::mac::CPUType::kTranslatedIntel) {
    return "arm";
  }
#elif BUILDFLAG(IS_IOS)
  return "arm";
#elif BUILDFLAG(IS_ANDROID)
  return std::string();
#elif BUILDFLAG(IS_POSIX)
  std::string cpu_info = BuildCpuInfo();
  if (base::StartsWith(cpu_info, "arm") ||
      base::StartsWith(cpu_info, "aarch")) {
    return "arm";
  } else if ((base::StartsWith(cpu_info, "i") &&
              cpu_info.substr(2, 2) == "86") ||
             base::StartsWith(cpu_info, "x86")) {
    return "x86";
  }
#elif BUILDFLAG(IS_FUCHSIA)
  std::string cpu_arch = base::SysInfo::ProcessCPUArchitecture();
  if (base::StartsWith(cpu_arch, "x86")) {
    return "x86";
  } else if (base::StartsWith(cpu_arch, "ARM")) {
    return "arm";
  }
#else
#error Unsupported platform
#endif
  DLOG(WARNING) << "Unrecognized CPU Architecture";
  return std::string();
}

// Return the CPU bitness in Windows/Mac/POSIX/Fuchsia and the empty string
// on Android.
std::string GetCpuBitness() {
#if BUILDFLAG(IS_WIN)
  return (base::win::OSInfo::GetInstance()->GetArchitecture() ==
          base::win::OSInfo::X86_ARCHITECTURE)
             ? "32"
             : "64";
#elif BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
  return "64";
#elif BUILDFLAG(IS_ANDROID)
  return std::string();
#elif BUILDFLAG(IS_POSIX)
  return base::Contains(BuildCpuInfo(), "64") ? "64" : "32";
#else
#error Unsupported platform
#endif
}

std::string GetOSVersion(IncludeAndroidBuildNumber include_android_build_number,
                         IncludeAndroidModel include_android_model) {
  std::string os_version;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS)
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);

#if BUILDFLAG(IS_MAC)
  // A significant amount of web content breaks if the reported "Mac
  // OS X" major version number is greater than 10. Continue to report
  // this as 10_15_7, the last dot release for that macOS version.
  if (os_major_version > 10) {
    os_major_version = 10;
    os_minor_version = 15;
    os_bugfix_version = 7;
  }
#endif

#endif

#if BUILDFLAG(IS_ANDROID)
  std::string android_version_str = base::SysInfo::OperatingSystemVersion();
  std::string android_info_str =
      GetAndroidOSInfo(include_android_build_number, include_android_model);
#endif

  base::StringAppendF(&os_version,
#if BUILDFLAG(IS_WIN)
                      "%d.%d", os_major_version, os_minor_version
#elif BUILDFLAG(IS_MAC)
                      "%d_%d_%d", os_major_version, os_minor_version,
                      os_bugfix_version
#elif BUILDFLAG(IS_IOS)
                      "%d_%d", os_major_version, os_minor_version
#elif BUILDFLAG(IS_CHROMEOS)
                      "%d.%d.%d", os_major_version, os_minor_version,
                      os_bugfix_version
#elif BUILDFLAG(IS_ANDROID)
                      "%s%s", android_version_str.c_str(),
                      android_info_str.c_str()
#else
                      ""
#endif
  );
  return os_version;
}

std::string BuildOSCpuInfo(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model) {
  return BuildOSCpuInfoFromOSVersionAndCpuType(
      GetOSVersion(include_android_build_number, include_android_model),
      BuildCpuInfo());
}

std::string BuildOSCpuInfoFromOSVersionAndCpuType(const std::string& os_version,
                                                  const std::string& cpu_type) {
  std::string os_cpu;

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);
#endif

#if BUILDFLAG(IS_WIN)
  if (!cpu_type.empty()) {
    base::StringAppendF(&os_cpu, "Windows NT %s; %s", os_version.c_str(),
                        cpu_type.c_str());
  } else {
    base::StringAppendF(&os_cpu, "Windows NT %s", os_version.c_str());
  }
#else
  base::StringAppendF(&os_cpu,
#if BUILDFLAG(IS_MAC)
                      "%s Mac OS X %s", cpu_type.c_str(), os_version.c_str()
#elif BUILDFLAG(IS_CHROMEOS)
                      "CrOS "
                      "%s %s",
                      cpu_type.c_str(),  // e.g. i686
                      os_version.c_str()
#elif BUILDFLAG(IS_ANDROID)
                      "Android %s", os_version.c_str()
#elif BUILDFLAG(IS_FUCHSIA)
                      "Fuchsia"
#elif BUILDFLAG(IS_IOS)
                      "CPU %s OS %s like Mac OS X", cpu_type.c_str(),
                      os_version.c_str()
#elif BUILDFLAG(IS_POSIX)
                      "%s %s",
                      unixinfo.sysname,  // e.g. Linux
                      cpu_type.c_str()   // e.g. i686
#endif
  );
#endif

  return os_cpu;
}

std::string GetReducedUserAgent(bool mobile, std::string major_version) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // There is an extra field in the template on Mobile.
  std::string device_compat;
  // Note: The extra space after Mobile is meaningful here, to avoid
  // "MobileSafari", but unneeded for non-mobile Android devices.
  device_compat = mobile ? "Mobile " : "";
#endif
  std::string user_agent =
      base::StringPrintf(kFrozenUserAgentTemplate, GetUnifiedPlatform().c_str(),
                         major_version.c_str()
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
                             ,
                         device_compat.c_str()
#endif
      );

  return user_agent;
}

std::string BuildUnifiedPlatformUserAgentFromProduct(
    const std::string& product) {
  std::string os_info;
  base::StringAppendF(&os_info, "%s", GetUnifiedPlatform().c_str());
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

std::string BuildUserAgentFromProduct(const std::string& product) {
  std::string os_info;
  base::StringAppendF(&os_info, "%s%s", GetUserAgentPlatform().c_str(),
                      BuildOSCpuInfo(IncludeAndroidBuildNumber::Exclude,
                                     IncludeAndroidModel::Include)
                          .c_str());
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

std::string BuildModelInfo() {
  std::string model;
#if BUILDFLAG(IS_ANDROID)
  // Only send the model information if on the release build of Android,
  // matching user agent behaviour.
  if (base::SysInfo::GetAndroidBuildCodename() == "REL")
    model = base::SysInfo::HardwareModelName();
#endif
  return model;
}

#if BUILDFLAG(IS_ANDROID)
std::string BuildUserAgentFromProductAndExtraOSInfo(
    const std::string& product,
    const std::string& extra_os_info,
    IncludeAndroidBuildNumber include_android_build_number) {
  std::string os_info;
  base::StrAppend(&os_info, {GetUserAgentPlatform(),
                             BuildOSCpuInfo(include_android_build_number,
                                            IncludeAndroidModel::Include),
                             extra_os_info});
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

std::string BuildUnifiedPlatformUAFromProductAndExtraOs(
    const std::string& product,
    const std::string& extra_os_info) {
  std::string os_info;
  base::StrAppend(&os_info, {GetUnifiedPlatform(), extra_os_info});
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

std::string GetAndroidOSInfo(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model) {
  std::string android_info_str;

  // Send information about the device.
  bool semicolon_inserted = false;
  if (include_android_model == IncludeAndroidModel::Include) {
    std::string android_device_name = BuildModelInfo();
    if (!android_device_name.empty()) {
      android_info_str += "; " + android_device_name;
      semicolon_inserted = true;
    }
  }

  // Append the build ID.
  if (include_android_build_number == IncludeAndroidBuildNumber::Include) {
    std::string android_build_id = base::SysInfo::GetAndroidBuildID();
    if (!android_build_id.empty()) {
      if (!semicolon_inserted)
        android_info_str += ";";
      android_info_str += " Build/" + android_build_id;
    }
  }

  return android_info_str;
}
#endif  // BUILDFLAG(IS_ANDROID)

std::string BuildUserAgentFromOSAndProduct(const std::string& os_info,
                                           const std::string& product) {
  // Derived from Safari's UA string.
  // This is done to expose our product name in a manner that is maximally
  // compatible with Safari, we hope!!
  std::string user_agent;
  base::StringAppendF(&user_agent,
                      "Mozilla/5.0 (%s) AppleWebKit/537.36 (KHTML, like Gecko) "
                      "%s Safari/537.36",
                      os_info.c_str(), product.c_str());
  return user_agent;
}

bool IsWoW64() {
#if BUILDFLAG(IS_WIN)
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  return os_info->IsWowX86OnAMD64();
#else
  return false;
#endif
}

}  // namespace content
