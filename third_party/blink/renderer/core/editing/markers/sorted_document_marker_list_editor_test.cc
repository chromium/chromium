// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/sorted_document_marker_list_editor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"

namespace blink {

class SortedDocumentMarkerListEditorTest : public testing::Test {
 protected:
  DocumentMarker* CreateMarker(unsigned startOffset, unsigned endOffset) {
    return MakeGarbageCollected<TextMatchMarker>(
        startOffset, endOffset, TextMatchMarker::MatchStatus::kInactive);
  }
};

TEST_F(SortedDocumentMarkerListEditorTest, RemoveMarkersEmptyList) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  SortedDocumentMarkerListEditor::RemoveMarkers(&markers, 0, 10);
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest, RemoveMarkersTouchingEndpoints) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));
  markers.push_back(CreateMarker(10, 20));
  markers.push_back(CreateMarker(20, 30));

  SortedDocumentMarkerListEditor::RemoveMarkers(&markers, 10, 10);

  EXPECT_EQ(2u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  EXPECT_EQ(20u, markers[1]->StartOffset());
  EXPECT_EQ(30u, markers[1]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       RemoveMarkersOneCharacterIntoInterior) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));
  markers.push_back(CreateMarker(10, 20));
  markers.push_back(CreateMarker(20, 30));

  SortedDocumentMarkerListEditor::RemoveMarkers(&markers, 9, 12);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceStartOfMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 5,
                                                               5);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceStartOfMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  // Replace with shorter text
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 5,
                                                                 4);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(9u, markers[0]->EndOffset());

  // Replace with longer text
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 4,
                                                                 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  // Replace with text of same length
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 5,
                                                                 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceContainsStartOfMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 15));

  SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 10,
                                                               10);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceContainsStartOfMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 15));

  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                 10, 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers[0]->StartOffset());
  EXPECT_EQ(15u, markers[0]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceEndOfMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 5, 5,
                                                               5);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceEndOfMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  // Replace with shorter text
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5, 5,
                                                                 4);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(9u, markers[0]->EndOffset());

  // Replace with longer text
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5, 4,
                                                                 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  // Replace with text of same length
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5, 5,
                                                                 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceContainsEndOfMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 5, 10,
                                                               10);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceContainsEndOfMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5,
                                                                 10, 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceEntireMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 10,
                                                               10);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceEntireMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  // Replace with shorter text
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                 10, 9);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(9u, markers[0]->EndOffset());

  // Replace with longer text
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 9,
                                                                 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  // Replace with text of same length
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                 10, 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceTextWithMarkerAtBeginning) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 15,
                                                               15);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceTextWithMarkerAtBeginning) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                 15, 15);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceTextWithMarkerAtEnd) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 15));

  SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 15,
                                                               15);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceTextWithMarkerAtEnd) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 15));

  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                 15, 15);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest, ContentDependentMarker_Deletions) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));
  markers.push_back(CreateMarker(15, 20));
  markers.push_back(CreateMarker(20, 25));

  // Delete range containing the end of the second marker, the entire third
  // marker, and the start of the fourth marker
  SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 8, 9,
                                                               0);

  EXPECT_EQ(2u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(11u, markers[1]->StartOffset());
  EXPECT_EQ(16u, markers[1]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest, ContentIndependentMarker_Deletions) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));
  markers.push_back(CreateMarker(15, 20));
  markers.push_back(CreateMarker(20, 25));

  // Delete range containing the end of the second marker, the entire third
  // marker, and the start of the fourth marker
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 8, 9,
                                                                 0);

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

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentDependentMarker_DeleteExactlyOnMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 10,
                                                               0);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_DeleteExactlyOnMarker) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0,
                                                                 10, 0);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentDependentMarker_InsertInMarkerInterior) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));

  // insert in middle of second marker
  SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 7, 0,
                                                               5);

  EXPECT_EQ(2u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(15u, markers[1]->StartOffset());
  EXPECT_EQ(20u, markers[1]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_InsertInMarkerInterior) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));

  // insert in middle of second marker
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 7, 0,
                                                                 5);

  EXPECT_EQ(3u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(5u, markers[1]->StartOffset());
  EXPECT_EQ(15u, markers[1]->EndOffset());

  EXPECT_EQ(15u, markers[2]->StartOffset());
  EXPECT_EQ(20u, markers[2]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentDependentMarker_InsertBetweenMarkers) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));

  // insert before second marker
  SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 5, 0,
                                                               5);

  EXPECT_EQ(3u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(10u, markers[1]->StartOffset());
  EXPECT_EQ(15u, markers[1]->EndOffset());

  EXPECT_EQ(15u, markers[2]->StartOffset());
  EXPECT_EQ(20u, markers[2]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       ContentIndependentMarker_InsertBetweenMarkers) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));

  // insert before second marker
  SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5, 0,
                                                                 5);

  EXPECT_EQ(3u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(10u, markers[1]->StartOffset());
  EXPECT_EQ(15u, markers[1]->EndOffset());

  EXPECT_EQ(15u, markers[2]->StartOffset());
  EXPECT_EQ(20u, markers[2]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest, FirstMarkerIntersectingRange_Empty) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));

  DocumentMarker* marker =
      SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 10,
                                                                   15);
  EXPECT_EQ(nullptr, marker);
}

TEST_F(SortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_TouchingAfter) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));

  DocumentMarker* marker =
      SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 5,
                                                                   10);
  EXPECT_EQ(nullptr, marker);
}

TEST_F(SortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_TouchingBefore) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  DocumentMarker* marker =
      SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 0,
                                                                   5);
  EXPECT_EQ(nullptr, marker);
}

TEST_F(SortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_IntersectingAfter) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  DocumentMarker* marker =
      SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 0,
                                                                   6);
  EXPECT_NE(nullptr, marker);

  EXPECT_EQ(5u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_IntersectingBefore) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  DocumentMarker* marker =
      SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 9,
                                                                   15);
  EXPECT_NE(nullptr, marker);

  EXPECT_EQ(5u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_MultipleMarkers) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));
  markers.push_back(CreateMarker(15, 20));
  markers.push_back(CreateMarker(20, 25));

  DocumentMarker* marker =
      SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 7,
                                                                   17);
  EXPECT_NE(nullptr, marker);

  EXPECT_EQ(5u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest, MarkersIntersectingRange_Empty) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));

  SortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      SortedDocumentMarkerListEditor::MarkersIntersectingRange(markers, 10, 15);
  EXPECT_EQ(0u, markers_intersecting_range.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_TouchingAfter) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));

  SortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      SortedDocumentMarkerListEditor::MarkersIntersectingRange(markers, 5, 10);
  EXPECT_EQ(0u, markers_intersecting_range.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_TouchingBefore) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  SortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      SortedDocumentMarkerListEditor::MarkersIntersectingRange(markers, 0, 5);
  EXPECT_EQ(0u, markers_intersecting_range.size());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_IntersectingAfter) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  SortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      SortedDocumentMarkerListEditor::MarkersIntersectingRange(markers, 0, 6);
  EXPECT_EQ(1u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_IntersectingBefore) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  SortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      SortedDocumentMarkerListEditor::MarkersIntersectingRange(markers, 9, 15);
  EXPECT_EQ(1u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_CollapsedRange) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  SortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      SortedDocumentMarkerListEditor::MarkersIntersectingRange(markers, 7, 7);
  EXPECT_EQ(1u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());
}

TEST_F(SortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_MultipleMarkers) {
  SortedDocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));
  markers.push_back(CreateMarker(15, 20));
  markers.push_back(CreateMarker(20, 25));

  SortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      SortedDocumentMarkerListEditor::MarkersIntersectingRange(markers, 7, 17);
  EXPECT_EQ(3u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());

  EXPECT_EQ(10u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(15u, markers_intersecting_range[1]->EndOffset());

  EXPECT_EQ(15u, markers_intersecting_range[2]->StartOffset());
  EXPECT_EQ(20u, markers_intersecting_range[2]->EndOffset());
}

}  // namespace blink
