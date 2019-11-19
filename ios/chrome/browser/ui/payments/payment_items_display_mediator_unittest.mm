// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_items_display_mediator.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestPaymentItemsDisplayMediatorTest
    : public PaymentRequestUnitTestBase,
      public PlatformTest {
 protected:
  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();
    DoSetUp();
  }

  // PlatformTest:
  void TearDown() override {
    DoTearDown();
    PlatformTest::TearDown();
  }
};

// Tests that the expected total item is created.
TEST_F(PaymentRequestPaymentItemsDisplayMediatorTest, TotalItem) {
  payments::WebPaymentRequest web_payment_request;
  web_payment_request.details.total = std::make_unique<payments::PaymentItem>();
  web_payment_request.details.total->label = "Total Cost";
  web_payment_request.details.total->amount->value = "9.99";
  web_payment_request.details.total->amount->currency = "USD";

  payments::TestPaymentRequest payment_request(web_payment_request,
                                               browser_state(), web_state(),
                                               personal_data_manager());
  PaymentItemsDisplayMediator* mediator = [[PaymentItemsDisplayMediator alloc]
      initWithPaymentRequest:&payment_request];

  CollectionViewItem* total_item = [mediator totalItem];
  ASSERT_TRUE([total_item isKindOfClass:[PriceItem class]]);
  PriceItem* total_price_item =
      base::mac::ObjCCastStrict<PriceItem>(total_item);
  EXPECT_TRUE([total_price_item.item isEqualToString:@"Total Cost"]);
  EXPECT_TRUE([total_price_item.price isEqualToString:@"USD $9.99"]);
}

// Tests that the expected line items are created.
TEST_F(PaymentRequestPaymentItemsDisplayMediatorTest, LineItems) {
  payments::WebPaymentRequest web_payment_request;
  web_payment_request.details.total = std::make_unique<payments::PaymentItem>();

  payments::TestPaymentRequest payment_request1(web_payment_request,
                                                browser_state(), web_state(),
                                                personal_data_manager());
  PaymentItemsDisplayMediator* mediator = [[PaymentItemsDisplayMediator alloc]
      initWithPaymentRequest:&payment_request1];

  NSArray<CollectionViewItem*>* line_items = [mediator lineItems];
  ASSERT_EQ(0U, line_items.count);

  payments::PaymentItem display_item1;
  display_item1.label = "Line Item 1";
  display_item1.amount->value = "9.00";
  display_item1.amount->currency = "USD";
  web_payment_request.details.display_items.push_back(display_item1);
  payments::PaymentItem display_item2;
  display_item2.label = "Line Item 2";
  display_item2.amount->value = "0.99";
  display_item2.amount->currency = "USD";
  web_payment_request.details.display_items.push_back(display_item2);

  payments::TestPaymentRequest payment_request2(web_payment_request,
                                                browser_state(), web_state(),
                                                personal_data_manager());
  mediator = [[PaymentItemsDisplayMediator alloc]
      initWithPaymentRequest:&payment_request2];

  line_items = [mediator lineItems];
  ASSERT_EQ(2U, line_items.count);

  PriceItem* line_item1 = base::mac::ObjCCastStrict<PriceItem>(line_items[0]);
  EXPECT_TRUE([line_item1.item isEqualToString:@"Line Item 1"]);
  EXPECT_TRUE([line_item1.price isEqualToString:@"9.00"]);

  PriceItem* line_item2 = base::mac::ObjCCastStrict<PriceItem>(line_items[1]);
  EXPECT_TRUE([line_item2.item isEqualToString:@"Line Item 2"]);
  EXPECT_TRUE([line_item2.price isEqualToString:@"0.99"]);
}
