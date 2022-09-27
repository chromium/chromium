// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/active_suggestion_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/active_suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class ActiveSuggestionMarkerListImplTest : public EditingTestBase {
 protected:
  ActiveSuggestionMarkerListImplTest()
      : marker_list_(MakeGarbageCollected<ActiveSuggestionMarkerListImpl>()) {}

  DocumentMarker* CreateMarker(unsigned start_offset, unsigned end_offset) {
    return MakeGarbageCollected<ActiveSuggestionMarker>(
        start_offset, end_offset, Color::kTransparent,
        ui::mojom::ImeTextSpanThickness::kThin,
        ui::mojom::ImeTextSpanUnderlineStyle::kSolid, Color::kBlack,
        Color::kBlack);
  }

  Persistent<ActiveSuggestionMarkerListImpl> marker_list_;
};

// ActiveSuggestionMarkerListImpl shouldn't merge markers with touching
// endpoints
TEST_F(ActiveSuggestionMarkerListImplTest, Add) {
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
