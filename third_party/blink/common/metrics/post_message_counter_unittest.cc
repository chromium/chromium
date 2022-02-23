// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/metrics/post_message_counter.h"

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PostMessageCounterTest : public ::testing::Test {
 protected:
  PostMessageCounterTest()
      : frame_counter_(PostMessagePartition::kSameProcess),
        page_counter_(PostMessagePartition::kCrossProcess) {}

  PostMessageCounter frame_counter_;
  PostMessageCounter page_counter_;
  ukm::TestUkmRecorder recorder_;
};

TEST_F(PostMessageCounterTest, UsageWithValidSourceIds) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check frame counter state
  frame_counter_.RecordMessage(1, 2, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame")[0]
                ->metrics.size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check page counter state
  page_counter_.RecordMessage(1, 2, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame")[0]
                ->metrics.size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page")[0]
                ->metrics.size(),
            1u);
}

TEST_F(PostMessageCounterTest, UsageWithInvalidSourceIds) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check frame counter state (and verify metrics are empty)
  frame_counter_.RecordMessage(ukm::kInvalidSourceId, ukm::kInvalidSourceId,
                               &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame")[0]
                ->metrics.size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check page counter state (and verify metrics are empty)
  page_counter_.RecordMessage(ukm::kInvalidSourceId, ukm::kInvalidSourceId,
                              &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame")[0]
                ->metrics.size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page")[0]
                ->metrics.size(),
            0u);
}

TEST_F(PostMessageCounterTest, UsageWithDeduplicationRecall) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check frame counter state after first bump
  frame_counter_.RecordMessage(1, 2, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check frame counter state after second bump
  frame_counter_.RecordMessage(1, 2, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check page counter state after first bump
  page_counter_.RecordMessage(1, 2, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 1u);

  // Check page counter state after second bump
  page_counter_.RecordMessage(1, 2, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 2u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 1u);
}

TEST_F(PostMessageCounterTest, UsageWithoutDeduplicationRecall) {
  // Initial state check
  EXPECT_EQ(recorder_.entries_count(), 0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            0u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check frame counter state after first bump
  frame_counter_.RecordMessage(1, 2, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            1u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Bump frame counter twenty times
  for (int i = 0; i < 20; i++)
    frame_counter_.RecordMessage(i + 2, i + 3, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 21u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            21u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check frame counter state with same source and target as first bump
  frame_counter_.RecordMessage(1, 2, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 22u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            22u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 0u);

  // Check page counter state after first bump
  page_counter_.RecordMessage(1, 2, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 23u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            22u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(), 1u);

  // Bump page counter twenty times
  for (int i = 0; i < 20; i++)
    page_counter_.RecordMessage(i + 2, i + 3, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 43u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            22u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(),
            21u);

  // Check page counter state with same source and target as first bump
  page_counter_.RecordMessage(1, 2, &recorder_);
  EXPECT_EQ(recorder_.entries_count(), 44u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Frame").size(),
            22u);
  EXPECT_EQ(recorder_.GetEntriesByName("PostMessage.Incoming.Page").size(),
            22u);
}

}  // namespace blink
