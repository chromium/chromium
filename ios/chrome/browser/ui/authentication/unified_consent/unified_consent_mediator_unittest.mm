// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_mediator.h"

#import <UIKit/UIKit.h>

#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
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
    identity1_ = [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                                gaiaID:@"foo1ID"
                                                  name:@"Fake Foo 1"];
    identity2_ = [FakeChromeIdentity identityWithEmail:@"foo2@gmail.com"
                                                gaiaID:@"foo2ID"
                                                  name:@"Fake Foo 2"];

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    browser_state_ = builder.Build();

    view_controller_ = [[UnifiedConsentViewController alloc] init];
    pref_service_ = new TestingPrefServiceSimple();

    mediator_delegate_mock_ =
        OCMProtocolMock(@protocol(UnifiedConsentMediatorDelegate));
    mediator_ = [[UnifiedConsentMediator alloc]
        initWithUnifiedConsentViewController:view_controller_
                       authenticationService:authentication_service()
                       accountManagerService:
                           ChromeAccountManagerServiceFactory::
                               GetForBrowserState(browser_state_.get())];
    mediator_.delegate = mediator_delegate_mock_;
  }

  void TearDown() override {
    // Ensures that the prefService is no longer observing changes.
    [mediator_ disconnect];

    EXPECT_OCMOCK_VERIFY((id)mediator_delegate_mock_);
    PlatformTest::TearDown();
  }
  // Identity services.
  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  ios::FakeChromeIdentityService* identity_service() {
    return ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;
  FakeChromeIdentity* identity1_ = nullptr;
  FakeChromeIdentity* identity2_ = nullptr;

  std::unique_ptr<TestChromeBrowserState> browser_state_;

  UnifiedConsentMediator* mediator_ = nullptr;
  PrefService* pref_service_ = nullptr;

  id<UnifiedConsentMediatorDelegate> mediator_delegate_mock_ = nil;
  UnifiedConsentViewController* view_controller_ = nullptr;
};

// Tests that there is no selected default identity for a signed-out user with
// no accounts on the device.
TEST_F(UnifiedConsentMediatorTest,
       SelectDefaultIdentityForSignedOutUserWithNoAccounts) {
  [mediator_ start];

  ASSERT_EQ(nil, mediator_.selectedIdentity);
}

// Tests that the default identity selected for a signed-out user with accounts
// on the device is the first option.
TEST_F(UnifiedConsentMediatorTest,
       SelectDefaultIdentityForSignedOutUserWithAccounts) {
  identity_service()->AddIdentity(identity2_);
  identity_service()->AddIdentity(identity1_);

  [mediator_ start];

  ASSERT_EQ(identity2_, mediator_.selectedIdentity);
}

// Tests that the default identity selected for a signed-in user is the
// authenticated identity.
TEST_F(UnifiedConsentMediatorTest, SelectDefaultIdentityForSignedInUser) {
  identity_service()->AddIdentity(identity2_);
  identity_service()->AddIdentity(identity1_);

  authentication_service()->SignIn(identity1_);
  [mediator_ start];

  ASSERT_EQ(identity1_, mediator_.selectedIdentity);
}

// Tests that |start| will override the selected identity with a pre-determined
// default identity based on the accounts on the device and user sign-in state.
TEST_F(UnifiedConsentMediatorTest, OverrideIdentityForSignedInUser) {
  identity_service()->AddIdentity(identity2_);
  identity_service()->AddIdentity(identity1_);

  authentication_service()->SignIn(identity1_);
  mediator_.selectedIdentity = identity2_;
  [mediator_ start];

  ASSERT_EQ(identity1_, mediator_.selectedIdentity);
}
