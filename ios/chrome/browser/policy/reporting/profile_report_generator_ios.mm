// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/reporting/profile_report_generator_ios.h"

#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/browser/policy_conversions.h"
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/policy_conversions_client_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

namespace enterprise_reporting {

ProfileReportGeneratorIOS::ProfileReportGeneratorIOS() = default;

ProfileReportGeneratorIOS::~ProfileReportGeneratorIOS() = default;

bool ProfileReportGeneratorIOS::Init(const base::FilePath& path) {
  browser_state_ =
      GetApplicationContext()->GetChromeBrowserStateManager()->GetBrowserState(
          path);

  if (!browser_state_) {
    return false;
  }

  return true;
}

void ProfileReportGeneratorIOS::GetSigninUserInfo(
    enterprise_management::ChromeUserProfileInfo* report) {
  if (!AuthenticationServiceFactory::GetForBrowserState(browser_state_)
           ->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return;
  }

  id<SystemIdentity> account_info =
      AuthenticationServiceFactory::GetForBrowserState(browser_state_)
          ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  auto* signed_in_user_info = report->mutable_chrome_signed_in_user();
  signed_in_user_info->set_email(
      base::SysNSStringToUTF8(account_info.userEmail));
  signed_in_user_info->set_obfuscated_gaia_id(
      base::SysNSStringToUTF8(account_info.hashedGaiaID));
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
ProfileReportGeneratorIOS::MakePolicyConversionsClient() {
  return std::make_unique<PolicyConversionsClientIOS>(browser_state_);
}

policy::MachineLevelUserCloudPolicyManager*
ProfileReportGeneratorIOS::GetCloudPolicyManager() {
  return GetApplicationContext()
      ->GetBrowserPolicyConnector()
      ->machine_level_user_cloud_policy_manager();
}

}  // namespace enterprise_reporting
