// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_converter.h"

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

constexpr gfx::Size kCodedSize(128, 128);
constexpr gfx::Rect kVisibleRect(64, 64, 64, 64);

gfx::Size SelectDestSize(bool scaled) {
  return scaled ? gfx::ScaleToRoundedSize(kCodedSize, 0.5) : kCodedSize;
}

gfx::Rect SelectDestRect(bool scaled) {
  return scaled ? gfx::ScaleToRoundedRect(kVisibleRect, 0.5, 0.5)
                : kVisibleRect;
}

bool IsConversionSupported(VideoPixelFormat src, VideoPixelFormat dest) {
  if (!IsOpaque(dest) && IsOpaque(src)) {
    // We can't make an alpha channel from nothing.
    return false;
  }

  switch (src) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_ARGB:
      break;

    default:
      return false;
  }

  switch (dest) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
      break;

    default:
      return false;
  }

  return true;
}

}  // namespace

using TestParams = testing::tuple<VideoPixelFormat, VideoPixelFormat, bool>;
class VideoFrameConverterTest
    : public testing::Test,
      public ::testing::WithParamInterface<TestParams> {
 public:
  VideoFrameConverterTest()
      : src_format_(testing::get<0>(GetParam())),
        dest_format_(testing::get<1>(GetParam())),
        dest_coded_size_(SelectDestSize(testing::get<2>(GetParam()))),
        dest_visible_rect_(SelectDestRect(testing::get<2>(GetParam()))) {}

 protected:
  const VideoPixelFormat src_format_;
  const VideoPixelFormat dest_format_;
  const gfx::Size dest_coded_size_;
  const gfx::Rect dest_visible_rect_;
  VideoFrameConverter converter_;
};

TEST_P(VideoFrameConverterTest, ConvertAndScale) {
  // Zero initialize so coded size regions are all zero.
  auto src_frame = VideoFrame::CreateZeroInitializedFrame(
      src_format_, kCodedSize, kVisibleRect, kVisibleRect.size(),
      base::TimeDelta());
  auto dest_frame = VideoFrame::CreateZeroInitializedFrame(
      dest_format_, dest_coded_size_, dest_visible_rect_,
      dest_visible_rect_.size(), base::TimeDelta());

  // This test doesn't test pixel correctness, just that supported operations
  // execute without failure.
  EXPECT_EQ(IsConversionSupported(src_format_, dest_format_),
            converter_.ConvertAndScale(*src_frame, *dest_frame).is_ok());
}

INSTANTIATE_TEST_SUITE_P(,
                         VideoFrameConverterTest,
                         testing::Combine(testing::Values(PIXEL_FORMAT_XBGR,
                                                          PIXEL_FORMAT_XRGB,
                                                          PIXEL_FORMAT_ABGR,
                                                          PIXEL_FORMAT_ARGB,
                                                          PIXEL_FORMAT_I420,
                                                          PIXEL_FORMAT_I420A,
                                                          PIXEL_FORMAT_NV12,
                                                          PIXEL_FORMAT_NV12A),
                                          testing::Values(PIXEL_FORMAT_XBGR,
                                                          PIXEL_FORMAT_XRGB,
                                                          PIXEL_FORMAT_ABGR,
                                                          PIXEL_FORMAT_ARGB,
                                                          PIXEL_FORMAT_I420,
                                                          PIXEL_FORMAT_I420A,
                                                          PIXEL_FORMAT_NV12,
                                                          PIXEL_FORMAT_NV12A),
                                          testing::Bool()));

}  // namespace media
