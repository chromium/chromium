// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/frame_complexity_estimator.h"

#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::cast {

namespace {

scoped_refptr<VideoFrame> CreateFrame(base::span<const uint8_t> y_plane_data,
                                      const gfx::Size& size) {
  scoped_refptr<VideoFrame> result = VideoFrame::CreateFrame(
      PIXEL_FORMAT_I420, size, gfx::Rect(size), size, base::TimeDelta());

  base::span<uint8_t> result_y_plane =
      result->GetWritableVisiblePlaneData(VideoFrame::Plane::kY);
  const int stride = result->stride(VideoFrame::Plane::kY);

  for (int y = 0, y_end = size.height(); y < y_end; ++y) {
    base::span<uint8_t> dest_row = result_y_plane.subspan(
        static_cast<size_t>(y * stride), static_cast<size_t>(size.width()));
    base::span<const uint8_t> src_row =
        y_plane_data.subspan(static_cast<size_t>(y * size.width()),
                             static_cast<size_t>(size.width()));
    dest_row.copy_from(src_row);
  }
  return result;
}

}  // namespace

TEST(FrameComplexityEstimatorTest, EstimatesForTrivialFrames) {
  FrameComplexityEstimator fce;

  const gfx::Size frame_size(320, 180);
  std::vector<uint8_t> black_frame_data(frame_size.GetArea(), 0);
  const scoped_refptr<VideoFrame> black_frame =
      CreateFrame(black_frame_data, frame_size);

  // A solid color frame should always generate a minimum complexity value (0.0)
  // as a key frame.  If it is provided repeatedly as delta frames, the minimum
  // complexity value should be repeatedly generated since there is no
  // difference between frames.
  EXPECT_EQ(0.0, fce.EstimateForKeyFrame(*black_frame).value());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(0.0, fce.EstimateForDeltaFrame(*black_frame).value());
  }

  std::vector<uint8_t> checkerboard_frame_data(frame_size.GetArea());
  for (int i = 0, end = frame_size.GetArea(); i < end; ++i) {
    checkerboard_frame_data[i] = (((i % 2) == 0) ? 0 : 255);
  }
  const scoped_refptr<VideoFrame> checkerboard_frame =
      CreateFrame(checkerboard_frame_data, frame_size);

  // Now, introduce a frame with a checkerboard pattern.  Half of the pixels
  // will have a difference of 255, and half will have zero difference.
  // Therefore, the Shannon Entropy should be 1.0 and the resulting complexity
  // estimate should be ~0.133 (1.0 / 7.5).
  EXPECT_NEAR(0.133, fce.EstimateForDeltaFrame(*checkerboard_frame).value(),
              0.01);

  // Now, introduce a series of frames with "random snow" in them.  Expect this
  // results in high complexity estimates.
  for (int i = 0; i < 3; ++i) {
    uint32_t rand_seed = 0xdeadbeef + i;
    std::vector<uint8_t> random_frame_data(frame_size.GetArea());
    for (int j = 0, end = frame_size.GetArea(); j < end; ++j) {
      rand_seed = (1103515245u * rand_seed + 12345u) % (1u << 31);
      random_frame_data[j] = static_cast<uint8_t>(rand_seed & 0xff);
    }
    const scoped_refptr<VideoFrame> random_frame =
        CreateFrame(random_frame_data, frame_size);
    // Random noise should push the entropy near ~6.64, giving a
    // normalized complexity of around 0.88.
    EXPECT_GT(fce.EstimateForDeltaFrame(*random_frame).value(), 0.85);
  }
}

}  // namespace media::cast
