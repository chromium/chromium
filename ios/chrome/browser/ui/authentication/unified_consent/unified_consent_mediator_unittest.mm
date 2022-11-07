// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_mediator.h"

#import <UIKit/UIKit.h>

#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class UnifiedConsentMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    identity1_ = [FakeSystemIdentity fakeIdentity1];
    identity2_ = [FakeSystemIdentity fakeIdentity2];
    identity3_ = [FakeSystemIdentity fakeIdentity3];

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    view_controller_ = [[UnifiedConsentViewController alloc]
        initWithPostRestoreSigninPromo:NO];
    pref_service_ = new TestingPrefServiceSimple();

    mediator_delegate_mock_ =
        OCMProtocolMock(@protocol(UnifiedConsentMediatorDelegate));
  }

  void TearDown() override {
    // Ensures that the prefService is no longer observing changes.
    [mediator_ disconnect];

    EXPECT_OCMOCK_VERIFY((id)mediator_delegate_mock_);
    PlatformTest::TearDown();
  }
  // Identity services.
  AuthenticationService* GetAuthenticationService() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  ios::FakeChromeIdentityService* GetIdentityService() {
    return ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

  void AddIdentities() {
    GetIdentityService()->AddIdentity(identity1_);
    GetIdentityService()->AddIdentity(identity2_);
    GetIdentityService()->AddIdentity(identity3_);
  }

  void CreateMediator() {
    mediator_ = [[UnifiedConsentMediator alloc]
        initWithUnifiedConsentViewController:view_controller_
                       authenticationService:GetAuthenticationService()
                       accountManagerService:
                           ChromeAccountManagerServiceFactory::
                               GetForBrowserState(browser_state_.get())];
    mediator_.delegate = mediator_delegate_mock_;
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;

  id<SystemIdentity> identity1_ = nil;
  id<SystemIdentity> identity2_ = nil;
  id<SystemIdentity> identity3_ = nil;

  UnifiedConsentMediator* mediator_ = nullptr;
  PrefService* pref_service_ = nullptr;

  id<UnifiedConsentMediatorDelegate> mediator_delegate_mock_ = nil;
  UnifiedConsentViewController* view_controller_ = nullptr;
};

// Tests that there is no selected default identity for a signed-out user with
// no accounts on the device.
TEST_F(UnifiedConsentMediatorTest,
       SelectDefaultIdentityForSignedOutUserWithNoAccounts) {
  CreateMediator();
  [mediator_ start];

  ASSERT_EQ(nil, mediator_.selectedIdentity);
}

// Tests that the default identity selected for a signed-out user with accounts
// on the device is the first option.
TEST_F(UnifiedConsentMediatorTest,
       SelectDefaultIdentityForSignedOutUserWithAccounts) {
  AddIdentities();
  CreateMediator();

  [mediator_ start];

  ASSERT_EQ(identity1_, mediator_.selectedIdentity);
}

// Tests that the default identity becomes the next identity on the device after
// forgetting the default identity.
TEST_F(UnifiedConsentMediatorTest, SelectDefaultIdentityAfterForgetIdentity) {
  AddIdentities();
  CreateMediator();

  [mediator_ start];
  ASSERT_EQ(identity1_, mediator_.selectedIdentity);
  GetIdentityService()->ForgetIdentity(identity1_, nil);

  ASSERT_EQ(identity2_, mediator_.selectedIdentity);
}

// Tests that the default identity selected for a signed-in user is the
// authenticated identity.
TEST_F(UnifiedConsentMediatorTest, SelectDefaultIdentityForSignedInUser) {
  AddIdentities();
  CreateMediator();

  GetAuthenticationService()->SignIn(identity2_);
  [mediator_ start];

  ASSERT_EQ(identity2_, mediator_.selectedIdentity);
}

// Tests that the default identity is the next identity on the device after
// sign-out and forgetting the authenticated identity.
TEST_F(UnifiedConsentMediatorTest,
       SelectDefaultIdentityAfterSignOutAndForgetIdentity) {
  AddIdentities();
  CreateMediator();

  GetAuthenticationService()->SignIn(identity3_);
  [mediator_ start];
  ASSERT_EQ(identity3_, mediator_.selectedIdentity);
  GetAuthenticationService()->SignOut(signin_metrics::SIGNOUT_TEST, false, nil);
  GetIdentityService()->ForgetIdentity(identity3_, nil);

  ASSERT_EQ(identity1_, mediator_.selectedIdentity);
}

// Tests that the selected identity before start is kept.
TEST_F(UnifiedConsentMediatorTest, SelectIdentity) {
  AddIdentities();
  CreateMediator();

  mediator_.selectedIdentity = identity2_;
  [mediator_ start];

  ASSERT_EQ(identity2_, mediator_.selectedIdentity);
}

// Tests that `start` will not override the selected identity with a
// pre-determined default identity based on the accounts on the device and user
// sign-in state.
TEST_F(UnifiedConsentMediatorTest, DontOverrideIdentityForSignedInUser) {
  AddIdentities();
  CreateMediator();

  GetAuthenticationService()->SignIn(identity1_);
  mediator_.selectedIdentity = identity2_;
  [mediator_ start];

  ASSERT_EQ(identity2_, mediator_.selectedIdentity);
}
