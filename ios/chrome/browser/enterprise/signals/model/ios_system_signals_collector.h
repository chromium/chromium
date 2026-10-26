// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_IOS_SYSTEM_SIGNALS_COLLECTOR_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_IOS_SYSTEM_SIGNALS_COLLECTOR_H_

#import <memory>
#import <string>

#import "base/containers/flat_set.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/system/sys_info.h"
#import "base/time/time.h"
#import "components/device_signals/core/browser/base_signals_collector.h"
#import "components/device_signals/core/browser/signals_types.h"

@protocol ReauthenticationProtocol;

namespace policy {
class CloudPolicyManager;
}  // namespace policy

class IOSSystemSignalsCollector : public device_signals::BaseSignalsCollector {
 public:
  using DeviceAffiliationIdsCallback =
      base::RepeatingCallback<base::flat_set<std::string>()>;

  // `device_cloud_policy_manager` is the browser-level (CBCM) policy manager,
  // used to report the device enrollment domain. May be null when the browser
  // is not enrolled; it must outlive this collector otherwise.
  IOSSystemSignalsCollector(
      DeviceAffiliationIdsCallback device_affiliation_ids_callback,
      policy::CloudPolicyManager* device_cloud_policy_manager,
      id<ReauthenticationProtocol> reauth_module);
  ~IOSSystemSignalsCollector() override;

  IOSSystemSignalsCollector(const IOSSystemSignalsCollector&) = delete;
  IOSSystemSignalsCollector& operator=(const IOSSystemSignalsCollector&) =
      delete;

 private:
  void GetOsSignals(device_signals::UserPermission permission,
                    const device_signals::SignalsAggregationRequest& request,
                    device_signals::SignalsAggregationResponse& response,
                    base::OnceClosure done_closure);

  void OnHardwareInfoReceived(
      base::TimeTicks start_time,
      device_signals::SignalsAggregationResponse& response,
      std::unique_ptr<device_signals::OsSignalsResponse> signal_response,
      base::OnceClosure done_closure,
      base::SysInfo::HardwareInfo hardware_info);

  const DeviceAffiliationIdsCallback device_affiliation_ids_callback_;
  const raw_ptr<policy::CloudPolicyManager> device_cloud_policy_manager_;
  const id<ReauthenticationProtocol> reauth_module_;
  base::WeakPtrFactory<IOSSystemSignalsCollector> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_IOS_SYSTEM_SIGNALS_COLLECTOR_H_
