// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UNITTEST_BASE_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UNITTEST_BASE_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/macros.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

// Base class for various payment request related unit tests. This purposely
// does not inherit from PlatformTest (testing::Test) so that it can be used
// by ViewController unit tests.
class PaymentRequestUnitTestBase {
 protected:
  PaymentRequestUnitTestBase();
  ~PaymentRequestUnitTestBase();

  void DoSetUp(TestChromeBrowserState::TestingFactories factories = {});
  void DoTearDown();

  // Should be called after data is added to the database via AddAutofillProfile
  // and/or AddCreditCard.
  void CreateTestPaymentRequest();

  void AddAutofillProfile(const autofill::AutofillProfile& profile);
  void AddCreditCard(const autofill::CreditCard& card);

  payments::TestPaymentRequest* payment_request() {
    return payment_request_.get();
  }
  web::TestWebState* web_state() { return &web_state_; }
  PrefService* pref_service() { return pref_service_.get(); }
  autofill::TestPersonalDataManager* personal_data_manager() {
    return &personal_data_manager_;
  }
  TestChromeBrowserState* browser_state() {
    return chrome_browser_state_.get();
  }
  std::vector<autofill::AutofillProfile*> profiles() const {
    return personal_data_manager_.GetProfiles();
  }
  std::vector<autofill::CreditCard*> credit_cards() const {
    return personal_data_manager_.GetCreditCards();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  web::TestWebState web_state_;
  std::unique_ptr<PrefService> pref_service_;
  autofill::TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<payments::TestPaymentRequest> payment_request_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestUnitTestBase);
};

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UNITTEST_BASE_H_
