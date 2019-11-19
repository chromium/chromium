// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const base::Time kSomeDate = base::Time::FromDoubleT(1484505871);
const base::Time kSomeLaterDate = base::Time::FromDoubleT(1497552271);

using chrome_test_util::ButtonWithAccessibilityLabelId;

// URLs of the test pages.
const char kNoShippingPage[] =
    "https://components/test/data/payments/"
    "payment_request_no_shipping_test.html";
const char kFreeShippingPage[] =
    "https://components/test/data/payments/"
    "payment_request_free_shipping_test.html";
const char kRequestNamePage[] =
    "https://components/test/data/payments/"
    "payment_request_name_test.html";
const char kContactDetailsFreeShippingPage[] =
    "https://components/test/data/payments/"
    "payment_request_contact_details_and_free_shipping_test.html";

}  // namepsace

// Various tests to validate that the use stats for the autofill cards and
// profiles used in a Payment Request are properly updated upon completion.
@interface PaymentRequestPaymentUseStatsEGTest : PaymentRequestEGTestBase

@end

@implementation PaymentRequestPaymentUseStatsEGTest

#pragma mark - Helper methods

// Sets up a credit card with an associated billing address.
- (void)setUpCreditCard {
  autofill::AutofillProfile billingAddress = autofill::test::GetFullProfile();
  [self addAutofillProfile:billingAddress];
  autofill::CreditCard card = autofill::test::GetCreditCard();  // visa
  card.set_billing_address_id(billingAddress.guid());
  [self addCreditCard:card];
}

// Completes the Payment Request.
- (void)completePayment {
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Tap the buy button.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_PAYMENTS_PAY_BUTTON)]
      performAction:grey_tap()];

  // Type in the CVC.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"CVC_textField")]
      performAction:grey_replaceText(@"123")];

  // Tap the confirm button.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_AUTOFILL_CARD_UNMASK_CONFIRM_BUTTON)]
      performAction:grey_tap()];
}

#pragma mark - Tests

// Tests that use stats for the autofill card used in a Payment Request are
// properly updated upon completion. The use stats for the billing address
// associated with the card is expected not to change.
- (void)testRecordUseOfCard {
  if (@available(iOS 13, *)) {
    // TODO(crbug.com/1007432): Enable this test.
    EARL_GREY_TEST_DISABLED(@"The test is flaky on iOS 13");
  }

  autofill::TestAutofillClock testClock;
  testClock.SetNow(kSomeDate);

  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billingAddress = autofill::test::GetFullProfile();
  [self addAutofillProfile:billingAddress];

  autofill::CreditCard card = autofill::test::GetCreditCard();  // visa
  card.set_billing_address_id(billingAddress.guid());
  [self addCreditCard:card];

  // Check that the initial use stats were set correctly.
  autofill::CreditCard* initialCard =
      [self personalDataManager]->GetCreditCardByGUID(card.guid());
  EXPECT_EQ(1U, initialCard->use_count());
  EXPECT_EQ(kSomeDate, initialCard->use_date());

  autofill::AutofillProfile* initialBilling =
      [self personalDataManager]->GetProfileByGUID(billingAddress.guid());
  EXPECT_EQ(1U, initialBilling->use_count());
  EXPECT_EQ(kSomeDate, initialBilling->use_date());

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kNoShippingPage)];

  testClock.SetNow(kSomeLaterDate);
  [self completePayment];

  // Check that the usage of the card was recorded.
  autofill::CreditCard* updatedCard =
      [self personalDataManager]->GetCreditCardByGUID(card.guid());
  EXPECT_EQ(2U, updatedCard->use_count());
  EXPECT_EQ(kSomeLaterDate, updatedCard->use_date());

  // Check that the usage of the profile was not recorded.
  autofill::AutofillProfile* updatedBilling =
      [self personalDataManager]->GetProfileByGUID(billingAddress.guid());
  EXPECT_EQ(1U, updatedBilling->use_count());
  EXPECT_EQ(kSomeDate, updatedBilling->use_date());
}

// Tests that use stats for the shipping address used in a Payment Request are
// properly updated upon completion.
- (void)testRecordUseOfShippingAddress {
  if (@available(iOS 13, *)) {
    // TODO(crbug.com/1007432): Enable this test.
    EARL_GREY_TEST_DISABLED(@"The test is flaky on iOS 13");
  }

  autofill::TestAutofillClock testClock;
  testClock.SetNow(kSomeDate);

  [self setUpCreditCard];

  // Create a shipping address with a higher frecency score, so that it is
  // selected as the default shipping address.
  autofill::AutofillProfile shippingAddress = autofill::test::GetFullProfile2();
  shippingAddress.set_use_count(3);
  [self addAutofillProfile:shippingAddress];

  // Check that the initial use stats were set correctly.
  autofill::AutofillProfile* initialShipping =
      [self personalDataManager]->GetProfileByGUID(shippingAddress.guid());
  EXPECT_EQ(3U, initialShipping->use_count());
  EXPECT_EQ(kSomeDate, initialShipping->use_date());

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kFreeShippingPage)];

  testClock.SetNow(kSomeLaterDate);
  [self completePayment];

  // Check that the usage of the profile was recorded.
  autofill::AutofillProfile* updatedShipping =
      [self personalDataManager]->GetProfileByGUID(shippingAddress.guid());
  EXPECT_EQ(4U, updatedShipping->use_count());
  EXPECT_EQ(kSomeLaterDate, updatedShipping->use_date());
}

// Tests that use stats for the contact address used in a Payment Request are
// properly updated upon completion.
- (void)testRecordUseOfContactAddress {
  if (@available(iOS 13, *)) {
    // TODO(crbug.com/1007432): Enable this test.
    EARL_GREY_TEST_DISABLED(@"The test is flaky on iOS 13");
  }

  autofill::TestAutofillClock testClock;
  testClock.SetNow(kSomeDate);

  [self setUpCreditCard];

  // Create a contact address with a higher frecency score, so that it is
  // selected as the default shipping address.
  autofill::AutofillProfile contactAddress = autofill::test::GetFullProfile2();
  contactAddress.set_use_count(3);
  [self addAutofillProfile:contactAddress];

  // Check that the initial use stats were set correctly.
  autofill::AutofillProfile* initialContact =
      [self personalDataManager]->GetProfileByGUID(contactAddress.guid());
  EXPECT_EQ(3U, initialContact->use_count());
  EXPECT_EQ(kSomeDate, initialContact->use_date());

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kRequestNamePage)];

  testClock.SetNow(kSomeLaterDate);
  [self completePayment];

  // Check that the usage of the profile was recorded.
  autofill::AutofillProfile* updatedContact =
      [self personalDataManager]->GetProfileByGUID(contactAddress.guid());
  EXPECT_EQ(4U, updatedContact->use_count());
  EXPECT_EQ(kSomeLaterDate, updatedContact->use_date());
}

// Tests that use stats for an address that was used both as a shipping and
// contact address in a Payment Request are properly updated upon completion.
- (void)testRecordUseOfContactAndShippingAddress {
  if (@available(iOS 13, *)) {
    // TODO(crbug.com/1007432): Enable this test.
    EARL_GREY_TEST_DISABLED(@"The test is flaky on iOS 13");
  }

  autofill::TestAutofillClock testClock;
  testClock.SetNow(kSomeDate);

  [self setUpCreditCard];

  // Create an address with a higher frecency score, so that it is selected as
  // the default shipping and contact address.
  autofill::AutofillProfile multiAddress = autofill::test::GetFullProfile2();
  multiAddress.set_use_count(3);
  [self addAutofillProfile:multiAddress];

  // Check that the initial use stats were set correctly.
  autofill::AutofillProfile* initialAddress =
      [self personalDataManager]->GetProfileByGUID(multiAddress.guid());
  EXPECT_EQ(3U, initialAddress->use_count());
  EXPECT_EQ(kSomeDate, initialAddress->use_date());

  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kContactDetailsFreeShippingPage)];

  testClock.SetNow(kSomeLaterDate);
  [self completePayment];

  // Check that the usage of the profile was only recorded once.
  autofill::AutofillProfile* updatedAddress =
      [self personalDataManager]->GetProfileByGUID(multiAddress.guid());
  EXPECT_EQ(4U, updatedAddress->use_count());
  EXPECT_EQ(kSomeLaterDate, updatedAddress->use_date());
}

@end
