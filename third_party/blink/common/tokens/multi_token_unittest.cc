// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/tokens/multi_token.h"

#include <algorithm>

#include "base/types/token_type.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using FooToken = base::TokenType<class FooTokenTag>;
using BarToken = base::TokenType<class BarTokenTag>;
using BazToken = base::TokenType<class BazTokenTag>;

// Test MultiTokenVariantCount.
static_assert(internal::MultiTokenVariantCount<FooToken, BarToken>::kValue == 2,
              "unexpected count");
static_assert(
    internal::MultiTokenVariantCount<FooToken, BarToken, BazToken>::kValue == 3,
    "unexpected count");

// Test MultiTokenTypeRepeated.
static_assert(!internal::MultiTokenTypeRepeated<FooToken>::kValue,
              "unexpected repeated value");
static_assert(!internal::MultiTokenTypeRepeated<FooToken, FooToken>::kValue,
              "unexpected repeated value");
static_assert(
    !internal::MultiTokenTypeRepeated<FooToken, FooToken, BarToken>::kValue,
    "unexpected repeated value");
static_assert(
    internal::MultiTokenTypeRepeated<FooToken, FooToken, BarToken, FooToken>::
        kValue,
    "unexpected repeated value");
static_assert(
    internal::MultiTokenTypeRepeated<FooToken, BarToken, FooToken, FooToken>::
        kValue,
    "unexpected repeated value");

// Test MultiTokenAnyTypeRepeated.
static_assert(!internal::MultiTokenAnyTypeRepeated<FooToken>::kValue,
              "unexpected any repeated value");
static_assert(!internal::MultiTokenAnyTypeRepeated<FooToken, BarToken>::kValue,
              "unexpected any repeated value");
static_assert(
    !internal::MultiTokenAnyTypeRepeated<FooToken, BarToken, BazToken>::kValue,
    "unexpected any repeated value");
static_assert(
    internal::MultiTokenAnyTypeRepeated<FooToken, BarToken, FooToken>::kValue,
    "unexpected any repeated value");
static_assert(
    internal::MultiTokenAnyTypeRepeated<FooToken, BarToken, BarToken>::kValue,
    "unexpected any repeated value");

// Test MultiTokenVariantIsTokenType.
static_assert(internal::MultiTokenVariantIsTokenType<FooToken>::kValue,
              "unexpected is token type value");
static_assert(!internal::MultiTokenVariantIsTokenType<int>::kValue,
              "unexpected is token type value");

// Test MultiTokenAllVariantsAreTokenType.
static_assert(
    internal::MultiTokenAllVariantsAreTokenType<FooToken, BarToken>::kValue,
    "unexpected all variants are token type value");
static_assert(!internal::MultiTokenAllVariantsAreTokenType<FooToken,
                                                           BarToken,
                                                           int>::kValue,
              "unexpected all variants are token type value");

using FooBarToken = MultiToken<FooToken, BarToken>;
using FooBarBazToken = MultiToken<FooToken, BarToken, BazToken>;

TEST(MultiTokenTest, MultiTokenWorks) {
  // Test default initialization.
  FooBarToken token1;
  EXPECT_FALSE(token1.value().is_empty());
  EXPECT_EQ(0u, token1.variant_index());
  EXPECT_TRUE(token1.Is<FooToken>());
  EXPECT_FALSE(token1.Is<BarToken>());

  // Test copy construction.
  BarToken bar = BarToken();
  FooBarToken token2(bar);
  EXPECT_EQ(token2.value(), bar.value());
  EXPECT_FALSE(token2.value().is_empty());
  EXPECT_EQ(1u, token2.variant_index());
  EXPECT_FALSE(token2.Is<FooToken>());
  EXPECT_TRUE(token2.Is<BarToken>());

  // Test assignment.
  FooBarToken token3;
  token3 = token2;
  EXPECT_EQ(token3.value(), token2.value());
  EXPECT_FALSE(token3.value().is_empty());
  EXPECT_EQ(token2.variant_index(), token3.variant_index());
  EXPECT_FALSE(token3.Is<FooToken>());
  EXPECT_TRUE(token3.Is<BarToken>());

  // Test comparison operators.
  EXPECT_FALSE(token1 == token2);
  EXPECT_TRUE(token1 != token2);
  EXPECT_TRUE(token2 == token3);
  EXPECT_FALSE(token2 != token3);
  EXPECT_EQ(token1 < token2, token1.value() < token2.value());
  EXPECT_FALSE(token2 < token3);
  EXPECT_FALSE(token3 < token2);

  // Test hasher.
  EXPECT_EQ(FooBarToken::Hasher()(token2),
            base::UnguessableTokenHash()(token2.value()));

  // Test string representation.
  EXPECT_EQ(token2.ToString(), token2.value().ToString());

  // Test type conversions.
  FooToken foo(token1.value());
  EXPECT_EQ(foo, token1.GetAs<FooToken>());
  EXPECT_EQ(token2.GetAs<BarToken>(), token3.GetAs<BarToken>());
}

}  // namespace blink
