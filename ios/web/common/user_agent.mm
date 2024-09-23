// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/user_agent.h"

#import <UIKit/UIKit.h>

#import <stddef.h>
#import <stdint.h>
#import <sys/sysctl.h>
#import <string>

#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "ios/web/common/features.h"

namespace {

// The Desktop OS version should always remain 10_15_7, to be consisent with
// Chrome, Firefox, and Safari: https://crbug.com/40167872
const char kDesktopUserAgentProductPlaceholder[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
    "AppleWebKit/605.1.15 (KHTML, like Gecko) %s"
    "Version/11.1.1 "
    "Safari/605.1.15";

// UserAgentType description strings.
const char kUserAgentTypeAutomaticDescription[] = "AUTOMATIC";
const char kUserAgentTypeNoneDescription[] = "NONE";
const char kUserAgentTypeMobileDescription[] = "MOBILE";
const char kUserAgentTypeDesktopDescription[] = "DESKTOP";

std::string OSVersion() {
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;

  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);

  std::string os_version;

  if (base::FeatureList::IsEnabled(web::features::kUserAgentBugFixVersion)) {
    base::StringAppendF(&os_version, "%d_%d_%d", os_major_version,
                        os_minor_version, os_bugfix_version);
  } else {
    base::StringAppendF(&os_version, "%d_%d", os_major_version,
                        os_minor_version);
  }

  return os_version;
}

}  // namespace

namespace web {

std::string GetUserAgentTypeDescription(UserAgentType type) {
  switch (type) {
    case UserAgentType::AUTOMATIC:
      return std::string(kUserAgentTypeAutomaticDescription);
    case UserAgentType::NONE:
      return std::string(kUserAgentTypeNoneDescription);
    case UserAgentType::MOBILE:
      return std::string(kUserAgentTypeMobileDescription);
    case UserAgentType::DESKTOP:
      return std::string(kUserAgentTypeDesktopDescription);
  }
}

UserAgentType GetUserAgentTypeWithDescription(const std::string& description) {
  if (description == std::string(kUserAgentTypeMobileDescription))
    return UserAgentType::MOBILE;
  if (description == std::string(kUserAgentTypeDesktopDescription))
    return UserAgentType::DESKTOP;
  if (description == std::string(kUserAgentTypeAutomaticDescription))
    return UserAgentType::AUTOMATIC;
  return UserAgentType::NONE;
}

std::string BuildOSCpuInfo() {
  std::string os_cpu;
  // Remove the end of the platform name. For example "iPod touch" becomes
  // "iPod".
  std::string platform =
      base::SysNSStringToUTF8([[UIDevice currentDevice] model]);
  size_t position = platform.find_first_of(" ");
  if (position != std::string::npos)
    platform = platform.substr(0, position);

  base::StringAppendF(&os_cpu, "%s; CPU %s %s like Mac OS X", platform.c_str(),
                      (platform == "iPad") ? "OS" : "iPhone OS",
                      OSVersion().c_str());

  return os_cpu;
}

std::string BuildDesktopUserAgent(const std::string& desktop_product) {
  std::string product = desktop_product;
  if (!desktop_product.empty()) {
    // In case the product isn't empty, add a space after it.
    product = product + " ";
  }
  std::string user_agent;
  base::StringAppendF(&user_agent, kDesktopUserAgentProductPlaceholder,
                      product.c_str());
  return user_agent;
}

std::string BuildMobileUserAgent(const std::string& mobile_product) {
  std::string user_agent;
  base::StringAppendF(&user_agent,
                      "Mozilla/5.0 (%s) AppleWebKit/605.1.15"
                      " (KHTML, like Gecko) %s Mobile/15E148 Safari/604.1",
                      BuildOSCpuInfo().c_str(), mobile_product.c_str());

  return user_agent;
}

}  // namespace web
