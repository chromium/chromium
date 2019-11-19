// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class TextFragmentMarkerListImplTest : public EditingTestBase {
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

}  // namespace blink
