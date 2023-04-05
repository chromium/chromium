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

TEST(AdAuctionCurrenciesTest, IsValidOrUnspecifiedAdCurrencyCode) {
  EXPECT_FALSE(IsValidOrUnspecifiedAdCurrencyCode("A"));
  EXPECT_FALSE(IsValidOrUnspecifiedAdCurrencyCode("ABCD"));
  EXPECT_TRUE(
      IsValidOrUnspecifiedAdCurrencyCode(blink::kUnspecifiedAdCurrency));
  EXPECT_TRUE(IsValidOrUnspecifiedAdCurrencyCode("ABC"));
  EXPECT_FALSE(IsValidOrUnspecifiedAdCurrencyCode("aBC"));
  EXPECT_FALSE(IsValidOrUnspecifiedAdCurrencyCode("AbC"));
  EXPECT_FALSE(IsValidOrUnspecifiedAdCurrencyCode("ABc"));
}

TEST(AdAuctionCurrenciesTest, VerifyAdCurrencyCode) {
  EXPECT_FALSE(VerifyAdCurrencyCode("ABC", "CBA"));
  EXPECT_TRUE(VerifyAdCurrencyCode("ABC", "ABC"));
  EXPECT_TRUE(VerifyAdCurrencyCode("ABC", blink::kUnspecifiedAdCurrency));
  EXPECT_TRUE(VerifyAdCurrencyCode(blink::kUnspecifiedAdCurrency, "ABC"));
  EXPECT_TRUE(VerifyAdCurrencyCode(blink::kUnspecifiedAdCurrency,
                                   blink::kUnspecifiedAdCurrency));
}

}  // namespace blink
