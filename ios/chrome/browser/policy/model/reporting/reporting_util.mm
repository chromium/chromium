// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/reporting_util.h"

#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "components/policy/core/common/cloud/affiliation.h"
#import "components/policy/core/common/management/management_service.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/management_service_ios_factory.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

namespace enterprise_reporting {

bool IsProfileAffiliated(ProfileIOS* profile) {
  return policy::IsAffiliated(
      profile->GetPolicyConnector()->GetUserAffiliationIds(),
      GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->GetDeviceAffiliationIds());
}

// Returns more details for unaffiliated profile. Only used for unaffiliated
// profile.
enterprise_management::AffiliationState::UnaffiliationReason
GetUnaffiliatedReason(ProfileIOS* profile) {
  auto* connector = profile->GetPolicyConnector();
  namespace em = enterprise_management;
  if (!connector->IsManaged()) {
    return em::AffiliationState_UnaffiliationReason_USER_UNMANAGED;
  }
  if (GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->GetDeviceAffiliationIds()
          .size() > 0) {
    return em::
        AffiliationState_UnaffiliationReason_DEVICE_MANANGED_DIFFERENT_DOMAIN;
  }
  if (policy::ManagementServiceIOSFactory::GetForPlatform()
          ->IsBrowserManaged()) {
    return em::AffiliationState_UnaffiliationReason_DEVICE_MANAGED_BY_PLATFORM;
  }
  return em::AffiliationState_UnaffiliationReason_DEVICE_UNMANAGED;
}

std::string SanitizeProfilePath(std::string_view profile_name) {
  if (!base::FeatureList::IsEnabled(kSanitizeProfilePaths)) {
    // Kill-switch active, use the raw profile path like the old code.
    ProfileIOS* profile =
        GetApplicationContext()->GetProfileManager()->GetProfileWithName(
            profile_name);
    CHECK(profile);
    return profile->GetStatePath().AsUTF8Unsafe();
  }
  return base::StrCat({"/Profile/", profile_name});
}

}  // namespace enterprise_reporting
