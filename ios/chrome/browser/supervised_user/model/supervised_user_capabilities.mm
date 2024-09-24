// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities.h"

#import "base/feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/supervised_user/core/browser/supervised_user_capabilities.h"
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/browser_state.h"

namespace supervised_user {

bool IsSubjectToParentalControls(ProfileIOS* profile) {
  CHECK(profile);
  if (profile->IsOffTheRecord()) {
    // An OTR profile cannot be under parental controls.
    return false;
  }
  if (base::FeatureList::IsEnabled(
          supervised_user::
              kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS)) {
    // If the capability is kUnknown, the profile should not be subjected
    // to parental controls. Additionally, the retiring prefs-based
    // `IsSubjectToParentalControls` will also return false.
    return IsPrimaryAccountSubjectToParentalControls(
               IdentityManagerFactory::GetForProfile(profile)) ==
           signin::Tribool::kTrue;
  } else {
    return IsSubjectToParentalControls(*profile->GetPrefs());
  }
}

}  // namespace supervised_user
