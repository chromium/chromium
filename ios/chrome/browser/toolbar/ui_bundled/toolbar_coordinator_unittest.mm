// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/toolbar_coordinator.h"

#import "base/scoped_observation.h"
#import "base/test/scoped_feature_list.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent_observer.h"
#import "ios/chrome/browser/reading_list/model/offline_page_tab_helper.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
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
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_browser_agent.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_type.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {
// Sample observer for use in tests.
class TestOmniboxPositionObserver : public OmniboxPositionBrowserAgentObserver {
 public:
  void OmniboxPositionBrowserAgentHasNewBottomLayout(
      OmniboxPositionBrowserAgent* browser_agent,
      bool is_current_layout_bottom_omnibox) override {
    is_bottom_omnibox_ = is_current_layout_bottom_omnibox;
  }

  bool is_bottom_omnibox_ = false;
};
}  // namespace

// Unittests related to the ToolbarCoordinator.
class ToolbarCoordinatorTest : public PlatformTest {
 public:
  ToolbarCoordinatorTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        tab_groups::TabGroupSyncServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDefaultFactory());
    profile_ = std::move(test_profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Setup all necessary handlers.

    id mock_find_in_page_handler =
        OCMProtocolMock(@protocol(FindInPageCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_find_in_page_handler
                     forProtocol:@protocol(FindInPageCommands)];

    id mock_text_zoom_handler = OCMProtocolMock(@protocol(TextZoomCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_text_zoom_handler
                     forProtocol:@protocol(TextZoomCommands)];

    id mock_help_handler = OCMProtocolMock(@protocol(HelpCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_help_handler
                     forProtocol:@protocol(HelpCommands)];

    id mock_lens_handler = OCMProtocolMock(@protocol(LensCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_lens_handler
                     forProtocol:@protocol(LensCommands)];

    id mock_application_handler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_handler
                     forProtocol:@protocol(ApplicationCommands)];

    id mock_QR_scanner_handler = OCMProtocolMock(@protocol(QRScannerCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_QR_scanner_handler
                     forProtocol:@protocol(QRScannerCommands)];

    id mock_settings_handler = OCMProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_handler
                     forProtocol:@protocol(SettingsCommands)];

    id mock_quick_delete_handler =
        OCMProtocolMock(@protocol(QuickDeleteCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_quick_delete_handler
                     forProtocol:@protocol(QuickDeleteCommands)];

    id mock_page_action_menu_commands =
        OCMProtocolMock(@protocol(PageActionMenuCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_page_action_menu_commands
                     forProtocol:@protocol(PageActionMenuCommands)];

    id mock_bwg_commands = OCMProtocolMock(@protocol(BWGCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_bwg_commands
                     forProtocol:@protocol(BWGCommands)];

    id mock_popup_menu_handler = OCMProtocolMock(@protocol(PopupMenuCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_popup_menu_handler
                     forProtocol:@protocol(PopupMenuCommands)];

    id mock_activity_service_handler =
        OCMProtocolMock(@protocol(ActivityServiceCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_activity_service_handler
                     forProtocol:@protocol(ActivityServiceCommands)];

    id mock_contextual_sheet_handler =
        OCMProtocolMock(@protocol(ContextualSheetCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_contextual_sheet_handler
                     forProtocol:@protocol(ContextualSheetCommands)];

    id mock_contextual_panel_entrypoint_IPH_handler =
        OCMProtocolMock(@protocol(ContextualPanelEntrypointIPHCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_contextual_panel_entrypoint_IPH_handler
                     forProtocol:@protocol(
                                     ContextualPanelEntrypointIPHCommands)];

    id mock_browser_coordinator_commands =
        OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_browser_coordinator_commands
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    OmniboxPositionBrowserAgent::CreateForBrowser(browser_.get());
    AutocompleteBrowserAgent::CreateForBrowser(browser_.get());
    // FullscreenController depends on ToolbarsSizeBrowserAgent, so the agent
    // must be created first. Please maintain this order.
    ToolbarsSizeBrowserAgent::CreateForBrowser(browser_.get());
    FullscreenController::CreateForBrowser(browser_.get());
  }

  ~ToolbarCoordinatorTest() override {}

  void TearDown() override { [coordinator_ stop]; }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ToolbarCoordinator* coordinator_;
};

// Test that the OmniboxPositionBrowserAgent can be observed to tell when the
// bottom omnibox position changes.
TEST_F(ToolbarCoordinatorTest, TestOmniboxPositionBrowserAgentObservation) {
  // Bottom omnibox is not supported on all devices (e.g. iPad).
  if (!IsBottomOmniboxAvailable()) {
    return;
  }
  TestOmniboxPositionObserver observer;
  base::ScopedObservation<OmniboxPositionBrowserAgent,
                          TestOmniboxPositionObserver>
      obs{&observer};

  OmniboxPositionBrowserAgent* browser_agent =
      OmniboxPositionBrowserAgent::FromBrowser(browser_.get());
  obs.Observe(browser_agent);

  coordinator_ = [[ToolbarCoordinator alloc] initWithBrowser:browser_.get()];
  [coordinator_ start];
  EXPECT_FALSE(observer.is_bottom_omnibox_);

  // Change bottom omnibox pref.
  GetApplicationContext()->GetLocalState()->SetBoolean(
      omnibox::kIsOmniboxInBottomPosition, true);

  EXPECT_TRUE(observer.is_bottom_omnibox_);
}

// Tests that taking a side swipe snapshot of a toolbar that is not in the
// view hierarchy does not crash and returns nil.
TEST_F(ToolbarCoordinatorTest, SideSwipeSnapshotForToolbarNotInHierarchy) {
  coordinator_ = [[ToolbarCoordinator alloc] initWithBrowser:browser_.get()];
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
  OfflinePageTabHelper::CreateForWebState(
      web_state, ReadingListModelFactory::GetForProfile(profile_.get()));

  browser_->GetWebStateList()->InsertWebState(
      std::move(test_web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  // Add the coordinator's view to a window to ensure it has a window property.
  UIWindow* window = [[UIWindow alloc] init];
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
