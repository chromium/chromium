// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_mediator.h"

#import <UIKit/UIKit.h>

#import "base/test/task_environment.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"

using syncer::MockSyncService;
using syncer::SyncService;
using testing::Return;

namespace {
// Constants for configuring a FakeSystemIdentity.
const char kTestGaiaID[] = "fooID";
const char kTestEmail[] = "foo@gmail.com";

}  // namespace

class AdvancedSettingsSigninMediatorTest : public PlatformTest {
 public:
  AdvancedSettingsSigninMediatorTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    identity_ = [FakeSystemIdentity fakeIdentity1];
    fake_system_identity_manager()->AddIdentity(identity_);

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    mediator_ = [[AdvancedSettingsSigninMediator alloc]
        initWithAuthenticationService:authentication_service()
                          syncService:sync_service()
                          prefService:GetPrefService()
                      identityManager:identity_manager()];

    authentication_service_ =
        static_cast<AuthenticationService*>(authentication_service());
  }

  // Registers account preferences that will be used in reauthentication.
  PrefService* GetPrefService() {
    TestingPrefServiceSimple* prefs = new TestingPrefServiceSimple();
    PrefRegistrySimple* registry = prefs->registry();
    registry->RegisterStringPref(prefs::kGoogleServicesLastUsername,
                                 kTestEmail);
    registry->RegisterStringPref(prefs::kGoogleServicesLastGaiaId, kTestGaiaID);
    return prefs;
  }

  // Identity services.
  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  SyncService* sync_service() {
    return SyncServiceFactory::GetForBrowserState(browser_state_.get());
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForBrowserState(browser_state_.get());
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  base::test::TaskEnvironment environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  id<SystemIdentity> identity_ = nil;

  AdvancedSettingsSigninMediator* mediator_ = nil;

  AuthenticationService* authentication_service_ = nullptr;
};

// Tests that a user's authentication does not change when sign-in is
// interrupted.
TEST_F(AdvancedSettingsSigninMediatorTest,
       saveUserPreferenceSigninInterruptedWithSyncDisabled) {
  authentication_service_->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  [mediator_
      saveUserPreferenceForSigninResult:SigninCoordinatorResultInterrupted
                    originalSigninState:
                        IdentitySigninStateSignedInWithSyncDisabled];

  ASSERT_TRUE(authentication_service_->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests that a user's authentication reverted when sign-in is
// interrupted with IdentitySigninStateSignedOut.
TEST_F(AdvancedSettingsSigninMediatorTest,
       saveUserPreferenceSigninInterruptedWithSignout) {
  authentication_service_->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  [mediator_
      saveUserPreferenceForSigninResult:SigninCoordinatorResultInterrupted
                    originalSigninState:IdentitySigninStateSignedOut];

  ASSERT_FALSE(authentication_service_->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}
