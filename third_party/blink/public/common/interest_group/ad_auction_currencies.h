// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AD_AUCTION_CURRENCIES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AD_AUCTION_CURRENCIES_H_

#include <string>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// True if the currency code is in format FLEDGE expects.
// Does not accept kUnspecifiedAdCurrency.
BLINK_COMMON_EXPORT bool IsValidAdCurrencyCode(const std::string& input);

// True if the input is a currency code in format that FLEDGE expects or
// kUnspecifiedAdCurrency.
BLINK_COMMON_EXPORT bool IsValidOrUnspecifiedAdCurrencyCode(
    const std::string& input);

// Returns true if `actual` currency should be accepted under expectation
// `expected; this can happen if they match or if any of them is unspecified.
BLINK_COMMON_EXPORT bool VerifyAdCurrencyCode(const std::string& expected,
                                              const std::string& actual);

// Magical value to denote that a party in the auction didn't specify
// currency value or expectation.  It can't be provided directly, but it's meant
// to be human-readable.
BLINK_COMMON_EXPORT extern const char* const kUnspecifiedAdCurrency;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AD_AUCTION_CURRENCIES_H_
