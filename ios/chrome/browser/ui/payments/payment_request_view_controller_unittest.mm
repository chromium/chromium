// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_view_controller.h"

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/ui/payments/cells/page_info_item.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_view_controller_data_source.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestPaymentRequestMediator
    : NSObject<PaymentRequestViewControllerDataSource>

@end

@implementation TestPaymentRequestMediator

- (BOOL)canPay {
  return YES;
}

- (BOOL)hasPaymentItems {
  return YES;
}

- (BOOL)requestShipping {
  return YES;
}

- (BOOL)requestContactInfo {
  return YES;
}

- (CollectionViewItem*)paymentSummaryItem {
  return [[PriceItem alloc] init];
}

- (CollectionViewItem*)shippingSectionHeaderItem {
  return [[PaymentsTextItem alloc] init];
}

- (CollectionViewItem*)shippingAddressItem {
  return [[AutofillProfileItem alloc] init];
}

- (CollectionViewItem*)shippingOptionItem {
  return [[PaymentsTextItem alloc] init];
}

- (CollectionViewItem*)paymentMethodSectionHeaderItem {
  return [[PaymentsTextItem alloc] init];
}

- (CollectionViewItem*)paymentMethodItem {
  return [[PaymentMethodItem alloc] init];
}

- (CollectionViewItem*)contactInfoSectionHeaderItem {
  return [[PaymentsTextItem alloc] init];
}

- (CollectionViewItem*)contactInfoItem {
  return [[AutofillProfileItem alloc] init];
}

- (CollectionViewFooterItem*)footerItem {
  return [[CollectionViewFooterItem alloc] init];
}

@end

@interface TestPaymentRequestMediatorNoShipping : TestPaymentRequestMediator

@end

@implementation TestPaymentRequestMediatorNoShipping

- (BOOL)requestShipping {
  return NO;
}

@end

@interface TestPaymentRequestMediatorNoContactInfo : TestPaymentRequestMediator

@end

@implementation TestPaymentRequestMediatorNoContactInfo

- (BOOL)requestContactInfo {
  return NO;
}

@end

@interface TestPaymentRequestMediatorCantShip : TestPaymentRequestMediator

@end

class PaymentRequestViewControllerTest : public CollectionViewControllerTest,
                                         public PaymentRequestUnitTestBase {
 protected:
  // CollectionViewControllerTest:
  void SetUp() override {
    CollectionViewControllerTest::SetUp();
    DoSetUp();

    mediator_ = [[TestPaymentRequestMediator alloc] init];
  }

  // CollectionViewControllerTest:
  void TearDown() override {
    DoTearDown();
    CollectionViewControllerTest::TearDown();
  }

  CollectionViewController* InstantiateController() override {
    PaymentRequestViewController* viewController =
        [[PaymentRequestViewController alloc] init];
    [viewController setDataSource:mediator_];
    return viewController;
  }

  PaymentRequestViewController* GetPaymentRequestViewController() {
    return base::mac::ObjCCastStrict<PaymentRequestViewController>(
        controller());
  }

  TestPaymentRequestMediator* mediator_;
};

// Tests that the correct items are displayed after loading the model.
TEST_F(PaymentRequestViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_PAYMENTS_TITLE);

  [GetPaymentRequestViewController() loadModel];

  // There should be five sections in total. Summary, Shipping, Payment Method,
  // Contact Info and the Footer.
  ASSERT_EQ(5, NumberOfSections());

  // There should be two items in the Summary section
  ASSERT_EQ(2U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The first one should be of type PageInfoItem.
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[PageInfoItem class]]);

  // The next item should be of type PriceItem.
  item = GetCollectionViewItem(0, 1);
  EXPECT_TRUE([item isMemberOfClass:[PriceItem class]]);

  // There should be two items in the Shipping section.
  ASSERT_EQ(2U, static_cast<unsigned int>(NumberOfItemsInSection(1)));

  // The first one should be of type AutofillProfileItem.
  item = GetCollectionViewItem(1, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);

  // The next item should be of type PaymentsTextItem.
  item = GetCollectionViewItem(1, 1);
  EXPECT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);

  // The only item in the Payment Method section should be of type
  // PaymentMethodItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(2)));
  item = GetCollectionViewItem(2, 0);
  EXPECT_TRUE([item isMemberOfClass:[PaymentMethodItem class]]);

  // The only item in the Contact Info section should be of type
  // AutofillProfileItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(3)));
  item = GetCollectionViewItem(3, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);

  // The only item in the Footer section should be of type
  // CollectionViewFooterItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(4)));
  item = GetCollectionViewItem(4, 0);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewFooterItem class]]);
}

// Tests that the correct items are displayed after loading the model, when no
// shipping information is requested.
TEST_F(PaymentRequestViewControllerTest, TestModelNoShipping) {
  mediator_ = [[TestPaymentRequestMediatorNoShipping alloc] init];

  CreateController();
  CheckController();

  // There should be four sections in total now.
  ASSERT_EQ(4, NumberOfSections());

  // The second section is the Payment Method section isntead of the Shipping
  // section.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(1)));
  CollectionViewItem* item = GetCollectionViewItem(1, 0);
  EXPECT_TRUE([item isMemberOfClass:[PaymentMethodItem class]]);
}

// Tests that the correct items are displayed after loading the model, when no
// contact information is requested.
TEST_F(PaymentRequestViewControllerTest, TestModelNoContactInfo) {
  mediator_ = [[TestPaymentRequestMediatorNoContactInfo alloc] init];

  CreateController();
  CheckController();

  // There should be four sections in total now.
  ASSERT_EQ(4, NumberOfSections());

  // The fourth section is the Footer section instead of the Contact Info
  // section.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(3)));
  CollectionViewItem* item = GetCollectionViewItem(3, 0);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewFooterItem class]]);
}

// Tests that the correct items are displayed after updating the Shipping
// section.
TEST_F(PaymentRequestViewControllerTest, TestUpdateShippingSection) {
  CreateController();
  CheckController();

  [GetPaymentRequestViewController() reloadShippingSection];

  // There should be two items in the Shipping section.
  ASSERT_EQ(2U, static_cast<unsigned int>(NumberOfItemsInSection(1)));

  // The first one should be of type AutofillProfileItem.
  id item = GetCollectionViewItem(1, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);

  // The next item should be of type PaymentsTextItem.
  item = GetCollectionViewItem(1, 1);
  EXPECT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
}

// Tests that the correct items are displayed after updating the Payment Method
// section.
TEST_F(PaymentRequestViewControllerTest, TestUpdatePaymentMethodSection) {
  CreateController();
  CheckController();

  [GetPaymentRequestViewController() reloadPaymentMethodSection];

  // The only item in the Payment Method section should be of type
  // PaymentMethodItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(2)));
  id item = GetCollectionViewItem(2, 0);
  EXPECT_TRUE([item isMemberOfClass:[PaymentMethodItem class]]);
}

// Tests that the correct items are displayed after updating the Contact Info
// section.
TEST_F(PaymentRequestViewControllerTest, TestUpdateContactInfoSection) {
  CreateController();
  CheckController();

  [GetPaymentRequestViewController() reloadPaymentMethodSection];

  // The only item in the Contact Info section should be of type
  // AutofillProfileItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(3)));
  id item = GetCollectionViewItem(3, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
}

// Tests that the correct items are displayed after loading the model, when
// the view is in pending state.
TEST_F(PaymentRequestViewControllerTest, TestModelPendingState) {
  CreateController();
  CheckController();

  [GetPaymentRequestViewController() setPending:YES];
  [GetPaymentRequestViewController() loadModel];

  // There should only be one section which has two items.
  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(2U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The first item should be of type PageInfoItem.
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[PageInfoItem class]]);

  // The second item should be of type StatusItem.
  item = GetCollectionViewItem(0, 1);
  EXPECT_TRUE([item isMemberOfClass:[StatusItem class]]);
}
