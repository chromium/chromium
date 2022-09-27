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

TEST_F(UnsortedDocumentMarkerListEditorTest, MoveMarkers) {
  marker_list_->push_back(CreateMarker(30, 40));
  marker_list_->push_back(CreateMarker(0, 30));
  marker_list_->push_back(CreateMarker(10, 40));
  marker_list_->push_back(CreateMarker(0, 20));
  marker_list_->push_back(CreateMarker(0, 40));
  marker_list_->push_back(CreateMarker(20, 40));
  marker_list_->push_back(CreateMarker(20, 30));
  marker_list_->push_back(CreateMarker(0, 10));
  marker_list_->push_back(CreateMarker(10, 30));
  marker_list_->push_back(CreateMarker(10, 20));
  marker_list_->push_back(CreateMarker(11, 21));

  DocumentMarkerList* dst_list =
      MakeGarbageCollected<SuggestionMarkerListImpl>();
  // The markers with start and end offset < 11 should be moved to dst_list.
  // Markers that start before 11 and end at 11 or later should be removed.
  // Markers that start at 11 or later should not be moved.
  UnsortedDocumentMarkerListEditor::MoveMarkers(marker_list_, 11, dst_list);

  std::sort(marker_list_->begin(), marker_list_->end(), compare_markers);

  EXPECT_EQ(4u, marker_list_->size());

  EXPECT_EQ(11u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(21u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(20u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(30u, marker_list_->at(1)->EndOffset());

  EXPECT_EQ(20u, marker_list_->at(2)->StartOffset());
  EXPECT_EQ(40u, marker_list_->at(2)->EndOffset());

  EXPECT_EQ(30u, marker_list_->at(3)->StartOffset());
  EXPECT_EQ(40u, marker_list_->at(3)->EndOffset());

  DocumentMarkerVector dst_list_markers = dst_list->GetMarkers();
  std::sort(dst_list_markers.begin(), dst_list_markers.end(), compare_markers);

  // Markers
  EXPECT_EQ(1u, dst_list_markers.size());

  EXPECT_EQ(0u, dst_list_markers[0]->StartOffset());
  EXPECT_EQ(10u, dst_list_markers[0]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest, RemoveMarkersEmptyList) {
  EXPECT_FALSE(
      UnsortedDocumentMarkerListEditor::RemoveMarkers(marker_list_, 0, 10));
  EXPECT_EQ(0u, marker_list_->size());
}

TEST_F(UnsortedDocumentMarkerListEditorTest, RemoveMarkersTouchingEndpoints) {
  marker_list_->push_back(CreateMarker(30, 40));
  marker_list_->push_back(CreateMarker(40, 50));
  marker_list_->push_back(CreateMarker(10, 20));
  marker_list_->push_back(CreateMarker(0, 10));
  marker_list_->push_back(CreateMarker(20, 30));

  EXPECT_TRUE(
      UnsortedDocumentMarkerListEditor::RemoveMarkers(marker_list_, 20, 10));

  std::sort(marker_list_->begin(), marker_list_->end(), compare_markers);

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
  marker_list_->push_back(CreateMarker(30, 40));
  marker_list_->push_back(CreateMarker(40, 50));
  marker_list_->push_back(CreateMarker(10, 20));
  marker_list_->push_back(CreateMarker(0, 10));
  marker_list_->push_back(CreateMarker(20, 30));

  EXPECT_TRUE(
      UnsortedDocumentMarkerListEditor::RemoveMarkers(marker_list_, 19, 12));

  std::sort(marker_list_->begin(), marker_list_->end(), compare_markers);

  EXPECT_EQ(2u, marker_list_->size());

  EXPECT_EQ(0u, marker_list_->at(0)->StartOffset());
  EXPECT_EQ(10u, marker_list_->at(0)->EndOffset());

  EXPECT_EQ(40u, marker_list_->at(1)->StartOffset());
  EXPECT_EQ(50u, marker_list_->at(1)->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceStartOfMarker) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  // Replace with shorter text
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                   5, 4);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(9u, markers[0]->EndOffset());

  // Replace with longer text
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                   4, 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  // Replace with text of same length
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                   5, 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceContainsStartOfMarker) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 15));

  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                   10, 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers[0]->StartOffset());
  EXPECT_EQ(15u, markers[0]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceEndOfMarker) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  // Replace with shorter text
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5,
                                                                   5, 4);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(9u, markers[0]->EndOffset());

  // Replace with longer text
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5,
                                                                   4, 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  // Replace with text of same length
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5,
                                                                   5, 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceContainsEndOfMarker) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5,
                                                                   10, 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceEntireMarker) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  // Replace with shorter text
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                   10, 9);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(9u, markers[0]->EndOffset());

  // Replace with longer text
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                   9, 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  // Replace with text of same length
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                   10, 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceTextWithMarkerAtBeginning) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                   15, 15);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceTextWithMarkerAtEnd) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 15));

  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                   15, 15);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_Deletions) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));
  markers.push_back(CreateMarker(15, 20));
  markers.push_back(CreateMarker(20, 25));

  // Delete range containing the end of the second marker, the entire third
  // marker, and the start of the fourth marker
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 8,
                                                                   9, 0);

  EXPECT_EQ(4u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(5u, markers[1]->StartOffset());
  EXPECT_EQ(8u, markers[1]->EndOffset());

  EXPECT_EQ(8u, markers[2]->StartOffset());
  EXPECT_EQ(11u, markers[2]->EndOffset());

  EXPECT_EQ(11u, markers[3]->StartOffset());
  EXPECT_EQ(16u, markers[3]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_DeleteExactlyOnMarker) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                   10, 0);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_InsertInMarkerInterior) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));

  // insert in middle of second marker
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 7,
                                                                   0, 5);

  EXPECT_EQ(3u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(5u, markers[1]->StartOffset());
  EXPECT_EQ(15u, markers[1]->EndOffset());

  EXPECT_EQ(15u, markers[2]->StartOffset());
  EXPECT_EQ(20u, markers[2]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_InsertBetweenMarkers) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));

  // insert before second marker
  UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5,
                                                                   0, 5);

  EXPECT_EQ(3u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(10u, markers[1]->StartOffset());
  EXPECT_EQ(15u, markers[1]->EndOffset());

  EXPECT_EQ(15u, markers[2]->StartOffset());
  EXPECT_EQ(20u, markers[2]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_Empty) {
  DocumentMarker* marker =
      UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
          *marker_list_, 0, 10);
  EXPECT_EQ(nullptr, marker);
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_RangeContainingNoMarkers) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  DocumentMarker* marker =
      UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 5,
                                                                     15);
  EXPECT_EQ(nullptr, marker);
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_TouchingStart) {
  marker_list_->push_back(CreateMarker(1, 10));
  marker_list_->push_back(CreateMarker(0, 10));

  DocumentMarker* marker =
      UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
          *marker_list_, 0, 1);

  EXPECT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_TouchingEnd) {
  marker_list_->push_back(CreateMarker(0, 9));
  marker_list_->push_back(CreateMarker(0, 10));

  DocumentMarker* marker =
      UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
          *marker_list_, 9, 10);

  EXPECT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_CollapsedRange) {
  marker_list_->push_back(CreateMarker(5, 10));

  DocumentMarker* marker =
      UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
          *marker_list_, 7, 7);

  EXPECT_NE(nullptr, marker);
  EXPECT_EQ(5u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_RangeContainingNoMarkers) {
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  UnsortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(markers, 10,
                                                                 15);
  EXPECT_EQ(0u, markers_intersecting_range.size());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_TouchingStart) {
  marker_list_->push_back(CreateMarker(0, 9));
  marker_list_->push_back(CreateMarker(1, 9));
  marker_list_->push_back(CreateMarker(0, 10));
  marker_list_->push_back(CreateMarker(1, 10));

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
  marker_list_->push_back(CreateMarker(0, 9));
  marker_list_->push_back(CreateMarker(1, 9));
  marker_list_->push_back(CreateMarker(0, 10));
  marker_list_->push_back(CreateMarker(1, 10));

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
  UnsortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  UnsortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(markers, 7, 7);
  EXPECT_EQ(1u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());
}

}  // namespace blink
