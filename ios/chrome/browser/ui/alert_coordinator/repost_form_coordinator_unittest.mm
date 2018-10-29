// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_coordinator/repost_form_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Test location passed to RepostFormCoordinator.
const CGFloat kDialogHorizontalLocation = 10;
const CGFloat kDialogVerticalLocation = 20;
}

// Test fixture to test RepostFormCoordinator class.
class RepostFormCoordinatorTest : public PlatformTest {
 protected:
  RepostFormCoordinatorTest() {
    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];
    UIView* view = [[UIView alloc] initWithFrame:view_controller_.view.bounds];
    web_state_.SetView(view);

    CGPoint dialogLocation =
        CGPointMake(kDialogHorizontalLocation, kDialogVerticalLocation);
    coordinator_ = [[RepostFormCoordinator alloc]
        initWithBaseViewController:view_controller_
                    dialogLocation:dialogLocation
                          webState:&web_state_
                 completionHandler:^(BOOL){
                 }];
  }

  UIAlertController* GetAlertController() const {
    return base::mac::ObjCCastStrict<UIAlertController>(
        view_controller_.presentedViewController);
  }

  // Coordinator will not present the dialog until view is added to the window.
  void AddViewToWindow() {
    [view_controller_.view addSubview:web_state_.GetView()];
  }

  RepostFormCoordinator* coordinator_;

 private:
  ScopedKeyWindow scoped_key_window_;
  web::TestWebState web_state_;
  UIViewController* view_controller_;
};

// Tests that if there is a popover, it uses location passed in init.
TEST_F(RepostFormCoordinatorTest, CGRectUsage) {
  AddViewToWindow();
  [coordinator_ start];
  UIPopoverPresentationController* popover_presentation_controller =
      GetAlertController().popoverPresentationController;
  if (IsIPadIdiom()) {
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

  base::test::ios::WaitUntilCondition(^bool {
    return GetAlertController();
  });

  EXPECT_EQ(2U, GetAlertController().actions.count);

  [coordinator_ stop];
}
