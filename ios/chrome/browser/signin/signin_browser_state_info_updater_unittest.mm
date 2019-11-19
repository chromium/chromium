// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/signin_browser_state_info_updater.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const char kEmail[] = "example@email.com";

// A wrapper around a base::ScopedTempDir that creates the temporary directory
// in its constructor. This allow declaring all objects used as member fields
// instead of having to allocate separately.
class ScopedTempDirWrapper {
 public:
  ScopedTempDirWrapper() {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  const base::FilePath& GetPath() const { return scoped_temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir scoped_temp_dir_;
};

}  // namespace

class SigninBrowserStateInfoUpdaterTest : public PlatformTest {
 public:
  SigninBrowserStateInfoUpdaterTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(
                browser_state_path().DirName())),
        signin_error_controller_(
            SigninErrorController::AccountMode::PRIMARY_ACCOUNT,
            identity_test_env()->identity_manager()),
        signin_browser_state_info_updater_(
            identity_test_env()->identity_manager(),
            &signin_error_controller_,
            browser_state_path()) {
    browser_state_info()->AddBrowserState(browser_state_path(),
                                          /*gaia_id=*/std::string(),
                                          /*username=*/base::string16());
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  base::FilePath browser_state_path() const {
    return scoped_state_path_.GetPath();
  }

  BrowserStateInfoCache* browser_state_info() const {
    return GetApplicationContext()
        ->GetChromeBrowserStateManager()
        ->GetBrowserStateInfoCache();
  }

  // Returns index of cached information.
  size_t cached_information_index() const {
    return browser_state_info()->GetIndexOfBrowserStateWithPath(
        browser_state_path());
  }

  ScopedTempDirWrapper scoped_state_path_;
  web::WebTaskEnvironment task_environment_;

  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
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
  AccountInfo account_info =
      identity_test_env()->MakePrimaryAccountAvailable(kEmail);

  EXPECT_TRUE(
      browser_state_info()->BrowserStateIsAuthenticatedAtIndex(cache_index));
  EXPECT_EQ(account_info.gaia,
            browser_state_info()->GetGAIAIdOfBrowserStateAtIndex(cache_index));
  EXPECT_EQ(
      kEmail,
      base::UTF16ToUTF8(
          browser_state_info()->GetUserNameOfBrowserStateAtIndex(cache_index)));

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
  AccountInfo account_info =
      identity_test_env()->MakePrimaryAccountAvailable(kEmail);

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
