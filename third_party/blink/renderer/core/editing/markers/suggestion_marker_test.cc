// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"

namespace blink {

class SuggestionMarkerTest : public testing::Test {};

TEST_F(SuggestionMarkerTest, MarkerType) {
  DocumentMarker* marker = MakeGarbageCollected<SuggestionMarker>(
      0, 1, SuggestionMarkerProperties());
  EXPECT_EQ(DocumentMarker::kSuggestion, marker->GetType());
}

TEST_F(SuggestionMarkerTest, IsStyleableMarker) {
  DocumentMarker* marker = MakeGarbageCollected<SuggestionMarker>(
      0, 1, SuggestionMarkerProperties());
  EXPECT_TRUE(IsStyleableMarker(*marker));
}

TEST_F(SuggestionMarkerTest, ConstructorAndGetters) {
  Vector<String> suggestions = {"this", "that"};
  SuggestionMarker* marker = MakeGarbageCollected<SuggestionMarker>(
      0, 1,
      SuggestionMarkerProperties::Builder()
          .SetType(SuggestionMarker::SuggestionType::kNotMisspelling)
          .SetSuggestions(suggestions)
          .SetHighlightColor(Color::kTransparent)
          .SetUnderlineColor(Color::kDarkGray)
          .SetThickness(ui::mojom::ImeTextSpanThickness::kThin)
          .SetBackgroundColor(Color::kGray)
          .Build());
  EXPECT_EQ(suggestions, marker->Suggestions());
  EXPECT_FALSE(marker->IsMisspelling());
  EXPECT_EQ(Color::kTransparent, marker->SuggestionHighlightColor());
  EXPECT_EQ(Color::kDarkGray, marker->UnderlineColor());
  EXPECT_TRUE(marker->HasThicknessThin());
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());

  SuggestionMarker* marker2 = MakeGarbageCollected<SuggestionMarker>(
      0, 1,
      SuggestionMarkerProperties::Builder()
          .SetType(SuggestionMarker::SuggestionType::kMisspelling)
          .SetHighlightColor(Color::kBlack)
          .SetThickness(ui::mojom::ImeTextSpanThickness::kThick)
          .Build());
  EXPECT_TRUE(marker2->HasThicknessThick());
  EXPECT_TRUE(marker2->IsMisspelling());
  EXPECT_EQ(marker2->SuggestionHighlightColor(), Color::kBlack);
}

TEST_F(SuggestionMarkerTest, SetSuggestion) {
  Vector<String> suggestions = {"this", "that"};
  SuggestionMarker* marker = MakeGarbageCollected<SuggestionMarker>(
      0, 1,
      SuggestionMarkerProperties::Builder()
          .SetSuggestions(suggestions)
          .Build());

  marker->SetSuggestion(1, "these");

  EXPECT_EQ(2u, marker->Suggestions().size());

  EXPECT_EQ("this", marker->Suggestions()[0]);
  EXPECT_EQ("these", marker->Suggestions()[1]);
}

}  // namespace blink
