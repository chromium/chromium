// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// URLs of the test pages.
const char kBobPayPage[] =
    "https://components/test/data/payments/"
    "payment_request_bobpay_test.html";

}  // namepsace

// Tests for a merchant that requests payment apps as the payment method.
@interface PaymentRequestPaymentAppEGTest : PaymentRequestEGTestBase

@end

@implementation PaymentRequestPaymentAppEGTest

#pragma mark - Tests

// Tests that the Promise returned by show() gets rejected with a
// NotSupportedError if the requested payment methods are payment apps that
// are not installed.
- (void)testShowPaymentAppNotInstalled {
  if (!base::ios::IsRunningOnOrLater(10, 3, 0)) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled on iOS versions below 10.3 because DOMException is not "
        @"available.");
  }

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kBobPayPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  [self waitForWebViewContainingTexts:{"NotSupportedError",
                                       "The payment method is not supported"}];
}

// Tests that the Promise returned by canMakePayment() gets resolved with false
// if the requested payment methods are payment apps that are not installed.
- (void)testCanMakePaymentPaymentAppNotInstalled {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kBobPayPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"canMakePayment"];

  [self waitForWebViewContainingTexts:{"false"}];
}

@end
