// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/report_scheduler_ios.h"

#import "components/policy/core/common/cloud/dm_token.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
namespace enterprise_reporting {

ReportSchedulerIOS::ReportSchedulerIOS() = default;

ReportSchedulerIOS::~ReportSchedulerIOS() = default;

PrefService* ReportSchedulerIOS::GetPrefService() {
  return GetApplicationContext()->GetLocalState();
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

policy::DMToken ReportSchedulerIOS::GetProfileDMToken() {
  // Profile reporting is not supported.
  return policy::DMToken::CreateEmptyToken();
}
std::string ReportSchedulerIOS::GetProfileClientId() {
  // Profile reporting is not supported.
  return std::string();
}

}  // namespace enterprise_reporting
