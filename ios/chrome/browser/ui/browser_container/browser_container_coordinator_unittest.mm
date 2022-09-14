// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/link_to_text/link_to_text_payload.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/share_highlight_command.h"
#import "ios/chrome/browser/ui/link_to_text/link_to_text_consumer.h"
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

// Tests that the coordinator properly handles link-to-text payload updates.
TEST_F(BrowserContainerCoordinatorTest, LinkToTextConsumerGeneratePayload) {
  BrowserContainerCoordinator* coordinator = CreateAndStartCoordinator();

  LinkToTextPayload* test_payload =
      [[LinkToTextPayload alloc] initWithURL:GURL("https://google.com")
                                       title:@"Some title"
                                selectedText:@"Selected on page"
                                  sourceView:[[UIView alloc] init]
                                  sourceRect:CGRectMake(0, 1, 2, 3)];
  [[mocked_handler_ expect]
      shareHighlight:[OCMArg checkWithBlock:^BOOL(
                                 ShareHighlightCommand* command) {
        EXPECT_EQ(test_payload.URL, command.URL);
        EXPECT_TRUE([test_payload.title isEqualToString:command.title]);
        EXPECT_TRUE(
            [test_payload.selectedText isEqualToString:command.selectedText]);
        EXPECT_EQ(test_payload.sourceView, command.sourceView);
        EXPECT_TRUE(
            CGRectEqualToRect(test_payload.sourceRect, command.sourceRect));
        return YES;
      }]];

  id<LinkToTextConsumer> consumer =
      static_cast<id<LinkToTextConsumer>>(coordinator);
  [consumer generatedPayload:test_payload];

  [mocked_handler_ verify];
}

// Tests that the coordinator displays an alert when getting an update about how
// a link-to-text link generation failed.
TEST_F(BrowserContainerCoordinatorTest,
       LinkToTextConsumerLinkGenerationFailed) {
  BrowserContainerCoordinator* coordinator = CreateAndStartCoordinator();

  id<LinkToTextConsumer> consumer =
      static_cast<id<LinkToTextConsumer>>(coordinator);
  [consumer linkGenerationFailed];

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
