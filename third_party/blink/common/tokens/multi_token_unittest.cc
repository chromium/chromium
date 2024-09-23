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

static_assert(internal::IsBaseToken<FooToken>);
static_assert(!internal::IsBaseToken<int>);

static_assert(internal::AreAllUnique<int>);
static_assert(!internal::AreAllUnique<int, int>);
static_assert(!internal::AreAllUnique<int, char, int>);

static_assert(internal::IsCompatible<BazToken, FooToken, BarToken, BazToken>);
static_assert(!internal::IsCompatible<BazToken, FooToken, BarToken>);

using FooBarToken = MultiToken<FooToken, BarToken>;
using FooBarBazToken = MultiToken<FooToken, BarToken, BazToken>;

static_assert(FooBarBazToken::IndexOf<FooToken>() == FooBarBazToken::Tag{0});
static_assert(FooBarBazToken::IndexOf<BarToken>() == FooBarBazToken::Tag{1});
static_assert(FooBarBazToken::IndexOf<BazToken>() == FooBarBazToken::Tag{2});

TEST(MultiTokenTest, MultiTokenWorks) {
  // Test default initialization.
  FooBarToken token1;
  EXPECT_FALSE(token1.value().is_empty());
  EXPECT_EQ(FooBarToken::Tag{0}, token1.variant_index());
  EXPECT_TRUE(token1.Is<FooToken>());
  EXPECT_FALSE(token1.Is<BarToken>());

  // Test copy construction.
  BarToken bar = BarToken();
  FooBarToken token2(bar);
  EXPECT_EQ(token2.value(), bar.value());
  EXPECT_FALSE(token2.value().is_empty());
  EXPECT_EQ(FooBarToken::Tag{1}, token2.variant_index());
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

TEST(MultiTokenTest, Comparison) {
  // Tests comparisons between:
  // - two multi tokens that hold different types and underlying values
  // - two multi tokens that hold the same type and underlying value
  {
    FooBarToken token1 = FooToken();
    FooBarToken token2 = BarToken();
    FooBarToken token3 = token2;

    EXPECT_FALSE(token1 == token2);
    EXPECT_TRUE(token1 != token2);
    EXPECT_TRUE(token2 == token3);
    EXPECT_FALSE(token2 != token3);

    // absl::variant and std::variant order by index. If the indexes are equal
    // (e.g. the same type is held in both), then the comparison operator of the
    // held type is used.
    EXPECT_TRUE(token1 < token2);
    EXPECT_TRUE(token1 < token3);
    EXPECT_FALSE(token2 < token3);

    EXPECT_TRUE(token1 <= token2);
    EXPECT_TRUE(token1 <= token3);
    EXPECT_TRUE(token2 <= token3);
    EXPECT_TRUE(token3 <= token2);

    EXPECT_FALSE(token1 > token2);
    EXPECT_FALSE(token1 > token3);
    EXPECT_FALSE(token2 > token3);

    EXPECT_FALSE(token1 >= token2);
    EXPECT_FALSE(token1 >= token3);
    EXPECT_TRUE(token2 >= token3);
    EXPECT_TRUE(token3 >= token2);
  }

  // Tests comparisons between two multi tokens that hold the same type but
  // different underlying values.
  {
    // Necessary because std::minmax() returns a pair of references.
    FooToken foo1;
    FooToken foo2;
    const auto& [lesser, greater] = std::minmax(foo1, foo2);
    FooBarToken token1 = lesser;
    FooBarToken token2 = greater;

    EXPECT_FALSE(token1 == token2);
    EXPECT_TRUE(token1 != token2);

    EXPECT_TRUE(token1 < token2);
    EXPECT_FALSE(token2 < token1);

    EXPECT_FALSE(token1 > token2);
    EXPECT_TRUE(token2 > token1);

    EXPECT_TRUE(token1 <= token2);
    EXPECT_FALSE(token2 <= token1);

    EXPECT_FALSE(token1 >= token2);
    EXPECT_TRUE(token2 >= token1);
  }
}

TEST(MultiTokenTest, Visit) {
  struct Visitor {
    std::string_view operator()(const FooToken& token) { return "FooToken"; }
    std::string_view operator()(const BarToken& token) { return "BarToken"; }
    std::string_view operator()(const BazToken& token) { return "BazToken"; }
  };

  FooBarBazToken token(FooToken{});
  EXPECT_EQ(token.Visit(Visitor()), "FooToken");

  token = BarToken{};
  EXPECT_EQ(token.Visit(Visitor()), "BarToken");

  token = BazToken{};
  EXPECT_EQ(token.Visit(Visitor()), "BazToken");
}

TEST(MultiTokenTest, CompatibleConstruction) {
  {
    FooBarToken foo_bar_token(FooToken{});
    FooBarBazToken foo_bar_baz_token(foo_bar_token);
    EXPECT_EQ(FooBarBazToken::Tag{0}, foo_bar_baz_token.variant_index());
  }
  {
    FooBarToken foo_bar_token(BarToken{});
    FooBarBazToken foo_bar_baz_token(foo_bar_token);
    EXPECT_EQ(FooBarBazToken::Tag{1}, foo_bar_baz_token.variant_index());
  }
}

TEST(MultiTokenTest, CompatibleAssignment) {
  FooBarBazToken foo_bar_baz_token;
  {
    FooBarToken foo_bar_token(FooToken{});
    foo_bar_baz_token = foo_bar_token;
    EXPECT_EQ(FooBarBazToken::Tag{0}, foo_bar_baz_token.variant_index());
  }
  {
    FooBarToken foo_bar_token(BarToken{});
    foo_bar_baz_token = foo_bar_token;
    EXPECT_EQ(FooBarBazToken::Tag{1}, foo_bar_baz_token.variant_index());
  }
}

}  // namespace blink
