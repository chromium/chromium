// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_browser_state_info_updater.h"

#import <memory>
#import <string>

#import "base/files/file_path.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/utf_string_conversions.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

const char kEmail[] = "example@email.com";

}  // namespace

class SigninBrowserStateInfoUpdaterTest : public PlatformTest {
 public:
  SigninBrowserStateInfoUpdaterTest()
      : signin_error_controller_(
            SigninErrorController::AccountMode::PRIMARY_ACCOUNT,
            identity_test_env()->identity_manager()),
        signin_browser_state_info_updater_(
            identity_test_env()->identity_manager(),
            &signin_error_controller_,
            browser_state_name()) {
    browser_state_info()->AddBrowserState(browser_state_name(),
                                          /*gaia_id=*/std::string(),
                                          /*username=*/std::string());
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  std::string browser_state_name() const { return "default"; }

  BrowserStateInfoCache* browser_state_info() const {
    return GetApplicationContext()
        ->GetChromeBrowserStateManager()
        ->GetBrowserStateInfoCache();
  }

  // Returns index of cached information.
  size_t cached_information_index() const {
    return browser_state_info()->GetIndexOfBrowserStateWithName(
        browser_state_name());
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestChromeBrowserStateManager browser_state_manager_;

  signin::IdentityTestEnvironment identity_test_env_;
  SigninErrorController signin_error_controller_;
  SigninBrowserStateInfoUpdater signin_browser_state_info_updater_;
};

// Tests that the browser state info is updated on signin and signout.
TEST_F(SigninBrowserStateInfoUpdaterTest, SigninSignout) {
  const size_t cache_index = cached_information_index();

  ASSERT_FALSE(
      browser_state_info()->BrowserStateIsAuthenticatedAtIndex(cache_index));

  // Signin.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSync);

  EXPECT_TRUE(
      browser_state_info()->BrowserStateIsAuthenticatedAtIndex(cache_index));
  EXPECT_EQ(account_info.gaia,
            browser_state_info()->GetGAIAIdOfBrowserStateAtIndex(cache_index));
  EXPECT_EQ(kEmail, browser_state_info()->GetUserNameOfBrowserStateAtIndex(
                        cache_index));

  // Signout.
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(
      browser_state_info()->BrowserStateIsAuthenticatedAtIndex(cache_index));
}

// Tests that the browser state info is updated on auth error change.
TEST_F(SigninBrowserStateInfoUpdaterTest, AuthError) {
  const size_t cache_index = cached_information_index();

  ASSERT_FALSE(
      browser_state_info()->BrowserStateIsAuthenticatedAtIndex(cache_index));

  // Signin.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSync);

  EXPECT_TRUE(
      browser_state_info()->BrowserStateIsAuthenticatedAtIndex(cache_index));
  EXPECT_FALSE(
      browser_state_info()->BrowserStateIsAuthErrorAtIndex(cache_index));

  // Set auth error.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(
      browser_state_info()->BrowserStateIsAuthErrorAtIndex(cache_index));

  // Remove auth error.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(
      browser_state_info()->BrowserStateIsAuthErrorAtIndex(cache_index));
}
