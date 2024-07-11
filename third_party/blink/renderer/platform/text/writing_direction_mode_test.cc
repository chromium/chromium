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

}  // namespace blink
