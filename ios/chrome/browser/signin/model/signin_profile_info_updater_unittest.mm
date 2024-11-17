// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_profile_info_updater.h"

#import <memory>
#import <string>

#import "base/files/file_path.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/utf_string_conversions.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

const char kEmail[] = "example@email.com";
const char kProfileName[] = "default";

}  // namespace

class SigninProfileInfoUpdaterTest : public PlatformTest {
 public:
  SigninProfileInfoUpdaterTest()
      : signin_error_controller_(
            SigninErrorController::AccountMode::PRIMARY_ACCOUNT,
            identity_test_env()->identity_manager()) {
    // The Profile needs to be registered before SigninProfileInfoUpdater
    // construction (thus the std::unique_ptr<...>).
    GetApplicationContext()
        ->GetProfileManager()
        ->GetProfileAttributesStorage()
        ->AddProfile(kProfileName);
    signin_profile_info_updater_ = std::make_unique<SigninProfileInfoUpdater>(
        identity_test_env()->identity_manager(), &signin_error_controller_,
        kProfileName);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  ProfileAttributesIOS GetAttributesForProfile() const {
    return GetApplicationContext()
        ->GetProfileManager()
        ->GetProfileAttributesStorage()
        ->GetAttributesForProfileWithName(kProfileName);
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;

  signin::IdentityTestEnvironment identity_test_env_;
  SigninErrorController signin_error_controller_;
  std::unique_ptr<SigninProfileInfoUpdater> signin_profile_info_updater_;
};

// Tests that the profile info is updated on signin and signout.
TEST_F(SigninProfileInfoUpdaterTest, SigninSignout) {
  ASSERT_FALSE(GetAttributesForProfile().IsAuthenticated());

  // Signin.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSync);

  {
    ProfileAttributesIOS attr = GetAttributesForProfile();
    EXPECT_TRUE(attr.IsAuthenticated());
    EXPECT_EQ(account_info.gaia, attr.GetGaiaId());
    EXPECT_EQ(kEmail, attr.GetUserName());
  }

  // Signout.
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(GetAttributesForProfile().IsAuthenticated());
}

// Tests that the profile info is updated on auth error change.
TEST_F(SigninProfileInfoUpdaterTest, AuthError) {
  ASSERT_FALSE(GetAttributesForProfile().IsAuthenticated());

  // Signin.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSync);

  {
    ProfileAttributesIOS attr = GetAttributesForProfile();
    EXPECT_TRUE(attr.IsAuthenticated());
    EXPECT_FALSE(attr.HasAuthenticationError());
  }

  // Set auth error.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  EXPECT_TRUE(GetAttributesForProfile().HasAuthenticationError());

  // Remove auth error.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id, GoogleServiceAuthError::AuthErrorNone());

  EXPECT_FALSE(GetAttributesForProfile().HasAuthenticationError());
}
