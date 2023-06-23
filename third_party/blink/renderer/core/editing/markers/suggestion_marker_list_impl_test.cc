// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_list_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/markers/marker_test_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_replacement_scope.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class SuggestionMarkerListImplTest : public testing::Test {
 protected:
  SuggestionMarkerListImplTest()
      : marker_list_(MakeGarbageCollected<SuggestionMarkerListImpl>()) {}

  SuggestionMarker* CreateMarker(unsigned start_offset, unsigned end_offset) {
    return MakeGarbageCollected<SuggestionMarker>(start_offset, end_offset,
                                                  SuggestionMarkerProperties());
  }

  SuggestionMarker* CreateMarker(unsigned start_offset,
                                 unsigned end_offset,
                                 const SuggestionMarkerProperties& properties) {
    return MakeGarbageCollected<SuggestionMarker>(start_offset, end_offset,
                                                  properties);
  }

  Persistent<SuggestionMarkerListImpl> marker_list_;
};

TEST_F(SuggestionMarkerListImplTest, MarkerType) {
  EXPECT_EQ(DocumentMarker::kSuggestion, marker_list_->MarkerType());
}

TEST_F(SuggestionMarkerListImplTest, AddOverlapping) {
  // Add some overlapping markers in an arbitrary order and verify that the
  // list stores them properly
  marker_list_->Add(CreateMarker(40, 50));
  marker_list_->Add(CreateMarker(10, 40));
  marker_list_->Add(CreateMarker(20, 50));
  marker_list_->Add(CreateMarker(10, 30));
  marker_list_->Add(CreateMarker(10, 50));
  marker_list_->Add(CreateMarker(30, 50));
  marker_list_->Add(CreateMarker(30, 40));
  marker_list_->Add(CreateMarker(10, 20));
  marker_list_->Add(CreateMarker(20, 40));
  marker_list_->Add(CreateMarker(20, 30));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  std::sort(markers.begin(), markers.end(), compare_markers);

  EXPECT_EQ(10u, markers.size());

  EXPECT_EQ(10u, markers[0]->StartOffset());
  EXPECT_EQ(20u, markers[0]->EndOffset());

  EXPECT_EQ(10u, markers[1]->StartOffset());
  EXPECT_EQ(30u, markers[1]->EndOffset());

  EXPECT_EQ(10u, markers[2]->StartOffset());
  EXPECT_EQ(40u, markers[2]->EndOffset());

  EXPECT_EQ(10u, markers[3]->StartOffset());
  EXPECT_EQ(50u, markers[3]->EndOffset());

  EXPECT_EQ(20u, markers[4]->StartOffset());
  EXPECT_EQ(30u, markers[4]->EndOffset());

  EXPECT_EQ(20u, markers[5]->StartOffset());
  EXPECT_EQ(40u, markers[5]->EndOffset());

  EXPECT_EQ(20u, markers[6]->StartOffset());
  EXPECT_EQ(50u, markers[6]->EndOffset());

  EXPECT_EQ(30u, markers[7]->StartOffset());
  EXPECT_EQ(40u, markers[7]->EndOffset());

  EXPECT_EQ(30u, markers[8]->StartOffset());
  EXPECT_EQ(50u, markers[8]->EndOffset());

  EXPECT_EQ(40u, markers[9]->StartOffset());
  EXPECT_EQ(50u, markers[9]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForSuggestionReplacement_ReturnsFalseWhenNoShift) {
  marker_list_->Add(CreateMarker(0, 10));

  {
    SuggestionMarkerReplacementScope scope;
    // Replace range 0 to 10 with a ten character string.
    // Text is ignored for suggestion replacement, so we can just pass an empty
    // string.
    EXPECT_FALSE(marker_list_->ShiftMarkers("", 0, 10, 10));
  }

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForSuggestionReplacement_MarkersUpdateProperly) {
  // Marker with suggestion to apply.
  // Should be kept (and shifted).
  marker_list_->Add(CreateMarker(10, 20));

  // Marker touching start of replacement range.
  // Should be kept.
  marker_list_->Add(CreateMarker(0, 10));

  // Marker partially overlapping start of replacement range.
  // Should be removed,
  marker_list_->Add(CreateMarker(0, 11));

  // Marker touching end of replacement range.
  // Should be kept (and shifted).
  marker_list_->Add(CreateMarker(20, 30));

  // Marker partially overlapping end of replacement range
  // Should be removed.
  marker_list_->Add(CreateMarker(19, 30));

  // Marker contained inside replacement range
  // Should be removed.
  marker_list_->Add(CreateMarker(11, 19));

  // Marker containing replacement range
  // Should be kept (and shifted).
  marker_list_->Add(CreateMarker(9, 21));

  {
    SuggestionMarkerReplacementScope scope;
    // Replace range 10 to 20 with a five character string.
    // Text is ignored for suggestion replacement, so we can just pass an empty
    // string.
    EXPECT_TRUE(marker_list_->ShiftMarkers("", 10, 10, 5));
  }

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  std::sort(markers.begin(), markers.end(), compare_markers);

  EXPECT_EQ(4u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  EXPECT_EQ(9u, markers[1]->StartOffset());
  EXPECT_EQ(16u, markers[1]->EndOffset());

  EXPECT_EQ(10u, markers[2]->StartOffset());
  EXPECT_EQ(15u, markers[2]->EndOffset());

  EXPECT_EQ(15u, markers[3]->StartOffset());
  EXPECT_EQ(25u, markers[3]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_DeleteFromMiddle) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("hello", 2, 1, 0));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_InsertIntoMiddle) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("hello", 2, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_PrependLetter) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("ahello", 0, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(
    SuggestionMarkerListImplTest,
    ShiftMarkersForNonSuggestionEditingOperation_PrependSurrogatePairLetter) {
  marker_list_->Add(CreateMarker(0, 5));

  // Prepending MATHEMATICAL SCRIPT CAPITAL C
  EXPECT_TRUE(marker_list_->ShiftMarkers(u"\U0001d49ehello", 0, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_PrependDigit) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("0hello", 0, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_PrependSurrogatePairDigit) {
  marker_list_->Add(CreateMarker(0, 5));

  // Prepending MATHEMATICAL DOUBLE-STRUCK DIGIT ONE
  EXPECT_TRUE(marker_list_->ShiftMarkers(u"\U0001d7d9hello", 0, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_PrependNonAlphanumeric) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers(".hello", 0, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();

  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(1u, markers[0]->StartOffset());
  EXPECT_EQ(6u, markers[0]->EndOffset());
}

TEST_F(
    SuggestionMarkerListImplTest,
    ShiftMarkersForNonSuggestionEditingOperation_PrependSurrogatePairNonAlphanumeric) {
  marker_list_->Add(CreateMarker(0, 5));

  // Prepending FACE WITH TEARS OF JOY
  EXPECT_TRUE(marker_list_->ShiftMarkers(u"\U0001f602hello", 0, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();

  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(2u, markers[0]->StartOffset());
  EXPECT_EQ(7u, markers[0]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_AppendLetter) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("helloa", 5, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_AppendSurrogatePairLetter) {
  marker_list_->Add(CreateMarker(0, 5));

  // Appending MATHEMATICAL SCRIPT CAPITAL C
  EXPECT_TRUE(marker_list_->ShiftMarkers(u"hello\U0001d49e", 5, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_AppendDigit) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("hello0", 5, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_AppendSurrogatePairDigit) {
  marker_list_->Add(CreateMarker(0, 5));

  // Appending MATHEMATICAL DOUBLE-STRUCK DIGIT ONE
  EXPECT_TRUE(marker_list_->ShiftMarkers(u"hello\U0001d7d9", 5, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_AppendNonAlphanumeric) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_FALSE(marker_list_->ShiftMarkers("hello.", 5, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();

  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());
}

TEST_F(
    SuggestionMarkerListImplTest,
    ShiftMarkersForNonSuggestionEditingOperation_AppendSurrogatePairNonAlphanumeric) {
  marker_list_->Add(CreateMarker(0, 5));

  // Appending FACE WITH TEARS OF JOY
  EXPECT_FALSE(marker_list_->ShiftMarkers(u"hello\U0001f602", 5, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();

  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest, RemoveMarkerByTag_NotFound) {
  SuggestionMarker* const marker = CreateMarker(0, 10);
  marker_list_->Add(marker);

  EXPECT_FALSE(marker_list_->RemoveMarkerByTag(marker->Tag() + 1));
}

TEST_F(SuggestionMarkerListImplTest, RemoveMarkerByTag_Found) {
  SuggestionMarker* const marker1 = CreateMarker(0, 10);
  SuggestionMarker* const marker2 = CreateMarker(10, 20);

  marker_list_->Add(marker1);
  marker_list_->Add(marker2);

  EXPECT_TRUE(marker_list_->RemoveMarkerByTag(marker1->Tag()));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(10u, markers[0]->StartOffset());
  EXPECT_EQ(20u, markers[0]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest, RemoveMarkerByType_NotFound) {
  SuggestionMarker* const marker = CreateMarker(0, 10);
  marker_list_->Add(marker);
  EXPECT_TRUE(marker->GetSuggestionType() !=
              SuggestionMarker::SuggestionType::kAutocorrect);
  EXPECT_FALSE(marker_list_->RemoveMarkerByType(
      SuggestionMarker::SuggestionType::kAutocorrect));
}

TEST_F(SuggestionMarkerListImplTest, RemoveMarkerByType_Found) {
  SuggestionMarker* const marker1 = CreateMarker(0, 10);
  SuggestionMarker* const marker2 =
      CreateMarker(10, 20,
                   SuggestionMarkerProperties::Builder()
                       .SetType(SuggestionMarker::SuggestionType::kAutocorrect)
                       .Build());

  marker_list_->Add(marker1);
  marker_list_->Add(marker2);

  EXPECT_TRUE(marker1->GetSuggestionType() !=
              SuggestionMarker::SuggestionType::kAutocorrect);
  EXPECT_TRUE(marker_list_->RemoveMarkerByType(marker1->GetSuggestionType()));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(10u, markers[0]->StartOffset());
  EXPECT_EQ(20u, markers[0]->EndOffset());
}

}  // namespace blink
