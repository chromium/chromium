// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

#include <memory>
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "url/origin.h"

namespace blink {

namespace {

std::unique_ptr<PermissionsPolicy> CreateFromParentPolicy(
    const PermissionsPolicy* parent,
    ParsedPermissionsPolicy header_policy,
    const url::Origin& origin) {
  ParsedPermissionsPolicy empty_container_policy;
  return PermissionsPolicy::CreateFromParentPolicy(
      parent, header_policy, empty_container_policy, origin);
}

}  // namespace

TEST(ResourceRequestTest, SetHasUserGesture) {
  ResourceRequest original;
  EXPECT_FALSE(original.HasUserGesture());
  original.SetHasUserGesture(true);
  EXPECT_TRUE(original.HasUserGesture());
  original.SetHasUserGesture(false);
  EXPECT_TRUE(original.HasUserGesture());
}

TEST(ResourceRequestTest, SetIsAdResource) {
  ResourceRequest original;
  EXPECT_FALSE(original.IsAdResource());
  original.SetIsAdResource();
  EXPECT_TRUE(original.IsAdResource());

  // Should persist across redirects.
  std::unique_ptr<ResourceRequest> redirect_request =
      original.CreateRedirectRequest(
          KURL("https://example.test/redirect"), original.HttpMethod(),
          original.SiteForCookies(), original.ReferrerString(),
          original.GetReferrerPolicy(), original.GetSkipServiceWorker());
  EXPECT_TRUE(redirect_request->IsAdResource());
}

TEST(ResourceRequestTest, UpgradeIfInsecureAcrossRedirects) {
  ResourceRequest original;
  EXPECT_FALSE(original.UpgradeIfInsecure());
  original.SetUpgradeIfInsecure(true);
  EXPECT_TRUE(original.UpgradeIfInsecure());

  // Should persist across redirects.
  std::unique_ptr<ResourceRequest> redirect_request =
      original.CreateRedirectRequest(
          KURL("https://example.test/redirect"), original.HttpMethod(),
          original.SiteForCookies(), original.ReferrerString(),
          original.GetReferrerPolicy(), original.GetSkipServiceWorker());
  EXPECT_TRUE(redirect_request->UpgradeIfInsecure());
}

// A cross-origin subresource request that explicitly sets an opt-in flag (e.g.
// `browsingTopics`, `sharedStorageWritable`) should have the corresponding
// permission as long as it passes the allowlist check, regardless of the
// feature's default state.
TEST(ResourceRequestTest, IsFeatureEnabledForSubresourceRequestAssumingOptIn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {blink::features::kBrowsingTopics, blink::features::kSharedStorageAPI},
      /*disabled_features=*/{});

  ResourceRequest request_with_topics_opt_in;
  request_with_topics_opt_in.SetBrowsingTopics(true);

  ResourceRequest request_with_shared_storage_opt_in;
  request_with_shared_storage_opt_in.SetSharedStorageWritableOptedIn(true);

  ResourceRequest request_with_both_opt_in;
  request_with_both_opt_in.SetBrowsingTopics(true);
  request_with_both_opt_in.SetSharedStorageWritableOptedIn(true);

  const url::Origin origin_a =
      url::Origin::Create(GURL("https://example.com/"));
  const url::Origin origin_b =
      url::Origin::Create(GURL("https://example.net/"));
  const url::Origin origin_c =
      url::Origin::Create(GURL("https://example.org/"));

  {
    // +--------------------------------------------------------+
    // |(1)Origin A                                             |
    // |No Policy                                               |
    // |                                                        |
    // | fetch(<Origin B's url>, {browsingTopics: true})        |
    // | fetch(<Origin B's url>, {sharedStorageWritable: true}) |
    // | fetch(<Origin B's url>, {browsingTopics: true,         |
    // |                          sharedStorageWritable: true}) |
    // +--------------------------------------------------------+

    std::unique_ptr<PermissionsPolicy> policy =
        CreateFromParentPolicy(nullptr, /*header_policy=*/{}, origin_a);

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics, origin_a));
    EXPECT_TRUE(request_with_topics_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_a));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_a));

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kSharedStorage, origin_a));
    EXPECT_TRUE(request_with_shared_storage_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_a));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_a));

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics, origin_b));
    EXPECT_TRUE(request_with_topics_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_b));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_b));

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kSharedStorage, origin_b));
    EXPECT_TRUE(request_with_shared_storage_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_b));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_b));
  }

  {
    // +--------------------------------------------------------+
    // |(1)Origin A                                             |
    // |Permissions-Policy: browsing-topics=(self),             |
    // |                    shared-storage=(self)               |
    // |                                                        |
    // | fetch(<Origin B's url>, {browsingTopics: true})        |
    // | fetch(<Origin B's url>, {sharedStorageWritable: true}) |
    // | fetch(<Origin B's url>, {browsingTopics: true,         |
    // |                          sharedStorageWritable: true}) |
    // +--------------------------------------------------------+

    std::unique_ptr<PermissionsPolicy> policy = CreateFromParentPolicy(
        nullptr,
        {{{mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
           /*allowed_origins=*/{},
           /*self_if_matches=*/origin_a,
           /*matches_all_origins=*/false,
           /*matches_opaque_src=*/false},
          {mojom::blink::PermissionsPolicyFeature::kSharedStorage,
           /*allowed_origins=*/{},
           /*self_if_matches=*/origin_a,
           /*matches_all_origins=*/false,
           /*matches_opaque_src=*/false}}},
        origin_a);

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics, origin_a));
    EXPECT_TRUE(request_with_topics_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_a));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_a));

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kSharedStorage, origin_a));
    EXPECT_TRUE(request_with_shared_storage_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_a));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_a));

    EXPECT_FALSE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics, origin_b));
    EXPECT_FALSE(
        request_with_topics_opt_in
            .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                policy.get(),
                mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                origin_b));
    EXPECT_FALSE(
        request_with_both_opt_in
            .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                policy.get(),
                mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                origin_b));

    EXPECT_FALSE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kSharedStorage, origin_b));
    EXPECT_FALSE(request_with_shared_storage_opt_in
                     .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                         policy.get(),
                         mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                         origin_b));
    EXPECT_FALSE(request_with_both_opt_in
                     .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                         policy.get(),
                         mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                         origin_b));
  }

  {
    // +--------------------------------------------------------+
    // |(1)Origin A                                             |
    // |Permissions-Policy: browsing-topics=(none),             |
    // |                    shared-storage=(none)               |
    // |                                                        |
    // | fetch(<Origin B's url>, {browsingTopics: true})        |
    // | fetch(<Origin B's url>, {sharedStorageWritable: true}) |
    // | fetch(<Origin B's url>, {browsingTopics: true,         |
    // |                          sharedStorageWritable: true}) |
    // +--------------------------------------------------------+

    std::unique_ptr<PermissionsPolicy> policy = CreateFromParentPolicy(
        nullptr,
        {{{mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
           /*allowed_origins=*/{},
           /*self_if_matches=*/std::nullopt,
           /*matches_all_origins=*/false,
           /*matches_opaque_src=*/false},
          {mojom::blink::PermissionsPolicyFeature::kSharedStorage,
           /*allowed_origins=*/{},
           /*self_if_matches=*/std::nullopt,
           /*matches_all_origins=*/false,
           /*matches_opaque_src=*/false}}},
        origin_a);

    EXPECT_FALSE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics, origin_a));
    EXPECT_FALSE(
        request_with_topics_opt_in
            .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                policy.get(),
                mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                origin_a));
    EXPECT_FALSE(
        request_with_both_opt_in
            .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                policy.get(),
                mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                origin_a));

    EXPECT_FALSE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kSharedStorage, origin_a));
    EXPECT_FALSE(request_with_shared_storage_opt_in
                     .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                         policy.get(),
                         mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                         origin_a));
    EXPECT_FALSE(request_with_both_opt_in
                     .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                         policy.get(),
                         mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                         origin_a));

    EXPECT_FALSE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics, origin_b));
    EXPECT_FALSE(
        request_with_topics_opt_in
            .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                policy.get(),
                mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                origin_b));
    EXPECT_FALSE(
        request_with_both_opt_in
            .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                policy.get(),
                mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                origin_b));

    EXPECT_FALSE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kSharedStorage, origin_b));
    EXPECT_FALSE(request_with_shared_storage_opt_in
                     .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                         policy.get(),
                         mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                         origin_b));
    EXPECT_FALSE(request_with_both_opt_in
                     .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                         policy.get(),
                         mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                         origin_b));
  }

  {
    // +--------------------------------------------------------+
    // |(1)Origin A                                             |
    // |Permissions-Policy: browsing-topics=*,                  |
    // |                    shared-storage=*                    |
    // |                                                        |
    // | fetch(<Origin B's url>, {browsingTopics: true})        |
    // | fetch(<Origin B's url>, {sharedStorageWritable: true}) |
    // | fetch(<Origin B's url>, {browsingTopics: true,         |
    // |                          sharedStorageWritable: true}) |
    // +--------------------------------------------------------+

    std::unique_ptr<PermissionsPolicy> policy = CreateFromParentPolicy(
        nullptr,
        {{{mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
           /*allowed_origins=*/{},
           /*self_if_matches=*/std::nullopt,
           /*matches_all_origins=*/true,
           /*matches_opaque_src=*/false},
          {mojom::blink::PermissionsPolicyFeature::kSharedStorage,
           /*allowed_origins=*/{},
           /*self_if_matches=*/std::nullopt,
           /*matches_all_origins=*/true,
           /*matches_opaque_src=*/false}}},
        origin_a);

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics, origin_a));
    EXPECT_TRUE(request_with_topics_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_a));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_a));

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kSharedStorage, origin_a));
    EXPECT_TRUE(request_with_shared_storage_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_a));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_a));

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics, origin_b));
    EXPECT_TRUE(request_with_topics_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_b));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_b));

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kSharedStorage, origin_b));
    EXPECT_TRUE(request_with_shared_storage_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_b));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_b));
  }

  {
    // +--------------------------------------------------------+
    // |(1)Origin A                                             |
    // |Permissions-Policy: browsing-topics=(Origin B),         |
    // |                    shared-storage=(Origin B)           |
    // |                                                        |
    // | fetch(<Origin B's url>, {browsingTopics: true})        |
    // | fetch(<Origin B's url>, {sharedStorageWritable: true}) |
    // | fetch(<Origin B's url>, {browsingTopics: true,         |
    // |                          sharedStorageWritable: true}) |
    // | fetch(<Origin C's url>, {browsingTopics: true})        |
    // | fetch(<Origin C's url>, {sharedStorageWritable: true}) |
    // | fetch(<Origin C's url>, {browsingTopics: true,         |
    // |                          sharedStorageWritable: true}) |
    // +--------------------------------------------------------+

    std::unique_ptr<PermissionsPolicy> policy = CreateFromParentPolicy(
        nullptr,
        {{{mojom::blink::PermissionsPolicyFeature::
               kBrowsingTopics, /*allowed_origins=*/
           {*blink::OriginWithPossibleWildcards::FromOrigin(origin_b)},
           /*self_if_matches=*/std::nullopt,
           /*matches_all_origins=*/false,
           /*matches_opaque_src=*/false},
          {mojom::blink::PermissionsPolicyFeature::
               kSharedStorage, /*allowed_origins=*/
           {*blink::OriginWithPossibleWildcards::FromOrigin(origin_b)},
           /*self_if_matches=*/std::nullopt,
           /*matches_all_origins=*/false,
           /*matches_opaque_src=*/false}}},
        origin_a);

    EXPECT_FALSE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics, origin_a));
    EXPECT_FALSE(
        request_with_topics_opt_in
            .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                policy.get(),
                mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                origin_a));
    EXPECT_FALSE(
        request_with_both_opt_in
            .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                policy.get(),
                mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                origin_a));

    EXPECT_FALSE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kSharedStorage, origin_a));
    EXPECT_FALSE(request_with_shared_storage_opt_in
                     .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                         policy.get(),
                         mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                         origin_a));
    EXPECT_FALSE(request_with_both_opt_in
                     .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                         policy.get(),
                         mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                         origin_a));

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics, origin_b));
    EXPECT_TRUE(request_with_topics_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_b));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                        origin_b));

    EXPECT_TRUE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kSharedStorage, origin_b));
    EXPECT_TRUE(request_with_shared_storage_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_b));
    EXPECT_TRUE(request_with_both_opt_in
                    .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                        policy.get(),
                        mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                        origin_b));

    EXPECT_FALSE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kBrowsingTopics, origin_c));
    EXPECT_FALSE(
        request_with_topics_opt_in
            .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                policy.get(),
                mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                origin_c));
    EXPECT_FALSE(
        request_with_both_opt_in
            .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                policy.get(),
                mojom::blink::PermissionsPolicyFeature::kBrowsingTopics,
                origin_c));

    EXPECT_FALSE(policy->IsFeatureEnabledForOrigin(
        mojom::blink::PermissionsPolicyFeature::kSharedStorage, origin_c));
    EXPECT_FALSE(request_with_shared_storage_opt_in
                     .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                         policy.get(),
                         mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                         origin_c));
    EXPECT_FALSE(request_with_both_opt_in
                     .IsFeatureEnabledForSubresourceRequestAssumingOptIn(
                         policy.get(),
                         mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                         origin_c));
  }
}

}  // namespace blink
