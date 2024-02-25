// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AdAuctionCurrenciesTest, IsValidAdCurrencyCode) {
  EXPECT_FALSE(IsValidAdCurrencyCode("A"));
  EXPECT_FALSE(IsValidAdCurrencyCode("ABCD"));
  EXPECT_FALSE(IsValidAdCurrencyCode(blink::kUnspecifiedAdCurrency));
  EXPECT_TRUE(IsValidAdCurrencyCode("ABC"));
  EXPECT_FALSE(IsValidAdCurrencyCode("aBC"));
  EXPECT_FALSE(IsValidAdCurrencyCode("AbC"));
  EXPECT_FALSE(IsValidAdCurrencyCode("ABc"));
}

TEST(AdAuctionCurrenciesTest, VerifyAdCurrencyCode) {
  EXPECT_FALSE(
      VerifyAdCurrencyCode(AdCurrency::From("ABC"), AdCurrency::From("CBA")));
  EXPECT_TRUE(
      VerifyAdCurrencyCode(AdCurrency::From("ABC"), AdCurrency::From("ABC")));
  EXPECT_TRUE(VerifyAdCurrencyCode(AdCurrency::From("ABC"), std::nullopt));
  EXPECT_TRUE(VerifyAdCurrencyCode(std::nullopt, AdCurrency::From("ABC")));
  EXPECT_TRUE(VerifyAdCurrencyCode(std::nullopt, std::nullopt));
}

TEST(AdAuctionCurrenciesTest, PrintableAdCurrency) {
  EXPECT_EQ(kUnspecifiedAdCurrency, PrintableAdCurrency(std::nullopt));
  EXPECT_EQ("USD", PrintableAdCurrency(AdCurrency::From("USD")));
}

}  // namespace blink
