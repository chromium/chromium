// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_manager.h"

#import <UIKit/UIKit.h>

#import "base/test/task_environment.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Constants for configuring a FakeChromeIdentity.
const char kTestGaiaID[] = "fooID";
const char kTestEmail[] = "foo@gmail.com";
}  // namespace

// Fake implementation of ChromeIdentityInteractionManagerDelegate that calls
// completion callback.
@interface FakeChromeIdentityInteractionManagerDelegate
    : NSObject <ChromeIdentityInteractionManagerDelegate>
@end

@implementation FakeChromeIdentityInteractionManagerDelegate
- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
     presentViewController:(UIViewController*)viewController
                  animated:(BOOL)animated
                completion:(ProceduralBlock)completion {
  if (completion) {
    completion();
  }
}

- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
    dismissViewControllerAnimated:(BOOL)animated
                       completion:(ProceduralBlock)completion {
  if (completion) {
    completion();
  }
}
@end

class AddAccountSigninManagerTest : public PlatformTest {
 public:
  AddAccountSigninManagerTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        base_view_controller_([[UIViewController alloc] init]),
        identity_service_(
            ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()) {
    identity_interaction_manager_delegate_ =
        [[FakeChromeIdentityInteractionManagerDelegate alloc] init];
    identity_interaction_manager_ = GetIdentityInteractionManager();
  }

  FakeChromeIdentityInteractionManager* GetIdentityInteractionManager() {
    FakeChromeIdentityInteractionManager* identity_interaction_manager =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
            ->CreateFakeChromeIdentityInteractionManager(
                identity_interaction_manager_delegate_);
    fake_identity_ = [FakeChromeIdentity
        identityWithEmail:[NSString stringWithUTF8String:kTestEmail]
                   gaiaID:[NSString stringWithUTF8String:kTestGaiaID]
                     name:@"Foo"];
    FakeChromeIdentityInteractionManager.identity = fake_identity_;
    return identity_interaction_manager;
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForBrowserState(browser_state_.get());
  }

  // Registers account preferences that will be used in reauthentication.
  PrefService* GetPrefService() {
    TestingPrefServiceSimple* prefs = new TestingPrefServiceSimple();
    PrefRegistrySimple* registry = prefs->registry();
    registry->RegisterStringPref(prefs::kGoogleServicesLastUsername,
                                 kTestEmail);
    registry->RegisterStringPref(prefs::kGoogleServicesLastAccountId,
                                 kTestGaiaID);
    return prefs;
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    signin_manager_ = [[AddAccountSigninManager alloc]
        initWithPresentingViewController:base_view_controller_
              identityInteractionManager:identity_interaction_manager_
                             prefService:GetPrefService()
                         identityManager:GetIdentityManager()];
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
  UIViewController* base_view_controller_;

  AddAccountSigninManager* signin_manager_ = nil;
  id<AddAccountSigninManagerDelegate> signin_manager_delegate_ = nil;

  ios::FakeChromeIdentityService* identity_service_ = nullptr;
  FakeChromeIdentityInteractionManager* identity_interaction_manager_ = nil;
  id<ChromeIdentityInteractionManagerDelegate>
      identity_interaction_manager_delegate_ = nil;
  FakeChromeIdentity* fake_identity_ = nil;
};

// Verifies the following state in the successful add account flow:
//   - Account is added to the identity service
//   - Completion callback is called with success state
TEST_F(AddAccountSigninManagerTest, AddAccountIntent) {
  // Verify that completion was called with success state.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultSuccess
                                             identity:fake_identity_]);

  [signin_manager_
      showSigninWithIntent:AddAccountSigninIntentAddSecondaryAccount];
  [identity_interaction_manager_ addAccountViewControllerDidTapSignIn];

  // Account added.
  EXPECT_TRUE(identity_service_->IsValidIdentity(fake_identity_));
}

// Verifies the following state in the add account flow with a user cancel:
//   - Account is not added to the identity service
//   - Completion callback is called with user cancel state
TEST_F(AddAccountSigninManagerTest, AddAccountIntentWithUserCancel) {
  // Verify that completion was called with canceled result state.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultCanceledByUser
                                             identity:fake_identity_]);

  [signin_manager_
      showSigninWithIntent:AddAccountSigninIntentAddSecondaryAccount];
  [identity_interaction_manager_ addAccountViewControllerDidTapCancel];

  // No account is present.
  EXPECT_FALSE(identity_service_->HasIdentities());
}

// Verifies the following state in the add account flow with an error handled by
// the view controller:
//   - Account is not added to the identity service
//   - Completion callback is called with user cancel state
TEST_F(AddAccountSigninManagerTest,
       AddAccountIntentWithErrorHandledByViewController) {
  // Verify that completion was called with canceled result state and an error
  // is shown.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFailedWithError:[OCMArg any]]);

  [signin_manager_
      showSigninWithIntent:AddAccountSigninIntentAddSecondaryAccount];
  [identity_interaction_manager_
      addAccountViewControllerDidThrowUnhandledError];

  // No account is present.
  EXPECT_FALSE(identity_service_->HasIdentities());
}

TEST_F(AddAccountSigninManagerTest, AddAccountSigninInterrupted) {
  // Verify that completion was called with interrupted result state.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultInterrupted
                                             identity:nil]);

  [signin_manager_
      showSigninWithIntent:AddAccountSigninIntentAddSecondaryAccount];
  signin_manager_.signinInterrupted = YES;
  [identity_interaction_manager_ addAccountViewControllerDidInterrupt];
}

// Verifies the following state in the successful reauth flow:
//   - Account is added to the identity service
//   - Completion callback is called with success state
TEST_F(AddAccountSigninManagerTest, ReauthIntentWithSuccess) {
  // Verify that completion was called with canceled result state.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultSuccess
                                             identity:fake_identity_]);

  [signin_manager_
      showSigninWithIntent:AddAccountSigninIntentReauthPrimaryAccount];
  [identity_interaction_manager_ addAccountViewControllerDidTapSignIn];

  EXPECT_TRUE(identity_service_->HasIdentities());
}

// Verifies the following state in the reauth flow with a user cancel:
//   - Account is not added to the identity service
//   - Completion callback is called with user cancel state
TEST_F(AddAccountSigninManagerTest, ReauthIntentWithUserCancel) {
  // Verify that completion was called with canceled result state.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultCanceledByUser
                                             identity:fake_identity_]);

  [signin_manager_
      showSigninWithIntent:AddAccountSigninIntentReauthPrimaryAccount];
  [identity_interaction_manager_ addAccountViewControllerDidTapCancel];

  // No account is present.
  EXPECT_FALSE(identity_service_->HasIdentities());
}

// Verifies the following state in the reauth flow with an error handled by the
// view controller:
//   - Account is not added to the identity service
//   - Completion callback is called with user cancel state
TEST_F(AddAccountSigninManagerTest,
       ReauthIntentWithErrorHandledByViewController) {
  // Verify that completion was called with canceled result state and an error
  // is shown.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFailedWithError:[OCMArg any]]);

  [signin_manager_
      showSigninWithIntent:AddAccountSigninIntentReauthPrimaryAccount];
  [identity_interaction_manager_
      addAccountViewControllerDidThrowUnhandledError];

  // No account is present.
  EXPECT_FALSE(identity_service_->HasIdentities());
}

TEST_F(AddAccountSigninManagerTest, ReauthSigninInterrupted) {
  // Verify that completion was called with interrupted result state.
  OCMExpect([signin_manager_delegate_
      addAccountSigninManagerFinishedWithSigninResult:
          SigninCoordinatorResultInterrupted
                                             identity:nil]);

  [signin_manager_
      showSigninWithIntent:AddAccountSigninIntentReauthPrimaryAccount];
  signin_manager_.signinInterrupted = YES;
  [identity_interaction_manager_ addAccountViewControllerDidInterrupt];
}
