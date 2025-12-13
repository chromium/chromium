// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_mediator.h"

#import <Foundation/Foundation.h>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_mediator_delegate.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class CreditCardScannerMediatorTest : public PlatformTest {
 public:
  CreditCardScannerMediatorTest() {
    mock_delegate_ =
        OCMProtocolMock(@protocol(CreditCardScannerMediatorDelegate));
    mock_consumer_ = OCMProtocolMock(@protocol(CreditCardScannerConsumer));
    mediator_ =
        [[CreditCardScannerMediator alloc] initWithDelegate:mock_delegate_
                                                   consumer:mock_consumer_];
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  OCMockObject<CreditCardScannerMediatorDelegate>* mock_delegate_;
  OCMockObject<CreditCardScannerConsumer>* mock_consumer_;
  CreditCardScannerMediator* mediator_;
};

// Tests that the mediator reacts correctly to receiving a scanned credit card,
// informing both the delegate and consumer.
TEST_F(CreditCardScannerMediatorTest, HandlesScannedCreditCard) {
  OCMExpect([mock_delegate_ creditCardScannerMediatorDidFinishScan:mediator_]);
  OCMExpect([mock_consumer_ setCreditCardNumber:@"4444333322221111"
                                expirationMonth:@"12"
                                 expirationYear:@"99"]);

  [mediator_ setCreditCardNumber:@"4444333322221111"
                 expirationMonth:@"12"
                  expirationYear:@"99"];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}
