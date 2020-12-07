// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_CONFIGURATION_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_CONFIGURATION_IOS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace policy {

// The iOS implementation of the device management service configuration that is
// used to create device management service instances.
class DeviceManagementServiceConfigurationIOS
    : public DeviceManagementService::Configuration {
 public:
  DeviceManagementServiceConfigurationIOS(
      const std::string& dm_server_url,
      const std::string& realtime_reporting_server_url,
      const std::string& encrypted_reporting_server_url);
  DeviceManagementServiceConfigurationIOS(
      const DeviceManagementServiceConfigurationIOS&) = delete;
  DeviceManagementServiceConfigurationIOS& operator=(
      const DeviceManagementServiceConfigurationIOS&) = delete;
  ~DeviceManagementServiceConfigurationIOS() override;

  // DeviceManagementService::Configuration implementation.
  std::string GetDMServerUrl() override;
  std::string GetAgentParameter() override;
  std::string GetPlatformParameter() override;
  std::string GetRealtimeReportingServerUrl() override;
  std::string GetEncryptedReportingServerUrl() override;
  std::string GetReportingConnectorServerUrl(
      content::BrowserContext* context) override;

 private:
  const std::string dm_server_url_;
  const std::string realtime_reporting_server_url_;
  const std::string encrypted_reporting_server_url_;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_CONFIGURATION_IOS_H_
