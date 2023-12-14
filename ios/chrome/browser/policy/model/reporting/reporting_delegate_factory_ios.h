// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORTING_DELEGATE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORTING_DELEGATE_FACTORY_IOS_H_

#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"

#include <memory>

#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/enterprise/browser/reporting/profile_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"
#include "components/enterprise/browser/reporting/real_time_report_generator.h"
#include "components/enterprise/browser/reporting/report_generator.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"

namespace enterprise_reporting {

// iOS implementation of the reporting delegate factory. Creates iOS-specific
// delegates for the enterprise reporting classes.
class ReportingDelegateFactoryIOS : public ReportingDelegateFactory {
 public:
  ReportingDelegateFactoryIOS() = default;
  ReportingDelegateFactoryIOS(const ReportingDelegateFactoryIOS&) = delete;
  ReportingDelegateFactoryIOS& operator=(const ReportingDelegateFactoryIOS&) =
      delete;
  ~ReportingDelegateFactoryIOS() override = default;

  std::unique_ptr<BrowserReportGenerator::Delegate>
  GetBrowserReportGeneratorDelegate() const override;

  std::unique_ptr<ProfileReportGenerator::Delegate>
  GetProfileReportGeneratorDelegate() const override;

  std::unique_ptr<ReportGenerator::Delegate> GetReportGeneratorDelegate()
      const override;

  std::unique_ptr<ReportScheduler::Delegate> GetReportSchedulerDelegate()
      const override;

  std::unique_ptr<RealTimeReportGenerator::Delegate>
  GetRealTimeReportGeneratorDelegate() const override;

  std::unique_ptr<RealTimeReportController::Delegate>
  GetRealTimeReportControllerDelegate() const override;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REPORTING_DELEGATE_FACTORY_IOS_H_
