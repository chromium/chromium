// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cast/encoding/external_video_encoder.h"

#include <stdint.h>

#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::cast {

namespace {

scoped_refptr<VideoFrame> CreateFrame(const uint8_t* y_plane_data,
                                      const gfx::Size& size) {
  scoped_refptr<VideoFrame> result = VideoFrame::CreateFrame(
      PIXEL_FORMAT_I420, size, gfx::Rect(size), size, base::TimeDelta());
  for (int y = 0, y_end = size.height(); y < y_end; ++y) {
    memcpy(result->GetWritableVisibleData(VideoFrame::Plane::kY) +
               y * result->stride(VideoFrame::Plane::kY),
           y_plane_data + y * size.width(), size.width());
  }
  return result;
}

}  // namespace

TEST(QuantizerEstimatorTest, EstimatesForTrivialFrames) {
  QuantizerEstimator qe;

  const gfx::Size frame_size(320, 180);
  const auto black_frame_data =
      std::make_unique<uint8_t[]>(frame_size.GetArea());
  memset(black_frame_data.get(), 0, frame_size.GetArea());
  const scoped_refptr<VideoFrame> black_frame =
      CreateFrame(black_frame_data.get(), frame_size);

  // A solid color frame should always generate a minimum quantizer value (4.0)
  // as a key frame.  If it is provided repeatedly as delta frames, the minimum
  // quantizer value should be repeatedly generated since there is no difference
  // between frames.
  EXPECT_EQ(4.0, qe.EstimateForKeyFrame(*black_frame));
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(4.0, qe.EstimateForDeltaFrame(*black_frame));

  const auto checkerboard_frame_data =
      std::make_unique<uint8_t[]>(frame_size.GetArea());
  for (int i = 0, end = frame_size.GetArea(); i < end; ++i)
    checkerboard_frame_data.get()[i] = (((i % 2) == 0) ? 0 : 255);
  const scoped_refptr<VideoFrame> checkerboard_frame =
      CreateFrame(checkerboard_frame_data.get(), frame_size);

  // Now, introduce a frame with a checkerboard pattern.  Half of the pixels
  // will have a difference of 255, and half will have zero difference.
  // Therefore, the Shannon Entropy should be 1.0 and the resulting quantizer
  // estimate should be ~11.9.
  EXPECT_NEAR(11.9, qe.EstimateForDeltaFrame(*checkerboard_frame), 0.1);

  // Now, introduce a series of frames with "random snow" in them.  Expect this
  // results in high quantizer estimates.
  for (int i = 0; i < 3; ++i) {
    uint32_t rand_seed = 0xdeadbeef + i;
    const auto random_frame_data =
        std::make_unique<uint8_t[]>(frame_size.GetArea());
    for (int j = 0, end = frame_size.GetArea(); j < end; ++j) {
      rand_seed = (1103515245u * rand_seed + 12345u) % (1u << 31);
      random_frame_data.get()[j] = static_cast<uint8_t>(rand_seed & 0xff);
    }
    const scoped_refptr<VideoFrame> random_frame =
        CreateFrame(random_frame_data.get(), frame_size);
    EXPECT_LE(50.0, qe.EstimateForDeltaFrame(*random_frame));
  }
}

}  // namespace media::cast
