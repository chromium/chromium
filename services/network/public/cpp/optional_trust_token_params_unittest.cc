// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/optional_trust_token_params.h"

#include <optional>
#include <tuple>

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/origin.h"

namespace network {

namespace {
// For tests that require a populated OptionalTrustTokenParams, use this helper
// to avoid needing to update several tests every time the format of
// mojom::TrustTokenParams (and, consequently, the signature to its constructor)
// changes.
OptionalTrustTokenParams NonemptyTrustTokenParams() {
  return mojom::TrustTokenParams(
      mojom::TrustTokenOperationType::kRedemption,
      mojom::TrustTokenRefreshPolicy::kRefresh, "custom_key_commitment",
      url::Origin::Create(GURL("https://custom-issuer.com")),
      mojom::TrustTokenSignRequestData::kInclude,
      /*include_timestamp_header=*/true,
      std::vector<url::Origin>{url::Origin::Create(GURL("https://issuer.com"))},
      std::vector<std::string>{"some_header", "another_header"},
      "some additional signing data");
}
}  // namespace

TEST(OptionalTrustTokenParams, Empty) {
  EXPECT_EQ(OptionalTrustTokenParams(), OptionalTrustTokenParams());
  EXPECT_FALSE(OptionalTrustTokenParams().has_value());
  EXPECT_FALSE(OptionalTrustTokenParams(std::nullopt).has_value());

  EXPECT_EQ(OptionalTrustTokenParams(std::nullopt), OptionalTrustTokenParams());
  EXPECT_NE(OptionalTrustTokenParams(), NonemptyTrustTokenParams());
}

TEST(OptionalTrustTokenParams, CopyAndMove) {
  {
    OptionalTrustTokenParams in = NonemptyTrustTokenParams();
    EXPECT_TRUE(in.has_value());
    EXPECT_EQ(in, OptionalTrustTokenParams(in));

    OptionalTrustTokenParams assigned = in;
    EXPECT_EQ(in, assigned);

    OptionalTrustTokenParams moved(std::move(assigned));
    EXPECT_EQ(in, moved);

    OptionalTrustTokenParams move_assigned = std::move(moved);
    EXPECT_EQ(in, move_assigned);
  }

  {
    const OptionalTrustTokenParams const_in = NonemptyTrustTokenParams();
    EXPECT_TRUE(const_in.has_value());
    EXPECT_EQ(const_in, OptionalTrustTokenParams(const_in));

    const OptionalTrustTokenParams assigned = const_in;
    EXPECT_EQ(const_in, assigned);

    OptionalTrustTokenParams moved(std::move(assigned));
    EXPECT_EQ(const_in, moved);

    OptionalTrustTokenParams move_assigned = std::move(moved);
    EXPECT_EQ(const_in, move_assigned);
  }
}

TEST(OptionalTrustTokenParams, Dereference) {
  OptionalTrustTokenParams in = NonemptyTrustTokenParams();
  EXPECT_EQ(in->operation, mojom::TrustTokenOperationType::kRedemption);
  EXPECT_EQ(in.as_ptr()->operation,
            mojom::TrustTokenOperationType::kRedemption);
  EXPECT_EQ(in.value().operation, mojom::TrustTokenOperationType::kRedemption);
}

TEST(OptionalTrustTokenParams, DereferenceEmpty) {
  OptionalTrustTokenParams in = std::nullopt;
  EXPECT_CHECK_DEATH(std::ignore = in->operation);
  EXPECT_CHECK_DEATH(std::ignore = in.value());
  EXPECT_EQ(in.as_ptr(), mojom::TrustTokenParamsPtr());
}

}  // namespace network
