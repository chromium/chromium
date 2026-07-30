// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/ios_system_signals_collector.h"

#import <string>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/system/sys_info.h"
#import "components/device_signals/core/browser/signals_types.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

namespace {

constexpr char kIOSOperatingSystem[] = "iOS";

device_signals::SettingValue GetScreenLockSecured() {
  ReauthenticationModule* auth_module = [[ReauthenticationModule alloc] init];
  return [auth_module canAttemptReauth]
             ? device_signals::SettingValue::ENABLED
             : device_signals::SettingValue::DISABLED;
}

}  // namespace

IOSSystemSignalsCollector::IOSSystemSignalsCollector()
    : device_signals::BaseSignalsCollector({
          {device_signals::SignalName::kOsSignals,
           base::BindRepeating(&IOSSystemSignalsCollector::GetOsSignals,
                               base::Unretained(this))},
      }) {}

IOSSystemSignalsCollector::~IOSSystemSignalsCollector() = default;

void IOSSystemSignalsCollector::GetOsSignals(
    device_signals::UserPermission permission,
    const device_signals::SignalsAggregationRequest& request,
    device_signals::SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  std::unique_ptr<device_signals::OsSignalsResponse> signal_response =
      std::make_unique<device_signals::OsSignalsResponse>();

  signal_response->browser_version =
      std::string(version_info::GetVersionNumber());
  signal_response->operating_system = kIOSOperatingSystem;

  device_signals::SettingValue screen_lock = GetScreenLockSecured();
  signal_response->screen_lock_secured = screen_lock;

  // `diskEncryption` mirrors `screenLockSecured` on iOS.
  signal_response->disk_encryption = screen_lock;

  // Fetch hardware info asynchronously to be safe.
  base::SysInfo::GetHardwareInfo(
      base::BindOnce(&IOSSystemSignalsCollector::OnHardwareInfoReceived,
                     weak_factory_.GetWeakPtr(), std::ref(response),
                     std::move(signal_response), std::move(done_closure)));
}

void IOSSystemSignalsCollector::OnHardwareInfoReceived(
    device_signals::SignalsAggregationResponse& response,
    std::unique_ptr<device_signals::OsSignalsResponse> signal_response,
    base::OnceClosure done_closure,
    base::SysInfo::HardwareInfo hardware_info) {
  signal_response->device_manufacturer = hardware_info.manufacturer;
  signal_response->device_model = hardware_info.model;
  signal_response->os_version = base::SysInfo::OperatingSystemVersion();

  response.os_signals_response = std::move(*signal_response);

  std::move(done_closure).Run();
}
