// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/permissions_policy/permissions_policy_mojom_traits.h"

#include <memory>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy.mojom-shared.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

constexpr char kUrlA[] = "https://a.test/index.html";
constexpr char kUrlB[] = "https://b.test/index.html";

TEST(PermissionsPolicyMojomTraitsTest, Roundtrips_PermissionsPolicy) {
  ParsedPermissionsPolicyDeclaration
      policy_declaration_with_reporting_endpoint = {
          network::mojom::PermissionsPolicyFeature::
              kCamera, /*allowed_origins=*/
          {*network::OriginWithPossibleWildcards::FromOrigin(
              url::Origin::Create(GURL(kUrlB)))},
          /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/false,
          /*matches_opaque_src=*/false};
  policy_declaration_with_reporting_endpoint.reporting_endpoint =
      "https://example.com";

  std::unique_ptr<network::PermissionsPolicy> parent_policy =
      PermissionsPolicy::CreateFromParentPolicy(
          /*parent_policy=*/nullptr,
          /*header_policy=*/
          {{{network::mojom::PermissionsPolicyFeature::
                 kAccelerometer, /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(
                 url::Origin::Create(GURL(kUrlA)))},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/false,
             /*matches_opaque_src=*/true}}},
          /*container_policy=*/
          {{policy_declaration_with_reporting_endpoint}},
          url::Origin::Create(GURL(kUrlA)), true);

  network::ParsedPermissionsPolicy empty_container_policy;
  std::unique_ptr<network::PermissionsPolicy> parent_copied =
      PermissionsPolicy::CreateFromParentPolicy(
          /*parent_policy=*/nullptr,
          /*header_policy=*/empty_container_policy,
          /*container_policy=*/empty_container_policy,
          url::Origin::Create(GURL(kUrlA)));
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PermissionsPolicy>(
      *parent_policy, *parent_copied));
  EXPECT_EQ(*parent_policy, *parent_copied);

  std::unique_ptr<network::PermissionsPolicy> original =
      PermissionsPolicy::CreateFromParentPolicy(
          /*parent_policy=*/parent_policy.get(),
          /*header_policy=*/
          {{{network::mojom::PermissionsPolicyFeature::
                 kBrowsingTopics, /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(
                 url::Origin::Create(GURL(kUrlB)))},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/false,
             /*matches_opaque_src=*/false},
            {network::mojom::PermissionsPolicyFeature::
                 kSharedStorage, /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(
                 url::Origin::Create(GURL(kUrlA)))},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/false,
             /*matches_opaque_src=*/true}}},
          /*container_policy=*/
          {{{network::mojom::PermissionsPolicyFeature::
                 kStorageAccessAPI, /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(
                 url::Origin::Create(GURL(kUrlA)))},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/false,
             /*matches_opaque_src=*/false},
            {network::mojom::PermissionsPolicyFeature::
                 kAutoplay, /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(
                 url::Origin::Create(GURL(kUrlB)))},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/true,
             /*matches_opaque_src=*/false}}},
          url::Origin::Create(GURL(kUrlA)), false);

  std::unique_ptr<network::PermissionsPolicy> copied =
      PermissionsPolicy::CreateFromParentPolicy(
          /*parent_policy=*/nullptr,
          /*header_policy=*/empty_container_policy,
          /*container_policy=*/empty_container_policy,
          url::Origin::Create(GURL(kUrlA)));
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PermissionsPolicy>(
      *original, *copied));
  EXPECT_EQ(*original, *copied);
}

}  // namespace
}  // namespace network
