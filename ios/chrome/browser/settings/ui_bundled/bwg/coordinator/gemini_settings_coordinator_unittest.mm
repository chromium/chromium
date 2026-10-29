// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_mutator.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_suggestions_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_settings_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_suggestions_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class GeminiSettingsCoordinatorTest : public PlatformTest {
 protected:
  GeminiSettingsCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::MockSyncService>();
            }));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_);
    scene_handler_ = OCMStrictProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:scene_handler_
                     forProtocol:@protocol(SceneCommands)];
  }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  id scene_handler_ = nil;
};

// Tests that settingsViewControllerDidRequestDismissal: calls
// closeSettings on SettingsNavigationController.
TEST_F(GeminiSettingsCoordinatorTest,
       SettingsViewControllerDidRequestDismissal) {
  id base_navigation_controller =
      OCMPartialMock([[SettingsNavigationController alloc] init]);

  GeminiSettingsCoordinator* coordinator = [[GeminiSettingsCoordinator alloc]
      initWithBaseNavigationController:base_navigation_controller
                               browser:browser_.get()];

  OCMExpect([base_navigation_controller closeSettings]);

  id<GeminiSettingsDismissalDelegate> dismissalDelegate =
      static_cast<id<GeminiSettingsDismissalDelegate>>(coordinator);
  [dismissalDelegate settingsViewControllerDidRequestDismissal:nil];

  EXPECT_OCMOCK_VERIFY(base_navigation_controller);
}

// Tests that openSyncSettings does not push sync settings when user is signed
// out.
TEST_F(GeminiSettingsCoordinatorTest, OpenSyncSettingsSignedOut) {
  id base_navigation_controller =
      OCMPartialMock([[SettingsNavigationController alloc] init]);

  GeminiSuggestionsCoordinator* coordinator =
      [[GeminiSuggestionsCoordinator alloc]
          initWithBaseNavigationController:base_navigation_controller
                                   browser:browser_.get()];

  OCMReject([base_navigation_controller pushViewController:[OCMArg any]
                                                  animated:YES]);

  id<GeminiSettingsMediatorDelegate> mediatorDelegate =
      static_cast<id<GeminiSettingsMediatorDelegate>>(coordinator);
  [mediatorDelegate openSyncSettings];

  EXPECT_OCMOCK_VERIFY(base_navigation_controller);
}

// Test that starting `GeminiSuggestionsCoordinator` pushes
// `GeminiSuggestionsViewController`.
TEST_F(GeminiSettingsCoordinatorTest, StartSuggestionsCoordinator) {
  id base_navigation_controller =
      OCMPartialMock([[SettingsNavigationController alloc] init]);

  GeminiSuggestionsCoordinator* coordinator =
      [[GeminiSuggestionsCoordinator alloc]
          initWithBaseNavigationController:base_navigation_controller
                                   browser:browser_.get()];

  OCMExpect([base_navigation_controller
      pushViewController:[OCMArg isKindOfClass:[GeminiSuggestionsViewController
                                                   class]]
                animated:YES]);

  [coordinator start];

  EXPECT_OCMOCK_VERIFY(base_navigation_controller);
  [coordinator stop];
}

// Test that selecting the suggestions row in `GeminiSettingsViewController`
// starts `GeminiSuggestionsCoordinator` and pushes
// `GeminiSuggestionsViewController`.
TEST_F(GeminiSettingsCoordinatorTest,
       DidSelectSuggestionsStartsSuggestionsCoordinator) {
  id base_navigation_controller =
      OCMPartialMock([[SettingsNavigationController alloc] init]);

  GeminiSettingsCoordinator* coordinator = [[GeminiSettingsCoordinator alloc]
      initWithBaseNavigationController:base_navigation_controller
                               browser:browser_.get()];

  __block GeminiSettingsViewController* settings_view_controller = nil;
  OCMExpect([base_navigation_controller
      pushViewController:[OCMArg checkWithBlock:^BOOL(id view_controller) {
        if ([view_controller
                isKindOfClass:[GeminiSettingsViewController class]]) {
          settings_view_controller = view_controller;
          return YES;
        }
        return NO;
      }]
                animated:YES]);
  [coordinator start];

  OCMExpect([base_navigation_controller
      pushViewController:[OCMArg isKindOfClass:[GeminiSuggestionsViewController
                                                   class]]
                animated:YES]);
  id<GeminiSettingsViewControllerPresentationDelegate> presentationDelegate =
      static_cast<id<GeminiSettingsViewControllerPresentationDelegate>>(
          coordinator);
  [presentationDelegate geminiSettingsViewControllerDidSelectSuggestions:
                            settings_view_controller];

  EXPECT_OCMOCK_VERIFY(base_navigation_controller);
  [coordinator stop];
}

// Tests that the Camera row in `GeminiSettingsViewController` is rendered as an
// inline switch when Image Remix is available and updates the camera permission
// preference and user action metrics when toggled.
TEST_F(GeminiSettingsCoordinatorTest, CameraPermissionSwitchToggle) {
  base::UserActionTester user_action_tester;
  GeminiSettingsViewController* view_controller =
      [[GeminiSettingsViewController alloc]
          initWithStyle:ChromeTableViewStyle()];
  id mutator = OCMProtocolMock(@protocol(GeminiSettingsMutator));
  OCMStub([mutator isImageRemixAvailable]).andReturn(YES);
  view_controller.mutator = mutator;

  [view_controller setCameraPermissionEnabled:YES];
  [view_controller loadViewIfNeeded];

  // Section 0 is Preferences; Section 1 is the first enabled Permissions
  // section (Camera when Location is disabled).
  ASSERT_GT([view_controller.tableViewModel numberOfSections], 1);
  TableViewItem* camera_item = [view_controller.tableViewModel
      itemAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:1]];
  TableViewSwitchItem* camera_switch_item =
      base::apple::ObjCCast<TableViewSwitchItem>(camera_item);
  ASSERT_TRUE(camera_switch_item);
  EXPECT_NSEQ(@"CameraCellId", camera_switch_item.accessibilityIdentifier);
  EXPECT_TRUE(camera_switch_item.isOn);

  OCMExpect([mutator setCameraPermissionPref:NO]);
  UISwitch* switch_view = [[UISwitch alloc] init];
  switch_view.on = NO;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  [camera_switch_item.target performSelector:camera_switch_item.selector
                                  withObject:switch_view];
#pragma clang diagnostic pop

  EXPECT_OCMOCK_VERIFY(mutator);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "MobileGeminiCameraSettingsGeminiCameraPermissionToggledOff"));

  // Verify that consumer updates after the view is loaded reconfigure the item.
  [view_controller setCameraPermissionEnabled:NO];
  EXPECT_FALSE(camera_switch_item.isOn);
}
