// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENTS_VALIDATORS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENTS_VALIDATORS_H_

#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AddressErrors;
class PayerErrors;
class PaymentValidationErrors;

class MODULES_EXPORT PaymentsValidators final {
  STATIC_ONLY(PaymentsValidators);

 public:
  // The most common identifiers are three-letter alphabetic codes as defined by
  // [ISO4217] (for example, "USD" for US Dollars).
  static bool IsValidCurrencyCodeFormat(const String& code,
                                        String* optional_error_message);

  // Returns true if |amount| is a valid currency code as defined in ISO 20022
  // CurrencyAnd30Amount.
  static bool IsValidAmountFormat(const String& amount,
                                  const String& item_name,
                                  String* optional_error_message);

  // Returns true if |code| is a valid ISO 3166 country code.
  static bool IsValidCountryCodeFormat(const String& code,
                                       String* optional_error_message);

  // Returns true if |code| is a valid ISO 639 language code.
  static bool IsValidLanguageCodeFormat(const String& code,
                                        String* optional_error_message);

  // Returns true if |code| is a valid ISO 15924 script code.
  static bool IsValidScriptCodeFormat(const String& code,
                                      String* optional_error_message);

  // Returns true if the payment address is valid:
  //  - Has a valid region code
  //  - Has a valid language code, if any.
  //  - Has a valid script code, if any.
  // A script code should be present only if language code is present.
  static bool IsValidShippingAddress(
      const payments::mojom::blink::PaymentAddressPtr&,
      String* optional_error_message);

  // Returns false if |error| is too long (greater than 2048).
  static bool IsValidErrorMsgFormat(const String& code,
                                    String* optional_error_message);

  // Returns false if |errors| has too long string (greater than 2048).
  static bool IsValidAddressErrorsFormat(const AddressErrors& errors,
                                         String* optional_error_message);

  // Returns false if |errors| has too long string (greater than 2048).
  static bool IsValidPayerErrorsFormat(const PayerErrors& errors,
                                       String* optional_error_message);

  // Returns false if |errors| has too long string (greater than 2048).
  static bool IsValidPaymentValidationErrorsFormat(
      const PaymentValidationErrors& errors,
      String* optional_error_message);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENTS_VALIDATORS_H_
