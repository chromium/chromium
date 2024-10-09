// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATA_RESULTS_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATA_RESULTS_H_

#include <stdint.h>
#include <utility>

#include "base/functional/callback.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/webdata/common/webdata_export.h"

class WDTypedResult;

//
// Result types for WebDataService.
//
typedef enum {
  BOOL_RESULT = 1,               // WDResult<bool>
  KEYWORDS_RESULT,               // WDResult<WDKeywordsResult>
  INT64_RESULT,                  // WDResult<int64_t>
#if BUILDFLAG(IS_WIN)            //
  PASSWORD_IE7_RESULT,           // WDResult<IE7PasswordInfo>
#endif                           //
  WEB_APP_IMAGES,                // WDResult<WDAppImagesResult>
  TOKEN_RESULT,                  // WDResult<TokenResult>
  AUTOFILL_VALUE_RESULT,         // WDResult<std::vector<AutofillEntry>>
  AUTOFILL_CLEANUP_RESULT,       // WDResult<size_t>
  AUTOFILL_CHANGES,              // WDResult<std::vector<AutofillChange>>
  AUTOFILL_PROFILES_RESULT,      // WDResult<std::vector<AutofillProfile>>
  AUTOFILL_CLOUDTOKEN_RESULT,    // WDResult<std::vector<std::unique_ptr<
                                 //     CreditCardCloudTokenData>>>
  AUTOFILL_CREDITCARDS_RESULT,   // WDResult<std::vector<
                                 //     std::unique_ptr<CreditCard>>>
  AUTOFILL_IBANS_RESULT,         // WDResult<std::vector<
                                 //     std::unique_ptr<Iban>>>
  AUTOFILL_CUSTOMERDATA_RESULT,  // WDResult<std::unique_ptr<
                                 //     PaymentsCustomerData>>
  AUTOFILL_OFFER_DATA,           // WDResult<std::vector<std::unique_ptr<
                                 //     AutofillOfferData>>>
  AUTOFILL_VIRTUAL_CARD_USAGE_DATA,  // WDResult<std::vector<
                                     //     VirtualCardUsageData>>
  CREDIT_CARD_BENEFIT_RESULT,        // WDResult<std::vector<std::unique_ptr<
                                     //     CreditCardBenefit>>>
  MASKED_BANK_ACCOUNTS_RESULT,       // WDResult<std::vector<BankAccount>>
  PAYMENT_INSTRUMENT_RESULT,         // WDResult<std::vector<
                                     //     sync_pb::PaymentInstrument>>
#if BUILDFLAG(USE_BLINK)         //
  PAYMENT_WEB_APP_MANIFEST,      // WDResult<std::vector<
                                 //     mojom::WebAppManifestSectionPtr>>
  PAYMENT_METHOD_MANIFEST,       // WDResult<std::vector<std::string>>
  SECURE_PAYMENT_CONFIRMATION,   // WDResult<std::vector<std::unique_ptr<
                                 //     SecurePaymentConfirmationInstrument>>>
#endif                           //
  PLUS_ADDRESS_RESULT,           // WDResult<std::vector<PlusProfile>>
} WDResultType;

//
// The top level class for a result.
//
class WEBDATA_EXPORT WDTypedResult {
 public:
  WDTypedResult(const WDTypedResult&) = delete;
  WDTypedResult& operator=(const WDTypedResult&) = delete;

  virtual ~WDTypedResult() = default;

  // Return the result type.
  WDResultType GetType() const { return type_; }

 protected:
  explicit WDTypedResult(WDResultType type) : type_(type) {}

 private:
  WDResultType type_;
};

// A result containing one specific pointer or literal value.
template <class T>
class WDResult : public WDTypedResult {
 public:
  WDResult(WDResultType type, const T& v) : WDTypedResult(type), value_(v) {}
  WDResult(WDResultType type, T&& v)
      : WDTypedResult(type), value_(std::move(v)) {}

  WDResult(const WDResult&) = delete;
  WDResult& operator=(const WDResult&) = delete;

  ~WDResult() override = default;

  // Return a single value result.
  const T& GetValue() const { return value_; }
  T GetValue() { return std::move(value_); }

 private:
  T value_;
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATA_RESULTS_H_
