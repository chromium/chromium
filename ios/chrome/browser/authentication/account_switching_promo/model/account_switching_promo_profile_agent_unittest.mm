// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_switching_promo/model/account_switching_promo_profile_agent.h"

#import <memory>
#import <optional>
#import <string>
#import <utility>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_service.h"
#import "components/signin/core/browser/account_preview_data_service.h"
#import "components/signin/core/browser/test_account_preview_data_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/browser/account_switching_promo_metrics.h"
#import "components/signin/public/browser/account_switching_promo_prefs.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/account_preview_data_service_factory.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class AccountSwitchingPromoProfileAgentTest : public PlatformTest {
 public:
  // Marks `identity` as known not to be subject to parental controls.
  void SetNotSupervised(id<SystemIdentity> identity) {
    SetSupervised(identity, /*supervised=*/false);
  }

  // Sets whether `identity` is subject to parental controls (`supervised`).
  void SetSupervised(id<SystemIdentity> identity, bool supervised) {
    AccountInfo account =
        identity_manager_->FindExtendedAccountInfoByGaiaId(identity.gaiaId);
    AccountCapabilitiesTestMutator mutator(&account);
    mutator.set_is_subject_to_parental_controls(supervised);
    account = AccountInfo::Builder(account)
                  .SetIsChildAccount(supervised ? signin::Tribool::kTrue
                                                : signin::Tribool::kFalse)
                  .Build();
    signin::UpdateAccountInfoForAccount(identity_manager_, account);
  }

  // Adds `primary` and `target` to the device, signs in with `primary` as a
  // non-supervised account, and sets `target` as the preferred account for
  // switching. `target` may be `primary`.
  void SetUpEligibleAccounts(FakeSystemIdentity* primary,
                             FakeSystemIdentity* target) {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(primary);
    if (target != primary) {
      system_identity_manager->AddIdentity(target);
    }
    authentication_service_->SignIn(primary,
                                    signin_metrics::AccessPoint::kStartPage);
    signin::AccountPreviewDataService::AccountPreviewPreference pref{
        .gaia_id = target.gaiaId};
    test_preview_service_->SetPreferredAccountForSwitching(pref);
    SetNotSupervised(primary);
  }

  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    builder.AddTestingFactory(
        AccountPreviewDataServiceFactory::GetInstance(),
        base::BindOnce(
            &std::make_unique<signin::TestAccountPreviewDataService>));
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindOnce(
            &std::make_unique<
                testing::NiceMock<feature_engagement::test::MockTracker>>));
    builder.AddTestingFactory(
        PromosManagerFactory::GetInstance(),
        base::BindOnce(&std::make_unique<MockPromosManager>));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);

    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_);
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
    test_preview_service_ = static_cast<signin::TestAccountPreviewDataService*>(
        AccountPreviewDataServiceFactory::GetForProfile(profile_));
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_));
    promos_manager_ = static_cast<MockPromosManager*>(
        PromosManagerFactory::GetForProfile(profile_));

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
    profile_state_.profile = profile_;
    scene_state_.profileState = profile_state_;
    scene_state_.UIEnabled = YES;
    [profile_state_ sceneStateConnected:scene_state_];

    agent_ = [[AccountSwitchingPromoProfileAgent alloc] init];
    [profile_state_ addAgent:agent_];
  }

  void TearDown() override {
    scene_state_.UIEnabled = NO;
    [scene_state_ shutdown];
    if (agent_) {
      [profile_state_ removeAgent:agent_];
      agent_ = nil;
    }
    scene_state_ = nil;
    profile_state_ = nil;
    authentication_service_ = nullptr;
    identity_manager_ = nullptr;
    test_preview_service_ = nullptr;
    mock_tracker_ = nullptr;
    promos_manager_ = nullptr;
    profile_ = nullptr;
    [AccountSwitchingPromoProfileAgent resetPromoAlreadyEvaluatedForTesting];
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  FakeSceneState* scene_state_;
  ProfileState* profile_state_;
  raw_ptr<AuthenticationService> authentication_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<signin::TestAccountPreviewDataService> test_preview_service_;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_;
  raw_ptr<MockPromosManager> promos_manager_;
  AccountSwitchingPromoProfileAgent* agent_;
};

// Tests that signing in notifies `feature_engagement::Tracker`, and that the
// promo is registered only the first time the scene is foreground-active.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       NotifiesFeatureEngagementOnPrimarySigninAndRegistersPromoOnce) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(
          feature_engagement::events::kAccountSwitchingPrimaryAccountChanged))
      .Times(1);
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1],
                        [FakeSystemIdentity fakeIdentity2]);
  task_environment_.FastForwardBy(base::Days(28));

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(
                                    promos_manager::Promo::AccountSwitching))
      .Times(1);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  histogram_tester.ExpectUniqueSample(
      signin::kAccountSwitchingPromoEventsHistogram,
      signin::AccountSwitchingPromoEvent::kEligibleOnStartup, 1);

  // Going to the background and back does not register the promo again.
  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  histogram_tester.ExpectTotalCount(
      signin::kAccountSwitchingPromoEventsHistogram, 1);
}

// Tests that signing out does not notify `feature_engagement::Tracker`.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DoesNotNotifyFeatureEngagementOnSignout) {
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1],
                        [FakeSystemIdentity fakeIdentity2]);
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(
          feature_engagement::events::kAccountSwitchingPrimaryAccountChanged))
      .Times(0);
  authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest);
}

// Tests that signing in after the agent is removed from its profile state
// neither notifies `feature_engagement::Tracker` nor crashes.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DoesNotNotifyFeatureEngagementAfterRemovedFromProfileState) {
  // Keeps the agent alive after it is removed from `profile_state_`.
  AccountSwitchingPromoProfileAgent* agent = agent_;
  agent_ = nil;
  [profile_state_ removeAgent:agent];
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(
          feature_engagement::events::kAccountSwitchingPrimaryAccountChanged))
      .Times(0);
  // Simulate the user signing in after the agent was removed.
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(primary);
  authentication_service_->SignIn(primary,
                                  signin_metrics::AccessPoint::kStartPage);
}

// Tests that clearing `profileState.profile` during `ProfileController`
// shutdown (before the `ProfileIOS` itself is destroyed) does not crash if an
// observer callback fires in that window.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DoesNotCrashWhenProfileStateProfileClearedDuringShutdown) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* target = [FakeSystemIdentity fakeIdentity2];
  SetUpEligibleAccounts(primary, target);

  // Simulate `-[ProfileController shutdown]` clearing `profile_state_.profile`
  // before the `ProfileIOS` is destroyed.
  profile_state_.profile = nullptr;

  // Trigger an observer callback that checks `-isPersonalProfile` while
  // `profile_state_.profile` is null. The agent cannot access `PromosManager`
  // without `profile_state_.profile`, so the promo is not deregistered.
  EXPECT_CALL(*promos_manager_, DeregisterPromo(testing::_)).Times(0);
  authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest);
}

// Tests that opening a second scene (e.g., a second window on iPad) does not
// emit `kEligibleOnStartup` or call `RegisterPromoForSingleDisplay()` again.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       RegistersPromoOnceAcrossMultipleScenes) {
  base::HistogramTester histogram_tester;
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1],
                        [FakeSystemIdentity fakeIdentity2]);
  // Let enough time pass for the promo to be eligible.
  task_environment_.FastForwardBy(base::Days(28));

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(
                                    promos_manager::Promo::AccountSwitching))
      .Times(1);
  // Simulate the first scene becoming foreground active.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  histogram_tester.ExpectUniqueSample(
      signin::kAccountSwitchingPromoEventsHistogram,
      signin::AccountSwitchingPromoEvent::kEligibleOnStartup, 1);

  // Connect and activate a second scene for the same profile.
  FakeSceneState* second_scene_state =
      [[FakeSceneState alloc] initWithProfile:profile_];
  second_scene_state.profileState = profile_state_;
  second_scene_state.UIEnabled = YES;
  [profile_state_ sceneStateConnected:second_scene_state];
  second_scene_state.activationLevel = SceneActivationLevelForegroundActive;

  histogram_tester.ExpectTotalCount(
      signin::kAccountSwitchingPromoEventsHistogram, 1);

  // Simulate the second scene being torn down.
  second_scene_state.UIEnabled = NO;
  [second_scene_state shutdown];
}

// Tests that an agent attached to a profile that is not the personal profile
// neither registers nor deregisters the promo, and has no `targetIdentity`.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DoesNothingWhenProfileIsNotPersonal) {
  [profile_state_ removeAgent:agent_];
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1],
                        [FakeSystemIdentity fakeIdentity2]);
  // Let enough time pass for the promo to be eligible.
  task_environment_.FastForwardBy(base::Days(28));
  const std::string other_profile_name =
      profile_manager_.ReserveNewProfileName();
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->SetPersonalProfileName(other_profile_name);

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(testing::_))
      .Times(0);
  EXPECT_CALL(*promos_manager_, DeregisterPromo(testing::_)).Times(0);
  agent_ = [[AccountSwitchingPromoProfileAgent alloc] init];
  [profile_state_ addAgent:agent_];
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_FALSE(agent_.targetIdentity);
}

// Tests that evaluating at startup on a non-personal (managed) profile does
// not prevent a personal profile agent from evaluating later in the same
// session.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       ManagedProfileDoesNotBlockPersonalProfileEvaluation) {
  [profile_state_ removeAgent:agent_];
  const std::string personal_profile_name = profile_->GetProfileName();
  const std::string managed_profile_name =
      profile_manager_.ReserveNewProfileName();
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->SetPersonalProfileName(managed_profile_name);

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(testing::_))
      .Times(0);
  agent_ = [[AccountSwitchingPromoProfileAgent alloc] init];
  [profile_state_ addAgent:agent_];
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  testing::Mock::VerifyAndClearExpectations(promos_manager_.get());

  [profile_state_ removeAgent:agent_];
  scene_state_.activationLevel = SceneActivationLevelBackground;
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->SetPersonalProfileName(personal_profile_name);
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1],
                        [FakeSystemIdentity fakeIdentity2]);
  task_environment_.FastForwardBy(base::Days(28));
  agent_ = [[AccountSwitchingPromoProfileAgent alloc] init];
  [profile_state_ addAgent:agent_];

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(
                                    promos_manager::Promo::AccountSwitching))
      .Times(1);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that once evaluated in a session, attaching a new
// `AccountSwitchingPromoProfileAgent` instance (e.g. after switching to a
// managed profile and back) does not evaluate or register the promo again.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       EvaluatesOnlyOnceAcrossProfileAgentInstancesInSession) {
  base::HistogramTester histogram_tester;
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1],
                        [FakeSystemIdentity fakeIdentity2]);
  task_environment_.FastForwardBy(base::Days(28));

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(
                                    promos_manager::Promo::AccountSwitching))
      .Times(1);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  histogram_tester.ExpectUniqueSample(
      signin::kAccountSwitchingPromoEventsHistogram,
      signin::AccountSwitchingPromoEvent::kEligibleOnStartup, 1);
  testing::Mock::VerifyAndClearExpectations(promos_manager_.get());

  // Re-create the agent on the profile state and re-activate a scene.
  [profile_state_ removeAgent:agent_];
  agent_ = [[AccountSwitchingPromoProfileAgent alloc] init];
  [profile_state_ addAgent:agent_];

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(testing::_))
      .Times(0);
  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  histogram_tester.ExpectTotalCount(
      signin::kAccountSwitchingPromoEventsHistogram, 1);
}

// Tests that `targetIdentity` is the preferred account for switching.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       TargetIdentityIsPreferredAccount) {
  FakeSystemIdentity* target = [FakeSystemIdentity fakeIdentity2];
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1], target);
  EXPECT_EQ(target.gaiaId, agent_.targetIdentity.gaiaId);
}

// Tests that `targetIdentity` is `nil` when signed out.
TEST_F(AccountSwitchingPromoProfileAgentTest, NoTargetIdentityWhenSignedOut) {
  FakeSystemIdentity* target = [FakeSystemIdentity fakeIdentity2];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(target);
  signin::AccountPreviewDataService::AccountPreviewPreference pref{
      .gaia_id = target.gaiaId};
  test_preview_service_->SetPreferredAccountForSwitching(pref);
  EXPECT_FALSE(agent_.targetIdentity);
}

// Tests that `targetIdentity` is `nil` when the primary account is managed.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       NoTargetIdentityWhenPrimaryAccountIsManaged) {
  FakeSystemIdentity* managed = [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentity* target = [FakeSystemIdentity fakeIdentity2];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(managed);
  system_identity_manager->AddIdentity(target);
  GetApplicationContext()
      ->GetAccountProfileMapper()
      ->MoveManagedAccountToPersonalProfileForTesting(managed.gaiaId);
  authentication_service_->SignIn(managed,
                                  signin_metrics::AccessPoint::kStartPage);
  signin::AccountPreviewDataService::AccountPreviewPreference pref{
      .gaia_id = target.gaiaId};
  test_preview_service_->SetPreferredAccountForSwitching(pref);
  SetNotSupervised(managed);
  ASSERT_TRUE(authentication_service_->HasPrimaryIdentityManaged());
  EXPECT_FALSE(agent_.targetIdentity);
}

// Tests that `targetIdentity` is `nil` when the candidate target account is
// managed (even if it is mapped to the personal profile).
TEST_F(AccountSwitchingPromoProfileAgentTest,
       NoTargetIdentityWhenTargetAccountIsManaged) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* managed_target = [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(primary);
  system_identity_manager->AddIdentity(managed_target);
  GetApplicationContext()
      ->GetAccountProfileMapper()
      ->MoveManagedAccountToPersonalProfileForTesting(managed_target.gaiaId);
  authentication_service_->SignIn(primary,
                                  signin_metrics::AccessPoint::kStartPage);
  signin::AccountPreviewDataService::AccountPreviewPreference pref{
      .gaia_id = managed_target.gaiaId};
  test_preview_service_->SetPreferredAccountForSwitching(pref);
  SetNotSupervised(primary);
  ASSERT_EQ(
      identity_manager_->FindExtendedAccountInfoByGaiaId(managed_target.gaiaId)
          .IsManaged(),
      signin::Tribool::kTrue);
  EXPECT_FALSE(agent_.targetIdentity);
}

// Tests that `targetIdentity` is `nil` when the preferred account is the same
// as the primary account.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       NoTargetIdentityWhenPreferredAccountIsPrimaryAccount) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  SetUpEligibleAccounts(primary, primary);
  EXPECT_FALSE(agent_.targetIdentity);
}

// Tests that `targetIdentity` is `nil` when the preferred account does not
// exist on the device.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       NoTargetIdentityWhenPreferredAccountIsNotOnDevice) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(primary);
  authentication_service_->SignIn(primary,
                                  signin_metrics::AccessPoint::kStartPage);
  SetNotSupervised(primary);
  signin::AccountPreviewDataService::AccountPreviewPreference pref{
      .gaia_id = GaiaId("non_existent_gaia_id")};
  test_preview_service_->SetPreferredAccountForSwitching(pref);
  EXPECT_FALSE(agent_.targetIdentity);
}

// Tests that `targetIdentity` is `nil` without a preferred account.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       NoTargetIdentityWithoutPreferredAccount) {
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1],
                        [FakeSystemIdentity fakeIdentity2]);
  test_preview_service_->SetPreferredAccountForSwitching(std::nullopt);
  EXPECT_FALSE(agent_.targetIdentity);
}

// Tests that the promo is not registered within 28 days of the last sign-in,
// nor later in the same session once 28 days have passed.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DoesNotRegisterPromoWithin28DaysOfSignin) {
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1],
                        [FakeSystemIdentity fakeIdentity2]);

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(testing::_))
      .Times(0);
  EXPECT_CALL(*promos_manager_,
              DeregisterPromo(promos_manager::Promo::AccountSwitching));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // The promo is only evaluated at startup.
  scene_state_.activationLevel = SceneActivationLevelBackground;
  task_environment_.FastForwardBy(base::Days(28));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the registered promo is deregistered once the target account is
// removed from the device.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DeregistersPromoWhenTargetAccountIsRemoved) {
  FakeSystemIdentity* target = [FakeSystemIdentity fakeIdentity2];
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1], target);
  task_environment_.FastForwardBy(base::Days(28));
  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(
                                    promos_manager::Promo::AccountSwitching));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  testing::Mock::VerifyAndClearExpectations(promos_manager_);

  EXPECT_CALL(*promos_manager_,
              DeregisterPromo(promos_manager::Promo::AccountSwitching))
      .Times(testing::AtLeast(1));
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->ForgetIdentityFromOtherApplication(target);
}

// Tests that the registered promo is deregistered once its account pair
// becomes suppressed.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DeregistersPromoWhenPairBecomesSuppressed) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* target = [FakeSystemIdentity fakeIdentity2];
  SetUpEligibleAccounts(primary, target);
  task_environment_.FastForwardBy(base::Days(28));
  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(
                                    promos_manager::Promo::AccountSwitching));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  testing::Mock::VerifyAndClearExpectations(promos_manager_);

  EXPECT_CALL(*promos_manager_,
              DeregisterPromo(promos_manager::Promo::AccountSwitching));
  signin::SuppressAccountSwitchingReversePair(*profile_->GetPrefs(),
                                              target.gaiaId, primary.gaiaId);
}

// Tests that the promo is not registered for a suppressed reverse pair, even
// 60 days after the switch.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       SwitchingAccountsPermanentlySuppressesReversePair) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* target = [FakeSystemIdentity fakeIdentity2];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(primary);
  system_identity_manager->AddIdentity(target);

  // Sign into `target` first, then switch to `primary`.
  authentication_service_->SignIn(target,
                                  signin_metrics::AccessPoint::kStartPage);
  authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest);
  authentication_service_->SignIn(primary,
                                  signin_metrics::AccessPoint::kStartPage);
  signin::SuppressAccountSwitchingReversePair(*profile_->GetPrefs(),
                                              target.gaiaId, primary.gaiaId);
  SetNotSupervised(primary);

  signin::AccountPreviewDataService::AccountPreviewPreference pref{
      .gaia_id = target.gaiaId};
  test_preview_service_->SetPreferredAccountForSwitching(pref);

  // Even 60 days later, prompting (`primary` -> `target`) remains suppressed.
  task_environment_.FastForwardBy(base::Days(60));
  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(testing::_))
      .Times(0);
  EXPECT_CALL(*promos_manager_,
              DeregisterPromo(promos_manager::Promo::AccountSwitching))
      .Times(testing::AtLeast(1));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that removing an account from the device deletes its reverse pair
// suppression, so the promo is registered again once it is re-added.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       RemovingAccountDeletesPairStateAllowingFreshStartWhenReadded) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* target = [FakeSystemIdentity fakeIdentity2];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(primary);
  system_identity_manager->AddIdentity(target);

  // Switch `target` -> `primary` so reverse pair (`primary` -> `target`) is
  // suppressed.
  authentication_service_->SignIn(target,
                                  signin_metrics::AccessPoint::kStartPage);
  authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest);
  authentication_service_->SignIn(primary,
                                  signin_metrics::AccessPoint::kStartPage);
  signin::SuppressAccountSwitchingReversePair(*profile_->GetPrefs(),
                                              target.gaiaId, primary.gaiaId);
  SetNotSupervised(primary);
  task_environment_.FastForwardBy(base::Days(60));

  // Remove `target` from device, then re-add it.
  system_identity_manager->ForgetIdentityFromOtherApplication(target);
  system_identity_manager->AddIdentity(target);

  signin::AccountPreviewDataService::AccountPreviewPreference pref{
      .gaia_id = target.gaiaId};
  test_preview_service_->SetPreferredAccountForSwitching(pref);

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(
                                    promos_manager::Promo::AccountSwitching));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the promo is deregistered when there is no switching candidate.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DeregistersPromoWhenNoSwitchingCandidate) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(primary);
  authentication_service_->SignIn(primary,
                                  signin_metrics::AccessPoint::kStartPage);
  SetNotSupervised(primary);
  task_environment_.FastForwardBy(base::Days(28));

  test_preview_service_->SetPreferredAccountForSwitching(std::nullopt);

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(testing::_))
      .Times(0);
  EXPECT_CALL(*promos_manager_,
              DeregisterPromo(promos_manager::Promo::AccountSwitching));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the promo is not registered, and `targetIdentity` is `nil`, when
// the primary account is supervised (`Tribool::kTrue`).
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DoesNotRegisterPromoWhenSupervised) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* target = [FakeSystemIdentity fakeIdentity2];
  SetUpEligibleAccounts(primary, target);
  SetSupervised(primary, /*supervised=*/true);
  task_environment_.FastForwardBy(base::Days(28));

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(testing::_))
      .Times(0);
  EXPECT_CALL(*promos_manager_,
              DeregisterPromo(promos_manager::Promo::AccountSwitching));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_FALSE(agent_.targetIdentity);
}

// Tests that the promo is not registered, and `targetIdentity` is `nil`, when
// the supervision status of the primary account is unknown
// (`Tribool::kUnknown`).
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DoesNotRegisterPromoWhenSupervisionIsUnknown) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* target = [FakeSystemIdentity fakeIdentity2];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentityWithUnknownCapabilities(primary);
  system_identity_manager->AddIdentity(target);
  authentication_service_->SignIn(primary,
                                  signin_metrics::AccessPoint::kStartPage);
  signin::AccountPreviewDataService::AccountPreviewPreference pref{
      .gaia_id = target.gaiaId};
  test_preview_service_->SetPreferredAccountForSwitching(pref);
  task_environment_.FastForwardBy(base::Days(28));

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(testing::_))
      .Times(0);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_FALSE(agent_.targetIdentity);
}

// Tests that the promo is neither registered nor stored once the profile is no
// longer the personal profile.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DoesNothingWhenProfileIsNoLongerPersonal) {
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1],
                        [FakeSystemIdentity fakeIdentity2]);
  ASSERT_TRUE(agent_.targetIdentity);
  task_environment_.FastForwardBy(base::Days(28));
  const std::string other_profile_name =
      profile_manager_.ReserveNewProfileName();
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->SetPersonalProfileName(other_profile_name);

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(testing::_))
      .Times(0);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_FALSE(agent_.targetIdentity);
}

// Tests that the promo is only registered once `ProfileInitStage::kFinal` is
// reached.
TEST_F(AccountSwitchingPromoProfileAgentTest, EvaluatesOnlyAtFinalInitStage) {
  // Set up a profile that has not reached `ProfileInitStage::kFinal` yet, with
  // a connected scene whose UI is enabled.
  ProfileState* profile_state = [[ProfileState alloc] initWithAppState:nil];
  SetProfileStateInitStage(profile_state, ProfileInitStage::kNormalUI);
  profile_state.profile = profile_;
  FakeSceneState* scene_state =
      [[FakeSceneState alloc] initWithProfile:profile_];
  scene_state.profileState = profile_state;
  scene_state.UIEnabled = YES;
  AccountSwitchingPromoProfileAgent* agent =
      [[AccountSwitchingPromoProfileAgent alloc] init];
  [profile_state addAgent:agent];
  [profile_state sceneStateConnected:scene_state];

  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1],
                        [FakeSystemIdentity fakeIdentity2]);
  // Let enough time pass for the promo to be eligible.
  task_environment_.FastForwardBy(base::Days(28));
  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(testing::_))
      .Times(0);
  // Simulate the scene becoming foreground active before
  // `ProfileInitStage::kFinal`.
  scene_state.activationLevel = SceneActivationLevelForegroundActive;
  testing::Mock::VerifyAndClearExpectations(promos_manager_.get());

  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(
                                    promos_manager::Promo::AccountSwitching));
  // Simulate the profile initialization reaching `ProfileInitStage::kFinal`.
  SetProfileStateInitStage(profile_state, ProfileInitStage::kFinal);

  // Simulate the scene being torn down.
  scene_state.UIEnabled = NO;
  [scene_state shutdown];
}

// Tests that the registered promo is deregistered if the personal profile
// becomes managed mid-session.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DeregistersWhenProfileBecomesManaged) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  SetUpEligibleAccounts(primary, [FakeSystemIdentity fakeIdentity2]);
  task_environment_.FastForwardBy(base::Days(28));
  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(
                                    promos_manager::Promo::AccountSwitching));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  testing::Mock::VerifyAndClearExpectations(promos_manager_.get());

  EXPECT_CALL(*promos_manager_,
              DeregisterPromo(promos_manager::Promo::AccountSwitching));

  // Simulate the profile becoming managed.
  const std::string other_profile_name =
      profile_manager_.ReserveNewProfileName();
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->SetPersonalProfileName(other_profile_name);

  // Trigger an observer update while all other eligibility criteria remain met.
  AccountInfo account =
      identity_manager_->FindExtendedAccountInfoByGaiaId(primary.gaiaId);
  account = AccountInfo::Builder(account).SetFullName("Updated Name").Build();
  signin::UpdateAccountInfoForAccount(identity_manager_, account);
}

// Tests that the registered promo is deregistered when the target account is
// filtered out of the profile by `prefs::kRestrictAccountsToPatterns`.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       DeregistersPromoWhenTargetAccountIsRestrictedByPattern) {
  FakeSystemIdentity* target = [FakeSystemIdentity fakeIdentity2];
  SetUpEligibleAccounts([FakeSystemIdentity fakeIdentity1], target);
  task_environment_.FastForwardBy(base::Days(28));
  EXPECT_CALL(*promos_manager_, RegisterPromoForSingleDisplay(
                                    promos_manager::Promo::AccountSwitching));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  testing::Mock::VerifyAndClearExpectations(promos_manager_.get());

  EXPECT_CALL(*promos_manager_,
              DeregisterPromo(promos_manager::Promo::AccountSwitching));
  GetApplicationContext()->GetLocalState()->SetList(
      prefs::kRestrictAccountsToPatterns,
      base::ListValue().Append("foo1@gmail.com"));
  EXPECT_FALSE(agent_.targetIdentity);
}

// Tests that signing out within 24 hours of accepting the promo records `true`
// for `Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours`.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       RecordsRetentionMetricOnPrimaryAccountChange) {
  base::HistogramTester histogram_tester;
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  SetUpEligibleAccounts(primary, [FakeSystemIdentity fakeIdentity2]);
  // Simulates that `primary` was selected via the promo.
  signin::RecordAccountSwitchingPromoAccepted(*profile_->GetPrefs(),
                                              primary.gaiaId);

  authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest);

  histogram_tester.ExpectUniqueSample(
      "Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours", true, 1);
}

// Tests that staying on the accepted account for 24 hours records `false` for
// `Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours` once startup reaches
// `ProfileInitStage::kFinal`.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       RecordsRetentionMetricOnStartupAfter24Hours) {
  base::HistogramTester histogram_tester;
  // Set up a profile in `ProfileInitStage::kFirstRun` with the agent attached.
  ProfileState* profile_state = [[ProfileState alloc] initWithAppState:nil];
  SetProfileStateInitStage(profile_state, ProfileInitStage::kFirstRun);
  profile_state.profile = profile_;
  AccountSwitchingPromoProfileAgent* agent =
      [[AccountSwitchingPromoProfileAgent alloc] init];
  [profile_state addAgent:agent];

  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  SetUpEligibleAccounts(primary, [FakeSystemIdentity fakeIdentity2]);
  // Simulates that `primary` was selected via the promo.
  signin::RecordAccountSwitchingPromoAccepted(*profile_->GetPrefs(),
                                              primary.gaiaId);

  // Advance past the 24-hour window and progress startup to
  // `ProfileInitStage::kNormalUI`, before the final stage.
  task_environment_.FastForwardBy(base::Hours(25));
  SetProfileStateInitStage(profile_state, ProfileInitStage::kNormalUI);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours", 0);

  // Simulate the profile initialization reaching `ProfileInitStage::kFinal`.
  SetProfileStateInitStage(profile_state, ProfileInitStage::kFinal);

  histogram_tester.ExpectUniqueSample(
      "Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours", false, 1);
}

// Tests that staying on the accepted account for 24 hours records `false` for
// `Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours` on scene activation.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       RecordsRetentionMetricOnSceneActivationAfter24Hours) {
  base::HistogramTester histogram_tester;
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  SetUpEligibleAccounts(primary, [FakeSystemIdentity fakeIdentity2]);
  // Simulates that `primary` was selected via the promo.
  signin::RecordAccountSwitchingPromoAccepted(*profile_->GetPrefs(),
                                              primary.gaiaId);

  task_environment_.FastForwardBy(base::Hours(25));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  histogram_tester.ExpectUniqueSample(
      "Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours", false, 1);
}

// Tests that removing the accepted primary account from the device within 24
// hours prunes its state without recording
// `Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours`, both during an
// active session (including async sign-out) and on scene activation when the
// account was removed while Chrome was not running.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       RemovingAcceptedAccountDoesNotRecordRetentionMetric) {
  base::HistogramTester histogram_tester;
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  SetUpEligibleAccounts(primary, [FakeSystemIdentity fakeIdentity2]);
  // Simulates that `primary` was selected via the promo.
  signin::RecordAccountSwitchingPromoAccepted(*profile_->GetPrefs(),
                                              primary.gaiaId);

  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->ForgetIdentityFromOtherApplication(primary);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !authentication_service_->HasPrimaryIdentity(); }));
  histogram_tester.ExpectTotalCount(
      "Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours", 0);

  // Also simulate `primary` having been removed while Chrome was not running,
  // before `scene_state_` becomes foreground-active.
  signin::RecordAccountSwitchingPromoAccepted(*profile_->GetPrefs(),
                                              primary.gaiaId);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  histogram_tester.ExpectTotalCount(
      "Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours", 0);
  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(signin::kAccountSwitchingPromoStatePref)
                  .empty());
}

// Tests that `kAccountSwitchingPromoStatePref` is cleared when the profile is
// no longer the personal profile.
TEST_F(AccountSwitchingPromoProfileAgentTest,
       ClearsPromoStateWhenProfileIsNoLongerPersonal) {
  FakeSystemIdentity* primary = [FakeSystemIdentity fakeIdentity1];
  SetUpEligibleAccounts(primary, [FakeSystemIdentity fakeIdentity2]);
  signin::RecordAccountSwitchingPromoAccepted(*profile_->GetPrefs(),
                                              primary.gaiaId);
  ASSERT_FALSE(profile_->GetPrefs()
                   ->GetDict(signin::kAccountSwitchingPromoStatePref)
                   .empty());

  const std::string other_profile_name =
      profile_manager_.ReserveNewProfileName();
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->SetPersonalProfileName(other_profile_name);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(signin::kAccountSwitchingPromoStatePref)
                  .empty());
}

using AccountSwitchingPromoProfileAgentNoServiceTest = PlatformTest;

// Tests that the agent has no `targetIdentity` when `AccountPreviewDataService`
// is unavailable for the profile.
TEST_F(AccountSwitchingPromoProfileAgentNoServiceTest,
       NoTargetIdentityWhenAccountPreviewDataServiceUnavailable) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileManagerIOS profile_manager;
  TestProfileIOS* profile =
      profile_manager.AddProfileWithBuilder(TestProfileIOS::Builder());
  ASSERT_FALSE(AccountPreviewDataServiceFactory::GetForProfile(profile));
  ProfileState* profile_state = [[ProfileState alloc] initWithAppState:nil];
  SetProfileStateInitStage(profile_state, ProfileInitStage::kFinal);
  profile_state.profile = profile;
  AccountSwitchingPromoProfileAgent* agent =
      [[AccountSwitchingPromoProfileAgent alloc] init];
  [profile_state addAgent:agent];

  EXPECT_FALSE(agent.targetIdentity);
  [profile_state removeAgent:agent];
}
