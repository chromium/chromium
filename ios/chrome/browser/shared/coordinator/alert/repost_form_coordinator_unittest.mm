// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/alert/repost_form_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "base/test/test_timeouts.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Test location passed to RepostFormCoordinator.
const CGFloat kDialogHorizontalLocation = 10;
const CGFloat kDialogVerticalLocation = 20;
}  // namespace

// Test fixture to test RepostFormCoordinator class.
class RepostFormCoordinatorTest : public PlatformTest {
 protected:
  RepostFormCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    view_controller_ = [[UIViewController alloc] init];
    CGPoint dialogLocation =
        CGPointMake(kDialogHorizontalLocation, kDialogVerticalLocation);
    coordinator_ = [[RepostFormCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()
                    dialogLocation:dialogLocation
                          webState:&web_state_
                 completionHandler:^(BOOL){
                 }];
  }

  UIAlertController* GetAlertController() const {
    return base::apple::ObjCCastStrict<UIAlertController>(
        view_controller_.presentedViewController);
  }

  // Coordinator will not present the dialog until view is added to the window.
  void AddViewToWindow() {
    [scoped_key_window_.Get() setRootViewController:view_controller_];
  }

  RepostFormCoordinator* coordinator_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_key_window_;
  web::FakeWebState web_state_;
  UIViewController* view_controller_;
};

// Tests that if there is a popover, it uses location passed in init.
TEST_F(RepostFormCoordinatorTest, CGRectUsage) {
  AddViewToWindow();
  [coordinator_ start];
  UIPopoverPresentationController* popover_presentation_controller =
      GetAlertController().popoverPresentationController;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    CGRect source_rect = popover_presentation_controller.sourceRect;
    EXPECT_EQ(kDialogHorizontalLocation, CGRectGetMinX(source_rect));
    EXPECT_EQ(kDialogVerticalLocation, CGRectGetMinY(source_rect));
  }
}

// Tests the repost form dialog has nil title.
TEST_F(RepostFormCoordinatorTest, Title) {
  AddViewToWindow();
  [coordinator_ start];
  EXPECT_FALSE(GetAlertController().title);
  [coordinator_ stop];
}

// Tests the repost form dialog has correct message.
TEST_F(RepostFormCoordinatorTest, Message) {
  AddViewToWindow();
  [coordinator_ start];
  EXPECT_TRUE([GetAlertController().message
      containsString:l10n_util::GetNSString(IDS_HTTP_POST_WARNING_TITLE)]);
  EXPECT_TRUE([GetAlertController().message
      containsString:l10n_util::GetNSString(IDS_HTTP_POST_WARNING)]);
  [coordinator_ stop];
}

// Tests the repost form dialog actions have correct titles.
TEST_F(RepostFormCoordinatorTest, ActionTitles) {
  AddViewToWindow();
  [coordinator_ start];
  EXPECT_EQ(2U, GetAlertController().actions.count);

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL),
              [GetAlertController().actions.firstObject title]);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_HTTP_POST_WARNING_RESEND),
              [GetAlertController().actions.lastObject title]);
  [coordinator_ stop];
}

// Tests that repost form dialog is presented once view is added to the window.
TEST_F(RepostFormCoordinatorTest, Retrying) {
  [coordinator_ start];
  EXPECT_FALSE(GetAlertController());

  AddViewToWindow();

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool {
        return GetAlertController();
      }));

  EXPECT_EQ(2U, GetAlertController().actions.count);

  [coordinator_ stop];
}
