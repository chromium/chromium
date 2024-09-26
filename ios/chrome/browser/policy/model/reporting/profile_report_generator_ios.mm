// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/profile_report_generator_ios.h"

#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/browser/policy_conversions.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/policy_conversions_client_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

namespace enterprise_reporting {

ProfileReportGeneratorIOS::ProfileReportGeneratorIOS() = default;

ProfileReportGeneratorIOS::~ProfileReportGeneratorIOS() = default;

bool ProfileReportGeneratorIOS::Init(const base::FilePath& path) {
  // TODO(crbug.com/356050207): this API should not assume that the name of
  // a Profile can be derived from its path.
  const std::string name = path.BaseName().AsUTF8Unsafe();
  profile_ =
      GetApplicationContext()->GetProfileManager()->GetProfileWithName(name);

  if (!profile_) {
    return false;
  }

  return true;
}

void ProfileReportGeneratorIOS::GetSigninUserInfo(
    enterprise_management::ChromeUserProfileInfo* report) {
  if (!AuthenticationServiceFactory::GetForProfile(profile_)
           ->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return;
  }

  id<SystemIdentity> account_info =
      AuthenticationServiceFactory::GetForProfile(profile_)->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin);
  auto* signed_in_user_info = report->mutable_chrome_signed_in_user();
  signed_in_user_info->set_email(
      base::SysNSStringToUTF8(account_info.userEmail));
  signed_in_user_info->set_obfuscated_gaia_id(
      base::SysNSStringToUTF8(account_info.gaiaID));
}

void ProfileReportGeneratorIOS::GetAffiliationInfo(enterprise_management::ChromeUserProfileInfo* report) {
  // Affiliation information is currently not supported on iOS.
}

void ProfileReportGeneratorIOS::GetExtensionInfo(
    enterprise_management::ChromeUserProfileInfo* report) {
  // Extensions aren't supported on iOS.
}

void ProfileReportGeneratorIOS::GetExtensionRequest(
    enterprise_management::ChromeUserProfileInfo* report) {
  // Extensions aren't supported on iOS.
}

std::unique_ptr<policy::PolicyConversionsClient>
ProfileReportGeneratorIOS::MakePolicyConversionsClient(bool is_machine_scope) {
  // Note that profile reporting is not supported on iOS yet, hence we igore
  // `is_machine_scope` value.
  return std::make_unique<PolicyConversionsClientIOS>(profile_);
}

policy::CloudPolicyManager* ProfileReportGeneratorIOS::GetCloudPolicyManager(
    bool is_machine_scope) {
  // Note that profile reporting is not supported on iOS yet, hence we igore
  // `is_machine_scope` value.
  return GetApplicationContext()
      ->GetBrowserPolicyConnector()
      ->machine_level_user_cloud_policy_manager();
}

}  // namespace enterprise_reporting
