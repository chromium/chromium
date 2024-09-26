// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_PROFILE_REPORT_GENERATOR_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_PROFILE_REPORT_GENERATOR_IOS_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "components/enterprise/browser/reporting/profile_report_generator.h"
#import "components/policy/core/browser/policy_conversions_client.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace base {
class FilePath;
}

namespace policy {
class CloudPolicyManager;
}


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
  void GetAffiliationInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;
  void GetExtensionInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;
  void GetExtensionRequest(
      enterprise_management::ChromeUserProfileInfo* report) override;
  std::unique_ptr<policy::PolicyConversionsClient> MakePolicyConversionsClient(
      bool is_machine_scope) override;
  policy::CloudPolicyManager* GetCloudPolicyManager(
      bool is_machine_scope) override;

 private:
  raw_ptr<ProfileIOS> profile_;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_PROFILE_REPORT_GENERATOR_IOS_H_
