// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/metrics/post_message_counter.h"

#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

class PostMessageCounterTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kPostMessageDifferentPartitionSameOriginBlocked,
        IsPostMessageDifferentPartitionSameOriginBlocked());
  }

 protected:
  PostMessageCounterTest()
      : frame_counter_(PostMessagePartition::kSameProcess),
        page_counter_(PostMessagePartition::kCrossProcess) {}

  bool IsPostMessageDifferentPartitionSameOriginBlocked() { return GetParam(); }

  PostMessageCounter frame_counter_;
  PostMessageCounter page_counter_;
  ukm::TestUkmRecorder recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, PostMessageCounterTest, testing::Bool());

TEST_P(PostMessageCounterTest, UsageWithoutStorageKey) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check frame counter state
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(), 2, StorageKey(), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame")[0]
                ->metrics.size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check page counter state
  EXPECT_TRUE(page_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(), 2, StorageKey(), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 4u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame")[0]
                ->metrics.size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            2u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page")[0]
                ->metrics.size(),
            1u);
}

TEST_P(PostMessageCounterTest, UsageWithDeduplicationRecall) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check frame counter state after first bump
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(), 2, StorageKey(), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check frame counter state after second bump
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(), 2, StorageKey(), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check page counter state after first bump
  EXPECT_TRUE(page_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(), 2, StorageKey(), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 4u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            2u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 1u);

  // Check page counter state after second bump
  EXPECT_TRUE(page_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(), 2, StorageKey(), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 4u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            2u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 1u);
}

TEST_P(PostMessageCounterTest, UsageWithoutDeduplicationRecall) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check frame counter state after first bump
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(), 2, StorageKey(), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Bump frame counter twenty times
  for (int i = 0; i < 20; i++) {
    EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
        i + 2, StorageKey(), i + 3, StorageKey(), &recorder_));
  }
  EXPECT_EQ(recorder_.entries_count(), 42u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            21u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            21u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check frame counter state with same source and target as first bump
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(), 2, StorageKey(), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 44u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            22u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            22u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check page counter state after first bump
  EXPECT_TRUE(page_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(), 2, StorageKey(), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 46u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            22u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            23u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 1u);

  // Bump page counter twenty times
  for (int i = 0; i < 20; i++) {
    EXPECT_TRUE(page_counter_.RecordMessageAndCheckIfShouldSend(
        i + 2, StorageKey(), i + 3, StorageKey(), &recorder_));
  }
  EXPECT_EQ(recorder_.entries_count(), 86u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            22u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            43u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(),
            21u);

  // Check page counter state with same source and target as first bump
  EXPECT_TRUE(page_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(), 2, StorageKey(), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 88u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            22u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Opaque").size(),
            44u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(),
            22u);
}

TEST_P(PostMessageCounterTest, FirstPartyToFirstPartyDifferentBucket) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(
      recorder_
          .GetEntriesByName(
              "PostMessage.Incoming.FirstPartyToFirstParty.DifferentBucket")
          .size(),
      0u);

  // Check storage key counter state
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(url::Origin::Create(GURL("https://foo.com/"))), 2,
      StorageKey(url::Origin::Create(GURL("https://bar.com/"))), &recorder_));
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(url::Origin::Create(GURL("https://foo.com/"))), 2,
      StorageKey(url::Origin::Create(GURL("https://bar.com/"))), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(
      recorder_
          .GetEntriesByName(
              "PostMessage.Incoming.FirstPartyToFirstParty.DifferentBucket")
          .size(),
      1u);
}

TEST_P(PostMessageCounterTest, FirstPartyToFirstPartySameBucket) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_
                .GetEntriesByName(
                    "PostMessage.Incoming.FirstPartyToFirstParty.SameBucket")
                .size(),
            0u);

  // Check storage key counter state
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(url::Origin::Create(GURL("https://foo.com/"))), 2,
      StorageKey(url::Origin::Create(GURL("https://foo.com/"))), &recorder_));
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(url::Origin::Create(GURL("https://foo.com/"))), 2,
      StorageKey(url::Origin::Create(GURL("https://foo.com/"))), &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_
                .GetEntriesByName(
                    "PostMessage.Incoming.FirstPartyToFirstParty.SameBucket")
                .size(),
            1u);
}

TEST_P(PostMessageCounterTest,
       FirstPartyToThirdPartyDifferentBucketDifferentOrigin) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.FirstPartyToThirdParty."
                                  "DifferentBucket.DifferentOrigin")
                .size(),
            0u);

  // Check storage key counter state
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(url::Origin::Create(GURL("https://foo.com/"))), 2,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://qux.com/")),
          url::Origin::Create(GURL("https://bar.com/"))),
      &recorder_));
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1, StorageKey(url::Origin::Create(GURL("https://foo.com/"))), 2,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://qux.com/")),
          url::Origin::Create(GURL("https://bar.com/"))),
      &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.FirstPartyToThirdParty."
                                  "DifferentBucket.DifferentOrigin")
                .size(),
            1u);
}

TEST_P(PostMessageCounterTest,
       FirstPartyToThirdPartyDifferentBucketSameOrigin) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.FirstPartyToThirdParty."
                                  "DifferentBucket.SameOrigin")
                .size(),
            0u);

  // Check storage key counter state
  EXPECT_EQ(!IsPostMessageDifferentPartitionSameOriginBlocked(),
            frame_counter_.RecordMessageAndCheckIfShouldSend(
                1, StorageKey(url::Origin::Create(GURL("https://foo.com/"))), 2,
                StorageKey::CreateForTesting(
                    url::Origin::Create(GURL("https://foo.com/")),
                    url::Origin::Create(GURL("https://qux.com/"))),
                &recorder_));
  EXPECT_EQ(!IsPostMessageDifferentPartitionSameOriginBlocked(),
            frame_counter_.RecordMessageAndCheckIfShouldSend(
                1, StorageKey(url::Origin::Create(GURL("https://foo.com/"))), 2,
                StorageKey::CreateForTesting(
                    url::Origin::Create(GURL("https://foo.com/")),
                    url::Origin::Create(GURL("https://qux.com/"))),
                &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.FirstPartyToThirdParty."
                                  "DifferentBucket.SameOrigin")
                .size(),
            1u);
}

TEST_P(PostMessageCounterTest,
       ThirdPartyToFirstPartyDifferentBucketDifferentOrigin) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.ThirdPartyToFirstParty."
                                  "DifferentBucket.DifferentOrigin")
                .size(),
            0u);

  // Check storage key counter state
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://qux.com/")),
          url::Origin::Create(GURL("https://bar.com/"))),
      2, StorageKey(url::Origin::Create(GURL("https://foo.com/"))),
      &recorder_));
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://qux.com/")),
          url::Origin::Create(GURL("https://bar.com/"))),
      2, StorageKey(url::Origin::Create(GURL("https://foo.com/"))),
      &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.ThirdPartyToFirstParty."
                                  "DifferentBucket.DifferentOrigin")
                .size(),
            1u);
}

TEST_P(PostMessageCounterTest,
       ThirdPartyToFirstPartyDifferentBucketSameOrigin) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.ThirdPartyToFirstParty."
                                  "DifferentBucket.SameOrigin")
                .size(),
            0u);

  // Check storage key counter state
  EXPECT_EQ(!IsPostMessageDifferentPartitionSameOriginBlocked(),
            frame_counter_.RecordMessageAndCheckIfShouldSend(
                1,
                StorageKey::CreateForTesting(
                    url::Origin::Create(GURL("https://foo.com/")),
                    url::Origin::Create(GURL("https://qux.com/"))),
                2, StorageKey(url::Origin::Create(GURL("https://foo.com/"))),
                &recorder_));
  EXPECT_EQ(!IsPostMessageDifferentPartitionSameOriginBlocked(),
            frame_counter_.RecordMessageAndCheckIfShouldSend(
                1,
                StorageKey::CreateForTesting(
                    url::Origin::Create(GURL("https://foo.com/")),
                    url::Origin::Create(GURL("https://qux.com/"))),
                2, StorageKey(url::Origin::Create(GURL("https://foo.com/"))),
                &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.ThirdPartyToFirstParty."
                                  "DifferentBucket.SameOrigin")
                .size(),
            1u);
}

TEST_P(PostMessageCounterTest,
       ThirdPartyToThirdPartyDifferentBucketDifferentOrigin) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.ThirdPartyToThirdParty."
                                  "DifferentBucket.DifferentOrigin")
                .size(),
            0u);

  // Check storage key counter state
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://foo.com/")),
          url::Origin::Create(GURL("https://qux.com/"))),
      2,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://bar.com/")),
          url::Origin::Create(GURL("https://qux.com/"))),
      &recorder_));
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://foo.com/")),
          url::Origin::Create(GURL("https://qux.com/"))),
      2,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://bar.com/")),
          url::Origin::Create(GURL("https://qux.com/"))),
      &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.ThirdPartyToThirdParty."
                                  "DifferentBucket.DifferentOrigin")
                .size(),
            1u);
}

TEST_P(PostMessageCounterTest,
       ThirdPartyToThirdPartyDifferentBucketSameOrigin) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.ThirdPartyToThirdParty."
                                  "DifferentBucket.SameOrigin")
                .size(),
            0u);

  // Check storage key counter state
  EXPECT_EQ(!IsPostMessageDifferentPartitionSameOriginBlocked(),
            frame_counter_.RecordMessageAndCheckIfShouldSend(
                1,
                StorageKey::CreateForTesting(
                    url::Origin::Create(GURL("https://foo.com/")),
                    url::Origin::Create(GURL("https://qux.com/"))),
                2,
                StorageKey::CreateForTesting(
                    url::Origin::Create(GURL("https://foo.com/")),
                    url::Origin::Create(GURL("https://bar.com/"))),
                &recorder_));
  EXPECT_EQ(!IsPostMessageDifferentPartitionSameOriginBlocked(),
            frame_counter_.RecordMessageAndCheckIfShouldSend(
                1,
                StorageKey::CreateForTesting(
                    url::Origin::Create(GURL("https://foo.com/")),
                    url::Origin::Create(GURL("https://qux.com/"))),
                2,
                StorageKey::CreateForTesting(
                    url::Origin::Create(GURL("https://foo.com/")),
                    url::Origin::Create(GURL("https://bar.com/"))),
                &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.ThirdPartyToThirdParty."
                                  "DifferentBucket.SameOrigin")
                .size(),
            1u);
}

TEST_P(PostMessageCounterTest, ThirdPartyToThirdPartySameBucket) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.ThirdPartyToThirdParty."
                                  "SameBucket")
                .size(),
            0u);

  // Check storage key counter state
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://foo.com/")),
          url::Origin::Create(GURL("https://bar.com/"))),
      2,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://foo.com/")),
          url::Origin::Create(GURL("https://bar.com/"))),
      &recorder_));
  EXPECT_TRUE(frame_counter_.RecordMessageAndCheckIfShouldSend(
      1,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://foo.com/")),
          url::Origin::Create(GURL("https://bar.com/"))),
      2,
      StorageKey::CreateForTesting(
          url::Origin::Create(GURL("https://foo.com/")),
          url::Origin::Create(GURL("https://bar.com/"))),
      &recorder_));
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_
                .GetEntriesByName("PostMessage.Incoming.ThirdPartyToThirdParty."
                                  "SameBucket")
                .size(),
            1u);
}

}  // namespace blink
