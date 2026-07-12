// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_view_controller.h"

#import "base/test/metrics/user_action_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/scanner/ui_bundled/scanner_presenting.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface CreditCardScannerViewController (Testing)
- (void)didTapEnterManually:(id)sender;
- (void)setupEnterManuallyButton;
@end

class CreditCardScannerViewControllerTest : public PlatformTest {
 protected:
  CreditCardScannerViewControllerTest() {
    mock_presentation_provider_ = OCMProtocolMock(@protocol(ScannerPresenting));
    view_controller_ = [[CreditCardScannerViewController alloc]
        initWithPresentationProvider:mock_presentation_provider_];
  }

  base::test::TaskEnvironment task_environment_;
  OCMockObject<ScannerPresenting>* mock_presentation_provider_;
  CreditCardScannerViewController* view_controller_;
};

// Tests that tapping "Enter Manually" dismisses the view controller.
TEST_F(CreditCardScannerViewControllerTest, EnterManuallyDismisses) {
  OCMExpect([mock_presentation_provider_
      dismissScannerViewController:view_controller_
                        completion:[OCMArg any]]);

  [view_controller_ didTapEnterManually:nil];

  EXPECT_OCMOCK_VERIFY(mock_presentation_provider_);
}

// Tests that calling setupEnterManuallyButton multiple times adds the button
// only once.
TEST_F(CreditCardScannerViewControllerTest, EnterManuallyButtonAddedOnce) {
  UIView* mockScannerView = [[UIView alloc] init];
  UIToolbar* toolbar = [[UIToolbar alloc] init];
  UIBarButtonItem* item1 = [[UIBarButtonItem alloc] init];
  UIBarButtonItem* item2 = [[UIBarButtonItem alloc] init];
  UIBarButtonItem* item3 = [[UIBarButtonItem alloc] init];
  toolbar.items = @[ item1, item2, item3 ];
  [mockScannerView addSubview:toolbar];

  view_controller_.scannerView = (ScannerView*)mockScannerView;

  // Call once to setup the button.
  [view_controller_ setupEnterManuallyButton];
  EXPECT_EQ(toolbar.items.count, 5U);

  // Call again (simulating view reappearing). It should not add a duplicate
  // button.
  [view_controller_ setupEnterManuallyButton];
  EXPECT_EQ(toolbar.items.count, 5U);
}

// Tests that closing the scanner via CLOSE_BUTTON records the correct metric.
TEST_F(CreditCardScannerViewControllerTest, MetricsOnClose) {
  base::UserActionTester action_tester;

  OCMExpect([mock_presentation_provider_
      dismissScannerViewController:view_controller_
                        completion:[OCMArg any]]);

  [view_controller_ dismissForReason:scannerViewController::CLOSE_BUTTON
                      withCompletion:nil];

  EXPECT_EQ(
      1, action_tester.GetActionCount("IOS.CreditCardScanner.DismissedByUser"));
  EXPECT_OCMOCK_VERIFY(mock_presentation_provider_);
}

// Tests that closing the scanner via ERROR_DIALOG records the correct metric.
TEST_F(CreditCardScannerViewControllerTest, MetricsOnError) {
  base::UserActionTester action_tester;

  OCMExpect([mock_presentation_provider_
      dismissScannerViewController:view_controller_
                        completion:[OCMArg any]]);

  [view_controller_ dismissForReason:scannerViewController::ERROR_DIALOG
                      withCompletion:nil];

  EXPECT_EQ(1, action_tester.GetActionCount("IOS.CreditCardScanner.Error"));
  EXPECT_OCMOCK_VERIFY(mock_presentation_provider_);
}

// Tests that scanning a card via SCAN_COMPLETE records the correct metric.
TEST_F(CreditCardScannerViewControllerTest, MetricsOnScanComplete) {
  base::UserActionTester action_tester;

  OCMExpect([mock_presentation_provider_
      dismissScannerViewController:view_controller_
                        completion:[OCMArg any]]);

  [view_controller_ dismissForReason:scannerViewController::SCAN_COMPLETE
                      withCompletion:nil];

  EXPECT_EQ(1,
            action_tester.GetActionCount("IOS.CreditCardScanner.ScannedCard"));
  EXPECT_OCMOCK_VERIFY(mock_presentation_provider_);
}
