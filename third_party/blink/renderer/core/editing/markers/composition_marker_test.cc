// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"

#include "testing/gtest/include/gtest/gtest.h"

using ui::mojom::ImeTextSpanThickness;
using ui::mojom::ImeTextSpanUnderlineStyle;

namespace blink {

class CompositionMarkerTest : public testing::Test {};

TEST_F(CompositionMarkerTest, MarkerType) {
  DocumentMarker* marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kTransparent, ImeTextSpanThickness::kNone,
      ImeTextSpanUnderlineStyle::kNone, Color::kTransparent,
      Color::kTransparent);
  EXPECT_EQ(DocumentMarker::kComposition, marker->GetType());
}

TEST_F(CompositionMarkerTest, IsStyleableMarker) {
  DocumentMarker* marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kTransparent, ImeTextSpanThickness::kNone,
      ImeTextSpanUnderlineStyle::kNone, Color::kTransparent,
      Color::kTransparent);
  EXPECT_TRUE(IsStyleableMarker(*marker));
}

TEST_F(CompositionMarkerTest, ConstructorAndGetters) {
  CompositionMarker* marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kGray);
  EXPECT_EQ(Color::kDarkGray, marker->UnderlineColor());
  EXPECT_TRUE(marker->HasThicknessThin());
  EXPECT_EQ(ImeTextSpanUnderlineStyle::kSolid, marker->UnderlineStyle());
  EXPECT_EQ(Color::kTransparent, marker->TextColor());
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());

  CompositionMarker* thick_marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThick,
      ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kGray);
  EXPECT_TRUE(thick_marker->HasThicknessThick());
  EXPECT_EQ(ImeTextSpanUnderlineStyle::kSolid, marker->UnderlineStyle());
  EXPECT_EQ(Color::kTransparent, marker->TextColor());
}

TEST_F(CompositionMarkerTest, UnderlineStyleDottedAndGrayText) {
  CompositionMarker* marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kDot, Color::kGray, Color::kGray);
  EXPECT_EQ(Color::kDarkGray, marker->UnderlineColor());
  EXPECT_TRUE(marker->HasThicknessThin());
  EXPECT_EQ(ImeTextSpanUnderlineStyle::kDot, marker->UnderlineStyle());
  EXPECT_EQ(Color::kGray, marker->TextColor());
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());

  CompositionMarker* thick_marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThick,
      ImeTextSpanUnderlineStyle::kDot, Color::kGray, Color::kGray);
  EXPECT_TRUE(thick_marker->HasThicknessThick());
  EXPECT_EQ(ImeTextSpanUnderlineStyle::kDot, marker->UnderlineStyle());
  EXPECT_EQ(Color::kGray, marker->TextColor());
}

TEST_F(CompositionMarkerTest, UnderlineStyleDashed) {
  CompositionMarker* marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kDash, Color::kTransparent, Color::kGray);
  EXPECT_EQ(Color::kDarkGray, marker->UnderlineColor());
  EXPECT_TRUE(marker->HasThicknessThin());
  EXPECT_EQ(ImeTextSpanUnderlineStyle::kDash, marker->UnderlineStyle());
  EXPECT_EQ(Color::kTransparent, marker->TextColor());
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());

  CompositionMarker* thick_marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThick,
      ImeTextSpanUnderlineStyle::kDash, Color::kTransparent, Color::kGray);
  EXPECT_TRUE(thick_marker->HasThicknessThick());
  EXPECT_EQ(ImeTextSpanUnderlineStyle::kDash, marker->UnderlineStyle());
  EXPECT_EQ(Color::kTransparent, marker->TextColor());
}

TEST_F(CompositionMarkerTest, UnderlineStyleSquiggled) {
  CompositionMarker* marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSquiggle, Color::kTransparent, Color::kGray);
  EXPECT_EQ(Color::kDarkGray, marker->UnderlineColor());
  EXPECT_TRUE(marker->HasThicknessThin());
  EXPECT_EQ(ImeTextSpanUnderlineStyle::kSquiggle, marker->UnderlineStyle());
  EXPECT_EQ(Color::kTransparent, marker->TextColor());
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());

  CompositionMarker* thick_marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThick,
      ImeTextSpanUnderlineStyle::kSquiggle, Color::kTransparent, Color::kGray);
  EXPECT_TRUE(thick_marker->HasThicknessThick());
  EXPECT_EQ(ImeTextSpanUnderlineStyle::kSquiggle, marker->UnderlineStyle());
  EXPECT_EQ(Color::kTransparent, marker->TextColor());
}

}  // namespace blink
