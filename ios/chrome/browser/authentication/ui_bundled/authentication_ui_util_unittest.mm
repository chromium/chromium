// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"

#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_test_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"

namespace {
void ExpectErrorInMessage(NSError* error,
                          NSString* message,
                          BOOL hasDescription) {
  if (hasDescription) {
    EXPECT_TRUE([message containsString:error.localizedDescription]);
  } else if (error.userInfo[NSLocalizedDescriptionKey]) {
    EXPECT_FALSE([message containsString:error.localizedDescription]);
  }
  EXPECT_TRUE([message containsString:error.domain]);
  EXPECT_TRUE([message containsString:[@(error.code) description]]);
}
}  // namespace

using AuthenticationUIUtil = PlatformTest;

// Tests the error message with one error with a localized description.
TEST_F(AuthenticationUIUtil, DialogMessageFromErrorWithLocalizedDescription) {
  NSDictionary* userInfo =
      @{NSLocalizedDescriptionKey : @"MyLocalizedDescription"};
  NSError* error = [NSError errorWithDomain:@"MyErrorDomain"
                                       code:-1234
                                   userInfo:userInfo];
  NSString* message = DialogMessageFromError(error);
  ExpectErrorInMessage(error, message, YES);
}

// Tests the error message with one error without a localized description.
TEST_F(AuthenticationUIUtil,
       DialogMessageFromErrorWithoutLocalizedDescription) {
  NSError* error = [NSError errorWithDomain:@"MyErrorDomain"
                                       code:-1234
                                   userInfo:nil];
  NSString* message = DialogMessageFromError(error);
  ExpectErrorInMessage(error, message, NO);
}

// Tests the error message with an error with 2 underlying errors.
TEST_F(AuthenticationUIUtil, DialogMessageFromErrorWithUnderlyingErrors) {
  // Error 1
  NSDictionary* userInfo1 =
      @{NSLocalizedDescriptionKey : @"MyLocalizedDescription1"};
  NSError* error1 = [NSError errorWithDomain:@"MyErrorDomain1"
                                        code:-1234
                                    userInfo:userInfo1];

  // Error 2
  NSDictionary* userInfo2 = @{
    NSLocalizedDescriptionKey : @"MyLocalizedDescription2",
    NSUnderlyingErrorKey : error1
  };
  NSError* error2 = [NSError errorWithDomain:@"MyErrorDomain2"
                                        code:-567
                                    userInfo:userInfo2];

  // Error 3
  NSDictionary* userInfo3 = @{
    NSLocalizedDescriptionKey : @"MyLocalizedDescription3",
    NSUnderlyingErrorKey : error2
  };
  NSError* error3 = [NSError errorWithDomain:@"MyErrorDomain3"
                                        code:-890
                                    userInfo:userInfo3];

  NSString* message = DialogMessageFromError(error3);
  ExpectErrorInMessage(error3, message, YES);
  ExpectErrorInMessage(error2, message, NO);
  ExpectErrorInMessage(error1, message, NO);
}

TEST_F(AuthenticationUIUtil,
       ForceLeavingPrimaryAccountConfirmationDialog_DifferentAccount) {
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  web::WebTaskEnvironment task_environment;
  TestProfileManagerIOS profile_manager;
  TestProfileIOS::Builder builder;
  builder.SetName(GetApplicationContext()
                      ->GetProfileManager()
                      ->GetProfileAttributesStorage()
                      ->GetPersonalProfileName());
  builder.AddTestingFactory(
      IdentityManagerFactory::GetInstance(),
      base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                              BuildIdentityManagerForTests));
  TestProfileIOS* profile = profile_manager.AddProfileWithBuilder(std::move(builder));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  // Add a primary account.
  AccountInfo primary_account = signin::MakePrimaryAccountAvailable(
      identity_manager, "primary@gmail.com", signin::ConsentLevel::kSignin);

  // Test with the same account.
  EXPECT_TRUE(ForceLeavingPrimaryAccountConfirmationDialog(
      SignedInUserState::kManagedAccountClearsDataOnSignout, profile,
      primary_account.gaia));

  // Test with a different account.
  EXPECT_FALSE(ForceLeavingPrimaryAccountConfirmationDialog(
      SignedInUserState::kManagedAccountClearsDataOnSignout, profile,
      GaiaId("different_gaia")));

  // Test with an empty account (sign-out).
  EXPECT_TRUE(ForceLeavingPrimaryAccountConfirmationDialog(
      SignedInUserState::kManagedAccountClearsDataOnSignout, profile,
      {}));
}
