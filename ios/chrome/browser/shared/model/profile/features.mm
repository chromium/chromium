// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/features.h"

#import "base/check_is_test.h"
#import "base/feature_list.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

bool DetermineHasMultipleProfiles() {
  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();
  ProfileAttributesStorageIOS* attributes =
      profile_manager ? profile_manager->GetProfileAttributesStorage()
                      : nullptr;
  if (!attributes) {
    // ProfileManagerIOS or ProfileAttributesStorageIOS shouldn't be null,
    // except in some unit tests.
    CHECK_IS_TEST();
    return false;
  }
  // If only a single profile has been registered, nothing else to do.
  if (attributes->GetNumberOfProfiles() <= 1) {
    return false;
  }
  // When more than one profile has been registered, count how many have been
  // been initialized (i.e. actually created).
  int fully_initialized_profiles = 0;
  attributes->IterateOverProfileAttributes(base::BindRepeating(
      [](int& fully_initialized_profiles, const ProfileAttributesIOS& attr) {
        if (attr.IsFullyInitialized()) {
          ++fully_initialized_profiles;
        }
      },
      std::ref(fully_initialized_profiles)));
  return fully_initialized_profiles > 1;
}

}  // namespace

bool AreSeparateProfilesForManagedAccountsEnabled() {
  // Standard case: Check the regular feature flag.
  if (base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts)) {
    return true;
  }
  // If the feature is disabled, but the user already has multiple profiles
  // (can happen if the feature used to be enabled), leave it on so these
  // profiles remain accessible.
  static const bool has_multiple_profiles = DetermineHasMultipleProfiles();
  return has_multiple_profiles;
}

bool IsMultiProfilePushNotificationHandlingEnabled() {
  return AreSeparateProfilesForManagedAccountsEnabled() &&
         base::FeatureList::IsEnabled(kIOSPushNotificationMultiProfile);
}

BASE_FEATURE(kDestroyOTRProfileEarly, base::FEATURE_DISABLED_BY_DEFAULT);
