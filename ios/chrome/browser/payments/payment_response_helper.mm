// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_response_helper.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_normalization_manager.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payment_shipping_option.h"
#include "ios/chrome/browser/payments/payment_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace payments {

PaymentResponseHelper::PaymentResponseHelper(
    id<PaymentResponseHelperConsumer> consumer,
    PaymentRequest* payment_request)
    : consumer_(consumer), payment_request_(payment_request) {}

PaymentResponseHelper::~PaymentResponseHelper() {}

void PaymentResponseHelper::OnInstrumentDetailsReady(
    const std::string& method_name,
    const std::string& stringified_details,
    const PayerData& payer_data) {
  method_name_ = method_name;
  stringified_details_ = stringified_details;

  [consumer_ paymentResponseHelperDidReceivePaymentMethodDetails];

  payment_request_->journey_logger().SetEventOccurred(
      JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);

  if (payment_request_->request_shipping()) {
    DCHECK(payment_request_->selected_shipping_profile());
    shipping_address_ = *payment_request_->selected_shipping_profile();
    payment_request_->GetAddressNormalizationManager()
        ->NormalizeAddressUntilFinalized(&shipping_address_);
  }

  if (payment_request_->request_payer_name() ||
      payment_request_->request_payer_email() ||
      payment_request_->request_payer_phone()) {
    DCHECK(payment_request_->selected_contact_profile());
    contact_info_ = *payment_request_->selected_contact_profile();
    payment_request_->GetAddressNormalizationManager()
        ->NormalizeAddressUntilFinalized(&contact_info_);
  }

  payment_request_->GetAddressNormalizationManager()
      ->FinalizeWithCompletionCallback(base::Bind(
          &PaymentResponseHelper::AddressNormalizationCompleted, AsWeakPtr()));
}

void PaymentResponseHelper::OnInstrumentDetailsError(
    const std::string& error_message) {
  [consumer_ paymentResponseHelperDidFailToReceivePaymentMethodDetails];
}

void PaymentResponseHelper::AddressNormalizationCompleted() {
  payments::PaymentResponse response;

  response.payment_request_id =
      payment_request_->web_payment_request().payment_request_id;
  response.method_name = method_name_;
  response.details = stringified_details_;

  if (payment_request_->request_shipping()) {
    response.shipping_address = data_util::GetPaymentAddressFromAutofillProfile(
        shipping_address_, payment_request_->GetApplicationLocale());

    PaymentShippingOption* shippingOption =
        payment_request_->selected_shipping_option();
    DCHECK(shippingOption);
    response.shipping_option = shippingOption->id;
  }

  if (payment_request_->request_payer_name()) {
    response.payer_name =
        contact_info_.GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                              payment_request_->GetApplicationLocale());
  }
  if (payment_request_->request_payer_email()) {
    response.payer_email = contact_info_.GetRawInfo(autofill::EMAIL_ADDRESS);
  }
  if (payment_request_->request_payer_phone()) {
    // Try to format the phone number to the E.164 format to send in the Payment
    // Response, as defined in the Payment Request spec. If it's not possible,
    // send the original. More info at:
    // https://w3c.github.io/browser-payment-api/#paymentrequest-updated-algorithm
    const std::string original_number = base::UTF16ToUTF8(contact_info_.GetInfo(
        autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
        payment_request_->GetApplicationLocale()));

    const std::string default_region_code =
        autofill::AutofillCountry::CountryCodeForLocale(
            payment_request_->GetApplicationLocale());
    response.payer_phone =
        base::UTF8ToUTF16(autofill::i18n::FormatPhoneForResponse(
            original_number, default_region_code));
  }

  [consumer_ paymentResponseHelperDidCompleteWithPaymentResponse:response];
}

}  // namespace payments
