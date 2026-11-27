// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/expanded_manual_fill_view_controller.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/expanded_manual_fill_view_controller+Testing.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/fallback_view_controller.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_action_cell.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

using manual_fill::ManualFillDataType;

// Test fixture for testing the ExpandedManualFillViewController class.
class ExpandedManualFillViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    delegate_ =
        OCMProtocolMock(@protocol(ExpandedManualFillViewControllerDelegate));

    InitViewControllerForDataType(ManualFillDataType::kCredential);
  }

  // Instanciate the view controller for the given `data_type` and load its view
  // if needed.
  void InitViewControllerForDataType(ManualFillDataType data_type) {
    view_controller_ =
        [[ExpandedManualFillViewController alloc] initWithDelegate:delegate()
                                                       forDataType:data_type];
    [view_controller_ loadViewIfNeeded];
  }

  // Resets the view controller.
  void ResetViewController() { view_controller_ = nil; }

  id delegate() { return delegate_; }

  ExpandedManualFillViewController* view_controller() {
    return view_controller_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  id delegate_;
  ExpandedManualFillViewController* view_controller_;
};

// Tests that the right segmented control segment is selected depending on the
// data type the view controller is instanciated for.
TEST_F(ExpandedManualFillViewControllerTest,
       TestSegmentedControlPreSelectedData) {
  EXPECT_EQ(view_controller().segmentedControl.selectedSegmentIndex, 0);

  ResetViewController();

  InitViewControllerForDataType(ManualFillDataType::kPaymentMethod);
  EXPECT_EQ(view_controller().segmentedControl.selectedSegmentIndex, 1);

  ResetViewController();

  InitViewControllerForDataType(ManualFillDataType::kAddress);
  EXPECT_EQ(view_controller().segmentedControl.selectedSegmentIndex, 2);
}

// Tests that the delegate is correctly notified when a data type is selected
// from the segmented control.
TEST_F(ExpandedManualFillViewControllerTest, TestDataTypeSelection) {
  UISegmentedControl* segmented_control = view_controller().segmentedControl;

  // Select the address segment and verify the delegate call.
  OCMExpect([delegate()
      expandedManualFillViewController:view_controller()
                didSelectSegmentOfType:ManualFillDataType::kAddress]);
  segmented_control.selectedSegmentIndex = 2;
  [segmented_control sendActionsForControlEvents:UIControlEventValueChanged];
  EXPECT_OCMOCK_VERIFY(delegate());

  // Select the payment method segment and verify the delegate call.
  OCMExpect([delegate()
      expandedManualFillViewController:view_controller()
                didSelectSegmentOfType:ManualFillDataType::kPaymentMethod]);
  segmented_control.selectedSegmentIndex = 1;
  [segmented_control sendActionsForControlEvents:UIControlEventValueChanged];
  EXPECT_OCMOCK_VERIFY(delegate());

  // Select the password segment and verify the delegate call.
  OCMExpect([delegate()
      expandedManualFillViewController:view_controller()
                didSelectSegmentOfType:ManualFillDataType::kCredential]);
  segmented_control.selectedSegmentIndex = 0;
  [segmented_control sendActionsForControlEvents:UIControlEventValueChanged];
  EXPECT_OCMOCK_VERIFY(delegate());
}

// Tests embedding and replacing `childViewController` (including syncing cell
// horizontal insets during layout) and forwarding close button taps to the
// delegate.
TEST_F(ExpandedManualFillViewControllerTest,
       EmbedsAndReplacesChildViewControllerAndForwardsCloseButton) {
  view_controller().view.frame = CGRectMake(0, 0, 390, 500);

  FallbackViewController* first_child = [[FallbackViewController alloc] init];
  [first_child loadViewIfNeeded];
  [first_child presentActionItems:@[ [[ManualFillActionItem alloc]
                                      initWithTitle:@"Manage..."
                                             action:^{
                                             }] ]];
  view_controller().childViewController = first_child;
  // Re-assigning the same child view controller is a no-op.
  view_controller().childViewController = first_child;
  [first_child.tableView layoutIfNeeded];
  [view_controller().view setNeedsLayout];
  [view_controller().view layoutIfNeeded];
  EXPECT_EQ(first_child.parentViewController, view_controller());
  EXPECT_EQ(first_child.view.superview, view_controller().view);

  FallbackViewController* second_child = [[FallbackViewController alloc] init];
  view_controller().childViewController = second_child;
  EXPECT_EQ(first_child.parentViewController, nil);
  EXPECT_EQ(first_child.view.superview, nil);
  EXPECT_EQ(second_child.parentViewController, view_controller());

  NSString* close_label = l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_CLOSE_BUTTON_ACCESSIBILITY_LABEL);
  UIButton* close_button = chrome_test_util::FindViewWithPredicate<UIButton>(
      view_controller().view, ^BOOL(UIView* candidate) {
        return [candidate isKindOfClass:[UIButton class]] &&
               [candidate.accessibilityLabel isEqualToString:close_label];
      });
  ASSERT_NE(close_button, nil);

  OCMExpect([delegate() expandedManualFillViewController:view_controller()
                                     didPressCloseButton:close_button]);
  [close_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(delegate());
}
