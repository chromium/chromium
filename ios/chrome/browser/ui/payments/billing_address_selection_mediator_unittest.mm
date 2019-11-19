// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/billing_address_selection_mediator.h"

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
using ::payment_request_util::GetBillingAddressLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetAddressNotificationLabelFromAutofillProfile;
}  // namespace

class PaymentRequestBillingAddressSelectionMediatorTest
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
TEST_F(PaymentRequestBillingAddressSelectionMediatorTest, TestSelectableItems) {
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());
  CreateTestPaymentRequest();

  BillingAddressSelectionMediator* mediator =
      [[BillingAddressSelectionMediator alloc]
          initWithPaymentRequest:payment_request()
          selectedBillingProfile:payment_request()->billing_profiles()[1]];

  NSArray<CollectionViewItem*>* selectable_items = [mediator selectableItems];

  ASSERT_EQ(2U, selectable_items.count);

  // The second item must be selected.
  EXPECT_EQ(1U, mediator.selectedItemIndex);

  CollectionViewItem* item_1 = [[mediator selectableItems] objectAtIndex:0];
  DCHECK([item_1 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_1 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_1);
  EXPECT_TRUE([profile_item_1.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[0])]);
  EXPECT_TRUE([profile_item_1.address
      isEqualToString:GetBillingAddressLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[0])]);
  EXPECT_TRUE([profile_item_1.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[0])]);
  EXPECT_EQ(nil, profile_item_1.notification);
  EXPECT_TRUE(profile_item_1.complete);

  CollectionViewItem* item_2 = [[mediator selectableItems] objectAtIndex:1];
  DCHECK([item_2 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_2 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_2);
  EXPECT_TRUE([profile_item_2.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[1])]);
  EXPECT_TRUE([profile_item_2.address
      isEqualToString:GetBillingAddressLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[1])]);
  EXPECT_TRUE([profile_item_2.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[1])]);
  EXPECT_EQ(nil, profile_item_2.notification);
  EXPECT_TRUE(profile_item_2.complete);
}

// Tests that the index of the selected item is as expected when there is no
// billing profile.
TEST_F(PaymentRequestBillingAddressSelectionMediatorTest, TestNoItems) {
  CreateTestPaymentRequest();

  BillingAddressSelectionMediator* mediator =
      [[BillingAddressSelectionMediator alloc]
          initWithPaymentRequest:payment_request()
          selectedBillingProfile:nil];

  NSArray<CollectionViewItem*>* selectable_items = [mediator selectableItems];

  ASSERT_EQ(0U, selectable_items.count);

  // The selected item index must be invalid.
  EXPECT_EQ(NSUIntegerMax, mediator.selectedItemIndex);
}

// Tests that the expected selectable items are created and the index of the
// selected item is as expected when there is no selected billing profile.
TEST_F(PaymentRequestBillingAddressSelectionMediatorTest, TestNoSelectedItem) {
  AddAutofillProfile(autofill::test::GetIncompleteProfile1());
  AddAutofillProfile(autofill::test::GetIncompleteProfile2());
  CreateTestPaymentRequest();

  BillingAddressSelectionMediator* mediator =
      [[BillingAddressSelectionMediator alloc]
          initWithPaymentRequest:payment_request()
          selectedBillingProfile:nil];

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
