// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_scene_agent.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
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
  base::test::TaskEnvironment task_environment_;
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
  void InsertNewWebState(int index, WebStateOpener opener, GURL url) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetCurrentURL(url);
    test_web_state->SetNavigationItemCount(1);
    test_web_state->SetBrowserState(browser_state_.get());
    Browser* browser =
        scene_state_.browserProviderInterface.mainBrowserProvider.browser;
    WebStateList* web_state_list = browser->GetWebStateList();
    NewTabPageTabHelper::CreateForWebState(test_web_state.get());
    web_state_list->InsertWebState(index, std::move(test_web_state),
                                   WebStateList::INSERT_FORCE_INDEX, opener);
  }

  // Create a WebState that has a navigation history of more than one at `index`
  // with `url` as the current url.
  void InsertNewWebStateWithNavigationHistory(int index,
                                              WebStateOpener opener,
                                              GURL url) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetCurrentURL(url);
    test_web_state->SetNavigationItemCount(2);
    Browser* browser =
        scene_state_.browserProviderInterface.mainBrowserProvider.browser;
    WebStateList* web_state_list = browser->GetWebStateList();
    web_state_list->InsertWebState(index, std::move(test_web_state),
                                   WebStateList::INSERT_FORCE_INDEX, opener);
  }
};

// Tests that all but one of the NTP tabs are removed after the app goes from a
// foreground to a background state.
TEST_F(StartSurfaceSceneAgentTest, RemoveExcessNTP) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.push_back(kRemoveExcessNTPs);

  scoped_feature_list.InitWithFeatures(enabled_features, {});

  InsertNewWebState(0, WebStateOpener(), GURL(kChromeUINewTabURL));
  InsertNewWebState(1, WebStateOpener(), GURL(kURL));
  InsertNewWebState(2, WebStateOpener(), GURL(kChromeUINewTabURL));
  InsertNewWebState(3, WebStateOpener(), GURL(kChromeUINewTabURL));

  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];
  WebStateList* web_state_list =
      scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  ASSERT_EQ(2, web_state_list->count());
  // NTP at index 3 should be the one saved, so the remaining WebState with an
  // NTP should be at index 1.
  EXPECT_TRUE(IsUrlNtp(web_state_list->GetWebStateAt(1)->GetVisibleURL()));
}

// Tests that only the NTP tab with navigation history is the only NTP tab that
// is kept after moving to the background state.
TEST_F(StartSurfaceSceneAgentTest, OnlyRemoveEmptyNTPTabs) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.push_back(kRemoveExcessNTPs);

  scoped_feature_list.InitWithFeatures(enabled_features, {});

  InsertNewWebState(0, WebStateOpener(), GURL(kChromeUINewTabURL));
  InsertNewWebState(1, WebStateOpener(), GURL(kURL));
  InsertNewWebStateWithNavigationHistory(2, WebStateOpener(),
                                         GURL(kChromeUINewTabURL));
  InsertNewWebState(3, WebStateOpener(), GURL(kChromeUINewTabURL));

  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];
  WebStateList* web_state_list =
      scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  ASSERT_EQ(2, web_state_list->count());
  EXPECT_TRUE(IsUrlNtp(web_state_list->GetWebStateAt(1)->GetVisibleURL()));
}

// Tests that, starting with an active WebState with no navigation history and a
// WebState with navigation history showing the NTP, the latter WebState becomes
// the active WebState after a background.
TEST_F(StartSurfaceSceneAgentTest, KeepNTPAsActiveTab) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.push_back(kRemoveExcessNTPs);

  scoped_feature_list.InitWithFeatures(enabled_features, {});
  InsertNewWebStateWithNavigationHistory(0, WebStateOpener(),
                                         GURL(kChromeUINewTabURL));
  InsertNewWebState(1, WebStateOpener(), GURL(kURL));
  InsertNewWebState(2, WebStateOpener(), GURL(kChromeUINewTabURL));
  WebStateList* web_state_list =
      scene_state_.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  web_state_list->ActivateWebStateAt(2);
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];
  ASSERT_EQ(2, web_state_list->count());
  EXPECT_TRUE(IsUrlNtp(web_state_list->GetWebStateAt(0)->GetVisibleURL()));
  EXPECT_EQ(web_state_list->GetActiveWebState(),
            web_state_list->GetWebStateAt(0));
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

  InsertNewWebState(0, WebStateOpener(), GURL(kURL));
  InsertNewWebState(1, WebStateOpener(), GURL(kChromeUINewTabURL));
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

  InsertNewWebState(0, WebStateOpener(), GURL(kURL));
  InsertNewWebState(1, WebStateOpener(), GURL(kChromeUINewTabURL));
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
