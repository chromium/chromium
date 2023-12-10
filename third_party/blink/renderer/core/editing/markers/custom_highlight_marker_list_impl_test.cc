// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker_list_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class CustomHighlightMarkerListImplTest : public testing::Test {
 protected:
  CustomHighlightMarkerListImplTest()
      : marker_list_(MakeGarbageCollected<CustomHighlightMarkerListImpl>()) {}

  CustomHighlightMarker* CreateMarker(unsigned start_offset,
                                      unsigned end_offset,
                                      const AtomicString& name) {
    HeapVector<Member<AbstractRange>> dummy_ranges;
    return MakeGarbageCollected<CustomHighlightMarker>(
        start_offset, end_offset, name,
        MakeGarbageCollected<Highlight>(dummy_ranges));
  }

  Persistent<CustomHighlightMarkerListImpl> marker_list_;
};

TEST_F(CustomHighlightMarkerListImplTest, MarkerType) {
  EXPECT_EQ(DocumentMarker::kCustomHighlight, marker_list_->MarkerType());
}

TEST_F(CustomHighlightMarkerListImplTest, MergeOverlappingMarkersEmpty) {
  marker_list_->MergeOverlappingMarkers();
  EXPECT_TRUE(marker_list_->IsEmpty());
}

TEST_F(CustomHighlightMarkerListImplTest, MergeOverlappingMarkersSingleton) {
  marker_list_->Add(CreateMarker(10, 20, AtomicString("A")));
  marker_list_->MergeOverlappingMarkers();
  const HeapVector<Member<DocumentMarker>>& markers =
      marker_list_->GetMarkers();
  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(20u, markers.at(0)->EndOffset());
}

TEST_F(CustomHighlightMarkerListImplTest, MergeOverlappingMarkersMultiNames) {
  marker_list_->Add(CreateMarker(10, 15, AtomicString("A")));
  marker_list_->Add(CreateMarker(0, 5, AtomicString("A")));
  marker_list_->Add(CreateMarker(14, 20, AtomicString("A")));
  marker_list_->Add(CreateMarker(12, 14, AtomicString("A")));
  marker_list_->Add(CreateMarker(25, 30, AtomicString("A")));

  marker_list_->Add(CreateMarker(20, 30, AtomicString("B")));
  marker_list_->Add(CreateMarker(15, 30, AtomicString("B")));
  marker_list_->Add(CreateMarker(0, 15, AtomicString("B")));
  marker_list_->Add(CreateMarker(0, 5, AtomicString("B")));

  marker_list_->MergeOverlappingMarkers();
  const HeapVector<Member<DocumentMarker>>& markers =
      marker_list_->GetMarkers();

  EXPECT_EQ(5u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());
  EXPECT_EQ("A", To<CustomHighlightMarker>(markers[0].Get())
                     ->GetHighlightName()
                     .GetString());
  EXPECT_EQ(0u, markers[1]->StartOffset());
  EXPECT_EQ(15u, markers[1]->EndOffset());
  EXPECT_EQ("B", To<CustomHighlightMarker>(markers[1].Get())
                     ->GetHighlightName()
                     .GetString());
  EXPECT_EQ(10u, markers[2]->StartOffset());
  EXPECT_EQ(20u, markers[2]->EndOffset());
  EXPECT_EQ("A", To<CustomHighlightMarker>(markers[2].Get())
                     ->GetHighlightName()
                     .GetString());
  EXPECT_EQ(15u, markers[3]->StartOffset());
  EXPECT_EQ(30u, markers[3]->EndOffset());
  EXPECT_EQ("B", To<CustomHighlightMarker>(markers[3].Get())
                     ->GetHighlightName()
                     .GetString());
  EXPECT_EQ(25u, markers[4]->StartOffset());
  EXPECT_EQ(30u, markers[4]->EndOffset());
  EXPECT_EQ("A", To<CustomHighlightMarker>(markers[4].Get())
                     ->GetHighlightName()
                     .GetString());
}

}  // namespace blink
