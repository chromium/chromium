// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"

#import "base/files/file_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/download/external_app_util.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/lens/lens_browser_agent.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/ui/browser_view/browser_coordinator+private.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/web_state_delegate_browser_agent.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_observer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"

// Test fixture for BrowserCoordinator testing.
class BrowserCoordinatorTest : public PlatformTest {
 protected:
  BrowserCoordinatorTest() {
    scoped_feature_list_.InitAndDisableFeature(
        web::features::kEnableSessionSerializationOptimizations);

    base_view_controller_ = [[UIViewController alloc] init];
    scene_state_ = [[SceneState alloc] initWithAppState:nil];

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
        ios::LocalOrSyncableBookmarkModelFactory::GetInstance(),
        ios::LocalOrSyncableBookmarkModelFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(AuthenticationServiceFactory::GetDefaultFactory()));

    chrome_browser_state_ = test_cbs_builder.Build();
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
    WebStateDelegateBrowserAgent::CreateForBrowser(
        browser_.get(), TabInsertionBrowserAgent::FromBrowser(browser_.get()));
    SyncErrorBrowserAgent::CreateForBrowser(browser_.get());

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    SessionRestorationBrowserAgent::CreateForBrowser(
        browser_.get(), [[TestSessionService alloc] init], false);
    SessionRestorationBrowserAgent::FromBrowser(browser_.get())
        ->SetSessionID([[NSUUID UUID] UUIDString]);

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

  // Creates and inserts a new WebState.
  int InsertWebState() {
    web::WebState::CreateParams params(chrome_browser_state_.get());
    std::unique_ptr<web::WebState> web_state = web::WebState::Create(params);
    AttachTabHelpers(web_state.get(), NO);

    int insertion_index = browser_->GetWebStateList()->InsertWebState(
        /*index=*/0, std::move(web_state), WebStateList::INSERT_ACTIVATE,
        WebStateOpener());
    browser_->GetWebStateList()->ActivateWebStateAt(insertion_index);

    return insertion_index;
  }

  // Returns the active WebState.
  web::WebState* GetActiveWebState() {
    return browser_->GetWebStateList()->GetActiveWebState();
  }

  void OpenURL(const GURL& url) {
    web::WebState* web_state = GetActiveWebState();

    UrlLoadingBrowserAgent* urlLoadingBrowserAgent =
        UrlLoadingBrowserAgent::FromBrowser(browser_.get());
    UrlLoadParams urlLoadParams = UrlLoadParams::InCurrentTab(url);
    urlLoadingBrowserAgent->Load(urlLoadParams);

    // Force the WebStateObserver callbacks that simulate a page load.
    web::WebStateObserver* ntpHelper =
        (web::WebStateObserver*)NewTabPageTabHelper::FromWebState(web_state);
    web::FakeNavigationContext context;
    context.SetUrl(url);
    context.SetIsSameDocument(false);
    ntpHelper->DidStartNavigation(web_state, &context);
    ntpHelper->PageLoaded(web_state, web::PageLoadCompletionStatus::SUCCESS);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState local_state_;
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
// with SharingParams where scenario is ShareChrome, leaving fullscreen
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
      [OCMArg checkWithBlock:^BOOL(SharingParams* params) {
        return params.scenario == SharingScenario::ShareChrome;
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

// Tests that BrowserCoordinator properly implements
// the NewTabPageTabHelperDelegate protocol.
TEST_F(BrowserCoordinatorTest, NewTabPageTabHelperDelegate) {
  // This test is wrapped in an @autoreleasepool because some arguments passed
  // to methods on some of the mock objects need to be freed before
  // TestChromeBrowserState is destroyed. Without the @autoreleasepool the
  // NSInvocation objects which keep these arguments alive aren't destroyed
  // until the parent PlatformTest class itself is destroyed.
  @autoreleasepool {
    BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
    [browser_coordinator start];

    id mockNTPCoordinator = OCMPartialMock(browser_coordinator.NTPCoordinator);

    // Insert the web_state into the Browser.
    InsertWebState();

    // Open an NTP to start the coordinator.
    OpenURL(GURL("chrome://newtab/"));
    EXPECT_OCMOCK_VERIFY(mockNTPCoordinator);

    // Open a non-NTP page and expect a call to the NTP coordinator.
    [[mockNTPCoordinator expect] didNavigateAwayFromNTP];
    OpenURL(GURL("chrome://version/"));
    EXPECT_OCMOCK_VERIFY(mockNTPCoordinator);

    // Open another NTP and expect a navigation call.
    [[mockNTPCoordinator expect]
        didNavigateToNTPInWebState:GetActiveWebState()];
    OpenURL(GURL("chrome://newtab/"));
    EXPECT_OCMOCK_VERIFY(mockNTPCoordinator);

    [browser_coordinator stop];
  }
}

// Tests that BrowserCoordinator starts and stops the SaveToPhotosCoordinator
// properly when SaveToPhotosCommands are issued.
TEST_F(BrowserCoordinatorTest, StartsAndStopsSaveToPhotosCoordinator) {
  // Mock the SaveToPhotosCoordinator class
  id mockSaveToPhotosCoordinator =
      OCMStrictClassMock([SaveToPhotosCoordinator class]);

  // Start the BrowserCoordinator
  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];

  // At rest, check the SaveToPhotosCoordinator is nil
  EXPECT_EQ(browser_coordinator.saveToPhotosCoordinator, nil);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<SaveToPhotosCommands> handler =
      HandlerForProtocol(dispatcher, SaveToPhotosCommands);

  // Insert a web state into the Browser.
  InsertWebState();

  GURL fakeImageURL("http://www.example.com/image.jpg");
  web::Referrer fakeImageReferrer;
  web::WebState* webState = GetActiveWebState();
  SaveImageToPhotosCommand* command =
      [[SaveImageToPhotosCommand alloc] initWithImageURL:fakeImageURL
                                                referrer:fakeImageReferrer
                                                webState:webState];

  // Tests that -[BrowserCoordinator saveImageToPhotos:] starts the
  // SaveToPhotosCoordinator.
  OCMExpect([mockSaveToPhotosCoordinator alloc])
      .andReturn(mockSaveToPhotosCoordinator);
  OCMExpect([[mockSaveToPhotosCoordinator ignoringNonObjectArgs]
                initWithBaseViewController:browser_coordinator.viewController
                                   browser:browser_.get()
                                  imageURL:command.imageURL
                                  referrer:command.referrer
                                  webState:command.webState.get()])
      .andReturn(mockSaveToPhotosCoordinator);
  OCMExpect([(SaveToPhotosCoordinator*)mockSaveToPhotosCoordinator start]);
  [handler saveImageToPhotos:command];
  EXPECT_OCMOCK_VERIFY(mockSaveToPhotosCoordinator);
  EXPECT_NE(browser_coordinator.saveToPhotosCoordinator, nil);

  // Tests that -[BrowserCoordinator stopSaveToPhotos:] stops the
  // SaveToPhotosCoordinator.
  OCMExpect([mockSaveToPhotosCoordinator stop]);
  [handler stopSaveToPhotos];
  EXPECT_OCMOCK_VERIFY(mockSaveToPhotosCoordinator);
  EXPECT_EQ(browser_coordinator.saveToPhotosCoordinator, nil);

  [browser_coordinator stop];
}

// Tests that the displayDefaultBrowserPromoAfterRemindMeLater command does not
// crash.
TEST_F(BrowserCoordinatorTest, DisplayDefaultBrowserPromoAfterRemindMeLater) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kDefaultBrowserRefactoringPromoManager);

  // Start the BrowserCoordinator
  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<PromosManagerCommands> handler =
      HandlerForProtocol(dispatcher, PromosManagerCommands);

  [handler displayDefaultBrowserPromoAfterRemindMeLater];

  [browser_coordinator stop];
}
