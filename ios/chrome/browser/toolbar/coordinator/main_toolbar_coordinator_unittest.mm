// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/main_toolbar_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/test/scoped_feature_list.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/omnibox/model/omnibox_focus/omnibox_focus_browser_agent.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/browser_layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/fullscreen/toolbars_size_browser_agent.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/legacy_toolbar_mediator.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_type.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Exposes `ToolbarMediatorDelegate` in order to invoke
// `transitionOmniboxToToolbarType:`.
@interface MainToolbarCoordinator (Testing) <ToolbarMediatorDelegate>
@property(nonatomic, strong, readonly)
    LegacyToolbarMediator* legacyToolbarMediator;
@end

@interface TestBrowserLayoutStateObserver
    : NSObject <BrowserLayoutStateObserver>
@property(nonatomic, assign) ToolbarPosition toolbarPosition;
@property(nonatomic, assign) BOOL positionChangedCalled;
@end

@implementation TestBrowserLayoutStateObserver
- (void)browserLayoutState:(BrowserLayoutState*)layoutState
    didChangeToolbarPosition:(ToolbarPosition)toolbarPosition {
  _toolbarPosition = toolbarPosition;
  _positionChangedCalled = YES;
}
@end


// Unittests related to the MainToolbarCoordinator.
class MainToolbarCoordinatorTest : public PlatformTest {
 public:
  MainToolbarCoordinatorTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        tab_groups::TabGroupSyncServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(test_profile_builder).Build();
    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    LayoutGuideSceneAgent* layout_guide_scene_agent =
        [[LayoutGuideSceneAgent alloc] init];
    [scene_state_ addAgent:layout_guide_scene_agent];

    // Setup all necessary handlers.
    Browser* browser =
        scene_state_.browserProviderInterface.currentBrowserProvider.browser;

    id mock_find_in_page_handler =
        OCMProtocolMock(@protocol(FindInPageCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_find_in_page_handler
                     forProtocol:@protocol(FindInPageCommands)];

    id mock_text_zoom_handler = OCMProtocolMock(@protocol(TextZoomCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_text_zoom_handler
                     forProtocol:@protocol(TextZoomCommands)];

    id mock_help_handler = OCMProtocolMock(@protocol(HelpCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_help_handler
                     forProtocol:@protocol(HelpCommands)];

    id mock_lens_handler = OCMProtocolMock(@protocol(LensCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_lens_handler
                     forProtocol:@protocol(LensCommands)];

    id mock_application_handler = OCMProtocolMock(@protocol(SceneCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_handler
                     forProtocol:@protocol(SceneCommands)];

    id mock_QR_scanner_handler = OCMProtocolMock(@protocol(QRScannerCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_QR_scanner_handler
                     forProtocol:@protocol(QRScannerCommands)];

    id mock_settings_handler = OCMProtocolMock(@protocol(SettingsCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_handler
                     forProtocol:@protocol(SettingsCommands)];

    id mock_quick_delete_handler =
        OCMProtocolMock(@protocol(QuickDeleteCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_quick_delete_handler
                     forProtocol:@protocol(QuickDeleteCommands)];

    id mock_page_action_menu_commands =
        OCMProtocolMock(@protocol(PageActionMenuCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_page_action_menu_commands
                     forProtocol:@protocol(PageActionMenuCommands)];

    id mock_gemini_commands = OCMProtocolMock(@protocol(GeminiCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_gemini_commands
                     forProtocol:@protocol(GeminiCommands)];

    id mock_popup_menu_handler = OCMProtocolMock(@protocol(PopupMenuCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_popup_menu_handler
                     forProtocol:@protocol(PopupMenuCommands)];

    id mock_activity_service_handler =
        OCMProtocolMock(@protocol(ActivityServiceCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_activity_service_handler
                     forProtocol:@protocol(ActivityServiceCommands)];

    id mock_contextual_sheet_handler =
        OCMProtocolMock(@protocol(ContextualSheetCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_contextual_sheet_handler
                     forProtocol:@protocol(ContextualSheetCommands)];

    id mock_contextual_panel_entrypoint_IPH_handler =
        OCMProtocolMock(@protocol(ContextualPanelEntrypointIPHCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_contextual_panel_entrypoint_IPH_handler
                     forProtocol:@protocol(
                                     ContextualPanelEntrypointIPHCommands)];

    id mock_browser_coordinator_commands =
        OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_browser_coordinator_commands
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    id mock_fullscreen_commands =
        OCMProtocolMock(@protocol(FullscreenCommands));
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:mock_fullscreen_commands
                     forProtocol:@protocol(FullscreenCommands)];

    OmniboxFocusBrowserAgent::CreateForBrowser(browser);
    AutocompleteBrowserAgent::CreateForBrowser(browser);
    // FullscreenController depends on ToolbarsSizeBrowserAgent, so the agent
    ToolbarsSizeBrowserAgent::CreateForBrowser(browser);
    FullscreenController::CreateForBrowser(browser);
    FullscreenBrowserAgent::CreateForBrowser(browser);
  }

  ~MainToolbarCoordinatorTest() override {}

  void TearDown() override {
    [coordinator_ stop];
    [scene_state_ shutdown];
  }

  // Verifies that observing `BrowserLayoutState` correctly detects when the
  // omnibox position transitions from top to bottom.
  void VerifyOmniboxPositionObservation() {
    TestBrowserLayoutStateObserver* observer =
        [[TestBrowserLayoutStateObserver alloc] init];
    BrowserLayoutState* browserLayoutState = browser()->GetBrowserLayoutState();
    [browserLayoutState addObserver:observer];

    coordinator_ = [[MainToolbarCoordinator alloc] initWithBrowser:browser()];
    [coordinator_ start];
    UIWindow* window = [[UIWindow alloc]
        initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
    window.rootViewController = coordinator_.primaryToolbarViewController;
    UITraitCollection* compact_regular_traits = [window.traitCollection
        traitCollectionByModifyingTraits:^(id<UIMutableTraits> traits) {
          traits.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
          traits.verticalSizeClass = UIUserInterfaceSizeClassRegular;
        }];
    [coordinator_.legacyToolbarMediator
        toolbarTraitCollectionChangedTo:compact_regular_traits];
    EXPECT_FALSE(observer.positionChangedCalled);
    EXPECT_EQ(browserLayoutState.toolbarPosition, ToolbarPosition::kTop);

    // Change bottom omnibox pref.
    GetApplicationContext()->GetLocalState()->SetBoolean(
        omnibox::kIsOmniboxInBottomPosition, true);

    EXPECT_TRUE(observer.positionChangedCalled);
    EXPECT_EQ(observer.toolbarPosition, ToolbarPosition::kBottom);

    [browserLayoutState removeObserver:observer];
  }

  Browser* browser() {
    return scene_state_.browserProviderInterface.currentBrowserProvider.browser;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  MainToolbarCoordinator* coordinator_;
  FakeSceneState* scene_state_;
  base::test::ScopedFeatureList feature_list_;
};

// Test that the BrowserLayoutState can be observed to tell when the
// bottom omnibox position changes.
TEST_F(MainToolbarCoordinatorTest, TestLayoutStateToolbarPositionObservation) {
  // Bottom omnibox is not supported on all devices (e.g. iPad).
  if (!IsBottomOmniboxAvailable()) {
    return;
  }
  VerifyOmniboxPositionObservation();
}

// Test that the BrowserLayoutState can be observed to tell when the
// bottom omnibox position changes when ChromeNextIa is enabled.
TEST_F(MainToolbarCoordinatorTest,
       TestLayoutStateToolbarPositionObservation_ChromeNextIa) {
  // Bottom omnibox is not supported on all devices (e.g. iPad).
  if (!IsBottomOmniboxAvailable()) {
    return;
  }
  feature_list_.InitAndEnableFeature(kChromeNextIa);
  VerifyOmniboxPositionObservation();
}

// Tests that during early app launch when activeBrowser is nil, transitioning
// the omnibox position to bottom updates the shared BrowserLayoutState.
TEST_F(MainToolbarCoordinatorTest,
       TransitionOmniboxToToolbarTypeDuringEarlyStartup) {
  if (!IsBottomOmniboxAvailable()) {
    return;
  }
  Browser* targetBrowser = browser();
  coordinator_ = [[MainToolbarCoordinator alloc] initWithBrowser:targetBrowser];
  [coordinator_ start];

  // Simulate early startup where currentBrowserProvider.browser is nil.
  [scene_state_ setCurrentBrowserProvider:nil];

  [coordinator_ transitionOmniboxToToolbarType:ToolbarType::kSecondary];
  EXPECT_EQ(targetBrowser->GetBrowserLayoutState().toolbarPosition,
            ToolbarPosition::kBottom);
}

// Tests that taking a side swipe snapshot of a toolbar that is not in the
// view hierarchy does not crash and returns nil.
TEST_F(MainToolbarCoordinatorTest, SideSwipeSnapshotForToolbarNotInHierarchy) {
  coordinator_ = [[MainToolbarCoordinator alloc] initWithBrowser:browser()];
  [coordinator_ start];
  // Add a dummy WebState to the WebStateList.
  auto test_web_state = std::make_unique<web::FakeWebState>();
  test_web_state->SetBrowserState(profile_.get());
  test_web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());

  // Add required web state service dependencies.
  web::WebState* web_state = test_web_state.get();
  WebViewProxyTabHelper::CreateForWebState(web_state);
  InfoBarManagerImpl::CreateForWebState(web_state);
  InfobarBadgeTabHelper::CreateForWebState(web_state);

  browser()->GetWebStateList()->InsertWebState(
      std::move(test_web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  // Add the coordinator's view to a window to ensure it has a window property.
  UIWindow* window = [[UIWindow alloc]
      initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
  [window addSubview:coordinator_.baseViewController.view];

  // Remove the primary toolbar from the view hierarchy to simulate the race
  // condition.
  [coordinator_.primaryToolbarViewController.view removeFromSuperview];

  // Attempt to take a snapshot. This should not crash.
  UIImage* snapshot =
      [coordinator_ toolbarSideSwipeSnapshotForWebState:web_state
                                        withToolbarType:ToolbarType::kPrimary];

  // The snapshot should be nil because the view is not in the hierarchy.
  EXPECT_EQ(snapshot, nil);
}
