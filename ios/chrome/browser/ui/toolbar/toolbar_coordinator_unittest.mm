// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator.h"

#import "base/scoped_observation.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
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
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Setup all necessary handlers.

    id mockHelpHandler = OCMProtocolMock(@protocol(HelpCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockHelpHandler
                     forProtocol:@protocol(HelpCommands)];

    id mockLensHandler = OCMProtocolMock(@protocol(LensCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockLensHandler
                     forProtocol:@protocol(LensCommands)];

    id mockApplicationHandler = OCMProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockApplicationHandler
                     forProtocol:@protocol(ApplicationCommands)];

    id mockQRScannerHandler = OCMProtocolMock(@protocol(QRScannerCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockQRScannerHandler
                     forProtocol:@protocol(QRScannerCommands)];

    id mockSettingsHandler = OCMProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockSettingsHandler
                     forProtocol:@protocol(SettingsCommands)];

    id mockQuickDeleteHandler = OCMProtocolMock(@protocol(QuickDeleteCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockQuickDeleteHandler
                     forProtocol:@protocol(QuickDeleteCommands)];

    id mockPopupMenuHandler = OCMProtocolMock(@protocol(PopupMenuCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockPopupMenuHandler
                     forProtocol:@protocol(PopupMenuCommands)];

    id mockActivityServiceHandler =
        OCMProtocolMock(@protocol(ActivityServiceCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockActivityServiceHandler
                     forProtocol:@protocol(ActivityServiceCommands)];

    OmniboxPositionBrowserAgent::CreateForBrowser(browser_.get());
  }

  ~ToolbarCoordinatorTest() override {}

  void TearDown() override { [coordinator_ stop]; }

  base::test::TaskEnvironment task_environment_;
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
  GetApplicationContext()->GetLocalState()->SetBoolean(prefs::kBottomOmnibox,
                                                       true);

  EXPECT_TRUE(observer.is_bottom_omnibox_);
}
