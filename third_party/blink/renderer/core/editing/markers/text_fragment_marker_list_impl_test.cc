// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker_list_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker.h"

namespace blink {

class TextFragmentMarkerListImplTest : public testing::Test {
 protected:
  TextFragmentMarkerListImplTest()
      : marker_list_(MakeGarbageCollected<TextFragmentMarkerListImpl>()) {}

  DocumentMarker* CreateMarker(unsigned start_offset, unsigned end_offset) {
    return MakeGarbageCollected<TextFragmentMarker>(start_offset, end_offset);
  }

  Persistent<TextFragmentMarkerListImpl> marker_list_;
};

TEST_F(TextFragmentMarkerListImplTest, MarkerType) {
  EXPECT_EQ(DocumentMarker::kTextFragment, marker_list_->MarkerType());
}

TEST_F(TextFragmentMarkerListImplTest, Add) {
  EXPECT_EQ(0u, marker_list_->GetMarkers().size());

  marker_list_->Add(CreateMarker(0, 1));
  marker_list_->Add(CreateMarker(1, 2));

  EXPECT_EQ(2u, marker_list_->GetMarkers().size());

  EXPECT_EQ(0u, marker_list_->GetMarkers()[0]->StartOffset());
  EXPECT_EQ(1u, marker_list_->GetMarkers()[0]->EndOffset());

  EXPECT_EQ(1u, marker_list_->GetMarkers()[1]->StartOffset());
  EXPECT_EQ(2u, marker_list_->GetMarkers()[1]->EndOffset());
}

TEST_F(TextFragmentMarkerListImplTest, MergeOverlappingMarkersEmpty) {
  marker_list_->MergeOverlappingMarkers();
  EXPECT_TRUE(marker_list_->IsEmpty());
}

TEST_F(TextFragmentMarkerListImplTest, MergeOverlappingMarkersSingleton) {
  marker_list_->Add(CreateMarker(10, 20));
  marker_list_->MergeOverlappingMarkers();
  const HeapVector<Member<DocumentMarker>>& markers =
      marker_list_->GetMarkers();
  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(20u, markers.at(0)->EndOffset());
}

TEST_F(TextFragmentMarkerListImplTest, MergeOverlappingMarkersMultiNames) {
  marker_list_->Add(CreateMarker(10, 15));
  marker_list_->Add(CreateMarker(0, 5));
  marker_list_->Add(CreateMarker(14, 20));
  marker_list_->Add(CreateMarker(12, 14));
  marker_list_->Add(CreateMarker(25, 30));

  marker_list_->MergeOverlappingMarkers();
  const HeapVector<Member<DocumentMarker>>& markers =
      marker_list_->GetMarkers();

  EXPECT_EQ(3u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(10u, markers[1]->StartOffset());
  EXPECT_EQ(20u, markers[1]->EndOffset());

  EXPECT_EQ(25u, markers[2]->StartOffset());
  EXPECT_EQ(30u, markers[2]->EndOffset());
}

}  // namespace blink
