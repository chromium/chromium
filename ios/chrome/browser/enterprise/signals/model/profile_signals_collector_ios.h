// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_PROFILE_SIGNALS_COLLECTOR_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_PROFILE_SIGNALS_COLLECTOR_IOS_H_

#import "base/memory/raw_ptr.h"
#import "components/device_signals/core/browser/base_signals_collector.h"

class PrefService;

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace enterprise_connectors {
class ConnectorsService;
}  // namespace enterprise_connectors

namespace policy {
class UserCloudPolicyManager;
}  // namespace policy

// iOS implementation of BaseSignalsCollector that collects profile-level
// signals required for Enterprise reporting.
class ProfileSignalsCollectorIOS : public device_signals::BaseSignalsCollector {
 public:
  // `profile_prefs` and `policy_manager` must be non-null.
  // `profile_id_service` and `connectors_service` may be null.
  ProfileSignalsCollectorIOS(
      PrefService* profile_prefs,
      policy::UserCloudPolicyManager* policy_manager,
      enterprise::ProfileIdService* profile_id_service,
      enterprise_connectors::ConnectorsService* connectors_service);
  ~ProfileSignalsCollectorIOS() override;

  ProfileSignalsCollectorIOS(const ProfileSignalsCollectorIOS&) = delete;
  ProfileSignalsCollectorIOS& operator=(const ProfileSignalsCollectorIOS&) =
      delete;

 private:
  // Fetches the profile signals asynchronously and populates the response.
  void PopulateProfileSignals(
      device_signals::UserPermission permission,
      const device_signals::SignalsAggregationRequest& request,
      device_signals::SignalsAggregationResponse& response,
      base::OnceClosure done_closure);

  // Weak pointers to required services.
  raw_ptr<PrefService> profile_prefs_;
  raw_ptr<policy::UserCloudPolicyManager> policy_manager_;
  raw_ptr<enterprise::ProfileIdService> profile_id_service_;
  raw_ptr<enterprise_connectors::ConnectorsService> connectors_service_;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_PROFILE_SIGNALS_COLLECTOR_IOS_H_
