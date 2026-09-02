// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/report_scheduler_ios.h"

#import "components/device_signals/core/common/signals_features.h"
#import "components/enterprise/browser/reporting/reporting_features.h"
#import "components/enterprise/browser/reporting/user_security_signals_service.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/dm_token.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/policy_service.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_reporting {

ReportSchedulerIOS::ReportSchedulerIOS(ProfileIOS* profile)
    : profile_(profile) {
  if (profile_) {
    DCHECK(base::FeatureList::IsEnabled(
        enterprise_reporting::kCloudProfileReporting));
    ProfilePolicyConnector* policy_connector = profile_->GetPolicyConnector();
    if (base::FeatureList::IsEnabled(
            enterprise_reporting::kIOSSignalSharingEnabled) &&
        enterprise_signals::features::IsProfileSignalsReportingEnabled() &&
        policy_connector && policy_connector->GetPolicyService()) {
      user_security_signals_service_ =
          std::make_unique<UserSecuritySignalsService>(
              GetPrefService(), this, policy_connector->GetPolicyService());
    }
  }
}

ReportSchedulerIOS::~ReportSchedulerIOS() = default;

PrefService* ReportSchedulerIOS::GetPrefService() {
  return profile_ ? profile_->GetPrefs()
                  : GetApplicationContext()->GetLocalState();
}

void ReportSchedulerIOS::OnInitializationCompleted() {
  // No-op.
}

void ReportSchedulerIOS::StartWatchingUpdatesIfNeeded(
    base::Time last_upload,
    base::TimeDelta upload_interval) {
  // Not used on iOS because there is no in-app auto-update.
}

void ReportSchedulerIOS::StopWatchingUpdates() {
  // Not used on iOS because there is no in-app auto-update.
}

void ReportSchedulerIOS::OnBrowserVersionUploaded() {
  // Not used on iOS because there is no in-app auto-update.
}

void ReportSchedulerIOS::OnReportEventTriggered(
    SecurityReportTrigger trigger) {
  if (!AreSecurityReportsEnabled()) {
    return;
  }
  if (!trigger_report_callback_.is_null()) {
    trigger_report_callback_.Run(ReportTrigger::kTriggerSecurity);
  }
}

network::mojom::CookieManager* ReportSchedulerIOS::GetCookieManager() {
  return profile_ ? profile_->GetCookieManager() : nullptr;
}

policy::DMToken ReportSchedulerIOS::GetProfileDMToken() {
  if (!base::FeatureList::IsEnabled(
          enterprise_reporting::kCloudProfileReporting)) {
    // Profile reporting is not supported.
    return policy::DMToken::CreateEmptyToken();
  }
  CHECK(profile_);
  policy::UserCloudPolicyManager* manager =
      profile_->GetUserCloudPolicyManager();
  if (!manager) {
    return policy::DMToken::CreateEmptyToken();
  }
  return manager->GetDMToken().value_or(policy::DMToken::CreateEmptyToken());
}

std::string ReportSchedulerIOS::GetProfileClientId() {
  if (!base::FeatureList::IsEnabled(
          enterprise_reporting::kCloudProfileReporting)) {
    // Profile reporting is not supported.
    return std::string();
  }
  CHECK(profile_);
  policy::UserCloudPolicyManager* manager =
      profile_->GetUserCloudPolicyManager();
  if (!manager) {
    return std::string();
  }
  return manager->GetClientId().value_or(std::string());
}

}  // namespace enterprise_reporting
