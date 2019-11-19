// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/shipping_address_selection_mediator.h"

#include "base/mac/foundation_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetShippingAddressLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetAddressNotificationLabelFromAutofillProfile;
}  // namespace

class PaymentRequestShippingAddressSelectionMediatorTest
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
TEST_F(PaymentRequestShippingAddressSelectionMediatorTest,
       TestSelectableItems) {
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());
  CreateTestPaymentRequest();

  ShippingAddressSelectionMediator* mediator =
      [[ShippingAddressSelectionMediator alloc]
          initWithPaymentRequest:payment_request()];

  NSArray<CollectionViewItem*>* selectable_items = [mediator selectableItems];

  ASSERT_EQ(2U, selectable_items.count);

  // The first item must be selected.
  EXPECT_EQ(0U, mediator.selectedItemIndex);

  CollectionViewItem* item_1 = [[mediator selectableItems] objectAtIndex:0];
  DCHECK([item_1 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_1 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_1);
  EXPECT_TRUE([profile_item_1.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->shipping_profiles()[0])]);
  EXPECT_TRUE([profile_item_1.address
      isEqualToString:GetShippingAddressLabelFromAutofillProfile(
                          *payment_request()->shipping_profiles()[0])]);
  EXPECT_TRUE([profile_item_1.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request()->shipping_profiles()[0])]);
  EXPECT_EQ(nil, profile_item_1.notification);
  EXPECT_TRUE(profile_item_1.complete);

  CollectionViewItem* item_2 = [[mediator selectableItems] objectAtIndex:1];
  DCHECK([item_2 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_2 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_2);
  EXPECT_TRUE([profile_item_2.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->shipping_profiles()[1])]);
  EXPECT_TRUE([profile_item_2.address
      isEqualToString:GetShippingAddressLabelFromAutofillProfile(
                          *payment_request()->shipping_profiles()[1])]);
  EXPECT_TRUE([profile_item_2.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request()->shipping_profiles()[1])]);
  EXPECT_EQ(nil, profile_item_2.notification);
  EXPECT_TRUE(profile_item_2.complete);
}

// Tests that the index of the selected item is as expected when there is no
// shipping profile.
TEST_F(PaymentRequestShippingAddressSelectionMediatorTest, TestNoItems) {
  CreateTestPaymentRequest();

  ShippingAddressSelectionMediator* mediator =
      [[ShippingAddressSelectionMediator alloc]
          initWithPaymentRequest:payment_request()];

  NSArray<CollectionViewItem*>* selectable_items = [mediator selectableItems];

  ASSERT_EQ(0U, selectable_items.count);

  // The selected item index must be invalid.
  EXPECT_EQ(NSUIntegerMax, mediator.selectedItemIndex);
}

// Tests that the expected selectable items are created and the index of the
// selected item is as expected when there is no selected shipping profile.
TEST_F(PaymentRequestShippingAddressSelectionMediatorTest, TestNoSelectedItem) {
  AddAutofillProfile(autofill::test::GetIncompleteProfile1());
  AddAutofillProfile(autofill::test::GetIncompleteProfile2());
  CreateTestPaymentRequest();

  ShippingAddressSelectionMediator* mediator =
      [[ShippingAddressSelectionMediator alloc]
          initWithPaymentRequest:payment_request()];

  NSArray<CollectionViewItem*>* selectable_items = [mediator selectableItems];

  ASSERT_EQ(2U, selectable_items.count);

  // The selected item index must be invalid.
  EXPECT_EQ(NSUIntegerMax, mediator.selectedItemIndex);

  CollectionViewItem* item_1 = [[mediator selectableItems] objectAtIndex:0];
  DCHECK([item_1 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_1 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_1);
  EXPECT_FALSE([profile_item_1.notification isEqualToString:@""]);
  EXPECT_FALSE(profile_item_1.complete);

  CollectionViewItem* item_2 = [[mediator selectableItems] objectAtIndex:0];
  DCHECK([item_2 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_2 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_2);
  EXPECT_FALSE([profile_item_2.notification isEqualToString:@""]);
  EXPECT_FALSE(profile_item_2.complete);
}
