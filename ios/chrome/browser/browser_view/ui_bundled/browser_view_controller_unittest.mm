// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"

#import <Foundation/Foundation.h>
#import <PassKit/PassKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "components/search_engines/template_url_service.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_coordinator.h"
#import "ios/chrome/browser/browser_container/ui_bundled/browser_container_view_controller.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller+private.h"
#import "ios/chrome/browser/browser_view/ui_bundled/key_commands_provider.h"
#import "ios/chrome/browser/browser_view/ui_bundled/safe_area_provider.h"
#import "ios/chrome/browser/browser_view/ui_bundled/tab_consumer.h"
#import "ios/chrome/browser/browser_view/ui_bundled/tab_events_mediator.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_commands.h"
#import "ios/chrome/browser/lens/model/lens_browser_agent.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/logo_animation_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_component_factory.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_coordinator.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_coordinator.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_coordinator.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/coordinator/tab_strip_coordinator.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/tabs/ui_bundled/foreground_tab_animation_view.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_browser_agent.h"
#import "ios/chrome/browser/toolbar/ui_bundled/toolbar_coordinator.h"
#import "ios/chrome/browser/url_loading/model/new_tab_animation_tab_helper.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/model/web_state_update_browser_agent.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

// Scoped clipboard recent content so its destroyed after the profile. This
// allows the autocomplete keyed service to cleanup before the clipboard
// dependency is destroyed.
class ScopedClipboardRecentContentInstaller {
 public:
  ScopedClipboardRecentContentInstaller(
      std::unique_ptr<ClipboardRecentContent> instance) {
    ClipboardRecentContent::SetInstance(std::move(instance));
  }

  ~ScopedClipboardRecentContentInstaller() {
    ClipboardRecentContent::SetInstance(nullptr);
  }
};

class BrowserViewControllerTest : public BlockCleanupTest {
 public:
 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();

    scene_state_ = [[SceneState alloc] initWithAppState:nil];

    // Set up a TestProfileIOS instance.
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return commerce::MockShoppingService::Build();
            }));
    test_profile_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
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
        TipsManagerIOSFactory::GetInstance(),
        TipsManagerIOSFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        feature_engagement::TrackerFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        tab_groups::TabGroupSyncServiceFactory::GetDefaultFactory());

    profile_ =
        profile_manager_.AddProfileWithBuilder(std::move(test_profile_builder));

    browser_ = std::make_unique<TestBrowser>(GetProfile(), scene_state_);
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    TabUsageRecorderBrowserAgent::CreateForBrowser(browser_.get());
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());
    OmniboxPositionBrowserAgent::CreateForBrowser(browser_.get());
    AutocompleteBrowserAgent::CreateForBrowser(browser_.get());
    BrowserViewVisibilityNotifierBrowserAgent::CreateForBrowser(browser_.get());
    // FullscreenController depends on ToolbarsSizeBrowserAgent, so the agent
    // must be created first. Please maintain this order.
    ToolbarsSizeBrowserAgent::CreateForBrowser(browser_.get());
    FullscreenController::CreateForBrowser(browser_.get());
    DiscoverFeedVisibilityBrowserAgent::CreateForBrowser(browser_.get());

    WebUsageEnablerBrowserAgent::FromBrowser(browser_.get())
        ->SetWebUsageEnabled(true);

    WebStateUpdateBrowserAgent::CreateForBrowser(browser_.get());

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();

    id mock_activity_service_handler =
        OCMProtocolMock(@protocol(ActivityServiceCommands));
    [dispatcher startDispatchingToTarget:mock_activity_service_handler
                             forProtocol:@protocol(ActivityServiceCommands)];
    id mock_find_in_page_handler =
        OCMProtocolMock(@protocol(FindInPageCommands));
    [dispatcher startDispatchingToTarget:mock_find_in_page_handler
                             forProtocol:@protocol(FindInPageCommands)];
    id mock_lens_handler = OCMProtocolMock(@protocol(LensCommands));
    [dispatcher startDispatchingToTarget:mock_lens_handler
                             forProtocol:@protocol(LensCommands)];
    id mock_text_zoom_handler = OCMProtocolMock(@protocol(TextZoomCommands));
    [dispatcher startDispatchingToTarget:mock_text_zoom_handler
                             forProtocol:@protocol(TextZoomCommands)];
    id mock_page_info_handler = OCMProtocolMock(@protocol(PageInfoCommands));
    [dispatcher startDispatchingToTarget:mock_page_info_handler
                             forProtocol:@protocol(PageInfoCommands)];
    id mock_qr_scanner_handler = OCMProtocolMock(@protocol(QRScannerCommands));
    [dispatcher startDispatchingToTarget:mock_qr_scanner_handler
                             forProtocol:@protocol(QRScannerCommands)];
    id mock_snackbar_handler = OCMProtocolMock(@protocol(SnackbarCommands));
    [dispatcher startDispatchingToTarget:mock_snackbar_handler
                             forProtocol:@protocol(SnackbarCommands)];
    id mock_contextual_sheet_handler =
        OCMProtocolMock(@protocol(ContextualSheetCommands));
    [dispatcher startDispatchingToTarget:mock_contextual_sheet_handler
                             forProtocol:@protocol(ContextualSheetCommands)];
    id mock_contextual_panel_entrypoint_iph_handler =
        OCMProtocolMock(@protocol(ContextualPanelEntrypointIPHCommands));
    [dispatcher
        startDispatchingToTarget:mock_contextual_panel_entrypoint_iph_handler
                     forProtocol:@protocol(
                                     ContextualPanelEntrypointIPHCommands)];
    id mock_browser_coordinator_handler =
        OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
    [dispatcher startDispatchingToTarget:mock_browser_coordinator_handler
                             forProtocol:@protocol(BrowserCoordinatorCommands)];

    id mock_help_handler = OCMProtocolMock(@protocol(HelpCommands));
    [dispatcher startDispatchingToTarget:mock_help_handler
                             forProtocol:@protocol(HelpCommands)];

    id page_action_menu_handler =
        OCMProtocolMock(@protocol(PageActionMenuCommands));
    [dispatcher startDispatchingToTarget:page_action_menu_handler
                             forProtocol:@protocol(PageActionMenuCommands)];

    id bwg_handler = OCMProtocolMock(@protocol(BWGCommands));
    [dispatcher startDispatchingToTarget:bwg_handler
                             forProtocol:@protocol(BWGCommands)];

    // Set up Applicationhander and SettingsHandler mocks.
    mock_application_handler_ = OCMProtocolMock(@protocol(ApplicationCommands));
    id mock_settings_handler = OCMProtocolMock(@protocol(SettingsCommands));
    [dispatcher startDispatchingToTarget:mock_application_handler_
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher startDispatchingToTarget:mock_settings_handler
                             forProtocol:@protocol(SettingsCommands)];

    id mock_quick_delete_handler =
        OCMProtocolMock(@protocol(QuickDeleteCommands));
    [dispatcher startDispatchingToTarget:mock_quick_delete_handler
                             forProtocol:@protocol(QuickDeleteCommands)];

    // Create three web states.
    for (int i = 0; i < 3; i++) {
      web::WebState::CreateParams params(GetProfile());
      std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
      AttachTabHelpers(webState.get());
      browser_->GetWebStateList()->InsertWebState(std::move(webState));
      browser_->GetWebStateList()->ActivateWebStateAt(0);
    }

    shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForProfile(GetProfile()));
    shopping_service_->SetIsShoppingListEligible(true);

    // Load TemplateURLService.
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(GetProfile());
    template_url_service->Load();

    clipboard_installer_ =
        std::make_unique<ScopedClipboardRecentContentInstaller>(
            std::make_unique<FakeClipboardRecentContent>());

    container_ = [[BrowserContainerViewController alloc] init];
    key_commands_provider_ =
        [[KeyCommandsProvider alloc] initWithBrowser:browser_.get()];
    safe_area_provider_ =
        [[SafeAreaProvider alloc] initWithBrowser:browser_.get()];

    popup_menu_coordinator_ =
        [[PopupMenuCoordinator alloc] initWithBrowser:browser_.get()];
    [popup_menu_coordinator_ start];

    toolbar_coordinator_ =
        [[ToolbarCoordinator alloc] initWithBrowser:browser_.get()];
    [toolbar_coordinator_ start];

    tab_strip_coordinator_ =
        [[TabStripCoordinator alloc] initWithBrowser:browser_.get()];

    fullscreen_controller_ = FullscreenController::FromBrowser(browser_.get());
    side_swipe_coordinator_ = [[SideSwipeCoordinator alloc]
        initWithBaseViewController:nil
                           browser:browser_.get()];

    bookmarks_coordinator_ =
        [[BookmarksCoordinator alloc] initWithBrowser:browser_.get()];

    tab_usage_recorder_browser_agent_ =
        TabUsageRecorderBrowserAgent::FromBrowser(browser_.get());
    NTPCoordinator_ = [[NewTabPageCoordinator alloc]
         initWithBrowser:browser_.get()
        componentFactory:[[NewTabPageComponentFactory alloc] init]];
    NTPCoordinator_.toolbarDelegate =
        OCMProtocolMock(@protocol(NewTabPageControllerDelegate));

    BrowserViewControllerDependencies dependencies;
    dependencies.popupMenuCoordinator = popup_menu_coordinator_;
    dependencies.toolbarCoordinator = toolbar_coordinator_;
    dependencies.tabStripCoordinator = tab_strip_coordinator_;
    dependencies.sideSwipeCoordinator = side_swipe_coordinator_;
    dependencies.bookmarksCoordinator = bookmarks_coordinator_;
    dependencies.fullscreenController = fullscreen_controller_;
    dependencies.tabUsageRecorderBrowserAgent =
        tab_usage_recorder_browser_agent_;
    dependencies.layoutGuideCenter =
        LayoutGuideCenterForBrowser(browser_.get());
    dependencies.webStateList = browser_->GetWebStateList()->AsWeakPtr();
    dependencies.safeAreaProvider = safe_area_provider_;
    dependencies.applicationCommandsHandler = mock_application_handler_;
    dependencies.ntpCoordinator = NTPCoordinator_;

    bvc_ = [[BrowserViewController alloc]
        initWithBrowserContainerViewController:container_
                           keyCommandsProvider:key_commands_provider_
                                  dependencies:dependencies];
    bvc_.webUsageEnabled = YES;

    id mockReauthHandler = OCMProtocolMock(@protocol(IncognitoReauthCommands));
    bvc_.reauthHandler = mockReauthHandler;

    UrlLoadingNotifierBrowserAgent* url_loading_notifier =
        UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get());
    tab_events_mediator_ = [[TabEventsMediator alloc]
        initWithWebStateList:browser_.get()->GetWebStateList()
              ntpCoordinator:NTPCoordinator_
                     tracker:feature_engagement::TrackerFactory::GetForProfile(
                                 GetProfile())
                   incognito:NO
             loadingNotifier:url_loading_notifier];
    tab_events_mediator_.consumer = bvc_;

    // Force the view to load.
    UIWindow* window = [[UIWindow alloc] initWithFrame:CGRectZero];
    window.rootViewController = bvc_;
    [window makeKeyAndVisible];
    window_ = window;

    UIView.animationsEnabled = YES;
  }

  void TearDown() override {
    [tab_events_mediator_ disconnect];
    window_.rootViewController = nil;
    [[bvc_ view] removeFromSuperview];
    [bvc_ shutdown];
    [bookmarks_coordinator_ stop];
    [tab_strip_coordinator_ stop];
    [toolbar_coordinator_ stop];
    [popup_menu_coordinator_ stop];
    [NTPCoordinator_ stop];
    [side_swipe_coordinator_ stop];

    BlockCleanupTest::TearDown();
  }

  TestProfileIOS* GetProfile() { return profile_.get(); }

  web::WebState* ActiveWebState() {
    return browser_->GetWebStateList()->GetActiveWebState();
  }

  void InsertWebState(std::unique_ptr<web::WebState> web_state) {
    WebStateList* web_state_list = browser_->GetWebStateList();
    web_state_list->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  std::unique_ptr<web::WebState> CreateWebState() {
    web::WebState::CreateParams params(GetProfile());
    auto web_state = web::WebState::Create(params);
    AttachTabHelpers(web_state.get());
    return web_state;
  }

  std::unique_ptr<web::WebState> CreateOffTheRecordWebState() {
    web::WebState::CreateParams params(
        GetProfile()->CreateOffTheRecordProfileWithTestingFactories(
            {TestProfileIOS::TestingFactory{
                TipsManagerIOSFactory::GetInstance(),
                TipsManagerIOSFactory::GetDefaultFactory()}}));
    auto web_state = web::WebState::Create(params);
    AttachTabHelpers(web_state.get());
    return web_state;
  }

  // Fakes loading the NTP for a given `web_state`.
  void LoadNTP(web::WebState* web_state) {
    web::FakeWebState fake_web_state;
    fake_web_state.SetVisibleURL(GURL("chrome://newtab/"));
    // Use the fake_web_state to fake the NTPHelper into believing that the NTP
    // has been loaded.
    NewTabPageTabHelper::FromWebState(web_state)->PageLoaded(
        &fake_web_state, web::PageLoadCompletionStatus::SUCCESS);
  }

  void ExpectNewTabInsertionAnimation(bool animated, ProceduralBlock block) {
    id mock_animation_view_class =
        OCMClassMock([ForegroundTabAnimationView class]);

    if (animated) {
      OCMExpect([mock_animation_view_class alloc]).andReturn(nil);
    } else {
      [[mock_animation_view_class reject] alloc];
    }

    block();

    EXPECT_OCMOCK_VERIFY(mock_animation_view_class);
    [mock_animation_view_class stopMocking];
  }

  // Used as an OCMArg to check that the argument's `contentView` matches the
  // return value of the given `expected_view` block.
  id OCMArgWithContentView(UIView* (^expected_view)()) {
    return [OCMArg checkWithBlock:^(UIView* view) {
      UIView* content_view =
          base::apple::ObjCCast<ForegroundTabAnimationView>(view).contentView;
      return content_view == expected_view();
    }];
  }

  MOCK_METHOD0(OnCompletionCalled, void());

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ScopedClipboardRecentContentInstaller> clipboard_installer_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  KeyCommandsProvider* key_commands_provider_;
  BrowserContainerViewController* container_;
  BrowserViewController* bvc_;
  UIWindow* window_;
  SceneState* scene_state_;
  raw_ptr<commerce::MockShoppingService> shopping_service_;
  PopupMenuCoordinator* popup_menu_coordinator_;
  ToolbarCoordinator* toolbar_coordinator_;
  TabStripCoordinator* tab_strip_coordinator_;
  SideSwipeCoordinator* side_swipe_coordinator_;
  BookmarksCoordinator* bookmarks_coordinator_;
  raw_ptr<FullscreenController> fullscreen_controller_;
  TabEventsMediator* tab_events_mediator_;
  NewTabPageCoordinator* NTPCoordinator_;
  raw_ptr<TabUsageRecorderBrowserAgent> tab_usage_recorder_browser_agent_;
  SafeAreaProvider* safe_area_provider_;
  id mock_application_handler_;
};

TEST_F(BrowserViewControllerTest, TestWebStateSelected) {
  [bvc_ webStateSelected];
  EXPECT_EQ(ActiveWebState()->GetView().superview, container_.view);
  EXPECT_TRUE(ActiveWebState()->IsVisible());
}

TEST_F(BrowserViewControllerTest, TestClearPresentedState) {
  EXPECT_CALL(*this, OnCompletionCalled());
  [bvc_
      clearPresentedStateWithCompletion:^{
        this->OnCompletionCalled();
      }
                         dismissOmnibox:YES];
}

// Tests that WebState::WasShown() and WebState::WasHidden() is properly called
// for WebState activations in the BrowserViewController's WebStateList.
TEST_F(BrowserViewControllerTest, UpdateWebStateVisibility) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(3, web_state_list->count());

  // Activate each WebState in the list and check the visibility.
  web_state_list->ActivateWebStateAt(0);
  EXPECT_EQ(web_state_list->GetWebStateAt(0)->IsVisible(), true);
  EXPECT_EQ(web_state_list->GetWebStateAt(1)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(2)->IsVisible(), false);
  web_state_list->ActivateWebStateAt(1);
  EXPECT_EQ(web_state_list->GetWebStateAt(0)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(1)->IsVisible(), true);
  EXPECT_EQ(web_state_list->GetWebStateAt(2)->IsVisible(), false);
  web_state_list->ActivateWebStateAt(2);
  EXPECT_EQ(web_state_list->GetWebStateAt(0)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(1)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(2)->IsVisible(), true);
}

TEST_F(BrowserViewControllerTest, didInsertWebStateWithAnimation) {
  // Animation is only expected on iPhone, not iPad.
  bool animation_expected =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
  ExpectNewTabInsertionAnimation(animation_expected, ^{
    auto web_state = CreateWebState();
    InsertWebState(std::move(web_state));
  });
}

TEST_F(BrowserViewControllerTest, didInsertWebStateWithoutAnimation) {
  ExpectNewTabInsertionAnimation(false, ^{
    auto web_state = CreateWebState();
    NewTabAnimationTabHelper::CreateForWebState(web_state.get());
    NewTabAnimationTabHelper::FromWebState(web_state.get())
        ->DisableNewTabAnimation();
    InsertWebState(std::move(web_state));
  });
}

TEST_F(BrowserViewControllerTest,
       presentIncognitoReauthDismissesPresentedState) {
  // Add a presented VC so dismiss modal dialogs is dispatched.
  [bvc_ presentViewController:[[UIViewController alloc] init]
                     animated:NO
                   completion:nil];

  OCMExpect([mock_application_handler_ dismissModalDialogsWithCompletion:nil]);

  // Present incognito authentication must dismiss presented state.
  [bvc_ setItemsRequireAuthentication:YES];

  // Verify that the command was dispatched.
  EXPECT_OCMOCK_VERIFY(mock_application_handler_);
}

// Tests that an off-the-record web state can be created and inserted in the
// browser view controller.
TEST_F(BrowserViewControllerTest, didInsertOffTheRecordWebState) {
  // The animation being tested only runs on the phone form factor.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  id container_view_mock = OCMPartialMock(container_.view);

  // Add an off-the-record web state with supported tab helpers.
  std::unique_ptr<web::WebState> web_state = CreateOffTheRecordWebState();
  UIView* web_state_view = web_state->GetView();

  // Insert in browser web state list.
  [[container_view_mock expect] addSubview:OCMArgWithContentView(^{
                                  return web_state_view;
                                })];
  InsertWebState(std::move(web_state));
  EXPECT_OCMOCK_VERIFY(container_view_mock);
}

// Tests that when a webstate is inserted, the correct view is used during
// the animation.
TEST_F(BrowserViewControllerTest, ViewOnInsert) {
  // The animation being tested only runs on the phone form factor.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  id container_view_mock = OCMPartialMock(container_.view);

  // When inserting a non-ntp WebState, the WebState's view should be animated.
  std::unique_ptr<web::WebState> web_state = CreateWebState();
  UIView* web_state_view = web_state->GetView();
  [[container_view_mock expect] addSubview:OCMArgWithContentView(^{
                                  return web_state_view;
                                })];
  InsertWebState(std::move(web_state));
  EXPECT_OCMOCK_VERIFY(container_view_mock);

  // When inserting an NTP WebState, the NTP's view should be animated.
  std::unique_ptr<web::WebState> ntp_web_state = CreateWebState();
  LoadNTP(ntp_web_state.get());
  [[container_view_mock expect] addSubview:OCMArgWithContentView(^{
                                  return NTPCoordinator_.viewController.view;
                                })];
  InsertWebState(std::move(ntp_web_state));
  EXPECT_OCMOCK_VERIFY(container_view_mock);

  // When inserting a second NTP WebState, the NTP's view should be animated.
  // In this case the NTP is already started, so we want to ensure that the
  // correct view is used in this case too.
  std::unique_ptr<web::WebState> ntp_web_state2 = CreateWebState();
  LoadNTP(ntp_web_state2.get());
  [[container_view_mock expect] addSubview:OCMArgWithContentView(^{
                                  return NTPCoordinator_.viewController.view;
                                })];
  InsertWebState(std::move(ntp_web_state2));
  EXPECT_OCMOCK_VERIFY(container_view_mock);
}

// BrowserViewController needs to conform to
// `<LogoAnimationControllerOwnerOwner>` to support VoiceOver. Related to
// crbug.com/442767141.
TEST_F(BrowserViewControllerTest, LogoAnimationControllerOwnerOwner) {
  EXPECT_TRUE(
      [bvc_ conformsToProtocol:@protocol(LogoAnimationControllerOwnerOwner)]);
}
