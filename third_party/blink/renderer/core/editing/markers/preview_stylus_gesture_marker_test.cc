// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/preview_stylus_gesture_marker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PreviewStylusGestureMarkerTest : public testing::Test {};

TEST_F(PreviewStylusGestureMarkerTest, MarkerType) {
  DocumentMarker* marker = MakeGarbageCollected<PreviewStylusGestureMarker>(
      0, 1, Color::kTransparent);
  EXPECT_EQ(DocumentMarker::kPreviewStylusGesture, marker->GetType());
}

TEST_F(PreviewStylusGestureMarkerTest, IsStyleableMarker) {
  DocumentMarker* marker = MakeGarbageCollected<PreviewStylusGestureMarker>(
      0, 1, Color::kTransparent);
  EXPECT_TRUE(IsStyleableMarker(*marker));
}

TEST_F(PreviewStylusGestureMarkerTest, BackgroundColor) {
  PreviewStylusGestureMarker* marker =
      MakeGarbageCollected<PreviewStylusGestureMarker>(0, 1, Color::kGray);
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());
}

TEST_F(PreviewStylusGestureMarkerTest, Defaults) {
  PreviewStylusGestureMarker* marker =
      MakeGarbageCollected<PreviewStylusGestureMarker>(0, 1, Color::kGray);
  EXPECT_EQ(Color::kTransparent, marker->UnderlineColor());
  EXPECT_EQ(true, marker->HasThicknessNone());
  EXPECT_EQ(ui::mojom::blink::ImeTextSpanUnderlineStyle::kNone,
            marker->UnderlineStyle());
  EXPECT_EQ(Color::kTransparent, marker->TextColor());
}
}  // namespace blink
