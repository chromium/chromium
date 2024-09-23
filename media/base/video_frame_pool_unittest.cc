// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <tuple>

#include "base/bits.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/video_frame_pool.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class VideoFramePoolTest
    : public ::testing::TestWithParam<std::tuple<VideoPixelFormat, gfx::Size>> {
 public:
  VideoFramePoolTest() : pool_(new VideoFramePool()) {
    // Seed test clock with some dummy non-zero value to avoid confusion with
    // empty base::TimeTicks values.
    test_clock_.Advance(base::Seconds(1234));
    pool_->SetTickClockForTesting(&test_clock_);
  }

  scoped_refptr<VideoFrame> CreateFrame(VideoPixelFormat format,
                                        const gfx::Size& coded_size,
                                        int timestamp_ms) {
    gfx::Rect visible_rect(coded_size);
    gfx::Size natural_size(coded_size);

    scoped_refptr<VideoFrame> frame =
        pool_->CreateFrame(format, coded_size, visible_rect, natural_size,
                           base::Milliseconds(timestamp_ms));
    EXPECT_EQ(format, frame->format());
    EXPECT_EQ(base::Milliseconds(timestamp_ms), frame->timestamp());
    if (format == PIXEL_FORMAT_ARGB) {
      EXPECT_EQ(coded_size, frame->coded_size());
    } else {
      const gfx::Size adjusted(
          base::bits::AlignUpDeprecatedDoNotUse(coded_size.width(), 2),
          base::bits::AlignUpDeprecatedDoNotUse(coded_size.height(), 2));
      EXPECT_EQ(adjusted, frame->coded_size());
    }
    EXPECT_EQ(visible_rect, frame->visible_rect());
    EXPECT_EQ(natural_size, frame->natural_size());

    return frame;
  }

  void CheckPoolSize(size_t size) const {
    EXPECT_EQ(size, pool_->GetPoolSizeForTesting());
  }

 protected:
  base::SimpleTestTickClock test_clock_;
  std::unique_ptr<VideoFramePool> pool_;
};

TEST_P(VideoFramePoolTest, FrameInitializedAndZeroed) {
  scoped_refptr<VideoFrame> frame =
      CreateFrame(std::get<0>(GetParam()), std::get<1>(GetParam()), 10);

  // Verify that frame is initialized with zeros.
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame->format()); ++i)
    EXPECT_EQ(0, frame->data(i)[0]);
}

TEST_P(VideoFramePoolTest, FrameReuse) {
  scoped_refptr<VideoFrame> frame =
      CreateFrame(std::get<0>(GetParam()), std::get<1>(GetParam()), 10);
  const uint8_t* old_y_data = frame->data(VideoFrame::Plane::kY);

  // Clear frame reference to return the frame to the pool.
  frame.reset();

  // Verify that the next frame from the pool uses the same memory.
  scoped_refptr<VideoFrame> new_frame =
      CreateFrame(std::get<0>(GetParam()), std::get<1>(GetParam()), 20);
  EXPECT_EQ(old_y_data, new_frame->data(VideoFrame::Plane::kY));
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoFramePoolTest,
                         ::testing::Combine(testing::Values(PIXEL_FORMAT_I420,
                                                            PIXEL_FORMAT_NV12,
                                                            PIXEL_FORMAT_ARGB),
                                            testing::Values(gfx::Size(320, 240),
                                                            gfx::Size(321, 240),
                                                            gfx::Size(320, 241),
                                                            gfx::Size(321,
                                                                      241))));

TEST_F(VideoFramePoolTest, SimpleFormatChange) {
  scoped_refptr<VideoFrame> frame_a =
      CreateFrame(PIXEL_FORMAT_I420, gfx::Size(320, 240), 10);
  scoped_refptr<VideoFrame> frame_b =
      CreateFrame(PIXEL_FORMAT_I420, gfx::Size(320, 240), 10);

  // Clear frame references to return the frames to the pool.
  frame_a.reset();
  frame_b.reset();

  // Verify that both frames are in the pool.
  CheckPoolSize(2u);

  // Verify that requesting a frame with a different format causes the pool
  // to get drained.
  scoped_refptr<VideoFrame> new_frame =
      CreateFrame(PIXEL_FORMAT_I420A, gfx::Size(320, 240), 10);
  CheckPoolSize(0u);
}

TEST_F(VideoFramePoolTest, FrameValidAfterPoolDestruction) {
  scoped_refptr<VideoFrame> frame =
      CreateFrame(PIXEL_FORMAT_I420, gfx::Size(320, 240), 10);

  // Destroy the pool.
  pool_.reset();

  // Write to the Y plane. The memory tools should detect a
  // use-after-free if the storage was actually removed by pool destruction.
  memset(frame->writable_data(VideoFrame::Plane::kY), 0xff,
         frame->rows(VideoFrame::Plane::kY) *
             frame->stride(VideoFrame::Plane::kY));
}

TEST_F(VideoFramePoolTest, StaleFramesAreExpired) {
  scoped_refptr<VideoFrame> frame_1 =
      CreateFrame(PIXEL_FORMAT_I420, gfx::Size(320, 240), 10);
  scoped_refptr<VideoFrame> frame_2 =
      CreateFrame(PIXEL_FORMAT_I420, gfx::Size(320, 240), 10);
  EXPECT_NE(frame_1.get(), frame_2.get());
  CheckPoolSize(0u);

  // Drop frame and verify that resources are still available for reuse.
  frame_1 = nullptr;
  CheckPoolSize(1u);

  // Advance clock far enough to hit stale timer; ensure only frame_1 has its
  // resources released.
  test_clock_.Advance(base::Minutes(1));
  frame_2 = nullptr;
  CheckPoolSize(1u);
}

}  // namespace media
