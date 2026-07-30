// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/signin/fullscreen_promo/model/fullscreen_signin_promo_scene_agent.h"

#import "base/memory/raw_ptr.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class FullscreenSigninPromoSceneAgentTest : public PlatformTest {
 public:
  FullscreenSigninPromoSceneAgentTest() : PlatformTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));

    profile_ = std::move(builder).Build();
    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);
    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_.get());
    promos_manager_ = std::make_unique<MockPromosManager>();
    agent_ = [[FullscreenSigninPromoSceneAgent alloc]
        initWithPromosManager:promos_manager_.get()
                  authService:authentication_service_
              identityManager:identity_manager_
                  syncService:&sync_service_
                  prefService:profile_->GetPrefs()];

    agent_.sceneState = scene_state_;

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
    profile_state_.profile = profile_.get();
    scene_state_.profileState = profile_state_;

    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());

    scene_state_.UIEnabled = YES;
  }

  void TearDown() override {
    @autoreleasepool {
      NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
      [standardDefaults
          removeObjectForKey:kDisplayedSSORecallForMajorVersionKey];
      [standardDefaults removeObjectForKey:kLastShownAccountGaiaIdVersionKey];
      [standardDefaults removeObjectForKey:kSigninPromoViewDisplayCountKey];
      [standardDefaults synchronize];
      scene_state_.UIEnabled = NO;
      [scene_state_ shutdown];
      scene_state_ = nil;
      profile_state_ = nil;
    }
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  syncer::TestSyncService sync_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  raw_ptr<AuthenticationService> authentication_service_;
  ProfileState* profile_state_;
  FakeSceneState* scene_state_;
  FullscreenSigninPromoSceneAgent* agent_;
};

// Tests that the sign-in fullscreen promo registers with the promo manager when
// the eligibility criteria are met.
TEST_F(FullscreenSigninPromoSceneAgentTest,
       TestFullscreenSigninPromoRegistration) {
  const base::Version version_1_0("1.0");
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(fake_identity1);
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);

  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::FullscreenSignin))
      .Times(1);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the sign-in fullscreen promo is not registered in the promo
// manager when the eligibility criteria are not met.
TEST_F(FullscreenSigninPromoSceneAgentTest,
       TestFullscreenSigninPromoNoRegistration) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::FullscreenSignin))
      .Times(0);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that when a promo was previously registered, it is deregistered when
// user is signed in and history sync is opted in.
TEST_F(FullscreenSigninPromoSceneAgentTest,
       TestPromoDeregistrationWhenSignedInWithHistorySync) {
  // Register the promo.
  const base::Version version_1_0("1.0");
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(fake_identity1);
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::FullscreenSignin))
      .Times(1);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::FullscreenSignin))
      .Times(0);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Sign in and enable history sync.
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::FullscreenSignin))
      .Times(0);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::FullscreenSignin))
      .Times(1);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, YES);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, YES);
  authentication_service_->SignIn(fake_identity1,
                                  signin_metrics::AccessPoint::kStartPage);
  EXPECT_TRUE(authentication_service_->HasPrimaryIdentity());
}

// Tests that when a promo was previously registered, it is still registered
// when user is signed in without history sync.
TEST_F(FullscreenSigninPromoSceneAgentTest,
       TestPromoRegistrationWhenSignedInWithoutHistorySync) {
  // Register the promo.
  const base::Version version_1_0("1.0");
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(fake_identity1);
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::FullscreenSignin))
      .Times(1);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::FullscreenSignin))
      .Times(0);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Sign in without history sync.
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::FullscreenSignin))
      .Times(0);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::FullscreenSignin))
      .Times(1);
  authentication_service_->SignIn(fake_identity1,
                                  signin_metrics::AccessPoint::kStartPage);
  EXPECT_TRUE(authentication_service_->HasPrimaryIdentity());
}
