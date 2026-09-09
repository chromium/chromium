// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_report_scheduler_delegate_ios.h"

#import "base/check.h"
#import "components/policy/core/common/policy_logger.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

namespace enterprise_reporting {

SaasUsageReportSchedulerDelegateIOS::SaasUsageReportSchedulerDelegateIOS() {
  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();
  CHECK(profile_manager);

  // This immediately calls OnProfileLoaded() for pre-existing profiles.
  profile_manager_observation_.Observe(profile_manager);
}

SaasUsageReportSchedulerDelegateIOS::~SaasUsageReportSchedulerDelegateIOS() =
    default;

void SaasUsageReportSchedulerDelegateIOS::SetReadyStateChangedCallback(
    base::RepeatingClosure callback) {
  ready_state_changed_callback_ = callback;
}

bool SaasUsageReportSchedulerDelegateIOS::IsReady() {
  return !reporting_profiles_.empty();
}

void SaasUsageReportSchedulerDelegateIOS::OnProfileManagerWillBeDestroyed(
    ProfileManagerIOS* manager) {}

void SaasUsageReportSchedulerDelegateIOS::OnProfileManagerDestroyed(
    ProfileManagerIOS* manager) {
  profile_manager_observation_.Reset();
}

void SaasUsageReportSchedulerDelegateIOS::OnProfileCreated(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {}

void SaasUsageReportSchedulerDelegateIOS::OnProfileLoaded(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {
  // If browser is managed, any profile with a RealtimeReportingClient will be
  // able to upload reports using browser DM token.
  if (!enterprise_connectors::IOSRealtimeReportingClientFactory::GetForProfile(
          profile)) {
    return;
  }

  bool was_ready = IsReady();
  reporting_profiles_.insert(profile);
  if (ready_state_changed_callback_ && !was_ready) {
    VLOG_POLICY(1, REPORTING)
        << "SaaS usage reporting is enabled because a reporting-enabled "
           "profile has been added.";
    ready_state_changed_callback_.Run();
  }
}

void SaasUsageReportSchedulerDelegateIOS::OnProfileUnloaded(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {
  if (!reporting_profiles_.erase(profile)) {
    return;
  }

  if (ready_state_changed_callback_ && !IsReady()) {
    VLOG_POLICY(1, REPORTING)
        << "SaaS usage reporting is disabled because the last "
           "reporting-enabled profile is being unloaded.";
    ready_state_changed_callback_.Run();
  }
}

void SaasUsageReportSchedulerDelegateIOS::OnProfileMarkedForPermanentDeletion(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {}

}  // namespace enterprise_reporting
