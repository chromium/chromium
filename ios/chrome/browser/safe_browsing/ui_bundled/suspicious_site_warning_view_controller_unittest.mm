// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/ui_bundled/suspicious_site_warning_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/run_until.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using SuspiciousSiteWarningViewControllerTest = PlatformTest;

// Tests that `SuspiciousSiteWarningViewController` configures modal
// presentation, accessibility traits, localized copy, and vertically stacked
// primary and secondary action buttons with the expected styling.
TEST_F(SuspiciousSiteWarningViewControllerTest,
       DisplaysTitleMessageAndStyledButtons) {
  SuspiciousSiteWarningViewController* view_controller =
      [[SuspiciousSiteWarningViewController alloc] init];
  EXPECT_EQ(UIModalPresentationOverFullScreen,
            view_controller.modalPresentationStyle);
  EXPECT_EQ(UIModalTransitionStyleCrossDissolve,
            view_controller.modalTransitionStyle);

  (void)view_controller.view;
  EXPECT_TRUE(view_controller.view.accessibilityViewIsModal);

  UILabel* title_label = chrome_test_util::FindLabelWithText(
      view_controller.view, l10n_util::GetNSString(IDS_SUSPICIOUS_SITE_TITLE));
  ASSERT_NE(nil, title_label);
  EXPECT_TRUE((title_label.accessibilityTraits & UIAccessibilityTraitHeader) !=
              0);

  EXPECT_NE(nil, chrome_test_util::FindLabelWithText(
                     view_controller.view,
                     l10n_util::GetNSString(IDS_SUSPICIOUS_SITE_MESSAGE)));

  NSArray<UIButton*>* buttons =
      chrome_test_util::FindViewsByClass<UIButton>(view_controller.view);
  ASSERT_EQ(2u, buttons.count);
  UIButton* back_button = buttons[0];
  UIButton* mark_as_safe_button = buttons[1];

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SUSPICIOUS_SITE_BACK_TO_SAFETY),
              back_button.configuration.title);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SUSPICIOUS_SITE_PROCEED_BUTTON),
              mark_as_safe_button.configuration.title);

  EXPECT_NSEQ([UIColor colorNamed:kBlueColor],
              back_button.configuration.background.backgroundColor);
  EXPECT_NSEQ([UIColor colorNamed:kSolidButtonTextColor],
              back_button.configuration.baseForegroundColor);
  EXPECT_NSEQ([UIColor colorNamed:kTertiaryBackgroundColor],
              mark_as_safe_button.configuration.background.backgroundColor);
  EXPECT_NSEQ([UIColor colorNamed:kTextPrimaryColor],
              mark_as_safe_button.configuration.baseForegroundColor);

  // Verify the buttons are arranged vertically inside their containing stack.
  UIStackView* button_stack =
      base::apple::ObjCCastStrict<UIStackView>(back_button.superview);
  EXPECT_EQ(UILayoutConstraintAxisVertical, button_stack.axis);
  ASSERT_EQ(2u, button_stack.arrangedSubviews.count);
  EXPECT_EQ(back_button, button_stack.arrangedSubviews[0]);
  EXPECT_EQ(mark_as_safe_button, button_stack.arrangedSubviews[1]);
}

// Tests that tapping "Back to safety" notifies the delegate once, disables both
// action buttons, and ignores subsequent taps.
TEST_F(SuspiciousSiteWarningViewControllerTest,
       TappingBackToSafetyNotifiesDelegateOnceAndDisablesButtons) {
  SuspiciousSiteWarningViewController* view_controller =
      [[SuspiciousSiteWarningViewController alloc] init];
  id mock_delegate = OCMStrictProtocolMock(
      @protocol(SuspiciousSiteWarningViewControllerDelegate));
  view_controller.delegate = mock_delegate;
  (void)view_controller.view;

  NSArray<UIButton*>* buttons =
      chrome_test_util::FindViewsByClass<UIButton>(view_controller.view);
  ASSERT_EQ(2u, buttons.count);
  UIButton* back_button = buttons[0];
  UIButton* mark_as_safe_button = buttons[1];

  OCMExpect([mock_delegate
      suspiciousSiteWarningViewControllerDidTapBackToSafety:view_controller]);
  [back_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  // A second tap on either button should not invoke the delegate again.
  [back_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  [mark_as_safe_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(mock_delegate);
  EXPECT_FALSE(back_button.enabled);
  EXPECT_FALSE(mark_as_safe_button.enabled);
}

// Tests that tapping "Mark as safe" notifies the delegate once, disables both
// action buttons, and ignores subsequent taps.
TEST_F(SuspiciousSiteWarningViewControllerTest,
       TappingMarkAsSafeNotifiesDelegateOnceAndDisablesButtons) {
  SuspiciousSiteWarningViewController* view_controller =
      [[SuspiciousSiteWarningViewController alloc] init];
  id mock_delegate = OCMStrictProtocolMock(
      @protocol(SuspiciousSiteWarningViewControllerDelegate));
  view_controller.delegate = mock_delegate;
  (void)view_controller.view;

  NSArray<UIButton*>* buttons =
      chrome_test_util::FindViewsByClass<UIButton>(view_controller.view);
  ASSERT_EQ(2u, buttons.count);
  UIButton* back_button = buttons[0];
  UIButton* mark_as_safe_button = buttons[1];

  OCMExpect([mock_delegate
      suspiciousSiteWarningViewControllerDidTapMarkAsSafe:view_controller]);
  [mark_as_safe_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  [mark_as_safe_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  [back_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(mock_delegate);
  EXPECT_FALSE(back_button.enabled);
  EXPECT_FALSE(mark_as_safe_button.enabled);
}

// Tests that dismissing the warning view controller externally without tapping
// an action button notifies `DidDismissWithoutAction`.
TEST_F(SuspiciousSiteWarningViewControllerTest,
       ExternalDismissalNotifiesDelegate) {
  web::WebTaskEnvironment task_environment;
  ScopedKeyWindow scoped_key_window;
  UIViewController* root_view_controller = [[UIViewController alloc] init];
  [scoped_key_window.Get() setRootViewController:root_view_controller];

  SuspiciousSiteWarningViewController* view_controller =
      [[SuspiciousSiteWarningViewController alloc] init];
  id mock_delegate = OCMStrictProtocolMock(
      @protocol(SuspiciousSiteWarningViewControllerDelegate));
  view_controller.delegate = mock_delegate;

  [root_view_controller presentViewController:view_controller
                                     animated:NO
                                   completion:nil];
  ASSERT_EQ(view_controller, root_view_controller.presentedViewController);

  OCMExpect(
      [mock_delegate suspiciousSiteWarningViewControllerDidDismissWithoutAction:
                         view_controller]);
  [root_view_controller dismissViewControllerAnimated:NO completion:nil];

  EXPECT_TRUE(base::test::RunUntil(^bool {
    return root_view_controller.presentedViewController == nil;
  }));
  EXPECT_OCMOCK_VERIFY(mock_delegate);
}

// Tests that overriding the preferred content size category to an accessibility
// size widens the alert card via trait change observation.
TEST_F(SuspiciousSiteWarningViewControllerTest,
       AccessibilityContentSizeWidensAlertCard) {
  ScopedKeyWindow scoped_key_window;
  SuspiciousSiteWarningViewController* view_controller =
      [[SuspiciousSiteWarningViewController alloc] init];
  view_controller.traitOverrides.preferredContentSizeCategory =
      UIContentSizeCategoryLarge;
  [scoped_key_window.Get() setRootViewController:view_controller];
  [view_controller.view layoutIfNeeded];

  UIView* card_view = view_controller.view.subviews.firstObject;
  ASSERT_NE(nil, card_view);
  CGFloat regular_width = CGRectGetWidth(card_view.bounds);

  view_controller.traitOverrides.preferredContentSizeCategory =
      UIContentSizeCategoryAccessibilityExtraLarge;
  [view_controller updateTraitsIfNeeded];
  [view_controller.view layoutIfNeeded];

  CGFloat accessibility_width = CGRectGetWidth(card_view.bounds);
  EXPECT_GT(accessibility_width, regular_width);
}

}  // namespace
