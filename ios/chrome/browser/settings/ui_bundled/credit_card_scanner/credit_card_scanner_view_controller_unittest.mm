// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_view_controller.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/scanner/ui_bundled/scanner_presenting.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface CreditCardScannerViewController (Testing)
- (void)didTapEnterManually:(id)sender;
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
