// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_match_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class TextMatchMarkerListImplTest : public EditingTestBase {
 protected:
  TextMatchMarkerListImplTest()
      : marker_list_(MakeGarbageCollected<TextMatchMarkerListImpl>()) {}

  DocumentMarker* CreateMarker(wtf_size_t start_offset, wtf_size_t end_offset) {
    return MakeGarbageCollected<TextMatchMarker>(
        start_offset, end_offset, TextMatchMarker::MatchStatus::kInactive);
  }

  Persistent<TextMatchMarkerListImpl> marker_list_;
};

TEST_F(TextMatchMarkerListImplTest, MarkerType) {
  EXPECT_EQ(DocumentMarker::kTextMatch, marker_list_->MarkerType());
}

TEST_F(TextMatchMarkerListImplTest, Add) {
  EXPECT_EQ(0u, marker_list_->GetMarkers().size());

  marker_list_->Add(CreateMarker(0, 1));
  marker_list_->Add(CreateMarker(1, 2));

  EXPECT_EQ(2u, marker_list_->GetMarkers().size());

  EXPECT_EQ(0u, marker_list_->GetMarkers()[0]->StartOffset());
  EXPECT_EQ(1u, marker_list_->GetMarkers()[0]->EndOffset());

  EXPECT_EQ(1u, marker_list_->GetMarkers()[1]->StartOffset());
  EXPECT_EQ(2u, marker_list_->GetMarkers()[1]->EndOffset());
}

TEST_F(TextMatchMarkerListImplTest,
       MarkersIntersectingRangeAfterMovingCollapsedMarker) {
  marker_list_->Add(CreateMarker(0, 1));

  auto* dst_list = MakeGarbageCollected<TextMatchMarkerListImpl>();
  ASSERT_TRUE(marker_list_->MoveMarkers(1, dst_list));
  EXPECT_TRUE(marker_list_->IsEmpty());
  dst_list->Add(CreateMarker(0, 2));

  const auto& markers = dst_list->GetMarkers();
  ASSERT_EQ(2u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(0u, markers[0]->EndOffset());
  EXPECT_EQ(0u, markers[1]->StartOffset());
  EXPECT_EQ(2u, markers[1]->EndOffset());

  EXPECT_TRUE(dst_list->MarkersIntersectingRange(0, 0).empty());
  EXPECT_TRUE(dst_list->MarkersIntersectingRange(2, 2).empty());

  const auto interior_matches = dst_list->MarkersIntersectingRange(1, 1);
  ASSERT_EQ(1u, interior_matches.size());
  EXPECT_EQ(markers[1].Get(), interior_matches[0].Get());

  const auto range_matches = dst_list->MarkersIntersectingRange(0, 2);
  ASSERT_EQ(1u, range_matches.size());
  EXPECT_EQ(markers[1].Get(), range_matches[0].Get());
}

TEST_F(TextMatchMarkerListImplTest,
       MarkersIntersectingRangeAfterMovingCollapsedMarkerAtNonzeroOffset) {
  marker_list_->Add(CreateMarker(0, 1));
  marker_list_->Add(CreateMarker(2, 3));

  auto* dst_list = MakeGarbageCollected<TextMatchMarkerListImpl>();
  ASSERT_TRUE(marker_list_->MoveMarkers(3, dst_list));
  EXPECT_TRUE(marker_list_->IsEmpty());
  dst_list->Add(CreateMarker(2, 4));

  const auto& markers = dst_list->GetMarkers();
  ASSERT_EQ(3u, markers.size());
  EXPECT_EQ(2u, markers[1]->StartOffset());
  EXPECT_EQ(2u, markers[1]->EndOffset());
  EXPECT_EQ(2u, markers[2]->StartOffset());
  EXPECT_EQ(4u, markers[2]->EndOffset());

  EXPECT_TRUE(dst_list->MarkersIntersectingRange(2, 2).empty());
  EXPECT_TRUE(dst_list->MarkersIntersectingRange(1, 2).empty());
  EXPECT_TRUE(dst_list->MarkersIntersectingRange(4, 4).empty());

  const auto interior_matches = dst_list->MarkersIntersectingRange(3, 3);
  ASSERT_EQ(1u, interior_matches.size());
  EXPECT_EQ(markers[2].Get(), interior_matches[0].Get());

  const auto range_matches = dst_list->MarkersIntersectingRange(2, 4);
  ASSERT_EQ(1u, range_matches.size());
  EXPECT_EQ(markers[2].Get(), range_matches[0].Get());

  const auto all_matches = dst_list->MarkersIntersectingRange(0, 4);
  ASSERT_EQ(3u, all_matches.size());
  for (wtf_size_t i = 0; i < markers.size(); ++i) {
    EXPECT_EQ(markers[i].Get(), all_matches[i].Get());
  }
}

}  // namespace blink
