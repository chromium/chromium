// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/video_frame_converter.h"

#include "base/logging.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

// Use 64x64 for visible size ensure U,V planes are > 8x8 since libyuv can't
// calculate the SSIM for smaller sizes.
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
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A:
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
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
      break;

    default:
      return false;
  }

  return true;
}

// SSIM/PSNR mismatch debugging function. Writes a VideoFrame into a packed
// plane by plane layout that can be used with ffplay or ffmpeg to view the raw
// output or convert to png. E.g.,
//
//   ffplay -f rawvideo -pixel_format yuv444p -video_size 64x64 -framerate 1
//       expected_PIXEL_FORMAT_I444_64x64.bin
//
//   ffmpeg -f rawvideo -pixel_format yuv444p -video_size 64x64 -framerate 1
//       -i expected_PIXEL_FORMAT_I444_64x64.bin expected.png
//
[[maybe_unused]] void DumpFrame(const VideoFrame& frame, const char* prefix) {
  FILE* f =
      fopen(base::StringPrintf("/tmp/%s_%s_%s.bin", prefix,
                               VideoPixelFormatToString(frame.format()).c_str(),
                               frame.visible_rect().size().ToString().c_str())
                .c_str(),
            "wc");
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame.format()); ++i) {
    auto plane_size =
        VideoFrame::PlaneSize(frame.format(), i, frame.visible_rect().size());
    for (int y = 0; y < plane_size.height(); ++y) {
      fwrite(frame.visible_data(i) + y * frame.stride(i), 1, plane_size.width(),
             f);
    }
  }
  fclose(f);
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

  FillFourColors(*src_frame);

  if (!IsConversionSupported(src_format_, dest_format_)) {
    EXPECT_FALSE(converter_.ConvertAndScale(*src_frame, *dest_frame).is_ok());
    return;
  }

  ASSERT_TRUE(converter_.ConvertAndScale(*src_frame, *dest_frame).is_ok());

  // Recreate the ideal frame at the destination size.
  DCHECK(IsYuvPlanar(dest_format_));
  auto expected_dest_frame = VideoFrame::CreateZeroInitializedFrame(
      dest_format_, dest_coded_size_, dest_visible_rect_,
      dest_visible_rect_.size(), base::TimeDelta());
  FillFourColors(*expected_dest_frame);

  auto dest_visible_size = expected_dest_frame->visible_rect().size();
  for (size_t i = 0; i < VideoFrame::NumPlanes(expected_dest_frame->format());
       ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "%s -> %s, plane=%d, (%s -> %s)",
        VideoPixelFormatToString(src_format_).c_str(),
        VideoPixelFormatToString(dest_format_).c_str(), static_cast<int>(i),
        src_frame->visible_rect().size().ToString().c_str(),
        expected_dest_frame->visible_rect().size().ToString().c_str()));

    auto plane_size = VideoFrame::PlaneSize(expected_dest_frame->format(), i,
                                            dest_visible_size);
    auto ssim = libyuv::CalcFrameSsim(
        dest_frame->visible_data(i), dest_frame->stride(i),
        expected_dest_frame->visible_data(i), expected_dest_frame->stride(i),
        plane_size.width(), plane_size.height());
    auto psnr = libyuv::CalcFramePsnr(
        dest_frame->visible_data(i), dest_frame->stride(i),
        expected_dest_frame->visible_data(i), expected_dest_frame->stride(i),
        plane_size.width(), plane_size.height());
    EXPECT_DOUBLE_EQ(ssim, 1.0);
    EXPECT_EQ(psnr, libyuv::kMaxPsnr);
  }

  // Ensure memory pool is functioning correctly by running conversions which
  // use scratch space twice.
  if (converter_.get_pool_size_for_testing() > 0) {
    EXPECT_EQ(converter_.get_pool_size_for_testing(), 1u);
    ASSERT_TRUE(converter_.ConvertAndScale(*src_frame, *dest_frame).is_ok());
    EXPECT_EQ(converter_.get_pool_size_for_testing(), 1u);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         VideoFrameConverterTest,
                         testing::Combine(testing::Values(PIXEL_FORMAT_XBGR,
                                                          PIXEL_FORMAT_XRGB,
                                                          PIXEL_FORMAT_ABGR,
                                                          PIXEL_FORMAT_ARGB,
                                                          PIXEL_FORMAT_I420,
                                                          PIXEL_FORMAT_I420A,
                                                          PIXEL_FORMAT_I444,
                                                          PIXEL_FORMAT_I444A,
                                                          PIXEL_FORMAT_NV12,
                                                          PIXEL_FORMAT_NV12A),
                                          testing::Values(PIXEL_FORMAT_XBGR,
                                                          PIXEL_FORMAT_XRGB,
                                                          PIXEL_FORMAT_ABGR,
                                                          PIXEL_FORMAT_ARGB,
                                                          PIXEL_FORMAT_I420,
                                                          PIXEL_FORMAT_I420A,
                                                          PIXEL_FORMAT_I444,
                                                          PIXEL_FORMAT_I444A,
                                                          PIXEL_FORMAT_NV12,
                                                          PIXEL_FORMAT_NV12A),
                                          testing::Bool()));

}  // namespace media
