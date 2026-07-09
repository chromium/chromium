// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/cert_builder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(MtcLogBuilderTest, SubtreesForLandmarkRangeLargeSplit) {
  constexpr uint64_t kEnd = uint64_t{1} << 40;
  constexpr uint64_t kMid = uint64_t{1} << 39;

  std::vector<bssl::Subtree> subtrees =
      MtcLogBuilder::SubtreesForLandmarkRangeForTesting(0, kEnd);

  ASSERT_EQ(subtrees.size(), 2u);
  ASSERT_EQ(subtrees[0].start, 0u);
  ASSERT_EQ(subtrees[0].end, kMid);
  ASSERT_EQ(subtrees[1].start, kMid);
  ASSERT_EQ(subtrees[1].end, kEnd);
}

}  // namespace net
