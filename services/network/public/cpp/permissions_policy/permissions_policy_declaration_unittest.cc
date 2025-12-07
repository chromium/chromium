// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"

#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

TEST(ParsedPermissionsPolicyDeclarationTest, Contains) {
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.test/"));
  const url::Origin kOpaqueOrigin = url::Origin();

  // Empty / default declaration.
  ParsedPermissionsPolicyDeclaration empty_decl;
  EXPECT_FALSE(empty_decl.Contains(kTestOrigin));
  EXPECT_FALSE(empty_decl.Contains(kOpaqueOrigin));

  // Matches opaque.
  ParsedPermissionsPolicyDeclaration opaque_decl;
  opaque_decl.matches_opaque_src = true;
  EXPECT_FALSE(opaque_decl.Contains(kTestOrigin));
  EXPECT_TRUE(opaque_decl.Contains(kOpaqueOrigin));

  // Matches all.
  ParsedPermissionsPolicyDeclaration all_decl;
  all_decl.matches_all_origins = true;
  EXPECT_TRUE(all_decl.Contains(kTestOrigin));
  EXPECT_TRUE(all_decl.Contains(kOpaqueOrigin));

  // Origin mismatch.
  ParsedPermissionsPolicyDeclaration mismatch_decl;
  mismatch_decl.allowed_origins.emplace_back(
      *network::OriginWithPossibleWildcards::FromOrigin(
          url::Origin::Create(GURL("https://example2.test/"))));
  EXPECT_FALSE(mismatch_decl.Contains(kTestOrigin));
  EXPECT_FALSE(mismatch_decl.Contains(kOpaqueOrigin));

  // Origin match.
  ParsedPermissionsPolicyDeclaration match_decl;
  match_decl.allowed_origins.emplace_back(
      *network::OriginWithPossibleWildcards::FromOrigin(
          url::Origin::Create(GURL("https://example.test/"))));
  EXPECT_TRUE(match_decl.Contains(kTestOrigin));
  EXPECT_FALSE(match_decl.Contains(kOpaqueOrigin));

  // Self match.
  ParsedPermissionsPolicyDeclaration self_decl;
  self_decl.self_if_matches =
      url::Origin::Create(GURL("https://example.test/"));
  EXPECT_TRUE(self_decl.Contains(kTestOrigin));
  EXPECT_FALSE(self_decl.Contains(kOpaqueOrigin));

  // Opaque self match.
  ParsedPermissionsPolicyDeclaration opaque_self_decl;
  opaque_self_decl.self_if_matches = kOpaqueOrigin;
  EXPECT_FALSE(opaque_self_decl.Contains(kTestOrigin));
  EXPECT_TRUE(opaque_self_decl.Contains(kOpaqueOrigin));
}

TEST(ParsedPermissionsPolicyDeclarationTest, Equality) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("https://example1.test"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("https://example2.test"));

  ParsedPermissionsPolicyDeclaration decl1(
      network::mojom::PermissionsPolicyFeature::kGyroscope);
  decl1.allowed_origins.emplace_back(
      *network::OriginWithPossibleWildcards::FromOrigin(kOrigin1));
  decl1.matches_all_origins = true;
  decl1.matches_opaque_src = false;
  decl1.self_if_matches = kOrigin1;

  // Same as decl1.
  ParsedPermissionsPolicyDeclaration decl2(
      network::mojom::PermissionsPolicyFeature::kGyroscope);
  decl2.allowed_origins.emplace_back(
      *network::OriginWithPossibleWildcards::FromOrigin(kOrigin1));
  decl2.matches_all_origins = true;
  decl2.matches_opaque_src = false;
  decl2.self_if_matches = kOrigin1;

  // Different feature.
  ParsedPermissionsPolicyDeclaration decl3(
      network::mojom::PermissionsPolicyFeature::kFullscreen);
  decl3.allowed_origins.emplace_back(
      *network::OriginWithPossibleWildcards::FromOrigin(kOrigin1));
  decl3.matches_all_origins = true;
  decl3.matches_opaque_src = false;
  decl3.self_if_matches = kOrigin1;

  // Different allowed origins.
  ParsedPermissionsPolicyDeclaration decl4(
      network::mojom::PermissionsPolicyFeature::kGyroscope);
  decl4.allowed_origins.emplace_back(
      *network::OriginWithPossibleWildcards::FromOrigin(kOrigin2));
  decl4.matches_all_origins = true;
  decl4.matches_opaque_src = false;
  decl4.self_if_matches = kOrigin1;

  // Different matches_all_origins.
  ParsedPermissionsPolicyDeclaration decl5(
      network::mojom::PermissionsPolicyFeature::kGyroscope);
  decl5.allowed_origins.emplace_back(
      *network::OriginWithPossibleWildcards::FromOrigin(kOrigin1));
  decl5.matches_all_origins = false;
  decl5.matches_opaque_src = false;
  decl5.self_if_matches = kOrigin1;

  // Different matches_opaque_src.
  ParsedPermissionsPolicyDeclaration decl6(
      network::mojom::PermissionsPolicyFeature::kGyroscope);
  decl6.allowed_origins.emplace_back(
      *network::OriginWithPossibleWildcards::FromOrigin(kOrigin1));
  decl6.matches_all_origins = true;
  decl6.matches_opaque_src = true;
  decl6.self_if_matches = kOrigin1;

  // Different self_if_matches.
  ParsedPermissionsPolicyDeclaration decl7(
      network::mojom::PermissionsPolicyFeature::kGyroscope);
  decl7.allowed_origins.emplace_back(
      *network::OriginWithPossibleWildcards::FromOrigin(kOrigin1));
  decl7.matches_all_origins = true;
  decl7.matches_opaque_src = false;
  decl7.self_if_matches = kOrigin2;

  EXPECT_TRUE(decl1 == decl2);
  EXPECT_FALSE(decl1 == decl3);
  EXPECT_FALSE(decl1 == decl4);
  EXPECT_FALSE(decl1 == decl5);
  EXPECT_FALSE(decl1 == decl6);
  EXPECT_FALSE(decl1 == decl7);
}

TEST(ParsedPermissionsPolicyDeclarationTest, Constructors) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("https://example1.test"));
  const std::vector<network::OriginWithPossibleWildcards> allowed_origins = {
      *network::OriginWithPossibleWildcards::FromOrigin(kOrigin1)};

  // Default constructor.
  ParsedPermissionsPolicyDeclaration decl1;
  // `decl1.feature` is not tested because its value is nondeterministic.
  EXPECT_TRUE(decl1.allowed_origins.empty());
  EXPECT_FALSE(decl1.self_if_matches);
  EXPECT_FALSE(decl1.matches_all_origins);
  EXPECT_FALSE(decl1.matches_opaque_src);

  // Feature constructor.
  ParsedPermissionsPolicyDeclaration decl2(
      network::mojom::PermissionsPolicyFeature::kGyroscope);
  EXPECT_EQ(decl2.feature,
            network::mojom::PermissionsPolicyFeature::kGyroscope);
  EXPECT_TRUE(decl2.allowed_origins.empty());
  EXPECT_FALSE(decl2.self_if_matches);
  EXPECT_FALSE(decl2.matches_all_origins);
  EXPECT_FALSE(decl2.matches_opaque_src);

  // All-members constructor.
  ParsedPermissionsPolicyDeclaration decl3(
      network::mojom::PermissionsPolicyFeature::kGyroscope, allowed_origins,
      kOrigin1, true, false);
  EXPECT_EQ(decl3.feature,
            network::mojom::PermissionsPolicyFeature::kGyroscope);
  EXPECT_EQ(decl3.allowed_origins, allowed_origins);
  EXPECT_EQ(decl3.self_if_matches, kOrigin1);
  EXPECT_TRUE(decl3.matches_all_origins);
  EXPECT_FALSE(decl3.matches_opaque_src);

  // Copy constructor.
  ParsedPermissionsPolicyDeclaration decl4(decl3);
  EXPECT_EQ(decl4.feature, decl3.feature);
  EXPECT_EQ(decl4.allowed_origins, decl3.allowed_origins);
  EXPECT_EQ(decl4.self_if_matches, decl3.self_if_matches);
  EXPECT_EQ(decl4.matches_all_origins, decl3.matches_all_origins);
  EXPECT_EQ(decl4.matches_opaque_src, decl3.matches_opaque_src);

  // Move constructor.
  ParsedPermissionsPolicyDeclaration decl5(std::move(decl4));
  EXPECT_EQ(decl5.feature, decl3.feature);
  EXPECT_EQ(decl5.allowed_origins, decl3.allowed_origins);
  EXPECT_EQ(decl5.self_if_matches, decl3.self_if_matches);
  EXPECT_EQ(decl5.matches_all_origins, decl3.matches_all_origins);
  EXPECT_EQ(decl5.matches_opaque_src, decl3.matches_opaque_src);
}

TEST(ParsedPermissionsPolicyDeclarationTest, Assignment) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("https://example1.test"));
  const std::vector<network::OriginWithPossibleWildcards> allowed_origins = {
      *network::OriginWithPossibleWildcards::FromOrigin(kOrigin1)};

  // All-members constructor.
  ParsedPermissionsPolicyDeclaration decl1(
      network::mojom::PermissionsPolicyFeature::kGyroscope, allowed_origins,
      kOrigin1, true, false);

  // Copy assignment.
  ParsedPermissionsPolicyDeclaration decl2;
  decl2 = decl1;
  EXPECT_EQ(decl2.feature, decl1.feature);
  EXPECT_EQ(decl2.allowed_origins, decl1.allowed_origins);
  EXPECT_EQ(decl2.self_if_matches, decl1.self_if_matches);
  EXPECT_EQ(decl2.matches_all_origins, decl1.matches_all_origins);
  EXPECT_EQ(decl2.matches_opaque_src, decl1.matches_opaque_src);

  // Move assignment.
  ParsedPermissionsPolicyDeclaration decl3;
  decl3 = std::move(decl2);
  EXPECT_EQ(decl3.feature, decl1.feature);
  EXPECT_EQ(decl3.allowed_origins, decl1.allowed_origins);
  EXPECT_EQ(decl3.self_if_matches, decl1.self_if_matches);
  EXPECT_EQ(decl3.matches_all_origins, decl1.matches_all_origins);
  EXPECT_EQ(decl3.matches_opaque_src, decl1.matches_opaque_src);
}

}  // namespace network
