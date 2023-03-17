// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

namespace {

mojom::PermissionsPolicyFeature kDefaultOnFeature =
    static_cast<mojom::PermissionsPolicyFeature>(
        static_cast<int>(mojom::PermissionsPolicyFeature::kMaxValue) + 1);

mojom::PermissionsPolicyFeature kDefaultSelfFeature =
    static_cast<mojom::PermissionsPolicyFeature>(
        static_cast<int>(mojom::PermissionsPolicyFeature::kMaxValue) + 2);

// This feature is defined in code, but not present in the feature list.
mojom::PermissionsPolicyFeature kUnavailableFeature =
    static_cast<mojom::PermissionsPolicyFeature>(
        static_cast<int>(mojom::PermissionsPolicyFeature::kMaxValue) + 4);

}  // namespace

class PermissionsPolicyTest : public testing::Test {
 protected:
  PermissionsPolicyTest()
      : feature_list_(
            {{kDefaultOnFeature, PermissionsPolicyFeatureDefault::EnableForAll},
             {kDefaultSelfFeature,
              PermissionsPolicyFeatureDefault::EnableForSelf},
             {mojom::PermissionsPolicyFeature::kBrowsingTopics,
              PermissionsPolicyFeatureDefault::EnableForSelf},
             {mojom::PermissionsPolicyFeature::kClientHintDPR,
              PermissionsPolicyFeatureDefault::EnableForSelf},
             {mojom::PermissionsPolicyFeature::kAttributionReporting,
              PermissionsPolicyFeatureDefault::EnableForSelf},
             {mojom::PermissionsPolicyFeature::kSharedStorage,
              PermissionsPolicyFeatureDefault::EnableForSelf}}) {}

  ~PermissionsPolicyTest() override = default;

  std::unique_ptr<PermissionsPolicy> CreateFromParentPolicy(
      const PermissionsPolicy* parent,
      const url::Origin& origin) {
    ParsedPermissionsPolicy empty_container_policy;
    return PermissionsPolicy::CreateFromParentPolicy(
        parent, empty_container_policy, origin, feature_list_);
  }

  std::unique_ptr<PermissionsPolicy> CreateFromParsedPolicy(
      const ParsedPermissionsPolicy& parsed_policy,
      const url::Origin& origin) {
    return PermissionsPolicy::CreateFromParsedPolicy(parsed_policy, origin,
                                                     feature_list_);
  }

  std::unique_ptr<PermissionsPolicy> CreateFromParentWithFramePolicy(
      const PermissionsPolicy* parent,
      const ParsedPermissionsPolicy& frame_policy,
      const url::Origin& origin) {
    return PermissionsPolicy::CreateFromParentPolicy(parent, frame_policy,
                                                     origin, feature_list_);
  }
  std::unique_ptr<PermissionsPolicy> CreateForFencedFrame(
      const url::Origin& origin,
      const blink::mojom::FencedFrameMode mode) {
    return PermissionsPolicy::CreateForFencedFrame(origin, feature_list_, mode);
  }

  bool PolicyContainsInheritedValue(const PermissionsPolicy* policy,
                                    mojom::PermissionsPolicyFeature feature) {
    return policy->inherited_policies_.find(feature) !=
           policy->inherited_policies_.end();
  }

  url::Origin origin_a_ = url::Origin::Create(GURL("https://example.com/"));
  url::Origin origin_b_ = url::Origin::Create(GURL("https://example.net/"));
  url::Origin origin_c_ = url::Origin::Create(GURL("https://example.org/"));

 private:
  // Contains the list of controlled features, so that we are guaranteed to
  // have at least one of each kind of default behaviour represented.
  PermissionsPolicyFeatureList feature_list_;
};

TEST_F(PermissionsPolicyTest, TestInitialPolicy) {
  // +-------------+
  // |(1)Origin A  |
  // |No Policy    |
  // +-------------+
  // Default-on and top-level-only features should be enabled in top-level
  // frame.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestInitialSameOriginChildPolicy) {
  // +-----------------+
  // |(1)Origin A      |
  // |No Policy        |
  // | +-------------+ |
  // | |(2)Origin A  | |
  // | |No Policy    | |
  // | +-------------+ |
  // +-----------------+
  // Default-on and Default-self features should be enabled in a same-origin
  // child frame. Default-off features should be disabled.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestInitialCrossOriginChildPolicy) {
  // +-----------------+
  // |(1)Origin A      |
  // |No Policy        |
  // | +-------------+ |
  // | |(2)Origin B  | |
  // | |No Policy    | |
  // | +-------------+ |
  // +-----------------+
  // Default-on features should be enabled in child frame. Default-self and
  // Default-off features should be disabled.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestCrossOriginChildCannotEnableFeature) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |No Policy                                 |
  // | +--------------------------------------+ |
  // | |(2) Origin B                          | |
  // | |Permissions-Policy: default-self=self | |
  // | +--------------------------------------+ |
  // +------------------------------------------+
  // Default-self feature should be disabled in cross origin frame, even if no
  // policy was specified in the parent frame.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestFrameSelfInheritance) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Permissions-Policy: default-self=self     |
  // | +-----------------+  +-----------------+ |
  // | |(2) Origin A     |  |(4) Origin B     | |
  // | |No Policy        |  |No Policy        | |
  // | | +-------------+ |  | +-------------+ | |
  // | | |(3)Origin A  | |  | |(5)Origin B  | | |
  // | | |No Policy    | |  | |No Policy    | | |
  // | | +-------------+ |  | +-------------+ | |
  // | +-----------------+  +-----------------+ |
  // +------------------------------------------+
  // Feature should be enabled at the top-level, and through the chain of
  // same-origin frames 2 and 3. It should be disabled in frames 4 and 5, as
  // they are at a different origin.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy5 =
      CreateFromParentPolicy(policy4.get(), origin_b_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy5->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestReflexiveFrameSelfInheritance) {
  // +--------------------------------------+
  // |(1) Origin A                          |
  // |Permissions-Policy: default-self=self |
  // | +-----------------+                  |
  // | |(2) Origin B     |                  |
  // | |No Policy        |                  |
  // | | +-------------+ |                  |
  // | | |(3)Origin A  | |                  |
  // | | |No Policy    | |                  |
  // | | +-------------+ |                  |
  // | +-----------------+                  |
  // +--------------------------------------+
  // Feature which is enabled at top-level should be disabled in frame 3, as
  // it is embedded by frame 2, for which the feature is not enabled.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestSelectiveFrameInheritance) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Permissions-Policy: default-self="OriginB"|
  // | +-----------------+  +-----------------+ |
  // | |(2) Origin B     |  |(3) Origin C     | |
  // | |No Policy        |  |No Policy        | |
  // | |                 |  | +-------------+ | |
  // | |                 |  | |(4)Origin B  | | |
  // | |                 |  | |No Policy    | | |
  // | |                 |  | +-------------+ | |
  // | +-----------------+  +-----------------+ |
  // +------------------------------------------+
  // Feature should be disabled in all frames, even though the
  // header indicates Origin B, there is no container policy to explicitly
  // delegate to that origin, in either frame 2 or 4.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_c_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy3.get(), origin_b_);
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestSelectiveFrameInheritance2) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Permissions-Policy: default-self="OriginB"|
  // | <iframe allow="default-self OriginB">    |
  // | +-----------------+  +-----------------+ |
  // | |(2) Origin B     |  |(3) Origin C     | |
  // | |No Policy        |  |No Policy        | |
  // | |                 |  | +-------------+ | |
  // | |                 |  | |(4)Origin B  | | |
  // | |                 |  | |No Policy    | | |
  // | |                 |  | +-------------+ | |
  // | +-----------------+  +-----------------+ |
  // +------------------------------------------+
  // Feature should be enabled in second level Origin B frame, but disabled in
  // Frame 4, because it is embedded by frame 3, where the feature is not
  // enabled.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_c_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy3.get(), origin_b_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestPolicyCanBlockSelf) {
  // +----------------------------------+
  // |(1)Origin A                       |
  // |Permissions-Policy: default-on=() |
  // +----------------------------------+
  // Default-on feature should be disabled in top-level frame.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, /*allowed_origins=*/{}, false, false}}});
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(PermissionsPolicyTest, TestParentPolicyBlocksSameOriginChildPolicy) {
  // +----------------------------------+
  // |(1)Origin A                       |
  // |Permissions-Policy: default-on=() |
  // | +-------------+                  |
  // | |(2)Origin A  |                  |
  // | |No Policy    |                  |
  // | +-------------+                  |
  // +----------------------------------+
  // Feature should be disabled in child frame.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, /*allowed_origins=*/{}, false, false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(PermissionsPolicyTest, TestChildPolicyCanBlockSelf) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |No Policy                             |
  // | +----------------------------------+ |
  // | |(2)Origin B                       | |
  // | |Permissions-Policy: default-on=() | |
  // | +----------------------------------+ |
  // +--------------------------------------+
  // Default-on feature should be disabled by cross-origin child frame.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultOnFeature, /*allowed_origins=*/{}, false, false}}});
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(PermissionsPolicyTest, TestChildPolicyCanBlockChildren) {
  // +----------------------------------------+
  // |(1)Origin A                             |
  // |No Policy                               |
  // | +------------------------------------+ |
  // | |(2)Origin B                         | |
  // | |Permissions-Policy: default-on=self | |
  // | | +-------------+                    | |
  // | | |(3)Origin C  |                    | |
  // | | |No Policy    |                    | |
  // | | +-------------+                    | |
  // | +------------------------------------+ |
  // +----------------------------------------+
  // Default-on feature should be enabled in frames 1 and 2; disabled in frame
  // 3 by child frame policy.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultOnFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(PermissionsPolicyTest, TestParentPolicyBlocksCrossOriginChildPolicy) {
  // +----------------------------------+
  // |(1)Origin A                       |
  // |Permissions-Policy: default-on=() |
  // | +-------------+                  |
  // | |(2)Origin B  |                  |
  // | |No Policy    |                  |
  // | +-------------+                  |
  // +----------------------------------+
  // Default-on feature should be disabled in cross-origin child frame.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, /*allowed_origins=*/{}, false, false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(PermissionsPolicyTest, TestEnableForAllOrigins) {
  // +----------------------------------+
  // |(1) Origin A                      |
  // |Permissions-Policy: default-self=*|
  // | +-----------------+              |
  // | |(2) Origin B     |              |
  // | |No Policy        |              |
  // | | +-------------+ |              |
  // | | |(3)Origin A  | |              |
  // | | |No Policy    | |              |
  // | | +-------------+ |              |
  // | +-----------------+              |
  // +----------------------------------+
  // Feature should be enabled in top level; disabled in frame 2 and 3.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestEnableForAllOriginsAndDelegate) {
  // +--------------------------------------+
  // |(1) Origin A                          |
  // |Permissions-Policy: default-self=*    |
  // |<iframe allow="default-self OriginB"> |
  // | +-----------------+                  |
  // | |(2) Origin B     |                  |
  // | |No Policy        |                  |
  // | | +-------------+ |                  |
  // | | |(3)Origin A  | |                  |
  // | | |No Policy    | |                  |
  // | | +-------------+ |                  |
  // | +-----------------+                  |
  // +--------------------------------------+
  // Feature should be enabled in top and second level; disabled in frame 3.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}});
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestDefaultOnStillNeedsSelf) {
  // +-----------------------------------------+
  // |(1) Origin A                             |
  // |Permissions-Policy: default-on="OriginB" |
  // | +-----------------------------------+   |
  // | |(2) Origin B                       |   |
  // | |No Policy                          |   |
  // | | +-------------+   +-------------+ |   |
  // | | |(3)Origin B  |   |(4)Origin C  | |   |
  // | | |No Policy    |   |No Policy    | |   |
  // | | +-------------+   +-------------+ |   |
  // | +-----------------------------------+   |
  // +-----------------------------------------+
  // Feature should be disabled in all frames.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(PermissionsPolicyTest, TestDefaultOnEnablesForAllDescendants) {
  // +------------------------------------------------+
  // |(1) Origin A                                    |
  // |Permissions-Policy: default-on=(self "OriginB") |
  // | +-----------------------------------+          |
  // | |(2) Origin B                       |          |
  // | |No Policy                          |          |
  // | | +-------------+   +-------------+ |          |
  // | | |(3)Origin B  |   |(4)Origin C  | |          |
  // | | |No Policy    |   |No Policy    | |          |
  // | | +-------------+   +-------------+ |          |
  // | +-----------------------------------+          |
  // +------------------------------------------------+
  // Feature should be enabled in all frames.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false),
          blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy4->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(PermissionsPolicyTest, TestDefaultSelfRequiresDelegation) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Permissions-Policy: default-self="OriginB"|
  // | +-----------------------------------+    |
  // | |(2) Origin B                       |    |
  // | |No Policy                          |    |
  // | | +-------------+   +-------------+ |    |
  // | | |(3)Origin B  |   |(4)Origin C  | |    |
  // | | |No Policy    |   |No Policy    | |    |
  // | | +-------------+   +-------------+ |    |
  // | +-----------------------------------+    |
  // +------------------------------------------+
  // Feature should be disabled in all frames.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestDefaultSelfRespectsSameOriginEmbedding) {
  // +--------------------------------------------------+
  // |(1) Origin A                                      |
  // |Permissions-Policy: default-self=(self "OriginB") |
  // |<iframe allow="default-self">                     |
  // | +-----------------------------------+            |
  // | |(2) Origin B                       |            |
  // | |No Policy                          |            |
  // | | +-------------+   +-------------+ |            |
  // | | |(3)Origin B  |   |(4)Origin C  | |            |
  // | | |No Policy    |   |No Policy    | |            |
  // | | +-------------+   +-------------+ |            |
  // | +-----------------------------------+            |
  // +--------------------------------------------------+
  // Feature should be disabled in frame 4; enabled in frames 1, 2 and 3.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false),
          blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestDelegationRequiredAtAllLevels) {
  // +------------------------------------+
  // |(1) Origin A                        |
  // |<iframe allow="default-self *">     |
  // | +--------------------------------+ |
  // | |(2) Origin B                    | |
  // | |No Policy                       | |
  // | | +-------------+                | |
  // | | |(3)Origin A  |                | |
  // | | |No Policy    |                | |
  // | | +-------------+                | |
  // | +--------------------------------+ |
  // +------------------------------------+
  // Feature should be enabled in frames 1 and 2. Feature is not enabled in
  // frame 3, even though it is the same origin as the top-level, because it is
  // not explicitly delegated.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}});
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, true}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestBlockedFrameCannotReenable) {
  // +----------------------------------------+
  // |(1)Origin A                             |
  // |Permissions-Policy: default-self=self   |
  // | +----------------------------------+   |
  // | |(2)Origin B                       |   |
  // | |Permissions-Policy: default-self=*|   |
  // | | +-------------+  +-------------+ |   |
  // | | |(3)Origin A  |  |(4)Origin C  | |   |
  // | | |No Policy    |  |No Policy    | |   |
  // | | +-------------+  +-------------+ |   |
  // | +----------------------------------+   |
  // +----------------------------------------+
  // Feature should be enabled at the top level; disabled in all other frames.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}});
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestEnabledFrameCanDelegate) {
  // +---------------------------------------------------+
  // |(1) Origin A                                       |
  // |No Policy                                          |
  // |<iframe allow="default-self">                      |
  // | +-----------------------------------------------+ |
  // | |(2) Origin B                                   | |
  // | |No Policy                                      | |
  // | |<iframe allow="default-self">                  | |
  // | | +-------------+                               | |
  // | | |(3)Origin C  |                               | |
  // | | |No Policy    |                               | |
  // | | +-------------+                               | |
  // | +-----------------------------------------------+ |
  // +---------------------------------------------------+
  // Feature should be enabled in all frames.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  ParsedPermissionsPolicy frame_policy2 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_c_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy2, origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestEnabledFrameCanDelegateByDefault) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Permissions-Policy: default-on=(self "OriginB")|
  // | +--------------------+ +--------------------+ |
  // | |(2) Origin B        | | (4) Origin C       | |
  // | |No Policy           | | No Policy          | |
  // | | +-------------+    | |                    | |
  // | | |(3)Origin C  |    | |                    | |
  // | | |No Policy    |    | |                    | |
  // | | +-------------+    | |                    | |
  // | +--------------------+ +--------------------+ |
  // +-----------------------------------------------+
  // Feature should be enabled in frames 1, 2, and 3, and disabled in frame 4.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{
      {kDefaultOnFeature, /*allowed_origins=*/
       {blink::OriginWithPossibleWildcards(origin_a_,
                                           /*has_subdomain_wildcard=*/false),
        blink::OriginWithPossibleWildcards(origin_b_,
                                           /*has_subdomain_wildcard=*/false)},
       false,
       false},
  }});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy1.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(PermissionsPolicyTest, TestFeaturesDontDelegateByDefault) {
  // +-------------------------------------------------+
  // |(1) Origin A                                     |
  // |Permissions-Policy: default-self=(self "OriginB")|
  // | +--------------------+ +--------------------+   |
  // | |(2) Origin B        | | (4) Origin C       |   |
  // | |No Policy           | | No Policy          |   |
  // | | +-------------+    | |                    |   |
  // | | |(3)Origin C  |    | |                    |   |
  // | | |No Policy    |    | |                    |   |
  // | | +-------------+    | |                    |   |
  // | +--------------------+ +--------------------+   |
  // +-------------------------------------------------+
  // Feature should be enabled in frames 1 only. Without a container policy, the
  // feature is not delegated to any child frames.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false),
          blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy1.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, TestFeaturesAreIndependent) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |No Policy                                      |
  // |<iframe allow="default-self 'self' OriginB;    |
  // |               default-on 'self'>              |
  // | +-------------------------------------------+ |
  // | |(2) Origin B                               | |
  // | |No Policy                                  | |
  // | |<iframe allow="default-self 'self' OriginC;| |
  // | |               default-on 'self'>          | |
  // | | +-------------+                           | |
  // | | |(3)Origin C  |                           | |
  // | | |No Policy    |                           | |
  // | | +-------------+                           | |
  // | +-------------------------------------------+ |
  // +-----------------------------------------------+
  // Default-self feature should be enabled in all frames; Default-on feature
  // should be enabled in frame 1, and disabled in frames 2 and 3.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false),
          blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false},
        {kDefaultOnFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_a_,
                                            /*has_subdomain_wildcard=*/false),
         blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false},
       {kDefaultOnFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_a_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  ParsedPermissionsPolicy frame_policy2 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_a_,
                                            /*has_subdomain_wildcard=*/false),
         blink::OriginWithPossibleWildcards(origin_c_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false},
       {kDefaultOnFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy2, origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultOnFeature));
}

// Test frame policies

TEST_F(PermissionsPolicyTest, TestSimpleFramePolicy) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |No Policy                             |
  // |                                      |
  // |<iframe allow="default-self OriginB"> |
  // | +-------------+                      |
  // | |(2)Origin B  |                      |
  // | |No Policy    |                      |
  // | +-------------+                      |
  // +--------------------------------------+
  // Default-self feature should be enabled in cross-origin child frame because
  // permission was delegated through frame policy.
  // This is the same scenario as when the iframe is declared as
  // <iframe allow="default-self">
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
}

TEST_F(PermissionsPolicyTest, TestAllOriginFramePolicy) {
  // +--------------------------------+
  // |(1)Origin A                     |
  // |No Policy                       |
  // |                                |
  // |<iframe allow="default-self *"> |
  // | +-------------+                |
  // | |(2)Origin B  |                |
  // | |No Policy    |                |
  // | +-------------+                |
  // +--------------------------------+
  // Default-self feature should be enabled in cross-origin child frame because
  // permission was delegated through frame policy.
  // This is the same scenario that arises when the iframe is declared as
  // <iframe allowfullscreen>
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
}

TEST_F(PermissionsPolicyTest, TestFramePolicyCanBeFurtherDelegated) {
  // +------------------------------------------+
  // |(1)Origin A                               |
  // |No Policy                                 |
  // |                                          |
  // |<iframe allow="default-self OriginB">     |
  // | +--------------------------------------+ |
  // | |(2)Origin B                           | |
  // | |No Policy                             | |
  // | |                                      | |
  // | |<iframe allow="default-self OriginC"> | |
  // | | +-------------+                      | |
  // | | |(3)Origin C  |                      | |
  // | | |No Policy    |                      | |
  // | | +-------------+                      | |
  // | |                                      | |
  // | |<iframe> (No frame policy)            | |
  // | | +-------------+                      | |
  // | | |(4)Origin C  |                      | |
  // | | |No Policy    |                      | |
  // | | +-------------+                      | |
  // | +--------------------------------------+ |
  // +------------------------------------------+
  // Default-self feature should be enabled in cross-origin child frames 2 and
  // 3. Feature should be disabled in frame 4 because it was not further
  // delegated through frame policy.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedPermissionsPolicy frame_policy1 = {{
      {kDefaultSelfFeature, /*allowed_origins=*/
       {blink::OriginWithPossibleWildcards(origin_b_,
                                           /*has_subdomain_wildcard=*/false)},
       false,
       false},
  }};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  ParsedPermissionsPolicy frame_policy2 = {{
      {kDefaultSelfFeature, /*allowed_origins=*/
       {blink::OriginWithPossibleWildcards(origin_c_,
                                           /*has_subdomain_wildcard=*/false)},
       false,
       false},
  }};
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy2, origin_c_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_TRUE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
  EXPECT_FALSE(
      policy4->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
}

TEST_F(PermissionsPolicyTest, TestDefaultOnCanBeDisabledByFramePolicy) {
  // +-----------------------------------+
  // |(1)Origin A                        |
  // |No Policy                          |
  // |                                   |
  // |<iframe allow="default-on 'none'"> |
  // | +-------------+                   |
  // | |(2)Origin A  |                   |
  // | |No Policy    |                   |
  // | +-------------+                   |
  // |                                   |
  // |<iframe allow="default-on 'none'"> |
  // | +-------------+                   |
  // | |(3)Origin B  |                   |
  // | |No Policy    |                   |
  // | +-------------+                   |
  // +-----------------------------------+
  // Default-on feature should be disabled in both same-origin and cross-origin
  // child frames because permission was removed through frame policy.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedPermissionsPolicy frame_policy1 = {
      {{kDefaultOnFeature, /*allowed_origins=*/{}, false, false}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_a_);
  ParsedPermissionsPolicy frame_policy2 = {
      {{kDefaultOnFeature, /*allowed_origins=*/{}, false, false}}};
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy2, origin_b_);
  EXPECT_TRUE(policy1->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_TRUE(policy1->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_b_));
  EXPECT_TRUE(policy1->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_c_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_b_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_c_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_b_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_c_));
}

TEST_F(PermissionsPolicyTest, TestFramePolicyModifiesHeaderPolicy) {
  // +-------------------------------------------------+
  // |(1)Origin A                                      |
  // |Permissions-Policy: default-self=(self "OriginB")|
  // |                                                 |
  // |<iframe allow="default-self 'none'">             |
  // | +-----------------------------------------+     |
  // | |(2)Origin B                              |     |
  // | |No Policy                                |     |
  // | +-----------------------------------------+     |
  // |                                                 |
  // |<iframe allow="default-self 'none'">             |
  // | +-----------------------------------------+     |
  // | |(3)Origin B                              |     |
  // | |Permissions-Policy: default-self=self    |     |
  // | +-----------------------------------------+     |
  // +-------------------------------------------------+
  // Default-self feature should be disabled in both cross-origin child frames
  // by frame policy, even though the parent frame's header policy would
  // otherwise enable it. This is true regardless of the child frame's header
  // policy.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{
      {kDefaultSelfFeature, /*allowed_origins=*/
       {blink::OriginWithPossibleWildcards(origin_a_,
                                           /*has_subdomain_wildcard=*/false),
        blink::OriginWithPossibleWildcards(origin_b_,
                                           /*has_subdomain_wildcard=*/false)},
       false,
       false},
  }});
  ParsedPermissionsPolicy frame_policy1 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, false, false}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  ParsedPermissionsPolicy frame_policy2 = {{
      {kDefaultSelfFeature, /*allowed_origins=*/{}, false, false},
  }};
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy2, origin_b_);
  policy3->SetHeaderPolicy({{
      {kDefaultSelfFeature, /*allowed_origins=*/
       {blink::OriginWithPossibleWildcards(origin_b_,
                                           /*has_subdomain_wildcard=*/false)},
       false,
       false},
  }});
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
}

TEST_F(PermissionsPolicyTest, TestCombineFrameAndHeaderPolicies) {
  // +-----------------------------------------+
  // |(1)Origin A                              |
  // |No Policy                                |
  // |                                         |
  // |<iframe allow="default-self OriginB">    |
  // | +-------------------------------------+ |
  // | |(2)Origin B                          | |
  // | |Permissions-Policy: default-self=*   | |
  // | |                                     | |
  // | |<iframe allow="default-self 'none'"> | |
  // | | +-------------+                     | |
  // | | |(3)Origin C  |                     | |
  // | | |No Policy    |                     | |
  // | | +-------------+                     | |
  // | |                                     | |
  // | |<iframe> (No frame policy)           | |
  // | | +-------------+                     | |
  // | | |(4)Origin C  |                     | |
  // | | |No Policy    |                     | |
  // | | +-------------+                     | |
  // | +-------------------------------------+ |
  // +-----------------------------------------+
  // Default-self feature should be enabled in cross-origin child frames 2 and
  // 4. Feature should be disabled in frame 3 by frame policy.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedPermissionsPolicy frame_policy1 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}});
  ParsedPermissionsPolicy frame_policy2 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, false, false}}};
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy2, origin_c_);
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
}

TEST_F(PermissionsPolicyTest, TestFeatureDeclinedAtTopLevel) {
  // +-----------------------------------------+
  // |(1)Origin A                              |
  // |Permissions-Policy: default-self=()      |
  // |                                         |
  // |<iframe allow="default-self OriginB">    |
  // | +-------------------------------------+ |
  // | |(2)Origin B                          | |
  // | |No Policy                            | |
  // | +-------------------------------------+ |
  // |                                         |
  // |<iframe allow="default-self *">          |
  // | +-------------------------------------+ |
  // | |(3)Origin A                          | |
  // | |No Policy                            | |
  // | +-------------------------------------+ |
  // +-----------------------------------------+
  // Default-self feature should be disabled in all frames.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{
      {kDefaultSelfFeature, /*allowed_origins=*/{}, false, false},
  }});
  ParsedPermissionsPolicy frame_policy1 = {{
      {kDefaultSelfFeature, /*allowed_origins=*/
       {blink::OriginWithPossibleWildcards(origin_b_,
                                           /*has_subdomain_wildcard=*/false)},
       false,
       false},
  }};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  ParsedPermissionsPolicy frame_policy2 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}};
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy2, origin_a_);
  EXPECT_FALSE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
}

TEST_F(PermissionsPolicyTest, TestFeatureDelegatedAndAllowed) {
  // +--------------------------------------------------+
  // |(1)Origin A                                       |
  // |Permissions-Policy: default-self=(self "OriginB") |
  // |                                                  |
  // |<iframe allow="default-self OriginA">             |
  // | +-------------------------------------+          |
  // | |(2)Origin B                          |          |
  // | |No Policy                            |          |
  // | +-------------------------------------+          |
  // |                                                  |
  // |<iframe allow="default-self OriginB">             |
  // | +-------------------------------------+          |
  // | |(3)Origin B                          |          |
  // | |No Policy                            |          |
  // | +-------------------------------------+          |
  // |                                                  |
  // |<iframe allow="default-self *">                   |
  // | +-------------------------------------+          |
  // | |(4)Origin B                          |          |
  // | |No Policy                            |          |
  // | +-------------------------------------+          |
  // +--------------------------------------------------+
  // Default-self feature should be disabled in frame 2, as the origin does not
  // match, and enabled in the remaining frames.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false),
          blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  ParsedPermissionsPolicy frame_policy1 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_a_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  ParsedPermissionsPolicy frame_policy2 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy2, origin_b_);
  ParsedPermissionsPolicy frame_policy3 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}};
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy3, origin_b_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_TRUE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_TRUE(
      policy4->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
}

TEST_F(PermissionsPolicyTest, TestDefaultSandboxedFramePolicy) {
  // +------------------+
  // |(1)Origin A       |
  // |No Policy         |
  // |                  |
  // |<iframe sandbox>  |
  // | +-------------+  |
  // | |(2)Sandboxed |  |
  // | |No Policy    |  |
  // | +-------------+  |
  // +------------------+
  // Default-on feature should be enabled in child frame with opaque origin.
  // Other features should be disabled.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  url::Origin sandboxed_origin = url::Origin();
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), sandboxed_origin);
  EXPECT_TRUE(policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, sandboxed_origin));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_FALSE(policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                  sandboxed_origin));
}

TEST_F(PermissionsPolicyTest, TestSandboxedFramePolicyForAllOrigins) {
  // +----------------------------------------+
  // |(1)Origin A                             |
  // |No Policy                               |
  // |                                        |
  // |<iframe sandbox allow="default-self *"> |
  // | +-------------+                        |
  // | |(2)Sandboxed |                        |
  // | |No Policy    |                        |
  // | +-------------+                        |
  // +----------------------------------------+
  // Default-self feature should be enabled in child frame with opaque origin,
  // only for that origin, because container policy matches all origins.
  // However, it will not pass that on to any other origin
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  url::Origin sandboxed_origin = url::Origin();
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, true}}};
  std::unique_ptr<PermissionsPolicy> policy2 = CreateFromParentWithFramePolicy(
      policy1.get(), frame_policy, sandboxed_origin);
  EXPECT_TRUE(policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, sandboxed_origin));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                 sandboxed_origin));
}

TEST_F(PermissionsPolicyTest, TestSandboxedFramePolicyForOpaqueSrcOrigin) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |No Policy                             |
  // |                                      |
  // |<iframe sandbox allow="default-self"> |
  // | +-------------+                      |
  // | |(2)Sandboxed |                      |
  // | |No Policy    |                      |
  // | +-------------+                      |
  // +--------------------------------------+
  // Default-self feature should be enabled in child frame with opaque origin,
  // only for that origin, because container policy matches the opaque src.
  // However, it will not pass that on to any other origin
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  url::Origin sandboxed_origin = url::Origin();
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, false, true}}};
  std::unique_ptr<PermissionsPolicy> policy2 = CreateFromParentWithFramePolicy(
      policy1.get(), frame_policy, sandboxed_origin);
  EXPECT_TRUE(policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, sandboxed_origin));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                 sandboxed_origin));
}

TEST_F(PermissionsPolicyTest, TestSandboxedFrameFromHeaderPolicy) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |Permissions-Policy: default-self=*    |
  // |                                      |
  // | +-------------+                      |
  // | |(2)Sandboxed |                      |
  // | |No Policy    |                      |
  // | +-------------+                      |
  // +--------------------------------------+
  // Default-self feature should not be enabled in child frame with opaque
  // origin, as it is cross-origin with its parent, and there is no container
  // policy.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}});
  url::Origin sandboxed_origin = url::Origin();
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, false, true}}};
  std::unique_ptr<PermissionsPolicy> policy2 = CreateFromParentWithFramePolicy(
      policy1.get(), frame_policy, sandboxed_origin);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                  sandboxed_origin));
}

TEST_F(PermissionsPolicyTest, TestSandboxedPolicyIsNotInherited) {
  // +----------------------------------------+
  // |(1)Origin A                             |
  // |No Policy                               |
  // |                                        |
  // |<iframe sandbox allow="default-self *"> |
  // | +------------------------------------+ |
  // | |(2)Sandboxed                        | |
  // | |No Policy                           | |
  // | |                                    | |
  // | | +-------------+                    | |
  // | | |(3)Sandboxed |                    | |
  // | | |No Policy    |                    | |
  // | | +-------------+                    | |
  // | +------------------------------------+ |
  // +----------------------------------------+
  // Default-on feature should be enabled in frame 3 with opaque origin, but all
  // other features should be disabled.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  url::Origin sandboxed_origin_1 = url::Origin();
  url::Origin sandboxed_origin_2 = url::Origin();
  ParsedPermissionsPolicy frame_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}};
  std::unique_ptr<PermissionsPolicy> policy2 = CreateFromParentWithFramePolicy(
      policy1.get(), frame_policy, sandboxed_origin_1);
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), sandboxed_origin_2);
  EXPECT_TRUE(policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_TRUE(policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature,
                                                 sandboxed_origin_1));
  EXPECT_TRUE(policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature,
                                                 sandboxed_origin_2));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_FALSE(policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                  sandboxed_origin_1));
  EXPECT_FALSE(policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                  sandboxed_origin_2));
}

TEST_F(PermissionsPolicyTest, TestSandboxedPolicyCanBePropagated) {
  // +--------------------------------------------+
  // |(1)Origin A                                 |
  // |No Policy                                   |
  // |                                            |
  // |<iframe sandbox allow="default-self *">     |
  // | +----------------------------------------+ |
  // | |(2)Sandboxed                            | |
  // | |No Policy                               | |
  // | |                                        | |
  // | |<iframe sandbox allow="default-self *"> | |
  // | | +-------------+                        | |
  // | | |(3)Sandboxed |                        | |
  // | | |No Policy    |                        | |
  // | | +-------------+                        | |
  // | +----------------------------------------+ |
  // +--------------------------------------------+
  // Default-self feature should be enabled in child frame with opaque origin,
  // only for that origin, because container policy matches all origins.
  // However, it will not pass that on to any other origin
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  url::Origin sandboxed_origin_1 = origin_a_.DeriveNewOpaqueOrigin();
  url::Origin sandboxed_origin_2 = sandboxed_origin_1.DeriveNewOpaqueOrigin();
  ParsedPermissionsPolicy frame_policy_1 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, true}}};
  std::unique_ptr<PermissionsPolicy> policy2 = CreateFromParentWithFramePolicy(
      policy1.get(), frame_policy_1, sandboxed_origin_1);
  ParsedPermissionsPolicy frame_policy_2 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, true}}};
  std::unique_ptr<PermissionsPolicy> policy3 = CreateFromParentWithFramePolicy(
      policy2.get(), frame_policy_2, sandboxed_origin_2);
  EXPECT_TRUE(policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_TRUE(policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature,
                                                 sandboxed_origin_2));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                 sandboxed_origin_2));
}

TEST_F(PermissionsPolicyTest, TestUndefinedFeaturesInFramePolicy) {
  // +---------------------------------------------------+
  // |(1)Origin A                                        |
  // |No Policy                                          |
  // |                                                   |
  // |<iframe allow="nosuchfeature; unavailablefeature"> |
  // | +-------------+                                   |
  // | |(2)Origin B  |                                   |
  // | |No Policy    |                                   |
  // | +-------------+                                   |
  // +---------------------------------------------------+
  // A feature which is not in the declared feature list should be ignored if
  // present in a container policy.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedPermissionsPolicy frame_policy = {
      {{mojom::PermissionsPolicyFeature::kNotFound, /*allowed_origins=*/{},
        false, true},
       {kUnavailableFeature, /*allowed_origins=*/{}, false, true}}};
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  EXPECT_FALSE(PolicyContainsInheritedValue(
      policy1.get(), mojom::PermissionsPolicyFeature::kNotFound));
  EXPECT_FALSE(
      PolicyContainsInheritedValue(policy1.get(), kUnavailableFeature));
  EXPECT_FALSE(PolicyContainsInheritedValue(
      policy2.get(), mojom::PermissionsPolicyFeature::kNotFound));
  EXPECT_FALSE(
      PolicyContainsInheritedValue(policy2.get(), kUnavailableFeature));
}

// Tests for proposed algorithm change in
// https://github.com/w3c/webappsec-permissions-policy/pull/499 to construct
// the policy for subresource request when there exists an equivalent and
// enabled opt-in flag for the request.

// A cross-origin subresource request that explicitly sets the browsingTopics
// flag should have the browsing-topics permission as long as it passes
// allowlist check, regardless of the feature's default state.
TEST_F(PermissionsPolicyTest,
       ProposedTestIsBrowsingTopicsFeatureEnabledForSubresourceRequest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(blink::features::kBrowsingTopics);

  network::ResourceRequest request_without_topics_opt_in;

  network::ResourceRequest request_with_topics_opt_in;
  request_with_topics_opt_in.browsing_topics = true;

  {
    // +-------------------------------------------------+
    // |(1)Origin A                                      |
    // |No Policy                                        |
    // |                                                 |
    // | fetch(<Origin B's url>, {browsingTopics: true}) |
    // +-------------------------------------------------+

    std::unique_ptr<PermissionsPolicy> policy =
        CreateFromParentPolicy(nullptr, origin_a_);

    EXPECT_TRUE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_a_,
        request_without_topics_opt_in));
    EXPECT_TRUE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_a_,
        request_with_topics_opt_in));
    EXPECT_FALSE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_b_,
        request_without_topics_opt_in));
    EXPECT_TRUE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_b_,
        request_with_topics_opt_in));
  }

  {
    // +-------------------------------------------------+
    // |(1)Origin A                                      |
    // |Permissions-Policy: browsing-topics=(self)       |
    // |                                                 |
    // | fetch(<Origin B's url>, {browsingTopics: true}) |
    // +-------------------------------------------------+

    std::unique_ptr<PermissionsPolicy> policy =
        CreateFromParentPolicy(nullptr, origin_a_);
    policy->SetHeaderPolicy({{{mojom::PermissionsPolicyFeature::
                                   kBrowsingTopics, /*allowed_origins=*/
                               {blink::OriginWithPossibleWildcards(
                                   origin_a_,
                                   /*has_subdomain_wildcard=*/false)},
                               /*matches_all_origins=*/false,
                               /*matches_opaque_src=*/false}}});

    EXPECT_TRUE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_a_,
        request_without_topics_opt_in));
    EXPECT_TRUE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_a_,
        request_with_topics_opt_in));
    EXPECT_FALSE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_b_,
        request_without_topics_opt_in));
    EXPECT_FALSE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_b_,
        request_with_topics_opt_in));
  }

  {
    // +-------------------------------------------------+
    // |(1)Origin A                                      |
    // |Permissions-Policy: browsing-topics=(none)       |
    // |                                                 |
    // | fetch(<Origin B's url>, {browsingTopics: true}) |
    // +-------------------------------------------------+

    std::unique_ptr<PermissionsPolicy> policy =
        CreateFromParentPolicy(nullptr, origin_a_);
    policy->SetHeaderPolicy({{{mojom::PermissionsPolicyFeature::
                                   kBrowsingTopics, /*allowed_origins=*/
                               {},
                               /*matches_all_origins=*/false,
                               /*matches_opaque_src=*/false}}});

    EXPECT_FALSE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_a_,
        request_without_topics_opt_in));
    EXPECT_FALSE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_a_,
        request_with_topics_opt_in));
    EXPECT_FALSE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_b_,
        request_without_topics_opt_in));
    EXPECT_FALSE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_b_,
        request_with_topics_opt_in));
  }

  {
    // +-------------------------------------------------+
    // |(1)Origin A                                      |
    // |Permissions-Policy: browsing-topics=*            |
    // |                                                 |
    // | fetch(<Origin B's url>, {browsingTopics: true}) |
    // +-------------------------------------------------+

    std::unique_ptr<PermissionsPolicy> policy =
        CreateFromParentPolicy(nullptr, origin_a_);
    policy->SetHeaderPolicy({{{mojom::PermissionsPolicyFeature::
                                   kBrowsingTopics, /*allowed_origins=*/
                               {},
                               /*matches_all_origins=*/true,
                               /*matches_opaque_src=*/false}}});

    EXPECT_TRUE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_a_,
        request_without_topics_opt_in));
    EXPECT_TRUE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_a_,
        request_with_topics_opt_in));
    EXPECT_TRUE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_b_,
        request_without_topics_opt_in));
    EXPECT_TRUE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_b_,
        request_with_topics_opt_in));
  }

  {
    // +-------------------------------------------------+
    // |(1)Origin A                                      |
    // |Permissions-Policy: browsing-topics=(Origin B)   |
    // |                                                 |
    // | fetch(<Origin B's url>, {browsingTopics: true}) |
    // | fetch(<Origin C's url>, {browsingTopics: true}) |
    // +-------------------------------------------------+

    std::unique_ptr<PermissionsPolicy> policy =
        CreateFromParentPolicy(nullptr, origin_a_);
    policy->SetHeaderPolicy({{{mojom::PermissionsPolicyFeature::
                                   kBrowsingTopics, /*allowed_origins=*/
                               {blink::OriginWithPossibleWildcards(
                                   origin_b_,
                                   /*has_subdomain_wildcard=*/false)},
                               /*matches_all_origins=*/false,
                               /*matches_opaque_src=*/false}}});

    EXPECT_FALSE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_a_,
        request_without_topics_opt_in));
    EXPECT_FALSE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_a_,
        request_with_topics_opt_in));
    EXPECT_TRUE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_b_,
        request_without_topics_opt_in));
    EXPECT_TRUE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_b_,
        request_with_topics_opt_in));
    EXPECT_FALSE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_c_,
        request_without_topics_opt_in));
    EXPECT_FALSE(policy->IsFeatureEnabledForSubresourceRequest(
        mojom::PermissionsPolicyFeature::kBrowsingTopics, origin_c_,
        request_with_topics_opt_in));
  }
}

// Tests for proposed algorithm change. These tests construct policies in
// various embedding scenarios, and verify that the proposed value for "should
// feature be allowed in the child frame" matches what we expect. The points
// where this differs from the current feature policy algorithm are called
// out specifically. See https://crbug.com/937131 for additional context.

TEST_F(PermissionsPolicyTest, ProposedTestImplicitPolicy) {
  // +-----------------+
  // |(1)Origin A      |
  // |No Policy        |
  // | +-------------+ |
  // | |(2)Origin A  | |
  // | |No Policy    | |
  // | +-------------+ |
  // | +-------------+ |
  // | |(3)Origin B  | |
  // | |No Policy    | |
  // | +-------------+ |
  // +-----------------+
  // With no policy specified at all, Default-on and Default-self features
  // should be enabled at the top-level, and in a same-origin child frame.
  // Default-self features should be disabled in a cross-origin child frame.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));

  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, ProposedTestCompletelyBlockedPolicy) {
  // +------------------------------------+
  // |(1)Origin A                         |
  // |Permissions-Policy: default-self=() |
  // | +--------------+  +--------------+ |
  // | |(2)Origin A   |  |(3)Origin B   | |
  // | |No Policy     |  |No Policy     | |
  // | +--------------+  +--------------+ |
  // | <allow="default-self *">           |
  // | +--------------+                   |
  // | |(4)Origin B   |                   |
  // | |No Policy     |                   |
  // | +--------------+                   |
  // | <allow="default-self OriginB">     |
  // | +--------------+                   |
  // | |(5)Origin B   |                   |
  // | |No Policy     |                   |
  // | +--------------+                   |
  // | <allow="default-self OriginB">     |
  // | +--------------+                   |
  // | |(6)Origin C   |                   |
  // | |No Policy     |                   |
  // | +--------------+                   |
  // +------------------------------------+
  // When a feature is disabled in the parent frame, it should be disabled in
  // all child frames, regardless of any declared frame policies.
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/{}, false, false}}});
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedPermissionsPolicy frame_policy4 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}};
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy4, origin_b_);
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedPermissionsPolicy frame_policy5 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy5 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy5, origin_b_);
  EXPECT_FALSE(policy5->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedPermissionsPolicy frame_policy6 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_c_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy6 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy6, origin_b_);
  EXPECT_FALSE(policy6->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, ProposedTestDisallowedCrossOriginChildPolicy) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |Permissions-Policy: default-self=self |
  // | +--------------+  +--------------+   |
  // | |(2)Origin A   |  |(3)Origin B   |   |
  // | |No Policy     |  |No Policy     |   |
  // | +--------------+  +--------------+   |
  // | <allow="default-self *">             |
  // | +--------------+                     |
  // | |(4)Origin B   |                     |
  // | |No Policy     |                     |
  // | +--------------+                     |
  // | <allow="default-self OriginB">       |
  // | +--------------+                     |
  // | |(5)Origin B   |                     |
  // | |No Policy     |                     |
  // | +--------------+                     |
  // | <allow="default-self OriginB">       |
  // | +--------------+                     |
  // | |(6)Origin C   |                     |
  // | |No Policy     |                     |
  // | +--------------+                     |
  // +--------------------------------------+
  // When a feature is not explicitly enabled for an origin, it should be
  // disabled in any frame at that origin, regardless of the declared frame
  // policy. (This is different from the current algorithm, in the case where
  // the frame policy declares that the feature should be allowed.)
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});

  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));

  // This is a critical change from the existing semantics.
  ParsedPermissionsPolicy frame_policy4 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}};
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy4, origin_b_);
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));

  // This is a critical change from the existing semantics.
  ParsedPermissionsPolicy frame_policy5 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy5 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy5, origin_b_);
  EXPECT_FALSE(policy5->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedPermissionsPolicy frame_policy6 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_c_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy6 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy6, origin_b_);
  EXPECT_FALSE(policy6->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, ProposedTestAllowedCrossOriginChildPolicy) {
  // +---------------------------------------------------+
  // |(1)Origin A                                        |
  // |Permissions-Policy: default-self=(self "OriginB")  |
  // | +--------------+  +--------------+                |
  // | |(2)Origin A   |  |(3)Origin B   |                |
  // | |No Policy     |  |No Policy     |                |
  // | +--------------+  +--------------+                |
  // | <allow="default-self *">                          |
  // | +--------------+                                  |
  // | |(4)Origin B   |                                  |
  // | |No Policy     |                                  |
  // | +--------------+                                  |
  // | <allow="default-self OriginB">                    |
  // | +--------------+                                  |
  // | |(5)Origin B   |                                  |
  // | |No Policy     |                                  |
  // | +--------------+                                  |
  // | <allow="default-self OriginB">                    |
  // | +--------------+                                  |
  // | |(6)Origin C   |                                  |
  // | |No Policy     |                                  |
  // | +--------------+                                  |
  // +---------------------------------------------------+
  // When a feature is explicitly enabled for an origin by the header in the
  // parent document, it still requires that the frame policy also grant it to
  // that frame in order to be enabled in the child. (This is different from the
  // current algorithm, in the case where the frame policy does not mention the
  // feature explicitly.)
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false),
          blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});

  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  // This is a critical change from the existing semantics.
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedPermissionsPolicy frame_policy4 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}};
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy4, origin_b_);
  EXPECT_TRUE(policy4->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedPermissionsPolicy frame_policy5 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy5 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy5, origin_b_);
  EXPECT_TRUE(policy5->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedPermissionsPolicy frame_policy6 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_c_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy6 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy6, origin_b_);
  EXPECT_FALSE(policy6->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, ProposedTestAllAllowedCrossOriginChildPolicy) {
  // +------------------------------------+
  // |(1)Origin A                         |
  // |Permissions-Policy: default-self=*  |
  // | +--------------+  +--------------+ |
  // | |(2)Origin A   |  |(3)Origin B   | |
  // | |No Policy     |  |No Policy     | |
  // | +--------------+  +--------------+ |
  // | <allow="default-self *">           |
  // | +--------------+                   |
  // | |(4)Origin B   |                   |
  // | |No Policy     |                   |
  // | +--------------+                   |
  // | <allow="default-self OriginB">     |
  // | +--------------+                   |
  // | |(5)Origin B   |                   |
  // | |No Policy     |                   |
  // | +--------------+                   |
  // | <allow="default-self OriginB">     |
  // | +--------------+                   |
  // | |(6)Origin C   |                   |
  // | |No Policy     |                   |
  // | +--------------+                   |
  // +------------------------------------+
  // When a feature is explicitly enabled for all origins by the header in the
  // parent document, it still requires that the frame policy also grant it to
  // that frame in order to be enabled in the child. (This is different from the
  // current algorithm, in the case where the frame policy does not mention the
  // feature explicitly.)
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}});

  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  // This is a critical change from the existing semantics.
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedPermissionsPolicy frame_policy4 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}};
  std::unique_ptr<PermissionsPolicy> policy4 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy4, origin_b_);
  EXPECT_TRUE(policy4->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedPermissionsPolicy frame_policy5 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy5 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy5, origin_b_);
  EXPECT_TRUE(policy5->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedPermissionsPolicy frame_policy6 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_c_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  std::unique_ptr<PermissionsPolicy> policy6 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy6, origin_b_);
  EXPECT_FALSE(policy6->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, ProposedTestNestedPolicyPropagates) {
  // +-------------------------------------------------+
  // |(1)Origin A                                      |
  // |Permissions-Policy: default-self=(self "OriginB")|
  // | +--------------------------------+              |
  // | |(2)Origin B                     |              |
  // | |No Policy                       |              |
  // | | <allow="default-self *">       |              |
  // | | +--------------+               |              |
  // | | |(3)Origin B   |               |              |
  // | | |No Policy     |               |              |
  // | | +--------------+               |              |
  // | +--------------------------------+              |
  // +-------------------------------------------------+
  // Ensures that a proposed policy change will propagate down the frame tree.
  // This is important so that we can tell when a change has happened, even if
  // the feature is tested in a different one than where the
  std::unique_ptr<PermissionsPolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false),
          blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});

  // This is where the change first occurs.
  std::unique_ptr<PermissionsPolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  // The proposed value in frame 2 should affect the proposed value in frame 3.
  ParsedPermissionsPolicy frame_policy3 = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, true, false}}};
  std::unique_ptr<PermissionsPolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy3, origin_b_);
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, CreateForDefaultFencedFrame) {
  std::unique_ptr<PermissionsPolicy> policy =
      CreateForFencedFrame(origin_a_, blink::mojom::FencedFrameMode::kDefault);
  EXPECT_FALSE(policy->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy->IsFeatureEnabled(
      mojom::PermissionsPolicyFeature::kAttributionReporting));
  EXPECT_FALSE(policy->IsFeatureEnabled(
      mojom::PermissionsPolicyFeature::kSharedStorage));
}

TEST_F(PermissionsPolicyTest, CreateForOpaqueFencedFrame) {
  std::unique_ptr<PermissionsPolicy> policy = CreateForFencedFrame(
      origin_a_, blink::mojom::FencedFrameMode::kOpaqueAds);
  EXPECT_FALSE(policy->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy->IsFeatureEnabled(
      mojom::PermissionsPolicyFeature::kAttributionReporting));
  EXPECT_TRUE(policy->IsFeatureEnabled(
      mojom::PermissionsPolicyFeature::kSharedStorage));
}

TEST_F(PermissionsPolicyTest, CreateFromParsedPolicy) {
  ParsedPermissionsPolicy parsed_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_a_,
                                            /*has_subdomain_wildcard=*/false),
         blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  auto policy = CreateFromParsedPolicy(parsed_policy, origin_a_);
  EXPECT_TRUE(
      policy->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
}

TEST_F(PermissionsPolicyTest, CreateFromParsedPolicyExcludingSelf) {
  ParsedPermissionsPolicy parsed_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/
        {blink::OriginWithPossibleWildcards(origin_b_,
                                            /*has_subdomain_wildcard=*/false)},
        false,
        false}}};
  auto policy = CreateFromParsedPolicy(parsed_policy, origin_a_);
  EXPECT_FALSE(
      policy->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_FALSE(
      policy->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
}

TEST_F(PermissionsPolicyTest, CreateFromParsedPolicyWithEmptyAllowlist) {
  ParsedPermissionsPolicy parsed_policy = {
      {{kDefaultSelfFeature, /*allowed_origins=*/{}, false, false}}};
  auto policy = CreateFromParsedPolicy(parsed_policy, origin_a_);
  EXPECT_FALSE(policy->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(PermissionsPolicyTest, SetHeaderPolicy) {
  // We can construct a policy, set headers, and then check.
  auto policy1 = CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));

  // We can't construct a policy, check, then set headers.
  auto policy2 = CreateFromParentPolicy(nullptr, origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_DCHECK_DEATH(policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}}));

  // We can't construct a policy, set headers, then set the header.
  auto policy3 = CreateFromParentPolicy(nullptr, origin_a_);
  policy3->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  EXPECT_DCHECK_DEATH(policy3->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}}));

  // We can't construct a policy, then set headers for kNotFound.
  auto policy4 = CreateFromParentPolicy(nullptr, origin_a_);
  EXPECT_DCHECK_DEATH(policy4->SetHeaderPolicy(
      {{{mojom::PermissionsPolicyFeature::kNotFound, {}, false, false}}}));
}

TEST_F(PermissionsPolicyTest, OverwriteHeaderPolicyForClientHints) {
  // We can construct a policy, set/overwrite the same header, and then check.
  auto policy1 = CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{mojom::PermissionsPolicyFeature::kClientHintDPR,
         /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  policy1->OverwriteHeaderPolicyForClientHints(
      {{{mojom::PermissionsPolicyFeature::kClientHintDPR,
         /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  EXPECT_TRUE(policy1->IsFeatureEnabled(
      mojom::PermissionsPolicyFeature::kClientHintDPR));

  // If we overwrite an enabled header with a disabled header it's now disabled.
  auto policy2 = CreateFromParentPolicy(nullptr, origin_a_);
  policy2->SetHeaderPolicy(
      {{{mojom::PermissionsPolicyFeature::kClientHintDPR,
         /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  policy2->OverwriteHeaderPolicyForClientHints(
      {{{mojom::PermissionsPolicyFeature::kClientHintDPR,
         /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  EXPECT_FALSE(policy2->IsFeatureEnabled(
      mojom::PermissionsPolicyFeature::kClientHintDPR));

  // We can construct a policy, set/overwrite different headers, and then check.
  auto policy3 = CreateFromParentPolicy(nullptr, origin_a_);
  policy3->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_b_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  policy3->OverwriteHeaderPolicyForClientHints(
      {{{mojom::PermissionsPolicyFeature::kClientHintDPR,
         /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  EXPECT_TRUE(policy3->IsFeatureEnabled(
      mojom::PermissionsPolicyFeature::kClientHintDPR));

  // We can't overwrite a non-client-hint header.
  auto policy4 = CreateFromParentPolicy(nullptr, origin_a_);
  EXPECT_DCHECK_DEATH(policy4->OverwriteHeaderPolicyForClientHints(
      {{{kDefaultSelfFeature, /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}}));

  // We can't construct a policy, set headers, check, then overwrite the header.
  auto policy5 = CreateFromParentPolicy(nullptr, origin_a_);
  policy5->SetHeaderPolicy(
      {{{mojom::PermissionsPolicyFeature::kClientHintDPR,
         /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}});
  EXPECT_TRUE(policy5->IsFeatureEnabled(
      mojom::PermissionsPolicyFeature::kClientHintDPR));
  EXPECT_DCHECK_DEATH(policy5->OverwriteHeaderPolicyForClientHints(
      {{{mojom::PermissionsPolicyFeature::kClientHintDPR,
         /*allowed_origins=*/
         {blink::OriginWithPossibleWildcards(origin_a_,
                                             /*has_subdomain_wildcard=*/false)},
         false,
         false}}}));
}

TEST_F(PermissionsPolicyTest, GetAllowlistForFeatureIfExists) {
  // If we set a policy, then we can extract it.
  auto policy1 = CreateFromParentPolicy(nullptr, origin_a_);
  const std::vector<blink::OriginWithPossibleWildcards> origins1(
      {blink::OriginWithPossibleWildcards(origin_b_,
                                          /*has_subdomain_wildcard=*/false)});
  policy1->SetHeaderPolicy({{{mojom::PermissionsPolicyFeature::kClientHintDPR,
                              origins1, false, false}}});
  const auto& maybe_allow_list1 = policy1->GetAllowlistForFeatureIfExists(
      mojom::PermissionsPolicyFeature::kClientHintDPR);
  EXPECT_TRUE(maybe_allow_list1.has_value());
  EXPECT_FALSE(maybe_allow_list1.value().MatchesAll());
  EXPECT_FALSE(maybe_allow_list1.value().MatchesOpaqueSrc());
  EXPECT_THAT(maybe_allow_list1.value().AllowedOrigins(),
              testing::ContainerEq(origins1));

  // If we don't set a policy, then we can't extract it.
  auto policy2 = CreateFromParentPolicy(nullptr, origin_a_);
  const auto& maybe_allow_list2 = policy2->GetAllowlistForFeatureIfExists(
      mojom::PermissionsPolicyFeature::kClientHintDPR);
  EXPECT_FALSE(maybe_allow_list2.has_value());

  // If we set a policy, then overwrite it, we can extract it.
  auto policy3 = CreateFromParentPolicy(nullptr, origin_a_);
  const std::vector<blink::OriginWithPossibleWildcards> origins3(
      {blink::OriginWithPossibleWildcards(origin_a_,
                                          /*has_subdomain_wildcard=*/false)});
  policy3->SetHeaderPolicy(
      {{{mojom::PermissionsPolicyFeature::kClientHintDPR, {}, false, false}}});
  policy3->OverwriteHeaderPolicyForClientHints(
      {{{mojom::PermissionsPolicyFeature::kClientHintDPR, origins3, false,
         false}}});
  const auto& maybe_allow_list3 = policy3->GetAllowlistForFeatureIfExists(
      mojom::PermissionsPolicyFeature::kClientHintDPR);
  EXPECT_TRUE(maybe_allow_list3.has_value());
  EXPECT_FALSE(maybe_allow_list3.value().MatchesAll());
  EXPECT_FALSE(maybe_allow_list3.value().MatchesOpaqueSrc());
  EXPECT_THAT(maybe_allow_list3.value().AllowedOrigins(),
              testing::ContainerEq(origins3));

  // If we don't set a policy, then overwrite it, we can extract it.
  auto policy4 = CreateFromParentPolicy(nullptr, origin_a_);
  const std::vector<blink::OriginWithPossibleWildcards> origins4(
      {blink::OriginWithPossibleWildcards(origin_a_,
                                          /*has_subdomain_wildcard=*/false),
       blink::OriginWithPossibleWildcards(origin_b_,
                                          /*has_subdomain_wildcard=*/false)});
  policy4->OverwriteHeaderPolicyForClientHints(
      {{{mojom::PermissionsPolicyFeature::kClientHintDPR, origins4, false,
         false}}});
  const auto& maybe_allow_list4 = policy4->GetAllowlistForFeatureIfExists(
      mojom::PermissionsPolicyFeature::kClientHintDPR);
  EXPECT_TRUE(maybe_allow_list4.has_value());
  EXPECT_FALSE(maybe_allow_list4.value().MatchesAll());
  EXPECT_FALSE(maybe_allow_list4.value().MatchesOpaqueSrc());
  EXPECT_THAT(maybe_allow_list4.value().AllowedOrigins(),
              testing::ContainerEq(origins4));
}

}  // namespace blink
