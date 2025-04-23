// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/add_account_signin/add_account_signin_manager.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/test/test_timeouts.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_interaction_manager.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/web/common/uikit_ui_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

enum class TestCase {
  kAddAccountWhileSignedOut,
  kAddAccountWhileSignedIn,
  kPrimaryAccountReauth,
  kResigninWithUsername,
  kResigninWithoutUsername,
};

class AddAccountSigninManagerTest
    : public testing::WithParamInterface<TestCase>,
      public PlatformTest {
 public:
  AddAccountSigninManagerTest() {
    test_pref_service_.registry()->RegisterStringPref(
        prefs::kGoogleServicesLastSignedInUsername, std::string());
    add_account_signin_manager_.delegate = mock_delegate_;

    switch (GetParam()) {
      case TestCase::kAddAccountWhileSignedOut:
        break;
      case TestCase::kAddAccountWhileSignedIn:
        identity_test_environment_.MakePrimaryAccountAvailable(
            "signed-in-account@gmail.com", signin::ConsentLevel::kSignin);
        break;
      case TestCase::kPrimaryAccountReauth:
        identity_test_environment_.MakePrimaryAccountAvailable(
            "signed-in-account@gmail.com", signin::ConsentLevel::kSignin);
        expected_prefilled_email_ =
            base::SysUTF8ToNSString("signed-in-account@gmail.com");
        break;
      case TestCase::kResigninWithUsername:
        test_pref_service_.SetString(prefs::kGoogleServicesLastSignedInUsername,
                                     "previously-signed-in-account@gmail.com");
        expected_prefilled_email_ =
            base::SysUTF8ToNSString("previously-signed-in-account@gmail.com");
        break;
      case TestCase::kResigninWithoutUsername:
        break;
    }
  }

  AddAccountSigninIntent intent() {
    switch (GetParam()) {
      case TestCase::kAddAccountWhileSignedOut:
      case TestCase::kAddAccountWhileSignedIn:
        return AddAccountSigninIntent::kAddAccount;
      case TestCase::kPrimaryAccountReauth:
        return AddAccountSigninIntent::kPrimaryAccountReauth;
      case TestCase::kResigninWithUsername:
      case TestCase::kResigninWithoutUsername:
        return AddAccountSigninIntent::kResignin;
    }
    NOTREACHED();
  }

  NSString* expected_prefilled_email() { return expected_prefilled_email_; }

  FakeSystemIdentityInteractionManager* fake_interaction_manager() {
    return fake_interaction_manager_;
  }

  OCMockObject<AddAccountSigninManagerDelegate>* mock_delegate() {
    return mock_delegate_;
  }

  AddAccountSigninManager* add_account_signin_manager() {
    return add_account_signin_manager_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple test_pref_service_;
  signin::IdentityTestEnvironment identity_test_environment_;
  FakeSystemIdentityInteractionManager* fake_interaction_manager_ =
      base::apple::ObjCCastStrict<FakeSystemIdentityInteractionManager>(
          FakeSystemIdentityManager::FromSystemIdentityManager(
              GetApplicationContext()->GetSystemIdentityManager())
              ->CreateInteractionManager());
  OCMockObject<AddAccountSigninManagerDelegate>* mock_delegate_ =
      OCMStrictProtocolMock(@protocol(AddAccountSigninManagerDelegate));
  AddAccountSigninManager* add_account_signin_manager_ =
      [[AddAccountSigninManager alloc]
          initWithBaseViewController:GetAnyKeyWindow().rootViewController
                         prefService:&test_pref_service_
                     identityManager:identity_test_environment_
                                         .identity_manager()
          identityInteractionManager:fake_interaction_manager_];
  NSString* expected_prefilled_email_ = nil;
};

// Verifies the following state in the successful add account flow:
//   - Account is added to the identity service
//   - Completion callback is called with success state
TEST_P(AddAccountSigninManagerTest, ConfirmWithPrefilledEmail) {
  if (expected_prefilled_email().length == 0) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  [add_account_signin_manager() showSigninWithIntent:intent()];
  EXPECT_NSEQ(fake_interaction_manager().lastStartAuthActivityUserEmail,
              expected_prefilled_email());
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^{
        return fake_interaction_manager().isActivityViewPresented;
      }));

  id checkIdentityEmail =
      [OCMArg checkWithBlock:^BOOL(id<SystemIdentity> identity) {
        return [identity.userEmail isEqual:expected_prefilled_email()];
      }];
  OCMExpect([mock_delegate()
      addAccountSigninManagerFinishedWithResult:SigninAddAccountToDeviceResult::
                                                    kSuccess
                                       identity:checkIdentityEmail
                                          error:nil]);
  [fake_interaction_manager() simulateDidTapAddAccount];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return !fake_interaction_manager().isActivityViewPresented;
      }));
  histogram_tester.ExpectUniqueSample("Signin.AddAccountToDevice.Result",
                                      SigninAddAccountToDeviceResult::kSuccess,
                                      1);
  EXPECT_EQ(1U, histogram_tester
                    .GetAllSamples("Signin.AddAccountToDevice.Success.Duration")
                    .size());
}

// Verifies the following state in the successful add account flow:
//   - Account is added to the identity service
//   - Completion callback is called with success state
TEST_P(AddAccountSigninManagerTest, ConfirmWithDifferentEmail) {
  base::HistogramTester histogram_tester;
  [add_account_signin_manager() showSigninWithIntent:intent()];
  EXPECT_NSEQ(fake_interaction_manager().lastStartAuthActivityUserEmail,
              expected_prefilled_email());
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^{
        return fake_interaction_manager().isActivityViewPresented;
      }));
  FakeSystemIdentity* differentIdentity = [FakeSystemIdentity fakeIdentity2];
  [FakeSystemIdentityInteractionManager setIdentity:differentIdentity
                            withUnknownCapabilities:NO];

  OCMExpect([mock_delegate()
      addAccountSigninManagerFinishedWithResult:SigninAddAccountToDeviceResult::
                                                    kSuccess
                                       identity:differentIdentity
                                          error:nil]);
  [fake_interaction_manager() simulateDidTapAddAccount];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return !fake_interaction_manager().isActivityViewPresented;
      }));
  histogram_tester.ExpectUniqueSample("Signin.AddAccountToDevice.Result",
                                      SigninAddAccountToDeviceResult::kSuccess,
                                      1);
  EXPECT_EQ(1U, histogram_tester
                    .GetAllSamples("Signin.AddAccountToDevice.Success.Duration")
                    .size());
}

// Verifies the following state in the add account flow with a user cancel:
//   - Account is not added to the identity service
//   - Completion callback is called with user cancel state
TEST_P(AddAccountSigninManagerTest, Cancel) {
  base::HistogramTester histogram_tester;
  [add_account_signin_manager() showSigninWithIntent:intent()];
  EXPECT_NSEQ(fake_interaction_manager().lastStartAuthActivityUserEmail,
              expected_prefilled_email());
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^{
        return fake_interaction_manager().isActivityViewPresented;
      }));

  OCMExpect([mock_delegate()
      addAccountSigninManagerFinishedWithResult:SigninAddAccountToDeviceResult::
                                                    kCancelledByUser
                                       identity:nil
                                          error:nil]);
  [fake_interaction_manager() simulateDidTapCancel];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return !fake_interaction_manager().isActivityViewPresented;
      }));
  histogram_tester.ExpectUniqueSample(
      "Signin.AddAccountToDevice.Result",
      SigninAddAccountToDeviceResult::kCancelledByUser, 1);
  EXPECT_EQ(1U, histogram_tester
                    .GetAllSamples(
                        "Signin.AddAccountToDevice.CancelledByUser.Duration")
                    .size());
}

// Verifies the following state in the add account flow with an error handled by
// the view controller:
//   - Account is not added to the identity service
//   - Completion callback is called with user cancel state
TEST_P(AddAccountSigninManagerTest, ErrorHandledByViewController) {
  base::HistogramTester histogram_tester;
  [add_account_signin_manager() showSigninWithIntent:intent()];
  EXPECT_NSEQ(fake_interaction_manager().lastStartAuthActivityUserEmail,
              expected_prefilled_email());
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^{
        return fake_interaction_manager().isActivityViewPresented;
      }));

  OCMExpect([mock_delegate()
      addAccountSigninManagerFinishedWithResult:SigninAddAccountToDeviceResult::
                                                    kError
                                       identity:nil
                                          error:[OCMArg any]]);
  [fake_interaction_manager() simulateDidThrowUnhandledError];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return !fake_interaction_manager().isActivityViewPresented;
      }));
  histogram_tester.ExpectUniqueSample("Signin.AddAccountToDevice.Result",
                                      SigninAddAccountToDeviceResult::kError,
                                      1);
  EXPECT_EQ(1U, histogram_tester
                    .GetAllSamples("Signin.AddAccountToDevice.Error.Duration")
                    .size());
}

TEST_P(AddAccountSigninManagerTest, Interrupted) {
  base::HistogramTester histogram_tester;
  [add_account_signin_manager() showSigninWithIntent:intent()];
  EXPECT_NSEQ(fake_interaction_manager().lastStartAuthActivityUserEmail,
              expected_prefilled_email());
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^{
        return fake_interaction_manager().isActivityViewPresented;
      }));

  OCMExpect([mock_delegate() addAccountSigninManagerFinishedWithResult:
                                 SigninAddAccountToDeviceResult::kInterrupted
                                                              identity:nil
                                                                 error:nil]);
  [add_account_signin_manager() interruptAnimated:YES];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return !fake_interaction_manager().isActivityViewPresented;
      }));
  histogram_tester.ExpectUniqueSample(
      "Signin.AddAccountToDevice.Result",
      SigninAddAccountToDeviceResult::kInterrupted, 1);
  EXPECT_EQ(1U,
            histogram_tester
                .GetAllSamples("Signin.AddAccountToDevice.Interrupted.Duration")
                .size());
}

INSTANTIATE_TEST_SUITE_P(,
                         AddAccountSigninManagerTest,
                         testing::Values(TestCase::kAddAccountWhileSignedOut,
                                         TestCase::kAddAccountWhileSignedIn,
                                         TestCase::kPrimaryAccountReauth,
                                         TestCase::kResigninWithUsername,
                                         TestCase::kResigninWithoutUsername));

}  // namespace
