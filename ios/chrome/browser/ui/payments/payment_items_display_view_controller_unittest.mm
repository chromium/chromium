// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_items_display_view_controller.h"

#include "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#import "ios/chrome/browser/ui/payments/payment_items_display_view_controller_data_source.h"
#include "ios/chrome/grit/ios_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestPaymentItemsDisplayMediator
    : NSObject<PaymentItemsDisplayViewControllerDataSource>

@end

@implementation TestPaymentItemsDisplayMediator

#pragma mark - PaymentItemsDisplayViewControllerDataSource

- (BOOL)canPay {
  return YES;
}

- (CollectionViewItem*)totalItem {
  return [[PriceItem alloc] init];
}

- (NSArray<CollectionViewItem*>*)lineItems {
  return @[ [[PriceItem alloc] init] ];
}

@end

class PaymentRequestPaymentItemsDisplayViewControllerTest
    : public CollectionViewControllerTest,
      public PaymentRequestUnitTestBase {
 protected:
  // CollectionViewControllerTest:
  void SetUp() override {
    CollectionViewControllerTest::SetUp();
    DoSetUp();
  }

  // CollectionViewControllerTest:
  void TearDown() override {
    DoTearDown();
    CollectionViewControllerTest::TearDown();
  }

  CollectionViewController* InstantiateController() override {
    mediator_ = [[TestPaymentItemsDisplayMediator alloc] init];
    PaymentItemsDisplayViewController* viewController =
        [[PaymentItemsDisplayViewController alloc] init];
    [viewController setDataSource:mediator_];
    return viewController;
  }

  PaymentItemsDisplayViewController* GetPaymentItemsViewController() {
    return base::mac::ObjCCastStrict<PaymentItemsDisplayViewController>(
        controller());
  }

 private:
  TestPaymentItemsDisplayMediator* mediator_ = nil;
};

// Tests that the correct number of items are displayed after loading the model.
TEST_F(PaymentRequestPaymentItemsDisplayViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_PAYMENTS_ORDER_SUMMARY_LABEL);

  [GetPaymentItemsViewController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be one item for the total and another one for sub-total.
  ASSERT_EQ(2U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // They both should be of type PriceItem.
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[PriceItem class]]);
  item = GetCollectionViewItem(0, 1);
  EXPECT_TRUE([item isMemberOfClass:[PriceItem class]]);
}
