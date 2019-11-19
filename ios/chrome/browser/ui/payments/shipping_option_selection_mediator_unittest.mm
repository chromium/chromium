// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/shipping_option_selection_mediator.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_shipping_option.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestShippingOptionSelectionMediatorTest
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

  // Returns a PaymentDetails instance with two unselected shipping options.
  payments::PaymentDetails CreateDetailsWithUnselectedShippingOptions() {
    payments::PaymentDetails details;
    details.total = std::make_unique<payments::PaymentItem>();
    details.total->label = "Total Cost";
    details.total->amount->value = "9.99";
    details.total->amount->currency = "USD";
    payments::PaymentShippingOption option1;
    option1.id = "1";
    option1.label = "option 1";
    option1.amount->value = "0.99";
    option1.amount->currency = "USD";
    option1.selected = false;
    details.shipping_options.push_back(std::move(option1));
    payments::PaymentShippingOption option2;
    option2.id = "2";
    option2.label = "option 2";
    option2.amount->value = "1.99";
    option2.amount->currency = "USD";
    option2.selected = false;
    details.shipping_options.push_back(std::move(option2));
    return details;
  }
};

// Tests that the expected selectable items are created and that the index of
// the selected item is properly set.
TEST_F(PaymentRequestShippingOptionSelectionMediatorTest, TestSelectableItems) {
  payments::WebPaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithUnselectedShippingOptions();
  // Select the second shipping option.
  web_payment_request.details.shipping_options[1].selected = true;

  payments::TestPaymentRequest payment_request(web_payment_request,
                                               browser_state(), web_state(),
                                               personal_data_manager());

  ShippingOptionSelectionMediator* mediator =
      [[ShippingOptionSelectionMediator alloc]
          initWithPaymentRequest:&payment_request];

  NSArray<CollectionViewItem*>* selectable_items = [mediator selectableItems];

  ASSERT_EQ(2U, selectable_items.count);

  // The second item must be selected.
  EXPECT_EQ(1U, mediator.selectedItemIndex);

  CollectionViewItem* item_1 = [[mediator selectableItems] objectAtIndex:0];
  DCHECK([item_1 isKindOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* text_item_1 =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item_1);
  EXPECT_TRUE([text_item_1.text isEqualToString:@"option 1"]);
  EXPECT_TRUE([text_item_1.detailText isEqualToString:@"$0.99"]);

  CollectionViewItem* item_2 = [[mediator selectableItems] objectAtIndex:0];
  DCHECK([item_2 isKindOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* text_item_2 =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item_2);
  EXPECT_TRUE([text_item_2.text isEqualToString:@"option 1"]);
  EXPECT_TRUE([text_item_2.detailText isEqualToString:@"$0.99"]);
}

// Tests that the index of the selected item is as expected when there is no
// shipping option.
TEST_F(PaymentRequestShippingOptionSelectionMediatorTest, TestNoItems) {
  payments::WebPaymentRequest web_payment_request;

  payments::TestPaymentRequest payment_request(web_payment_request,
                                               browser_state(), web_state(),
                                               personal_data_manager());

  ShippingOptionSelectionMediator* mediator =
      [[ShippingOptionSelectionMediator alloc]
          initWithPaymentRequest:&payment_request];

  NSArray<CollectionViewItem*>* selectable_items = [mediator selectableItems];

  ASSERT_EQ(0U, selectable_items.count);

  // The selected item index must be invalid.
  EXPECT_EQ(NSUIntegerMax, mediator.selectedItemIndex);
}

// Tests that the index of the selected item is as expected when there is no
// selected shipping option.
TEST_F(PaymentRequestShippingOptionSelectionMediatorTest, TestNoSelectedItem) {
  payments::WebPaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithUnselectedShippingOptions();

  payments::TestPaymentRequest payment_request(web_payment_request,
                                               browser_state(), web_state(),
                                               personal_data_manager());

  ShippingOptionSelectionMediator* mediator =
      [[ShippingOptionSelectionMediator alloc]
          initWithPaymentRequest:&payment_request];

  NSArray<CollectionViewItem*>* selectable_items = [mediator selectableItems];

  ASSERT_EQ(2U, selectable_items.count);

  // The selected item index must be invalid.
  EXPECT_EQ(NSUIntegerMax, mediator.selectedItemIndex);
}
