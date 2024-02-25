// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/spell_check_marker_list_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/markers/spelling_marker.h"
#include "third_party/blink/renderer/core/editing/markers/spelling_marker_list_impl.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// This test class tests functionality implemented by SpellingMarkerListImpl and
// also functionality implemented by its parent class SpellCheckMarkerListImpl.

class SpellingMarkerListImplTest : public testing::Test {
 protected:
  SpellingMarkerListImplTest()
      : marker_list_(MakeGarbageCollected<SpellingMarkerListImpl>()) {}

  DocumentMarker* CreateMarker(unsigned start_offset, unsigned end_offset) {
    return MakeGarbageCollected<SpellingMarker>(start_offset, end_offset,
                                                g_empty_string);
  }

  Persistent<SpellingMarkerListImpl> marker_list_;
};

// Test cases for functionality implemented by SpellingMarkerListImpl.

TEST_F(SpellingMarkerListImplTest, MarkerType) {
  EXPECT_EQ(DocumentMarker::kSpelling, marker_list_->MarkerType());
}

// Test cases for functionality implemented by SpellCheckMarkerListImpl

TEST_F(SpellingMarkerListImplTest, AddSorting) {
  // Insert some markers in an arbitrary order and verify that the list stays
  // sorted
  marker_list_->Add(CreateMarker(80, 85));
  marker_list_->Add(CreateMarker(40, 45));
  marker_list_->Add(CreateMarker(10, 15));
  marker_list_->Add(CreateMarker(0, 5));
  marker_list_->Add(CreateMarker(70, 75));
  marker_list_->Add(CreateMarker(90, 95));
  marker_list_->Add(CreateMarker(60, 65));
  marker_list_->Add(CreateMarker(50, 55));
  marker_list_->Add(CreateMarker(30, 35));
  marker_list_->Add(CreateMarker(20, 25));

  EXPECT_EQ(10u, marker_list_->GetMarkers().size());

  EXPECT_EQ(0u, marker_list_->GetMarkers()[0]->StartOffset());
  EXPECT_EQ(5u, marker_list_->GetMarkers()[0]->EndOffset());

  EXPECT_EQ(10u, marker_list_->GetMarkers()[1]->StartOffset());
  EXPECT_EQ(15u, marker_list_->GetMarkers()[1]->EndOffset());

  EXPECT_EQ(20u, marker_list_->GetMarkers()[2]->StartOffset());
  EXPECT_EQ(25u, marker_list_->GetMarkers()[2]->EndOffset());

  EXPECT_EQ(30u, marker_list_->GetMarkers()[3]->StartOffset());
  EXPECT_EQ(35u, marker_list_->GetMarkers()[3]->EndOffset());

  EXPECT_EQ(40u, marker_list_->GetMarkers()[4]->StartOffset());
  EXPECT_EQ(45u, marker_list_->GetMarkers()[4]->EndOffset());

  EXPECT_EQ(50u, marker_list_->GetMarkers()[5]->StartOffset());
  EXPECT_EQ(55u, marker_list_->GetMarkers()[5]->EndOffset());

  EXPECT_EQ(60u, marker_list_->GetMarkers()[6]->StartOffset());
  EXPECT_EQ(65u, marker_list_->GetMarkers()[6]->EndOffset());

  EXPECT_EQ(70u, marker_list_->GetMarkers()[7]->StartOffset());
  EXPECT_EQ(75u, marker_list_->GetMarkers()[7]->EndOffset());

  EXPECT_EQ(80u, marker_list_->GetMarkers()[8]->StartOffset());
  EXPECT_EQ(85u, marker_list_->GetMarkers()[8]->EndOffset());

  EXPECT_EQ(90u, marker_list_->GetMarkers()[9]->StartOffset());
  EXPECT_EQ(95u, marker_list_->GetMarkers()[9]->EndOffset());
}

TEST_F(SpellingMarkerListImplTest, AddIntoEmptyList) {
  marker_list_->Add(CreateMarker(5, 10));

  EXPECT_EQ(1u, marker_list_->GetMarkers().size());

  EXPECT_EQ(5u, marker_list_->GetMarkers()[0]->StartOffset());
  EXPECT_EQ(10u, marker_list_->GetMarkers()[0]->EndOffset());
}

TEST_F(SpellingMarkerListImplTest, AddMarkerNonMerging) {
  marker_list_->Add(CreateMarker(5, 10));
  marker_list_->Add(CreateMarker(15, 20));

  EXPECT_EQ(2u, marker_list_->GetMarkers().size());

  EXPECT_EQ(5u, marker_list_->GetMarkers()[0]->StartOffset());
  EXPECT_EQ(10u, marker_list_->GetMarkers()[0]->EndOffset());

  EXPECT_EQ(15u, marker_list_->GetMarkers()[1]->StartOffset());
  EXPECT_EQ(20u, marker_list_->GetMarkers()[1]->EndOffset());
}

TEST_F(SpellingMarkerListImplTest, AddMarkerMergingLater) {
  marker_list_->Add(CreateMarker(5, 10));
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_EQ(1u, marker_list_->GetMarkers().size());

  EXPECT_EQ(0u, marker_list_->GetMarkers()[0]->StartOffset());
  EXPECT_EQ(10u, marker_list_->GetMarkers()[0]->EndOffset());
}

TEST_F(SpellingMarkerListImplTest, AddMarkerMergingEarlier) {
  marker_list_->Add(CreateMarker(0, 5));
  marker_list_->Add(CreateMarker(5, 10));

  EXPECT_EQ(1u, marker_list_->GetMarkers().size());

  EXPECT_EQ(0u, marker_list_->GetMarkers()[0]->StartOffset());
  EXPECT_EQ(10u, marker_list_->GetMarkers()[0]->EndOffset());
}

TEST_F(SpellingMarkerListImplTest, AddMarkerMergingEarlierAndLater) {
  marker_list_->Add(CreateMarker(0, 5));
  marker_list_->Add(CreateMarker(10, 15));
  marker_list_->Add(CreateMarker(5, 10));

  EXPECT_EQ(1u, marker_list_->GetMarkers().size());

  EXPECT_EQ(0u, marker_list_->GetMarkers()[0]->StartOffset());
  EXPECT_EQ(15u, marker_list_->GetMarkers()[0]->EndOffset());
}

TEST_F(SpellingMarkerListImplTest, RemoveMarkersUnderWords) {
  // wor
  marker_list_->Add(CreateMarker(0, 3));

  // word
  marker_list_->Add(CreateMarker(4, 8));

  // words
  marker_list_->Add(CreateMarker(9, 14));

  // word2
  marker_list_->Add(CreateMarker(15, 20));

  marker_list_->RemoveMarkersUnderWords("wor word words word2",
                                        {"word", "word2"});
  EXPECT_EQ(2u, marker_list_->GetMarkers().size());

  EXPECT_EQ(0u, marker_list_->GetMarkers()[0]->StartOffset());
  EXPECT_EQ(3u, marker_list_->GetMarkers()[0]->EndOffset());

  EXPECT_EQ(9u, marker_list_->GetMarkers()[1]->StartOffset());
  EXPECT_EQ(14u, marker_list_->GetMarkers()[1]->EndOffset());
}

}  // namespace
