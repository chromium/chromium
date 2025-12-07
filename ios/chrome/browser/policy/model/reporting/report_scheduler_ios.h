// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORT_SCHEDULER_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORT_SCHEDULER_IOS_H_

#include "base/memory/raw_ptr.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"

class ProfileIOS;

namespace enterprise_reporting {

// Desktop implementation of the ReportScheduler delegate.
class ReportSchedulerIOS : public ReportScheduler::Delegate {
 public:
  // Used for profile reporting if `profile` is non-null.
  explicit ReportSchedulerIOS(ProfileIOS* profile = nullptr);
  ReportSchedulerIOS(const ReportSchedulerIOS&) = delete;
  ReportSchedulerIOS& operator=(const ReportSchedulerIOS&) = delete;

  ~ReportSchedulerIOS() override;

  // ReportScheduler::Delegate implementation.
  PrefService* GetPrefService() override;
  void OnInitializationCompleted() override;
  void StartWatchingUpdatesIfNeeded(base::Time last_upload,
                                    base::TimeDelta upload_interval) override;
  void StopWatchingUpdates() override;
  void OnBrowserVersionUploaded() override;
  bool AreSecurityReportsEnabled() override;
  bool UseCookiesInUploads() override;
  void OnSecuritySignalsUploaded() override;
  policy::DMToken GetProfileDMToken() override;
  std::string GetProfileClientId() override;

 private:
  raw_ptr<ProfileIOS> profile_;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORT_SCHEDULER_IOS_H_
