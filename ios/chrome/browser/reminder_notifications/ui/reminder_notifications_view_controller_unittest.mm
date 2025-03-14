// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_date_picker_table_view_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

using ReminderNotificationsViewControllerTest = PlatformTest;

namespace {

// Recursively searches the view hierarchy for a view with the specified
// accessibility identifier.
UIView* FindViewById(UIView* view, NSString* accessibility_id) {
  if (view.accessibilityIdentifier == accessibility_id) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* foundView = FindViewById(subview, accessibility_id);
    if (foundView) {
      return foundView;
    }
  }
  return nil;
}

// Returns whether the view hierarchy contains a title label.
bool HasTitle(UIView* view) {
  return FindViewById(view, kConfirmationAlertTitleAccessibilityIdentifier) !=
         nil;
}

// Returns whether the view hierarchy contains a subtitle label.
bool HasSubtitle(UIView* view) {
  return FindViewById(view,
                      kConfirmationAlertSubtitleAccessibilityIdentifier) != nil;
}

// Returns whether the view hierarchy contains a primary action button.
bool HasPrimaryActionButton(UIView* view) {
  return FindViewById(view,
                      kConfirmationAlertPrimaryActionAccessibilityIdentifier) !=
         nil;
}

// Returns whether the view hierarchy contains a secondary action button.
bool HasSecondaryActionButton(UIView* view) {
  return FindViewById(
             view, kConfirmationAlertSecondaryActionAccessibilityIdentifier) !=
         nil;
}

}  // namespace

// Tests the basic creation and setup of the view controller.
TEST_F(ReminderNotificationsViewControllerTest,
       ShowsRequiredUIElementsWhenViewLoads) {
  ReminderNotificationsViewController* viewController =
      [[ReminderNotificationsViewController alloc] init];
  UIView* view = viewController.view;

  ASSERT_NE(viewController, nil);
  EXPECT_TRUE(HasTitle(view));
  EXPECT_TRUE(HasSubtitle(view));
  EXPECT_TRUE(HasPrimaryActionButton(view));
  EXPECT_TRUE(HasSecondaryActionButton(view));
}

// Tests that text content is set correctly.
TEST_F(ReminderNotificationsViewControllerTest,
       SetsLocalizedStringsForAllTextElements) {
  ReminderNotificationsViewController* viewController =
      [[ReminderNotificationsViewController alloc] init];
  [viewController view];

  EXPECT_NSEQ(
      viewController.titleString,
      l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_SHEET_TITLE));
  EXPECT_NSEQ(
      viewController.subtitleString,
      l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_DESCRIPTION));
  EXPECT_NSEQ(viewController.primaryActionString,
              l10n_util::GetNSString(
                  IDS_IOS_REMINDER_NOTIFICATIONS_SET_REMINDER_BUTTON));
  EXPECT_NSEQ(
      viewController.secondaryActionString,
      l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_CANCEL_BUTTON));
}

// Tests the layout configuration.
TEST_F(ReminderNotificationsViewControllerTest,
       ConfiguresViewLayoutWithCorrectProperties) {
  ReminderNotificationsViewController* viewController =
      [[ReminderNotificationsViewController alloc] init];
  [viewController view];

  EXPECT_FALSE(viewController.showDismissBarButton);
  EXPECT_TRUE(viewController.topAlignedLayout);
  EXPECT_TRUE(viewController.alwaysShowImage);
  EXPECT_TRUE(viewController.imageHasFixedSize);
  EXPECT_TRUE(viewController.imageEnclosedWithShadowWithoutBadge);

  EXPECT_EQ(viewController.customSpacingBeforeImageIfNoNavigationBar, 16);
  EXPECT_EQ(viewController.customSpacingAfterImage, 16);
  EXPECT_EQ(viewController.customFaviconSideLength, 42);
  EXPECT_NSEQ(viewController.titleTextStyle, UIFontTextStyleTitle2);
}

// Tests the image configuration.
TEST_F(ReminderNotificationsViewControllerTest,
       SetsBellIconImageWithBlueBackground) {
  ReminderNotificationsViewController* viewController =
      [[ReminderNotificationsViewController alloc] init];
  [viewController view];

  EXPECT_NE(viewController.image, nil);
  EXPECT_NSEQ(viewController.imageBackgroundColor,
              [UIColor colorNamed:kBlue500Color]);
}

// Tests that the image view has the correct accessibility label.
TEST_F(ReminderNotificationsViewControllerTest,
       SetsBellIconAccessibilityLabelForVoiceOver) {
  ReminderNotificationsViewController* viewController =
      [[ReminderNotificationsViewController alloc] init];
  [viewController view];

  EXPECT_NSEQ(viewController.imageViewAccessibilityLabel,
              @"ReminderNotificationsBellIconAccessibilityLabel");
}

// Tests the date picker table view cell configuration
TEST_F(ReminderNotificationsViewControllerTest, DatePickerTableViewCellConfig) {
  ReminderNotificationsDatePickerTableViewCell* cell =
      [[ReminderNotificationsDatePickerTableViewCell alloc]
            initWithStyle:UITableViewCellStyleDefault
          reuseIdentifier:@"TestCell"];

  [cell configureWithLabel:@"Test" value:@"Value"];

  // Find the labels (would need to add helper methods or use OCMock for more
  // complex verification)
  UILabel* titleLabel = nil;
  UILabel* valueLabel = nil;

  for (UIView* subview in cell.contentView.subviews) {
    if ([subview isKindOfClass:[UILabel class]]) {
      UILabel* label = (UILabel*)subview;
      if ([label.text isEqualToString:@"Test"]) {
        titleLabel = label;
      } else if ([label.text isEqualToString:@"Value"]) {
        valueLabel = label;
      }
    }
  }

  EXPECT_NE(titleLabel, nil);
  EXPECT_NE(valueLabel, nil);
  EXPECT_NSEQ(cell.accessibilityIdentifier, @"ReminderNotificationsTestRow");
}
