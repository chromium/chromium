// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/reporting/profile_report_generator_ios.h"

#include "base/strings/sys_string_conversions.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#include "ios/chrome/browser/policy/policy_conversions_client_ios.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  ChromeIdentity* account_info =
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
