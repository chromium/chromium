// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/active_suggestion_marker.h"

#include "testing/gtest/include/gtest/gtest.h"

using ui::mojom::ImeTextSpanThickness;

namespace blink {

class ActiveSuggestionMarkerTest : public testing::Test {};

TEST_F(ActiveSuggestionMarkerTest, MarkerType) {
  DocumentMarker* marker = MakeGarbageCollected<ActiveSuggestionMarker>(
      0, 1, Color::kTransparent, ImeTextSpanThickness::kNone,
      Color::kTransparent);
  EXPECT_EQ(DocumentMarker::kActiveSuggestion, marker->GetType());
}

TEST_F(ActiveSuggestionMarkerTest, IsStyleableMarker) {
  DocumentMarker* marker = MakeGarbageCollected<ActiveSuggestionMarker>(
      0, 1, Color::kTransparent, ImeTextSpanThickness::kNone,
      Color::kTransparent);
  EXPECT_TRUE(IsStyleableMarker(*marker));
}

TEST_F(ActiveSuggestionMarkerTest, ConstructorAndGetters) {
  ActiveSuggestionMarker* marker = MakeGarbageCollected<ActiveSuggestionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThin, Color::kGray);
  EXPECT_EQ(Color::kDarkGray, marker->UnderlineColor());
  EXPECT_FALSE(marker->HasThicknessThick());
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());

  ActiveSuggestionMarker* thick_marker =
      MakeGarbageCollected<ActiveSuggestionMarker>(
          0, 1, Color::kDarkGray, ImeTextSpanThickness::kThick, Color::kGray);
  EXPECT_EQ(true, thick_marker->HasThicknessThick());
}

}  // namespace blink
