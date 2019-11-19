// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EGTEST_BASE_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EGTEST_BASE_H_

#include "ios/chrome/browser/payments/payment_request_cache.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#include <string>
#include <vector>

@class NSError;

namespace autofill {
class AutofillProfile;
class CreditCard;
class PersonalDataManager;
}  // namespace autofill

namespace web {
class WebState;
}  // namespace web

// Base class for various Payment Request related EarlGrey tests.
@interface PaymentRequestEGTestBase : ChromeTestCase

// Adds |profile| to the PersonalDataManager. Induces a GREYAssert if the
// profile is not added within a timeout.
- (void)addAutofillProfile:(const autofill::AutofillProfile&)profile;

// Adds |card| to the PersonalDataManager. Induces a GREYAssert if the
// credit care is not added within a timeout.
- (void)addCreditCard:(const autofill::CreditCard&)card;

// Adds |card| as a server card to the PersonalDataManager.
- (void)addServerCreditCard:(const autofill::CreditCard&)card;

// Returns the payments::PaymentRequest instances for |webState|.
- (payments::PaymentRequestCache::PaymentRequestSet&)paymentRequestsForWebState:
    (web::WebState*)webState;

// Waits for the current web view to contain |texts|. If the condition is not
// met within a timeout, a GREYAssert is induced.
- (void)waitForWebViewContainingTexts:(const std::vector<std::string>&)texts;

// Returns the instance of PersonalDataManager for current ChromeBrowserState.
- (autofill::PersonalDataManager*)personalDataManager;

// Loads the specified |page|, which should be the name of a file in the
// //components/test/data/payments directory.
- (void)loadTestPage:(const std::string&)page;

// Taps the 'PAY' button in the UI, enters the specified |cvc| and confirms
// payment.
- (void)payWithCreditCardUsingCVC:(NSString*)cvc;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EGTEST_BASE_H_
