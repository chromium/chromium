// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/feature_policy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/feature_policy/policy_value.mojom.h"
#include "url/gurl.h"

namespace blink {

namespace {

mojom::FeaturePolicyFeature kDefaultOnFeature =
    static_cast<mojom::FeaturePolicyFeature>(
        static_cast<int>(mojom::FeaturePolicyFeature::kMaxValue) + 1);

mojom::FeaturePolicyFeature kDefaultSelfFeature =
    static_cast<mojom::FeaturePolicyFeature>(
        static_cast<int>(mojom::FeaturePolicyFeature::kMaxValue) + 2);

// This feature is defined in code, but not present in the feature list.
mojom::FeaturePolicyFeature kUnavailableFeature =
    static_cast<mojom::FeaturePolicyFeature>(
        static_cast<int>(mojom::FeaturePolicyFeature::kMaxValue) + 4);

}  // namespace

class FeaturePolicyTest : public testing::Test {
 protected:
  FeaturePolicyTest()
      : feature_list_({{kDefaultOnFeature,
                        FeaturePolicyFeatureDefault(
                            FeaturePolicyFeatureDefault::EnableForAll)},
                       {kDefaultSelfFeature,
                        FeaturePolicyFeatureDefault(
                            FeaturePolicyFeatureDefault::EnableForSelf)}}) {}

  ~FeaturePolicyTest() override = default;

  std::unique_ptr<FeaturePolicy> CreateFromParentPolicy(
      const FeaturePolicy* parent,
      const url::Origin& origin) {
    ParsedFeaturePolicy empty_container_policy;
    return FeaturePolicy::CreateFromParentPolicy(parent, empty_container_policy,
                                                 origin, feature_list_);
  }

  std::unique_ptr<FeaturePolicy> CreateFromParentWithFramePolicy(
      const FeaturePolicy* parent,
      const ParsedFeaturePolicy& frame_policy,
      const url::Origin& origin) {
    return FeaturePolicy::CreateFromParentPolicy(parent, frame_policy, origin,
                                                 feature_list_);
  }

  bool PolicyContainsInheritedValue(const FeaturePolicy* policy,
                                    mojom::FeaturePolicyFeature feature) {
    return policy->inherited_policies_.find(feature) !=
           policy->inherited_policies_.end();
  }

  url::Origin origin_a_ = url::Origin::Create(GURL("https://example.com/"));
  url::Origin origin_b_ = url::Origin::Create(GURL("https://example.net/"));
  url::Origin origin_c_ = url::Origin::Create(GURL("https://example.org/"));

 private:
  // Contains the list of controlled features, so that we are guaranteed to
  // have at least one of each kind of default behaviour represented.
  FeaturePolicyFeatureList feature_list_;
};

TEST_F(FeaturePolicyTest, TestInitialPolicy) {
  // +-------------+
  // |(1)Origin A  |
  // |No Policy    |
  // +-------------+
  // Default-on and top-level-only features should be enabled in top-level
  // frame.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestInitialSameOriginChildPolicy) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestInitialCrossOriginChildPolicy) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestCrossOriginChildCannotEnableFeature) {
  // +----------------------------------------+
  // |(1) Origin A                            |
  // |No Policy                               |
  // | +------------------------------------+ |
  // | |(2) Origin B                        | |
  // | |Feature-Policy: default-self 'self' | |
  // | +------------------------------------+ |
  // +----------------------------------------+
  // Default-self feature should be disabled in cross origin frame, even if no
  // policy was specified in the parent frame.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false,
         false}}});
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestFrameSelfInheritance) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Feature-Policy: default-self 'self'       |
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_a_}, false,
         false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy5 =
      CreateFromParentPolicy(policy4.get(), origin_b_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy5->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestReflexiveFrameSelfInheritance) {
  // +------------------------------------+
  // |(1) Origin A                        |
  // |Feature-Policy: default-self 'self' |
  // | +-----------------+                |
  // | |(2) Origin B     |                |
  // | |No Policy        |                |
  // | | +-------------+ |                |
  // | | |(3)Origin A  | |                |
  // | | |No Policy    | |                |
  // | | +-------------+ |                |
  // | +-----------------+                |
  // +------------------------------------+
  // Feature which is enabled at top-level should be disabled in frame 3, as
  // it is embedded by frame 2, for which the feature is not enabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_a_}, false,
         false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestSelectiveFrameInheritance) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Feature-Policy: default-self OriginB      |
  // | +-----------------+  +-----------------+ |
  // | |(2) Origin B     |  |(3) Origin C     | |
  // | |No Policy        |  |No Policy        | |
  // | |                 |  | +-------------+ | |
  // | |                 |  | |(4)Origin B  | | |
  // | |                 |  | |No Policy    | | |
  // | |                 |  | +-------------+ | |
  // | +-----------------+  +-----------------+ |
  // +------------------------------------------+
  // Feature should be only be enabled in the top-level frame, even though the
  // header indicates Origin B, there is no container policy to explicitly
  // delegate to that origin, in either frame 2 or 4.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false,
         false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_c_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy3.get(), origin_b_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestSelectiveFrameInheritance2) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Feature-Policy: default-self OriginB      |
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false,
         false}}});
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_c_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy3.get(), origin_b_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestPolicyCanBlockSelf) {
  // +----------------------------------+
  // |(1)Origin A                       |
  // |Feature-Policy: default-on 'none' |
  // +----------------------------------+
  // Default-on feature should be disabled in top-level frame.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, /* allowed_origins */ {}, false, false}}});
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestParentPolicyBlocksSameOriginChildPolicy) {
  // +----------------------------------+
  // |(1)Origin A                       |
  // |Feature-Policy: default-on 'none' |
  // | +-------------+                  |
  // | |(2)Origin A  |                  |
  // | |No Policy    |                  |
  // | +-------------+                  |
  // +----------------------------------+
  // Feature should be disabled in child frame.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, /* allowed_origins */ {}, false, false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestChildPolicyCanBlockSelf) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |No Policy                             |
  // | +----------------------------------+ |
  // | |(2)Origin B                       | |
  // | |Feature-Policy: default-on 'none' | |
  // | +----------------------------------+ |
  // +--------------------------------------+
  // Default-on feature should be disabled by cross-origin child frame.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultOnFeature, /* allowed_origins */ {}, false, false}}});
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestChildPolicyCanBlockChildren) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |No Policy                             |
  // | +----------------------------------+ |
  // | |(2)Origin B                       | |
  // | |Feature-Policy: default-on 'self' | |
  // | | +-------------+                  | |
  // | | |(3)Origin C  |                  | |
  // | | |No Policy    |                  | |
  // | | +-------------+                  | |
  // | +----------------------------------+ |
  // +--------------------------------------+
  // Default-on feature should be enabled in frames 1 and 2; disabled in frame
  // 3 by child frame policy.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultOnFeature, /* allowed_origins */ {origin_b_}, false, false}}});
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestParentPolicyBlocksCrossOriginChildPolicy) {
  // +----------------------------------+
  // |(1)Origin A                       |
  // |Feature-Policy: default-on 'none' |
  // | +-------------+                  |
  // | |(2)Origin B  |                  |
  // | |No Policy    |                  |
  // | +-------------+                  |
  // +----------------------------------+
  // Default-on feature should be disabled in cross-origin child frame.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, /* allowed_origins */ {}, false, false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestEnableForAllOrigins) {
  // +--------------------------------+
  // |(1) Origin A                    |
  // |Feature-Policy: default-self *  |
  // | +-----------------+            |
  // | |(2) Origin B     |            |
  // | |No Policy        |            |
  // | | +-------------+ |            |
  // | | |(3)Origin A  | |            |
  // | | |No Policy    | |            |
  // | | +-------------+ |            |
  // | +-----------------+            |
  // +--------------------------------+
  // Feature should be enabled in top and second level; disabled in frame 3.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestEnableForAllOriginsAndDelegate) {
  // +--------------------------------------+
  // |(1) Origin A                          |
  // |Feature-Policy: default-self *        |
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}});
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultOnStillNeedsSelf) {
  // +---------------------------------------+
  // |(1) Origin A                           |
  // |Feature-Policy: default-on OriginB     |
  // | +-----------------------------------+ |
  // | |(2) Origin B                       | |
  // | |No Policy                          | |
  // | | +-------------+   +-------------+ | |
  // | | |(3)Origin B  |   |(4)Origin C  | | |
  // | | |No Policy    |   |No Policy    | | |
  // | | +-------------+   +-------------+ | |
  // | +-----------------------------------+ |
  // +---------------------------------------+
  // Feature should be disabled in all frames.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, /* allowed_origins */ {origin_b_}, false, false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultOnEnablesForAllDescendants) {
  // +----------------------------------------+
  // |(1) Origin A                            |
  // |Feature-Policy: default-on self OriginB |
  // | +-----------------------------------+  |
  // | |(2) Origin B                       |  |
  // | |No Policy                          |  |
  // | | +-------------+   +-------------+ |  |
  // | | |(3)Origin B  |   |(4)Origin C  | |  |
  // | | |No Policy    |   |No Policy    | |  |
  // | | +-------------+   +-------------+ |  |
  // | +-----------------------------------+  |
  // +----------------------------------------+
  // Feature should be enabled in all frames.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultOnFeature, /* allowed_origins */ {origin_a_, origin_b_}, false,
         false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy4->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultSelfRequiresDelegation) {
  // +---------------------------------------+
  // |(1) Origin A                           |
  // |Feature-Policy: default-self OriginB   |
  // | +-----------------------------------+ |
  // | |(2) Origin B                       | |
  // | |No Policy                          | |
  // | | +-------------+   +-------------+ | |
  // | | |(3)Origin B  |   |(4)Origin C  | | |
  // | | |No Policy    |   |No Policy    | | |
  // | | +-------------+   +-------------+ | |
  // | +-----------------------------------+ |
  // +---------------------------------------+
  // Feature should be disabled in all frames.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false,
         false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_FALSE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultSelfRespectsSameOriginEmbedding) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Feature-Policy: default-self self OriginB |
  // |<iframe allow="default-self">             |
  // | +-----------------------------------+    |
  // | |(2) Origin B                       |    |
  // | |No Policy                          |    |
  // | | +-------------+   +-------------+ |    |
  // | | |(3)Origin B  |   |(4)Origin C  | |    |
  // | | |No Policy    |   |No Policy    | |    |
  // | | +-------------+   +-------------+ |    |
  // | +-----------------------------------+    |
  // +------------------------------------------+
  // Feature should be disabled in frame 4; enabled in frames 1, 2 and 3.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_a_, origin_b_},
         false, false}}});
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestDelegationRequiredAtAllLevels) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}});
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, true}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestBlockedFrameCannotReenable) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |Feature-Policy: default-self 'self'   |
  // | +----------------------------------+ |
  // | |(2)Origin B                       | |
  // | |Feature-Policy: default-self *    | |
  // | | +-------------+  +-------------+ | |
  // | | |(3)Origin A  |  |(4)Origin C  | | |
  // | | |No Policy    |  |No Policy    | | |
  // | | +-------------+  +-------------+ | |
  // | +----------------------------------+ |
  // +--------------------------------------+
  // Feature should be enabled at the top level; disabled in all other frames.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_a_}, false,
         false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}});
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_a_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestEnabledFrameCanDelegate) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  ParsedFeaturePolicy frame_policy2 = {
      {{kDefaultSelfFeature, {origin_c_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy2, origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestEnabledFrameCanDelegateByDefault) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Feature-Policy: default-on 'self' OriginB      |
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{
      {kDefaultOnFeature, /* allowed_origins */ {origin_a_, origin_b_}, false,
       false},
  }});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy1.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestFeaturesDontDelegateByDefault) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Feature-Policy: default-self 'self' OriginB    |
  // | +--------------------+ +--------------------+ |
  // | |(2) Origin B        | | (4) Origin C       | |
  // | |No Policy           | | No Policy          | |
  // | | +-------------+    | |                    | |
  // | | |(3)Origin C  |    | |                    | |
  // | | |No Policy    |    | |                    | |
  // | | +-------------+    | |                    | |
  // | +--------------------+ +--------------------+ |
  // +-----------------------------------------------+
  // Feature should be enabled in frames 1 only. Without a container policy, the
  // feature is not delegated to any child frames.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_a_, origin_b_},
         false, false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy1.get(), origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestFeaturesAreIndependent) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_a_, origin_b_},
         false, false},
        {kDefaultOnFeature, /* allowed_origins */ {origin_a_}, false, false}}});
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_a_, origin_b_},
        false, false},
       {kDefaultOnFeature, /* allowed_origins */ {origin_a_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  ParsedFeaturePolicy frame_policy2 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_b_, origin_c_},
        false, false},
       {kDefaultOnFeature, /* allowed_origins */ {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy2, origin_c_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultOnFeature));
}

// Test frame policies

TEST_F(FeaturePolicyTest, TestSimpleFramePolicy) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
}

TEST_F(FeaturePolicyTest, TestAllOriginFramePolicy) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
}

TEST_F(FeaturePolicyTest, TestFramePolicyCanBeFurtherDelegated) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicy frame_policy1 = {{
      {kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false},
  }};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  ParsedFeaturePolicy frame_policy2 = {{
      {kDefaultSelfFeature, /* allowed_origins */ {origin_c_}, false, false},
  }};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy2, origin_c_);
  std::unique_ptr<FeaturePolicy> policy4 =
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

TEST_F(FeaturePolicyTest, TestDefaultOnCanBeDisabledByFramePolicy) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicy frame_policy1 = {
      {{kDefaultOnFeature, /* allowed_origins */ {}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_a_);
  ParsedFeaturePolicy frame_policy2 = {
      {{kDefaultOnFeature, /* allowed_origins */ {}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy3 =
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

TEST_F(FeaturePolicyTest, TestFramePolicyModifiesHeaderPolicy) {
  // +---------------------------------------------+
  // |(1)Origin A                                  |
  // |Feature-Policy: default-self 'self' OriginB  |
  // |                                             |
  // |<iframe allow="default-self 'none'">         |
  // | +-----------------------------------------+ |
  // | |(2)Origin B                              | |
  // | |No Policy                                | |
  // | +-----------------------------------------+ |
  // |                                             |
  // |<iframe allow="default-self 'none'">         |
  // | +-----------------------------------------+ |
  // | |(3)Origin B                              | |
  // | |Feature-Policy: default-self 'self'      | |
  // | +-----------------------------------------+ |
  // +---------------------------------------------+
  // Default-self feature should be disabled in both cross-origin child frames
  // by frame policy, even though the parent frame's header policy would
  // otherwise enable it. This is true regardless of the child frame's header
  // policy.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{
      {kDefaultSelfFeature, /* allowed_origins */ {origin_a_, origin_b_}, false,
       false},
  }});
  ParsedFeaturePolicy frame_policy1 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  ParsedFeaturePolicy frame_policy2 = {{
      {kDefaultSelfFeature, /* allowed_origins */ {}, false, false},
  }};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy2, origin_b_);
  policy3->SetHeaderPolicy({{
      {kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false},
  }});
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
}

TEST_F(FeaturePolicyTest, TestCombineFrameAndHeaderPolicies) {
  // +-----------------------------------------+
  // |(1)Origin A                              |
  // |No Policy                                |
  // |                                         |
  // |<iframe allow="default-self OriginB">    |
  // | +-------------------------------------+ |
  // | |(2)Origin B                          | |
  // | |Feature-Policy: default-self *       | |
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicy frame_policy1 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  policy2->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}});
  ParsedFeaturePolicy frame_policy2 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy2, origin_c_);
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentPolicy(policy2.get(), origin_c_);
  EXPECT_TRUE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_c_));
}

TEST_F(FeaturePolicyTest, TestFeatureDeclinedAtTopLevel) {
  // +-----------------------------------------+
  // |(1)Origin A                              |
  // |Feature-Policy: default-self 'none'      |
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy({{
      {kDefaultSelfFeature, /* allowed_origins */ {}, false, false},
  }});
  ParsedFeaturePolicy frame_policy1 = {{
      {kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false},
  }};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  ParsedFeaturePolicy frame_policy2 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy2, origin_a_);
  EXPECT_FALSE(
      policy1->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_b_));
  EXPECT_FALSE(
      policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
}

TEST_F(FeaturePolicyTest, TestFeatureDelegatedAndAllowed) {
  // +--------------------------------------------+
  // |(1)Origin A                                 |
  // |Feature-Policy: default-self 'self' OriginB |
  // |                                            |
  // |<iframe allow="default-self OriginA">       |
  // | +-------------------------------------+    |
  // | |(2)Origin B                          |    |
  // | |No Policy                            |    |
  // | +-------------------------------------+    |
  // |                                            |
  // |<iframe allow="default-self OriginB">       |
  // | +-------------------------------------+    |
  // | |(3)Origin B                          |    |
  // | |No Policy                            |    |
  // | +-------------------------------------+    |
  // |                                            |
  // |<iframe allow="default-self *">             |
  // | +-------------------------------------+    |
  // | |(4)Origin B                          |    |
  // | |No Policy                            |    |
  // | +-------------------------------------+    |
  // +--------------------------------------------+
  // Default-self feature should be disabled in frame 2, as the origin does not
  // match, and enabled in the remaining frames.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_a_, origin_b_},
         false, false}}});
  ParsedFeaturePolicy frame_policy1 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_a_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy1, origin_b_);
  ParsedFeaturePolicy frame_policy2 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy2, origin_b_);
  ParsedFeaturePolicy frame_policy3 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}};
  std::unique_ptr<FeaturePolicy> policy4 =
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

TEST_F(FeaturePolicyTest, TestDefaultSandboxedFramePolicy) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  url::Origin sandboxed_origin = url::Origin();
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), sandboxed_origin);
  EXPECT_TRUE(policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, sandboxed_origin));
  EXPECT_FALSE(
      policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature, origin_a_));
  EXPECT_FALSE(policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                  sandboxed_origin));
}

TEST_F(FeaturePolicyTest, TestSandboxedFramePolicyForAllOrigins) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  url::Origin sandboxed_origin = url::Origin();
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, true}}};
  std::unique_ptr<FeaturePolicy> policy2 = CreateFromParentWithFramePolicy(
      policy1.get(), frame_policy, sandboxed_origin);
  EXPECT_TRUE(policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, sandboxed_origin));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                 sandboxed_origin));
}

TEST_F(FeaturePolicyTest, TestSandboxedFramePolicyForOpaqueSrcOrigin) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  url::Origin sandboxed_origin = url::Origin();
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, false, true}}};
  std::unique_ptr<FeaturePolicy> policy2 = CreateFromParentWithFramePolicy(
      policy1.get(), frame_policy, sandboxed_origin);
  EXPECT_TRUE(policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_TRUE(
      policy2->IsFeatureEnabledForOrigin(kDefaultOnFeature, sandboxed_origin));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                 sandboxed_origin));
}

TEST_F(FeaturePolicyTest, TestSandboxedFrameFromHeaderPolicy) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |Feature-Policy: default-self *        |
  // |                                      |
  // | +-------------+                      |
  // | |(2)Sandboxed |                      |
  // | |No Policy    |                      |
  // | +-------------+                      |
  // +--------------------------------------+
  // Default-self feature should not be enabled in child frame with opaque
  // origin, as it is cross-origin with its parent, and there is no container
  // policy.
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}});
  url::Origin sandboxed_origin = url::Origin();
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, false, true}}};
  std::unique_ptr<FeaturePolicy> policy2 = CreateFromParentWithFramePolicy(
      policy1.get(), frame_policy, sandboxed_origin);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                  sandboxed_origin));
}

TEST_F(FeaturePolicyTest, TestSandboxedPolicyIsNotInherited) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  url::Origin sandboxed_origin_1 = url::Origin();
  url::Origin sandboxed_origin_2 = url::Origin();
  ParsedFeaturePolicy frame_policy = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}};
  std::unique_ptr<FeaturePolicy> policy2 = CreateFromParentWithFramePolicy(
      policy1.get(), frame_policy, sandboxed_origin_1);
  std::unique_ptr<FeaturePolicy> policy3 =
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

TEST_F(FeaturePolicyTest, TestSandboxedPolicyCanBePropagated) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  url::Origin sandboxed_origin_1 = origin_a_.DeriveNewOpaqueOrigin();
  url::Origin sandboxed_origin_2 = sandboxed_origin_1.DeriveNewOpaqueOrigin();
  ParsedFeaturePolicy frame_policy_1 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, true}}};
  std::unique_ptr<FeaturePolicy> policy2 = CreateFromParentWithFramePolicy(
      policy1.get(), frame_policy_1, sandboxed_origin_1);
  ParsedFeaturePolicy frame_policy_2 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, true}}};
  std::unique_ptr<FeaturePolicy> policy3 = CreateFromParentWithFramePolicy(
      policy2.get(), frame_policy_2, sandboxed_origin_2);
  EXPECT_TRUE(policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature, origin_a_));
  EXPECT_TRUE(policy3->IsFeatureEnabledForOrigin(kDefaultOnFeature,
                                                 sandboxed_origin_2));
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->IsFeatureEnabledForOrigin(kDefaultSelfFeature,
                                                 sandboxed_origin_2));
}

TEST_F(FeaturePolicyTest, TestUndefinedFeaturesInFramePolicy) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  ParsedFeaturePolicy frame_policy = {
      {{mojom::FeaturePolicyFeature::kNotFound, /* allowed_origins */ {}, false,
        true},
       {kUnavailableFeature, /* allowed_origins */ {}, false, true}}};
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy, origin_b_);
  EXPECT_FALSE(PolicyContainsInheritedValue(
      policy1.get(), mojom::FeaturePolicyFeature::kNotFound));
  EXPECT_FALSE(
      PolicyContainsInheritedValue(policy1.get(), kUnavailableFeature));
  EXPECT_FALSE(PolicyContainsInheritedValue(
      policy2.get(), mojom::FeaturePolicyFeature::kNotFound));
  EXPECT_FALSE(
      PolicyContainsInheritedValue(policy2.get(), kUnavailableFeature));
}

// Tests for proposed algorithm change. These tests construct policies in
// various embedding scenarios, and verify that the proposed value for "should
// feature be allowed in the child frame" matches what we expect. The points
// where this differs from the current feature policy algorithm are called out
// specifically.
// See https://crbug.com/937131 for additional context.

TEST_F(FeaturePolicyTest, ProposedTestImplicitPolicy) {
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy1->IsFeatureEnabled(kDefaultSelfFeature));

  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_TRUE(policy3->IsFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, ProposedTestCompletelyBlockedPolicy) {
  // +------------------------------------+
  // |(1)Origin A                         |
  // |Feature-Policy: default-self 'none' |
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {}, false, false}}});
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedFeaturePolicy frame_policy4 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}};
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy4, origin_b_);
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedFeaturePolicy frame_policy5 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy5 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy5, origin_b_);
  EXPECT_FALSE(policy5->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedFeaturePolicy frame_policy6 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_c_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy6 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy6, origin_b_);
  EXPECT_FALSE(policy6->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, ProposedTestDisallowedCrossOriginChildPolicy) {
  // +------------------------------------+
  // |(1)Origin A                         |
  // |Feature-Policy: default-self 'self' |
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
  // When a feature is not explicitly enabled for an origin, it should be
  // disabled in any frame at that origin, regardless of the declared frame
  // policy. (This is different from the current algorithm, in the case where
  // the frame policy declares that the feature should be allowed.)
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_a_}, false,
         false}}});

  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));

  // This is a critical change from the existing semantics.
  ParsedFeaturePolicy frame_policy4 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}};
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy4, origin_b_);
  EXPECT_FALSE(policy4->IsFeatureEnabled(kDefaultSelfFeature));

  // This is a critical change from the existing semantics.
  ParsedFeaturePolicy frame_policy5 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy5 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy5, origin_b_);
  EXPECT_FALSE(policy5->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedFeaturePolicy frame_policy6 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_c_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy6 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy6, origin_b_);
  EXPECT_FALSE(policy6->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, ProposedTestAllowedCrossOriginChildPolicy) {
  // +-----------------------------------------------+
  // |(1)Origin A                                    |
  // |Feature-Policy: default-self 'self' OriginB;   |
  // | +--------------+  +--------------+            |
  // | |(2)Origin A   |  |(3)Origin B   |            |
  // | |No Policy     |  |No Policy     |            |
  // | +--------------+  +--------------+            |
  // | <allow="default-self *">                      |
  // | +--------------+                              |
  // | |(4)Origin B   |                              |
  // | |No Policy     |                              |
  // | +--------------+                              |
  // | <allow="default-self OriginB">                |
  // | +--------------+                              |
  // | |(5)Origin B   |                              |
  // | |No Policy     |                              |
  // | +--------------+                              |
  // | <allow="default-self OriginB">                |
  // | +--------------+                              |
  // | |(6)Origin C   |                              |
  // | |No Policy     |                              |
  // | +--------------+                              |
  // +-----------------------------------------------+
  // When a feature is explicitly enabled for an origin by the header in the
  // parent document, it still requires that the frame policy also grant it to
  // that frame in order to be enabled in the child. (This is different from the
  // current algorithm, in the case where the frame policy does not mention the
  // feature explicitly.)
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_a_, origin_b_},
         false, false}}});

  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  // This is a critical change from the existing semantics.
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedFeaturePolicy frame_policy4 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}};
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy4, origin_b_);
  EXPECT_TRUE(policy4->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedFeaturePolicy frame_policy5 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy5 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy5, origin_b_);
  EXPECT_TRUE(policy5->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedFeaturePolicy frame_policy6 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_c_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy6 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy6, origin_b_);
  EXPECT_FALSE(policy6->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, ProposedTestAllAllowedCrossOriginChildPolicy) {
  // +------------------------------------+
  // |(1)Origin A                         |
  // |Feature-Policy: default-self *      |
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
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}});

  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_a_);
  EXPECT_TRUE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  // This is a critical change from the existing semantics.
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedFeaturePolicy frame_policy4 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}};
  std::unique_ptr<FeaturePolicy> policy4 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy4, origin_b_);
  EXPECT_TRUE(policy4->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedFeaturePolicy frame_policy5 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_b_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy5 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy5, origin_b_);
  EXPECT_TRUE(policy5->IsFeatureEnabled(kDefaultSelfFeature));

  ParsedFeaturePolicy frame_policy6 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {origin_c_}, false, false}}};
  std::unique_ptr<FeaturePolicy> policy6 =
      CreateFromParentWithFramePolicy(policy1.get(), frame_policy6, origin_b_);
  EXPECT_FALSE(policy6->IsFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, ProposedTestNestedPolicyPropagates) {
  // +-----------------------------------------------+
  // |(1)Origin A                                    |
  // |Feature-Policy: default-self 'self' OriginB;   |
  // | +--------------------------------+            |
  // | |(2)Origin B                     |            |
  // | |No Policy                       |            |
  // | | <allow="default-self *">       |            |
  // | | +--------------+               |            |
  // | | |(3)Origin B   |               |            |
  // | | |No Policy     |               |            |
  // | | +--------------+               |            |
  // | +--------------------------------+            |
  // +-----------------------------------------------+
  // Ensures that a proposed policy change will propagate down the frame tree.
  // This is important so that we can tell when a change has happened, even if
  // the feature is tested in a different one than where the
  std::unique_ptr<FeaturePolicy> policy1 =
      CreateFromParentPolicy(nullptr, origin_a_);
  policy1->SetHeaderPolicy(
      {{{kDefaultSelfFeature, /* allowed_origins */ {origin_a_, origin_b_},
         false, false}}});

  // This is where the change first occurs.
  std::unique_ptr<FeaturePolicy> policy2 =
      CreateFromParentPolicy(policy1.get(), origin_b_);
  EXPECT_FALSE(policy2->IsFeatureEnabled(kDefaultSelfFeature));

  // The proposed value in frame 2 should affect the proposed value in frame 3.
  ParsedFeaturePolicy frame_policy3 = {
      {{kDefaultSelfFeature, /* allowed_origins */ {}, true, false}}};
  std::unique_ptr<FeaturePolicy> policy3 =
      CreateFromParentWithFramePolicy(policy2.get(), frame_policy3, origin_b_);
  EXPECT_FALSE(policy3->IsFeatureEnabled(kDefaultSelfFeature));
}

}  // namespace blink
