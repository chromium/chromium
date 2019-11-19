// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_RESPONSE_HELPER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_RESPONSE_HELPER_H_

#include <string>

#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/payments/core/payment_app.h"
#include "components/payments/core/payment_response.h"
#include "components/payments/core/web_payment_request.h"

@protocol PaymentResponseHelperConsumer

// Called when the payment method details have been successfully received.
- (void)paymentResponseHelperDidReceivePaymentMethodDetails;

// Called if an error occured when attempting to receive payment method details.
- (void)paymentResponseHelperDidFailToReceivePaymentMethodDetails;

// Called when the payment method details have been successfully received and
// the shipping address and the contact info are normalized, if applicable.
- (void)paymentResponseHelperDidCompleteWithPaymentResponse:
    (const payments::PaymentResponse&)paymentResponse;

@end

namespace payments {

class PaymentRequest;

// A helper class to facilitate creation of the PaymentResponse.
class PaymentResponseHelper
    : public PaymentApp::Delegate,
      public base::SupportsWeakPtr<PaymentResponseHelper> {
 public:
  PaymentResponseHelper(id<PaymentResponseHelperConsumer> consumer,
                        PaymentRequest* payment_request);
  ~PaymentResponseHelper() override;

  // PaymentApp::Delegate
  void OnInstrumentDetailsReady(const std::string& method_name,
                                const std::string& stringified_details,
                                const PayerData& payer_data) override;
  void OnInstrumentDetailsError(const std::string& error_message) override;

 private:
  // Called when the AddressNormalizationManager is done, whether any autofill
  // profile is actually normalized.
  void AddressNormalizationCompleted();

  __weak id<PaymentResponseHelperConsumer> consumer_;

  // Owns this instance and is guaranteed to outlive it.
  PaymentRequest* payment_request_;

  // Stored data to use in the payment response once normalization is complete.
  std::string method_name_;
  std::string stringified_details_;
  autofill::AutofillProfile shipping_address_;
  autofill::AutofillProfile contact_info_;

  DISALLOW_COPY_AND_ASSIGN(PaymentResponseHelper);
};

}  // namespace payments

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_RESPONSE_HELPER_H_
