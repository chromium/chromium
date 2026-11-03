// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_switching_promo/model/account_switching_promo_scene_agent.h"

#import <memory>
#import <string>
#import <utility>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/signin/core/browser/test_account_preview_data_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/account_preview_data_service_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Factory returning a test `AccountPreviewDataService`.
std::unique_ptr<KeyedService> BuildTestAccountPreviewDataService(
    ProfileIOS* profile) {
  return std::make_unique<signin::TestAccountPreviewDataService>();
}

// Factory returning a mock `feature_engagement::Tracker`.
std::unique_ptr<KeyedService> BuildMockTracker(ProfileIOS* profile) {
  return std::make_unique<
      testing::NiceMock<feature_engagement::test::MockTracker>>();
}

}  // namespace

class AccountSwitchingPromoSceneAgentTest : public PlatformTest {
 public:
  // Adds `identity` to the device and signs in with it.
  void SignIn(FakeSystemIdentity* identity) {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    authentication_service_->SignIn(identity,
                                    signin_metrics::AccessPoint::kStartPage);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        switches::kEnableAccountPreviewSwitchingAccount);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        AccountPreviewDataServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestAccountPreviewDataService));
    builder.AddTestingFactory(feature_engagement::TrackerFactory::GetInstance(),
                              base::BindRepeating(&BuildMockTracker));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);

    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_);
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_));

    scene_state_.UIEnabled = YES;

    agent_ = [AccountSwitchingPromoSceneAgent agentWithProfile:profile_];
    ASSERT_NE(agent_, nil);
    agent_.sceneState = scene_state_;
  }

  void TearDown() override {
    scene_state_.UIEnabled = NO;
    [scene_state_ shutdown];
    agent_ = nil;
    scene_state_ = nil;
    authentication_service_ = nullptr;
    mock_tracker_ = nullptr;
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  FakeSceneState* scene_state_;
  raw_ptr<AuthenticationService> authentication_service_;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_;
  AccountSwitchingPromoSceneAgent* agent_;
};

// Tests that signing in notifies the Feature Engagement Tracker
// (`feature_engagement::Tracker`).
TEST_F(AccountSwitchingPromoSceneAgentTest,
       NotifiesFeatureEngagementOnPrimarySignin) {
  EXPECT_CALL(*mock_tracker_,
              NotifyEvent(feature_engagement::events::
                              kIOSAccountSwitchingPrimaryAccountChanged))
      .Times(1);
  SignIn([FakeSystemIdentity fakeIdentity1]);
}

// Tests that signing out does not notify `feature_engagement::Tracker`.
TEST_F(AccountSwitchingPromoSceneAgentTest,
       DoesNotNotifyFeatureEngagementOnSignout) {
  SignIn([FakeSystemIdentity fakeIdentity1]);
  EXPECT_CALL(*mock_tracker_,
              NotifyEvent(feature_engagement::events::
                              kIOSAccountSwitchingPrimaryAccountChanged))
      .Times(0);
  authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest);
}

// Tests that signing in after the scene UI is disabled does not notify
// `feature_engagement::Tracker`.
TEST_F(AccountSwitchingPromoSceneAgentTest,
       DoesNotNotifyFeatureEngagementAfterUIDisabled) {
  scene_state_.UIEnabled = NO;
  EXPECT_CALL(*mock_tracker_,
              NotifyEvent(feature_engagement::events::
                              kIOSAccountSwitchingPrimaryAccountChanged))
      .Times(0);
  SignIn([FakeSystemIdentity fakeIdentity1]);
}

// Tests that `+agentWithProfile:` returns `nil` when
// `kEnableAccountPreviewSwitchingAccount` is disabled.
TEST_F(AccountSwitchingPromoSceneAgentTest, ReturnsNilWhenFeatureDisabled) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(
      switches::kEnableAccountPreviewSwitchingAccount);
  EXPECT_FALSE([AccountSwitchingPromoSceneAgent agentWithProfile:profile_]);
}

// Tests that `+agentWithProfile:` returns `nil` when the profile is not the
// personal profile.
TEST_F(AccountSwitchingPromoSceneAgentTest,
       ReturnsNilWhenProfileIsNotPersonal) {
  const std::string other_profile_name =
      profile_manager_.ReserveNewProfileName();
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->SetPersonalProfileName(other_profile_name);
  EXPECT_FALSE([AccountSwitchingPromoSceneAgent agentWithProfile:profile_]);
}

using AccountSwitchingPromoSceneAgentNoServiceTest = PlatformTest;

// Tests that `+agentWithProfile:` returns `nil` when
// `AccountPreviewDataService` is unavailable for the profile.
TEST_F(AccountSwitchingPromoSceneAgentNoServiceTest,
       ReturnsNilWhenAccountPreviewDataServiceUnavailable) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      switches::kEnableAccountPreviewSwitchingAccount);
  TestProfileManagerIOS profile_manager;
  TestProfileIOS* profile =
      profile_manager.AddProfileWithBuilder(TestProfileIOS::Builder());
  ASSERT_FALSE(AccountPreviewDataServiceFactory::GetForProfile(profile));
  EXPECT_FALSE([AccountSwitchingPromoSceneAgent agentWithProfile:profile]);
}
