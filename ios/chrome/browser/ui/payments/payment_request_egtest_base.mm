// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"

#import <EarlGrey/EarlGrey.h>

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/payments/ios_payment_request_cache_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/test/http_server/http_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Timeout in seconds while waiting for a profile or a credit to be added to the
// PersonalDataManager.
const NSTimeInterval kPDMMaxDelaySeconds = 10.0;
}

using base::test::ios::WaitUntilConditionOrTimeout;

@interface PaymentRequestEGTestBase () {
  // The PersonalDataManager instance for the current browser state.
  autofill::PersonalDataManager* _personalDataManager;

  // Profiles added to the PersonalDataManager.
  std::vector<autofill::AutofillProfile> _profiles;

  // Credit Cards added to the PersonalDataManager.
  std::vector<autofill::CreditCard> _cards;
}

@end

@implementation PaymentRequestEGTestBase

#pragma mark - XCTestCase

- (void)setUp {
  [super setUp];
  _personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  // Before starting, clear existing profiles.
  for (const auto* profile : _personalDataManager->GetProfiles()) {
    [self personalDataManager]->RemoveByGUID(profile->guid());
  }

  _personalDataManager->SetSyncingForTest(true);
}

- (void)tearDown {
  for (const auto& profile : _profiles) {
    [self personalDataManager]->RemoveByGUID(profile.guid());
  }
  _profiles.clear();

  for (const auto& card : _cards) {
    [self personalDataManager]->RemoveByGUID(card.guid());
  }
  _cards.clear();

  [super tearDown];
}

#pragma mark - Public methods

- (void)addAutofillProfile:(const autofill::AutofillProfile&)profile {
  _profiles.push_back(profile);
  size_t profile_count = [self personalDataManager]->GetProfiles().size();
  [self personalDataManager]->AddProfile(profile);
  bool profileAdded = WaitUntilConditionOrTimeout(kPDMMaxDelaySeconds, ^{
    return profile_count < [self personalDataManager] -> GetProfiles().size();
  });
  GREYAssert(profileAdded, @"Failed to add profile.");
}

- (void)addCreditCard:(const autofill::CreditCard&)card {
  _cards.push_back(card);
  size_t card_count = [self personalDataManager]->GetCreditCards().size();
  [self personalDataManager]->AddCreditCard(card);
  bool creditCardAdded = WaitUntilConditionOrTimeout(kPDMMaxDelaySeconds, ^{
    return card_count < [self personalDataManager] -> GetCreditCards().size();
  });
  GREYAssert(creditCardAdded, @"Failed to add credit card.");
}

- (void)addServerCreditCard:(const autofill::CreditCard&)card {
  DCHECK(card.record_type() != autofill::CreditCard::LOCAL_CARD);
  [self personalDataManager]->AddServerCreditCardForTest(
      std::make_unique<autofill::CreditCard>(card));
}

- (payments::PaymentRequestCache::PaymentRequestSet&)paymentRequestsForWebState:
    (web::WebState*)webState {
  return payments::IOSPaymentRequestCacheFactory::GetForBrowserState(
             chrome_test_util::GetOriginalBrowserState())
      ->GetPaymentRequests(webState);
}

- (void)waitForWebViewContainingTexts:(const std::vector<std::string>&)texts {
  for (const std::string& text : texts)
    [ChromeEarlGrey waitForWebStateContainingText:text];
}

- (autofill::PersonalDataManager*)personalDataManager {
  return _personalDataManager;
}

- (void)loadTestPage:(const std::string&)page {
  std::string fullPath = base::StringPrintf(
      "https://components/test/data/payments/%s", page.c_str());
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(fullPath)];
  // TODO(crbug.com/973440): Tests immediately tap on page elements without
  // waiting for page to render. -drainUntilIdle call is incorrect (but
  // functional) way to wait for page content.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)payWithCreditCardUsingCVC:(NSString*)cvc {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_PAYMENTS_PAY_BUTTON)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"CVC_textField")]
      performAction:grey_replaceText(cvc)];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_AUTOFILL_CARD_UNMASK_CONFIRM_BUTTON)]
      performAction:grey_tap()];
}

@end
