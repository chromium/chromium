// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video_capture_types.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

using SuggestedConstraints = VideoCaptureParams::SuggestedConstraints;

void ExpectEqualConstraints(const SuggestedConstraints& a,
                            const SuggestedConstraints& b) {
  EXPECT_EQ(a.min_frame_size, b.min_frame_size);
  EXPECT_EQ(a.max_frame_size, b.max_frame_size);
  EXPECT_EQ(a.fixed_aspect_ratio, b.fixed_aspect_ratio);
}

}  // namespace

TEST(VideoCaptureTypesTest, SuggestsConstraints) {
  VideoCaptureParams params;

  // For empty/invalid VideoCaptureParams, results will be empty sizes.
  ExpectEqualConstraints(
      SuggestedConstraints({gfx::Size(), gfx::Size(), false}),
      params.SuggestConstraints());

  // Set an impractically small frame size, and confirm the same suggestion as
  // if the params were empty.
  params.requested_format.frame_size = gfx::Size(1, 1);
  ExpectEqualConstraints(
      SuggestedConstraints({gfx::Size(), gfx::Size(), false}),
      params.SuggestConstraints());

  // Test: Fixed 1080p resolution.
  params.requested_format.frame_size = gfx::Size(1920, 1080);
  params.resolution_change_policy = ResolutionChangePolicy::FIXED_RESOLUTION;
  ExpectEqualConstraints(SuggestedConstraints({gfx::Size(1920, 1080),
                                               gfx::Size(1920, 1080), false}),
                         params.SuggestConstraints());

  // Test: Max 1080p resolution, fixed aspect ratio.
  params.requested_format.frame_size = gfx::Size(1920, 1080);
  params.resolution_change_policy = ResolutionChangePolicy::FIXED_ASPECT_RATIO;
  ExpectEqualConstraints(
      SuggestedConstraints({gfx::Size(320, 180), gfx::Size(1920, 1080), true}),
      params.SuggestConstraints());

  // Test: Max 1080p resolution, any aspect ratio.
  params.requested_format.frame_size = gfx::Size(1920, 1080);
  params.resolution_change_policy = ResolutionChangePolicy::ANY_WITHIN_LIMIT;
  ExpectEqualConstraints(
      SuggestedConstraints({gfx::Size(2, 2), gfx::Size(1920, 1080), false}),
      params.SuggestConstraints());

  // Test: Odd-valued resolution, fixed aspect ratio.
  params.requested_format.frame_size = gfx::Size(999, 777);
  params.resolution_change_policy = ResolutionChangePolicy::FIXED_ASPECT_RATIO;
  ExpectEqualConstraints(
      SuggestedConstraints({gfx::Size(232, 180), gfx::Size(998, 776), true}),
      params.SuggestConstraints());

  // Test: Max resolution less the hard-coded 180-line minimum, fixed aspect.
  params.requested_format.frame_size = gfx::Size(160, 90);
  params.resolution_change_policy = ResolutionChangePolicy::FIXED_ASPECT_RATIO;
  ExpectEqualConstraints(
      SuggestedConstraints({gfx::Size(160, 90), gfx::Size(160, 90), true}),
      params.SuggestConstraints());
}

}  // namespace media
