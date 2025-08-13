// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_IOS_H_

#include <memory>

#import "base/memory/raw_ptr.h"
#import "components/enterprise/browser/reporting/report_scheduler.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"

class ProfileIOS;

namespace enterprise_reporting {

class CloudProfileReportingServiceIOS : public KeyedService {
 public:
  explicit CloudProfileReportingServiceIOS(ProfileIOS* profile);
  CloudProfileReportingServiceIOS(const CloudProfileReportingServiceIOS&) =
      delete;
  CloudProfileReportingServiceIOS& operator=(
      const CloudProfileReportingServiceIOS&) = delete;
  ~CloudProfileReportingServiceIOS() override;

  ReportScheduler* report_scheduler() { return report_scheduler_.get(); }

  void CreateReportScheduler();

 private:
  void Init();

  std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client_;
  std::unique_ptr<ReportScheduler> report_scheduler_;
  raw_ptr<ProfileIOS> profile_;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_IOS_H_
