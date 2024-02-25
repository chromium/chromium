// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"

#include "base/strings/string_util.h"

namespace blink {

const char* const kUnspecifiedAdCurrency = "???";

bool IsValidAdCurrencyCode(const std::string& code) {
  if (code.length() != 3u) {
    return false;
  }
  return base::IsAsciiUpper(code[0]) && base::IsAsciiUpper(code[1]) &&
         base::IsAsciiUpper(code[2]);
}

bool VerifyAdCurrencyCode(const std::optional<AdCurrency>& expected,
                          const std::optional<AdCurrency>& actual) {
  // TODO(morlovich): Eventually we want to drop the compatibility
  // exceptions.
  return !expected.has_value() || !actual.has_value() ||
         expected->currency_code() == actual->currency_code();
}

std::string PrintableAdCurrency(const std::optional<AdCurrency>& currency) {
  return currency.has_value() ? currency->currency_code()
                              : kUnspecifiedAdCurrency;
}

bool operator==(const AdCurrency&, const AdCurrency&) = default;

}  // namespace blink
