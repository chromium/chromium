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

std::tuple<uint32_t, uint32_t, uint32_t, uint32_t> FourColors(bool opaque) {
  const uint32_t alpha = (opaque ? 0xFF : 0x80) << 24;
  const uint32_t yellow = 0x00FFFF00 | alpha;
  const uint32_t red = 0x00FF0000 | alpha;
  const uint32_t blue = 0x000000FF | alpha;
  const uint32_t green = 0x0000FF00 | alpha;
  return std::tie(yellow, red, blue, green);
}

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> RGBToYUV(uint32_t argb) {
  // We're not trying to test the quality of Y, U, V, A conversion, just that
  // it happened. So use the same internal method to convert ARGB to YUV values.
  uint8_t y, u, v, a;
  libyuv::ARGBToI444(reinterpret_cast<const uint8_t*>(&argb), 1, &y, 1, &u, 1,
                     &v, 1, 1, 1);
  a = argb >> 24;
  return std::tie(y, u, v, a);
}

void I4xxxRect(VideoFrame* dest_frame,
               int x,
               int y,
               int width,
               int height,
               uint8_t value_y,
               uint8_t value_u,
               uint8_t value_v,
               uint8_t value_a) {
  const int num_planes = VideoFrame::NumPlanes(dest_frame->format());
  DCHECK(num_planes == 3 || num_planes == 4);
  DCHECK(IsYuvPlanar(dest_frame->format()));

  // Write known full size planes first.
  libyuv::SetPlane(dest_frame->GetWritableVisibleData(VideoFrame::kYPlane) +
                       y * dest_frame->stride(VideoFrame::kYPlane) + x,
                   dest_frame->stride(VideoFrame::kYPlane), width, height,
                   value_y);
  if (num_planes == 4) {
    libyuv::SetPlane(dest_frame->GetWritableVisibleData(VideoFrame::kAPlane) +
                         y * dest_frame->stride(VideoFrame::kAPlane) + x,
                     dest_frame->stride(VideoFrame::kAPlane), width, height,
                     value_a);
  }

  // Adjust rect start and offset.
  auto start_xy = VideoFrame::PlaneSize(dest_frame->format(),
                                        VideoFrame::kUPlane, gfx::Size(x, y));
  auto uv_size = VideoFrame::PlaneSize(
      dest_frame->format(), VideoFrame::kUPlane, gfx::Size(width, height));

  // Write variable sized planes.
  libyuv::SetPlane(
      dest_frame->GetWritableVisibleData(VideoFrame::kUPlane) +
          start_xy.height() * dest_frame->stride(VideoFrame::kUPlane) +
          start_xy.width(),
      dest_frame->stride(VideoFrame::kUPlane), uv_size.width(),
      uv_size.height(), value_u);
  libyuv::SetPlane(
      dest_frame->GetWritableVisibleData(VideoFrame::kVPlane) +
          start_xy.height() * dest_frame->stride(VideoFrame::kVPlane) +
          start_xy.width(),
      dest_frame->stride(VideoFrame::kVPlane), uv_size.width(),
      uv_size.height(), value_v);
}

void FillFourColorsFrameYUV(VideoFrame& dest_frame) {
  DCHECK(IsYuvPlanar(dest_frame.format()));

  auto visible_size = dest_frame.visible_rect().size();

  auto* output_frame = &dest_frame;
  scoped_refptr<VideoFrame> temp_frame;
  if (dest_frame.format() == PIXEL_FORMAT_NV12 ||
      dest_frame.format() == PIXEL_FORMAT_NV12A) {
    temp_frame = VideoFrame::CreateZeroInitializedFrame(
        dest_frame.format() == PIXEL_FORMAT_NV12 ? PIXEL_FORMAT_I420
                                                 : PIXEL_FORMAT_I420A,
        dest_frame.coded_size(), dest_frame.visible_rect(),
        dest_frame.natural_size(), base::TimeDelta());
    output_frame = temp_frame.get();
  }

  uint32_t yellow, red, blue, green;
  std::tie(yellow, red, blue, green) =
      FourColors(IsOpaque(dest_frame.format()));

  uint8_t y, u, v, a;

  // Yellow top left.
  std::tie(y, u, v, a) = RGBToYUV(yellow);
  I4xxxRect(output_frame, 0, 0, visible_size.width() / 2,
            visible_size.height() / 2, y, u, v, a);

  // Red top right.
  std::tie(y, u, v, a) = RGBToYUV(red);
  I4xxxRect(output_frame, visible_size.width() / 2, 0, visible_size.width() / 2,
            visible_size.height() / 2, y, u, v, a);

  // Blue bottom left.
  std::tie(y, u, v, a) = RGBToYUV(blue);
  I4xxxRect(output_frame, 0, visible_size.height() / 2,
            visible_size.width() / 2, visible_size.height() / 2, y, u, v, a);

  // Green bottom right.
  std::tie(y, u, v, a) = RGBToYUV(green);
  I4xxxRect(output_frame, visible_size.width() / 2, visible_size.height() / 2,
            visible_size.width() / 2, visible_size.height() / 2, y, u, v, a);

  if (temp_frame) {
    ASSERT_EQ(libyuv::I420ToNV12(
                  temp_frame->visible_data(VideoFrame::kYPlane),
                  temp_frame->stride(VideoFrame::kYPlane),
                  temp_frame->visible_data(VideoFrame::kUPlane),
                  temp_frame->stride(VideoFrame::kUPlane),
                  temp_frame->visible_data(VideoFrame::kVPlane),
                  temp_frame->stride(VideoFrame::kVPlane),
                  dest_frame.GetWritableVisibleData(VideoFrame::kYPlane),
                  dest_frame.stride(VideoFrame::kYPlane),
                  dest_frame.GetWritableVisibleData(VideoFrame::kUVPlane),
                  dest_frame.stride(VideoFrame::kUVPlane),
                  dest_frame.visible_rect().width(),
                  dest_frame.visible_rect().height()),
              0);
    if (dest_frame.format() == PIXEL_FORMAT_NV12A) {
      libyuv::CopyPlane(
          temp_frame->visible_data(VideoFrame::kAPlane),
          temp_frame->stride(VideoFrame::kAPlane),
          dest_frame.GetWritableVisibleData(VideoFrame::kAPlaneTriPlanar),
          dest_frame.stride(VideoFrame::kAPlaneTriPlanar),
          dest_frame.visible_rect().width(),
          dest_frame.visible_rect().height());
    }
  }
}

void FillFourColorsFrameARGB(VideoFrame& dest_frame) {
  DCHECK(dest_frame.format() == PIXEL_FORMAT_ARGB ||
         dest_frame.format() == PIXEL_FORMAT_XRGB ||
         dest_frame.format() == PIXEL_FORMAT_ABGR ||
         dest_frame.format() == PIXEL_FORMAT_XBGR);

  auto visible_size = dest_frame.visible_rect().size();

  uint32_t yellow, red, blue, green;
  std::tie(yellow, red, blue, green) =
      FourColors(IsOpaque(dest_frame.format()));

  // Yellow top left.
  ASSERT_EQ(libyuv::ARGBRect(
                dest_frame.GetWritableVisibleData(VideoFrame::kARGBPlane),
                dest_frame.stride(VideoFrame::kARGBPlane), 0, 0,
                visible_size.width() / 2, visible_size.height() / 2, yellow),
            0);

  // Red top right.
  ASSERT_EQ(
      libyuv::ARGBRect(
          dest_frame.GetWritableVisibleData(VideoFrame::kARGBPlane),
          dest_frame.stride(VideoFrame::kARGBPlane), visible_size.width() / 2,
          0, visible_size.width() / 2, visible_size.height() / 2, red),
      0);

  // Blue bottom left.
  ASSERT_EQ(libyuv::ARGBRect(
                dest_frame.GetWritableVisibleData(VideoFrame::kARGBPlane),
                dest_frame.stride(VideoFrame::kARGBPlane), 0,
                visible_size.height() / 2, visible_size.width() / 2,
                visible_size.height() / 2, blue),
            0);

  // Green bottom right.
  ASSERT_EQ(libyuv::ARGBRect(
                dest_frame.GetWritableVisibleData(VideoFrame::kARGBPlane),
                dest_frame.stride(VideoFrame::kARGBPlane),
                visible_size.width() / 2, visible_size.height() / 2,
                visible_size.width() / 2, visible_size.height() / 2, green),
            0);

  if (dest_frame.format() == PIXEL_FORMAT_XBGR ||
      dest_frame.format() == PIXEL_FORMAT_ABGR) {
    ASSERT_EQ(libyuv::ARGBToABGR(
                  dest_frame.visible_data(VideoFrame::kARGBPlane),
                  dest_frame.stride(VideoFrame::kARGBPlane),
                  dest_frame.GetWritableVisibleData(VideoFrame::kARGBPlane),
                  dest_frame.stride(VideoFrame::kARGBPlane),
                  visible_size.width(), visible_size.height()),
              0);
  }
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

  if (IsRGB(src_format_)) {
    FillFourColorsFrameARGB(*src_frame);
  } else {
    FillFourColorsFrameYUV(*src_frame);
  }

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
  FillFourColorsFrameYUV(*expected_dest_frame);

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
