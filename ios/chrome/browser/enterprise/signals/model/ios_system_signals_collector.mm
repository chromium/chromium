// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/ios_system_signals_collector.h"

#import <string>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/system/sys_info.h"
#import "base/time/time.h"
#import "components/device_signals/core/browser/browser_utils.h"
#import "components/device_signals/core/browser/metrics_utils.h"
#import "components/device_signals/core/browser/signals_types.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/public/provider/chrome/browser/signin/device_identifier_api.h"

namespace {

constexpr char kIOSOperatingSystem[] = "iOS";

}  // namespace

IOSSystemSignalsCollector::IOSSystemSignalsCollector(
    DeviceAffiliationIdsCallback device_affiliation_ids_callback,
    policy::CloudPolicyManager* device_cloud_policy_manager,
    id<ReauthenticationProtocol> reauth_module)
    : device_signals::BaseSignalsCollector({
          {device_signals::SignalName::kOsSignals,
           base::BindRepeating(&IOSSystemSignalsCollector::GetOsSignals,
                               base::Unretained(this))},
      }),
      device_affiliation_ids_callback_(
          std::move(device_affiliation_ids_callback)),
      device_cloud_policy_manager_(device_cloud_policy_manager),
      reauth_module_(reauth_module) {
  CHECK(device_affiliation_ids_callback_);
  CHECK(reauth_module_);
}

IOSSystemSignalsCollector::~IOSSystemSignalsCollector() = default;

void IOSSystemSignalsCollector::GetOsSignals(
    device_signals::UserPermission permission,
    const device_signals::SignalsAggregationRequest& request,
    device_signals::SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  // Marks the start of the collection. The collection latency is measured
  // from here until the outcome is recorded in OnHardwareInfoReceived().
  const base::TimeTicks start_time = base::TimeTicks::Now();
  std::unique_ptr<device_signals::OsSignalsResponse> signal_response =
      std::make_unique<device_signals::OsSignalsResponse>();

  signal_response->browser_version =
      std::string(version_info::GetVersionNumber());
  const base::flat_set<std::string> device_affiliation_ids =
      device_affiliation_ids_callback_.Run();
  signal_response->device_affiliation_ids.assign(device_affiliation_ids.begin(),
                                                 device_affiliation_ids.end());
  // Null manager (not CBCM-enrolled) or a store without policy yields nullopt.
  signal_response->device_enrollment_domain =
      device_signals::TryGetEnrollmentDomain(device_cloud_policy_manager_);
  signal_response->operating_system = kIOSOperatingSystem;
  signal_response->vendor_id = ios::provider::GetDeviceIdentifier();
  device_signals::SettingValue screen_lock =
      [reauth_module_ canAttemptReauth]
          ? device_signals::SettingValue::ENABLED
          : device_signals::SettingValue::DISABLED;
  signal_response->screen_lock_secured = screen_lock;
  // `diskEncryption` mirrors `screenLockSecured` on iOS.
  signal_response->disk_encryption = screen_lock;

  // Fetch hardware info asynchronously to be safe.
  base::SysInfo::GetHardwareInfo(
      base::BindOnce(&IOSSystemSignalsCollector::OnHardwareInfoReceived,
                     weak_factory_.GetWeakPtr(), start_time, std::ref(response),
                     std::move(signal_response), std::move(done_closure)));
}

void IOSSystemSignalsCollector::OnHardwareInfoReceived(
    base::TimeTicks start_time,
    device_signals::SignalsAggregationResponse& response,
    std::unique_ptr<device_signals::OsSignalsResponse> signal_response,
    base::OnceClosure done_closure,
    base::SysInfo::HardwareInfo hardware_info) {
  signal_response->device_manufacturer = hardware_info.manufacturer;
  signal_response->device_model = hardware_info.model;
  signal_response->os_version = base::SysInfo::OperatingSystemVersion();

  response.os_signals_response = std::move(*signal_response);

  // OS signals collection has no failure path once the permission check has
  // passed, so the outcome is always recorded as a success. This also records
  // the collection latency, i.e. the time elapsed between `start_time` (when
  // the collection started) and now (when the outcome is known).
  device_signals::LogSignalCollectionSucceeded(
      device_signals::SignalName::kOsSignals, start_time,
      /*signal_collection_size=*/std::nullopt);

  std::move(done_closure).Run();
}
