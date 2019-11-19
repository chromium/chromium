// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request_test_util.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/payments/core/payment_item.h"
#include "components/payments/core/payment_method_data.h"
#include "components/payments/core/payment_shipping_option.h"
#include "components/payments/core/web_payment_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace payment_request_test_util {

payments::WebPaymentRequest CreateTestWebPaymentRequest() {
  payments::WebPaymentRequest web_payment_request;
  payments::PaymentMethodData method_data;
  method_data.supported_method = "basic-card";
  method_data.supported_networks.push_back("visa");
  method_data.supported_networks.push_back("amex");
  web_payment_request.method_data.push_back(method_data);
  web_payment_request.details.total = std::make_unique<payments::PaymentItem>();
  web_payment_request.details.total->label = "Total";
  web_payment_request.details.total->amount->value = "1.00";
  web_payment_request.details.total->amount->currency = "USD";
  payments::PaymentItem display_item;
  display_item.label = "Subtotal";
  display_item.amount->value = "1.00";
  display_item.amount->currency = "USD";
  web_payment_request.details.display_items.push_back(display_item);
  payments::PaymentShippingOption shipping_option;
  shipping_option.id = "123456";
  shipping_option.label = "1-Day";
  shipping_option.amount->value = "0.99";
  shipping_option.amount->currency = "USD";
  shipping_option.selected = true;
  web_payment_request.details.shipping_options.push_back(shipping_option);
  payments::PaymentShippingOption shipping_option2;
  shipping_option2.id = "654321";
  shipping_option2.label = "10-Days";
  shipping_option2.amount->value = "0.01";
  shipping_option2.amount->currency = "USD";
  shipping_option2.selected = false;
  web_payment_request.details.shipping_options.push_back(shipping_option2);
  web_payment_request.options.request_shipping = true;
  web_payment_request.options.request_payer_name = true;
  web_payment_request.options.request_payer_email = true;
  web_payment_request.options.request_payer_phone = true;
  return web_payment_request;
}

}  // namespace payment_request_test_util
