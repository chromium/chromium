// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/document_marker.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"

namespace blink {

using MarkerOffsets = DocumentMarker::MarkerOffsets;

class DocumentMarkerTest : public testing::Test {
 protected:
  DocumentMarker* CreateMarker(unsigned startOffset, unsigned endOffset) {
    return MakeGarbageCollected<TextMatchMarker>(
        startOffset, endOffset, TextMatchMarker::MatchStatus::kInactive);
  }
};

TEST_F(DocumentMarkerTest, MarkerTypeIteratorEmpty) {
  DocumentMarker::MarkerTypes types(0);
  EXPECT_TRUE(types.begin() == types.end());
}

TEST_F(DocumentMarkerTest, MarkerTypeIteratorOne) {
  DocumentMarker::MarkerTypes types(DocumentMarker::kSpelling);
  ASSERT_TRUE(types.begin() != types.end());
  auto it = types.begin();
  EXPECT_EQ(DocumentMarker::kSpelling, *it);
  ++it;
  EXPECT_TRUE(it == types.end());
}

TEST_F(DocumentMarkerTest, MarkerTypeIteratorConsecutive) {
  DocumentMarker::MarkerTypes types(0b11);  // Spelling | Grammar
  ASSERT_TRUE(types.begin() != types.end());
  auto it = types.begin();
  EXPECT_EQ(DocumentMarker::kSpelling, *it);
  ++it;
  EXPECT_EQ(DocumentMarker::kGrammar, *it);
  ++it;
  EXPECT_TRUE(it == types.end());
}

TEST_F(DocumentMarkerTest, MarkerTypeIteratorDistributed) {
  DocumentMarker::MarkerTypes types(0b101);  // Spelling | TextMatch
  ASSERT_TRUE(types.begin() != types.end());
  auto it = types.begin();
  EXPECT_EQ(DocumentMarker::kSpelling, *it);
  ++it;
  EXPECT_EQ(DocumentMarker::kTextMatch, *it);
  ++it;
  EXPECT_TRUE(it == types.end());
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_DeleteAfter) {
  DocumentMarker* marker = CreateMarker(0, 5);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(13, 19, 0);
  EXPECT_EQ(0u, result.value().start_offset);
  EXPECT_EQ(5u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_DeleteEndAndAfter) {
  DocumentMarker* marker = CreateMarker(10, 15);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(13, 19, 0);
  EXPECT_EQ(10u, result.value().start_offset);
  EXPECT_EQ(13u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_DeleteBeforeAndAfter) {
  DocumentMarker* marker = CreateMarker(20, 25);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(13, 19, 0);
  EXPECT_EQ(std::nullopt, result);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_DeleteBeforeAndBeginning) {
  DocumentMarker* marker = CreateMarker(30, 35);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(13, 19, 0);
  EXPECT_EQ(13u, result.value().start_offset);
  EXPECT_EQ(16u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_DeleteBefore) {
  DocumentMarker* marker = CreateMarker(40, 45);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(13, 19, 0);
  EXPECT_EQ(21u, result.value().start_offset);
  EXPECT_EQ(26u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_DeleteStartAndAfter) {
  DocumentMarker* marker = CreateMarker(0, 5);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(0, 10, 0);
  EXPECT_EQ(std::nullopt, result);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_DeleteBeforeAndEnd) {
  DocumentMarker* marker = CreateMarker(5, 10);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(0, 10, 0);
  EXPECT_EQ(std::nullopt, result);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_DeleteMarkerExactly) {
  DocumentMarker* marker = CreateMarker(5, 10);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(5, 5, 0);
  EXPECT_EQ(std::nullopt, result);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_DeleteMiddleOfMarker) {
  DocumentMarker* marker = CreateMarker(5, 10);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(6, 3, 0);
  EXPECT_EQ(5u, result.value().start_offset);
  EXPECT_EQ(7u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_InsertAfter) {
  DocumentMarker* marker = CreateMarker(0, 5);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(10, 0, 5);
  EXPECT_EQ(0u, result.value().start_offset);
  EXPECT_EQ(5u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_InsertImmediatelyAfter) {
  DocumentMarker* marker = CreateMarker(0, 5);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(5, 0, 5);
  EXPECT_EQ(0u, result.value().start_offset);
  EXPECT_EQ(5u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_InsertInMiddle) {
  DocumentMarker* marker = CreateMarker(0, 5);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(2, 0, 5);
  EXPECT_EQ(0u, result.value().start_offset);
  EXPECT_EQ(10u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_InsertImmediatelyBefore) {
  DocumentMarker* marker = CreateMarker(0, 5);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(0, 0, 5);
  EXPECT_EQ(5u, result.value().start_offset);
  EXPECT_EQ(10u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_InsertBefore) {
  DocumentMarker* marker = CreateMarker(5, 10);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(0, 0, 5);
  EXPECT_EQ(10u, result.value().start_offset);
  EXPECT_EQ(15u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_ReplaceAfter) {
  DocumentMarker* marker = CreateMarker(0, 5);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(13, 19, 1);
  EXPECT_EQ(0u, result.value().start_offset);
  EXPECT_EQ(5u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_ReplaceEndAndAfter) {
  DocumentMarker* marker = CreateMarker(10, 15);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(13, 19, 1);
  EXPECT_EQ(10u, result.value().start_offset);
  EXPECT_EQ(13u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_ReplaceBeforeAndAfter) {
  DocumentMarker* marker = CreateMarker(20, 25);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(13, 19, 1);
  EXPECT_EQ(std::nullopt, result);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_ReplaceBeforeAndBeginning) {
  DocumentMarker* marker = CreateMarker(30, 35);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(13, 19, 1);
  EXPECT_EQ(14u, result.value().start_offset);
  EXPECT_EQ(17u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_ReplaceBefore) {
  DocumentMarker* marker = CreateMarker(40, 45);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(13, 19, 1);
  EXPECT_EQ(22u, result.value().start_offset);
  EXPECT_EQ(27u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_ReplaceBeginning) {
  DocumentMarker* marker = CreateMarker(0, 5);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(0, 2, 1);
  EXPECT_EQ(0u, result.value().start_offset);
  EXPECT_EQ(4u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_ReplaceEnd) {
  DocumentMarker* marker = CreateMarker(0, 5);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(3, 2, 1);
  EXPECT_EQ(0u, result.value().start_offset);
  EXPECT_EQ(4u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_ReplaceExactly) {
  DocumentMarker* marker = CreateMarker(0, 5);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(0, 5, 1);
  EXPECT_EQ(0u, result.value().start_offset);
  EXPECT_EQ(1u, result.value().end_offset);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_ReplaceBeginningAndAfter) {
  DocumentMarker* marker = CreateMarker(0, 5);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(0, 6, 1);
  EXPECT_EQ(std::nullopt, result);
}

TEST_F(DocumentMarkerTest, GetShiftedMarkerPosition_ReplaceBeforeAndEnd) {
  DocumentMarker* marker = CreateMarker(5, 10);
  std::optional<MarkerOffsets> result =
      marker->ComputeOffsetsAfterShift(4, 6, 1);
  EXPECT_EQ(std::nullopt, result);
}

}  // namespace blink
