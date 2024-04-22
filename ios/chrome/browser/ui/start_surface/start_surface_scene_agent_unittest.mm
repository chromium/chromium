// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_scene_agent.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {
const char kURL[] = "https://chromium.org/";
}

// A fake that allows setting initStage.
@interface FakeAppStateInitStage : AppState
// Init stage that will be returned by the initStage getter when testing.
@property(nonatomic, assign) InitStage initStageForTesting;
@end

@implementation FakeAppStateInitStage

- (InitStage)initStage {
  return self.initStageForTesting;
}

@end

class StartSurfaceSceneAgentTest : public PlatformTest {
 public:
  StartSurfaceSceneAgentTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        startup_information_([[FakeStartupInformation alloc] init]),
        app_state_([[FakeAppStateInitStage alloc]
            initWithStartupInformation:startup_information_]),
        scene_state_([[FakeSceneState alloc]
            initWithAppState:app_state_
                browserState:browser_state_.get()]),
        agent_([[StartSurfaceSceneAgent alloc] init]) {
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness,
        -1);
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);
    agent_.sceneState = scene_state_;
    Browser* browser =
        scene_state_.browserProviderInterface.mainBrowserProvider.browser;
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser);
    TabInsertionBrowserAgent::CreateForBrowser(browser);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    TestingApplicationContext::GetGlobal()->SetLocalState(&pref_service_);
  }

  void TearDown() override {
    agent_ = nil;
    scene_state_ = nil;
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeStartupInformation* startup_information_;
  FakeAppStateInitStage* app_state_;
  // The scene state that the agent works with.
  FakeSceneState* scene_state_;
  ScopedKeyWindow scoped_window_;
  // The tested agent
  StartSurfaceSceneAgent* agent_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  // Create WebState at `index` with `url` as the current url.
  void InsertNewWebState(int index, GURL url) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetCurrentURL(url);
    test_web_state->SetNavigationItemCount(1);
    test_web_state->SetBrowserState(browser_state_.get());
    Browser* browser =
        scene_state_.browserProviderInterface.mainBrowserProvider.browser;
    WebStateList* web_state_list = browser->GetWebStateList();
    NewTabPageTabHelper::CreateForWebState(test_web_state.get());
    web_state_list->InsertWebState(
        std::move(test_web_state),
        WebStateList::InsertionParams::AtIndex(index));
  }

  // Create a WebState that has a navigation history of more than one at `index`
  // with `url` as the current url.
  void InsertNewWebStateWithNavigationHistory(int index,
                                              GURL url) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetCurrentURL(url);
    test_web_state->SetNavigationItemCount(2);
    Browser* browser =
        scene_state_.browserProviderInterface.mainBrowserProvider.browser;
    WebStateList* web_state_list = browser->GetWebStateList();
    web_state_list->InsertWebState(
        std::move(test_web_state),
        WebStateList::InsertionParams::AtIndex(index));
  }
};

// Tests that all but one of the NTP tabs are removed after the app goes from a
// foreground to a background state.
TEST_F(StartSurfaceSceneAgentTest, RemoveExcessNTPs) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.push_back(kRemoveExcessNTPs);

  scoped_feature_list.InitWithFeatures(enabled_features, {});

  InsertNewWebState(0, GURL(kChromeUINewTabURL));
  InsertNewWebState(1, GURL(kChromeUINewTabURL));
  InsertNewWebState(2, GURL(kURL));
  InsertNewWebState(3, GURL(kChromeUINewTabURL));

  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  histogram_tester_->ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 0);

  // Transition to the background, triggering the NTP clean up.
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];

  // Expect 2 calls to IOS.NTP.ExcessRemovedTabCount. One for the regular
  // browser, one for the incognito browser.
  histogram_tester_->ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 2);
  // Regular browser got 2 NTP removed.
  histogram_tester_->ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 2, 1);
  // Incognito browser got no NTP removed.
  histogram_tester_->ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 0, 1);
  WebStateList* web_state_list =
      scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  ASSERT_EQ(2, web_state_list->count());
  // NTP at index 3 should be the one saved, so the remaining WebState with an
  // NTP should now be at index 1.
  EXPECT_EQ(web_state_list->GetWebStateAt(0)->GetVisibleURL(), kURL);
  EXPECT_TRUE(IsUrlNtp(web_state_list->GetWebStateAt(1)->GetVisibleURL()));
}

// Tests that the NTP tab with navigation history is the only NTP tab that is
// kept after moving to the background state.
TEST_F(StartSurfaceSceneAgentTest, OnlyRemoveEmptyNTPs) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.push_back(kRemoveExcessNTPs);

  scoped_feature_list.InitWithFeatures(enabled_features, {});

  InsertNewWebState(0, GURL(kChromeUINewTabURL));
  InsertNewWebState(1, GURL(kURL));
  InsertNewWebStateWithNavigationHistory(2, GURL(kChromeUINewTabURL));
  InsertNewWebState(3, GURL(kChromeUINewTabURL));

  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  histogram_tester_->ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 0);

  // Transition to the background, triggering the NTP clean up.
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];

  // Expect 2 calls to IOS.NTP.ExcessRemovedTabCount. One for the regular
  // browser, one for the incognito browser.
  histogram_tester_->ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 2);
  // Regular browser got 2 NTP removed.
  histogram_tester_->ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 2, 1);
  // Incognito browser got no NTP removed.
  histogram_tester_->ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 0, 1);
  WebStateList* web_state_list =
      scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  ASSERT_EQ(2, web_state_list->count());
  EXPECT_EQ(web_state_list->GetWebStateAt(0)->GetVisibleURL(), kURL);
  EXPECT_TRUE(IsUrlNtp(web_state_list->GetWebStateAt(1)->GetVisibleURL()));
  EXPECT_GT(web_state_list->GetWebStateAt(1)->GetNavigationItemCount(), 1);
}

// Tests that, starting with a WebState with navigation history showing the NTP
// and an active WebState with no navigation history, the former WebState is
// kept and becomes the active WebState after backgrounding.
TEST_F(StartSurfaceSceneAgentTest, KeepAndActivateNonEmptyNTP) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.push_back(kRemoveExcessNTPs);

  scoped_feature_list.InitWithFeatures(enabled_features, {});
  InsertNewWebStateWithNavigationHistory(0, GURL(kChromeUINewTabURL));
  InsertNewWebState(1, GURL(kURL));
  InsertNewWebState(2, GURL(kChromeUINewTabURL));
  WebStateList* web_state_list =
      scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  web_state_list->ActivateWebStateAt(2);
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  histogram_tester_->ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 0);

  // Transition to the background, triggering the NTP clean up.
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];

  // Expect 2 calls to IOS.NTP.ExcessRemovedTabCount. One for the regular
  // browser, one for the incognito browser.
  histogram_tester_->ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 2);
  // Regular browser got 1 NTP removed.
  histogram_tester_->ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 1, 1);
  // Incognito browser got no NTP removed.
  histogram_tester_->ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 0, 1);
  ASSERT_EQ(2, web_state_list->count());
  EXPECT_TRUE(IsUrlNtp(web_state_list->GetWebStateAt(0)->GetVisibleURL()));
  EXPECT_GT(web_state_list->GetWebStateAt(0)->GetNavigationItemCount(), 1);
  EXPECT_EQ(web_state_list->GetActiveWebState(),
            web_state_list->GetWebStateAt(0));
  EXPECT_EQ(web_state_list->GetWebStateAt(1)->GetVisibleURL(), kURL);
}

// Tests that empty NTPs in tab groups are removed, keeping at most one per
// group, and one for the ungrouped tabs.
TEST_F(StartSurfaceSceneAgentTest, KeepAtMostOneEmptyNTPPerGroup) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.push_back(kRemoveExcessNTPs);

  scoped_feature_list.InitWithFeatures(enabled_features, {});
  InsertNewWebStateWithNavigationHistory(0, GURL(kChromeUINewTabURL));
  InsertNewWebState(1, GURL(kURL));
  InsertNewWebState(2, GURL(kChromeUINewTabURL));
  InsertNewWebState(3, GURL(kChromeUINewTabURL));
  InsertNewWebState(4, GURL(kChromeUINewTabURL));
  InsertNewWebStateWithNavigationHistory(5, GURL(kChromeUINewTabURL));
  InsertNewWebState(6, GURL(kChromeUINewTabURL));
  WebStateList* web_state_list =
      scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  const TabGroup* group_0 = web_state_list->CreateGroup({0, 1, 2, 3}, {});
  const TabGroup* group_1 = web_state_list->CreateGroup({6}, {});
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  histogram_tester_->ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 0);

  // Transition to the background, triggering the NTP clean up.
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];

  // Expect 2 calls to IOS.NTP.ExcessRemovedTabCount. One for the regular
  // browser, one for the incognito browser.
  histogram_tester_->ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 2);
  // Regular browser got 3 NTP removed.
  histogram_tester_->ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 3, 1);
  // Incognito browser got no NTP removed.
  histogram_tester_->ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 0, 1);
  ASSERT_EQ(4, web_state_list->count());
  // First is NTP with navigation, in `group_0`.
  EXPECT_TRUE(IsUrlNtp(web_state_list->GetWebStateAt(0)->GetVisibleURL()));
  EXPECT_GT(web_state_list->GetWebStateAt(0)->GetNavigationItemCount(), 1);
  EXPECT_EQ(group_0, web_state_list->GetGroupOfWebStateAt(0));
  // Second is non-NTP, in group_0.
  EXPECT_EQ(web_state_list->GetWebStateAt(1)->GetVisibleURL(), kURL);
  EXPECT_EQ(group_0, web_state_list->GetGroupOfWebStateAt(1));
  // Third is NTP with navigation, in no group.
  EXPECT_TRUE(IsUrlNtp(web_state_list->GetWebStateAt(2)->GetVisibleURL()));
  EXPECT_GT(web_state_list->GetWebStateAt(2)->GetNavigationItemCount(), 1);
  EXPECT_EQ(nullptr, web_state_list->GetGroupOfWebStateAt(2));
  // Fourth is empty NTP (no navigation), in group_1.
  EXPECT_TRUE(IsUrlNtp(web_state_list->GetWebStateAt(3)->GetVisibleURL()));
  EXPECT_LE(web_state_list->GetWebStateAt(3)->GetNavigationItemCount(), 1);
  EXPECT_EQ(group_1, web_state_list->GetGroupOfWebStateAt(3));
}

// Tests that when the active tab is a grouped NTP that gets closed, a new
// ungrouped NTP is opened and activated.
TEST_F(StartSurfaceSceneAgentTest,
       ClosingGroupedActiveNTPSpawnsNewUngroupedNTP) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.push_back(kRemoveExcessNTPs);

  scoped_feature_list.InitWithFeatures(enabled_features, {});
  InsertNewWebState(0, GURL(kChromeUINewTabURL));
  InsertNewWebState(1, GURL(kChromeUINewTabURL));
  WebStateList* web_state_list =
      scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  web_state_list->ActivateWebStateAt(0);
  const TabGroup* group_0 = web_state_list->CreateGroup({0, 1}, {});
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  histogram_tester_->ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 0);

  // Transition to the background, triggering the NTP clean up.
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];

  // Expect 2 calls to IOS.NTP.ExcessRemovedTabCount. One for the regular
  // browser, one for the incognito browser.
  histogram_tester_->ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 2);
  // Regular browser got 1 NTP removed.
  histogram_tester_->ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 1, 1);
  // Incognito browser got no NTP removed.
  histogram_tester_->ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 0, 1);
  ASSERT_EQ(2, web_state_list->count());
  // First is NTP initially at index 1, in `group_0`.
  EXPECT_TRUE(IsUrlNtp(web_state_list->GetWebStateAt(0)->GetVisibleURL()));
  EXPECT_LE(web_state_list->GetWebStateAt(0)->GetNavigationItemCount(), 1);
  EXPECT_EQ(group_0, web_state_list->GetGroupOfWebStateAt(0));
  // Second is a newly-opened empty NTP, in no group. It needs to load first, as
  // it's a real WebState.
  web_state_list->GetWebStateAt(1)->GetNavigationManager()->LoadIfNecessary();
  EXPECT_TRUE(IsUrlNtp(web_state_list->GetWebStateAt(1)->GetVisibleURL()));
  EXPECT_LE(web_state_list->GetWebStateAt(1)->GetNavigationItemCount(), 1);
  EXPECT_EQ(nullptr, web_state_list->GetGroupOfWebStateAt(1));
}

// Tests that IOS.StartSurfaceShown is correctly logged for a valid warm start
// open.
TEST_F(StartSurfaceSceneAgentTest, LogCorrectWarmStartHistogram) {
  std::map<std::string, std::string> parameters;
  parameters[kReturnToStartSurfaceInactiveDurationInSeconds] = "0";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kStartSurface,
                                                         parameters);

  app_state_.initStageForTesting = InitStageFinal;

  InsertNewWebState(0, GURL(kURL));
  InsertNewWebState(1, GURL(kChromeUINewTabURL));
  WebStateList* web_state_list =
      scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  web_state_list->ActivateWebStateAt(0);
  favicon::WebFaviconDriver::CreateForWebState(
      web_state_list->GetActiveWebState(),
      /*favicon_service=*/nullptr);
  SetStartSurfaceSessionObjectForSceneState(scene_state_);

  histogram_tester_->ExpectTotalCount("IOS.BackgroundTimeBeforeWarmStart", 0);
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  histogram_tester_->ExpectTotalCount("IOS.BackgroundTimeBeforeWarmStart", 1);
}

// Tests that IOS.StartSurfaceShown is correctly logged for a valid cold start
// open.
TEST_F(StartSurfaceSceneAgentTest, LogCorrectColdStartHistogram) {
  std::map<std::string, std::string> parameters;
  parameters[kReturnToStartSurfaceInactiveDurationInSeconds] = "0";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kStartSurface,
                                                         parameters);

  app_state_.initStageForTesting = InitStageFinal;
  [startup_information_ setIsColdStart:YES];

  InsertNewWebState(0, GURL(kURL));
  InsertNewWebState(1, GURL(kChromeUINewTabURL));
  WebStateList* web_state_list =
      scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  web_state_list->ActivateWebStateAt(0);
  favicon::WebFaviconDriver::CreateForWebState(
      web_state_list->GetActiveWebState(),
      /*favicon_service=*/nullptr);

  histogram_tester_->ExpectTotalCount("IOS.BackgroundTimeBeforeColdStart", 0);
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  histogram_tester_->ExpectTotalCount("IOS.BackgroundTimeBeforeColdStart", 1);
}
