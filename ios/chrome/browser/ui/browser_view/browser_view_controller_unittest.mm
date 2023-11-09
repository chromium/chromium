// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller+private.h"

#import <Foundation/Foundation.h>
#import <PassKit/PassKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "components/search_engines/template_url_service.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/lens/lens_browser_agent.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/ui/bookmarks/bookmarks_coordinator.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"
#import "ios/chrome/browser/ui/browser_view/safe_area_provider.h"
#import "ios/chrome/browser/ui/browser_view/tab_consumer.h"
#import "ios/chrome/browser/ui/browser_view/tab_events_mediator.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_commands.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_component_factory.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_coordinator.h"
#import "ios/chrome/browser/ui/tabs/foreground_tab_animation_view.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_legacy_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator.h"
#import "ios/chrome/browser/url_loading/model/new_tab_animation_tab_helper.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/web/page_placeholder_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/web_state_update_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
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

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

#pragma mark -

namespace {
class BrowserViewControllerTest : public BlockCleanupTest {
 public:
 protected:
  BrowserViewControllerTest() {
    scoped_feature_list_.InitWithFeatures(
        {supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS},
        {web::features::kEnableSessionSerializationOptimizations});
  }

  void SetUp() override {
    BlockCleanupTest::SetUp();

    scene_state_ = [[SceneState alloc] initWithAppState:nil];

    // Set up a TestChromeBrowserState instance.
    TestChromeBrowserState::Builder test_cbs_builder;

    test_cbs_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
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
    id passKitController =
        [OCMockObject niceMockForClass:[PKAddPassesViewController class]];
    passKitViewController_ = passKitController;

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    TabUsageRecorderBrowserAgent::CreateForBrowser(browser_.get());
    PagePlaceholderBrowserAgent::CreateForBrowser(browser_.get());

    WebUsageEnablerBrowserAgent::FromBrowser(browser_.get())
        ->SetWebUsageEnabled(true);

    SessionRestorationBrowserAgent::CreateForBrowser(
        browser_.get(), [[TestSessionService alloc] init], false);
    SessionRestorationBrowserAgent::FromBrowser(browser_.get())
        ->SetSessionID([[NSUUID UUID] UUIDString]);

    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
    WebStateUpdateBrowserAgent::CreateForBrowser(browser_.get());

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();

    id mockActivityServiceCommandHandler =
        OCMProtocolMock(@protocol(ActivityServiceCommands));
    [dispatcher startDispatchingToTarget:mockActivityServiceCommandHandler
                             forProtocol:@protocol(ActivityServiceCommands)];
    id mockFindInPageCommandHandler =
        OCMProtocolMock(@protocol(FindInPageCommands));
    [dispatcher startDispatchingToTarget:mockFindInPageCommandHandler
                             forProtocol:@protocol(FindInPageCommands)];
    id mockLensCommandHandler = OCMProtocolMock(@protocol(LensCommands));
    [dispatcher startDispatchingToTarget:mockLensCommandHandler
                             forProtocol:@protocol(LensCommands)];
    id mockTextZoomCommandHandler =
        OCMProtocolMock(@protocol(TextZoomCommands));
    [dispatcher startDispatchingToTarget:mockTextZoomCommandHandler
                             forProtocol:@protocol(TextZoomCommands)];
    id mockPageInfoCommandHandler =
        OCMProtocolMock(@protocol(PageInfoCommands));
    [dispatcher startDispatchingToTarget:mockPageInfoCommandHandler
                             forProtocol:@protocol(PageInfoCommands)];
    id mockQrScannerCommandHandler =
        OCMProtocolMock(@protocol(QRScannerCommands));
    [dispatcher startDispatchingToTarget:mockQrScannerCommandHandler
                             forProtocol:@protocol(QRScannerCommands)];
    id mockSnackbarCommandHandler =
        OCMProtocolMock(@protocol(SnackbarCommands));
    [dispatcher startDispatchingToTarget:mockSnackbarCommandHandler
                             forProtocol:@protocol(SnackbarCommands)];
    id mockParcelTrackingCommandHandler =
        OCMProtocolMock(@protocol(ParcelTrackingOptInCommands));
    [dispatcher
        startDispatchingToTarget:mockParcelTrackingCommandHandler
                     forProtocol:@protocol(ParcelTrackingOptInCommands)];

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to ApplicationSettingsCommands, that needs to be mocked and dispatched
    // as well.
    mockApplicationCommandHandler_ =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockApplicationSettingsCommandHandler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler_
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher
        startDispatchingToTarget:mockApplicationSettingsCommandHandler
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    // Create three web states.
    for (int i = 0; i < 3; i++) {
      web::WebState::CreateParams params(chrome_browser_state_.get());
      std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
      AttachTabHelpers(webState.get(), NO);
      browser_->GetWebStateList()->InsertWebState(0, std::move(webState), 0,
                                                  WebStateOpener());
      browser_->GetWebStateList()->ActivateWebStateAt(0);
    }

    // Load TemplateURLService.
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    template_url_service->Load();

    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());

    container_ = [[BrowserContainerViewController alloc] init];
    key_commands_provider_ =
        [[KeyCommandsProvider alloc] initWithBrowser:browser_.get()];
    safe_area_provider_ =
        [[SafeAreaProvider alloc] initWithBrowser:browser_.get()];

    popup_menu_coordinator_ =
        [[PopupMenuCoordinator alloc] initWithBrowser:browser_.get()];
    [popup_menu_coordinator_ start];

    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForBrowserState(
            chrome_browser_state_.get());
    HostContentSettingsMap* settings_map =
        ios::HostContentSettingsMapFactory::GetForBrowserState(
            chrome_browser_state_.get());
    UrlLoadingNotifierBrowserAgent* urlLoadingNotifier_ =
        UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get());

    bubble_presenter_ = [[BubblePresenter alloc]
        initWithDeviceSwitcherResultDispatcher:nullptr
                        hostContentSettingsMap:(HostContentSettingsMap*)
                                                   settings_map
                               loadingNotifier:urlLoadingNotifier_
                                   prefService:chrome_browser_state_.get()
                                                   ->GetPrefs()
                                    sceneState:scene_state_
                       tabStripCommandsHandler:nil
                                       tracker:(feature_engagement::Tracker*)
                                                   tracker
                                  webStateList:browser_->GetWebStateList()];
    [dispatcher startDispatchingToTarget:bubble_presenter_
                             forProtocol:@protocol(HelpCommands)];

    toolbar_coordinator_ =
        [[ToolbarCoordinator alloc] initWithBrowser:browser_.get()];
    [toolbar_coordinator_ start];

    tab_strip_coordinator_ =
        [[TabStripCoordinator alloc] initWithBrowser:browser_.get()];

    legacy_tab_strip_coordinator_ =
        [[TabStripLegacyCoordinator alloc] initWithBrowser:browser_.get()];

    fullscreen_controller_ = FullscreenController::FromBrowser(browser_.get());
    SnapshotBrowserAgent* snapshot_browser_agent =
        SnapshotBrowserAgent::FromBrowser(browser_.get());
    side_swipe_mediator_ = [[SideSwipeMediator alloc]
        initWithFullscreenController:fullscreen_controller_
                snapshotBrowserAgent:snapshot_browser_agent
                        webStateList:browser_->GetWebStateList()];

    bookmarks_coordinator_ =
        [[BookmarksCoordinator alloc] initWithBrowser:browser_.get()];

    url_loading_notifier_browser_agent_ =
        UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get());

    tab_usage_recorder_browser_agent_ =
        TabUsageRecorderBrowserAgent::FromBrowser(browser_.get());
    page_placeholder_browser_agent_ =
        PagePlaceholderBrowserAgent::FromBrowser(browser_.get());
    NTPCoordinator_ = [[NewTabPageCoordinator alloc]
         initWithBrowser:browser_.get()
        componentFactory:[[NewTabPageComponentFactory alloc] init]];
    NTPCoordinator_.toolbarDelegate =
        OCMProtocolMock(@protocol(NewTabPageControllerDelegate));

    BrowserViewControllerDependencies dependencies;
    dependencies.bubblePresenter = bubble_presenter_;
    dependencies.popupMenuCoordinator = popup_menu_coordinator_;
    dependencies.toolbarCoordinator = toolbar_coordinator_;
    dependencies.tabStripCoordinator = tab_strip_coordinator_;
    dependencies.legacyTabStripCoordinator = legacy_tab_strip_coordinator_;
    dependencies.sideSwipeMediator = side_swipe_mediator_;
    dependencies.bookmarksCoordinator = bookmarks_coordinator_;
    dependencies.fullscreenController = fullscreen_controller_;
    dependencies.urlLoadingNotifierBrowserAgent =
        url_loading_notifier_browser_agent_;
    dependencies.tabUsageRecorderBrowserAgent =
        tab_usage_recorder_browser_agent_;
    dependencies.layoutGuideCenter =
        LayoutGuideCenterForBrowser(browser_.get());
    dependencies.webStateList = browser_->GetWebStateList()->AsWeakPtr();
    dependencies.safeAreaProvider = safe_area_provider_;
    dependencies.pagePlaceholderBrowserAgent = page_placeholder_browser_agent_;
    dependencies.applicationCommandsHandler = mockApplicationCommandHandler_;
    dependencies.ntpCoordinator = NTPCoordinator_;

    bvc_ = [[BrowserViewController alloc]
        initWithBrowserContainerViewController:container_
                           keyCommandsProvider:key_commands_provider_
                                  dependencies:dependencies];
    bvc_.webUsageEnabled = YES;

    id mockReauthHandler = OCMProtocolMock(@protocol(IncognitoReauthCommands));
    bvc_.reauthHandler = mockReauthHandler;

    tab_events_mediator_ = [[TabEventsMediator alloc]
        initWithWebStateList:browser_.get()->GetWebStateList()
              ntpCoordinator:NTPCoordinator_
                browserState:chrome_browser_state_.get()
             loadingNotifier:urlLoadingNotifier_];
    tab_events_mediator_.consumer = bvc_;

    // Force the view to load.
    UIWindow* window = [[UIWindow alloc] initWithFrame:CGRectZero];
    [window addSubview:[bvc_ view]];
    window_ = window;
  }

  void TearDown() override {
    [tab_events_mediator_ disconnect];
    [[bvc_ view] removeFromSuperview];
    [bvc_ shutdown];
    [bookmarks_coordinator_ stop];
    [tab_strip_coordinator_ stop];
    [popup_menu_coordinator_ stop];
    [NTPCoordinator_ stop];
    [side_swipe_mediator_ disconnect];
    [bubble_presenter_ stop];

    BlockCleanupTest::TearDown();
  }

  web::WebState* ActiveWebState() {
    return browser_->GetWebStateList()->GetActiveWebState();
  }

  void InsertWebState(std::unique_ptr<web::WebState> web_state) {
    WebStateList* web_state_list = browser_->GetWebStateList();
    web_state_list->InsertWebState(0, std::move(web_state),
                                   WebStateList::INSERT_ACTIVATE,
                                   WebStateOpener());
  }

  std::unique_ptr<web::WebState> CreateWebState() {
    web::WebState::CreateParams params(chrome_browser_state_.get());
    auto web_state = web::WebState::Create(params);
    AttachTabHelpers(web_state.get(), NO);
    return web_state;
  }

  std::unique_ptr<web::WebState> CreateOffTheRecordWebState() {
    web::WebState::CreateParams params(
        chrome_browser_state_
            ->CreateOffTheRecordBrowserStateWithTestingFactories({}));
    auto web_state = web::WebState::Create(params);
    AttachTabHelpers(web_state.get(), NO);
    return web_state;
  }

  // Fakes loading the NTP for a given `web_state`.
  void LoadNTP(web::WebState* web_state) {
    web::FakeWebState fake_web_state;
    fake_web_state.SetVisibleURL(GURL("chrome://newtab/"));
    web::WebStateObserver* NTPHelper =
        (web::WebStateObserver*)NewTabPageTabHelper::FromWebState(web_state);
    // Use the fake_web_state to fake the NTPHelper into believing that the NTP
    // has been loaded.
    NTPHelper->PageLoaded(&fake_web_state,
                          web::PageLoadCompletionStatus::SUCCESS);
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

    [mock_animation_view_class verify];
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
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  KeyCommandsProvider* key_commands_provider_;
  PKAddPassesViewController* passKitViewController_;
  OCMockObject* dependencyFactory_;
  CommandDispatcher* command_dispatcher_;
  BubblePresenter* bubble_presenter_;
  BrowserContainerViewController* container_;
  BrowserViewController* bvc_;
  UIWindow* window_;
  SceneState* scene_state_;
  PopupMenuCoordinator* popup_menu_coordinator_;
  ToolbarCoordinator* toolbar_coordinator_;
  TabStripCoordinator* tab_strip_coordinator_;
  TabStripLegacyCoordinator* legacy_tab_strip_coordinator_;
  SideSwipeMediator* side_swipe_mediator_;
  BookmarksCoordinator* bookmarks_coordinator_;
  FullscreenController* fullscreen_controller_;
  TabEventsMediator* tab_events_mediator_;
  NewTabPageCoordinator* NTPCoordinator_;
  UrlLoadingNotifierBrowserAgent* url_loading_notifier_browser_agent_;
  TabUsageRecorderBrowserAgent* tab_usage_recorder_browser_agent_;
  SafeAreaProvider* safe_area_provider_;
  PagePlaceholderBrowserAgent* page_placeholder_browser_agent_;
  id mockApplicationCommandHandler_;
  base::test::ScopedFeatureList scoped_feature_list_;
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

  OCMExpect(
      [mockApplicationCommandHandler_ dismissModalDialogsWithCompletion:nil]);

  // Present incognito authentication must dismiss presented state.
  [bvc_ setItemsRequireAuthentication:YES];

  // Verify that the command was dispatched.
  EXPECT_OCMOCK_VERIFY(mockApplicationCommandHandler_);
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
  [NTPCoordinator_ stop];
}

}  // namespace
