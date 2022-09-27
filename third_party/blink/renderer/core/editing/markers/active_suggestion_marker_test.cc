// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/active_suggestion_marker.h"

#include "testing/gtest/include/gtest/gtest.h"

using ui::mojom::ImeTextSpanThickness;
using ui::mojom::ImeTextSpanUnderlineStyle;

namespace blink {

class ActiveSuggestionMarkerTest : public testing::Test {};

TEST_F(ActiveSuggestionMarkerTest, MarkerType) {
  DocumentMarker* marker = MakeGarbageCollected<ActiveSuggestionMarker>(
      0, 1, Color::kTransparent, ImeTextSpanThickness::kNone,
      ImeTextSpanUnderlineStyle::kNone, Color::kTransparent,
      Color::kTransparent);
  EXPECT_EQ(DocumentMarker::kActiveSuggestion, marker->GetType());
}

TEST_F(ActiveSuggestionMarkerTest, IsStyleableMarker) {
  DocumentMarker* marker = MakeGarbageCollected<ActiveSuggestionMarker>(
      0, 1, Color::kTransparent, ImeTextSpanThickness::kNone,
      ImeTextSpanUnderlineStyle::kNone, Color::kTransparent,
      Color::kTransparent);
  EXPECT_TRUE(IsStyleableMarker(*marker));
}

TEST_F(ActiveSuggestionMarkerTest, ConstructorAndGetters) {
  ActiveSuggestionMarker* marker = MakeGarbageCollected<ActiveSuggestionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kGray);
  EXPECT_EQ(Color::kDarkGray, marker->UnderlineColor());
  EXPECT_FALSE(marker->HasThicknessThick());
  EXPECT_EQ(ImeTextSpanUnderlineStyle::kSolid, marker->UnderlineStyle());
  EXPECT_EQ(Color::kTransparent, marker->TextColor());
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());

  ActiveSuggestionMarker* thick_marker =
      MakeGarbageCollected<ActiveSuggestionMarker>(
          0, 1, Color::kDarkGray, ImeTextSpanThickness::kThick,
          ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kGray);
  EXPECT_EQ(true, thick_marker->HasThicknessThick());
  EXPECT_EQ(ImeTextSpanUnderlineStyle::kSolid, marker->UnderlineStyle());
  EXPECT_EQ(Color::kTransparent, marker->TextColor());
}

}  // namespace blink
