// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/profile_signals_collector_ios.h"

#import <string>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "components/device_signals/core/browser/browser_utils.h"
#import "components/device_signals/core/browser/signals_types.h"
#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"

namespace {

std::vector<std::string> GetProfileAffiliationIds(
    policy::UserCloudPolicyManager* policy_manager) {
  if (!policy_manager || !policy_manager->core()) {
    return {};
  }

  policy::CloudPolicyStore* policy_store = policy_manager->core()->store();
  if (!policy_store || !policy_store->has_policy() || !policy_store->policy()) {
    return {};
  }

  const enterprise_management::PolicyData* policy_data = policy_store->policy();
  return {policy_data->user_affiliation_ids().begin(),
          policy_data->user_affiliation_ids().end()};
}

}  // namespace

ProfileSignalsCollectorIOS::ProfileSignalsCollectorIOS(
    PrefService* profile_prefs,
    policy::UserCloudPolicyManager* policy_manager,
    enterprise::ProfileIdService* profile_id_service,
    enterprise_connectors::ConnectorsService* connectors_service)
    : device_signals::BaseSignalsCollector({
          {device_signals::SignalName::kBrowserContextSignals,
           base::BindRepeating(
               &ProfileSignalsCollectorIOS::PopulateProfileSignals,
               base::Unretained(this))},
      }),
      profile_prefs_(profile_prefs),
      policy_manager_(policy_manager),
      profile_id_service_(profile_id_service),
      connectors_service_(connectors_service) {
  CHECK(profile_prefs_);
  CHECK(policy_manager_);
}

ProfileSignalsCollectorIOS::~ProfileSignalsCollectorIOS() = default;

void ProfileSignalsCollectorIOS::PopulateProfileSignals(
    device_signals::UserPermission permission,
    const device_signals::SignalsAggregationRequest& request,
    device_signals::SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  device_signals::ProfileSignalsResponse signal_response;

  // 1. Collect signals from Preferences using shared helpers.
  signal_response.password_protection_warning_trigger =
      device_signals::GetPasswordProtectionWarningTrigger(profile_prefs_);
  signal_response.site_isolation_enabled =
      device_signals::GetSiteIsolationEnabled();
  signal_response.safe_browsing_protection_level =
      device_signals::GetSafeBrowsingProtectionLevel(profile_prefs_);
  // 2. Collect signals from Policy Manager.
  signal_response.profile_affiliation_ids =
      GetProfileAffiliationIds(policy_manager_);
  signal_response.profile_enrollment_domain =
      device_signals::TryGetEnrollmentDomain(policy_manager_);

  // 3. Collect Profile ID if service is available.
  if (profile_id_service_) {
    signal_response.profile_id = profile_id_service_->GetProfileId();
  }

  // 4. Collect Connectors signals if service is available.
  if (connectors_service_) {
    signal_response.realtime_url_check_mode =
        connectors_service_->GetAppliedRealTimeUrlCheck();
    signal_response.security_event_providers =
        connectors_service_->GetReportingServiceProviderNames();
  }

  response.profile_signals_response = std::move(signal_response);

  // All signals are fetched synchronously for now.
  std::move(done_closure).Run();
}
