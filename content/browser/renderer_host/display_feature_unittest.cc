// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/display_feature.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

class DisplayFeatureTest : public testing::Test {};

TEST_F(DisplayFeatureTest, ViewportSegmentsWithEmptyTopBrowserControls) {
  DisplayFeature::ParamErrorEnum error;
  std::optional<DisplayFeature> feature = DisplayFeature::Create(
      DisplayFeature::Orientation::kHorizontal, /*offset*/ 200,
      /*mask_length*/ 0, /*screen_width*/ 400,
      /*screen_height*/ 400, &error);
  EXPECT_TRUE(feature);
  std::vector<gfx::Rect> segments = feature->ComputeViewportSegments(
      gfx::Size(400, 400), /*top_controls_height*/ 0);
  EXPECT_EQ(segments.size(), 2u);
  EXPECT_EQ(gfx::Rect(0, 0, 400, 200), segments[0]);
  EXPECT_EQ(gfx::Rect(0, 200, 400, 200), segments[1]);

  feature->orientation = DisplayFeature::Orientation::kVertical;
  segments = feature->ComputeViewportSegments(gfx::Size(400, 400),
                                              /*top_controls_height*/ 0);
  EXPECT_EQ(gfx::Rect(0, 0, 200, 400), segments[0]);
  EXPECT_EQ(gfx::Rect(200, 0, 200, 400), segments[1]);
}

TEST_F(DisplayFeatureTest, ViewportSegmentsWithTopBrowserControls) {
  DisplayFeature::ParamErrorEnum error;
  std::optional<DisplayFeature> feature = DisplayFeature::Create(
      DisplayFeature::Orientation::kHorizontal, /*offset*/ 200,
      /*mask_length*/ 0, /*screen_width*/ 400,
      /*screen_height*/ 400, &error);
  EXPECT_TRUE(feature);
  std::vector<gfx::Rect> segments = feature->ComputeViewportSegments(
      gfx::Size(400, 400), /*top_controls_height*/ 100);
  EXPECT_EQ(segments.size(), 2u);
  EXPECT_EQ(gfx::Rect(0, 0, 400, 100), segments[0]);
  EXPECT_EQ(gfx::Rect(0, 100, 400, 300), segments[1]);

  feature->orientation = DisplayFeature::Orientation::kVertical;
  segments = feature->ComputeViewportSegments(gfx::Size(400, 400),
                                              /*top_controls_height*/ 100);
  EXPECT_EQ(gfx::Rect(0, 0, 200, 400), segments[0]);
  EXPECT_EQ(gfx::Rect(200, 0, 200, 400), segments[1]);
}

TEST_F(DisplayFeatureTest, ViewportSegmentsWithTooBigBrowserControls) {
  DisplayFeature::ParamErrorEnum error;
  std::optional<DisplayFeature> feature = DisplayFeature::Create(
      DisplayFeature::Orientation::kHorizontal, /*offset*/ 200,
      /*mask_length*/ 0, /*screen_width*/ 400,
      /*screen_height*/ 400, &error);
  EXPECT_TRUE(feature);
  std::vector<gfx::Rect> segments = feature->ComputeViewportSegments(
      gfx::Size(400, 400), /*top_controls_height*/ 400);
  EXPECT_TRUE(segments.empty());

  feature->orientation = DisplayFeature::Orientation::kVertical;
  segments = feature->ComputeViewportSegments(gfx::Size(400, 400),
                                              /*top_controls_height*/ 400);
  EXPECT_EQ(gfx::Rect(0, 0, 200, 400), segments[0]);
  EXPECT_EQ(gfx::Rect(200, 0, 200, 400), segments[1]);
}

}  // namespace content
