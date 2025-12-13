// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/profile_report_generator_ios.h"

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "components/policy/core/browser/policy_conversions.h"
#import "components/policy/core/common/cloud/affiliation.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/policy_conversions_client_ios.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace enterprise_reporting {

ProfileReportGeneratorIOS::ProfileReportGeneratorIOS() = default;

ProfileReportGeneratorIOS::~ProfileReportGeneratorIOS() = default;

bool ProfileReportGeneratorIOS::Init(const base::FilePath& path) {
  // TODO(crbug.com/356050207): this API should not assume that the name of
  // a Profile can be derived from its path.
  const std::string name = path.BaseName().AsUTF8Unsafe();
  profile_ =
      GetApplicationContext()->GetProfileManager()->GetProfileWithName(name);
  return profile_ != nullptr;
}

void ProfileReportGeneratorIOS::GetSigninUserInfo(
    enterprise_management::ChromeUserProfileInfo* report) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return;
  }

  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  auto* signed_in_user_info = report->mutable_chrome_signed_in_user();
  signed_in_user_info->set_email(account_info.email);
  signed_in_user_info->set_obfuscated_gaia_id(account_info.gaia.ToString());
}

void ProfileReportGeneratorIOS::GetAffiliationInfo(
    enterprise_management::ChromeUserProfileInfo* report) {
  if (!base::FeatureList::IsEnabled(
          enterprise_reporting::kCloudProfileReporting)) {
    // Affiliation information is currently not supported on iOS.
    return;
  }

  auto* affiliation_state = report->mutable_affiliation();
  if (IsProfileAffiliated(profile_)) {
    affiliation_state->set_is_affiliated(true);
    return;
  }

  affiliation_state->set_is_affiliated(false);
  affiliation_state->set_unaffiliation_reason(GetUnaffiliatedReason(profile_));
}

void ProfileReportGeneratorIOS::GetExtensionInfo(
    enterprise_management::ChromeUserProfileInfo* report) {
  // Extensions aren't supported on iOS.
}

void ProfileReportGeneratorIOS::GetExtensionRequest(
    enterprise_management::ChromeUserProfileInfo* report) {
  // Extensions aren't supported on iOS.
}

void ProfileReportGeneratorIOS::GetProfileId(
    enterprise_management::ChromeUserProfileInfo* report) {
  if (base::FeatureList::IsEnabled(
          enterprise_reporting::kCloudProfileReporting)) {
    std::optional<std::string> profile_id =
        enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile_)
            ->GetProfileId();
    if (profile_id.has_value()) {
      report->set_profile_id(profile_id.value());
    }
  }
}

void ProfileReportGeneratorIOS::GetProfileName(
    enterprise_management::ChromeUserProfileInfo* report) {
  report->set_name(profile_->GetProfileName());
}

std::unique_ptr<policy::PolicyConversionsClient>
ProfileReportGeneratorIOS::MakePolicyConversionsClient(bool is_machine_scope) {
  auto client = std::make_unique<PolicyConversionsClientIOS>(profile_);
  if (base::FeatureList::IsEnabled(
          enterprise_reporting::kCloudProfileReporting)) {
    // For profile reporting, if user is not affiliated, we need to hide machine
    // policy value.
    client->EnableShowMachineValues(is_machine_scope ||
                                    IsProfileAffiliated(profile_));
  }
  return client;
}

policy::CloudPolicyManager* ProfileReportGeneratorIOS::GetCloudPolicyManager(
    bool is_machine_scope) {
  if (is_machine_scope) {
    // CBCM report will include CBCM policy fetch information.
    return GetApplicationContext()
        ->GetBrowserPolicyConnector()
        ->machine_level_user_cloud_policy_manager();
  }
  DCHECK(base::FeatureList::IsEnabled(
      enterprise_reporting::kCloudProfileReporting));
  // Profile report will include user cloud policy information by default.
  return profile_->GetUserCloudPolicyManager();
}

}  // namespace enterprise_reporting
