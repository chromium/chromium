// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"

#import <UIKit/UIKit.h>

#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_util_test_support.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser_list_observer.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/main/wrangled_browser.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/device_form_factor.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

@interface SceneStateWithFakeScene : SceneState

- (instancetype)initWithScene:(id)scene NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithAppState:(AppState*)appState NS_UNAVAILABLE;

@end

@implementation SceneStateWithFakeScene

- (instancetype)initWithScene:(id)scene {
  if ((self = [super initWithAppState:nil])) {
    [self setScene:scene];
  }
  return self;
}

@end

namespace {

class BrowserViewWranglerTest : public PlatformTest {
 protected:
  BrowserViewWranglerTest() {
    scoped_feature_list_.InitAndDisableFeature(
        web::features::kEnableSessionSerializationOptimizations);

    fake_scene_ = FakeSceneWithIdentifier([[NSUUID UUID] UUIDString]);
    scene_state_ = [[SceneStateWithFakeScene alloc] initWithScene:fake_scene_];
    test_session_service_ = [[TestSessionService alloc] init];

    TestChromeBrowserState::Builder test_cbs_builder;
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
        ios::LocalOrSyncableBookmarkModelFactory::GetInstance(),
        ios::LocalOrSyncableBookmarkModelFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());

    chrome_browser_state_ = test_cbs_builder.Build();

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    session_service_block_ = ^SessionServiceIOS*(id self) {
      return test_session_service_;
    };
    session_service_swizzler_.reset(new ScopedBlockSwizzler(
        [SessionServiceIOS class], @selector(sharedService),
        session_service_block_));
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  id fake_scene_;
  SceneState* scene_state_;
  TestSessionService* test_session_service_;
  id session_service_block_;
  std::unique_ptr<ScopedBlockSwizzler> session_service_swizzler_;
};

TEST_F(BrowserViewWranglerTest, TestInitNilObserver) {
  // `task_environment_` must outlive all objects created by BVC, because those
  // objects may rely on threading API in dealloc.
  @autoreleasepool {
    BrowserViewWrangler* wrangler = [[BrowserViewWrangler alloc]
               initWithBrowserState:chrome_browser_state_.get()
                         sceneState:scene_state_
         applicationCommandEndpoint:(id<ApplicationCommands>)nil
        browsingDataCommandEndpoint:nil];
    [wrangler createMainCoordinatorAndInterface];

    // Test that BVC is created on demand.
    UIViewController* bvc = wrangler.mainInterface.viewController;
    EXPECT_NE(bvc, nil);

    // Test that scene_state_ is associated with the browser.
    SceneState* main_browser_scene_state =
        SceneStateBrowserAgent::FromBrowser(wrangler.mainInterface.browser)
            ->GetSceneState();
    EXPECT_EQ(scene_state_, main_browser_scene_state);

    // Test that once created the BVC isn't re-created.
    EXPECT_EQ(bvc, wrangler.mainInterface.viewController);

    // Test that the OTR objects are (a) OTR and (b) not the same as the non-OTR
    // objects.
    EXPECT_NE(bvc, wrangler.incognitoInterface.viewController);
    EXPECT_TRUE(wrangler.incognitoInterface.browserState->IsOffTheRecord());

    // Test that the OTR browser has scene_state_ associated with it.
    SceneState* otr_browser_scene_state =
        SceneStateBrowserAgent::FromBrowser(wrangler.incognitoInterface.browser)
            ->GetSceneState();
    EXPECT_EQ(scene_state_, otr_browser_scene_state);

    [wrangler shutdown];
  }
}

TEST_F(BrowserViewWranglerTest, TestBrowserList) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {/* Enabled features */},
      {/* Disabled features */ kTabInactivityThreshold});

  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(chrome_browser_state_.get());
  TestBrowserListObserver observer;
  browser_list->AddObserver(&observer);

  BrowserViewWrangler* wrangler = [[BrowserViewWrangler alloc]
             initWithBrowserState:chrome_browser_state_.get()
                       sceneState:scene_state_
       applicationCommandEndpoint:nil
      browsingDataCommandEndpoint:nil];

  // Create the coordinator and interface. This is required to get access
  // to the Browser via the -mainInterface/-incognitoInterface providers.
  [wrangler createMainCoordinatorAndInterface];

  // The BrowserViewWrangler creates all browser in its initializer. The
  // first created CL is the main Browser, the second one the inactive
  // Browser, and then the OTR Browser.
  EXPECT_EQ(2UL, browser_list->AllRegularBrowsers().size());
  EXPECT_EQ(1UL, browser_list->AllIncognitoBrowsers().size());
  EXPECT_EQ(wrangler.mainInterface.inactiveBrowser,
            observer.GetLastAddedBrowser());
  EXPECT_EQ(wrangler.incognitoInterface.browser,
            observer.GetLastAddedIncognitoBrowser());

  // Record the old Browser before it is destroyed. This will be dangling
  // after the call to -willDestroyIncognitoBrowserState.
  Browser* prior_otr_browser = wrangler.incognitoInterface.browser;
  [wrangler willDestroyIncognitoBrowserState];
  chrome_browser_state_->DestroyOffTheRecordChromeBrowserState();
  chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  [wrangler incognitoBrowserStateCreated];

  // Expect that the prior OTR browser was removed, and a new one was added.
  EXPECT_EQ(prior_otr_browser, observer.GetLastRemovedIncognitoBrowser());
  EXPECT_EQ(wrangler.incognitoInterface.browser,
            observer.GetLastAddedIncognitoBrowser());
  // There still should be one OTR browser.
  EXPECT_EQ(1UL, browser_list->AllIncognitoBrowsers().size());

  // Store unsafe pointers to the current browsers.
  Browser* pre_shutdown_main_browser = wrangler.mainInterface.browser;
  Browser* pre_shutdown_incognito_browser = wrangler.incognitoInterface.browser;

  // After shutdown all browsers are destroyed.
  [wrangler shutdown];

  // There should be no browsers in the BrowserList.
  EXPECT_EQ(0UL, browser_list->AllRegularBrowsers().size());
  EXPECT_EQ(0UL, browser_list->AllIncognitoBrowsers().size());
  // Both browser removals should have been observed.
  EXPECT_EQ(pre_shutdown_main_browser, observer.GetLastRemovedBrowser());
  EXPECT_EQ(pre_shutdown_incognito_browser,
            observer.GetLastRemovedIncognitoBrowser());

  browser_list->RemoveObserver(&observer);
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

  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(chrome_browser_state_.get());
  TestBrowserListObserver observer;
  browser_list->AddObserver(&observer);

  BrowserViewWrangler* wrangler = [[BrowserViewWrangler alloc]
             initWithBrowserState:chrome_browser_state_.get()
                       sceneState:scene_state_
       applicationCommandEndpoint:nil
      browsingDataCommandEndpoint:nil];

  [wrangler createMainCoordinatorAndInterface];
  EXPECT_EQ(2UL, browser_list->AllRegularBrowsers().size());
  EXPECT_EQ(wrangler.mainInterface.inactiveBrowser,
            observer.GetLastAddedBrowser());

  // After shutdown all browsers are destroyed.
  [wrangler shutdown];
  EXPECT_EQ(0UL, browser_list->AllRegularBrowsers().size());

  browser_list->RemoveObserver(&observer);
}

TEST_F(BrowserViewWranglerTest, TestIncognitoBrowserSessionRestorationLogic) {
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(chrome_browser_state_.get());
  TestBrowserListObserver observer;
  browser_list->AddObserver(&observer);

  BrowserViewWrangler* wrangler = [[BrowserViewWrangler alloc]
             initWithBrowserState:chrome_browser_state_.get()
                       sceneState:scene_state_
       applicationCommandEndpoint:nil
      browsingDataCommandEndpoint:nil];

  // Create the coordinator and interface. This is required to get access
  // to the Browser via the -mainInterface/-incognitoInterface providers.
  [wrangler createMainCoordinatorAndInterface];
  EXPECT_EQ(0, test_session_service_.loadSessionCallsCount);

  // Load the session for all Browser. There should be one for the main
  // Browser, one for the inactive Browser and one for the OTR Browser.
  [wrangler loadSession];
  EXPECT_EQ(3, test_session_service_.loadSessionCallsCount);

  // Destroing and rebuilding the incognito browser should not restore the
  // sessions.
  [wrangler willDestroyIncognitoBrowserState];
  chrome_browser_state_->DestroyOffTheRecordChromeBrowserState();
  chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  [wrangler incognitoBrowserStateCreated];

  EXPECT_EQ(3, test_session_service_.loadSessionCallsCount);

  [wrangler shutdown];

  browser_list->RemoveObserver(&observer);
}

}  // namespace
