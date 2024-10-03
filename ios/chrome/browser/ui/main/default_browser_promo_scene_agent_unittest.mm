// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/default_browser_promo_scene_agent.h"

#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/default_promo/ui_bundled/post_default_abandonment/features.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using testing::_;
using testing::AnyNumber;
using testing::Mock;
using testing::NiceMock;

namespace {

// Factory returning a mock feature engagement tracker.
std::unique_ptr<KeyedService> BuildMockFeatureEngagementTracker(
    web::BrowserState* context) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

// Factory returning a mock PromosManager.
std::unique_ptr<KeyedService> BuildMockPromosManager(
    web::BrowserState* context) {
  return std::make_unique<NiceMock<MockPromosManager>>();
}

}  // namespace

class DefaultBrowserPromoSceneAgentTest : public PlatformTest {
 public:
  DefaultBrowserPromoSceneAgentTest() : PlatformTest() {}

 protected:
  void SetUp() override {
    ClearDefaultBrowserPromoData();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildMockFeatureEngagementTracker));
    builder.AddTestingFactory(PromosManagerFactory::GetInstance(),
                              base::BindOnce(&BuildMockPromosManager));
    profile_ = std::move(builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

    promos_manager_ = static_cast<NiceMock<MockPromosManager>*>(
        PromosManagerFactory::GetForProfile(profile_.get()));

    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));

    profile_state_ = OCMClassMock([ProfileState class]);
    OCMStub([profile_state_ initStage]).andReturn(ProfileInitStage::kFinal);
    OCMStub([profile_state_ profile]).andReturn(profile_.get());

    scene_state_ = [[FakeSceneState alloc] initWithAppState:nil
                                                    profile:profile_.get()];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);
    scene_state_.profileState = profile_state_;

    agent_ = [[DefaultBrowserPromoSceneAgent alloc] init];
    agent_.sceneState = scene_state_;
    agent_.promosManager = promos_manager_.get();
  }

  void TearDown() override {
    [[NSUserDefaults standardUserDefaults]
        setBool:NO
         forKey:@"SimulatePostDeviceRestore"];
    profile_.reset();
    ClearDefaultBrowserPromoData();
  }

  void SignIn() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForProfile(profile_.get())
        ->SignIn(identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  void SimulatePostDeviceRestore() {
    [[NSUserDefaults standardUserDefaults]
        setBool:YES
         forKey:@"SimulatePostDeviceRestore"];
  }

  void VerifyPromoRegistration(std::set<promos_manager::Promo> promos) {
    // Allow other promo registration calls.
    EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
        .Times(AnyNumber());
    // Expect a call to register the given promo.
    for (auto promo : promos) {
      EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(promo))
          .Times(1);
    }
    // Allow other promo deregistration calls.
    EXPECT_CALL(*promos_manager_.get(), DeregisterPromo(_)).Times(AnyNumber());
    // Expect no call to deregister the given promo.
    for (auto promo : promos) {
      EXPECT_CALL(*promos_manager_.get(), DeregisterPromo(promo)).Times(0);
    }
  }

  void VerifyPromoDeregistration(std::set<promos_manager::Promo> promos) {
    // Allow other promo registration calls.
    EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
        .Times(AnyNumber());
    // Expect no call to register the given promo.
    for (auto promo : promos) {
      EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(promo))
          .Times(0);
    }
    // Allow other promo deregistration calls.
    EXPECT_CALL(*promos_manager_.get(), DeregisterPromo(_)).Times(AnyNumber());
    // Expect a call to deregister the given promo.
    for (auto promo : promos) {
      EXPECT_CALL(*promos_manager_.get(), DeregisterPromo(promo)).Times(1);
    }
  }

  void VerifyAllDeregistration() {
    // No registration calls should happen for any promo.
    EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
        .Times(0);

    // All promos should be deregistered.
    EXPECT_CALL(
        *promos_manager_.get(),
        DeregisterPromo(promos_manager::Promo::PostRestoreDefaultBrowserAlert))
        .Times(1);
    EXPECT_CALL(*promos_manager_.get(),
                DeregisterPromo(promos_manager::Promo::PostDefaultAbandonment))
        .Times(1);
    EXPECT_CALL(*promos_manager_.get(),
                DeregisterPromo(promos_manager::Promo::AllTabsDefaultBrowser))
        .Times(1);
    EXPECT_CALL(
        *promos_manager_.get(),
        DeregisterPromo(promos_manager::Promo::MadeForIOSDefaultBrowser))
        .Times(1);
    EXPECT_CALL(*promos_manager_.get(),
                DeregisterPromo(promos_manager::Promo::StaySafeDefaultBrowser))
        .Times(1);
    EXPECT_CALL(*promos_manager_.get(),
                DeregisterPromo(promos_manager::Promo::DefaultBrowser))
        .Times(1);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<MockPromosManager> promos_manager_;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_;
  ProfileState* profile_state_;
  FakeSceneState* scene_state_;
  DefaultBrowserPromoSceneAgent* agent_;
};

// Tests that DefaultBrowser was registered with the promo manager when user is
// likely not a default browser user.
TEST_F(DefaultBrowserPromoSceneAgentTest,
       TestPromoRegistrationLikelyInterestedDefault) {
  // Verify registration for default browser promo.
  VerifyPromoRegistration({promos_manager::Promo::DefaultBrowser});

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that no promo was registered to the promo manager when Chrome is likley
// default browser.
TEST_F(DefaultBrowserPromoSceneAgentTest,
       TestChromeLikelyDefaultBrowserNoPromoRegistration) {
  LogOpenHTTPURLFromExternalURL();

  // All promos should be deregistered.
  VerifyAllDeregistration();

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the Post Restore Default Browser Promo is not registered when the
// user is not in a post restore state.
TEST_F(DefaultBrowserPromoSceneAgentTest,
       TestPromoRegistrationPostRestore_UserNotInPostRestoreState) {
  LogOpenHTTPURLFromExternalURL();

  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);

  VerifyPromoDeregistration(
      {promos_manager::Promo::PostRestoreDefaultBrowserAlert});

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the Post Restore Default Browser Promo is not registered when
// Chrome was not set as the user's default browser before the iOS restore.
TEST_F(DefaultBrowserPromoSceneAgentTest,
       TestPromoRegistrationPostRestore_ChromeNotSetDefaultBrowser) {
  SimulatePostDeviceRestore();
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);

  VerifyPromoDeregistration(
      {promos_manager::Promo::PostRestoreDefaultBrowserAlert});

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the Post Restore Default Browser Promo is registered when the
// conditions are met.
TEST_F(DefaultBrowserPromoSceneAgentTest, TestPromoRegistrationPostRestore) {
  SimulatePostDeviceRestore();
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  LogOpenHTTPURLFromExternalURL();

  VerifyPromoRegistration(
      {promos_manager::Promo::PostRestoreDefaultBrowserAlert});

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that Made for iOS and Stay Safe default browser promos are registered
// with the promo manager when Chrome is likely not the default browser.
TEST_F(DefaultBrowserPromoSceneAgentTest, TestTailoredPromoRegistration) {
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);

  // Expect a call to register the given promos.
  VerifyPromoRegistration({promos_manager::Promo::MadeForIOSDefaultBrowser,
                           promos_manager::Promo::StaySafeDefaultBrowser});

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that all individual tailored default browser promos are registered with
// the promo manager when Chrome is likely not the default browser and user is
// signed in.
TEST_F(DefaultBrowserPromoSceneAgentTest, TestTailoredPromoRegistrationSignIn) {
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  SignIn();

  // Expect a call to register the All Tabs promo
  VerifyPromoRegistration({promos_manager::Promo::AllTabsDefaultBrowser,
                           promos_manager::Promo::MadeForIOSDefaultBrowser,
                           promos_manager::Promo::StaySafeDefaultBrowser});

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

TEST_F(DefaultBrowserPromoSceneAgentTest,
       TestPostDefaultAbandonmentPromoDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kPostDefaultAbandonmentPromo);

  // Verify that the promo is never registered regardless of the eligibility
  // interval and the last external URL open time if the feature is disabled.
  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  scene_state_.activationLevel = SceneActivationLevelBackground;

  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* two_days_ago =
      (base::Time::Now() - base::Days(2) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, two_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* under_seven_days_ago =
      (base::Time::Now() - base::Days(7) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_seven_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* over_seven_days_ago =
      (base::Time::Now() - base::Days(7) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_seven_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* twelve_days_ago =
      (base::Time::Now() - base::Days(12) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, twelve_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* fifty_days_ago =
      (base::Time::Now() - base::Days(50) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, fifty_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

TEST_F(DefaultBrowserPromoSceneAgentTest,
       TestPostDefaultAbandonmentPromoDefaultParamValues) {
  scoped_feature_list_.InitAndEnableFeature(kPostDefaultAbandonmentPromo);

  // Verify scenario where Chrome was never likely default browser. Promo should
  // not be registered.
  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  Mock::VerifyAndClearExpectations(promos_manager_.get());
  scene_state_.activationLevel = SceneActivationLevelBackground;

  // Verify scenarios where Chrome is still likely default browser. Promo should
  // not be registered.
  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* two_days_ago =
      (base::Time::Now() - base::Days(2) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, two_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* under_seven_days_ago =
      (base::Time::Now() - base::Days(7) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_seven_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  // Verify scenarios where Chrome was likely default browser, but no longer is.
  // Promo should be registered.
  VerifyPromoRegistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* over_seven_days_ago =
      (base::Time::Now() - base::Days(7) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_seven_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  VerifyPromoRegistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* twelve_days_ago =
      (base::Time::Now() - base::Days(12) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, twelve_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  // Verify scenario where Chrome was likely default browser, but only a long
  // time ago. Promo should not be registered.
  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* fifty_days_ago =
      (base::Time::Now() - base::Days(50) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, fifty_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

TEST_F(DefaultBrowserPromoSceneAgentTest,
       TestPostDefaultAbandonmentPromoCustomParamValues) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kPostDefaultAbandonmentPromo,
      {
          {kPostDefaultAbandonmentIntervalStart.name, "35"},
          {kPostDefaultAbandonmentIntervalEnd.name, "14"},
      });

  // Verify scenario where Chrome was never likely default browser. Promo should
  // not be registered.
  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  scene_state_.activationLevel = SceneActivationLevelBackground;

  // Verify scenarios where Chrome is still likely default browser. Promo should
  // not be registered.
  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* two_days_ago =
      (base::Time::Now() - base::Days(2) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, two_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* under_fourteen_days_ago =
      (base::Time::Now() - base::Days(14) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_fourteen_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  // Verify scenarios where Chrome was likely default browser, but no longer is.
  // Promo should be registered.
  VerifyPromoRegistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* over_fourteen_days_ago =
      (base::Time::Now() - base::Days(14) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_fourteen_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  VerifyPromoRegistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* twenty_five_days_ago =
      (base::Time::Now() - base::Days(25) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, twenty_five_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(promos_manager_.get());
  ClearDefaultBrowserPromoData();
  scene_state_.activationLevel = SceneActivationLevelBackground;

  // Verify scenario where Chrome was likely default browser, but only a long
  // time ago. Promo should not be registered.
  VerifyPromoDeregistration({promos_manager::Promo::PostDefaultAbandonment});
  NSDate* fifty_days_ago =
      (base::Time::Now() - base::Days(50) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, fifty_days_ago);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

TEST_F(DefaultBrowserPromoSceneAgentTest, TestTriggerCriteriaExperiment) {
  scoped_feature_list_.InitAndEnableFeature(
      feature_engagement::kDefaultBrowserTriggerCriteriaExperiment);

  // FET shouldn't be notified if the experiment has just been started.
  EXPECT_CALL(*mock_tracker_,
              NotifyEvent(feature_engagement::events::
                              kDefaultBrowserPromoTriggerCriteriaConditionsMet))
      .Times(0);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(mock_tracker_);
  scene_state_.activationLevel = SceneActivationLevelBackground;

  // FET should be notified because it has been 21 days since the experiment
  // started.
  NSDate* over_twenty_one_days_ago =
      (base::Time::Now() - base::Days(21) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kTimestampTriggerCriteriaExperimentStarted,
                             over_twenty_one_days_ago);
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(feature_engagement::events::
                      kDefaultBrowserPromoTriggerCriteriaConditionsMet));
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  Mock::VerifyAndClearExpectations(mock_tracker_);
  scene_state_.activationLevel = SceneActivationLevelBackground;
}
