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

TEST(DefaultNetReferrerPolicyTest,
     DefaultPolicyRespectsReducedGranularityFeature) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(blink::features::kReducedReferrerGranularity);
  EXPECT_EQ(blink::ReferrerUtils::GetDefaultNetReferrerPolicy(),
            net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN);
}

}  // namespace blink
