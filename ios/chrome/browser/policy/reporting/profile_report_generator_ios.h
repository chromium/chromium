// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_REPORTING_PROFILE_REPORT_GENERATOR_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_REPORTING_PROFILE_REPORT_GENERATOR_IOS_H_

#include "components/enterprise/browser/reporting/profile_report_generator.h"

#include <memory>

#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class FilePath;
}

namespace policy {
class MachineLevelUserCloudPolicyManager;
}

class ChromeBrowserState;

namespace enterprise_reporting {

/**
 * iOS implementation of the profile reporting delegate.
 */
class ProfileReportGeneratorIOS : public ProfileReportGenerator::Delegate {
 public:
  ProfileReportGeneratorIOS();
  ProfileReportGeneratorIOS(const ProfileReportGeneratorIOS&) = delete;
  ProfileReportGeneratorIOS& operator=(const ProfileReportGeneratorIOS&) =
      delete;
  ~ProfileReportGeneratorIOS() override;

  // ProfileReportGenerator::Delegate implementation.
  bool Init(const base::FilePath& path) override;
  void GetSigninUserInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;
  void GetExtensionInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;
  void GetExtensionRequest(
      enterprise_management::ChromeUserProfileInfo* report) override;
  std::unique_ptr<policy::PolicyConversionsClient> MakePolicyConversionsClient()
      override;
  policy::MachineLevelUserCloudPolicyManager* GetCloudPolicyManager() override;

 private:
  ChromeBrowserState* browser_state_;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_REPORTING_PROFILE_REPORT_GENERATOR_IOS_H_
