// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request_unittest_base.h"

#include "components/payments/core/payment_prefs.h"
#include "components/payments/core/payments_test_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PaymentRequestUnitTestBase::PaymentRequestUnitTestBase()
    : pref_service_(payments::test::PrefServiceForTesting()) {}

PaymentRequestUnitTestBase::~PaymentRequestUnitTestBase() {}

void PaymentRequestUnitTestBase::DoSetUp(
    TestChromeBrowserState::TestingFactories factories) {
  TestChromeBrowserState::Builder builder;
  for (auto& pair : factories) {
    builder.AddTestingFactory(pair.first, pair.second);
  }
  chrome_browser_state_ = builder.Build();
  web_state_.SetBrowserState(chrome_browser_state_.get());
  personal_data_manager_.SetPrefService(pref_service_.get());
}

void PaymentRequestUnitTestBase::DoTearDown() {
  personal_data_manager_.SetPrefService(nullptr);
}

void PaymentRequestUnitTestBase::CreateTestPaymentRequest() {
  payment_request_ = std::make_unique<payments::TestPaymentRequest>(
      payment_request_test_util::CreateTestWebPaymentRequest(),
      chrome_browser_state_.get(), &web_state_, &personal_data_manager_);
  payment_request_->SetPrefService(pref_service_.get());
}

void PaymentRequestUnitTestBase::AddAutofillProfile(
    const autofill::AutofillProfile& profile) {
  personal_data_manager_.AddProfile(profile);
}

void PaymentRequestUnitTestBase::AddCreditCard(
    const autofill::CreditCard& card) {
  personal_data_manager_.AddCreditCard(card);
}
