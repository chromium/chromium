// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include <vector>

#include "base/ios/ios_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"
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
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::GetCurrentWebState;

// URLs of the test pages.
const char kAbortPage[] =
    "https://components/test/data/payments/payment_request_abort_test.html";
const char kNoShippingPage[] =
    "https://components/test/data/payments/"
    "payment_request_no_shipping_test.html";

}  // namepsace

// Tests for various scenarios in which Payment Request UI is displayed then
// closed (e.g., merchant cancellation, user cancellation, and completion).
@interface PaymentRequestOpenAndCloseEGTest : PaymentRequestEGTestBase

@end

@implementation PaymentRequestOpenAndCloseEGTest

#pragma mark - Tests

// Tests that navigating to a URL closes the Payment Request UI.
- (void)testOpenAndNavigateToURL {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kAbortPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kNoShippingPage)];

  // Confirm that the Payment Request UI is not showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_nil()];
}

// Tests that reloading the page closes the Payment Request UI.
- (void)testOpenAndReload {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kAbortPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey reload];

  // Confirm that the Payment Request UI is not showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_nil()];
}

// Tests that navigating to the previous page closes the Payment Request UI.
- (void)testOpenAndNavigateBack {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kAbortPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey goBack];

  // Confirm that the Payment Request UI is not showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_nil()];
}

// Tests that tapping the cancel button closes the Payment Request UI and
// rejects the Promise returned by request.show() with the appropriate error.
- (void)testOpenAndCancel {
  if (!base::ios::IsRunningOnOrLater(10, 3, 0)) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled on iOS versions below 10.3 because DOMException is not "
        @"available.");
  }

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kAbortPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  // Tap the cancel button.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_ACCNAME_CANCEL)]
      performAction:grey_tap()];

  // Confirm that the Payment Request UI is not showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_nil()];

  [self waitForWebViewContainingTexts:{"AbortError",
                                       "User closed the Payment Request UI."}];
}

// Tests that tapping the link to Chrome Settings closes the Payment Request UI,
// rejects the Promise returned by request.show() with the appropriate error,
// and displays the Autofill Settings UI.
- (void)testOpenAndNavigateToSettings {
  if (!base::ios::IsRunningOnOrLater(10, 3, 0)) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled on iOS versions below 10.3 because DOMException is not "
        @"available.");
  }

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kAbortPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  // Tap the settings link.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Settings")]
      performAction:grey_tap()];

  // Confirm that the Payment Request UI is not showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_nil()];

  // Confirm that the Autofill Settings UI is showing.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillProfileTableViewID)]
      assertWithMatcher:grey_notNil()];

  [self waitForWebViewContainingTexts:{"AbortError",
                                       "User closed the Payment Request UI."}];
}

// Tests that tapping the pay button closes the Payment Request UI, accepts the
// Promise returned by request.show() with the response object, and accepts the
// Promise returned by response.complete() with an appropriate response message.
- (void)testOpenAndPay {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  [self addAutofillProfile:profile];

  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(profile.guid());
  [self addCreditCard:card];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kNoShippingPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

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
      performAction:grey_replaceText(@"111")];

  // Tap the Confirm button.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_AUTOFILL_CARD_UNMASK_CONFIRM_BUTTON)]
      performAction:grey_tap()];

  // Confirm that the Payment Request UI is not showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_nil()];
}

// Tests that calling request.abort() successfully aborts the Payment Request.
- (void)testAbort {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kAbortPage)];

  [ChromeEarlGrey tapWebStateElementWithID:@"buy"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey tapWebStateElementWithID:@"abort"];

  // Confirm that the error confirmation UI is showing.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::PaymentRequestErrorView()]
      assertWithMatcher:grey_notNil()];

  // Confirm the error.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_ACCNAME_OK)]
      performAction:grey_tap()];

  // Confirm that the Payment Request UI is not showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_nil()];

  [self waitForWebViewContainingTexts:{"Aborted"}];
}

@end
