// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/first_run_profile_agent.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/sync_preferences/features.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/profile/first_run_profile_agent+Testing.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/synced_set_up/public/synced_set_up_metrics.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Tests the FirstRunProfileAgent.
class FirstRunProfileAgentTest : public PlatformTest {
 public:
  explicit FirstRunProfileAgentTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    profile_state_.profile = profile_.get();

    profile_agent_ = [[FirstRunProfileAgent alloc] init];
    [profile_state_ addAgent:profile_agent_];
  }

  ~FirstRunProfileAgentTest() override {
    [scene_state_ shutdown];
    profile_state_.profile = nullptr;
  }

  // Initializes `browser_` using a configured `profile_` and `scene_state_`.
  void InitializeTestBrowser() {
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    scene_state_.browserProviderInterface.currentBrowserProvider.browser =
        browser_.get();
  }

  // Initializes `scene_state_` using a configured `profile_`.
  void InitializeActiveSceneState(bool is_off_the_record = false) {
    StubBrowserProviderInterface* browser_provider_interface =
        [[StubBrowserProviderInterface alloc] init];
    browser_provider_interface.currentBrowserProvider =
        is_off_the_record ? browser_provider_interface.incognitoBrowserProvider
                          : browser_provider_interface.mainBrowserProvider;

    scene_state_ = [[FakeSceneState alloc] initWithAppState:nil
                                                    profile:profile_.get()];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    scene_state_.browserProviderInterface = browser_provider_interface;
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList enabled_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  FirstRunProfileAgent* profile_agent_;
  ProfileState* profile_state_;
  FakeSceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
};

// Validates that the correct metric is logged when the user rejects Guided Tour
// promo.
TEST_F(FirstRunProfileAgentTest, GuidedTourPromoMetrics) {
  enabled_feature_list_.InitAndEnableFeatureWithParameters(
      kBestOfAppFRE, {{kWelcomeBackParam, "4"}});
  base::HistogramTester tester;
  [profile_agent_ dismissGuidedTourPromo];
  tester.ExpectTotalCount("IOS.GuidedTour.Promo.DidAccept", 1);
}

// Validates that the correct metric is logged when a step in the Guided Tour
// finishes.
TEST_F(FirstRunProfileAgentTest, GuidedTourStepMetrics) {
  enabled_feature_list_.InitAndEnableFeatureWithParameters(
      kBestOfAppFRE, {{kWelcomeBackParam, "4"}});
  base::HistogramTester tester;
  [profile_agent_ nextTappedForStep:GuidedTourStep::kTabGridIncognito];
  tester.ExpectBucketCount("IOS.GuidedTour.DidFinishStep", 1, 1);
}

// Validates that the Synced Set Up flow does not trigger from an off-the-record
// profile.
TEST_F(FirstRunProfileAgentTest, SyncedSetUpDoesNotTriggerInIncognito) {
  enabled_feature_list_.InitAndEnableFeature(
      sync_preferences::features::kEnableCrossDevicePrefTracker);

  // Ensure that conditions are met for Synced Set Up to be shown on a
  // non-incognito profile.
  auto pref_service =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  pref_service->registry()->RegisterIntegerPref(
      prefs::kSyncedSetUpImpressionCount, 0);
  ASSERT_TRUE(pref_service->FindPreference(prefs::kSyncedSetUpImpressionCount));
  ASSERT_LT(pref_service->GetInteger(prefs::kSyncedSetUpImpressionCount),
            GetSyncedSetUpImpressionLimit());

  TestProfileIOS::Builder builder;
  builder.SetPrefService(std::move(pref_service));
  profile_ = std::move(builder).Build();

  InitializeActiveSceneState(/*is_off_the_record*/ true);
  ASSERT_NE(scene_state_.browserProviderInterface.currentBrowserProvider, nil);

  InitializeTestBrowser();

  // Configure the app state for the post-first run flow.
  FakeStartupInformation* startup_information =
      [[FakeStartupInformation alloc] init];
  startup_information.isFirstRun = YES;

  AppState* app_state =
      [[AppState alloc] initWithStartupInformation:startup_information];

  [profile_state_ removeAgent:profile_agent_];
  profile_state_ = [profile_state_ initWithAppState:app_state];
  [profile_state_ sceneStateConnected:scene_state_];
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);

  // Assert that whether the profile is off-the-record will be checked by the
  // profile agent.
  ASSERT_TRUE(profile_state_.appState.startupInformation.isFirstRun);
  ASSERT_EQ(profile_state_.initStage, ProfileInitStage::kFinal);
  ASSERT_FALSE(profile_state_.currentUIBlocker);
  ASSERT_TRUE(profile_state_.foregroundActiveScene);

  // Partial mock profile agent that prevents the profile agent from executing
  // other post-first run actions.
  id mock_profile_agent = OCMPartialMock(profile_agent_);
  OCMStub([mock_profile_agent showFirstRunUI]);
  OCMStub([mock_profile_agent performNextPostFirstRunAction]);

  [profile_state_ addAgent:mock_profile_agent];

  // Verify that no Synced Set Up triggers are logged after attempting to show
  // Synced Set Up on an off-the-record profile.
  base::HistogramTester histogram_tester;
  [mock_profile_agent profileState:profile_state_
        firstSceneHasInitializedUI:scene_state_];
  [mock_profile_agent showSyncedSetUp];
  histogram_tester.ExpectBucketCount(
      "IOS.SyncedSetUp.TriggerSource",
      static_cast<int>(SyncedSetUpTriggerSource::kPostFirstRun), 0);
  histogram_tester.ExpectTotalCount("IOS.SyncedSetUp.TriggerSource", 0);
}
