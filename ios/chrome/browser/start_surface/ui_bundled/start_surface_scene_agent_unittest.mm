// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_scene_agent.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using tab_groups::TabGroupId;

namespace {
const char kURL[] = "https://chromium.org/";
}

class StartSurfaceSceneAgentTest : public PlatformTest {
 public:
  StartSurfaceSceneAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    startup_information_ = [[FakeStartupInformation alloc] init];
    app_state_ = OCMClassMock([AppState class]);
    OCMStub([app_state_ startupInformation]).andReturn(startup_information_);

    scene_state_ = [[FakeSceneState alloc] initWithAppState:app_state_
                                                    profile:profile_.get()];
    agent_ = [[StartSurfaceSceneAgent alloc] init];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);
    InstallMockProfileState(ProfileInitStage::kStart);
    agent_.sceneState = scene_state_;
    Browser* browser =
        scene_state_.browserProviderInterface.mainBrowserProvider.browser;
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser);
    TabInsertionBrowserAgent::CreateForBrowser(browser);
  }

  void TearDown() override {
    // Close all WebState to make sure no Objective-C object reference the
    // ProfileIOS after its destruction (as the SceneState destruction may
    // be delayed).
    CloseAllWebStates(*GetWebStateList(), WebStateList::CLOSE_NO_FLAGS);

    // Drop the references to the Objective-C objects. This is a best-effort
    // try to have them being deallocated during TearDown().
    startup_information_ = nil;
    app_state_ = nil;
    profile_state_ = nil;
    scene_state_ = nil;
    agent_ = nil;

    PlatformTest::TearDown();
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  // Install a mock ProfileState with a fixed `init_stage`.
  void InstallMockProfileState(ProfileInitStage init_stage) {
    profile_state_ = OCMClassMock([ProfileState class]);
    OCMStub([profile_state_ initStage]).andReturn(init_stage);
    OCMStub([profile_state_ appState]).andReturn(app_state_);
    scene_state_.profileState = profile_state_;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeStartupInformation* startup_information_;
  AppState* app_state_;
  ProfileState* profile_state_;
  // The scene state that the agent works with.
  FakeSceneState* scene_state_;
  // The tested agent
  StartSurfaceSceneAgent* agent_;
  ScopedKeyWindow scoped_window_;
  base::HistogramTester histogram_tester_;

  // Returns the WebStateList for the SceneState.
  WebStateList* GetWebStateList() {
    return scene_state_.browserProviderInterface.mainBrowserProvider.browser
        ->GetWebStateList();
  }

  // Create WebState at `index` with `url` as the current url.
  void InsertNewWebState(int index, GURL url) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetCurrentURL(url);
    test_web_state->SetNavigationItemCount(1);
    test_web_state->SetBrowserState(profile_.get());
    NewTabPageTabHelper::CreateForWebState(test_web_state.get());
    GetWebStateList()->InsertWebState(
        std::move(test_web_state),
        WebStateList::InsertionParams::AtIndex(index));
  }

  // Create a WebState that has a navigation history of more than one at `index`
  // with `url` as the current url.
  void InsertNewWebStateWithNavigationHistory(int index, GURL url) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetCurrentURL(url);
    test_web_state->SetNavigationItemCount(2);
    GetWebStateList()->InsertWebState(
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
  histogram_tester_.ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 0);

  // Transition to the background, triggering the NTP clean up.
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];

  // Expect 2 calls to IOS.NTP.ExcessRemovedTabCount. One for the regular
  // browser, one for the incognito browser.
  histogram_tester_.ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 2);
  // Regular browser got 2 NTP removed.
  histogram_tester_.ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 2, 1);
  // Incognito browser got no NTP removed.
  histogram_tester_.ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 0, 1);
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
  histogram_tester_.ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 0);

  // Transition to the background, triggering the NTP clean up.
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];

  // Expect 2 calls to IOS.NTP.ExcessRemovedTabCount. One for the regular
  // browser, one for the incognito browser.
  histogram_tester_.ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 2);
  // Regular browser got 2 NTP removed.
  histogram_tester_.ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 2, 1);
  // Incognito browser got no NTP removed.
  histogram_tester_.ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 0, 1);
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
  histogram_tester_.ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 0);

  // Transition to the background, triggering the NTP clean up.
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];

  // Expect 2 calls to IOS.NTP.ExcessRemovedTabCount. One for the regular
  // browser, one for the incognito browser.
  histogram_tester_.ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 2);
  // Regular browser got 1 NTP removed.
  histogram_tester_.ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 1, 1);
  // Incognito browser got no NTP removed.
  histogram_tester_.ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 0, 1);
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
  const TabGroup* group_0 =
      web_state_list->CreateGroup({0, 1, 2, 3}, {}, TabGroupId::GenerateNew());
  const TabGroup* group_1 =
      web_state_list->CreateGroup({6}, {}, TabGroupId::GenerateNew());
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  histogram_tester_.ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 0);

  // Transition to the background, triggering the NTP clean up.
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];

  // Expect 2 calls to IOS.NTP.ExcessRemovedTabCount. One for the regular
  // browser, one for the incognito browser.
  histogram_tester_.ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 2);
  // Regular browser got 3 NTP removed.
  histogram_tester_.ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 3, 1);
  // Incognito browser got no NTP removed.
  histogram_tester_.ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 0, 1);
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
  const TabGroup* group_0 =
      web_state_list->CreateGroup({0, 1}, {}, TabGroupId::GenerateNew());
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  histogram_tester_.ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 0);

  // Transition to the background, triggering the NTP clean up.
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];

  // Expect 2 calls to IOS.NTP.ExcessRemovedTabCount. One for the regular
  // browser, one for the incognito browser.
  histogram_tester_.ExpectTotalCount("IOS.NTP.ExcessRemovedTabCount", 2);
  // Regular browser got 1 NTP removed.
  histogram_tester_.ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 1, 1);
  // Incognito browser got no NTP removed.
  histogram_tester_.ExpectBucketCount("IOS.NTP.ExcessRemovedTabCount", 0, 1);
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

  InstallMockProfileState(ProfileInitStage::kFinal);

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

  histogram_tester_.ExpectTotalCount("IOS.BackgroundTimeBeforeWarmStart", 0);
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  histogram_tester_.ExpectTotalCount("IOS.BackgroundTimeBeforeWarmStart", 1);
}

// Tests that IOS.StartSurfaceShown is correctly logged for a valid cold start
// open.
TEST_F(StartSurfaceSceneAgentTest, LogCorrectColdStartHistogram) {
  std::map<std::string, std::string> parameters;
  parameters[kReturnToStartSurfaceInactiveDurationInSeconds] = "0";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kStartSurface,
                                                         parameters);

  InstallMockProfileState(ProfileInitStage::kFinal);
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

  histogram_tester_.ExpectTotalCount("IOS.BackgroundTimeBeforeColdStart", 0);
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  histogram_tester_.ExpectTotalCount("IOS.BackgroundTimeBeforeColdStart", 1);
}

TEST_F(StartSurfaceSceneAgentTest, PrefetchCapabilitiesOnAppStart) {
  // Set up fake identity with account capabilities.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(identity);

  AccountCapabilitiesTestMutator* mutator =
      fake_system_identity_manager()->GetPendingCapabilitiesMutator(identity);
  mutator->SetAllSupportedCapabilities(true);

  // Set up expected app state that prefetches capabilities.
  InstallMockProfileState(ProfileInitStage::kFinal);
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

  ASSERT_FALSE(fake_system_identity_manager()
                   ->GetVisibleCapabilities(identity)
                   .AreAllCapabilitiesKnown());

  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_system_identity_manager()
                  ->GetVisibleCapabilities(identity)
                  .AreAllCapabilitiesKnown());
}
