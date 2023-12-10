// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/overlapping_document_marker_list_editor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/markers/marker_test_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class OverlappingDocumentMarkerListEditorTest : public testing::Test {
 public:
  OverlappingDocumentMarkerListEditorTest()
      : marker_list_(
            MakeGarbageCollected<HeapVector<Member<DocumentMarker>>>()) {}

 protected:
  DocumentMarker* CreateMarker(unsigned start_offset, unsigned end_offset) {
    return MakeGarbageCollected<SuggestionMarker>(start_offset, end_offset,
                                                  SuggestionMarkerProperties());
  }

  Persistent<HeapVector<Member<DocumentMarker>>> marker_list_;
};

TEST_F(OverlappingDocumentMarkerListEditorTest, AddMarkersOverlapping) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(40, 60));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 30));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(70, 100));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 20));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 40));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(45, 70));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(35, 65));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(90, 100));

  EXPECT_EQ(8u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(30u, marker_list_->at(0)->EndOffset());
  EXPECT_EQ(0u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(20u, marker_list_->at(1)->EndOffset());
  EXPECT_EQ(0u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(40u, marker_list_->at(2)->EndOffset());
  EXPECT_EQ(35u, marker_list_->at(3)->StartOffset());
  EXPECT_EQ(65u, marker_list_->at(3)->EndOffset());
  EXPECT_EQ(40u, marker_list_->at(4)->StartOffset());
  EXPECT_EQ(60u, marker_list_->at(4)->EndOffset());
  EXPECT_EQ(45u, marker_list_->at(5)->StartOffset());
  EXPECT_EQ(70u, marker_list_->at(5)->EndOffset());
  EXPECT_EQ(70u, marker_list_->at(6)->StartOffset());
  EXPECT_EQ(100u, marker_list_->at(6)->EndOffset());
  EXPECT_EQ(90u, marker_list_->at(7)->StartOffset());
  EXPECT_EQ(100u, marker_list_->at(7)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest, MoveMarkers) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(30, 40));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 30));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(10, 40));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 20));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 40));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(20, 40));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(20, 30));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(10, 30));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(10, 20));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(11, 21));

  DocumentMarkerList* dst_list =
      MakeGarbageCollected<SuggestionMarkerListImpl>();
  // The markers with start and end offset < 11 should be moved to dst_list.
  // Markers that start before 11 and end at 11 or later should be removed.
  // Markers that start at 11 or later should not be moved.
  OverlappingDocumentMarkerListEditor::MoveMarkers(marker_list_, 11, dst_list);

  EXPECT_EQ(4u, marker_list_->size());

  EXPECT_EQ(11u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(21u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(20u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(40u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(20u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(30u, marker_list_->at(2)->EndOffset());

  EXPECT_EQ(30u, marker_list_->at(3)->StartOffset());
  EXPECT_EQ(40u, marker_list_->at(3)->EndOffset());

  DocumentMarkerVector dst_list_markers = dst_list->GetMarkers();

  // Markers
  EXPECT_EQ(1u, dst_list_markers.size());

  EXPECT_EQ(0u, dst_list_markers[0]->StartOffset());
  EXPECT_EQ(10u, dst_list_markers[0]->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest, RemoveMarkersNestedOverlap) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(10, 30));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(15, 20));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(20, 30));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(30, 40));

  EXPECT_TRUE(
      OverlappingDocumentMarkerListEditor::RemoveMarkers(marker_list_, 20, 10));

  EXPECT_EQ(3u, marker_list_->size());

  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(15u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(20u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(30u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(40u, marker_list_->at(2)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       RemoveMarkersTouchingEndpoints) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(30, 40));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(40, 50));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(10, 20));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(20, 30));

  EXPECT_TRUE(
      OverlappingDocumentMarkerListEditor::RemoveMarkers(marker_list_, 20, 10));

  EXPECT_EQ(4u, marker_list_->size());

  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(10u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(20u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(30u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(40u, marker_list_->at(2)->EndOffset());

  EXPECT_EQ(40u, marker_list_->at(3)->StartOffset());
  EXPECT_EQ(50u, marker_list_->at(3)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       RemoveMarkersOneCharacterIntoInterior) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(30, 40));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(40, 50));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(10, 20));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(20, 30));

  EXPECT_TRUE(
      OverlappingDocumentMarkerListEditor::RemoveMarkers(marker_list_, 19, 12));

  EXPECT_EQ(2u, marker_list_->size());

  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(40u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(50u, marker_list_->at(1)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceStartOfMarker) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));

  // Replace with shorter text
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 5, 4);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(9u, marker_list_->at(0)->EndOffset());

  // Replace with longer text
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 4, 5);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  // Replace with text of same length
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 5, 5);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceContainsStartOfMarker) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(5, 15));

  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 10, 10);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(10u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(15u, marker_list_->at(0)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceEndOfMarker) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));

  // Replace with shorter text
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 5, 5, 4);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(9u, marker_list_->at(0)->EndOffset());

  // Replace with longer text
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 5, 4, 5);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  // Replace with text of same length
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 5, 5, 5);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceContainsEndOfMarker) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));

  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 5, 10, 10);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(5u, marker_list_->at(0)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceEntireMarker) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));

  // Replace with shorter text
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 10, 9);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(9u, marker_list_->at(0)->EndOffset());

  // Replace with longer text
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 9, 10);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  // Replace with text of same length
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 10, 10);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceTextWithMarkerAtBeginning) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));

  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 15, 15);

  EXPECT_EQ(0u, marker_list_->size());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceTextWithMarkerAtEnd) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(5, 15));

  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 15, 15);

  EXPECT_EQ(0u, marker_list_->size());
}

TEST_F(OverlappingDocumentMarkerListEditorTest, ShiftMarkers_Deletions) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 5));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(5, 10));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(10, 15));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(15, 20));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(20, 25));

  // Delete range containing the end of the second marker, the entire third
  // marker, and the start of the fourth marker
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 8, 9, 0);

  EXPECT_EQ(4u, marker_list_->size());

  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(5u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(5u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(8u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(8u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(11u, marker_list_->at(2)->EndOffset());

  EXPECT_EQ(11u, marker_list_->at(3)->StartOffset());
  EXPECT_EQ(16u, marker_list_->at(3)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       ShiftMarkers_DeletionWithinNested) {
  // A marker that overlaps the range with markers that do not overlap
  // nested within it.
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(5, 35));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(7, 10));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(15, 25));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(30, 32));

  // Delete range overlapping the outermost marker and containing the
  // third marker.
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 15, 10, 0);

  EXPECT_EQ(3u, marker_list_->size());

  EXPECT_EQ(5u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(25u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(7u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(20u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(22u, marker_list_->at(2)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       ShiftMarkers_DeleteExactlyOnMarker) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));

  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 10, 0);

  EXPECT_EQ(0u, marker_list_->size());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       ShiftMarkers_InsertInMarkerInterior) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 5));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(5, 10));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(10, 15));

  // insert in middle of second marker
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 7, 0, 5);

  EXPECT_EQ(3u, marker_list_->size());

  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(5u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(5u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(15u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(15u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(20u, marker_list_->at(2)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       ShiftMarkers_InsertBetweenMarkers) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 5));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(5, 10));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(10, 15));

  // insert before second marker
  OverlappingDocumentMarkerListEditor::ShiftMarkers(marker_list_, 5, 0, 5);

  EXPECT_EQ(3u, marker_list_->size());

  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(5u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(10u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(15u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(15u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(20u, marker_list_->at(2)->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       MarkersIntersectingRange_RangeContainingNoMarkers) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));

  OverlappingDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      OverlappingDocumentMarkerListEditor::MarkersIntersectingRange(
          *marker_list_, 10, 15);
  EXPECT_EQ(0u, markers_intersecting_range.size());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       MarkersIntersectingRange_TouchingStart) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 9));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(1, 9));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(1, 10));

  OverlappingDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      OverlappingDocumentMarkerListEditor::MarkersIntersectingRange(
          *marker_list_, 0, 1);

  EXPECT_EQ(2u, markers_intersecting_range.size());

  EXPECT_EQ(0u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(9u, markers_intersecting_range[0]->EndOffset());

  EXPECT_EQ(0u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[1]->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       MarkersIntersectingRange_TouchingEnd) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 9));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(1, 9));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(0, 10));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(1, 10));

  OverlappingDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      OverlappingDocumentMarkerListEditor::MarkersIntersectingRange(
          *marker_list_, 9, 10);

  EXPECT_EQ(2u, markers_intersecting_range.size());

  EXPECT_EQ(0u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());

  EXPECT_EQ(1u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[1]->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       MarkersIntersectingRange_CollapsedRange) {
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(5, 10));

  OverlappingDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      OverlappingDocumentMarkerListEditor::MarkersIntersectingRange(
          *marker_list_, 7, 7);
  EXPECT_EQ(1u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());
}

TEST_F(OverlappingDocumentMarkerListEditorTest,
       MarkersIntersectingRange_NestedMarkers) {
  // A marker that overlaps the range with markers that do not overlap
  // nested within it.
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(5, 35));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(7, 10));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(15, 25));
  OverlappingDocumentMarkerListEditor::AddMarker(marker_list_,
                                                 CreateMarker(30, 32));

  OverlappingDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      OverlappingDocumentMarkerListEditor::MarkersIntersectingRange(
          *marker_list_, 15, 25);
  EXPECT_EQ(2u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(35u, markers_intersecting_range[0]->EndOffset());
  EXPECT_EQ(15u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(25u, markers_intersecting_range[1]->EndOffset());
}

}  // namespace blink
