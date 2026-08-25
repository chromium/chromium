// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/video_frame_mac.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

const int kWidth = 64;
const int kHeight = 48;
const int kVisibleRectOffset = 8;
const base::TimeDelta kTimestamp = base::Microseconds(1337);

void Increment(int* i) {
  ++(*i);
}

void ExpectWrappedPixelBuffer(CVPixelBufferRef pb,
                              const VideoFrame& frame,
                              OSType cv_format) {
  ASSERT_TRUE(pb) << frame.format();
  EXPECT_EQ(cv_format, CVPixelBufferGetPixelFormatType(pb)) << frame.format();
  EXPECT_EQ(static_cast<size_t>(frame.coded_size().width()),
            CVPixelBufferGetWidth(pb));
  EXPECT_EQ(static_cast<size_t>(frame.coded_size().height()),
            CVPixelBufferGetHeight(pb));
  EXPECT_EQ(VideoFrame::NumPlanes(frame.format()),
            CVPixelBufferGetPlaneCount(pb));
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame.format()); ++i) {
    EXPECT_EQ(static_cast<size_t>(frame.columns(i)),
              CVPixelBufferGetWidthOfPlane(pb, i))
        << frame.format() << " plane " << i;
    EXPECT_EQ(static_cast<size_t>(frame.rows(i)),
              CVPixelBufferGetHeightOfPlane(pb, i))
        << frame.format() << " plane " << i;
  }
}

}  // namespace

TEST(VideoFrameMac, CheckBasicAttributes) {
  gfx::Size size(kWidth, kHeight);
  auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size, gfx::Rect(size),
                                       size, kTimestamp);
  ASSERT_TRUE(frame.get());

  auto pb = WrapVideoFrameInCVPixelBuffer(frame);
  ASSERT_TRUE(pb.get());

  const gfx::Size coded_size = frame->coded_size();
  const VideoPixelFormat format = frame->format();

  EXPECT_EQ(coded_size.width(),
            static_cast<int>(CVPixelBufferGetWidth(pb.get())));
  EXPECT_EQ(coded_size.height(),
            static_cast<int>(CVPixelBufferGetHeight(pb.get())));
  EXPECT_EQ(VideoFrame::NumPlanes(format),
            CVPixelBufferGetPlaneCount(pb.get()));

  CVPixelBufferLockBaseAddress(pb.get(), 0);
  for (size_t i = 0; i < VideoFrame::NumPlanes(format); ++i) {
    const gfx::Size plane_size = VideoFrame::PlaneSize(format, i, coded_size);
    EXPECT_EQ(plane_size.width(),
              static_cast<int>(CVPixelBufferGetWidthOfPlane(pb.get(), i)));
    EXPECT_EQ(plane_size.height(),
              static_cast<int>(CVPixelBufferGetHeightOfPlane(pb.get(), i)));
    EXPECT_EQ(frame->data(i), CVPixelBufferGetBaseAddressOfPlane(pb.get(), i));
  }
  CVPixelBufferUnlockBaseAddress(pb.get(), 0);
}

TEST(VideoFrameMac, CheckFormats) {
  const struct SupportedFormat {
    VideoPixelFormat pixel_format;
    OSType video_range;
    OSType full_range;
  } kSupportedCases[] = {
      {PIXEL_FORMAT_I420, kCVPixelFormatType_420YpCbCr8Planar,
       kCVPixelFormatType_420YpCbCr8PlanarFullRange},
      {PIXEL_FORMAT_NV12, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
       kCVPixelFormatType_420YpCbCr8BiPlanarFullRange},
      {PIXEL_FORMAT_NV16, kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange,
       kCVPixelFormatType_422YpCbCr8BiPlanarFullRange},
      {PIXEL_FORMAT_NV24, kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange,
       kCVPixelFormatType_444YpCbCr8BiPlanarFullRange},
      {PIXEL_FORMAT_P010LE, kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
       kCVPixelFormatType_420YpCbCr10BiPlanarFullRange},
      {PIXEL_FORMAT_P210LE, kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange,
       kCVPixelFormatType_422YpCbCr10BiPlanarFullRange},
      {PIXEL_FORMAT_P410LE, kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange,
       kCVPixelFormatType_444YpCbCr10BiPlanarFullRange},
  };
  constexpr VideoPixelFormat kUnsupportedCases[] = {
      PIXEL_FORMAT_YV12, PIXEL_FORMAT_I422, PIXEL_FORMAT_I420A,
      PIXEL_FORMAT_I444};

  gfx::Size size(kWidth, kHeight);
  for (const auto& format : kUnsupportedCases) {
    auto frame = VideoFrame::CreateFrame(format, size, gfx::Rect(size), size,
                                         kTimestamp);
    ASSERT_TRUE(frame.get()) << format;
    EXPECT_FALSE(
        CVPixelFormatForVideoFrame(format, gfx::ColorSpace::RangeID::LIMITED)
            .has_value())
        << format;
    EXPECT_FALSE(
        CVPixelFormatForVideoFrame(format, gfx::ColorSpace::RangeID::FULL)
            .has_value())
        << format;

    auto pb = WrapVideoFrameInCVPixelBuffer(frame);
    EXPECT_EQ(nullptr, pb.get()) << format;
  }

  for (const auto& test_case : kSupportedCases) {
    auto frame = VideoFrame::CreateFrame(test_case.pixel_format, size,
                                         gfx::Rect(size), size, kTimestamp);
    ASSERT_TRUE(frame.get()) << test_case.pixel_format;
    EXPECT_EQ(test_case.video_range,
              CVPixelFormatForVideoFrame(test_case.pixel_format,
                                         gfx::ColorSpace::RangeID::LIMITED)
                  .value_or(0))
        << test_case.pixel_format;
    EXPECT_EQ(test_case.full_range,
              CVPixelFormatForVideoFrame(test_case.pixel_format,
                                         gfx::ColorSpace::RangeID::FULL)
                  .value_or(0))
        << test_case.pixel_format;
    auto pb = WrapVideoFrameInCVPixelBuffer(frame);

    EXPECT_TRUE(IsAcceptableCvPixelFormat(test_case.pixel_format,
                                          test_case.video_range))
        << test_case.pixel_format;
    EXPECT_TRUE(
        IsAcceptableCvPixelFormat(test_case.pixel_format, test_case.full_range))
        << test_case.pixel_format;

    ExpectWrappedPixelBuffer(pb.get(), *frame, test_case.video_range);

    frame->set_color_space(gfx::ColorSpace(
        gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::BT709,
        gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::FULL));
    auto full_pb = WrapVideoFrameInCVPixelBuffer(frame);
    ExpectWrappedPixelBuffer(full_pb.get(), *frame, test_case.full_range);
  }
}

TEST(VideoFrameMac, CheckNV12AFormat) {
  constexpr OSType kCVFormat =
      kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar;
  gfx::Size size(kWidth, kHeight);
  auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_NV12A, size,
                                       gfx::Rect(size), size, kTimestamp);
  ASSERT_TRUE(frame.get());

  EXPECT_EQ(kCVFormat,
            CVPixelFormatForVideoFrame(PIXEL_FORMAT_NV12A,
                                       gfx::ColorSpace::RangeID::LIMITED)
                .value_or(0));
  EXPECT_FALSE(CVPixelFormatForVideoFrame(PIXEL_FORMAT_NV12A,
                                          gfx::ColorSpace::RangeID::FULL)
                   .has_value());
  EXPECT_TRUE(IsAcceptableCvPixelFormat(PIXEL_FORMAT_NV12A, kCVFormat));

  auto pb = WrapVideoFrameInCVPixelBuffer(frame);
  ExpectWrappedPixelBuffer(pb.get(), *frame, kCVFormat);

  frame->set_color_space(gfx::ColorSpace(
      gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::BT709,
      gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::FULL));
  auto full_pb = WrapVideoFrameInCVPixelBuffer(frame);
  ExpectWrappedPixelBuffer(full_pb.get(), *frame, kCVFormat);
}

TEST(VideoFrameMac, AcceptsLosslessIOSurfaceFormats) {
  const struct {
    VideoPixelFormat pixel_format;
    OSType lossless_cv_format;
    gfx::ColorSpace::RangeID range;
  } kCases[] = {
      {PIXEL_FORMAT_NV12,
       kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarVideoRange,
       gfx::ColorSpace::RangeID::LIMITED},
      {PIXEL_FORMAT_NV12,
       kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarFullRange,
       gfx::ColorSpace::RangeID::FULL},
      {PIXEL_FORMAT_P010LE,
       kCVPixelFormatType_Lossless_420YpCbCr10PackedBiPlanarVideoRange,
       gfx::ColorSpace::RangeID::LIMITED},
      {PIXEL_FORMAT_P010LE,
       kCVPixelFormatType_Lossless_420YpCbCr10PackedBiPlanarFullRange,
       gfx::ColorSpace::RangeID::FULL},
      {PIXEL_FORMAT_P210LE,
       kCVPixelFormatType_Lossless_422YpCbCr10PackedBiPlanarVideoRange,
       gfx::ColorSpace::RangeID::LIMITED},
  };

  for (const auto& test_case : kCases) {
    EXPECT_TRUE(IsAcceptableCvPixelFormat(test_case.pixel_format,
                                          test_case.lossless_cv_format))
        << test_case.pixel_format;
    EXPECT_NE(
        test_case.lossless_cv_format,
        CVPixelFormatForVideoFrame(test_case.pixel_format, test_case.range)
            .value_or(0))
        << test_case.pixel_format;
  }
}

TEST(VideoFrameMac, CheckLifetime) {
  gfx::Size size(kWidth, kHeight);
  auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size, gfx::Rect(size),
                                       size, kTimestamp);
  ASSERT_TRUE(frame.get());

  int instances_destroyed = 0;
  auto wrapper_frame = VideoFrame::WrapVideoFrame(
      frame, frame->format(), frame->visible_rect(), frame->natural_size());
  wrapper_frame->AddDestructionObserver(
      base::BindOnce(&Increment, &instances_destroyed));
  ASSERT_TRUE(wrapper_frame.get());

  auto pb = WrapVideoFrameInCVPixelBuffer(wrapper_frame);
  ASSERT_TRUE(pb.get());

  wrapper_frame = nullptr;
  EXPECT_EQ(0, instances_destroyed);
  pb.reset();
  EXPECT_EQ(1, instances_destroyed);
}

static void FillFrameWithPredictableValues(const VideoFrame& frame) {
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame.format()); ++i) {
    const gfx::Size& size =
        VideoFrame::PlaneSize(frame.format(), i, frame.coded_size());
    uint8_t* plane_ptr = const_cast<uint8_t*>(frame.data(i));
    for (int h = 0; h < size.height(); ++h) {
      const int row_index = h * frame.stride(i);
      for (int w = 0; w < size.width(); ++w) {
        const int index = row_index + w;
        UNSAFE_TODO(plane_ptr[index]) = static_cast<uint8_t>(w ^ h);
      }
    }
  }
}

TEST(VideoFrameMac, CorrectlyWrapsFramesWithPadding) {
  const gfx::Size coded_size(kWidth, kHeight);  // 64x48
  const gfx::Rect visible_rect(
      kVisibleRectOffset, kVisibleRectOffset, kWidth - 2 * kVisibleRectOffset,
      kHeight - 2 * kVisibleRectOffset);  // (8, 8, 48, 32)
  auto frame =
      VideoFrame::CreateFrame(PIXEL_FORMAT_I420, coded_size, visible_rect,
                              visible_rect.size(), kTimestamp);
  ASSERT_TRUE(frame);
  FillFrameWithPredictableValues(*frame);

  auto pb = WrapVideoFrameInCVPixelBuffer(frame);
  ASSERT_TRUE(pb.get());
  EXPECT_EQ(kCVPixelFormatType_420YpCbCr8Planar,
            CVPixelBufferGetPixelFormatType(pb.get()));

  // 1. CVPixelBuffer should reflect the full CODED size under Approach #2
  EXPECT_EQ(coded_size.width(),
            static_cast<int>(CVPixelBufferGetWidth(pb.get())));
  EXPECT_EQ(coded_size.height(),
            static_cast<int>(CVPixelBufferGetHeight(pb.get())));

  // 2. Retrieve and verify the Clean Aperture crop dict using base helpers
  CFDictionaryRef clean_aperture =
      base::apple::CFCast<CFDictionaryRef>(CVBufferCopyAttachment(
          pb.get(), kCVImageBufferCleanApertureKey, nullptr));
  ASSERT_NE(clean_aperture, nullptr);

  // Verify Width (48)
  double width = 0;
  CFNumberRef width_num = base::apple::GetValueFromDictionary<CFNumberRef>(
      clean_aperture, kCVImageBufferCleanApertureWidthKey);
  ASSERT_NE(width_num, nullptr);
  CFNumberGetValue(width_num, kCFNumberDoubleType, &width);
  EXPECT_EQ(width, visible_rect.width());

  // Verify Height (32)
  double height = 0;
  CFNumberRef height_num = base::apple::GetValueFromDictionary<CFNumberRef>(
      clean_aperture, kCVImageBufferCleanApertureHeightKey);
  ASSERT_NE(height_num, nullptr);
  CFNumberGetValue(height_num, kCFNumberDoubleType, &height);
  EXPECT_EQ(height, visible_rect.height());

  // Verify Horizontal Offset: 8 - (64 - 48) / 2.0 = 0
  double horiz_off = 0;
  CFNumberRef horiz_off_num = base::apple::GetValueFromDictionary<CFNumberRef>(
      clean_aperture, kCVImageBufferCleanApertureHorizontalOffsetKey);
  ASSERT_NE(horiz_off_num, nullptr);
  CFNumberGetValue(horiz_off_num, kCFNumberDoubleType, &horiz_off);
  EXPECT_EQ(horiz_off, 0.0);

  // Verify Vertical Offset: 8 - (48 - 32) / 2.0 = 0
  double vert_off = 0;
  CFNumberRef vert_off_num = base::apple::GetValueFromDictionary<CFNumberRef>(
      clean_aperture, kCVImageBufferCleanApertureVerticalOffsetKey);
  ASSERT_NE(vert_off_num, nullptr);
  CFNumberGetValue(vert_off_num, kCFNumberDoubleType, &vert_off);
  EXPECT_EQ(vert_off, 0.0);

  CVPixelBufferLockBaseAddress(pb.get(), 0);
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame->format()); ++i) {
    // 3. Plane dimensions in CVPixelBuffer should reflect the full coded size
    const gfx::Size plane_size =
        VideoFrame::PlaneSize(frame->format(), i, coded_size);
    EXPECT_EQ(plane_size.width(),
              static_cast<int>(CVPixelBufferGetWidthOfPlane(pb.get(), i)));
    EXPECT_EQ(plane_size.height(),
              static_cast<int>(CVPixelBufferGetHeightOfPlane(pb.get(), i)));

    uint8_t* plane_ptr = reinterpret_cast<uint8_t*>(
        CVPixelBufferGetBaseAddressOfPlane(pb.get(), i));

    // 4. Pointer should match frame->data() instead of frame->visible_data()
    ASSERT_EQ(frame->data(i), plane_ptr);

    const size_t stride =
        static_cast<size_t>(CVPixelBufferGetBytesPerRowOfPlane(pb.get(), i));
    ASSERT_EQ(frame->stride(i), stride);

    // 5. Verify pixel contents across the full coded frame
    auto frame_data = frame->data_span(i);
    for (int h = 0; h < plane_size.height(); ++h) {
      const int row_index = h * stride;
      for (int w = 0; w < plane_size.width(); ++w) {
        const int index = row_index + w;
        EXPECT_EQ(static_cast<uint8_t>(w ^ h), frame_data[index]);
      }
    }
  }
  CVPixelBufferUnlockBaseAddress(pb.get(), 0);
}

}  // namespace media
