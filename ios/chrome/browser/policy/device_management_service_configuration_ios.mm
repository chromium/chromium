// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/device_management_service_configuration_ios.h"

#import <stdint.h>

#import "base/logging.h"
#import "base/strings/stringprintf.h"
#import "base/system/sys_info.h"
#import "build/build_config.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "components/version_info/version_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace policy {

DeviceManagementServiceConfigurationIOS::
    DeviceManagementServiceConfigurationIOS(
        const std::string& dm_server_url,
        const std::string& realtime_reporting_server_url,
        const std::string& encrypted_reporting_server_url)
    : dm_server_url_(dm_server_url),
      realtime_reporting_server_url_(realtime_reporting_server_url),
      encrypted_reporting_server_url_(encrypted_reporting_server_url) {}

DeviceManagementServiceConfigurationIOS::
    ~DeviceManagementServiceConfigurationIOS() = default;

std::string DeviceManagementServiceConfigurationIOS::GetDMServerUrl() const {
  return dm_server_url_;
}

std::string DeviceManagementServiceConfigurationIOS::GetAgentParameter() const {
  return base::StringPrintf("%s %s(%s)", version_info::GetProductName().c_str(),
                            version_info::GetVersionNumber().c_str(),
                            version_info::GetLastChange().c_str());
}

std::string DeviceManagementServiceConfigurationIOS::GetPlatformParameter()
    const {
  std::string os_name = base::SysInfo::OperatingSystemName();
  std::string os_hardware = base::SysInfo::OperatingSystemArchitecture();

  std::string os_version("-");

  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);
  os_version = base::StringPrintf("%d.%d.%d", os_major_version,
                                  os_minor_version, os_bugfix_version);

  return base::StringPrintf("%s|%s|%s", os_name.c_str(), os_hardware.c_str(),
                            os_version.c_str());
}

std::string
DeviceManagementServiceConfigurationIOS::GetRealtimeReportingServerUrl() const {
  return realtime_reporting_server_url_;
}

std::string
DeviceManagementServiceConfigurationIOS::GetEncryptedReportingServerUrl()
    const {
  return encrypted_reporting_server_url_;
}

std::string
DeviceManagementServiceConfigurationIOS::GetReportingConnectorServerUrl(
    content::BrowserContext* context) const {
  return std::string();
}

}  // namespace policy
