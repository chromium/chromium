// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include <vector>

#import "ios/chrome/browser/payments/payment_request_cache.h"
#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using chrome_test_util::GetCurrentWebState;

// URLs of the test pages.
const char kPaymentMethodIdentifierPage[] =
    "https://components/test/data/payments/"
    "payment_request_payment_method_identifier_test.html";

}  // namepsace

// Various tests to ensure that the payment method identifiers are correctly
// parsed.
@interface PaymentRequestPaymentMethodIdentifierEGTest
    : PaymentRequestEGTestBase

@end

@implementation PaymentRequestPaymentMethodIdentifierEGTest

#pragma mark - Tests

// One network is specified in 'basic-card' data, one in supportedMethods.
- (void)testBasicCardNetworksSpecified {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kPaymentMethodIdentifierPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  const payments::PaymentRequestCache::PaymentRequestSet& requests =
      [self paymentRequestsForWebState:GetCurrentWebState()];
  GREYAssertEqual(1U, requests.size(), @"Expected one request.");
  std::vector<std::string> supportedCardNetworks =
      (*requests.begin())->supported_card_networks();
  GREYAssertEqual(2U, supportedCardNetworks.size(),
                  @"Expected two supported card networks.");
  // The networks appear in the order in which they were specified by the
  // merchant.
  GREYAssertEqual("mastercard", supportedCardNetworks[0], @"");
  GREYAssertEqual("visa", supportedCardNetworks[1], @"");
}

// Only specifying 'basic-card' with no supportedNetworks means all networks are
// supported.
- (void)testBasicCardNoNetworksSpecified {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kPaymentMethodIdentifierPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buyBasicCard"];

  const payments::PaymentRequestCache::PaymentRequestSet& requests =
      [self paymentRequestsForWebState:GetCurrentWebState()];
  GREYAssertEqual(1U, requests.size(), @"Expected one request.");
  std::vector<std::string> supportedCardNetworks =
      (*requests.begin())->supported_card_networks();
  GREYAssertEqual(8U, supportedCardNetworks.size(),
                  @"Expected eight supported card networks.");
  // The default ordering is alphabetical.
  GREYAssertEqual("amex", supportedCardNetworks[0], @"");
  GREYAssertEqual("diners", supportedCardNetworks[1], @"");
  GREYAssertEqual("discover", supportedCardNetworks[2], @"");
  GREYAssertEqual("jcb", supportedCardNetworks[3], @"");
  GREYAssertEqual("mastercard", supportedCardNetworks[4], @"");
  GREYAssertEqual("mir", supportedCardNetworks[5], @"");
  GREYAssertEqual("unionpay", supportedCardNetworks[6], @"");
  GREYAssertEqual("visa", supportedCardNetworks[7], @"");
}

// Specifying 'basic-card' with some networks after having explicitely included
// the same networks does not yield duplicates and has the expected order.
- (void)testBasicCardNetworkThenBasicCardWithSameNetwork {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kPaymentMethodIdentifierPage)];

  web::test::ExecuteJavaScript(
      GetCurrentWebState(),
      "buyHelper([{"
      "  supportedMethods: 'basic-card',"
      "  data: {"
      "    supportedNetworks: ['mastercard'],"
      "  }"
      "}, {"
      "  supportedMethods: 'basic-card',"
      "  data: {"
      "    supportedNetworks: ['visa', 'mastercard', 'jcb'],"
      "  }"
      "}]);");

  const payments::PaymentRequestCache::PaymentRequestSet& requests =
      [self paymentRequestsForWebState:GetCurrentWebState()];
  GREYAssertEqual(1U, requests.size(), @"Expected one request.");
  std::vector<std::string> supportedCardNetworks =
      (*requests.begin())->supported_card_networks();
  GREYAssertEqual(3U, supportedCardNetworks.size(),
                  @"Expected three supported card networks.");
  GREYAssertEqual("mastercard", supportedCardNetworks[0], @"");
  GREYAssertEqual("visa", supportedCardNetworks[1], @"");
  GREYAssertEqual("jcb", supportedCardNetworks[2], @"");
}

// A url-based payment method identifier is only supported if it has an https
// scheme.
- (void)testValidURLBasedPaymentMethodIdentifier {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kPaymentMethodIdentifierPage)];

  web::test::ExecuteJavaScript(GetCurrentWebState(),
                               "buyHelper([{"
                               "  supportedMethods: 'https://bobpay.xyz',"
                               "}, {"
                               "  supportedMethods: 'basic-card'"
                               "}]);");

  const payments::PaymentRequestCache::PaymentRequestSet& requests =
      [self paymentRequestsForWebState:GetCurrentWebState()];
  GREYAssertEqual(1U, requests.size(), @"Expected one request.");
  const std::vector<GURL>& urlPaymentMethodIdentifiers =
      (*requests.begin())->url_payment_method_identifiers();
  GREYAssertEqual(1U, urlPaymentMethodIdentifiers.size(),
                  @"Expected one URL-based payment method identifier.");
  GREYAssertEqual(GURL("https://bobpay.xyz"), urlPaymentMethodIdentifiers[0],
                  @"");
}

// An invalid URL-based payment method identifier results in a RangeError.
- (void)testURLBasedPaymentMethodIdentifierWithInvalidScheme {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kPaymentMethodIdentifierPage)];

  web::test::ExecuteJavaScript(GetCurrentWebState(),
                               "buyHelper([{"
                               "  supportedMethods: 'https://bobpay.xyz'"
                               "}, {"
                               "  supportedMethods: 'http://bobpay.xyz'"
                               "}, {"
                               "  supportedMethods: 'basic-card'"
                               "}]);");

  [self waitForWebViewContainingTexts:{"RangeError",
                                       "A payment method identifier must "
                                       "either be valid URL with a https "
                                       "scheme and empty username and password "
                                       "or a lower-case alphanumeric string "
                                       "with optional hyphens"}];
}

// An invalid standard payment method identifier results in a RangeError.
- (void)testStandardPaymentMethodIdentifierWithInvalidCharacters {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kPaymentMethodIdentifierPage)];

  web::test::ExecuteJavaScript(
      GetCurrentWebState(),
      "buyHelper([{"
      "  supportedMethods: 'BASIC-CARD',"
      "  data: {"
      "    supportedNetworks: ['visa', 'mastercard', 'jcb'],"
      "  }"
      "}]);");

  [self waitForWebViewContainingTexts:{"RangeError",
                                       "A payment method identifier must "
                                       "either be valid URL with a https "
                                       "scheme and empty username and password "
                                       "or a lower-case alphanumeric string "
                                       "with optional hyphens"}];
}

@end
