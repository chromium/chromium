// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_SCHEDULER_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_SCHEDULER_DELEGATE_IOS_H_

#import "base/containers/flat_set.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"

class ProfileIOS;
class ProfileManagerIOS;

namespace enterprise_reporting {

// This class is responsible for observing profile-related events
// and notifying the SaasUsageReportScheduler when there is at least one
// profile with an active RealtimeReportingClient on iOS.
//
// This delegate is used only by browser-level scheduler.
class SaasUsageReportSchedulerDelegateIOS
    : public SaasUsageReportScheduler::Delegate,
      public ProfileManagerObserverIOS {
 public:
  SaasUsageReportSchedulerDelegateIOS();
  SaasUsageReportSchedulerDelegateIOS(
      const SaasUsageReportSchedulerDelegateIOS&) = delete;
  SaasUsageReportSchedulerDelegateIOS& operator=(
      const SaasUsageReportSchedulerDelegateIOS&) = delete;
  ~SaasUsageReportSchedulerDelegateIOS() override;

  // SaasUsageReportScheduler::Delegate:
  void SetReadyStateChangedCallback(base::RepeatingClosure callback) override;
  bool IsReady() override;

  // ProfileManagerObserverIOS:
  void OnProfileManagerWillBeDestroyed(ProfileManagerIOS* manager) override;
  void OnProfileManagerDestroyed(ProfileManagerIOS* manager) override;
  void OnProfileCreated(ProfileManagerIOS* manager,
                        ProfileIOS* profile) override;
  void OnProfileLoaded(ProfileManagerIOS* manager,
                       ProfileIOS* profile) override;
  void OnProfileUnloaded(ProfileManagerIOS* manager,
                         ProfileIOS* profile) override;
  void OnProfileMarkedForPermanentDeletion(ProfileManagerIOS* manager,
                                           ProfileIOS* profile) override;

 private:
  base::RepeatingClosure ready_state_changed_callback_;
  base::flat_set<raw_ptr<ProfileIOS>> reporting_profiles_;

  base::ScopedObservation<ProfileManagerIOS, ProfileManagerObserverIOS>
      profile_manager_observation_{this};
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_SCHEDULER_DELEGATE_IOS_H_
