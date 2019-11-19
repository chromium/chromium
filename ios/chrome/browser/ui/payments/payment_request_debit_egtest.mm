// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include <memory>
#include <vector>

#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/payments/core/features.h"
#include "components/payments/core/payment_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/payments/payment_request_util.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
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

constexpr auto CREDIT = ::autofill::CreditCard::CardType::CARD_TYPE_CREDIT;
constexpr auto DEBIT = ::autofill::CreditCard::CardType::CARD_TYPE_DEBIT;
constexpr auto PREPAID = ::autofill::CreditCard::CardType::CARD_TYPE_PREPAID;
constexpr auto UNKNOWN = ::autofill::CreditCard::CardType::CARD_TYPE_UNKNOWN;

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using payment_request_util::GetBillingAddressLabelFromAutofillProfile;

// URLs of the test pages.
const char kDebitPage[] =
    "https://components/test/data/payments/payment_request_debit_test.html";

// Matcher for the PaymentMethodCell.
id<GREYMatcher> PaymentMethodCellMatcher(
    const autofill::CreditCard& credit_card,
    const autofill::AutofillProfile& billing_profile) {
  NSString* billing_address_label =
      GetBillingAddressLabelFromAutofillProfile(billing_profile);
  return chrome_test_util::ButtonWithAccessibilityLabel([NSString
      stringWithFormat:@"%@, %@, %@",
                       base::SysUTF16ToNSString(
                           credit_card.NetworkAndLastFourDigits()),
                       base::SysUTF16ToNSString(credit_card.GetRawInfo(
                           autofill::CREDIT_CARD_NAME_FULL)),
                       billing_address_label]);
}

}  // namepsace

// Various tests for a merchant that requests a debit card.
@interface PaymentRequestDebitEGTest : PaymentRequestEGTestBase

@end

std::unique_ptr<autofill::AutofillProfile> _profile;

@implementation PaymentRequestDebitEGTest

- (void)setUp {
  [super setUp];
  _profile = std::make_unique<autofill::AutofillProfile>(
      autofill::test::GetFullProfile());
  [self addAutofillProfile:*_profile];

  // Allow canMakePayment to return a truthful value by default.
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->SetBoolean(payments::kCanMakePaymentEnabled, true);
}

- (void)addServerCardWithType:(autofill::CreditCard::CardType)cardType {
  autofill::CreditCard card = autofill::test::GetMaskedServerCard();
  card.set_card_type(cardType);
  card.set_billing_address_id(_profile->guid());
  [self addServerCreditCard:card];
}

#pragma mark - Tests

// Tests that canMakePayment() resolves with true with a debit card.
- (void)testCanMakePaymentWithDebitCard {
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(
      payments::features::kReturnGooglePayInBasicCard);

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kDebitPage)];

  [self addServerCardWithType:DEBIT];

  [ChromeEarlGrey tapWebStateElementWithID:@"canMakePayment"];

  [self waitForWebViewContainingTexts:{"true"}];
}

// Tests that canMakePayment() resolves with true with an "unknown" card.
- (void)testCanMakePaymentWithUnknownCardType {
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(
      payments::features::kReturnGooglePayInBasicCard);

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kDebitPage)];

  [self addServerCardWithType:UNKNOWN];

  [ChromeEarlGrey tapWebStateElementWithID:@"canMakePayment"];

  [self waitForWebViewContainingTexts:{"true"}];
}

// Tests that canMakePayment() resolves with false with credit or prepaid cards.
- (void)testCannotMakePaymentWithCreditAndPrepaidCard {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kDebitPage)];

  [self addServerCardWithType:CREDIT];
  [self addServerCardWithType:PREPAID];

  [ChromeEarlGrey tapWebStateElementWithID:@"canMakePayment"];

  [self waitForWebViewContainingTexts:{"false"}];
}

// Tests that a debit card is preselected.
- (void)testDebitCardIsPreselected {
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(
      payments::features::kReturnGooglePayInBasicCard);

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kDebitPage)];

  [self addServerCardWithType:DEBIT];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  // Confirm that the Buy button is enabled.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_PAYMENTS_PAY_BUTTON)]
      assertWithMatcher:grey_enabled()];
}

// Tests that an "unknown" card is not preselected.
- (void)testUnknownCardTypeIsNotPreselected {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kDebitPage)];

  [self addServerCardWithType:UNKNOWN];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  // Confirm that the Buy button is not enabled.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_PAYMENTS_PAY_BUTTON)]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests that local cards are not preselected when only debit cards are
// requested. However, they can be selected and used for payment.
- (void)testCanPayWithLocalCard {
  // All local cards have "unknown" card type by design.
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(_profile->guid());
  [self addCreditCard:card];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kDebitPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  // Select the local card
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_CHOOSE_PAYMENT_METHOD)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PaymentMethodCellMatcher(card, *_profile)]
      performAction:grey_tap()];

  // Tap the Buy button.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_PAYMENTS_PAY_BUTTON)]
      performAction:grey_tap()];

  // Confirm that the Card Unmask Prompt is showing.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kCardUnmaskPromptCollectionViewAccessibilityID)]
      assertWithMatcher:grey_notNil()];

  // Type in the CVC.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"CVC_textField")]
      performAction:grey_replaceText(@"123")];

  // Tap the Confirm button.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_AUTOFILL_CARD_UNMASK_CONFIRM_BUTTON)]
      performAction:grey_tap()];

  // Verify that the CVC number is sent to the page.
  [self waitForWebViewContainingTexts:{"\"cardSecurityCode\": \"123\""}];
}

@end
