// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/user_agent.h"

#import <UIKit/UIKit.h>

#include <stddef.h>
#include <stdint.h>
#include <sys/sysctl.h>
#include <string>

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// UserAgentType description strings.
const char kUserAgentTypeNoneDescription[] = "NONE";
const char kUserAgentTypeMobileDescription[] = "MOBILE";
const char kUserAgentTypeDesktopDescription[] = "DESKTOP";

struct UAVersions {
  const char* safari_version_string;
  const char* webkit_version_string;
};

struct OSVersionMap {
  int32_t major_os_version;
  int32_t minor_os_version;
  UAVersions ua_versions;
};

const UAVersions& GetUAVersionsForCurrentOS() {
  // The WebKit version can be extracted dynamically from UIWebView, but the
  // Safari version can't be, so a lookup table is used instead (for both, since
  // the reported versions should stay in sync).
  static const OSVersionMap version_map[] = {
      {12, 0, {"605.1", "605.1.15"}},
      {11, 0, {"604.1", "604.1.34"}},
      {10, 3, {"602.1", "603.1.30"}},
      {10, 0, {"602.1", "602.1.50"}},
      {9, 0, {"601.1.46", "601.1"}},
  };

  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);

  // Return the versions corresponding to the first (and thus highest) OS
  // version less than or equal to the given OS version.
  for (unsigned int i = 0; i < base::size(version_map); ++i) {
    if (os_major_version > version_map[i].major_os_version ||
        (os_major_version == version_map[i].major_os_version &&
         os_minor_version >= version_map[i].minor_os_version))
      return version_map[i].ua_versions;
  }
  NOTREACHED();
  return version_map[base::size(version_map) - 1].ua_versions;
}

std::string BuildKernelVersion() {
  // Freeze the kernel version for iOS 11.3 and later (as Safari does).
  if (@available(iOS 11.3, *))
    return "15E148";

  int mib[2] = {CTL_KERN, KERN_OSVERSION};
  unsigned int namelen = sizeof(mib) / sizeof(mib[0]);
  size_t bufferSize = 0;
  sysctl(mib, namelen, nullptr, &bufferSize, nullptr, 0);
  char kernel_version[bufferSize];
  int result = sysctl(mib, namelen, kernel_version, &bufferSize, nullptr, 0);
  DCHECK(result == 0);
  return kernel_version;
}

}  // namespace

namespace web {

std::string GetUserAgentTypeDescription(UserAgentType type) {
  switch (type) {
    case UserAgentType::NONE:
      return std::string(kUserAgentTypeNoneDescription);
      break;
    case UserAgentType::MOBILE:
      return std::string(kUserAgentTypeMobileDescription);
      break;
    case UserAgentType::DESKTOP:
      return std::string(kUserAgentTypeDesktopDescription);
  }
}

UserAgentType GetUserAgentTypeWithDescription(const std::string& description) {
  if (description == std::string(kUserAgentTypeMobileDescription))
    return UserAgentType::MOBILE;
  if (description == std::string(kUserAgentTypeDesktopDescription))
    return UserAgentType::DESKTOP;
  return UserAgentType::NONE;
}

std::string BuildOSCpuInfo() {
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);

  // Drop bugfix version for iOS 11.3 and later (as Safari does).
  if (@available(iOS 11.3, *))
    os_bugfix_version = 0;

  std::string os_version;
  if (os_bugfix_version == 0) {
    base::StringAppendF(&os_version,
                        "%d_%d",
                        os_major_version,
                        os_minor_version);
  } else {
    base::StringAppendF(&os_version,
                        "%d_%d_%d",
                        os_major_version,
                        os_minor_version,
                        os_bugfix_version);
  }

  // Remove the end of the platform name. For example "iPod touch" becomes
  // "iPod".
  std::string platform = base::SysNSStringToUTF8(
      [[UIDevice currentDevice] model]);
  size_t position = platform.find_first_of(" ");
  if (position != std::string::npos)
    platform = platform.substr(0, position);

  std::string os_cpu;
  base::StringAppendF(
      &os_cpu,
      "%s; CPU %s %s like Mac OS X",
      platform.c_str(),
      (platform == "iPad") ? "OS" : "iPhone OS",
      os_version.c_str());

  return os_cpu;
}

std::string BuildUserAgentFromProduct(const std::string& product) {
  UAVersions ua_versions = GetUAVersionsForCurrentOS();

  std::string user_agent;
  base::StringAppendF(&user_agent,
                      "Mozilla/5.0 (%s) AppleWebKit/%s"
                      " (KHTML, like Gecko) %s Mobile/%s Safari/%s",
                      BuildOSCpuInfo().c_str(),
                      ua_versions.webkit_version_string, product.c_str(),
                      BuildKernelVersion().c_str(),
                      ua_versions.safari_version_string);

  return user_agent;
}

}  // namespace web
