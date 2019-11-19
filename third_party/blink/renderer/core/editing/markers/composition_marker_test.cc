// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"

#include "testing/gtest/include/gtest/gtest.h"

using ui::mojom::ImeTextSpanThickness;

namespace blink {

class CompositionMarkerTest : public testing::Test {};

TEST_F(CompositionMarkerTest, MarkerType) {
  DocumentMarker* marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kTransparent, ImeTextSpanThickness::kNone,
      Color::kTransparent);
  EXPECT_EQ(DocumentMarker::kComposition, marker->GetType());
}

TEST_F(CompositionMarkerTest, IsStyleableMarker) {
  DocumentMarker* marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kTransparent, ImeTextSpanThickness::kNone,
      Color::kTransparent);
  EXPECT_TRUE(IsStyleableMarker(*marker));
}

TEST_F(CompositionMarkerTest, ConstructorAndGetters) {
  CompositionMarker* marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThin, Color::kGray);
  EXPECT_EQ(Color::kDarkGray, marker->UnderlineColor());
  EXPECT_TRUE(marker->HasThicknessThin());
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());

  CompositionMarker* thick_marker = MakeGarbageCollected<CompositionMarker>(
      0, 1, Color::kDarkGray, ImeTextSpanThickness::kThick, Color::kGray);
  EXPECT_TRUE(thick_marker->HasThicknessThick());
}

}  // namespace blink
