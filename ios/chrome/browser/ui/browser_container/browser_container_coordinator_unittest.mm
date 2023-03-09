// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/browser_container/edit_menu_alert_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for BrowserContainerCoordinator.
class BrowserContainerCoordinatorTest : public PlatformTest {
 public:
  BrowserContainerCoordinatorTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    mocked_handler_ = OCMStrictProtocolMock(@protocol(ActivityServiceCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_handler_
                     forProtocol:@protocol(ActivityServiceCommands)];
  }

  BrowserContainerCoordinator* CreateAndStartCoordinator() {
    BrowserContainerCoordinator* coordinator =
        [[BrowserContainerCoordinator alloc]
            initWithBaseViewController:nil
                               browser:browser_.get()];
    [coordinator start];
    [scoped_key_window_.Get() setRootViewController:coordinator.viewController];
    return coordinator;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  id mocked_handler_;
  ScopedKeyWindow scoped_key_window_;
};

// Tests that the coordinator displays an alert with given title, message and
// actions.
TEST_F(BrowserContainerCoordinatorTest,
       LinkToTextConsumerLinkGenerationFailed) {
  BrowserContainerCoordinator* coordinator = CreateAndStartCoordinator();

  EditMenuAlertDelegateAction* action_ok = [[EditMenuAlertDelegateAction alloc]
      initWithTitle:l10n_util::GetNSString(IDS_APP_OK)
             action:^{
             }
              style:UIAlertActionStyleCancel
          preferred:NO];

  EditMenuAlertDelegateAction* action_share =
      [[EditMenuAlertDelegateAction alloc]
          initWithTitle:l10n_util::GetNSString(IDS_IOS_SHARE_PAGE_BUTTON_LABEL)
                 action:^{
                 }
                  style:UIAlertActionStyleDefault
              preferred:NO];

  id<EditMenuAlertDelegate> alert_delegate =
      static_cast<id<EditMenuAlertDelegate>>(coordinator);
  [alert_delegate showAlertWithTitle:@"alert title"
                             message:@"alert message"
                             actions:@[ action_ok, action_share ]];

  EXPECT_TRUE([coordinator.viewController.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          coordinator.viewController.presentedViewController);
  ASSERT_EQ(2LU, alert_controller.actions.count);

  // First action should be the OK button.
  UIAlertAction* ok_action = alert_controller.actions[0];
  EXPECT_TRUE(
      [l10n_util::GetNSString(IDS_APP_OK) isEqualToString:ok_action.title]);
  EXPECT_EQ(UIAlertActionStyleCancel, ok_action.style);

  // Second action should the Share button.
  UIAlertAction* share_action = alert_controller.actions[1];
  EXPECT_TRUE([l10n_util::GetNSString(IDS_IOS_SHARE_PAGE_BUTTON_LABEL)
      isEqualToString:share_action.title]);
  EXPECT_EQ(UIAlertActionStyleDefault, share_action.style);
}
