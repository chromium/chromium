// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/browser/payments/payment_request_cache.h"
#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using chrome_test_util::GetCurrentWebState;

// URLs of the test pages.
const char kMultipleRequestsPage[] =
    "https://components/test/data/payments/"
    "payment_request_multiple_requests.html";

}  // namepsace

// Miscellaneous tests for the Payment Request.
@interface PaymentRequestMiscellaneousEGTest : PaymentRequestEGTestBase

@end

@implementation PaymentRequestMiscellaneousEGTest

#pragma mark - Tests

// Tests that the page can create multiple PaymentRequest objects.
- (void)testMultipleRequests {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kMultipleRequestsPage)];

  const payments::PaymentRequestCache::PaymentRequestSet& payment_requests =
      [self paymentRequestsForWebState:GetCurrentWebState()];
  GREYAssertEqual(5U, payment_requests.size(),
                  @"Cannot create multiple PaymentRequest objects.");
}

@end
