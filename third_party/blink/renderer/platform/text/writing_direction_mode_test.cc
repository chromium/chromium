// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

PhysicalDirection LineOver(WritingMode mode) {
  return WritingDirectionMode(mode, TextDirection::kLtr).LineOver();
}

PhysicalDirection LineUnder(WritingMode mode) {
  return WritingDirectionMode(mode, TextDirection::kLtr).LineUnder();
}

}  // namespace

TEST(WritingDirectionModeTest, LineOver) {
  EXPECT_EQ(PhysicalDirection::kUp, LineOver(WritingMode::kHorizontalTb));
  EXPECT_EQ(PhysicalDirection::kRight, LineOver(WritingMode::kVerticalRl));
  EXPECT_EQ(PhysicalDirection::kRight, LineOver(WritingMode::kVerticalLr));
  EXPECT_EQ(PhysicalDirection::kRight, LineOver(WritingMode::kSidewaysRl));
  EXPECT_EQ(PhysicalDirection::kLeft, LineOver(WritingMode::kSidewaysLr));
}

TEST(WritingDirectionModeTest, LineUnder) {
  EXPECT_EQ(PhysicalDirection::kDown, LineUnder(WritingMode::kHorizontalTb));
  EXPECT_EQ(PhysicalDirection::kLeft, LineUnder(WritingMode::kVerticalRl));
  EXPECT_EQ(PhysicalDirection::kLeft, LineUnder(WritingMode::kVerticalLr));
  EXPECT_EQ(PhysicalDirection::kLeft, LineUnder(WritingMode::kSidewaysRl));
  EXPECT_EQ(PhysicalDirection::kRight, LineUnder(WritingMode::kSidewaysLr));
}

TEST(WritingDirectionModeTest, IsFlippedXY) {
  struct TestData {
    WritingDirectionMode writing_direction;
    bool is_flipped_x;
    bool is_flipped_y;
  } test_data_list[] = {
      {{WritingMode::kHorizontalTb, TextDirection::kLtr}, false, false},
      {{WritingMode::kHorizontalTb, TextDirection::kRtl}, true, false},
      {{WritingMode::kVerticalRl, TextDirection::kLtr}, true, false},
      {{WritingMode::kVerticalRl, TextDirection::kRtl}, true, true},
      {{WritingMode::kVerticalLr, TextDirection::kLtr}, false, false},
      {{WritingMode::kVerticalLr, TextDirection::kRtl}, false, true},
      {{WritingMode::kSidewaysRl, TextDirection::kLtr}, true, false},
      {{WritingMode::kSidewaysRl, TextDirection::kRtl}, true, true},
      {{WritingMode::kSidewaysLr, TextDirection::kLtr}, false, true},
      {{WritingMode::kSidewaysLr, TextDirection::kRtl}, false, false},
  };
  for (const TestData& data : test_data_list) {
    SCOPED_TRACE(data.writing_direction);
    EXPECT_EQ(data.writing_direction.IsFlippedX(), data.is_flipped_x);
    EXPECT_EQ(data.writing_direction.IsFlippedY(), data.is_flipped_y);
  }
}

}  // namespace blink
