// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/contact_info_selection_mediator.h"

#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetEmailLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetContactNotificationLabelFromAutofillProfile;
}  // namespace

class PaymentRequestContactInfoSelectionMediatorTest
    : public PaymentRequestUnitTestBase,
      public PlatformTest {
 protected:
  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();
    DoSetUp();

    AddAutofillProfile(autofill::test::GetFullProfile());
    AddAutofillProfile(autofill::test::GetFullProfile2());
    autofill::AutofillProfile profile3 = autofill::test::GetFullProfile();
    // Change the name of the third profile (to avoid deduplication), and make
    // it incomplete by removing the phone number.
    profile3.SetInfo(autofill::AutofillType(autofill::NAME_FULL),
                     base::ASCIIToUTF16("Richard Roe"), "en-US");
    profile3.SetInfo(autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                     base::string16(), "en-US");
    AddAutofillProfile(std::move(profile3));

    CreateTestPaymentRequest();

    // Override the selected contact profile.
    payment_request()->set_selected_contact_profile(
        payment_request()->contact_profiles()[1]);

    mediator_ = [[ContactInfoSelectionMediator alloc]
        initWithPaymentRequest:payment_request()];
  }

  // PlatformTest:
  void TearDown() override {
    DoTearDown();
    PlatformTest::TearDown();
  }

  ContactInfoSelectionMediator* mediator() const { return mediator_; }

  ContactInfoSelectionMediator* mediator_;
};

// Tests that the expected selectable items are created and that the index of
// the selected item is properly set.
TEST_F(PaymentRequestContactInfoSelectionMediatorTest, TestSelectableItems) {
  NSArray<CollectionViewItem*>* selectable_items = [mediator() selectableItems];

  // There must be three selectable items.
  ASSERT_EQ(3U, selectable_items.count);

  // The second item must be selected.
  EXPECT_EQ(1U, mediator().selectedItemIndex);

  CollectionViewItem* item_1 = [selectable_items objectAtIndex:0];
  DCHECK([item_1 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_1 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_1);
  EXPECT_TRUE([profile_item_1.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->contact_profiles()[0])]);
  EXPECT_TRUE([profile_item_1.email
      isEqualToString:GetEmailLabelFromAutofillProfile(
                          *payment_request()->contact_profiles()[0])]);
  EXPECT_TRUE([profile_item_1.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request()->contact_profiles()[0])]);
  EXPECT_EQ(nil, profile_item_1.address);
  EXPECT_EQ(nil, profile_item_1.notification);

  CollectionViewItem* item_2 = [selectable_items objectAtIndex:1];
  DCHECK([item_2 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_2 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_2);
  EXPECT_TRUE([profile_item_2.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->contact_profiles()[1])]);
  EXPECT_TRUE([profile_item_2.email
      isEqualToString:GetEmailLabelFromAutofillProfile(
                          *payment_request()->contact_profiles()[1])]);
  EXPECT_TRUE([profile_item_2.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request()->contact_profiles()[1])]);
  EXPECT_EQ(nil, profile_item_2.address);
  EXPECT_EQ(nil, profile_item_2.notification);

  CollectionViewItem* item_3 = [selectable_items objectAtIndex:2];
  DCHECK([item_3 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_3 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_3);
  EXPECT_TRUE([profile_item_3.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->contact_profiles()[2])]);
  EXPECT_TRUE([profile_item_3.email
      isEqualToString:GetEmailLabelFromAutofillProfile(
                          *payment_request()->contact_profiles()[2])]);
  EXPECT_EQ(nil, profile_item_3.phoneNumber);
  EXPECT_EQ(nil, profile_item_3.address);
  EXPECT_TRUE([profile_item_3.notification
      isEqualToString:GetContactNotificationLabelFromAutofillProfile(
                          *payment_request(),
                          *payment_request()->contact_profiles()[2])]);
}

// Tests that the index of the selected item is as expected when there is no
// selected contact profile.
TEST_F(PaymentRequestContactInfoSelectionMediatorTest, TestNoSelectedItem) {
  // Reset the selected contact profile.
  payment_request()->set_selected_contact_profile(nullptr);
  mediator_ = [[ContactInfoSelectionMediator alloc]
      initWithPaymentRequest:payment_request()];

  NSArray<CollectionViewItem*>* selectable_items = [mediator() selectableItems];

  // There must be three selectable items.
  ASSERT_EQ(3U, selectable_items.count);

  // The selected item index must be invalid.
  EXPECT_EQ(NSUIntegerMax, mediator().selectedItemIndex);
}

// Tests that only the requested fields are displayed.
TEST_F(PaymentRequestContactInfoSelectionMediatorTest, TestOnlyRequestedData) {
  // Update the web_payment_request and reload the items.
  payment_request()->web_payment_request().options.request_payer_name = false;
  [mediator() loadItems];

  NSArray<CollectionViewItem*>* selectable_items = [mediator() selectableItems];

  CollectionViewItem* item = [selectable_items objectAtIndex:0];
  DCHECK([item isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_EQ(nil, profile_item.name);
  EXPECT_NE(nil, profile_item.email);
  EXPECT_NE(nil, profile_item.phoneNumber);
  EXPECT_EQ(nil, profile_item.address);
  EXPECT_EQ(nil, profile_item.notification);

  // Incomplete item should display a notification since the phone number is
  // missing.
  CollectionViewItem* incomplete_item = [selectable_items objectAtIndex:2];
  DCHECK([incomplete_item isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* incomplete_profile_item =
      base::mac::ObjCCastStrict<AutofillProfileItem>(incomplete_item);
  EXPECT_NE(nil, incomplete_profile_item.notification);

  // Update the web_payment_request and reload the items.
  payment_request()->web_payment_request().options.request_payer_email = false;
  [mediator() loadItems];

  selectable_items = [mediator() selectableItems];

  item = [selectable_items objectAtIndex:0];
  DCHECK([item isKindOfClass:[AutofillProfileItem class]]);
  profile_item = base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_EQ(nil, profile_item.name);
  EXPECT_EQ(nil, profile_item.email);
  EXPECT_NE(nil, profile_item.phoneNumber);
  EXPECT_EQ(nil, profile_item.address);
  EXPECT_EQ(nil, profile_item.notification);

  // Update the web_payment_request and reload the items.
  payment_request()->web_payment_request().options.request_payer_name = true;
  payment_request()->web_payment_request().options.request_payer_email = true;
  payment_request()->web_payment_request().options.request_payer_phone = false;
  [mediator() loadItems];

  selectable_items = [mediator() selectableItems];

  item = [selectable_items objectAtIndex:0];
  DCHECK([item isKindOfClass:[AutofillProfileItem class]]);
  profile_item = base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_NE(nil, profile_item.name);
  EXPECT_NE(nil, profile_item.email);
  EXPECT_EQ(nil, profile_item.phoneNumber);
  EXPECT_EQ(nil, profile_item.address);
  EXPECT_EQ(nil, profile_item.notification);

  // Incomplete item should not display a notification, since the phone number
  // is not requested.
  incomplete_item = [selectable_items objectAtIndex:2];
  DCHECK([incomplete_item isKindOfClass:[AutofillProfileItem class]]);
  profile_item =
      base::mac::ObjCCastStrict<AutofillProfileItem>(incomplete_item);
  EXPECT_EQ(nil, profile_item.notification);
}
