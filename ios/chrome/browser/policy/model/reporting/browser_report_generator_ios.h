// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_BROWSER_REPORT_GENERATOR_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_BROWSER_REPORT_GENERATOR_IOS_H_

#include "components/enterprise/browser/reporting/browser_report_generator.h"

#include <memory>

#include "base/functional/callback.h"
#include "components/version_info/channel.h"

namespace enterprise_management {
class BrowserReport;
}  // namespace enterprise_management

namespace enterprise_reporting {

// iOS implementation of platform-specific info fetching for Enterprise browser
// report generation.
class BrowserReportGeneratorIOS : public BrowserReportGenerator::Delegate {
 public:
  using ReportCallback = base::OnceCallback<void(
      std::unique_ptr<enterprise_management::BrowserReport>)>;

  BrowserReportGeneratorIOS();
  BrowserReportGeneratorIOS(const BrowserReportGeneratorIOS&) = delete;
  BrowserReportGeneratorIOS& operator=(const BrowserReportGeneratorIOS&) =
      delete;
  ~BrowserReportGeneratorIOS() override;

  // BrowserReportGenerator::Delegate implementation.
  std::string GetExecutablePath() override;
  version_info::Channel GetChannel() override;
  std::vector<BrowserReportGenerator::ReportedProfileData> GetReportedProfiles()
      override;
  bool IsExtendedStableChannel() override;
  void GenerateBuildStateInfo(
      enterprise_management::BrowserReport* report) override;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_BROWSER_REPORT_GENERATOR_IOS_H_
