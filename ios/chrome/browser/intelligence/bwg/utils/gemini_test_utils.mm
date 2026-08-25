// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_test_utils.h"

#import "base/check.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace gemini::test {

AccountInfo SetUpEligibleAccount(ProfileIOS* profile,
                                 const std::string& email,
                                 bool can_use_model_execution,
                                 bool can_use_gemini_in_chrome) {
  CHECK(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, email, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(can_use_model_execution);
  mutator.set_can_use_gemini_in_chrome(can_use_gemini_in_chrome);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  return account_info;
}

}  // namespace gemini::test
