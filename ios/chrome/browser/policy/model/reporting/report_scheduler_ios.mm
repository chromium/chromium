// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/report_scheduler_ios.h"

#import "base/feature_list.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/dm_token.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_reporting {

ReportSchedulerIOS::ReportSchedulerIOS(ProfileIOS* profile)
    : profile_(profile) {
  if (profile_) {
    DCHECK(base::FeatureList::IsEnabled(
        enterprise_reporting::kCloudProfileReporting));
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

bool ReportSchedulerIOS::AreSecurityReportsEnabled() {
  // Not supported.
  return false;
}

bool ReportSchedulerIOS::UseCookiesInUploads() {
  // Not supported.
  return false;
}

void ReportSchedulerIOS::OnSecuritySignalsUploaded() {
  // No-op because signals reporting is not supported on Android.
}

policy::DMToken ReportSchedulerIOS::GetProfileDMToken() {
  if (!base::FeatureList::IsEnabled(
          enterprise_reporting::kCloudProfileReporting)) {
    // Profile reporting is not supported.
    return policy::DMToken::CreateEmptyToken();
  }
  CHECK(profile_);
  return profile_->GetUserCloudPolicyManager()->GetDMToken().value_or(
      policy::DMToken::CreateEmptyToken());
}

std::string ReportSchedulerIOS::GetProfileClientId() {
  if (!base::FeatureList::IsEnabled(
          enterprise_reporting::kCloudProfileReporting)) {
    // Profile reporting is not supported.
    return std::string();
  }
  CHECK(profile_);
  return profile_->GetUserCloudPolicyManager()->GetClientId().value_or(
      std::string());
}

}  // namespace enterprise_reporting
