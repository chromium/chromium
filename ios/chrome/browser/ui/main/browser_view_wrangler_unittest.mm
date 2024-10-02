// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"

#import <UIKit/UIKit.h>

#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_observer.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/test_session_restoration_observer.h"
#import "ios/chrome/browser/sessions/model/test_session_restoration_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_util_test_support.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser_list_observer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/main/wrangled_browser.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

class BrowserViewWranglerTest : public PlatformTest {
 protected:
  BrowserViewWranglerTest() {
    fake_scene_ = FakeSceneWithIdentifier([[NSUUID UUID] UUIDString]);
    scene_state_ = [[SceneStateWithFakeScene alloc] initWithScene:fake_scene_
                                                         appState:nil];

    TestProfileIOS::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        SendTabToSelfSyncServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        PrerenderServiceFactory::GetInstance(),
        PrerenderServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::BookmarkModelFactory::GetInstance(),
        ios::BookmarkModelFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        SessionRestorationServiceFactory::GetInstance(),
        TestSessionRestorationService::GetTestingFactory());

    profile_ = std::move(test_cbs_builder).Build();
    profile_->CreateOffTheRecordProfileWithTestingFactories(
        {TestProfileIOS::TestingFactory{
            SessionRestorationServiceFactory::GetInstance(),
            TestSessionRestorationService::GetTestingFactory(),
        }});

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

    scoped_session_restoration_observation_.AddObservation(
        SessionRestorationServiceFactory::GetForProfile(profile_.get()));
    scoped_session_restoration_observation_.AddObservation(
        SessionRestorationServiceFactory::GetForProfile(
            profile_->GetOffTheRecordProfile()));

    scoped_browser_list_observation_.Observe(
        BrowserListFactory::GetForProfile(profile_.get()));
  }

  void RecreateOffTheRecordProfile() {
    scoped_session_restoration_observation_.RemoveObservation(
        SessionRestorationServiceFactory::GetForProfile(
            profile_->GetOffTheRecordProfile()));

    profile_->DestroyOffTheRecordProfile();
    profile_->CreateOffTheRecordProfileWithTestingFactories(
        {TestProfileIOS::TestingFactory{
            SessionRestorationServiceFactory::GetInstance(),
            TestSessionRestorationService::GetTestingFactory(),
        }});

    scoped_session_restoration_observation_.AddObservation(
        SessionRestorationServiceFactory::GetForProfile(
            profile_->GetOffTheRecordProfile()));
  }

  ProfileIOS* profile() { return profile_.get(); }

  SceneState* scene_state() { return scene_state_; }

  TestSessionRestorationObserver& session_restoration_observer() {
    return session_restoration_observer_;
  }

  TestBrowserListObserver& browser_list_observer() {
    return browser_list_observer_;
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  id fake_scene_;
  SceneState* scene_state_;

  // SessionRestorationObserver and its scoped observation.
  TestSessionRestorationObserver session_restoration_observer_;
  base::ScopedMultiSourceObservation<SessionRestorationService,
                                     SessionRestorationObserver>
      scoped_session_restoration_observation_{&session_restoration_observer_};

  // TestBrowserListObserver and its scoped observation.
  TestBrowserListObserver browser_list_observer_;
  base::ScopedObservation<BrowserList, BrowserListObserver>
      scoped_browser_list_observation_{&browser_list_observer_};
};

TEST_F(BrowserViewWranglerTest, TestInitNilObserver) {
  // `task_environment_` must outlive all objects created by BVC, because those
  // objects may rely on threading API in dealloc.
  @autoreleasepool {
    BrowserViewWrangler* wrangler =
        [[BrowserViewWrangler alloc] initWithProfile:profile()
                                          sceneState:scene_state()
                                 applicationEndpoint:nil
                                    settingsEndpoint:nil];
    [wrangler createMainCoordinatorAndInterface];

    // Test that BVC is created on demand.
    UIViewController* bvc = wrangler.mainInterface.viewController;
    EXPECT_NE(bvc, nil);

    // Test that SceneState is associated with the browser.
    SceneState* main_browser_scene_state =
        wrangler.mainInterface.browser->GetSceneState();
    EXPECT_EQ(scene_state(), main_browser_scene_state);

    // Test that once created the BVC isn't re-created.
    EXPECT_EQ(bvc, wrangler.mainInterface.viewController);

    // Test that the OTR objects are (a) OTR and (b) not the same as the non-OTR
    // objects.
    EXPECT_NE(bvc, wrangler.incognitoInterface.viewController);
    EXPECT_TRUE(wrangler.incognitoInterface.profile->IsOffTheRecord());

    // Test that the OTR browser has SceneState associated with it.
    SceneState* otr_browser_scene_state =
        wrangler.incognitoInterface.browser->GetSceneState();
    EXPECT_EQ(scene_state(), otr_browser_scene_state);

    [wrangler shutdown];
  }
}

TEST_F(BrowserViewWranglerTest, TestBrowserList) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {/* Enabled features */},
      {/* Disabled features */ kTabInactivityThreshold});

  BrowserViewWrangler* wrangler =
      [[BrowserViewWrangler alloc] initWithProfile:profile()
                                        sceneState:scene_state()
                               applicationEndpoint:nil
                                  settingsEndpoint:nil];

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile());

  // Create the coordinator and interface. This is required to get access
  // to the Browser via the -mainInterface/-incognitoInterface providers.
  [wrangler createMainCoordinatorAndInterface];

  // The BrowserViewWrangler creates all browser in its initializer. The
  // first created CL is the main Browser, the second one the inactive
  // Browser, and then the OTR Browser.
  EXPECT_EQ(2UL,
            browser_list
                ->BrowsersOfType(BrowserList::BrowserType::kRegularAndInactive)
                .size());
  EXPECT_EQ(1UL,
            browser_list->BrowsersOfType(BrowserList::BrowserType::kIncognito)
                .size());
  EXPECT_EQ(wrangler.mainInterface.inactiveBrowser,
            browser_list_observer().GetLastAddedBrowser());
  EXPECT_EQ(wrangler.incognitoInterface.browser,
            browser_list_observer().GetLastAddedIncognitoBrowser());

  // Record the old Browser before it is destroyed. This will be dangling
  // after the call to -willDestroyIncognitoProfile.
  Browser* prior_otr_browser = wrangler.incognitoInterface.browser;
  [wrangler willDestroyIncognitoProfile];
  RecreateOffTheRecordProfile();
  [wrangler incognitoProfileCreated];

  // Expect that the prior OTR browser was removed, and a new one was added.
  EXPECT_EQ(prior_otr_browser,
            browser_list_observer().GetLastRemovedIncognitoBrowser());
  EXPECT_EQ(wrangler.incognitoInterface.browser,
            browser_list_observer().GetLastAddedIncognitoBrowser());
  // There still should be one OTR browser.
  EXPECT_EQ(1UL,
            browser_list->BrowsersOfType(BrowserList::BrowserType::kIncognito)
                .size());

  // Store unsafe pointers to the current browsers.
  Browser* pre_shutdown_main_browser = wrangler.mainInterface.browser;
  Browser* pre_shutdown_incognito_browser = wrangler.incognitoInterface.browser;

  // After shutdown all browsers are destroyed.
  [wrangler shutdown];

  // There should be no browsers in the BrowserList.
  EXPECT_EQ(0UL,
            browser_list
                ->BrowsersOfType(BrowserList::BrowserType::kRegularAndInactive)
                .size());
  EXPECT_EQ(0UL,
            browser_list->BrowsersOfType(BrowserList::BrowserType::kIncognito)
                .size());
  // Both browser removals should have been observed.
  EXPECT_EQ(pre_shutdown_main_browser,
            browser_list_observer().GetLastRemovedBrowser());
  EXPECT_EQ(pre_shutdown_incognito_browser,
            browser_list_observer().GetLastRemovedIncognitoBrowser());
}

TEST_F(BrowserViewWranglerTest, TestInactiveInterface) {
  // No inactive tabs on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  // Enabled inactive tabs feature.
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kTabInactivityThresholdParameterName] =
      kTabInactivityThresholdOneWeekParam;
  feature_list.InitAndEnableFeatureWithParameters(kTabInactivityThreshold,
                                                  parameters);

  BrowserViewWrangler* wrangler =
      [[BrowserViewWrangler alloc] initWithProfile:profile()
                                        sceneState:scene_state()
                               applicationEndpoint:nil
                                  settingsEndpoint:nil];

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile());

  [wrangler createMainCoordinatorAndInterface];
  EXPECT_EQ(2UL,
            browser_list
                ->BrowsersOfType(BrowserList::BrowserType::kRegularAndInactive)
                .size());
  EXPECT_EQ(wrangler.mainInterface.inactiveBrowser,
            browser_list_observer().GetLastAddedBrowser());

  // After shutdown all browsers are destroyed.
  [wrangler shutdown];
  EXPECT_EQ(0UL,
            browser_list
                ->BrowsersOfType(BrowserList::BrowserType::kRegularAndInactive)
                .size());
}

// Tests the session restoration logic.
TEST_F(BrowserViewWranglerTest, TestSessionRestorationLogic) {
  BrowserViewWrangler* wrangler =
      [[BrowserViewWrangler alloc] initWithProfile:profile()
                                        sceneState:scene_state()
                               applicationEndpoint:nil
                                  settingsEndpoint:nil];

  // Create the coordinator and interface. This is required to get access
  // to the Browser via the -mainInterface/-incognitoInterface providers.
  [wrangler createMainCoordinatorAndInterface];
  EXPECT_EQ(0, session_restoration_observer().session_restoration_call_count());

  // Load the session for all Browser. There should be one for the main
  // Browser, one for the inactive Browser and one for the OTR Browser.
  [wrangler loadSession];
  EXPECT_EQ(3, session_restoration_observer().session_restoration_call_count());

  // Destroing and rebuilding the incognito browser should not restore the
  // sessions.
  [wrangler willDestroyIncognitoProfile];
  RecreateOffTheRecordProfile();
  [wrangler incognitoProfileCreated];

  EXPECT_EQ(3, session_restoration_observer().session_restoration_call_count());
  [wrangler shutdown];
}
