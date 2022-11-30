// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"

#import "base/files/file_util.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/download/external_app_util.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/lens/lens_browser_agent.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/sync/sync_error_browser_agent.h"
#import "ios/chrome/browser/ui/activity_services/activity_params.h"
#import "ios/chrome/browser/ui/browser_view/browser_coordinator+private.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/web_state_delegate_browser_agent.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for BrowserCoordinator testing.
class BrowserCoordinatorTest : public PlatformTest {
 protected:
  BrowserCoordinatorTest()
      : base_view_controller_([[UIViewController alloc] init]),
        scene_state_([[SceneState alloc] initWithAppState:nil]) {
    TestChromeBrowserState::Builder test_cbs_builder;
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

    chrome_browser_state_ = test_cbs_builder.Build();
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
    WebStateDelegateBrowserAgent::CreateForBrowser(
        browser_.get(), TabInsertionBrowserAgent::FromBrowser(browser_.get()));
    SyncErrorBrowserAgent::CreateForBrowser(browser_.get());

    IncognitoReauthSceneAgent* reauthAgent = [[IncognitoReauthSceneAgent alloc]
        initWithReauthModule:[[ReauthenticationModule alloc] init]];
    [scene_state_ addAgent:reauthAgent];

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:reauthAgent
                             forProtocol:@protocol(IncognitoReauthCommands)];

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to ApplicationSettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockApplicationSettingsCommandHandler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher
        startDispatchingToTarget:mockApplicationSettingsCommandHandler
                     forProtocol:@protocol(ApplicationSettingsCommands)];
  }

  BrowserCoordinator* GetBrowserCoordinator() {
    return [[BrowserCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
  }

  web::WebTaskEnvironment task_environment_;
  UIViewController* base_view_controller_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  SceneState* scene_state_;
};

// Tests if the URL to open the downlads directory from files.app is valid.
TEST_F(BrowserCoordinatorTest, ShowDownloadsFolder) {

  base::FilePath download_dir;
  GetDownloadsDirectory(&download_dir);

  NSURL* url = GetFilesAppUrl();
  ASSERT_TRUE(url);

  UIApplication* shared_application = [UIApplication sharedApplication];
  ASSERT_TRUE([shared_application canOpenURL:url]);

  id shared_application_mock = OCMPartialMock(shared_application);

  OCMExpect([shared_application_mock openURL:url
                                     options:[OCMArg any]
                           completionHandler:nil]);

  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();

  [browser_coordinator start];

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> handler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  [handler showDownloadsFolder];

  [browser_coordinator stop];

  EXPECT_OCMOCK_VERIFY(shared_application_mock);
}

// Tests that -sharePage is leaving fullscreena and starting the share
// coordinator.
TEST_F(BrowserCoordinatorTest, SharePage) {
  FullscreenModel model;
  std::unique_ptr<TestFullscreenController> controller =
      std::make_unique<TestFullscreenController>(&model);
  TestFullscreenController* controller_ptr = controller.get();

  browser_->SetUserData(TestFullscreenController::UserDataKeyForTesting(),
                        std::move(controller));

  controller_ptr->EnterFullscreen();
  ASSERT_EQ(0.0, controller_ptr->GetProgress());

  id classMock = OCMClassMock([SharingCoordinator class]);
  SharingCoordinator* mockSharingCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:[OCMArg any]
                                   browser:browser_.get()
                                    params:[OCMArg any]
                                originView:[OCMArg any]
                                originRect:CGRectZero
                                    anchor:[OCMArg any]])
      .andReturn(mockSharingCoordinator);
  OCMExpect([mockSharingCoordinator start]);

  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];
  [browser_coordinator sharePage];

  // Check that fullscreen is exited.
  EXPECT_EQ(1.0, controller_ptr->GetProgress());

  [browser_coordinator stop];

  // Check that -start has been called.
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that -shareChromeApp is instantiating the SharingCoordinator
// with ActivityParams where scenario is ShareChrome, leaving fullscreen
// and starting the share coordinator.
TEST_F(BrowserCoordinatorTest, ShareChromeApp) {
  FullscreenModel model;
  std::unique_ptr<TestFullscreenController> controller =
      std::make_unique<TestFullscreenController>(&model);
  TestFullscreenController* controller_ptr = controller.get();

  browser_->SetUserData(TestFullscreenController::UserDataKeyForTesting(),
                        std::move(controller));

  controller_ptr->EnterFullscreen();
  ASSERT_EQ(0.0, controller_ptr->GetProgress());

  id expectShareChromeScenarioArg =
      [OCMArg checkWithBlock:^BOOL(ActivityParams* params) {
        return params.scenario == ActivityScenario::ShareChrome;
      }];

  id classMock = OCMClassMock([SharingCoordinator class]);
  SharingCoordinator* mockSharingCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:[OCMArg any]
                                   browser:browser_.get()
                                    params:expectShareChromeScenarioArg
                                originView:[OCMArg any]])
      .andReturn(mockSharingCoordinator);
  OCMExpect([mockSharingCoordinator start]);

  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];
  [browser_coordinator shareChromeApp];

  // Check that fullscreen is exited.
  EXPECT_EQ(1.0, controller_ptr->GetProgress());

  [browser_coordinator stop];

  // Check that -start has been called.
  EXPECT_OCMOCK_VERIFY(classMock);
}
