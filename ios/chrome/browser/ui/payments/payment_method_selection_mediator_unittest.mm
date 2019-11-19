// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_method_selection_mediator.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetBillingAddressLabelFromAutofillProfile;
}  // namespace

class PaymentRequestPaymentMethodSelectionMediatorTest
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

// Tests that the expected selectable items are created and that the index of
// the selected item is properly set.
TEST_F(PaymentRequestPaymentMethodSelectionMediatorTest, TestSelectableItems) {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  autofill::CreditCard card1 = autofill::test::GetCreditCard();
  card1.set_billing_address_id(profile.guid());
  AddCreditCard(std::move(card1));
  autofill::CreditCard card2 = autofill::test::GetCreditCard2();
  // Set the use count so it gets selected by default.
  card2.set_use_count(10U);
  card2.set_billing_address_id(profile.guid());
  AddCreditCard(std::move(card2));
  AddAutofillProfile(std::move(profile));
  CreateTestPaymentRequest();

  PaymentMethodSelectionMediator* mediator =
      [[PaymentMethodSelectionMediator alloc]
          initWithPaymentRequest:payment_request()];

  NSArray<CollectionViewItem*>* selectable_items = [mediator selectableItems];

  ASSERT_EQ(2U, selectable_items.count);

  // The first item must be selected.
  EXPECT_EQ(0U, mediator.selectedItemIndex);

  CollectionViewItem* item_1 = [[mediator selectableItems] objectAtIndex:0];
  DCHECK([item_1 isKindOfClass:[PaymentMethodItem class]]);
  PaymentMethodItem* payment_method_item_1 =
      base::mac::ObjCCastStrict<PaymentMethodItem>(item_1);
  EXPECT_TRUE([payment_method_item_1.methodID
      isEqualToString:base::SysUTF16ToNSString(
                          card2.NetworkAndLastFourDigits())]);
  EXPECT_TRUE([payment_method_item_1.methodDetail
      isEqualToString:base::SysUTF16ToNSString(card2.GetInfo(
                          autofill::AutofillType(
                              autofill::CREDIT_CARD_NAME_FULL),
                          payment_request()->GetApplicationLocale()))]);
  EXPECT_TRUE([payment_method_item_1.methodAddress
      isEqualToString:GetBillingAddressLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[0])]);
  EXPECT_EQ(nil, payment_method_item_1.notification);
  EXPECT_TRUE(payment_method_item_1.complete);

  CollectionViewItem* item_2 = [[mediator selectableItems] objectAtIndex:1];
  DCHECK([item_2 isKindOfClass:[PaymentMethodItem class]]);
  PaymentMethodItem* payment_method_item_2 =
      base::mac::ObjCCastStrict<PaymentMethodItem>(item_2);
  EXPECT_TRUE([payment_method_item_2.methodID
      isEqualToString:base::SysUTF16ToNSString(
                          card1.NetworkAndLastFourDigits())]);
  EXPECT_TRUE([payment_method_item_2.methodDetail
      isEqualToString:base::SysUTF16ToNSString(card1.GetInfo(
                          autofill::AutofillType(
                              autofill::CREDIT_CARD_NAME_FULL),
                          payment_request()->GetApplicationLocale()))]);
  EXPECT_TRUE([payment_method_item_2.methodAddress
      isEqualToString:GetBillingAddressLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[0])]);
  EXPECT_EQ(nil, payment_method_item_2.notification);
  EXPECT_TRUE(payment_method_item_2.complete);
}

// Tests that the index of the selected item is as expected when there is no
// payment method.
TEST_F(PaymentRequestPaymentMethodSelectionMediatorTest, TestNoItems) {
  CreateTestPaymentRequest();

  PaymentMethodSelectionMediator* mediator =
      [[PaymentMethodSelectionMediator alloc]
          initWithPaymentRequest:payment_request()];

  NSArray<CollectionViewItem*>* selectable_items = [mediator selectableItems];

  ASSERT_EQ(0U, selectable_items.count);
}

// Tests that the expected selectable items are created and the index of the
// selected item is as expected when there is no selected billing profile.
TEST_F(PaymentRequestPaymentMethodSelectionMediatorTest, TestNoSelectedItem) {
  AddCreditCard(autofill::test::GetCreditCard());
  AddCreditCard(autofill::test::GetCreditCard2());
  CreateTestPaymentRequest();

  PaymentMethodSelectionMediator* mediator =
      [[PaymentMethodSelectionMediator alloc]
          initWithPaymentRequest:payment_request()];

  NSArray<CollectionViewItem*>* selectable_items = [mediator selectableItems];

  ASSERT_EQ(2U, selectable_items.count);

  // The selected item index must be invalid.
  EXPECT_EQ(NSUIntegerMax, mediator.selectedItemIndex);

  CollectionViewItem* item_1 = [[mediator selectableItems] objectAtIndex:0];
  DCHECK([item_1 isKindOfClass:[PaymentMethodItem class]]);
  PaymentMethodItem* payment_method_item_1 =
      base::mac::ObjCCastStrict<PaymentMethodItem>(item_1);
  EXPECT_FALSE([payment_method_item_1.notification isEqualToString:@""]);
  EXPECT_FALSE(payment_method_item_1.complete);

  CollectionViewItem* item_2 = [[mediator selectableItems] objectAtIndex:0];
  DCHECK([item_2 isKindOfClass:[PaymentMethodItem class]]);
  PaymentMethodItem* payment_method_item_2 =
      base::mac::ObjCCastStrict<PaymentMethodItem>(item_2);
  EXPECT_FALSE([payment_method_item_2.notification isEqualToString:@""]);
  EXPECT_FALSE(payment_method_item_2.complete);
}
