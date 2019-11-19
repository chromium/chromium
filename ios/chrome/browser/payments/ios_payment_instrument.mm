// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/ios_payment_instrument.h"

#include <limits>

#include "base/strings/utf_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace payments {

// URL payment method identifiers for iOS payment apps.
const char kBobpayPaymentMethodIdentifier[] =
    "https://emerald-eon.appspot.com/bobpay";
const char kAlicepayPaymentMethodIdentifier[] =
    "https://emerald-eon.appspot.com/alicepay";

// Scheme names for iOS payment apps.
const char kBobpaySchemeName[] = "bobpay://";

const std::map<std::string, std::string>& GetMethodNameToSchemeName() {
  static const std::map<std::string, std::string> kMethodToScheme =
      std::map<std::string, std::string>{
          {kBobpayPaymentMethodIdentifier, kBobpaySchemeName},
          {kAlicepayPaymentMethodIdentifier, kBobpaySchemeName}};
  return kMethodToScheme;
}

IOSPaymentInstrument::IOSPaymentInstrument(
    const std::string& method_name,
    const GURL& universal_link,
    const std::string& app_name,
    UIImage* icon_image,
    id<PaymentRequestUIDelegate> payment_request_ui_delegate)
    : PaymentApp(-1 /* resource id not used */,
                 PaymentApp::Type::NATIVE_MOBILE_APP),
      method_name_(method_name),
      universal_link_(universal_link),
      app_name_(app_name),
      icon_image_(icon_image),
      payment_request_ui_delegate_(payment_request_ui_delegate) {}
IOSPaymentInstrument::~IOSPaymentInstrument() {}

void IOSPaymentInstrument::InvokePaymentApp(PaymentApp::Delegate* delegate) {
  DCHECK(delegate);
  [payment_request_ui_delegate_ paymentInstrument:this
                       launchAppWithUniversalLink:universal_link_
                               instrumentDelegate:delegate];
}

bool IOSPaymentInstrument::IsCompleteForPayment() const {
  // As long as the native app is installed on the user's device it is
  // always complete for payment.
  return true;
}

uint32_t IOSPaymentInstrument::GetCompletenessScore() const {
  // Return max value since the instrument is always complete for payment.
  return std::numeric_limits<uint32_t>::max();
}

bool IOSPaymentInstrument::CanPreselect() const {
  // Do not preselect the payment instrument when the name and/or icon is
  // missing.
  return !GetLabel().empty() && !!icon_image_ && icon_image_.size.height != 0 &&
         icon_image_.size.width != 0;
}

bool IOSPaymentInstrument::IsExactlyMatchingMerchantRequest() const {
  // TODO(crbug.com/602666): Determine if the native payment app supports
  // 'basic-card' if the merchant only accepts payment through credit cards.
  return true;
}

base::string16 IOSPaymentInstrument::GetMissingInfoLabel() const {
  // This will always be an empty string because a native app cannot
  // have incomplete information that can then be edited by the user.
  return base::string16();
}

bool IOSPaymentInstrument::IsValidForCanMakePayment() const {
  // Same as IsCompleteForPayment, as long as the native app is installed
  // and found on the user's device then it is valid for payment.
  return true;
}

void IOSPaymentInstrument::RecordUse() {
  // TODO(crbug.com/60266): Record the use of the native payment app.
}

base::string16 IOSPaymentInstrument::GetLabel() const {
  return base::ASCIIToUTF16(app_name_);
}

base::string16 IOSPaymentInstrument::GetSublabel() const {
  // Return host of |method_name_| e.g., paypal.com.
  return base::ASCIIToUTF16(GURL(method_name_).host());
}

bool IOSPaymentInstrument::IsValidForModifier(
    const std::string& method,
    bool supported_networks_specified,
    const std::set<std::string>& supported_networks,
    bool supported_types_specified,
    const std::set<autofill::CreditCard::CardType>& supported_types) const {
  return method_name_ == method;
}

void IOSPaymentInstrument::IsValidForPaymentMethodIdentifier(
    const std::string& payment_method_identifier,
    bool* is_valid) const {
  *is_valid = method_name_ == payment_method_identifier;
}

bool IOSPaymentInstrument::HandlesShippingAddress() const {
  return false;
}

bool IOSPaymentInstrument::HandlesPayerName() const {
  return false;
}

bool IOSPaymentInstrument::HandlesPayerEmail() const {
  return false;
}

bool IOSPaymentInstrument::HandlesPayerPhone() const {
  return false;
}

base::WeakPtr<PaymentApp> IOSPaymentInstrument::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
