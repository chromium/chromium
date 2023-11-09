// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_manager.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "base/test/test_timeouts.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_interaction_manager.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/common/uikit_ui_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
// Constants for configuring a FakeSystemIdentity.
const char kTestGaiaID[] = "fooID";
const char kTestEmail[] = "foo@gmail.com";
}  // namespace

class AddAccountSigninManagerTest : public PlatformTest {
 public:
  AddAccountSigninManagerTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {
    identity_interaction_manager_ = GetIdentityInteractionManager();
  }

  FakeSystemIdentityInteractionManager* GetIdentityInteractionManager() {
    fake_identity_ = [FakeSystemIdentity
        identityWithEmail:[NSString stringWithUTF8String:kTestEmail]
                   gaiaID:[NSString stringWithUTF8String:kTestGaiaID]
                     name:@"Foo"];
    return base::apple::ObjCCastStrict<FakeSystemIdentityInteractionManager>(
        fake_system_identity_manager()->CreateInteractionManager());
  }

  void WaitForFakeAddAccountViewPresented(NSString* expectedUserEmail) {
    EXPECT_NSEQ(expectedUserEmail,
                identity_interaction_manager_.lastStartAuthActivityUserEmail);
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        TestTimeouts::action_timeout(), ^bool() {
          return identity_interaction_manager_.isActivityViewPresented;
        }));
  }

  void WaitForFakeAddAccountViewDismissed() {
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        TestTimeouts::action_timeout(), ^bool() {
          return !identity_interaction_manager_.isActivityViewPresented;
        }));
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    PrefService* prefs = browser_state_->GetPrefs();
    prefs->SetString(prefs::kGoogleServicesLastSyncingUsername, kTestEmail);
    prefs->SetString(prefs::kGoogleServicesLastSyncingGaiaId, kTestGaiaID);

    base_view_controller_ = [[UIViewController alloc] init];
    base_view_controller_.view.backgroundColor = UIColor.blueColor;
    GetAnyKeyWindow().rootViewController = base_view_controller_;

    signin_manager_ = [[AddAccountSigninManager alloc]
        initWithBaseViewController:base_view_controller_
        identityInteractionManager:identity_interaction_manager_];
    signin_manager_delegate_ =
        OCMStrictProtocolMock(@protocol(AddAccountSigninManagerDelegate));
    signin_manager_.delegate = signin_manager_delegate_;
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)signin_manager_delegate_);
    PlatformTest::TearDown();
  }

  // Needed for test browser state created by TestChromeBrowserState().
  base::test::TaskEnvironment environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  UIViewController* base_view_controller_ = nil;

  AddAccountSigninManager* signin_manager_ = nil;
  id<AddAccountSigninManagerDelegate> signin_manager_delegate_ = nil;

  FakeSystemIdentityInteractionManager* identity_interaction_manager_ = nil;
  FakeSystemIdentity* fake_identity_ = nil;
};

// Verifies the following state in the successful add account flow:
//   - Account is added to the identity service
//   - Completion callback is called with success state
TEST_F(AddAccountSigninManagerTest, AddAccountWithEmail) {
  // Verify that completion was called with success state.
  FakeSystemIdentityInteractionManager.identity = fake_identity_;
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultSuccess
                                             identity:fake_identity_]);

  [signin_manager_ showSigninWithDefaultUserEmail:fake_identity_.userEmail];
  WaitForFakeAddAccountViewPresented(
      /*expectedUserEmail=*/fake_identity_.userEmail);
  [identity_interaction_manager_ simulateDidTapAddAccount];
  WaitForFakeAddAccountViewDismissed();
}

// Verifies the following state in the add account flow with a user cancel:
//   - Account is not added to the identity service
//   - Completion callback is called with user cancel state
TEST_F(AddAccountSigninManagerTest, AddAccountWithEmailIntentWithUserCancel) {
  // Verify that completion was called with canceled result state.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultCanceledByUser
                                             identity:nil]);

  [signin_manager_ showSigninWithDefaultUserEmail:@"email@example.com"];
  WaitForFakeAddAccountViewPresented(
      /*expectedUserEmail=*/@"email@example.com");
  [identity_interaction_manager_ simulateDidTapCancel];
  WaitForFakeAddAccountViewDismissed();
}

// Verifies the following state in the add account flow with an error handled by
// the view controller:
//   - Account is not added to the identity service
//   - Completion callback is called with user cancel state
TEST_F(AddAccountSigninManagerTest,
       AddAccountWithEmailWithErrorHandledByViewController) {
  // Verify that completion was called with canceled result state and an error
  // is shown.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFailedWithError:[OCMArg any]]);

  [signin_manager_ showSigninWithDefaultUserEmail:@"email@example.com"];
  WaitForFakeAddAccountViewPresented(
      /*expectedUserEmail=*/@"email@example.com");
  [identity_interaction_manager_ simulateDidThrowUnhandledError];
  WaitForFakeAddAccountViewDismissed();
}

TEST_F(AddAccountSigninManagerTest, AddAccountWithEmailSigninInterrupted) {
  // Verify that completion was called with interrupted result state.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultInterrupted
                                             identity:nil]);

  [signin_manager_ showSigninWithDefaultUserEmail:@"email@example.com"];
  WaitForFakeAddAccountViewPresented(
      /*expectedUserEmail=*/@"email@example.com");
  __block BOOL completionCalled = NO;
  [signin_manager_
      interruptWithAction:SigninCoordinatorInterrupt::DismissWithAnimation
               completion:^() {
                 completionCalled = YES;
               }];
  WaitForFakeAddAccountViewDismissed();
  EXPECT_TRUE(completionCalled);
}

// Verifies the following state in the successful reauth flow:
//   - Account is added to the identity service
//   - Completion callback is called with success state
TEST_F(AddAccountSigninManagerTest, AddAccountWithoutEmailWithSuccess) {
  // Verify that completion was called with canceled result state.
  FakeSystemIdentityInteractionManager.identity = fake_identity_;
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultSuccess
                                             identity:fake_identity_]);

  [signin_manager_ showSigninWithDefaultUserEmail:nil];
  WaitForFakeAddAccountViewPresented(/*expectedUserEmail=*/nil);
  [identity_interaction_manager_ simulateDidTapAddAccount];
  WaitForFakeAddAccountViewDismissed();
}

// Verifies the following state in the reauth flow with a user cancel:
//   - Account is not added to the identity service
//   - Completion callback is called with user cancel state
TEST_F(AddAccountSigninManagerTest, AddAccountWithoutEmailWithUserCancel) {
  // Verify that completion was called with canceled result state.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultCanceledByUser
                                             identity:nil]);

  [signin_manager_ showSigninWithDefaultUserEmail:nil];
  WaitForFakeAddAccountViewPresented(/*expectedUserEmail=*/nil);
  [identity_interaction_manager_ simulateDidTapCancel];
  WaitForFakeAddAccountViewDismissed();
}

// Verifies the following state in the successful reauth flow:
//   - No last know sync account in the identity service
//   - Completion callback is called with success state
//
// Regression test for crbug/1443096
// TODO(crbug.com/1454101): This test is not relevant anymore in this class.
// This should be migrated in a EGTest or an unittest for
// AddAccountSigninCoordinator.
TEST_F(AddAccountSigninManagerTest,
       AddAccountWithoutEmailWithSuccessNoLastKnowSyncAccount) {
  PrefService* prefs = browser_state_->GetPrefs();
  prefs->ClearPref(prefs::kGoogleServicesLastSyncingUsername);
  prefs->ClearPref(prefs::kGoogleServicesLastSyncingGaiaId);

  // Verify that completion was called with canceled result state.
  FakeSystemIdentityInteractionManager.identity = fake_identity_;
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultSuccess
                                             identity:fake_identity_]);

  [signin_manager_ showSigninWithDefaultUserEmail:nil];
  WaitForFakeAddAccountViewPresented(/*expectedUserEmail=*/nil);
  [identity_interaction_manager_ simulateDidTapAddAccount];
  WaitForFakeAddAccountViewDismissed();
}

// Verifies the following state in the reauth flow with an error handled by the
// view controller:
//   - Account is not added to the identity service
//   - Completion callback is called with user cancel state
TEST_F(AddAccountSigninManagerTest,
       AddAccountWithoutEmailWithErrorHandledByViewController) {
  // Verify that completion was called with canceled result state and an error
  // is shown.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFailedWithError:[OCMArg any]]);

  [signin_manager_ showSigninWithDefaultUserEmail:nil];
  WaitForFakeAddAccountViewPresented(/*expectedUserEmail=*/nil);
  [identity_interaction_manager_ simulateDidThrowUnhandledError];
  WaitForFakeAddAccountViewDismissed();
}

TEST_F(AddAccountSigninManagerTest, AddAccountWithoutEmailSigninInterrupted) {
  // Verify that completion was called with interrupted result state.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultInterrupted
                                             identity:nil]);

  [signin_manager_ showSigninWithDefaultUserEmail:nil];
  WaitForFakeAddAccountViewPresented(/*expectedUserEmail=*/nil);
  __block BOOL completionCalled = NO;
  [signin_manager_
      interruptWithAction:SigninCoordinatorInterrupt::DismissWithAnimation
               completion:^() {
                 completionCalled = YES;
               }];
  WaitForFakeAddAccountViewDismissed();
  EXPECT_TRUE(completionCalled);
}
