// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/test_payment_request.h"

#include "components/autofill/core/browser/geo/region_data_loader.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "components/payments/core/web_payment_request.h"
#include "components/prefs/pref_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace payments {

TestPaymentRequest::TestPaymentRequest(
    const payments::WebPaymentRequest& web_payment_request,
    ios::ChromeBrowserState* browser_state,
    web::WebState* web_state,
    autofill::PersonalDataManager* personal_data_manager,
    id<PaymentRequestUIDelegate> payment_request_ui_delegate)
    : PaymentRequest(web_payment_request,
                     browser_state,
                     web_state,
                     personal_data_manager,
                     payment_request_ui_delegate),
      address_normalization_manager_(&address_normalizer_, "en-US"),
      region_data_loader_(nullptr),
      pref_service_(nullptr),
      profile_comparator_(nullptr),
      is_incognito_(false) {}

TestPaymentRequest::TestPaymentRequest(
    const payments::WebPaymentRequest& web_payment_request,
    ios::ChromeBrowserState* browser_state,
    web::WebState* web_state,
    autofill::PersonalDataManager* personal_data_manager)
    : TestPaymentRequest(web_payment_request,
                         browser_state,
                         web_state,
                         personal_data_manager,
                         /*payment_request_ui_delegate=*/nil) {}

void TestPaymentRequest::ClearShippingProfiles() {
  shipping_profiles_.clear();
}

void TestPaymentRequest::ClearContactProfiles() {
  contact_profiles_.clear();
}

void TestPaymentRequest::ClearPaymentMethods() {
  payment_methods_.clear();
}

void TestPaymentRequest::ResetParsedPaymentMethodData() {
  url_payment_method_identifiers_.clear();
  supported_card_networks_.clear();
  basic_card_specified_networks_.clear();
  supported_card_types_set_.clear();
  PaymentRequest::ParsePaymentMethodData();
}

autofill::AddressNormalizer* TestPaymentRequest::GetAddressNormalizer() {
  return &address_normalizer_;
}

autofill::AddressNormalizationManager*
TestPaymentRequest::GetAddressNormalizationManager() {
  return &address_normalization_manager_;
}

autofill::RegionDataLoader* TestPaymentRequest::GetRegionDataLoader() {
  if (region_data_loader_)
    return region_data_loader_;
  return PaymentRequest::GetRegionDataLoader();
}

PrefService* TestPaymentRequest::GetPrefService() {
  if (pref_service_)
    return pref_service_;
  return PaymentRequest::GetPrefService();
}

PaymentsProfileComparator* TestPaymentRequest::profile_comparator() {
  if (profile_comparator_)
    return profile_comparator_;
  return PaymentRequest::profile_comparator();
}

bool TestPaymentRequest::IsIncognito() const {
  return is_incognito_;
}

}  // namespace payments
