// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_mediator.h"

#import <UIKit/UIKit.h>

#import "base/test/task_environment.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/driver/mock_sync_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using syncer::MockSyncService;
using syncer::SyncService;
using testing::Return;

namespace {
// Constants for configuring a FakeChromeIdentity.
const char kTestGaiaID[] = "fooID";
const char kTestEmail[] = "foo@gmail.com";

std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<MockSyncService>();
}
}  // namespace

class AdvancedSettingsSigninMediatorTest : public PlatformTest {
 public:
  AdvancedSettingsSigninMediatorTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    identity_ = [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                               gaiaID:@"foo1ID"
                                                 name:@"Fake Foo 1"];
    identity_service()->AddIdentity(identity_);

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    browser_state_ = builder.Build();

    mediator_ = [[AdvancedSettingsSigninMediator alloc]
        initWithSyncSetupService:sync_setup_service()
           authenticationService:authentication_service()
                     syncService:sync_service()
                     prefService:GetPrefService()];

    sync_setup_service_mock_ =
        static_cast<SyncSetupServiceMock*>(sync_setup_service());
    authentication_service_fake_ =
        static_cast<AuthenticationServiceFake*>(authentication_service());
  }

  // Registers account preferences that will be used in reauthentication.
  PrefService* GetPrefService() {
    TestingPrefServiceSimple* prefs = new TestingPrefServiceSimple();
    PrefRegistrySimple* registry = prefs->registry();
    registry->RegisterStringPref(prefs::kGoogleServicesLastUsername,
                                 kTestEmail);
    registry->RegisterStringPref(prefs::kGoogleServicesLastAccountId,
                                 kTestGaiaID);
    registry->RegisterBooleanPref(autofill::prefs::kAutofillWalletImportEnabled,
                                  false);
    return prefs;
  }

  // Identity services.
  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  SyncSetupService* sync_setup_service() {
    return SyncSetupServiceFactory::GetForBrowserState(browser_state_.get());
  }

  SyncService* sync_service() {
    return SyncServiceFactory::GetForBrowserState(browser_state_.get());
  }

  ios::FakeChromeIdentityService* identity_service() {
    return ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  base::test::TaskEnvironment environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeChromeIdentity* identity_ = nullptr;

  AdvancedSettingsSigninMediator* mediator_ = nil;

  SyncSetupServiceMock* sync_setup_service_mock_ = nullptr;
  AuthenticationServiceFake* authentication_service_fake_ = nullptr;
};

// Tests that a user is authenticated and synced when sync is enabled and
// sign-in is successful.
TEST_F(AdvancedSettingsSigninMediatorTest,
       saveUserPreferenceSigninSuccessSyncEnabled) {
  EXPECT_CALL(*sync_setup_service_mock_, CanSyncFeatureStart)
      .WillOnce(Return(true));
  EXPECT_CALL(*sync_setup_service_mock_,
              SetFirstSetupComplete(
                  syncer::SyncFirstSetupCompleteSource::ADVANCED_FLOW_CONFIRM));

  authentication_service_fake_->SignIn(identity_);
  [mediator_ saveUserPreferenceForSigninResult:SigninCoordinatorResultSuccess];

  ASSERT_TRUE(authentication_service_fake_->IsAuthenticated());
}

// Tests that a user is authenticated but not synced when sync is disabled and
// sign-in is successful.
TEST_F(AdvancedSettingsSigninMediatorTest,
       saveUserPreferenceSigninSuccessSyncDisabled) {
  EXPECT_CALL(*sync_setup_service_mock_, CanSyncFeatureStart)
      .WillOnce(Return(false));
  EXPECT_CALL(*sync_setup_service_mock_,
              SetFirstSetupComplete(
                  syncer::SyncFirstSetupCompleteSource::ADVANCED_FLOW_CONFIRM))
      .Times(0);

  authentication_service_fake_->SignIn(identity_);
  [mediator_ saveUserPreferenceForSigninResult:SigninCoordinatorResultSuccess];

  ASSERT_TRUE(authentication_service_fake_->IsAuthenticated());
}

// Tests that a user is not authenticated when sign-in is canceled.
TEST_F(AdvancedSettingsSigninMediatorTest, saveUserPreferenceSigninCanceled) {
  authentication_service_fake_->SignIn(identity_);
  [mediator_
      saveUserPreferenceForSigninResult:SigninCoordinatorResultCanceledByUser];

  ASSERT_FALSE(authentication_service_fake_->IsAuthenticated());
}

// Tests that a user's authentication does not change when sign-in is
// interrupted.
TEST_F(AdvancedSettingsSigninMediatorTest,
       saveUserPreferenceSigninInterrupted) {
  authentication_service_fake_->SignIn(identity_);
  [mediator_
      saveUserPreferenceForSigninResult:SigninCoordinatorResultInterrupted];

  ASSERT_TRUE(authentication_service_fake_->IsAuthenticated());
}
