// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow.h"

#include <memory>

#include "base/bind.h"
#import "base/mac/scoped_block.h"
#include "base/memory/ptr_util.h"
#import "base/test/ios/wait_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class AuthenticationFlowTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    builder.SetPrefService(CreatePrefService());
    browser_state_ = builder.Build();
    WebStateList* web_state_list = nullptr;
    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), web_state_list);

    ios::FakeChromeIdentityService* identityService =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identityService->AddIdentities(@[ @"identity1", @"identity2" ]);
    identity1_ =
        [identityService->GetAllIdentitiesSortedForDisplay() objectAtIndex:0];
    identity2_ =
        [identityService->GetAllIdentitiesSortedForDisplay() objectAtIndex:1];
    sign_in_completion_ = ^(BOOL success) {
      finished_ = true;
      signed_in_success_ = success;
    };
    finished_ = false;
    signed_in_success_ = false;
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());
    return prefs;
  }

  AuthenticationFlowPerformer* GetAuthenticationFlowPerformer() {
    return static_cast<AuthenticationFlowPerformer*>(performer_);
  }

  // Creates a new AuthenticationFlow with default values for fields that are
  // not directly useful.
  void CreateAuthenticationFlow(ShouldClearData shouldClearData,
                                PostSignInAction postSignInAction) {
    ChromeIdentity* identity = identity1_;
    view_controller_ = [OCMockObject niceMockForClass:[UIViewController class]];
    authentication_flow_ =
        [[AuthenticationFlow alloc] initWithBrowser:browser_.get()
                                           identity:identity
                                    shouldClearData:shouldClearData
                                   postSignInAction:postSignInAction
                           presentingViewController:view_controller_];
    performer_ =
        [OCMockObject mockForClass:[AuthenticationFlowPerformer class]];
    [authentication_flow_
        setPerformerForTesting:GetAuthenticationFlowPerformer()];
  }

  // Checks if the AuthenticationFlow operation has completed, and whether it
  // was successful.
  void CheckSignInCompletion(bool expectedSignedIn) {
    base::test::ios::WaitUntilCondition(^bool {
      return finished_;
    });
    EXPECT_EQ(true, finished_);
    EXPECT_EQ(expectedSignedIn, signed_in_success_);
    [performer_ verify];
  }

  web::WebTaskEnvironment task_environment_;
  AuthenticationFlow* authentication_flow_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  ChromeIdentity* identity1_;
  ChromeIdentity* identity2_;
  OCMockObject* performer_;
  signin_ui::CompletionCallback sign_in_completion_;
  UIViewController* view_controller_;

  // State of the flow
  bool finished_;
  bool signed_in_success_;
};

// Tests a Sign In of a normal account on the same profile, merging user data
// and showing the sync settings.
TEST_F(AuthenticationFlowTest, TestSignInSimple) {
  CreateAuthenticationFlow(SHOULD_CLEAR_DATA_MERGE_DATA,
                           POST_SIGNIN_ACTION_START_SYNC);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:nil];
  }] fetchManagedStatus:browser_state_.get()
             forIdentity:identity1_];

  [[[performer_ expect] andReturnBool:NO]
      shouldHandleMergeCaseForIdentity:identity1_
                          browserState:browser_state_.get()];

  [[performer_ expect] signInIdentity:identity1_
                     withHostedDomain:nil
                       toBrowserState:browser_state_.get()];

  [[performer_ expect] commitSyncForBrowserState:browser_state_.get()];

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(true);
}

// Tests that signing in an already signed in account correctly signs it out
// and back in.
TEST_F(AuthenticationFlowTest, TestAlreadySignedIn) {
  CreateAuthenticationFlow(SHOULD_CLEAR_DATA_MERGE_DATA,
                           POST_SIGNIN_ACTION_NONE);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:nil];
  }] fetchManagedStatus:browser_state_.get()
             forIdentity:identity1_];

  [[[performer_ expect] andReturnBool:NO]
      shouldHandleMergeCaseForIdentity:identity1_
                          browserState:browser_state_.get()];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didSignOut];
  }] signOutBrowserState:browser_state_.get()];

  [[performer_ expect] signInIdentity:identity1_
                     withHostedDomain:nil
                       toBrowserState:browser_state_.get()];

  AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
      ->SignIn(identity1_);
  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(true);
}

// Tests a Sign In of a different account, requiring a sign out of the already
// signed in account, and asking the user whether data should be cleared or
// merged.
TEST_F(AuthenticationFlowTest, TestSignOutUserChoice) {
  CreateAuthenticationFlow(SHOULD_CLEAR_DATA_USER_CHOICE,
                           POST_SIGNIN_ACTION_START_SYNC);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:nil];
  }] fetchManagedStatus:browser_state_.get()
             forIdentity:identity1_];

  [[[performer_ expect] andReturnBool:YES]
      shouldHandleMergeCaseForIdentity:identity1_
                          browserState:browser_state_.get()];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_
        didChooseClearDataPolicy:SHOULD_CLEAR_DATA_CLEAR_DATA];
  }] promptMergeCaseForIdentity:identity1_
                        browser:browser_.get()
                 viewController:view_controller_];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didSignOut];
  }] signOutBrowserState:browser_state_.get()];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didClearData];
  }] clearDataFromBrowser:browser_.get() commandHandler:nil];

  [[performer_ expect] signInIdentity:identity1_
                     withHostedDomain:nil
                       toBrowserState:browser_state_.get()];

  [[performer_ expect] commitSyncForBrowserState:browser_state_.get()];

  AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
      ->SignIn(identity2_);
  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(true);
}

// Tests the cancelling of a Sign In.
TEST_F(AuthenticationFlowTest, TestCancel) {
  CreateAuthenticationFlow(SHOULD_CLEAR_DATA_USER_CHOICE,
                           POST_SIGNIN_ACTION_START_SYNC);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:nil];
  }] fetchManagedStatus:browser_state_.get()
             forIdentity:identity1_];

  [[[performer_ expect] andReturnBool:YES]
      shouldHandleMergeCaseForIdentity:identity1_
                          browserState:browser_state_.get()];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ cancelAndDismiss];
  }] promptMergeCaseForIdentity:identity1_
                        browser:browser_.get()
                 viewController:view_controller_];

  [[performer_ expect] cancelAndDismiss];

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(false);
}

// Tests the fetch managed status failure case.
TEST_F(AuthenticationFlowTest, TestFailFetchManagedStatus) {
  CreateAuthenticationFlow(SHOULD_CLEAR_DATA_MERGE_DATA,
                           POST_SIGNIN_ACTION_START_SYNC);

  NSError* error = [NSError errorWithDomain:@"foo" code:0 userInfo:nil];
  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFailFetchManagedStatus:error];
  }] fetchManagedStatus:browser_state_.get()
             forIdentity:identity1_];

  [[[performer_ expect] andDo:^(NSInvocation* invocation) {
    __unsafe_unretained ProceduralBlock completionBlock;
    [invocation getArgument:&completionBlock atIndex:3];
    completionBlock();
  }] showAuthenticationError:[OCMArg any]
               withCompletion:[OCMArg any]
               viewController:view_controller_];

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(false);
}

// Tests the managed sign in confirmation dialog is shown when signing in to
// a managed identity.
TEST_F(AuthenticationFlowTest, TestShowManagedConfirmation) {
  CreateAuthenticationFlow(SHOULD_CLEAR_DATA_CLEAR_DATA,
                           POST_SIGNIN_ACTION_START_SYNC);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:@"foo.com"];
  }] fetchManagedStatus:browser_state_.get()
             forIdentity:identity1_];

  [[[performer_ expect] andReturnBool:NO]
      shouldHandleMergeCaseForIdentity:identity1_
                          browserState:browser_state_.get()];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didAcceptManagedConfirmation];
  }] showManagedConfirmationForHostedDomain:@"foo.com"
                              viewController:view_controller_];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didClearData];
  }] clearDataFromBrowser:browser_.get() commandHandler:nil];

  [[performer_ expect] signInIdentity:identity1_
                     withHostedDomain:@"foo.com"
                       toBrowserState:browser_state_.get()];

  [[performer_ expect] commitSyncForBrowserState:browser_state_.get()];

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(true);
}

}  // namespace
