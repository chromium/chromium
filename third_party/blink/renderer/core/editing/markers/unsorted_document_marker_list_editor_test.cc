// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/unsorted_document_marker_list_editor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/markers/marker_test_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class UnsortedDocumentMarkerListEditorTest : public testing::Test {
 public:
  UnsortedDocumentMarkerListEditorTest()
      : marker_list_(
            MakeGarbageCollected<HeapVector<Member<DocumentMarker>>>()) {}

 protected:
  DocumentMarker* CreateMarker(unsigned start_offset, unsigned end_offset) {
    return MakeGarbageCollected<SuggestionMarker>(start_offset, end_offset,
                                                  SuggestionMarkerProperties());
  }

  Persistent<HeapVector<Member<DocumentMarker>>> marker_list_;
};

TEST_F(UnsortedDocumentMarkerListEditorTest, AddMarkersOverlapping) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(40, 60));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 30));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(70, 100));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 20));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 40));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(45, 70));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(35, 65));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
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

TEST_F(UnsortedDocumentMarkerListEditorTest, MoveMarkers) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(30, 40));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 30));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(10, 40));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 20));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 40));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(20, 40));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(20, 30));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(10, 30));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(10, 20));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(11, 21));

  DocumentMarkerList* dst_list =
      MakeGarbageCollected<SuggestionMarkerListImpl>();
  // The markers with start and end offset < 11 should be moved to dst_list.
  // Markers that start before 11 and end at 11 or later should be removed.
  // Markers that start at 11 or later should not be moved.
  UnsortedDocumentMarkerListEditor::MoveMarkers(marker_list_, 11, dst_list);

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

TEST_F(UnsortedDocumentMarkerListEditorTest, RemoveMarkersNestedOverlap) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(10, 30));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(15, 20));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(20, 30));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(30, 40));

  EXPECT_TRUE(
      UnsortedDocumentMarkerListEditor::RemoveMarkers(marker_list_, 20, 10));

  EXPECT_EQ(3u, marker_list_->size());

  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(15u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(20u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(30u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(40u, marker_list_->at(2)->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest, RemoveMarkersTouchingEndpoints) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(30, 40));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(40, 50));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(10, 20));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(20, 30));

  EXPECT_TRUE(
      UnsortedDocumentMarkerListEditor::RemoveMarkers(marker_list_, 20, 10));

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

TEST_F(UnsortedDocumentMarkerListEditorTest,
       RemoveMarkersOneCharacterIntoInterior) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(30, 40));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(40, 50));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(10, 20));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(20, 30));

  EXPECT_TRUE(
      UnsortedDocumentMarkerListEditor::RemoveMarkers(marker_list_, 19, 12));

  EXPECT_EQ(2u, marker_list_->size());

  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(40u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(50u, marker_list_->at(1)->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceStartOfMarker) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));

  // Replace with shorter text
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 5, 4);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(9u, marker_list_->at(0)->EndOffset());

  // Replace with longer text
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 4, 5);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  // Replace with text of same length
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 5, 5);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceContainsStartOfMarker) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(5, 15));

  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 10, 10);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(10u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(15u, marker_list_->at(0)->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest, ShiftMarkers_ReplaceEndOfMarker) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));

  // Replace with shorter text
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 5, 5, 4);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(9u, marker_list_->at(0)->EndOffset());

  // Replace with longer text
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 5, 4, 5);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  // Replace with text of same length
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 5, 5, 5);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceContainsEndOfMarker) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));

  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 5, 10, 10);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(5u, marker_list_->at(0)->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest, ShiftMarkers_ReplaceEntireMarker) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));

  // Replace with shorter text
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 10, 9);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(9u, marker_list_->at(0)->EndOffset());

  // Replace with longer text
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 9, 10);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  // Replace with text of same length
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 10, 10);

  EXPECT_EQ(1u, marker_list_->size());
  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceTextWithMarkerAtBeginning) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));

  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 15, 15);

  EXPECT_EQ(0u, marker_list_->size());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ShiftMarkers_ReplaceTextWithMarkerAtEnd) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(5, 15));

  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 15, 15);

  EXPECT_EQ(0u, marker_list_->size());
}

TEST_F(UnsortedDocumentMarkerListEditorTest, ShiftMarkers_Deletions) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_, CreateMarker(0, 5));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(5, 10));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(10, 15));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(15, 20));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(20, 25));

  // Delete range containing the end of the second marker, the entire third
  // marker, and the start of the fourth marker
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 8, 9, 0);

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

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ShiftMarkers_DeletionWithinNested) {
  // A marker that overlaps the range with markers that do not overlap
  // nested within it.
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(5, 35));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(7, 10));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(15, 25));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(30, 32));

  // Delete range overlapping the outermost marker and containing the
  // third marker.
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 15, 10, 0);

  EXPECT_EQ(3u, marker_list_->size());

  EXPECT_EQ(5u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(25u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(7u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(20u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(22u, marker_list_->at(2)->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ShiftMarkers_DeleteExactlyOnMarker) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));

  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 0, 10, 0);

  EXPECT_EQ(0u, marker_list_->size());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ShiftMarkers_InsertInMarkerInterior) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_, CreateMarker(0, 5));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(5, 10));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(10, 15));

  // insert in middle of second marker
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 7, 0, 5);

  EXPECT_EQ(3u, marker_list_->size());

  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(5u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(5u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(15u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(15u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(20u, marker_list_->at(2)->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ShiftMarkers_InsertBetweenMarkers) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_, CreateMarker(0, 5));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(5, 10));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(10, 15));

  // insert before second marker
  UnsortedDocumentMarkerListEditor::ShiftMarkers(marker_list_, 5, 0, 5);

  EXPECT_EQ(3u, marker_list_->size());

  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(5u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(10u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(15u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(15u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(20u, marker_list_->at(2)->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_RangeContainingNoMarkers) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));

  UnsortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(*marker_list_,
                                                                 10, 15);
  EXPECT_EQ(0u, markers_intersecting_range.size());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_TouchingStart) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_, CreateMarker(0, 9));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_, CreateMarker(1, 9));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(1, 10));

  UnsortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(*marker_list_,
                                                                 0, 1);

  EXPECT_EQ(2u, markers_intersecting_range.size());

  EXPECT_EQ(0u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(9u, markers_intersecting_range[0]->EndOffset());

  EXPECT_EQ(0u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[1]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_TouchingEnd) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_, CreateMarker(0, 9));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_, CreateMarker(1, 9));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(0, 10));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(1, 10));

  UnsortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(*marker_list_,
                                                                 9, 10);

  EXPECT_EQ(2u, markers_intersecting_range.size());

  EXPECT_EQ(0u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());

  EXPECT_EQ(1u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[1]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_CollapsedRange) {
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(5, 10));

  UnsortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(*marker_list_,
                                                                 7, 7);
  EXPECT_EQ(1u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_NestedMarkers) {
  // A marker that overlaps the range with markers that do not overlap
  // nested within it.
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(5, 35));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(7, 10));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(15, 25));
  UnsortedDocumentMarkerListEditor::AddMarker(marker_list_,
                                              CreateMarker(30, 32));

  UnsortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(*marker_list_,
                                                                 15, 25);
  EXPECT_EQ(2u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(35u, markers_intersecting_range[0]->EndOffset());
  EXPECT_EQ(15u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(25u, markers_intersecting_range[1]->EndOffset());
}

}  // namespace blink
