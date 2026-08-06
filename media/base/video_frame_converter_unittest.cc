// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_converter.h"

#include <algorithm>
#include <array>

#include "base/compiler_specific.h"
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
constexpr gfx::Rect kOddRect(0, 0, 63, 63);

enum class TestConversionType {
  kNormal,
  kScaled,
  kOdd,  // Visible rect is the same as the coded size but odd.
};

gfx::Size SelectDestSize(TestConversionType conversion_type) {
  switch (conversion_type) {
    case TestConversionType::kNormal:
      return kCodedSize;
    case TestConversionType::kScaled:
      return gfx::ScaleToRoundedSize(kCodedSize, 0.5);
    case TestConversionType::kOdd:
      return kOddRect.size();
  }
}

gfx::Rect SelectDestRect(TestConversionType conversion_type) {
  switch (conversion_type) {
    case TestConversionType::kNormal:
      return kVisibleRect;
    case TestConversionType::kScaled:
      return gfx::ScaleToRoundedRect(kVisibleRect, 0.5, 0.5);
    case TestConversionType::kOdd:
      return kOddRect;
  }
}

gfx::Size SelectSrcCodedSize(TestConversionType conversion_type) {
  switch (conversion_type) {
    case TestConversionType::kNormal:
    case TestConversionType::kScaled:
      return kCodedSize;
    case TestConversionType::kOdd:
      return kOddRect.size();
  }
}

gfx::Rect SelectSrcRect(TestConversionType conversion_type) {
  switch (conversion_type) {
    case TestConversionType::kNormal:
    case TestConversionType::kScaled:
      return kVisibleRect;
    case TestConversionType::kOdd:
      return kOddRect;
  }
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
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
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
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV444P10:
      break;

    default:
      return false;
  }

  return true;
}

template <typename T>
void FillPlane(base::span<T> data,
               size_t stride_elements,
               size_t width,
               size_t height,
               T value) {
  for (size_t r = 0; r < height; ++r) {
    std::ranges::fill(data.subspan(r * stride_elements, width), value);
  }
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
      UNSAFE_TODO(fwrite(frame.visible_data(i) + y * frame.stride(i), 1,
                         plane_size.width(), f));
    }
  }
  fclose(f);
}

}  // namespace

using TestParams =
    testing::tuple<VideoPixelFormat, VideoPixelFormat, TestConversionType>;
class VideoFrameConverterTest
    : public testing::Test,
      public ::testing::WithParamInterface<TestParams> {
 public:
  VideoFrameConverterTest()
      : src_format_(testing::get<0>(GetParam())),
        dest_format_(testing::get<1>(GetParam())),
        src_coded_size_(SelectSrcCodedSize(testing::get<2>(GetParam()))),
        src_visible_rect_(SelectSrcRect(testing::get<2>(GetParam()))),
        dest_coded_size_(SelectDestSize(testing::get<2>(GetParam()))),
        dest_visible_rect_(SelectDestRect(testing::get<2>(GetParam()))) {}

 protected:
  const VideoPixelFormat src_format_;
  const VideoPixelFormat dest_format_;
  const gfx::Size src_coded_size_;
  const gfx::Rect src_visible_rect_;
  const gfx::Size dest_coded_size_;
  const gfx::Rect dest_visible_rect_;
  VideoFrameConverter converter_;
};

TEST_P(VideoFrameConverterTest, ConvertAndScale) {
  // Zero initialize so coded size regions are all zero.
  auto src_frame = VideoFrame::CreateZeroInitializedFrame(
      src_format_, src_coded_size_, src_visible_rect_, src_visible_rect_.size(),
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
  size_t expected_pool_size = converter_.get_pool_size_for_testing();
  if (expected_pool_size > 0) {
    EXPECT_EQ(converter_.get_pool_size_for_testing(), expected_pool_size);
    ASSERT_TRUE(converter_.ConvertAndScale(*src_frame, *dest_frame).is_ok());
    EXPECT_EQ(converter_.get_pool_size_for_testing(), expected_pool_size);
  }
}

TEST(VideoFrameConverterRegressionTest, WeirdScaling) {
  constexpr gfx::Size kTestSize(80, 50);
  auto src_frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, kTestSize, gfx::Rect(kTestSize), kTestSize,
      base::TimeDelta());
  constexpr gfx::Size kDestSize(188, 144);
  auto dest_frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_NV12, kDestSize, gfx::Rect(kDestSize), kDestSize,
      base::TimeDelta());

  FillFourColors(*src_frame);

  VideoFrameConverter converter;
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
}

TEST(VideoFrameConverterUnsupportedTest, UnsupportedConversions) {
  constexpr gfx::Size kSize(64, 64);
  VideoFrameConverter converter;

  auto is_convertible_format = [](VideoPixelFormat format) {
    if (!IsValidVideoPixelFormat(format)) {
      return false;
    }
    if (format == PIXEL_FORMAT_UNKNOWN || format == PIXEL_FORMAT_MJPEG ||
        format == PIXEL_FORMAT_Y16) {
      return false;
    }
    return true;
  };

  for (int i = 0; i <= PIXEL_FORMAT_MAX; ++i) {
    auto src_format = static_cast<VideoPixelFormat>(i);
    if (!is_convertible_format(src_format)) {
      continue;
    }
    auto src_frame = VideoFrame::CreateZeroInitializedFrame(
        src_format, kSize, gfx::Rect(kSize), kSize, base::TimeDelta());
    for (int j = 0; j <= PIXEL_FORMAT_MAX; ++j) {
      auto dest_format = static_cast<VideoPixelFormat>(j);
      if (!is_convertible_format(dest_format)) {
        continue;
      }
      if (IsConversionSupported(src_format, dest_format)) {
        continue;
      }
      SCOPED_TRACE(base::StringPrintf(
          "%s -> %s", VideoPixelFormatToString(src_format).c_str(),
          VideoPixelFormatToString(dest_format).c_str()));
      auto dest_frame = VideoFrame::CreateZeroInitializedFrame(
          dest_format, kSize, gfx::Rect(kSize), kSize, base::TimeDelta());
      if (src_frame && dest_frame) {
        EXPECT_FALSE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok())
            << VideoPixelFormatToString(src_format) << " -> "
            << VideoPixelFormatToString(dest_format);
      }
    }
  }
}

class VideoFrameConverterExtentsTest : public VideoFrameConverterTest {};

TEST_P(VideoFrameConverterExtentsTest, ConvertAndScaleExtents) {
  // Extents (0 to max) are preserved directly across YUV-to-YUV planar
  // conversions without color space matrix scaling.
  if (!IsYuvPlanar(src_format_) || !IsYuvPlanar(dest_format_)) {
    return;
  }

  if (!IsOpaque(dest_format_) && IsOpaque(src_format_)) {
    return;
  }

  ASSERT_TRUE(IsConversionSupported(src_format_, dest_format_));

  constexpr gfx::Size kSize(64, 64);
  auto src_frame = VideoFrame::CreateZeroInitializedFrame(
      src_format_, kSize, gfx::Rect(kSize), kSize, base::TimeDelta());
  auto dest_frame = VideoFrame::CreateZeroInitializedFrame(
      dest_format_, kSize, gfx::Rect(kSize), kSize, base::TimeDelta());

  auto get_max_sample_value = [](VideoPixelFormat format) -> uint16_t {
    switch (format) {
      case PIXEL_FORMAT_YUV420P10:
      case PIXEL_FORMAT_YUV422P10:
      case PIXEL_FORMAT_YUV444P10:
      case PIXEL_FORMAT_YUV420AP10:
      case PIXEL_FORMAT_YUV422AP10:
      case PIXEL_FORMAT_YUV444AP10:
        return 1023;
      case PIXEL_FORMAT_P010LE:
      case PIXEL_FORMAT_P210LE:
      case PIXEL_FORMAT_P410LE:
        return 1023 << 6;  // MSB aligned in 16-bit
      case PIXEL_FORMAT_YUV420P12:
      case PIXEL_FORMAT_YUV422P12:
      case PIXEL_FORMAT_YUV444P12:
        return 4095;
      default:
        return 255;
    }
  };

  uint16_t src_max = get_max_sample_value(src_format_);
  uint16_t dest_max = get_max_sample_value(dest_format_);

  // Fill Y plane with 0 in top-left quadrant and src_max in bottom-right
  // quadrant.
  const auto vis_size = src_frame->visible_rect().size();
  const size_t pw = vis_size.width();
  const size_t ph = vis_size.height();
  const size_t hpw = pw / 2;
  const size_t hph = ph / 2;

  if (BitDepth(src_format_) > 8) {
    auto data = base::subtle::reinterpret_span<uint16_t>(
        src_frame->GetWritableVisiblePlaneData(0));
    size_t stride_el = src_frame->stride(0) / sizeof(uint16_t);
    FillPlane(data, stride_el, pw, ph, static_cast<uint16_t>(0));
    FillPlane(data.subspan(hph * stride_el + hpw), stride_el, pw - hpw,
              ph - hph, src_max);
  } else {
    auto data = src_frame->GetWritableVisiblePlaneData(0);
    size_t stride = src_frame->stride(0);
    FillPlane(data, stride, pw, ph, static_cast<uint8_t>(0));
    FillPlane(data.subspan(hph * stride + hpw), stride, pw - hpw, ph - hph,
              static_cast<uint8_t>(src_max));
  }

  ASSERT_TRUE(converter_.ConvertAndScale(*src_frame, *dest_frame).is_ok());

  // Verify min value at (0, 0) and max value at (pw - 1, ph - 1) in dest_frame
  // Y plane.
  int dest_w = dest_frame->visible_rect().width();
  int dest_h = dest_frame->visible_rect().height();

  if (BitDepth(dest_format_) > 8) {
    auto dest_y = base::subtle::reinterpret_span<const uint16_t>(
        dest_frame->GetVisiblePlaneData(0));
    int stride_el = dest_frame->stride(0) / sizeof(uint16_t);

    EXPECT_EQ(dest_y[0], 0);
    EXPECT_EQ(dest_y[(dest_h - 1) * stride_el + (dest_w - 1)], dest_max);
  } else {
    auto dest_y = dest_frame->GetVisiblePlaneData(0);
    int stride = dest_frame->stride(0);

    EXPECT_EQ(dest_y[0], 0);
    EXPECT_EQ(dest_y[(dest_h - 1) * stride + (dest_w - 1)],
              static_cast<uint8_t>(dest_max));
  }
}

std::string PrintTestParams(const testing::TestParamInfo<TestParams>& info) {
  auto format_to_string = [](VideoPixelFormat format) {
    std::string name = VideoPixelFormatToString(format);
    const std::string prefix = "PIXEL_FORMAT_";
    if (name.find(prefix) == 0) {
      name = name.substr(prefix.length());
    }
    return name;
  };

  std::string result = format_to_string(testing::get<0>(info.param)) + "To" +
                       format_to_string(testing::get<1>(info.param));
  switch (testing::get<2>(info.param)) {
    case TestConversionType::kNormal:
      result += "_Normal";
      break;
    case TestConversionType::kScaled:
      result += "_Scaled";
      break;
    case TestConversionType::kOdd:
      result += "_Odd";
      break;
  }
  return result;
}

constexpr auto kInputFormats = std::to_array<VideoPixelFormat>({
    PIXEL_FORMAT_XBGR,       PIXEL_FORMAT_XRGB,       PIXEL_FORMAT_ABGR,
    PIXEL_FORMAT_ARGB,       PIXEL_FORMAT_I420,       PIXEL_FORMAT_I420A,
    PIXEL_FORMAT_I444,       PIXEL_FORMAT_I444A,      PIXEL_FORMAT_NV12,
    PIXEL_FORMAT_NV12A,      PIXEL_FORMAT_YUV420P10,  PIXEL_FORMAT_YUV422P10,
    PIXEL_FORMAT_YUV444P10,  PIXEL_FORMAT_YUV420P12,  PIXEL_FORMAT_YUV422P12,
    PIXEL_FORMAT_YUV444P12,  PIXEL_FORMAT_YUV420AP10, PIXEL_FORMAT_YUV422AP10,
    PIXEL_FORMAT_YUV444AP10, PIXEL_FORMAT_P010LE,     PIXEL_FORMAT_P210LE,
    PIXEL_FORMAT_P410LE,
});

constexpr auto kOutputFormats = std::to_array<VideoPixelFormat>({
    PIXEL_FORMAT_I420,
    PIXEL_FORMAT_I420A,
    PIXEL_FORMAT_I444,
    PIXEL_FORMAT_I444A,
    PIXEL_FORMAT_NV12,
    PIXEL_FORMAT_NV12A,
    PIXEL_FORMAT_P010LE,
    PIXEL_FORMAT_YUV420P10,
    PIXEL_FORMAT_YUV444P10,
});

INSTANTIATE_TEST_SUITE_P(
    ,
    VideoFrameConverterTest,
    testing::Combine(testing::ValuesIn(kInputFormats),
                     testing::ValuesIn(kOutputFormats),
                     testing::Values(TestConversionType::kNormal,
                                     TestConversionType::kScaled,
                                     TestConversionType::kOdd)),
    PrintTestParams);

INSTANTIATE_TEST_SUITE_P(
    Extents,
    VideoFrameConverterExtentsTest,
    testing::Combine(testing::ValuesIn(kInputFormats),
                     testing::ValuesIn(kOutputFormats),
                     testing::Values(TestConversionType::kNormal)),
    PrintTestParams);

}  // namespace media
