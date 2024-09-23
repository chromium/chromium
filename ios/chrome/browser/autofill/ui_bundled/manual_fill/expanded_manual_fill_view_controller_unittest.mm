// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/expanded_manual_fill_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/expanded_manual_fill_view_controller+Testing.h"

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using manual_fill::ManualFillDataType;

// Test fixture for testing the ExpandedManualFillViewController class.
class ExpandedManualFillViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    delegate_ =
        OCMProtocolMock(@protocol(ExpandedManualFillViewControllerDelegate));

    InitViewControllerForDataType(ManualFillDataType::kPassword);
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
                didSelectSegmentOfType:ManualFillDataType::kPassword]);
  segmented_control.selectedSegmentIndex = 0;
  [segmented_control sendActionsForControlEvents:UIControlEventValueChanged];
  EXPECT_OCMOCK_VERIFY(delegate());
}
