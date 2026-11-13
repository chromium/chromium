// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_converter.h"

#include <algorithm>
#include <array>
#include <vector>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_span.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_converter_internals.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace media {

namespace {

// Use 64x64 for visible size ensure U,V planes are > 8x8 since libyuv can't
// calculate the SSIM for smaller sizes.
constexpr gfx::Size kCodedSize(128, 128);
constexpr gfx::Rect kVisibleRect(64, 64, 64, 64);
constexpr gfx::Rect kOddRect(0, 0, 63, 63);

constexpr gfx::Size kDirectCodedSize(96, 96);
constexpr gfx::Rect kDirectSourceRect(4, 6, 63, 65);
constexpr gfx::Rect kDirectDestRect(8, 10, 63, 65);

struct Biplanar10BitTestFormat {
  VideoPixelFormat planar;
  VideoPixelFormat biplanar;
};

constexpr Biplanar10BitTestFormat kBiplanar10BitTestFormats[] = {
    {PIXEL_FORMAT_YUV420P10, PIXEL_FORMAT_P010LE},
    {PIXEL_FORMAT_YUV422P10, PIXEL_FORMAT_P210LE},
    {PIXEL_FORMAT_YUV444P10, PIXEL_FORMAT_P410LE},
};

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

  if (src == PIXEL_FORMAT_RGBAF16) {
    return dest == PIXEL_FORMAT_P010LE || dest == PIXEL_FORMAT_P210LE ||
           dest == PIXEL_FORMAT_P410LE;
  }

  switch (src) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24:
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
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
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
  base::File file(
      base::FilePath::FromASCII(base::StringPrintf(
          "/tmp/%s_%s_%s.bin", prefix, VideoPixelFormatToString(frame.format()),
          frame.visible_rect().size().ToString())),
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  CHECK(file.IsValid()) << base::File::ErrorToString(file.error_details());
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame.format()); ++i) {
    const auto plane_data = frame.GetVisiblePlaneData(i);
    const auto row_bytes =
        base::checked_cast<size_t>(frame.GetVisibleRowBytes(i));
    const auto rows = base::checked_cast<size_t>(frame.GetVisibleRows(i));
    for (size_t y = 0; y < rows; ++y) {
      CHECK(file.WriteAtCurrentPosAndCheck(
          plane_data.subspan(y * frame.stride(i), row_bytes)));
    }
  }
}

uint16_t ReadVisibleSample(const VideoFrame& frame,
                           size_t plane,
                           int x,
                           int y) {
  if (BitDepth(frame.format()) == 8) {
    return frame.GetVisiblePlaneData(plane)[y * frame.stride(plane) + x];
  }
  return base::subtle::reinterpret_span<const uint16_t>(
      frame.GetVisiblePlaneData(
          plane))[y * (frame.stride(plane) / sizeof(uint16_t)) + x];
}

void WriteVisibleSample(VideoFrame& frame,
                        size_t plane,
                        int x,
                        int y,
                        uint16_t value) {
  if (BitDepth(frame.format()) == 8) {
    frame.GetWritableVisiblePlaneData(plane)[y * frame.stride(plane) + x] =
        static_cast<uint8_t>(value);
  } else {
    base::subtle::reinterpret_span<uint16_t>(frame.GetWritableVisiblePlaneData(
        plane))[y * (frame.stride(plane) / sizeof(uint16_t)) + x] = value;
  }
}

void FillVisibleSamplesWithPattern(VideoFrame& frame) {
  const int bytes_per_sample = BitDepth(frame.format()) > 8 ? 2 : 1;
  const int mask = (1 << BitDepth(frame.format())) - 1;
  for (size_t plane = 0; plane < VideoFrame::NumPlanes(frame.format());
       ++plane) {
    for (int y = 0; y < frame.GetVisibleRows(plane); ++y) {
      for (int x = 0; x < frame.GetVisibleRowBytes(plane) / bytes_per_sample;
           ++x) {
        WriteVisibleSample(frame, plane, x, y,
                           (x * 37 + y * 73 + plane * 113 + x * y) & mask);
      }
    }
  }
}

// Build the MSB-aligned input independently of libyuv's packing functions.
void PackPlanar10BitFrame(const VideoFrame& planar, VideoFrame& biplanar) {
  for (size_t plane = 0; plane < 3; ++plane) {
    for (int y = 0; y < planar.GetVisibleRows(plane); ++y) {
      for (int x = 0; x < planar.GetVisibleColumns(plane); ++x) {
        WriteVisibleSample(biplanar, plane == 0 ? 0 : 1,
                           plane == 0 ? x : x * 2 + plane - 1, y,
                           ReadVisibleSample(planar, plane, x, y) << 6);
      }
    }
  }
}

void ExpectBiplanarSamplesMatch(const VideoFrame& planar,
                                const VideoFrame& biplanar) {
  ASSERT_EQ(planar.visible_rect().size(), biplanar.visible_rect().size());
  for (size_t plane = 0; plane < 3; ++plane) {
    for (int y = 0; y < planar.GetVisibleRows(plane); ++y) {
      for (int x = 0; x < planar.GetVisibleColumns(plane); ++x) {
        uint16_t expected = ReadVisibleSample(planar, plane, x, y);
        if (BitDepth(biplanar.format()) == 10) {
          // Expand all 8 bits to 10 bits, including the two replicated bits.
          expected = BitDepth(planar.format()) == 8 ? (expected * 257) & 0xffc0
                                                    : expected << 6;
        }
        ASSERT_EQ(ReadVisibleSample(biplanar, plane == 0 ? 0 : 1,
                                    plane == 0 ? x : x * 2 + plane - 1, y),
                  expected)
            << "plane=" << plane << ", x=" << x << ", y=" << y;
      }
    }
  }
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

  if (!IsConversionSupported(src_format_, dest_format_)) {
    src_frame->set_color_space(gfx::ColorSpace::CreateREC601());
    EXPECT_FALSE(converter_.ConvertAndScale(*src_frame, *dest_frame).is_ok());
    return;
  }

  std::vector<gfx::ColorSpace> color_spaces_to_test;
  if (IsRGB(src_format_)) {
    for (auto p : {gfx::ColorSpace::PrimaryID::BT709,
                   gfx::ColorSpace::PrimaryID::SMPTE170M,
                   gfx::ColorSpace::PrimaryID::BT2020}) {
      for (auto r : {gfx::ColorSpace::RangeID::FULL,
                     gfx::ColorSpace::RangeID::LIMITED}) {
        color_spaces_to_test.emplace_back(p, gfx::ColorSpace::TransferID::SRGB,
                                          gfx::ColorSpace::MatrixID::RGB, r);
      }
    }
  } else {
    color_spaces_to_test.push_back(gfx::ColorSpace::CreateREC709());
  }

  auto expected_dest_frame = VideoFrame::CreateZeroInitializedFrame(
      dest_format_, dest_coded_size_, dest_visible_rect_,
      dest_visible_rect_.size(), base::TimeDelta());

  for (const auto& cs : color_spaces_to_test) {
    src_frame->set_color_space(cs);
    FillFourColors(*src_frame);

    ASSERT_TRUE(converter_.ConvertAndScale(*src_frame, *dest_frame).is_ok());

    // Recreate the ideal frame at the destination size.
    DCHECK(IsYuvPlanar(dest_format_));
    expected_dest_frame->set_color_space(
        VideoFrameConverter::GetDestinationColorSpace(*src_frame));
    FillFourColors(*expected_dest_frame);

    auto dest_visible_size = expected_dest_frame->visible_rect().size();
    for (size_t i = 0; i < VideoFrame::NumPlanes(expected_dest_frame->format());
         ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "%s -> %s, cs=%s, plane=%d, (%s -> %s)",
          VideoPixelFormatToString(src_format_).c_str(),
          VideoPixelFormatToString(dest_format_).c_str(), cs.ToString().c_str(),
          static_cast<int>(i),
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

TEST(VideoFrameConverterFastPathTest, ConversionsAvoidScratch) {
  constexpr gfx::Size kSize(64, 64);
  struct Conversion {
    VideoPixelFormat src;
    VideoPixelFormat dest;
  };

  struct ConversionGroup {
    const char* name;
    base::raw_span<const Conversion> cases;
  };

  constexpr Conversion kBiplanarToBiplanarDirect[] = {
      {PIXEL_FORMAT_NV12, PIXEL_FORMAT_NV12},
      {PIXEL_FORMAT_NV12, PIXEL_FORMAT_NV16},
      {PIXEL_FORMAT_NV12, PIXEL_FORMAT_NV24},
      {PIXEL_FORMAT_NV12A, PIXEL_FORMAT_NV12},
      {PIXEL_FORMAT_NV12A, PIXEL_FORMAT_NV12A},
      {PIXEL_FORMAT_NV12A, PIXEL_FORMAT_NV16},
      {PIXEL_FORMAT_NV12A, PIXEL_FORMAT_NV24},
      {PIXEL_FORMAT_NV16, PIXEL_FORMAT_NV12},
      {PIXEL_FORMAT_NV16, PIXEL_FORMAT_NV16},
      {PIXEL_FORMAT_NV16, PIXEL_FORMAT_NV24},
      {PIXEL_FORMAT_NV24, PIXEL_FORMAT_NV12},
      {PIXEL_FORMAT_NV24, PIXEL_FORMAT_NV16},
      {PIXEL_FORMAT_NV24, PIXEL_FORMAT_NV24},
  };

  constexpr Conversion kPlanarToBiplanarDirect[] = {
      {PIXEL_FORMAT_I420, PIXEL_FORMAT_NV12},
      {PIXEL_FORMAT_I422, PIXEL_FORMAT_NV16},
      {PIXEL_FORMAT_I444, PIXEL_FORMAT_NV12},
      {PIXEL_FORMAT_I444, PIXEL_FORMAT_NV24},
  };

  constexpr Conversion kBiplanarToPlanarSameSampling[] = {
      {PIXEL_FORMAT_NV12, PIXEL_FORMAT_I420},
      {PIXEL_FORMAT_NV12A, PIXEL_FORMAT_I420},
      {PIXEL_FORMAT_NV12A, PIXEL_FORMAT_I420A},
      {PIXEL_FORMAT_NV16, PIXEL_FORMAT_I422},
      {PIXEL_FORMAT_NV24, PIXEL_FORMAT_I444},
  };

  constexpr Conversion kPlanarAlphaToBiplanarDirect[] = {
      {PIXEL_FORMAT_I420A, PIXEL_FORMAT_NV12},
      {PIXEL_FORMAT_I420A, PIXEL_FORMAT_NV12A},
      {PIXEL_FORMAT_I422A, PIXEL_FORMAT_NV16},
      {PIXEL_FORMAT_I444A, PIXEL_FORMAT_NV12},
      {PIXEL_FORMAT_I444A, PIXEL_FORMAT_NV12A},
      {PIXEL_FORMAT_I444A, PIXEL_FORMAT_NV24},
  };

  constexpr Conversion kRGBToBiplanarDirect[] = {
      {PIXEL_FORMAT_ARGB, PIXEL_FORMAT_NV16},
      {PIXEL_FORMAT_ARGB, PIXEL_FORMAT_NV24},
      {PIXEL_FORMAT_ARGB, PIXEL_FORMAT_P010LE},
      {PIXEL_FORMAT_ARGB, PIXEL_FORMAT_P210LE},
      {PIXEL_FORMAT_ARGB, PIXEL_FORMAT_P410LE},
  };

  constexpr Conversion kBiplanar10BitDirect[] = {
      {PIXEL_FORMAT_P010LE, PIXEL_FORMAT_P010LE},
      {PIXEL_FORMAT_P210LE, PIXEL_FORMAT_P210LE},
      {PIXEL_FORMAT_P410LE, PIXEL_FORMAT_P410LE},
      {PIXEL_FORMAT_I420, PIXEL_FORMAT_P010LE},
      {PIXEL_FORMAT_I420A, PIXEL_FORMAT_P010LE},
      {PIXEL_FORMAT_I422, PIXEL_FORMAT_P210LE},
      {PIXEL_FORMAT_I422A, PIXEL_FORMAT_P210LE},
      {PIXEL_FORMAT_I444, PIXEL_FORMAT_P410LE},
      {PIXEL_FORMAT_I444A, PIXEL_FORMAT_P410LE},
      {PIXEL_FORMAT_NV12, PIXEL_FORMAT_P010LE},
      {PIXEL_FORMAT_NV12A, PIXEL_FORMAT_P010LE},
      {PIXEL_FORMAT_NV16, PIXEL_FORMAT_P210LE},
      {PIXEL_FORMAT_NV24, PIXEL_FORMAT_P410LE},
      {PIXEL_FORMAT_P010LE, PIXEL_FORMAT_NV12},
      {PIXEL_FORMAT_P210LE, PIXEL_FORMAT_NV16},
      {PIXEL_FORMAT_P410LE, PIXEL_FORMAT_NV24},
      {PIXEL_FORMAT_P010LE, PIXEL_FORMAT_I420},
      {PIXEL_FORMAT_P210LE, PIXEL_FORMAT_I420},
      {PIXEL_FORMAT_P410LE, PIXEL_FORMAT_I420},
      {PIXEL_FORMAT_P010LE, PIXEL_FORMAT_YUV420P10},
      {PIXEL_FORMAT_P410LE, PIXEL_FORMAT_YUV444P10},
      {PIXEL_FORMAT_YUV420AP10, PIXEL_FORMAT_P010LE},
      {PIXEL_FORMAT_YUV422AP10, PIXEL_FORMAT_P210LE},
      {PIXEL_FORMAT_YUV444AP10, PIXEL_FORMAT_P410LE},
  };

  const ConversionGroup kGroups[] = {
      {"biplanar to biplanar direct", kBiplanarToBiplanarDirect},
      {"planar to biplanar direct", kPlanarToBiplanarDirect},
      {"biplanar to same-sampling planar", kBiplanarToPlanarSameSampling},
      {"planar alpha to biplanar direct", kPlanarAlphaToBiplanarDirect},
      {"RGB to biplanar direct", kRGBToBiplanarDirect},
      {"biplanar 10-bit direct", kBiplanar10BitDirect},
  };

  for (const auto& group : kGroups) {
    SCOPED_TRACE(group.name);
    for (const auto& test_case : group.cases) {
      SCOPED_TRACE(base::StringPrintf(
          "%s -> %s", VideoPixelFormatToString(test_case.src).c_str(),
          VideoPixelFormatToString(test_case.dest).c_str()));
      auto src_frame = VideoFrame::CreateZeroInitializedFrame(
          test_case.src, kSize, gfx::Rect(kSize), kSize, base::TimeDelta());
      auto dest_frame = VideoFrame::CreateZeroInitializedFrame(
          test_case.dest, kSize, gfx::Rect(kSize), kSize, base::TimeDelta());
      ASSERT_TRUE(src_frame);
      ASSERT_TRUE(dest_frame);
      FillFourColors(*src_frame);

      VideoFrameConverter converter;
      ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
      EXPECT_EQ(converter.get_pool_size_for_testing(), 0u);
    }
  }
}

TEST(VideoFrameConverterDirectTest, RGBMatchesPlanarConversion) {
  struct Conversion {
    VideoPixelFormat planar;
    VideoPixelFormat biplanar;
  };
  constexpr Conversion kConversions[] = {
      {PIXEL_FORMAT_I422, PIXEL_FORMAT_NV16},
      {PIXEL_FORMAT_I444, PIXEL_FORMAT_NV24},
      {PIXEL_FORMAT_I420, PIXEL_FORMAT_P010LE},
      {PIXEL_FORMAT_I422, PIXEL_FORMAT_P210LE},
      {PIXEL_FORMAT_I444, PIXEL_FORMAT_P410LE},
  };
  // Existing parameterized tests cover the matrices, scaling and XRGB/XBGR.
  // Here exercise textured input in each byte order and the new output paths.
  for (auto source_format : {PIXEL_FORMAT_ARGB, PIXEL_FORMAT_ABGR}) {
    auto source = VideoFrame::CreateZeroInitializedFrame(
        source_format, kDirectCodedSize, kDirectSourceRect,
        kDirectSourceRect.size(), base::TimeDelta());
    ASSERT_TRUE(source);
    source->set_color_space(gfx::ColorSpace::CreateSRGB());
    FillVisibleSamplesWithPattern(*source);
    for (const auto& conversion : kConversions) {
      SCOPED_TRACE(testing::Message()
                   << source_format << " -> " << conversion.biplanar);
      auto dest = VideoFrame::CreateZeroInitializedFrame(
          conversion.biplanar, kDirectCodedSize, kDirectDestRect,
          kDirectDestRect.size(), base::TimeDelta());
      auto reference = VideoFrame::CreateZeroInitializedFrame(
          conversion.planar, kDirectCodedSize, kDirectDestRect,
          kDirectDestRect.size(), base::TimeDelta());
      ASSERT_TRUE(dest);
      ASSERT_TRUE(reference);
      VideoFrameConverter converter;
      ASSERT_TRUE(converter.ConvertAndScale(*source, *dest).is_ok());
      ASSERT_TRUE(converter.ConvertAndScale(*source, *reference).is_ok());
      ExpectBiplanarSamplesMatch(*reference, *dest);
    }
  }
}

TEST(VideoFrameConverterDirectTest, Px10ScalingPreservesTenBitSamples) {
  for (const auto& format : kBiplanar10BitTestFormats) {
    auto planar = VideoFrame::CreateZeroInitializedFrame(
        format.planar, kDirectCodedSize, kDirectSourceRect,
        kDirectSourceRect.size(), base::TimeDelta());
    auto source = VideoFrame::CreateZeroInitializedFrame(
        format.biplanar, kDirectCodedSize, kDirectSourceRect,
        kDirectSourceRect.size(), base::TimeDelta());
    ASSERT_TRUE(planar);
    ASSERT_TRUE(source);
    FillVisibleSamplesWithPattern(*planar);
    PackPlanar10BitFrame(*planar, *source);
    for (const auto& size : {gfx::Size(23, 19), gfx::Size(126, 130)}) {
      SCOPED_TRACE(testing::Message()
                   << format.biplanar << ", " << size.ToString());
      const gfx::Rect dest_rect(gfx::Point(8, 10), size);
      const gfx::Size coded_size(size.width() + 32, size.height() + 32);
      auto dest = VideoFrame::CreateZeroInitializedFrame(
          format.biplanar, coded_size, dest_rect, size, base::TimeDelta());
      auto reference = VideoFrame::CreateZeroInitializedFrame(
          format.planar, coded_size, dest_rect, size, base::TimeDelta());
      ASSERT_TRUE(dest);
      ASSERT_TRUE(reference);
      for (size_t plane = 0; plane < 3; ++plane) {
        ASSERT_EQ(
            libyuv::ScalePlane_16(
                base::subtle::reinterpret_span<const uint16_t>(
                    planar->GetVisiblePlaneData(plane))
                    .data(),
                planar->stride(plane) / sizeof(uint16_t),
                planar->GetVisibleColumns(plane), planar->GetVisibleRows(plane),
                base::subtle::reinterpret_span<uint16_t>(
                    reference->GetWritableVisiblePlaneData(plane))
                    .data(),
                reference->stride(plane) / sizeof(uint16_t),
                reference->GetVisibleColumns(plane),
                reference->GetVisibleRows(plane), libyuv::kFilterBox),
            0);
      }
      VideoFrameConverter converter;
      ASSERT_TRUE(converter.ConvertAndScale(*source, *dest).is_ok());
      ExpectBiplanarSamplesMatch(*reference, *dest);
      // libyuv has workspace, but the converter no longer needs scratch frames.
      EXPECT_EQ(converter.get_pool_size_for_testing(), 0u);
    }
  }
}

TEST(VideoFrameConverterDirectTest, Planar8ToPx10PreservesAllSamples) {
  struct Conversion {
    VideoPixelFormat source;
    VideoPixelFormat dest;
  };
  constexpr Conversion kConversions[] = {
      {PIXEL_FORMAT_I420, PIXEL_FORMAT_P010LE},
      {PIXEL_FORMAT_I422, PIXEL_FORMAT_P210LE},
      {PIXEL_FORMAT_I444, PIXEL_FORMAT_P410LE},
  };
  for (const auto& conversion : kConversions) {
    SCOPED_TRACE(testing::Message()
                 << conversion.source << " -> " << conversion.dest);
    auto source = VideoFrame::CreateZeroInitializedFrame(
        conversion.source, kDirectCodedSize, kDirectSourceRect,
        kDirectSourceRect.size(), base::TimeDelta());
    auto dest = VideoFrame::CreateZeroInitializedFrame(
        conversion.dest, kDirectCodedSize, kDirectDestRect,
        kDirectDestRect.size(), base::TimeDelta());
    ASSERT_TRUE(source);
    ASSERT_TRUE(dest);
    FillVisibleSamplesWithPattern(*source);
    VideoFrameConverter converter;
    ASSERT_TRUE(converter.ConvertAndScale(*source, *dest).is_ok());
    ExpectBiplanarSamplesMatch(*source, *dest);
    EXPECT_EQ(converter.get_pool_size_for_testing(), 0u);
  }
}

TEST(VideoFrameConverterDirectTest, Px10ToI420MatchesPlanarConversion) {
  for (const auto& format : kBiplanar10BitTestFormats) {
    SCOPED_TRACE(format.biplanar);
    auto planar = VideoFrame::CreateZeroInitializedFrame(
        format.planar, kDirectCodedSize, kDirectSourceRect,
        kDirectSourceRect.size(), base::TimeDelta());
    auto source = VideoFrame::CreateZeroInitializedFrame(
        format.biplanar, kDirectCodedSize, kDirectSourceRect,
        kDirectSourceRect.size(), base::TimeDelta());
    auto dest = VideoFrame::CreateZeroInitializedFrame(
        PIXEL_FORMAT_I420, kDirectCodedSize, kDirectDestRect,
        kDirectDestRect.size(), base::TimeDelta());
    auto reference = VideoFrame::CreateZeroInitializedFrame(
        PIXEL_FORMAT_I420, kDirectCodedSize, kDirectDestRect,
        kDirectDestRect.size(), base::TimeDelta());
    ASSERT_TRUE(planar);
    ASSERT_TRUE(source);
    ASSERT_TRUE(dest);
    ASSERT_TRUE(reference);
    FillVisibleSamplesWithPattern(*planar);
    PackPlanar10BitFrame(*planar, *source);
    auto convert = libyuv::I010ToI420;
    if (format.biplanar == PIXEL_FORMAT_P210LE) {
      convert = libyuv::I210ToI420;
    } else if (format.biplanar == PIXEL_FORMAT_P410LE) {
      convert = libyuv::I410ToI420;
    }
    ASSERT_EQ(convert(base::subtle::reinterpret_span<const uint16_t>(
                          planar->GetVisiblePlaneData(VideoFrame::Plane::kY))
                          .data(),
                      planar->stride(VideoFrame::Plane::kY) / sizeof(uint16_t),
                      base::subtle::reinterpret_span<const uint16_t>(
                          planar->GetVisiblePlaneData(VideoFrame::Plane::kU))
                          .data(),
                      planar->stride(VideoFrame::Plane::kU) / sizeof(uint16_t),
                      base::subtle::reinterpret_span<const uint16_t>(
                          planar->GetVisiblePlaneData(VideoFrame::Plane::kV))
                          .data(),
                      planar->stride(VideoFrame::Plane::kV) / sizeof(uint16_t),
                      reference->GetWritableVisibleData(VideoFrame::Plane::kY),
                      reference->stride(VideoFrame::Plane::kY),
                      reference->GetWritableVisibleData(VideoFrame::Plane::kU),
                      reference->stride(VideoFrame::Plane::kU),
                      reference->GetWritableVisibleData(VideoFrame::Plane::kV),
                      reference->stride(VideoFrame::Plane::kV),
                      kDirectDestRect.width(), kDirectDestRect.height()),
              0);
    VideoFrameConverter converter;
    ASSERT_TRUE(converter.ConvertAndScale(*source, *dest).is_ok());
    for (size_t plane = 0; plane < 3; ++plane) {
      for (int y = 0; y < dest->GetVisibleRows(plane); ++y) {
        for (int x = 0; x < dest->GetVisibleColumns(plane); ++x) {
          ASSERT_EQ(ReadVisibleSample(*dest, plane, x, y),
                    ReadVisibleSample(*reference, plane, x, y))
              << "plane=" << plane << ", x=" << x << ", y=" << y;
        }
      }
    }
    EXPECT_EQ(converter.get_pool_size_for_testing(), 0u);
  }
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

constexpr VideoPixelFormat kPx10Formats[] = {
    PIXEL_FORMAT_P010LE, PIXEL_FORMAT_P210LE, PIXEL_FORMAT_P410LE};

// Creates an RGBAF16 frame whose visible pixel (x, y) is `color(x, y)`. Skia
// converts the floats to half floats.
template <typename ColorFunction>
scoped_refptr<VideoFrame> CreateRGBAF16Frame(const gfx::Size& size,
                                             const gfx::ColorSpace& color_space,
                                             ColorFunction color) {
  auto frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_RGBAF16, size, gfx::Rect(size), size, base::TimeDelta());
  if (!frame) {
    return nullptr;
  }
  frame->set_color_space(color_space);
  const SkImageInfo f32_info = SkImageInfo::Make(
      size.width(), 1, kRGBA_F32_SkColorType, kUnpremul_SkAlphaType);
  const SkImageInfo f16_info = f32_info.makeColorType(kRGBA_F16_SkColorType);
  std::vector<SkColor4f> row(size.width());
  auto data = frame->GetWritableVisiblePlaneData(VideoFrame::Plane::kARGB);
  const size_t stride = frame->stride(VideoFrame::Plane::kARGB);
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      row[x] = color(x, y);
    }
    const SkPixmap f32_row(f32_info, row.data(), f32_info.minRowBytes());
    const SkPixmap f16_row(f16_info, data.subspan(y * stride).data(), stride);
    if (!f32_row.readPixels(f16_row)) {
      return nullptr;
    }
  }
  return frame;
}

scoped_refptr<VideoFrame> CreateSolidRGBAF16Frame(
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    const SkColor4f& color) {
  return CreateRGBAF16Frame(size, color_space, [&](int, int) { return color; });
}

scoped_refptr<VideoFrame> CreatePx10Frame(VideoPixelFormat format,
                                          const gfx::Size& size) {
  return VideoFrame::CreateZeroInitializedFrame(format, size, gfx::Rect(size),
                                                size, base::TimeDelta());
}

// Returns the 10-bit value of the `index`th sample of row `row` of a visible
// Px10 plane. The UV plane interleaves U and V, so chroma sample x has U at 2x
// and V at 2x + 1.
int Px10Sample(const VideoFrame& frame, size_t plane, int index, int row) {
  const auto data = base::subtle::reinterpret_span<const uint16_t>(
      frame.GetVisiblePlaneData(plane));
  return data[row * frame.stride(plane) / sizeof(uint16_t) + index] >> 6;
}

struct Yuv10 {
  int y;
  int u;
  int v;
};

// Expects every visible sample of a Px10 frame to be `expected` within 1, to
// allow for the rounding of the color space conversion.
void ExpectSolidPx10(const VideoFrame& frame, const Yuv10& expected) {
  for (int row = 0; row < frame.GetVisibleRows(VideoFrame::Plane::kY); ++row) {
    for (int x = 0; x < frame.GetVisibleColumns(VideoFrame::Plane::kY); ++x) {
      SCOPED_TRACE(testing::Message() << "Y (" << x << ", " << row << ")");
      ASSERT_NEAR(Px10Sample(frame, VideoFrame::Plane::kY, x, row), expected.y,
                  1);
    }
  }
  for (int row = 0; row < frame.GetVisibleRows(VideoFrame::Plane::kUV); ++row) {
    for (int x = 0; x < frame.GetVisibleColumns(VideoFrame::Plane::kUV); ++x) {
      SCOPED_TRACE(testing::Message() << "UV (" << x << ", " << row << ")");
      ASSERT_NEAR(Px10Sample(frame, VideoFrame::Plane::kUV, 2 * x, row),
                  expected.u, 1);
      ASSERT_NEAR(Px10Sample(frame, VideoFrame::Plane::kUV, 2 * x + 1, row),
                  expected.v, 1);
    }
  }
}

// The expected values follow from the BT.709 and BT.2020 equations at 10 bits,
// where the chroma midpoint is 512.
TEST(VideoFrameConverterRGBAF16Test, ConvertsWithDestinationMatrix) {
  constexpr gfx::Size kSize(6, 4);
  struct TestCase {
    gfx::ColorSpace src_color_space;
    SkColor4f color;
    Yuv10 expected;
  };
  const TestCase kTestCases[] = {
      // sRGB becomes full range BT.709 YUV.
      {gfx::ColorSpace::CreateSRGB(), SkColors::kWhite, {1023, 512, 512}},
      {gfx::ColorSpace::CreateSRGB(), SkColors::kBlack, {0, 512, 512}},
      // Y = 0.2126, U = 0.5 - 0.2126 / 1.8556 and V = 1, clamped.
      {gfx::ColorSpace::CreateSRGB(), SkColors::kRed, {217, 395, 1023}},
      // Values outside [0, 1] of extended range content are clamped.
      {gfx::ColorSpace::CreateSRGB(),
       {2.0f, 2.0f, 2.0f, 1.0f},
       {1023, 512, 512}},
      {gfx::ColorSpace::CreateSRGB(),
       {-1.0f, -1.0f, -1.0f, 1.0f},
       {0, 512, 512}},
      // PQ content keeps its transfer, so only the BT.2020 matrix applies.
      {gfx::ColorSpace::CreateHDR10(),
       {0.25f, 0.25f, 0.25f, 1.0f},
       {256, 512, 512}},
      // Y = 0.0593 * 0.5, U = 0.5 + 0.5 * 0.5 and V = 0.5 - 0.0593 / 1.4746 *
      // 0.5.
      {gfx::ColorSpace::CreateHDR10(),
       {0.0f, 0.0f, 0.5f, 1.0f},
       {30, 768, 491}},
  };
  for (const auto& test_case : kTestCases) {
    auto src = CreateSolidRGBAF16Frame(kSize, test_case.src_color_space,
                                       test_case.color);
    ASSERT_TRUE(src);
    for (auto format : kPx10Formats) {
      SCOPED_TRACE(testing::Message()
                   << test_case.src_color_space.ToString() << " ("
                   << test_case.color.fR << ", " << test_case.color.fG << ", "
                   << test_case.color.fB << ") -> " << format);
      auto dest = CreatePx10Frame(format, kSize);
      ASSERT_TRUE(dest);
      VideoFrameConverter converter;
      ASSERT_TRUE(converter.ConvertAndScale(*src, *dest).is_ok());
      EXPECT_EQ(dest->ColorSpace(),
                VideoFrameConverter::GetDestinationColorSpace(*src));
      ExpectSolidPx10(*dest, test_case.expected);
    }
  }
}

// An RGB source without a color space is treated like the 8-bit RGB formats:
// it is taken to be in the RGB form of limited range BT.709.
TEST(VideoFrameConverterRGBAF16Test, InvalidColorSpaceUsesLimitedRangeBT709) {
  constexpr gfx::Size kSize(4, 4);
  auto white =
      CreateSolidRGBAF16Frame(kSize, gfx::ColorSpace(), SkColors::kWhite);
  auto black =
      CreateSolidRGBAF16Frame(kSize, gfx::ColorSpace(), SkColors::kBlack);
  ASSERT_TRUE(white);
  ASSERT_TRUE(black);
  for (auto format : kPx10Formats) {
    SCOPED_TRACE(format);
    auto dest = CreatePx10Frame(format, kSize);
    ASSERT_TRUE(dest);
    VideoFrameConverter converter;
    ASSERT_TRUE(converter.ConvertAndScale(*white, *dest).is_ok());
    EXPECT_EQ(dest->ColorSpace(), gfx::ColorSpace::CreateREC709());
    ExpectSolidPx10(*dest, {940, 512, 512});
    ASSERT_TRUE(converter.ConvertAndScale(*black, *dest).is_ok());
    ExpectSolidPx10(*dest, {64, 512, 512});
  }
}

// The source is color managed into the destination color space, not only
// re-matrixed. Linear 0.05088 is sRGB encoded 0.25.
TEST(VideoFrameConverterRGBAF16Test, ColorManagesIntoDestinationTransfer) {
  constexpr gfx::Size kSize(4, 2);
  auto src = CreateSolidRGBAF16Frame(kSize, gfx::ColorSpace::CreateSRGBLinear(),
                                     {0.05088f, 0.05088f, 0.05088f, 1.0f});
  ASSERT_TRUE(src);
  for (auto format : kPx10Formats) {
    SCOPED_TRACE(format);
    auto dest = CreatePx10Frame(format, kSize);
    ASSERT_TRUE(dest);
    dest->set_color_space(gfx::ColorSpace(
        gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::SRGB,
        gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::FULL));
    ASSERT_TRUE(internals::RGBAF16ToPx10(*src, *dest));
    ExpectSolidPx10(*dest, {256, 512, 512});
  }
}

// Subsampled chroma is the average of the chroma of the pixels it covers,
// including the partial samples at odd edges.
TEST(VideoFrameConverterRGBAF16Test, AveragesSubsampledChroma) {
  // The second size has odd dimensions and spans several strips of rows,
  // the last of them partial.
  for (const gfx::Size size : {gfx::Size(5, 3), gfx::Size(13, 19)}) {
    SCOPED_TRACE(size.ToString());
    auto color = [](int x, int y) {
      return SkColor4f{(x % 6) * 0.2f, (y % 3) * 0.4f,
                       (x + y) % 2 ? 1.0f : 0.1f, 1.0f};
    };
    auto src = CreateRGBAF16Frame(size, gfx::ColorSpace::CreateSRGB(), color);
    ASSERT_TRUE(src);
    // Full resolution chroma is the reference to average.
    auto reference = CreatePx10Frame(PIXEL_FORMAT_P410LE, size);
    ASSERT_TRUE(reference);
    VideoFrameConverter converter;
    ASSERT_TRUE(converter.ConvertAndScale(*src, *reference).is_ok());

    for (auto format : {PIXEL_FORMAT_P010LE, PIXEL_FORMAT_P210LE}) {
      SCOPED_TRACE(format);
      auto dest = CreatePx10Frame(format, size);
      ASSERT_TRUE(dest);
      ASSERT_TRUE(converter.ConvertAndScale(*src, *dest).is_ok());
      const gfx::Size subsample =
          VideoFrame::SampleSize(format, VideoFrame::Plane::kUV);
      for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
          EXPECT_EQ(Px10Sample(*dest, VideoFrame::Plane::kY, x, y),
                    Px10Sample(*reference, VideoFrame::Plane::kY, x, y));
        }
      }
      for (int cy = 0; cy < dest->GetVisibleRows(VideoFrame::Plane::kUV);
           ++cy) {
        for (int cx = 0; cx < dest->GetVisibleColumns(VideoFrame::Plane::kUV);
             ++cx) {
          for (int component = 0; component < 2; ++component) {
            int sum = 0;
            int count = 0;
            for (int y = cy * subsample.height();
                 y < std::min((cy + 1) * subsample.height(), size.height());
                 ++y) {
              for (int x = cx * subsample.width();
                   x < std::min((cx + 1) * subsample.width(), size.width());
                   ++x) {
                sum += Px10Sample(*reference, VideoFrame::Plane::kUV,
                                  2 * x + component, y);
                ++count;
              }
            }
            SCOPED_TRACE(testing::Message() << "chroma (" << cx << ", " << cy
                                            << ") component " << component);
            // The reference is quantized before averaging, the result after.
            EXPECT_NEAR(Px10Sample(*dest, VideoFrame::Plane::kUV,
                                   2 * cx + component, cy),
                        static_cast<float>(sum) / count, 1.0f);
          }
        }
      }
    }
  }
}

TEST(VideoFrameConverterRGBAF16Test, ScalesSolidColor) {
  auto src = CreateSolidRGBAF16Frame({64, 48}, gfx::ColorSpace::CreateSRGB(),
                                     SkColors::kRed);
  ASSERT_TRUE(src);
  for (auto format : kPx10Formats) {
    for (const auto& size : {gfx::Size(32, 24), gfx::Size(100, 70)}) {
      SCOPED_TRACE(testing::Message() << format << ", " << size.ToString());
      auto dest = CreatePx10Frame(format, size);
      ASSERT_TRUE(dest);
      VideoFrameConverter converter;
      ASSERT_TRUE(converter.ConvertAndScale(*src, *dest).is_ok());
      ExpectSolidPx10(*dest, {217, 395, 1023});
    }
  }
}

// Alpha is dropped, not composited: the color is converted as is.
TEST(VideoFrameConverterRGBAF16Test, DropsAlpha) {
  constexpr gfx::Size kSize(4, 2);
  auto src = CreateSolidRGBAF16Frame(kSize, gfx::ColorSpace::CreateREC709(),
                                     {1.0f, 1.0f, 1.0f, 0.5f});
  ASSERT_TRUE(src);
  for (auto format : kPx10Formats) {
    SCOPED_TRACE(format);
    auto dest = CreatePx10Frame(format, kSize);
    ASSERT_TRUE(dest);
    VideoFrameConverter converter;
    ASSERT_TRUE(converter.ConvertAndScale(*src, *dest).is_ok());
    ExpectSolidPx10(*dest, {940, 512, 512});
  }
}

// Values outside [0, 1] are clipped in RGB before the matrix. Clipping the YUV
// components instead would turn 2.0 red into a different, more saturated color
// than plain red.
TEST(VideoFrameConverterRGBAF16Test, OutOfRangeIsClippedInRgb) {
  constexpr gfx::Size kSize(4, 2);
  const struct {
    SkColor4f color;
    Yuv10 expected;
  } kTestCases[] = {
      {{2.0f, 0.0f, 0.0f, 1.0f}, {250, 409, 960}},
      {{2.0f, 2.0f, 2.0f, 1.0f}, {940, 512, 512}},
      {{-1.0f, -1.0f, -1.0f, 1.0f}, {64, 512, 512}},
  };
  for (const auto& test_case : kTestCases) {
    auto src = CreateSolidRGBAF16Frame(kSize, gfx::ColorSpace::CreateREC709(),
                                       test_case.color);
    ASSERT_TRUE(src);
    for (auto format : kPx10Formats) {
      SCOPED_TRACE(testing::Message()
                   << format << ", " << test_case.color.fR << ", "
                   << test_case.color.fG << ", " << test_case.color.fB);
      auto dest = CreatePx10Frame(format, kSize);
      ASSERT_TRUE(dest);
      VideoFrameConverter converter;
      ASSERT_TRUE(converter.ConvertAndScale(*src, *dest).is_ok());
      ExpectSolidPx10(*dest, test_case.expected);
    }
  }
}

// RGBAF16 can only be converted to the 10-bit semi-planar formats.
TEST(VideoFrameConverterRGBAF16Test, UnsupportedDestinationFormat) {
  constexpr gfx::Size kSize(4, 2);
  auto src = CreateSolidRGBAF16Frame(kSize, gfx::ColorSpace::CreateREC709(),
                                     SkColors::kWhite);
  ASSERT_TRUE(src);
  auto dest = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, kSize, gfx::Rect(kSize), kSize, base::TimeDelta());
  ASSERT_TRUE(dest);
  VideoFrameConverter converter;
  EXPECT_EQ(converter.ConvertAndScale(*src, *dest).code(),
            EncoderStatus::Codes::kUnsupportedFrameFormat);
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

TEST(VideoFrameConverterColorSpaceTest, ColorSpaceConversion) {
  constexpr gfx::Size kTestSize(64, 64);
  auto src_frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_ARGB, kTestSize, gfx::Rect(kTestSize), kTestSize,
      base::TimeDelta());
  auto dest_frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, kTestSize, gfx::Rect(kTestSize), kTestSize,
      base::TimeDelta());

  // sRGB source (Full Range BT.709)
  src_frame->set_color_space(gfx::ColorSpace::CreateSRGB());
  VideoFrameConverter converter;
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->ColorSpace(),
            gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                            gfx::ColorSpace::TransferID::SRGB,
                            gfx::ColorSpace::MatrixID::BT709,
                            gfx::ColorSpace::RangeID::FULL));

  // Limited Range BT.709 source
  src_frame->set_color_space(gfx::ColorSpace::CreateREC709());
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->ColorSpace(), gfx::ColorSpace::CreateREC709());

  // Limited Range BT.601 source
  src_frame->set_color_space(gfx::ColorSpace::CreateREC601());
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->ColorSpace(), gfx::ColorSpace::CreateREC601());

  // BT.2020 Limited Range source
  auto bt2020_limited = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                        gfx::ColorSpace::TransferID::BT2020_10,
                                        gfx::ColorSpace::MatrixID::BT2020_NCL,
                                        gfx::ColorSpace::RangeID::LIMITED);
  src_frame->set_color_space(bt2020_limited);
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->ColorSpace(), bt2020_limited);

  // BT.2020 Full Range source
  auto bt2020_full = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                     gfx::ColorSpace::TransferID::BT2020_10,
                                     gfx::ColorSpace::MatrixID::BT2020_NCL,
                                     gfx::ColorSpace::RangeID::FULL);
  src_frame->set_color_space(bt2020_full);
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->ColorSpace(), bt2020_full);

  // HDR10 (BT.2020 with PQ transfer function) source
  src_frame->set_color_space(gfx::ColorSpace::CreateHDR10());
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->ColorSpace(),
            gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                            gfx::ColorSpace::TransferID::PQ,
                            gfx::ColorSpace::MatrixID::BT2020_NCL,
                            gfx::ColorSpace::RangeID::FULL));

  // HLG (BT.2020 with HLG transfer function) source
  src_frame->set_color_space(gfx::ColorSpace::CreateHLG());
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->ColorSpace(),
            gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                            gfx::ColorSpace::TransferID::HLG,
                            gfx::ColorSpace::MatrixID::BT2020_NCL,
                            gfx::ColorSpace::RangeID::FULL));

  // Display P3 SDR source
  src_frame->set_color_space(gfx::ColorSpace::CreateDisplayP3D65());
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->ColorSpace(),
            gfx::ColorSpace(gfx::ColorSpace::PrimaryID::P3,
                            gfx::ColorSpace::TransferID::SRGB,
                            gfx::ColorSpace::MatrixID::BT709,
                            gfx::ColorSpace::RangeID::FULL));

  // Display P3 with PQ transfer function source
  src_frame->set_color_space(gfx::ColorSpace(
      gfx::ColorSpace::PrimaryID::P3, gfx::ColorSpace::TransferID::PQ,
      gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL));
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->ColorSpace(),
            gfx::ColorSpace(gfx::ColorSpace::PrimaryID::P3,
                            gfx::ColorSpace::TransferID::PQ,
                            gfx::ColorSpace::MatrixID::BT2020_NCL,
                            gfx::ColorSpace::RangeID::FULL));

  // BT.601 source
  src_frame->set_color_space(gfx::ColorSpace::CreateREC601());
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->ColorSpace(), gfx::ColorSpace::CreateREC601());

  // HDR metadata propagation for HDR sources
  skhdr::ContentLightLevelInformation clli;
  clli.fMaxCLL = 1000.f;
  clli.fMaxFALL = 400.f;
  gfx::HDRMetadata hdr_metadata(clli);
  src_frame->set_color_space(gfx::ColorSpace::CreateHDR10());
  src_frame->set_hdr_metadata(hdr_metadata);
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->hdr_metadata(), hdr_metadata);

  // HDR metadata is not propagated to SDR destinations
  src_frame->set_color_space(gfx::ColorSpace::CreateSRGB());
  src_frame->set_hdr_metadata(hdr_metadata);
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->hdr_metadata(), gfx::HDRMetadata());

  // Invalid color space
  src_frame->set_color_space(gfx::ColorSpace());
  ASSERT_TRUE(converter.ConvertAndScale(*src_frame, *dest_frame).is_ok());
  EXPECT_EQ(dest_frame->ColorSpace(), gfx::ColorSpace::CreateREC709());
}

std::string PrintTestParams(const testing::TestParamInfo<TestParams>& info) {
  auto format_to_string = [](VideoPixelFormat format) {
    std::string name = VideoPixelFormatToString(format);
    const std::string prefix = "PIXEL_FORMAT_";
    if (name.starts_with(prefix)) {
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
    PIXEL_FORMAT_I422,       PIXEL_FORMAT_I422A,      PIXEL_FORMAT_I444,
    PIXEL_FORMAT_I444A,      PIXEL_FORMAT_NV12,       PIXEL_FORMAT_NV12A,
    PIXEL_FORMAT_NV16,       PIXEL_FORMAT_NV24,       PIXEL_FORMAT_YUV420P10,
    PIXEL_FORMAT_YUV422P10,  PIXEL_FORMAT_YUV444P10,  PIXEL_FORMAT_YUV420P12,
    PIXEL_FORMAT_YUV422P12,  PIXEL_FORMAT_YUV444P12,  PIXEL_FORMAT_YUV420AP10,
    PIXEL_FORMAT_YUV422AP10, PIXEL_FORMAT_YUV444AP10, PIXEL_FORMAT_P010LE,
    PIXEL_FORMAT_P210LE,     PIXEL_FORMAT_P410LE,
});

constexpr auto kOutputFormats = std::to_array<VideoPixelFormat>({
    PIXEL_FORMAT_I420,
    PIXEL_FORMAT_I420A,
    PIXEL_FORMAT_I422,
    PIXEL_FORMAT_I422A,
    PIXEL_FORMAT_I444,
    PIXEL_FORMAT_I444A,
    PIXEL_FORMAT_NV12,
    PIXEL_FORMAT_NV12A,
    PIXEL_FORMAT_NV16,
    PIXEL_FORMAT_NV24,
    PIXEL_FORMAT_P010LE,
    PIXEL_FORMAT_P210LE,
    PIXEL_FORMAT_P410LE,
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
