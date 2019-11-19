// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/ios/ios_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/core/payment_prefs.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// URLs of the test pages.
const char kCanMakePaymentPage[] =
    "https://components/test/data/payments/"
    "payment_request_can_make_payment_query_test.html";
const char kCanMakePaymentCreditCardPage[] =
    "https://components/test/data/payments/"
    "payment_request_can_make_payment_query_cc_test.html";
const char kCanMakePaymentMethodIdentifierPage[] =
    "https://components/test/data/payments/"
    "payment_request_payment_method_identifier_test.html";

}  // namepsace

// Various tests for canMakePayment() support in the Payment Request API.
@interface PaymentRequestCanMakePaymentEGTest : PaymentRequestEGTestBase

@end

@implementation PaymentRequestCanMakePaymentEGTest

- (void)setUp {
  [super setUp];

  // Allow canMakePayment to return a truthful value by default.
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->SetBoolean(payments::kCanMakePaymentEnabled, true);
}

#pragma mark - Tests

// Tests canMakePayment() when visa is required and user has a visa instrument.
- (void)testCanMakePaymentIsSupported {
  [self addCreditCard:autofill::test::GetCreditCard()];  // visa.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kCanMakePaymentPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  [self waitForWebViewContainingTexts:{"true"}];
}

// Tests canMakePayment() when visa is required, user has a visa instrument, and
// user is in incognito mode.
- (void)testCanMakePaymentIsSupportedInIncognitoMode {
  [self addCreditCard:autofill::test::GetCreditCard()];  // visa.
  // Open an Incognito tab.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kToolsMenuNewIncognitoTabId)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kCanMakePaymentPage)];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kCanMakePaymentPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  [self waitForWebViewContainingTexts:{"true"}];
}

// Tests canMakePayment() when visa is required, but user doesn't have one.
- (void)testCanMakePaymentIsNotSupported {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kCanMakePaymentPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  [self waitForWebViewContainingTexts:{"false"}];
}

// Tests canMakePayment() when visa is required, user doesn't have a visa
// instrument and the user is in incognito mode.
- (void)testCanMakePaymentIsNotSupportedInIncognitoMode {
  // Open an Incognito tab.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kToolsMenuNewIncognitoTabId)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kCanMakePaymentPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  [self waitForWebViewContainingTexts:{"false"}];
}

// Tests canMakePayment() when visa is required, user has a visa instrument, but
// user has not allowed canMakePayment to return a truthful value.
- (void)testCanMakePaymentIsSupportedNoUserConsent {
  // Disallow canMakePayment to return a truthful value.
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->SetBoolean(payments::kCanMakePaymentEnabled, false);

  [self addCreditCard:autofill::test::GetCreditCard()];  // visa.

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kCanMakePaymentPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  [self waitForWebViewContainingTexts:{"false"}];
}

// Tests canMakePayment() when visa is required, user has a visa instrument,
// user is in incognito mode, but user has not allowed canMakePayment to return
// a truthful value.
- (void)testCanMakePaymentIsSupportedInIncognitoModeNoUserConsent {
  // Disallow canMakePayment to return a truthful value.
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->SetBoolean(payments::kCanMakePaymentEnabled, false);

  [self addCreditCard:autofill::test::GetCreditCard()];  // visa.

  // Open an Incognito tab.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kToolsMenuNewIncognitoTabId)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kCanMakePaymentPage)];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kCanMakePaymentPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  [self waitForWebViewContainingTexts:{"false"}];
}

@end

// Tests for canMakePayment() exceeding query quota, when querying explicit
// payment methods.
@interface PaymentRequestCanMakePaymentExceedsQueryQuotaEGTest
    : PaymentRequestEGTestBase

@end

@implementation PaymentRequestCanMakePaymentExceedsQueryQuotaEGTest

- (void)setUp {
  [super setUp];

  // Allow canMakePayment to return a truthful value by default.
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->SetBoolean(payments::kCanMakePaymentEnabled, true);
}

#pragma mark - Tests

// Tests canMakePayment() exceeds query quota when different payment methods are
// queried one after another.
- (void)testCanMakePaymentExceedsQueryQuota {
  if (!base::ios::IsRunningOnOrLater(10, 3, 0)) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled on iOS versions below 10.3 because DOMException is not "
        @"available.");
  }

  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCanMakePaymentCreditCardPage)];

  // Query visa payment method.
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // User does not have a visa card.
  [self waitForWebViewContainingTexts:{"false"}];

  // Query Mastercard payment method.
  [ChromeEarlGrey tapWebStateElementWithID:@"other-buy"];

  // Query quota exceeded.
  [self
      waitForWebViewContainingTexts:
          {"NotAllowedError", "Not allowed to check whether can make payment"}];

  [self addCreditCard:autofill::test::GetCreditCard()];  // visa.

  // Query visa payment method.
  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // User has a visa card. While the query is cached, result is always fresh.
  [self waitForWebViewContainingTexts:{"true"}];

  // Query Mastercard payment method.
  [ChromeEarlGrey tapWebStateElementWithID:@"other-buy"];

  // Query quota exceeded.
  [self
      waitForWebViewContainingTexts:
          {"NotAllowedError", "Not allowed to check whether can make payment"}];
}

@end

// Tests for canMakePayment() exceeding query quota, when querying basic-card
// payment method.
@interface PaymentRequestCanMakePaymentExceedsQueryQuotaBasicaCardEGTest
    : PaymentRequestEGTestBase

@end

@implementation PaymentRequestCanMakePaymentExceedsQueryQuotaBasicaCardEGTest

- (void)setUp {
  [super setUp];

  // Allow canMakePayment to return a truthful value by default.
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->SetBoolean(payments::kCanMakePaymentEnabled, true);
}

#pragma mark - Tests

// Tests canMakePayment() exceeds query quota when different payment methods are
// queried one after another.
- (void)testCanMakePaymentExceedsQueryQuotaBasicaCard {
  if (!base::ios::IsRunningOnOrLater(10, 3, 0)) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled on iOS versions below 10.3 because DOMException is not "
        @"available.");
  }

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(
                              kCanMakePaymentMethodIdentifierPage)];

  // Query basic-card payment method with "supportedNetworks": ["visa"] in the
  // payment method specific data.
  [ChromeEarlGrey tapWebStateElementWithID:@"checkBasicVisa"];

  // User does not have a visa card.
  [self waitForWebViewContainingTexts:{"false"}];

  // Query basic-card payment method without "supportedNetworks" parameter.
  [ChromeEarlGrey tapWebStateElementWithID:@"checkBasicCard"];

  // Query quota exceeded.
  [self
      waitForWebViewContainingTexts:
          {"NotAllowedError", "Not allowed to check whether can make payment"}];

  [self addCreditCard:autofill::test::GetCreditCard()];  // visa.

  // Query basic-card payment method with "supportedNetworks": ["visa"] in the
  // payment method specific data.
  [ChromeEarlGrey tapWebStateElementWithID:@"checkBasicVisa"];

  // User has a visa card. While the query is cached, result is always fresh.
  [self waitForWebViewContainingTexts:{"true"}];

  // Query basic-card payment method without "supportedNetworks" parameter.
  [ChromeEarlGrey tapWebStateElementWithID:@"checkBasicCard"];

  // Query quota exceeded.
  [self
      waitForWebViewContainingTexts:
          {"NotAllowedError", "Not allowed to check whether can make payment"}];
}

@end
