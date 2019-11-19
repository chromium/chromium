// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/payments/core/features.h"
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

constexpr auto CREDIT = ::autofill::CreditCard::CardType::CARD_TYPE_CREDIT;

using chrome_test_util::GetCurrentWebState;

// URLs of the test pages.
const char kModifiersPage[] =
    "https://components/test/data/payments/"
    "payment_request_bobpay_and_basic_card_with_modifiers_test.html";

// Matcher for the PriceCell.
id<GREYMatcher> PriceCellMatcher(NSString* accessibilityLabel, BOOL is_button) {
  return grey_allOf(
      grey_accessibilityLabel(accessibilityLabel),
      is_button ? grey_accessibilityTrait(UIAccessibilityTraitButton)
                : grey_not(grey_accessibilityTrait(UIAccessibilityTraitButton)),
      grey_sufficientlyVisible(), nil);
}

// Matcher for the PaymentMethodCell.
id<GREYMatcher> PaymentMethodCellMatcher(
    const autofill::CreditCard& credit_card) {
  return chrome_test_util::ButtonWithAccessibilityLabel([NSString
      stringWithFormat:@"%@, %@",
                       base::SysUTF16ToNSString(
                           credit_card.NetworkAndLastFourDigits()),
                       base::SysUTF16ToNSString(credit_card.GetRawInfo(
                           autofill::CREDIT_CARD_NAME_FULL))]);
}

}  // namepsace

// Tests for the PaymentDetailsModifier.
@interface PaymentRequestModifiersEGTest : PaymentRequestEGTestBase

@end

@implementation PaymentRequestModifiersEGTest {
  autofill::AutofillProfile _profile;
  autofill::CreditCard _localCard;
  autofill::CreditCard _serverCard;
}

- (void)setUp {
  [super setUp];
  if (![ChromeEarlGrey isWebPaymentsModifiersEnabled]) {
    // payments::features::kWebPaymentsModifiers feature is not enabled,
    // You have to pass --enable-features=WebPaymentsModifiers command line
    // argument in order to run this test.
    DCHECK(false);
  }

  [self addProfile];
}

#pragma mark - Helper methods

- (void)addProfile {
  _profile = autofill::test::GetFullProfile();
  [self addAutofillProfile:_profile];
}

- (void)addLocalCard {
  _localCard = autofill::test::GetCreditCard();  // Visa.
  _localCard.set_billing_address_id(_profile.guid());
  [self addCreditCard:_localCard];
}

- (void)addServerCardWithType:(autofill::CreditCard::CardType)cardType {
  _serverCard = autofill::test::GetMaskedServerCard();  // Mastercard.
  _serverCard.set_card_type(cardType);
  _serverCard.set_billing_address_id(_profile.guid());
  [self addServerCreditCard:_serverCard];
}

#pragma mark - Tests

// Tests that no modifier should be applied if there is no selected instrument.
- (void)testNoModifierAppliedNoSelectedInstrument {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kModifiersPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Verify there's no line item.
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(@"Total, USD $5.00", NO)]
      assertWithMatcher:grey_notNil()];
}

// Tests that modifiers should be applied if there is a selected local credit
// card instrument and the modifiers are for basic-card.
- (void)testModifierAppliedSelectedLocalInstrumentWithoutTypeOrNetwork {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kModifiersPage)];

  [self addLocalCard];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Verify there's a selected payment method.
  [[EarlGrey selectElementWithMatcher:PaymentMethodCellMatcher(_localCard)]
      assertWithMatcher:grey_notNil()];

  // Verify there is one line item for the discount and one for the total.
  [[EarlGrey
      selectElementWithMatcher:PriceCellMatcher(@"Total, USD $4.00", YES)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(@"Total, USD $4.00", NO)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(
                                          @"basic-card discount, -$1.00", NO)]
      assertWithMatcher:grey_notNil()];
}

// Tests that modifiers should be applied if there is a selected server credit
// card instrument and the modifiers are for basic-card.
- (void)testModifierAppliedSelectedServerInstrumentWithoutTypeOrNetwork {
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(
      payments::features::kReturnGooglePayInBasicCard);

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kModifiersPage)];

  [self addServerCardWithType:CREDIT];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Verify there's a selected payment method.
  [[EarlGrey selectElementWithMatcher:PaymentMethodCellMatcher(_serverCard)]
      assertWithMatcher:grey_notNil()];

  // Verify there is one line item for the discount and one for the total.
  [[EarlGrey
      selectElementWithMatcher:PriceCellMatcher(@"Total, USD $4.00", YES)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(@"Total, USD $4.00", NO)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(
                                          @"basic-card discount, -$1.00", NO)]
      assertWithMatcher:grey_notNil()];
}

// Tests that modifiers should be applied if there is a selected credit card
// instrument and the modifiers are for basic-card of matching type.
- (void)testModifierAppliedSelectedInstrumentWithMatchingSupportedType {
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(
      payments::features::kReturnGooglePayInBasicCard);

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kModifiersPage)];

  [self addServerCardWithType:CREDIT];

  [ChromeEarlGrey tapWebStateElementWithID:@"credit_supported_type"];

  // Verify there's a selected payment method.
  [[EarlGrey selectElementWithMatcher:PaymentMethodCellMatcher(_serverCard)]
      assertWithMatcher:grey_notNil()];

  // Verify there is one line item for the discount and one for the total.
  [[EarlGrey
      selectElementWithMatcher:PriceCellMatcher(@"Total, USD $4.00", YES)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(@"Total, USD $4.00", NO)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(
                                          @"basic-card discount, -$1.00", NO)]
      assertWithMatcher:grey_notNil()];
}

// Tests that no modifier should be applied if there is a selected credit card
// instrument but the modifiers are for basic-card of mismatching type.
- (void)testNoModifierAppliedSelectedInstrumentWithMismatchingSupportedType {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kModifiersPage)];

  [self addServerCardWithType:CREDIT];

  [ChromeEarlGrey tapWebStateElementWithID:@"debit_supported_type"];

  // Verify there's no line item.
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(@"Total, USD $5.00", NO)]
      assertWithMatcher:grey_notNil()];
}

// Tests that modifiers should be applied if there is a selected credit card
// instrument and the modifiers are for basic-card of a matching network.
- (void)testModifierAppliedSelectedInstrumentWithMatchingSupportedNetwork {
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(
      payments::features::kReturnGooglePayInBasicCard);

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kModifiersPage)];

  [self addServerCardWithType:CREDIT];

  [ChromeEarlGrey tapWebStateElementWithID:@"mastercard_any_supported_type"];

  // Verify there's a selected payment method.
  [[EarlGrey selectElementWithMatcher:PaymentMethodCellMatcher(_serverCard)]
      assertWithMatcher:grey_notNil()];

  // Verify there is one line item for the discount and one for the total.
  [[EarlGrey
      selectElementWithMatcher:PriceCellMatcher(@"Total, USD $4.00", YES)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(@"Total, USD $4.00", NO)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(
                                          @"basic-card discount, -$1.00", NO)]
      assertWithMatcher:grey_notNil()];
}

// Tests that no modifier should be applied if there is a selected credit card
// instrument but the modifiers are for basic-card of mismatching network.
- (void)testNoModifierAppliedSelectedInstrumentWithMismatchingSupportedNetwork {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kModifiersPage)];

  [self addLocalCard];

  [ChromeEarlGrey tapWebStateElementWithID:@"mastercard_any_supported_type"];

  // Verify there's a selected payment method.
  [[EarlGrey selectElementWithMatcher:PaymentMethodCellMatcher(_localCard)]
      assertWithMatcher:grey_notNil()];

  // Verify there's no line item.
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(@"Total, USD $5.00", NO)]
      assertWithMatcher:grey_notNil()];
}

// Tests that modifiers should be applied if there is a selected credit card
// instrument and the modifiers are for basic-card of a matching network and
// type.
- (void)testModifierAppliedSelectedInstrumentWithMatchingNetworkAndType {
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(
      payments::features::kReturnGooglePayInBasicCard);

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kModifiersPage)];

  [self addServerCardWithType:CREDIT];

  [ChromeEarlGrey tapWebStateElementWithID:@"mastercard_supported_network"];

  // Verify there's a selected payment method.
  [[EarlGrey selectElementWithMatcher:PaymentMethodCellMatcher(_serverCard)]
      assertWithMatcher:grey_notNil()];

  // Verify there is one line item for the discount and one for the total.
  [[EarlGrey
      selectElementWithMatcher:PriceCellMatcher(@"Total, USD $4.00", YES)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(@"Total, USD $4.00", NO)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(
                                          @"basic-card discount, -$1.00", NO)]
      assertWithMatcher:grey_notNil()];
}

@end
