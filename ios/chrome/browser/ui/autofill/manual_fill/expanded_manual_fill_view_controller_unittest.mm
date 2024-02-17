// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/expanded_manual_fill_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/expanded_manual_fill_view_controller+Testing.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "testing/platform_test.h"

using manual_fill::ManualFillDataType;

// Test fixture for testing the ExpandedManualFillViewController class.
class ExpandedManualFillViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    InitViewControllerForDataType(ManualFillDataType::kPassword);
  }

  // Instanciate the view controller for the given `data_type` and load its view
  // if needed.
  void InitViewControllerForDataType(ManualFillDataType data_type) {
    view_controller_ =
        [[ExpandedManualFillViewController alloc] initForDataType:data_type];
    [view_controller_ loadViewIfNeeded];
  }

  // Resets the view controller.
  void ResetViewController() { view_controller_ = nil; }

  ExpandedManualFillViewController* view_controller() {
    return view_controller_;
  }

 private:
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
