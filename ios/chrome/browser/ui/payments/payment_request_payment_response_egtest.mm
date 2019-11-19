// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/payment_request_cache.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::GetCurrentWebState;

// URLs of the test pages.
const char kNoShippingPage[] =
    "https://components/test/data/payments/"
    "payment_request_no_shipping_test.html";
const char kFreeShippingPage[] =
    "https://components/test/data/payments/"
    "payment_request_free_shipping_test.html";
const char kContactDetailsPage[] =
    "https://components/test/data/payments/"
    "payment_request_contact_details_and_free_shipping_test.html";
const char kRequestEmailPage[] =
    "https://components/test/data/payments/"
    "payment_request_email_and_free_shipping_test.html";

}  // namepsace

// Various tests for the validity of the payment response generated with an
// autofill payment instrument.
@interface PaymentRequestPaymentResponseAutofillPaymentInstrumentEGTest
    : PaymentRequestEGTestBase

@end

@implementation PaymentRequestPaymentResponseAutofillPaymentInstrumentEGTest

#pragma mark - Tests

// Tests that the PaymentResponse contains all the required fields for an
// Autofill payment instrument.
- (void)testPaymentResponseNoShipping {
  // Create a billing address and a card that uses it.
  autofill::AutofillProfile billingAddress = autofill::test::GetFullProfile();
  [self addAutofillProfile:billingAddress];
  autofill::CreditCard card = autofill::test::GetCreditCard();  // visa
  card.set_billing_address_id(billingAddress.guid());
  [self addCreditCard:card];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kNoShippingPage)];

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

  // Test that the card details were sent to the merchant.
  [self waitForWebViewContainingTexts:{"\"methodName\": \"basic-card\"",
                                       "\"details\": {",
                                       "\"cardNumber\": \"4111111111111111\"",
                                       "\"cardSecurityCode\": \"123\"",
                                       "\"cardholderName\": \"Test User\"",
                                       "\"expiryMonth\": \"11\"",
                                       "\"expiryYear\": \"2022\""}];

  // Test that the billing address was sent to the merchant.
  [self
      waitForWebViewContainingTexts:{"\"billingAddress\": {",
                                     "\"addressLine\": [", "\"666 Erebus St.\"",
                                     "\"Apt 8\"", "\"city\": \"Elysium\"",
                                     "\"country\": \"US\"",
                                     "\"dependentLocality\": \"\"",
                                     "\"organization\": \"Underworld\"",
                                     "\"phone\": \"+16502111111\"",
                                     "\"postalCode\": \"91111\"",
                                     "\"recipient\": \"John H. Doe\"",
                                     "\"region\": \"CA\"",
                                     "\"sortingCode\": \"\""}];

  // Test that the no shipping address, shipping option, or contact details was
  // sent to the merchant.
  [self waitForWebViewContainingTexts:{"\"shippingAddress\": null",
                                       "\"shippingOption\": null",
                                       "\"payerName\": null",
                                       "\"payerEmail\": null",
                                       "\"payerPhone\": null"}];
}

// Tests that the PaymentResponse contains all the required fields for a
// shipping address and shipping option.
- (void)testPaymentResponseFreeShipping {
  // Create a billing address and a card that uses it.
  autofill::AutofillProfile billingAddress = autofill::test::GetFullProfile();
  [self addAutofillProfile:billingAddress];
  autofill::CreditCard card = autofill::test::GetCreditCard();  // visa
  card.set_billing_address_id(billingAddress.guid());
  [self addCreditCard:card];

  // Create a shipping address with a higher frecency score, so that it is
  // selected as the default shipping address.
  autofill::AutofillProfile shippingAddress = autofill::test::GetFullProfile2();
  shippingAddress.set_use_count(2000);
  [self addAutofillProfile:shippingAddress];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kFreeShippingPage)];

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

  // Test that the billing address was sent to the merchant.
  [self waitForWebViewContainingTexts:{"\"shippingAddress\": {",
                                       "\"addressLine\": [",
                                       "\"123 Main Street\"", "\"Unit 1\"",
                                       "\"city\": \"Greensdale\"",
                                       "\"country\": \"US\"",
                                       "\"dependentLocality\": \"\"",
                                       "\"organization\": \"ACME\"",
                                       "\"phone\": \"+13105557889\"",
                                       "\"postalCode\": \"48838\"",
                                       "\"recipient\": \"Jane A. Smith\"",
                                       "\"region\": \"MI\"",
                                       "\"sortingCode\": \"\""}];

  // Test that the shipping option was sent to the merchant.
  [self waitForWebViewContainingTexts:
            {"\"shippingOption\": \"freeShippingOption\""}];

  // Test that the no contact details was sent to the merchant.
  [self waitForWebViewContainingTexts:{"\"payerName\": null",
                                       "\"payerEmail\": null",
                                       "\"payerPhone\": null"}];
}

// Tests that the PaymentResponse contains all the required fields for contact
// details when all three details are requested.
- (void)testPaymentResponseAllContactDetails {
  // Create a billing address and a card that uses it.
  autofill::AutofillProfile billingAddress = autofill::test::GetFullProfile();
  [self addAutofillProfile:billingAddress];

  autofill::CreditCard card = autofill::test::GetCreditCard();  // visa
  card.set_billing_address_id(billingAddress.guid());
  [self addCreditCard:card];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kContactDetailsPage)];

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

  // Test that the contact details were sent to the merchant.
  [self waitForWebViewContainingTexts:{"\"payerName\": \"John H. Doe\"",
                                       "\"payerEmail\": \"johndoe@hades.com\"",
                                       "\"payerPhone\": \"+16502111111\""}];
}

// Tests that the PaymentResponse contains all the required fields for contact
// details when only one detail is requested.
- (void)testPaymentResponseOneContactDetail {
  // Create a billing address and a card that uses it.
  autofill::AutofillProfile billingAddress = autofill::test::GetFullProfile();
  [self addAutofillProfile:billingAddress];

  autofill::CreditCard card = autofill::test::GetCreditCard();  // visa
  card.set_billing_address_id(billingAddress.guid());
  [self addCreditCard:card];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kRequestEmailPage)];

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

  // Test that the contact details were sent to the merchant.
  [self waitForWebViewContainingTexts:{"\"payerName\": null",
                                       "\"payerEmail\": \"johndoe@hades.com\"",
                                       "\"payerPhone\": null"}];
}

@end
