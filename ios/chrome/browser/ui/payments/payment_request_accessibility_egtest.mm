// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "third_party/libaddressinput/messages.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using chrome_test_util::ButtonWithAccessibilityLabel;
using payment_request_util::GetEmailLabelFromAutofillProfile;
using payment_request_util::GetNameLabelFromAutofillProfile;
using payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using payment_request_util::GetShippingAddressLabelFromAutofillProfile;

// Displacement for scroll action.
const CGFloat kScrollDisplacement = 100.0;
// URL of the Payment Request test page.
const char kPaymentRequestDemoPage[] =
    "https://components/test/data/payments/payment_request.html";

// Finds the shipping address cell on the Payment Summary page.
id<GREYMatcher> ShippingAddressCellMatcher(autofill::AutofillProfile* profile) {
  return chrome_test_util::ButtonWithAccessibilityLabel([NSString
      stringWithFormat:@"%@, %@, %@", GetNameLabelFromAutofillProfile(*profile),
                       GetShippingAddressLabelFromAutofillProfile(*profile),
                       GetPhoneNumberLabelFromAutofillProfile(*profile)]);
}

// Finds the payment method cell on the Payment Summary page.
id<GREYMatcher> PaymentMethodCellMatcher(autofill::CreditCard* credit_card) {
  return chrome_test_util::ButtonWithAccessibilityLabel([NSString
      stringWithFormat:@"%@, %@",
                       base::SysUTF16ToNSString(
                           credit_card->NetworkAndLastFourDigits()),
                       base::SysUTF16ToNSString(credit_card->GetRawInfo(
                           autofill::CREDIT_CARD_NAME_FULL))]);
}

// Finds the order summary cell on the Payment Summary page.
id<GREYMatcher> PriceCellMatcher(NSString* main_label, NSString* price_label) {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      [NSString stringWithFormat:@"%@, %@", main_label, price_label]);
}

// Finds the shipping option cell on the Payment Summary page.
id<GREYMatcher> ShippingOptionCellMatcher(NSString* main_label,
                                          NSString* detail_label) {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      [NSString stringWithFormat:@"%@, %@", main_label, detail_label]);
}

// Finds the contact info cell on the Payment Summary page.
id<GREYMatcher> ContactInfoCellMatcher(autofill::AutofillProfile* profile) {
  return chrome_test_util::ButtonWithAccessibilityLabel([NSString
      stringWithFormat:@"%@, %@, %@", GetNameLabelFromAutofillProfile(*profile),
                       GetPhoneNumberLabelFromAutofillProfile(*profile),
                       GetEmailLabelFromAutofillProfile(*profile)]);
}

// Finds a required field that has a Button accessibility trait, and has a label
// of |string_id|.
id<GREYMatcher> RequiredSelectorEditorFieldMatcher(int string_id) {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      [NSString stringWithFormat:@"%@*", l10n_util::GetNSString(string_id)]);
}

}  // namespace

// Various accessibility tests for Payment Request.
@interface PaymentRequestAccessibilityEGTest : PaymentRequestEGTestBase

@end

@implementation PaymentRequestAccessibilityEGTest {
  autofill::AutofillProfile _profile;
  autofill::CreditCard _creditCard1;
  autofill::CreditCard _creditCard2;
}

#pragma mark - XCTestCase

// Set up called once before each test.
- (void)setUp {
  [super setUp];

  _profile = autofill::test::GetFullProfile();
  [self addAutofillProfile:_profile];

  _creditCard1 = autofill::test::GetCreditCard();
  _creditCard1.set_billing_address_id(_profile.guid());
  [self addCreditCard:_creditCard1];

  _creditCard2 = autofill::test::GetCreditCard2();
  [self addCreditCard:_creditCard2];

  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kPaymentRequestDemoPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];
}

#pragma mark - Tests

// Tests accessibility on the Payment Request summary page.
- (void)testAccessibilityOnPaymentRequestSummaryPage {
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests accessibility on the Payment Request order summary page.
- (void)testAccessibilityOnPaymentRequestOrderSummaryPage {
  [[EarlGrey
      selectElementWithMatcher:PriceCellMatcher(@"Donation", @"USD $55.00")]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests accessibility on the Payment Request delivery address page, add address
// page and country selector page.
- (void)testAccessibilityOnPaymentRequestShippingAddressAndSubPages {
  [[EarlGrey selectElementWithMatcher:ShippingAddressCellMatcher(&_profile)]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Tap the 'add' button to go to the Add address page.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_PAYMENTS_ADD_SHIPPING_ADDRESS_LABEL)]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Tap the 'country' button to go to the country selector page.
  [[EarlGrey
      selectElementWithMatcher:RequiredSelectorEditorFieldMatcher(
                                   IDS_LIBADDRESSINPUT_COUNTRY_OR_REGION_LABEL)]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests accessibility on the Payment Request delivery method page.
- (void)testAccessibilityOnPaymentRequestDeliveryMethodPage {
  [[EarlGrey selectElementWithMatcher:ShippingOptionCellMatcher(
                                          @"Standard shipping", @"$0.00")]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests accessibility on the Payment Request payment method page, add credit
// card page and billing address selection page.
- (void)testAccessibilityOnPaymentRequestPaymentMethodAndSubPages {
  [[[EarlGrey selectElementWithMatcher:PaymentMethodCellMatcher(&_creditCard1)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollDisplacement)
      onElementWithMatcher:chrome_test_util::PaymentRequestView()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Tap the 'add' button to go to the Add credit card page.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_PAYMENTS_ADD_CARD)]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Tap the 'billing address' button.
  [[EarlGrey selectElementWithMatcher:RequiredSelectorEditorFieldMatcher(
                                          IDS_PAYMENTS_BILLING_ADDRESS)]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests accessibility on the Payment Request contact info page, and add contact
// info page.
- (void)testAccessibilityOnPaymentRequestContactInfoAndSubPages {
  [[[EarlGrey selectElementWithMatcher:ContactInfoCellMatcher(&_profile)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollDisplacement)
      onElementWithMatcher:chrome_test_util::PaymentRequestView()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Tap the 'add' button to go to the Add contact info page.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_PAYMENTS_ADD_CONTACT_DETAILS_LABEL)]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests accessibility on the card unmask prompt.
- (void)testAccessibilityOnPaymentRequestCardUnmaskPage {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_PAYMENTS_PAY_BUTTON)]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

@end
