// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for the Payment Request API in a page with data URL scheme.
@interface PaymentRequestDataURLEGTest : PaymentRequestEGTestBase

@end

@implementation PaymentRequestDataURLEGTest

#pragma mark - Tests

// Tests that PaymentRequest's constructor throws a SecurityError.
- (void)testSecurityError {
  [ChromeEarlGrey
      loadURL:GURL("data:text/html,<html><head><meta name=\"viewport\" "
                   "content=\"width=device-width, initial-scale=1, "
                   "maximum-scale=1\"></head><body><button id=\"buy\" "
                   "onclick=\"try { (new PaymentRequest([{supportedMethods: "
                   "'basic-card'}], {total: {label: 'Total',  amount: "
                   "{currency: 'USD', value: "
                   "'1.00'}}})).show(); } "
                   "catch(e) { document.getElementById('result').innerHTML = "
                   "e; }\">Data URL Test</button><div "
                   "id='result'></div></body></html>")];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  [self waitForWebViewContainingTexts:{"SecurityError",
                                       "Failed to construct 'PaymentRequest': "
                                       "Must be in a secure context"}];
}

@end
