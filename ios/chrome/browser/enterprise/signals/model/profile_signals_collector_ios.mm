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
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"

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

  // Initialize all fields to avoid undefined behavior.
  signal_response.built_in_dns_client_enabled = false;
  signal_response.chrome_remote_desktop_app_blocked = false;

  // 1. Collect signals from Preferences using shared helpers.
  signal_response.password_protection_warning_trigger =
      device_signals::GetPasswordProtectionWarningTrigger(profile_prefs_);
  signal_response.site_isolation_enabled =
      device_signals::GetSiteIsolationEnabled();
  signal_response.safe_browsing_protection_level =
      device_signals::GetSafeBrowsingProtectionLevel(profile_prefs_);
  // 2. Collect Enrollment Domain from Policy Manager.
  signal_response.profile_enrollment_domain =
      device_signals::TryGetEnrollmentDomain(policy_manager_);

  // 3. Collect Profile ID if service is available.
  if (profile_id_service_) {
    signal_response.profile_id = profile_id_service_->GetProfileId();
  }

  // 4. Collect Connectors signals if service is available.
  signal_response.realtime_url_check_mode =
      enterprise_connectors::REAL_TIME_CHECK_DISABLED;

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
