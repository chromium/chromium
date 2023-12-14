// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"

#import "ios/chrome/browser/policy/model/reporting/browser_report_generator_ios.h"
#import "ios/chrome/browser/policy/model/reporting/profile_report_generator_ios.h"
#import "ios/chrome/browser/policy/model/reporting/report_scheduler_ios.h"

namespace enterprise_reporting {

std::unique_ptr<BrowserReportGenerator::Delegate>
ReportingDelegateFactoryIOS::GetBrowserReportGeneratorDelegate() const {
  return std::make_unique<BrowserReportGeneratorIOS>();
}

std::unique_ptr<ProfileReportGenerator::Delegate>
ReportingDelegateFactoryIOS::GetProfileReportGeneratorDelegate() const {
  return std::make_unique<ProfileReportGeneratorIOS>();
}

std::unique_ptr<ReportGenerator::Delegate>
ReportingDelegateFactoryIOS::GetReportGeneratorDelegate() const {
  return nullptr;
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryIOS::GetReportSchedulerDelegate() const {
  return std::make_unique<ReportSchedulerIOS>();
}

std::unique_ptr<RealTimeReportGenerator::Delegate>
ReportingDelegateFactoryIOS::GetRealTimeReportGeneratorDelegate() const {
  // Using nullptr as the new pipeline is not supported on iOS.
  return nullptr;
}

std::unique_ptr<RealTimeReportController::Delegate>
ReportingDelegateFactoryIOS::GetRealTimeReportControllerDelegate() const {
  // Using nullptr as the new pipeline is not supported on iOS.
  return nullptr;
}

}  // namespace enterprise_reporting
