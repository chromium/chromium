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

bool IsValidOrUnspecifiedAdCurrencyCode(const std::string& input) {
  return input == kUnspecifiedAdCurrency || IsValidAdCurrencyCode(input);
}

bool VerifyAdCurrencyCode(const std::string& expected,
                          const std::string& actual) {
  // TODO(morlovich): Eventually we want to drop the compatibility
  // exceptions.
  return expected == actual || expected == kUnspecifiedAdCurrency ||
         actual == kUnspecifiedAdCurrency;
}

}  // namespace blink
