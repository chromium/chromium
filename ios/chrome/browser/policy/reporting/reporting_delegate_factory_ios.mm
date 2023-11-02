// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/reporting/reporting_delegate_factory_ios.h"

#import "ios/chrome/browser/policy/reporting/browser_report_generator_ios.h"
#import "ios/chrome/browser/policy/reporting/profile_report_generator_ios.h"
#import "ios/chrome/browser/policy/reporting/report_scheduler_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace enterprise_reporting {

std::unique_ptr<BrowserReportGenerator::Delegate>
ReportingDelegateFactoryIOS::GetBrowserReportGeneratorDelegate() {
  return std::make_unique<BrowserReportGeneratorIOS>();
}

std::unique_ptr<ProfileReportGenerator::Delegate>
ReportingDelegateFactoryIOS::GetProfileReportGeneratorDelegate() {
  return std::make_unique<ProfileReportGeneratorIOS>();
}

std::unique_ptr<ReportGenerator::Delegate>
ReportingDelegateFactoryIOS::GetReportGeneratorDelegate() {
  return nullptr;
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryIOS::GetReportSchedulerDelegate() {
  return std::make_unique<ReportSchedulerIOS>();
}

std::unique_ptr<RealTimeReportGenerator::Delegate>
ReportingDelegateFactoryIOS::GetRealTimeReportGeneratorDelegate() {
  // Using nullptr as the new pipeline is not supported on iOS.
  return nullptr;
}

}  // namespace enterprise_reporting
