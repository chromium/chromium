// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet_thread_selector.h"

#include <cstddef>

#include "base/hash/hash.h"
#include "base/test/scoped_feature_list.h"
#include "content/services/auction_worklet/public/cpp/auction_worklet_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace auction_worklet {
class BidderWorkletThreadSelectorTest : public testing::Test {
 public:
  BidderWorkletThreadSelectorTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kFledgeBidderUseBalancingThreadSelector,
        {{"BidderThreadSelectorMaxImbalance", "2"}});
  }

 protected:
  url::Origin kUrlA = url::Origin::Create(GURL("https://a.test"));
  url::Origin kUrlB = url::Origin::Create(GURL("https://b.test"));
  url::Origin kUrlC = url::Origin::Create(GURL("https://c.test"));
  url::Origin kUrlD = url::Origin::Create(GURL("https://d.test"));

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BidderWorkletThreadSelectorTest, OneThread_AlwaysReturnsSameThread) {
  BidderWorkletThreadSelector selector{/*num_threads=*/1};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 0u);
  EXPECT_EQ(selector.GetThread(kUrlB), 0u);
  EXPECT_EQ(selector.GetThread(kUrlC), 0u);
  EXPECT_EQ(selector.GetThread(kUrlD), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 0u);
  EXPECT_EQ(selector.GetThread(kUrlB), 0u);
  EXPECT_EQ(selector.GetThread(), 0u);
}

TEST_F(BidderWorkletThreadSelectorTest, TwoThreads_AllThreadsAreUsed) {
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  EXPECT_EQ(selector.GetThread(kUrlB), 0u);
  EXPECT_EQ(selector.GetThread(kUrlC), 1u);
  EXPECT_EQ(selector.GetThread(kUrlD), 0u);
  EXPECT_EQ(selector.GetThread(), 1u);
}

TEST_F(BidderWorkletThreadSelectorTest, ThreeThreads_AllThreadsAreUsed) {
  BidderWorkletThreadSelector selector{/*num_threads=*/3};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  EXPECT_EQ(selector.GetThread(kUrlB), 2u);
  EXPECT_EQ(selector.GetThread(kUrlC), 0u);
  EXPECT_EQ(selector.GetThread(kUrlD), 1u);
  // Reuse kUrlA's thread.
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  EXPECT_EQ(selector.GetThread(), 2u);
}

TEST_F(BidderWorkletThreadSelectorTest, MaxAllowedImbalanceIsRespected) {
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  // The max allowed imbalance has been reached.
  EXPECT_EQ(selector.GetThread(kUrlA), 0u);
}

TEST_F(BidderWorkletThreadSelectorTest,
       LeastUsedThreadGetsAssignedWhenReachingMaxImbalance) {
  BidderWorkletThreadSelector selector{/*num_threads=*/3};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  EXPECT_EQ(selector.GetThread(kUrlA), 2u);
}

TEST_F(BidderWorkletThreadSelectorTest,
       LeastUsedThreadGetsAssignedForNullOptJoiningOrigin) {
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(), 1u);
  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(), 1u);
  selector.TaskCompletedOnThread(1u);
  // Thread 1 is the least used now.
  EXPECT_EQ(selector.GetThread(), 1u);
}

TEST_F(BidderWorkletThreadSelectorTest,
       LeastUsedThreadGetsAssignedForUnencounteredJoiningOrigin) {
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  EXPECT_EQ(selector.GetThread(kUrlB), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  selector.TaskCompletedOnThread(1u);
  // Thread 1 is the least used now.
  EXPECT_EQ(selector.GetThread(kUrlC), 1u);
}

TEST_F(BidderWorkletThreadSelectorTest, ZeroImbalanceIsRespected) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kFledgeBidderUseBalancingThreadSelector,
      {{"BidderThreadSelectorMaxImbalance", "0"}});
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  EXPECT_EQ(selector.GetThread(kUrlA), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  EXPECT_EQ(selector.GetThread(kUrlA), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
}

TEST_F(BidderWorkletThreadSelectorTest, TaskCompletedOnThread) {
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  EXPECT_EQ(selector.GetThread(kUrlA), 1u);
  selector.TaskCompletedOnThread(0u);
  // The max allowed imbalance has been reached because thread 0 now has 0 tasks
  // and thread 1 has 2.
  EXPECT_EQ(selector.GetThread(kUrlA), 0u);
}

TEST_F(BidderWorkletThreadSelectorTest, BalancingThreadSelectorDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kFledgeBidderUseBalancingThreadSelector);
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  std::string hash_salt = selector.join_origin_hash_salt_for_testing();

  EXPECT_EQ(selector.GetThread(), 0u);
  size_t thread_a = base::FastHash(hash_salt + kUrlA.Serialize()) % 2;
  size_t thread_b = base::FastHash(hash_salt + kUrlB.Serialize()) % 2;
  EXPECT_EQ(selector.GetThread(kUrlA), thread_a);
  EXPECT_EQ(selector.GetThread(kUrlA), thread_a);
  EXPECT_EQ(selector.GetThread(kUrlA), thread_a);
  EXPECT_EQ(selector.GetThread(kUrlB), thread_b);
  EXPECT_EQ(selector.GetThread(kUrlB), thread_b);
  EXPECT_EQ(selector.GetThread(kUrlB), thread_b);
  EXPECT_EQ(selector.GetThread(kUrlA), thread_a);
  EXPECT_EQ(selector.GetThread(), 1u);
}

}  // namespace auction_worklet
