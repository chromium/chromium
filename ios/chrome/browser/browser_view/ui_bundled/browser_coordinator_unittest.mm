// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/test/ios/test_utils.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_coordinator+Testing.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_model.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/main/model/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_coordinator.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/save_to_photos/ui_bundled/save_to_photos_coordinator.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/sync_presenter_commands.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_params.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/fullscreen/toolbars_size_browser_agent.h"
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
        ios::BookmarkModelFactory::GetInstance(),
        ios::BookmarkModelFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    test_profile_builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<commerce::MockShoppingService>();
            }));
    test_profile_builder.AddTestingFactory(
        TipsManagerIOSFactory::GetInstance(),
        TipsManagerIOSFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::MockSyncService>();
            }));
    test_profile_builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        tab_groups::TabGroupSyncServiceFactory::GetDefaultFactory());
    profile_ =
        profile_manager_.AddProfileWithBuilder(std::move(test_profile_builder));

    ProfileIOS* profile = GetProfile();
    browser_ = std::make_unique<TestBrowser>(
        profile, scene_state_,
        std::make_unique<BrowserWebStateListDelegate>(
            profile,
            BrowserWebStateListDelegate::InsertionPolicy::kAttachTabHelpers,
            BrowserWebStateListDelegate::ActivationPolicy::kDoNothing),
        profile->IsOffTheRecord() ? Browser::Type::kIncognito
                                  : Browser::Type::kRegular);
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());
    WebStateDelegateBrowserAgent::CreateForBrowser(browser_.get());
    SyncErrorBrowserAgent::CreateForBrowser(browser_.get());
    OmniboxPositionBrowserAgent::CreateForBrowser(browser_.get());
    BrowserViewVisibilityNotifierBrowserAgent::CreateForBrowser(browser_.get());
    DiscoverFeedVisibilityBrowserAgent::CreateForBrowser(browser_.get());
    ToolbarsSizeBrowserAgent::CreateForBrowser(browser_.get());
    TestFullscreenController::CreateForBrowser(browser_.get());
    AutocompleteBrowserAgent::CreateForBrowser(browser_.get());

    WebUsageEnablerBrowserAgent* enabler =
        WebUsageEnablerBrowserAgent::FromBrowser(browser_.get());
    enabler->SetWebUsageEnabled(true);

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();

    // Set up SceneCommands mock. Because SceneCommands conforms
    // to SettingsCommands, that needs to be mocked and dispatched
    // as well.
    mock_scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    id mock_settings_handler = OCMProtocolMock(@protocol(SettingsCommands));
    [dispatcher startDispatchingToTarget:mock_scene_handler_
                             forProtocol:@protocol(SceneCommands)];
    [dispatcher startDispatchingToTarget:mock_settings_handler
                             forProtocol:@protocol(SettingsCommands)];

    IncognitoReauthSceneAgent* reauth_agent = [[IncognitoReauthSceneAgent alloc]
        initWithReauthModule:[[ReauthenticationModule alloc] init]
                sceneHandler:mock_scene_handler_];
    [scene_state_ addAgent:reauth_agent];
    [dispatcher startDispatchingToTarget:reauth_agent
                             forProtocol:@protocol(IncognitoReauthCommands)];
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
    NewTabPageTabHelper* ntp_helper =
        NewTabPageTabHelper::FromWebState(web_state);
    web::FakeNavigationContext context;
    context.SetUrl(url);
    context.SetIsSameDocument(false);
    ntp_helper->DidStartNavigation(web_state, &context);
    ntp_helper->PageLoaded(web_state, web::PageLoadCompletionStatus::SUCCESS);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
  UIViewController* base_view_controller_;
  std::unique_ptr<TestBrowser> browser_;
  SceneState* scene_state_;
  id<SceneCommands> mock_scene_handler_;
};

// Tests showDownloadsFolder opens Files.app when download list is disabled.
TEST_F(BrowserCoordinatorTest, ShowDownloadsFolder) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kDownloadList);

  base::FilePath download_dir;
  GetDownloadsDirectory(&download_dir);

  NSURL* url = GetFilesAppUrl();
  ASSERT_TRUE(url);

  UIApplication* shared_application = [UIApplication sharedApplication];
  ASSERT_TRUE([shared_application canOpenURL:url]);

  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> handler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);

  // When the download list feature is disabled, showDownloadsFolder should
  // open Files.app.
  id shared_application_mock = OCMPartialMock(shared_application);

  OCMExpect([shared_application_mock openURL:url
                                     options:[OCMArg any]
                           completionHandler:nil]);

  [handler showDownloadsFolder];

  EXPECT_OCMOCK_VERIFY(shared_application_mock);

  [browser_coordinator stop];
}

// Tests showDownloadsFolder shows download list UI when feature is enabled.
TEST_F(BrowserCoordinatorTest, ShowDownloadList) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kDownloadList);

  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> handler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);

  // When the download list feature is enabled, showDownloadsFolder should
  // present the download list UI instead of opening Files.app.
  [handler showDownloadsFolder];

  // Verify that the download list coordinator was created.
  EXPECT_NE(browser_coordinator.downloadListCoordinator, nil);

  [browser_coordinator stop];
}

// Tests that `-showShareSheet` is leaving fullscreen and starting the share
// coordinator.
TEST_F(BrowserCoordinatorTest, ShowShareSheet) {
  TestFullscreenController* controller =
      TestFullscreenController::FromBrowser(browser_.get());

  controller->EnterFullscreen();
  ASSERT_EQ(0.0, controller->GetProgress());

  UIView* source = [[UIView alloc] init];

  id classMock = OCMClassMock([SharingCoordinator class]);
  SharingCoordinator* mockSharingCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:[OCMArg any]
                                   browser:browser_.get()
                                    params:[OCMArg any]
                                sourceItem:source])
      .andReturn(mockSharingCoordinator);
  OCMExpect([mockSharingCoordinator start]);

  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];
  [browser_coordinator showShareSheetFromShareButton:source];

  // Check that fullscreen is exited.
  EXPECT_EQ(1.0, controller->GetProgress());

  [browser_coordinator stop];

  // Check that -start has been called.
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-showShareSheetForChromeApp` is instantiating the
// SharingCoordinator with SharingParams where scenario is ShareChrome, leaving
// fullscreen and starting the share coordinator.
TEST_F(BrowserCoordinatorTest, ShowShareSheetForChromeApp) {
  TestFullscreenController* controller =
      TestFullscreenController::FromBrowser(browser_.get());

  controller->EnterFullscreen();
  ASSERT_EQ(0.0, controller->GetProgress());

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
                                sourceItem:[OCMArg any]])
      .andReturn(mockSharingCoordinator);
  OCMExpect([mockSharingCoordinator start]);

  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];
  [browser_coordinator showShareSheetForChromeApp];

  // Check that fullscreen is exited.
  EXPECT_EQ(1.0, controller->GetProgress());

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

// Tests that the `-showDefaultBrowserPromoAfterRemindMeLater` command does not
// crash.
TEST_F(BrowserCoordinatorTest, ShowDefaultBrowserPromoAfterRemindMeLater) {
  // Starts the `BrowserCoordinator`.
  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<PromosManagerCommands> handler =
      HandlerForProtocol(dispatcher, PromosManagerCommands);

  [handler showDefaultBrowserPromoAfterRemindMeLater];

  [browser_coordinator stop];
}

// Tests that the BrowserCoordinator early returns from
// `overscrollActionRefresh:` if it doesn't have an active web state.
TEST_F(BrowserCoordinatorTest,
       NoCrashOnOverscrollActionsRefreshWithNoActiveWebState) {
  OverscrollActionsController* overscroll_actions_controller;
  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];

  [browser_coordinator overscrollActionRefresh:overscroll_actions_controller];

  [browser_coordinator stop];
}

// Tests that the completion callback for
// showPrimaryAccountReauthWithDismissalCompletion is called correctly.
TEST_F(BrowserCoordinatorTest, TestPrimaryAccountReauthCompletion) {
  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];
  id<SyncPresenterCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), SyncPresenterCommands);
  SigninCoordinator* signin_mock =
      OCMStrictClassMock([SigninCoordinator class]);
  OCMExpect(
      [((id)signin_mock)
          primaryAccountReauthCoordinatorWithBaseViewController:[OCMArg any]
                                                        browser:
                                                            ios::OCM::
                                                                AnyPointer<
                                                                    Browser>()
                                                   contextStyle:
                                                       SigninContextStyle::
                                                           kDefault
                                                    accessPoint:
                                                        signin_metrics::
                                                            AccessPoint::
                                                                kStartPage
                                                    promoAction:
                                                        signin_metrics::
                                                            PromoAction::
                                                                PROMO_ACTION_NO_SIGNIN_PROMO
                                           continuationProvider:
                                               DoNothingContinuationProvider()])
      .ignoringNonObjectArgs()
      .andReturn(signin_mock);

  __block SigninCoordinatorCompletionCallback signin_coordinator_callback = nil;
  OCMExpect([signin_mock
      setSigninCompletion:AssignValueToVariable(signin_coordinator_callback)]);
  OCMExpect([signin_mock start]);

  __block bool completion_was_called = false;
  [handler showPrimaryAccountReauthWithDismissalCompletion:^() {
    completion_was_called = true;
  }];

  EXPECT_OCMOCK_VERIFY((id)signin_mock);

  OCMExpect([signin_mock stop]);
  signin_coordinator_callback(
      signin_mock, SigninCoordinatorResult::SigninCoordinatorResultSuccess,
      nil);
  EXPECT_TRUE(completion_was_called);

  [browser_coordinator stop];

  EXPECT_OCMOCK_VERIFY((id)signin_mock);
}

// Tests that the completion callback for
// showTrustedVaultReauthForFetchKeysWithTrigger is called correctly.
TEST_F(BrowserCoordinatorTest, TestTrustedVaultReauthCompletion) {
  trusted_vault::TrustedVaultUserActionTriggerForUMA trigger =
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kSettings;
  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];
  id<SyncPresenterCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), SyncPresenterCommands);
  TrustedVaultReauthenticationCoordinator* trusted_vault_mock =
      OCMStrictClassMock([TrustedVaultReauthenticationCoordinator class]);
  OCMExpect([((id)trusted_vault_mock) alloc]).andReturn(trusted_vault_mock);
  OCMExpect(
      [trusted_vault_mock
          initWithBaseViewController:browser_coordinator.viewController
                             browser:browser_.get()
                              intent:SigninTrustedVaultDialogIntentFetchKeys
                    securityDomainID:trusted_vault::SecurityDomainId::
                                         kChromeSync
                             trigger:trigger])
      .andReturn(trusted_vault_mock);

  __block id<TrustedVaultReauthenticationCoordinatorDelegate> delegate;
  OCMExpect([trusted_vault_mock setDelegate:AssignValueToVariable(delegate)]);
  OCMExpect([trusted_vault_mock start]);

  __block bool completion_was_called = false;
  [handler showTrustedVaultReauthForFetchKeysWithTrigger:trigger
                                              completion:^() {
                                                completion_was_called = true;
                                              }];
  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);

  OCMExpect([trusted_vault_mock setDelegate:nil]);
  OCMExpect([trusted_vault_mock stop]);

  [delegate trustedVaultReauthenticationCoordinatorWantsToBeStopped:
                trusted_vault_mock];
  EXPECT_TRUE(completion_was_called);

  [browser_coordinator stop];

  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);
}

// Tests that a double tap on the trusted vault reauth errors button don’t
// trigger two openings of the trusted vault reauth coordinator.
TEST_F(BrowserCoordinatorTest, TestDoubleTapTrustedVaultReauth) {
  trusted_vault::TrustedVaultUserActionTriggerForUMA trigger =
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kSettings;
  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];
  id<SyncPresenterCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), SyncPresenterCommands);
  TrustedVaultReauthenticationCoordinator* trusted_vault_mock =
      OCMStrictClassMock([TrustedVaultReauthenticationCoordinator class]);
  OCMExpect([((id)trusted_vault_mock) alloc]).andReturn(trusted_vault_mock);
  OCMExpect(
      [trusted_vault_mock
          initWithBaseViewController:browser_coordinator.viewController
                             browser:browser_.get()
                              intent:SigninTrustedVaultDialogIntentFetchKeys
                    securityDomainID:trusted_vault::SecurityDomainId::
                                         kChromeSync
                             trigger:trigger])
      .andReturn(trusted_vault_mock);
  OCMExpect([trusted_vault_mock setDelegate:[OCMArg any]]);
  OCMExpect([trusted_vault_mock start]);
  [handler showTrustedVaultReauthForFetchKeysWithTrigger:trigger
                                              completion:nil];
  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);
  // Checks that the second tap is ignored.
  // Checks that the second tap is ignored. No more
  // TrustedVaultReauthenticationCoordinator are allocated
  OCMStub([((id)trusted_vault_mock) alloc]).andDo(^(NSInvocation* invocation) {
    EXPECT_FALSE(true);
  });
  [handler showTrustedVaultReauthForFetchKeysWithTrigger:trigger
                                              completion:nil];
  [handler showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:trigger
                                                           completion:nil];

  OCMExpect([trusted_vault_mock setDelegate:nil]);
  OCMExpect([trusted_vault_mock stop]);
  [browser_coordinator stop];

  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);
}

// Tests that a double tap on the trusted vault reauth errors button don’t
// trigger two openings of the trusted vault reauth coordinator.
TEST_F(BrowserCoordinatorTest,
       TestDoubleTapTrustedVaultReauthForDegradedRecoverability) {
  trusted_vault::TrustedVaultUserActionTriggerForUMA trigger =
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kSettings;
  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];
  id<SyncPresenterCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), SyncPresenterCommands);
  TrustedVaultReauthenticationCoordinator* trusted_vault_mock =
      OCMStrictClassMock([TrustedVaultReauthenticationCoordinator class]);
  OCMExpect([((id)trusted_vault_mock) alloc]).andReturn(trusted_vault_mock);
  SigninTrustedVaultDialogIntent intent =
      SigninTrustedVaultDialogIntentDegradedRecoverability;
  OCMExpect([trusted_vault_mock
                initWithBaseViewController:browser_coordinator.viewController
                                   browser:browser_.get()
                                    intent:intent
                          securityDomainID:trusted_vault::SecurityDomainId::
                                               kChromeSync
                                   trigger:trigger])
      .andReturn(trusted_vault_mock);
  OCMExpect([trusted_vault_mock setDelegate:[OCMArg any]]);
  OCMExpect([trusted_vault_mock start]);
  [handler showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:trigger
                                                           completion:nil];
  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);

  // Checks that the second tap is ignored. No more
  // TrustedVaultReauthenticationCoordinator are allocated
  OCMStub([((id)trusted_vault_mock) alloc]).andDo(^(NSInvocation* invocation) {
    EXPECT_FALSE(true);
  });
  [handler showTrustedVaultReauthForFetchKeysWithTrigger:trigger
                                              completion:nil];
  [handler showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:trigger
                                                           completion:nil];

  OCMExpect([trusted_vault_mock setDelegate:nil]);
  OCMExpect([trusted_vault_mock stop]);
  [browser_coordinator stop];

  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);
}

// Tests that showBookmarksLimitExceededHelp acknowledges the error and opens
// the help URL.
TEST_F(BrowserCoordinatorTest, ShowBookmarksLimitExceededHelp) {
  BrowserCoordinator* browser_coordinator = GetBrowserCoordinator();
  [browser_coordinator start];

  syncer::MockSyncService* mock_sync_service =
      static_cast<syncer::MockSyncService*>(
          SyncServiceFactory::GetForProfile(GetProfile()));

  EXPECT_CALL(*mock_sync_service,
              AcknowledgeBookmarksLimitExceededError(
                  syncer::SyncService::BookmarksLimitExceededHelpClickedSource::
                      kSyncErrorMessage));

  OCMExpect([mock_scene_handler_ closePresentedViewsAndOpenURL:[OCMArg any]]);

  [browser_coordinator showBookmarksLimitExceededHelp];

  EXPECT_OCMOCK_VERIFY((OCMockObject*)mock_scene_handler_);

  [browser_coordinator stop];
}
