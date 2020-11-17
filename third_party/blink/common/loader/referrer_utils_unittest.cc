// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/referrer_utils.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

TEST(DefaultNetReferrerPolicyTest, Unconfigured) {
  EXPECT_EQ(blink::ReferrerUtils::GetDefaultNetReferrerPolicy(),
            net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN);
}

TEST(DefaultNetReferrerPolicyTest, FeatureOnly) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kReducedReferrerGranularity);
  EXPECT_EQ(blink::ReferrerUtils::GetDefaultNetReferrerPolicy(),
            net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN);
}

TEST(DefaultReferrerPolicyTest, SetAndGetForceLegacy) {
  EXPECT_FALSE(blink::ReferrerUtils::ShouldForceLegacyDefaultReferrerPolicy());
  blink::ReferrerUtils::SetForceLegacyDefaultReferrerPolicy(true);
  EXPECT_TRUE(blink::ReferrerUtils::ShouldForceLegacyDefaultReferrerPolicy());
}

TEST(DefaultReferrerPolicyTest, ForceLegacyOnly) {
  blink::ReferrerUtils::SetForceLegacyDefaultReferrerPolicy(true);
  EXPECT_EQ(blink::ReferrerUtils::GetDefaultNetReferrerPolicy(),
            net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE);
}

TEST(DefaultReferrerPolicyTest, FeatureAndForceLegacy) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kReducedReferrerGranularity);
  blink::ReferrerUtils::SetForceLegacyDefaultReferrerPolicy(true);
  EXPECT_EQ(blink::ReferrerUtils::GetDefaultNetReferrerPolicy(),
            net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE);
}

}  // namespace blink
