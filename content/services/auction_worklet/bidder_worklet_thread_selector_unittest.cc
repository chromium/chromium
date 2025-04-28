// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet_thread_selector.h"

#include <cstddef>
#include <string>
#include <utility>

#include "base/hash/hash.h"
#include "base/test/scoped_feature_list.h"
#include "content/services/auction_worklet/public/cpp/auction_worklet_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace auction_worklet {
class BidderWorkletThreadSelectorTest : public testing::Test {
 public:
  BidderWorkletThreadSelectorTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kFledgeBidderUseBalancingThreadSelector,
        {{"BidderThreadSelectorMaxImbalance", "2"}});
  }

 protected:
  const uint64_t kKeyA = 0;
  const uint64_t kKeyB = 1;
  const uint64_t kKeyC = 2;
  const uint64_t kKeyD = 3;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BidderWorkletThreadSelectorTest, OneThread_AlwaysReturnsSameThread) {
  BidderWorkletThreadSelector selector{/*num_threads=*/1};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kKeyA), 0u);
  EXPECT_EQ(selector.GetThread(kKeyB), 0u);
  EXPECT_EQ(selector.GetThread(kKeyC), 0u);
  EXPECT_EQ(selector.GetThread(kKeyD), 0u);
  EXPECT_EQ(selector.GetThread(kKeyA), 0u);
  EXPECT_EQ(selector.GetThread(kKeyB), 0u);
  EXPECT_EQ(selector.GetThread(), 0u);
}

TEST_F(BidderWorkletThreadSelectorTest, TwoThreads_AllThreadsAreUsed) {
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  EXPECT_EQ(selector.GetThread(kKeyB), 0u);
  EXPECT_EQ(selector.GetThread(kKeyC), 1u);
  EXPECT_EQ(selector.GetThread(kKeyD), 0u);
  EXPECT_EQ(selector.GetThread(), 1u);
}

TEST_F(BidderWorkletThreadSelectorTest, ThreeThreads_AllThreadsAreUsed) {
  BidderWorkletThreadSelector selector{/*num_threads=*/3};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  EXPECT_EQ(selector.GetThread(kKeyB), 2u);
  EXPECT_EQ(selector.GetThread(kKeyC), 0u);
  EXPECT_EQ(selector.GetThread(kKeyD), 1u);
  // Reuse kKeyA's thread.
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  EXPECT_EQ(selector.GetThread(), 2u);
}

TEST_F(BidderWorkletThreadSelectorTest, MaxAllowedImbalanceIsRespected) {
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  // The max allowed imbalance has been reached.
  EXPECT_EQ(selector.GetThread(kKeyA), 0u);
}

TEST_F(BidderWorkletThreadSelectorTest,
       LeastUsedThreadGetsAssignedWhenReachingMaxImbalance) {
  BidderWorkletThreadSelector selector{/*num_threads=*/3};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  EXPECT_EQ(selector.GetThread(kKeyA), 2u);
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
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  EXPECT_EQ(selector.GetThread(kKeyB), 0u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  selector.TaskCompletedOnThread(1u);
  // Thread 1 is the least used now.
  EXPECT_EQ(selector.GetThread(kKeyC), 1u);
}

TEST_F(BidderWorkletThreadSelectorTest, ZeroImbalanceIsRespected) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kFledgeBidderUseBalancingThreadSelector,
      {{"BidderThreadSelectorMaxImbalance", "0"}});
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  EXPECT_EQ(selector.GetThread(kKeyA), 0u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  EXPECT_EQ(selector.GetThread(kKeyA), 0u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
}

TEST_F(BidderWorkletThreadSelectorTest, TaskCompletedOnThread) {
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  EXPECT_EQ(selector.GetThread(), 0u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  EXPECT_EQ(selector.GetThread(kKeyA), 1u);
  selector.TaskCompletedOnThread(0u);
  // The max allowed imbalance has been reached because thread 0 now has 0 tasks
  // and thread 1 has 2.
  EXPECT_EQ(selector.GetThread(kKeyA), 0u);
}

TEST_F(BidderWorkletThreadSelectorTest, BalancingThreadSelectorDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kFledgeBidderUseBalancingThreadSelector);
  BidderWorkletThreadSelector selector{/*num_threads=*/2};

  uint64_t hash_salt = selector.group_by_origin_key_hash_salt_for_testing();

  EXPECT_EQ(selector.GetThread(), 0u);
  size_t thread_a = base::HashCombine(hash_salt, kKeyA) % 2;
  size_t thread_b = base::HashCombine(hash_salt, kKeyB) % 2;
  EXPECT_EQ(selector.GetThread(kKeyA), thread_a);
  EXPECT_EQ(selector.GetThread(kKeyA), thread_a);
  EXPECT_EQ(selector.GetThread(kKeyA), thread_a);
  EXPECT_EQ(selector.GetThread(kKeyB), thread_b);
  EXPECT_EQ(selector.GetThread(kKeyB), thread_b);
  EXPECT_EQ(selector.GetThread(kKeyB), thread_b);
  EXPECT_EQ(selector.GetThread(kKeyA), thread_a);
  EXPECT_EQ(selector.GetThread(), 1u);
}

}  // namespace auction_worklet
