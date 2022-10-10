// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/commands/bookmarks_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter_delegate.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_updating.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeUIUpdater : NSObject <PopupMenuUIUpdating>

@property(nonatomic, assign) BOOL menuDisplayed;

@end

@implementation FakeUIUpdater

- (void)updateUIForMenuDisplayed:(PopupMenuType)popupType {
  self.menuDisplayed = YES;
}

- (void)updateUIForMenuDismissed {
  self.menuDisplayed = NO;
}

@end

class PopupMenuCoordinatorTest : public PlatformTest {
 protected:
  PopupMenuCoordinatorTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
  }

  void SetUp() override {
    mock_browser_commands_handler_ =
        OCMStrictProtocolMock(@protocol(BrowserCommands));
    mock_bookmarks_commands_handler_ =
        OCMStrictProtocolMock(@protocol(BookmarksCommands));
    mock_page_info_commands_handler_ =
        OCMStrictProtocolMock(@protocol(PageInfoCommands));

    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_browser_commands_handler_
                     forProtocol:@protocol(BrowserCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_bookmarks_commands_handler_
                     forProtocol:@protocol(BookmarksCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_page_info_commands_handler_
                     forProtocol:@protocol(PageInfoCommands)];
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  id mock_browser_commands_handler_;
  id mock_bookmarks_commands_handler_;
  id mock_page_info_commands_handler_;
};

// Tests that after the coordinator is stopped, calling methods on it does not
// trigger anything associated with dismissal.
TEST_F(PopupMenuCoordinatorTest, TestNoDismissAfterStop) {
  FakeUIUpdater* uiUpdater = [[FakeUIUpdater alloc] init];
  PopupMenuCoordinator* coordinator =
      [[PopupMenuCoordinator alloc] initWithBrowser:browser_.get()];
  coordinator.UIUpdater = uiUpdater;
  [coordinator start];

  // Create command handler and use it to show popup. This should update the UI
  // to be displayed.
  id<PopupMenuCommands> commandHandler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), PopupMenuCommands);
  [[mock_browser_commands_handler_ expect]
      prepareForPopupMenuPresentation:PopupMenuCommandTypeToolsMenu];
  [commandHandler showToolsMenuPopup];
  [mock_browser_commands_handler_ verify];
  EXPECT_TRUE(uiUpdater.menuDisplayed);

  [coordinator stop];

  // Call a protocol method that will lead to `-dismissPopupMenuAnimated:`,
  // a command that should not be called after `-stop`.
  ASSERT_TRUE(
      [coordinator conformsToProtocol:@protocol(PopupMenuPresenterDelegate)]);
  id<PopupMenuPresenterDelegate> presenterDelegate =
      static_cast<id<PopupMenuPresenterDelegate>>(coordinator);
  [presenterDelegate popupMenuPresenterWillDismiss:nil];

  // As the command was not called, the `uiUpdater` should still think the menu
  // is displayed.
  EXPECT_TRUE(uiUpdater.menuDisplayed);
}
