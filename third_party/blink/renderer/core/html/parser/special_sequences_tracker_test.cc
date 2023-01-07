// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/special_sequences_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(SpecialSequencesTrackerTest, FindNullChar) {
  SpecialSequencesTracker tracker;
  tracker.UpdateIndices(String("123\0", 4u));
  EXPECT_EQ(3u, tracker.index_of_first_special_sequence());
}

TEST(SpecialSequencesTrackerTest, FindNullCharSecondChunk) {
  SpecialSequencesTracker tracker;
  tracker.UpdateIndices("abc");
  tracker.UpdateIndices(String("d\0f", 3u));
  EXPECT_EQ(4u, tracker.index_of_first_special_sequence());
}

TEST(SpecialSequencesTrackerTest, SimpleCData) {
  SpecialSequencesTracker tracker;
  tracker.UpdateIndices("abcdef<<![CDATA[");
  EXPECT_EQ(7u, tracker.index_of_first_special_sequence());
}

TEST(SpecialSequencesTrackerTest, SplitCData) {
  SpecialSequencesTracker tracker;
  tracker.UpdateIndices("abc<![");
  tracker.UpdateIndices("CD");
  tracker.UpdateIndices("ATA[");
  EXPECT_EQ(3u, tracker.index_of_first_special_sequence());
}

TEST(SpecialSequencesTrackerTest, IncompleteCData) {
  SpecialSequencesTracker tracker;
  tracker.UpdateIndices("abcdef<<![CDATA");
  EXPECT_EQ(SpecialSequencesTracker::kNoSpecialSequencesFound,
            tracker.index_of_first_special_sequence());
}

TEST(SpecialSequencesTrackerTest, SplitWithPartialThenFull) {
  SpecialSequencesTracker tracker;
  tracker.UpdateIndices("abc<![");
  tracker.UpdateIndices("<![CDATA[");
  EXPECT_EQ(6u, tracker.index_of_first_special_sequence());
}

}  // namespace blink
