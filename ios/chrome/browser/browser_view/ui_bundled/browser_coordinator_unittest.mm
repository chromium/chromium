// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_coordinator.h"

#import "base/files/file_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_coordinator+Testing.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/lens/model/lens_browser_agent.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_coordinator.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/model/web_state_delegate_browser_agent.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
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
#import "ui/base/device_form_factor.h"

// Test fixture for BrowserCoordinator testing.
class BrowserCoordinatorTest : public PlatformTest {
 protected:
  BrowserCoordinatorTest() {
    base_view_controller_ = [[UIViewController alloc] init];
    scene_state_ = [[SceneState alloc] initWithAppState:nil];

    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        PrerenderServiceFactory::GetInstance(),
        PrerenderServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        ios::BookmarkModelFactory::GetInstance(),
        ios::BookmarkModelFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<commerce::MockShoppingService>();
            }));
    profile_ =
        profile_manager_.AddProfileWithBuilder(std::move(test_profile_builder));

    browser_ = std::make_unique<TestBrowser>(GetProfile(), scene_state_);
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());
    WebStateDelegateBrowserAgent::CreateForBrowser(
        browser_.get(), TabInsertionBrowserAgent::FromBrowser(browser_.get()));
    SyncErrorBrowserAgent::CreateForBrowser(browser_.get());
    OmniboxPositionBrowserAgent::CreateForBrowser(browser_.get());

    WebUsageEnablerBrowserAgent* enabler =
        WebUsageEnablerBrowserAgent::FromBrowser(browser_.get());
    enabler->SetWebUsageEnabled(true);

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        GetProfile(), std::make_unique<FakeAuthenticationServiceDelegate>());

    IncognitoReauthSceneAgent* reauthAgent = [[IncognitoReauthSceneAgent alloc]
        initWithReauthModule:[[ReauthenticationModule alloc] init]];
    [scene_state_ addAgent:reauthAgent];

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:reauthAgent
                             forProtocol:@protocol(IncognitoReauthCommands)];

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to SettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockSettingsCommandHandler =
        OCMProtocolMock(@protocol(SettingsCommands));
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher startDispatchingToTarget:mockSettingsCommandHandler
                             forProtocol:@protocol(SettingsCommands)];
  }

  BrowserCoordinator* GetBrowserCoordinator() {
    return [[BrowserCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
  }

  ProfileIOS* GetProfile() { return profile_.get(); }

  // Creates and inserts a new WebState.
  int InsertWebState() {
    web::WebState::CreateParams params(GetProfile());
    std::unique_ptr<web::WebState> web_state = web::WebState::Create(params);
    AttachTabHelpers(web_state.get());

    int insertion_index = browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
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

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
  UIViewController* base_view_controller_;
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
  // TestProfileIOS is destroyed. Without the @autoreleasepool the
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
  // Mock the SaveToPhotosCoordinator class.
  id mockSaveToPhotosCoordinator =
      OCMStrictClassMock([SaveToPhotosCoordinator class]);

  // Start the BrowserCoordinator.
  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];

  // At rest, check the SaveToPhotosCoordinator is nil.
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
  // Start the BrowserCoordinator
  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<PromosManagerCommands> handler =
      HandlerForProtocol(dispatcher, PromosManagerCommands);

  [handler displayDefaultBrowserPromoAfterRemindMeLater];

  [browser_coordinator stop];
}
