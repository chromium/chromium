// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/video_frame_mac.h"

#include <CoreVideo/CoreVideo.h>
#include <stddef.h>

#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
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
  if (CVPixelBufferIsPlanar(pb)) {
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
  } else {
    EXPECT_EQ(1u, VideoFrame::NumPlanes(frame.format()));
    EXPECT_EQ(0u, CVPixelBufferGetPlaneCount(pb));
  }
}

base::apple::ScopedCFTypeRef<CVPixelBufferRef>
CreateIOSurfaceBackedPixelBuffer() {
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> io_surface_properties(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> pixel_buffer_attributes(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 1,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(pixel_buffer_attributes.get(),
                       kCVPixelBufferIOSurfacePropertiesKey,
                       io_surface_properties.get());

  base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer;
  const CVReturn err = CVPixelBufferCreate(
      kCFAllocatorDefault, kWidth, kHeight,
      kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
      pixel_buffer_attributes.get(), pixel_buffer.InitializeInto());
  EXPECT_EQ(err, kCVReturnSuccess);
  return pixel_buffer;
}

double GetCleanApertureNumber(CFDictionaryRef clean_aperture, CFStringRef key) {
  double value = 0;
  CFNumberRef number =
      base::apple::GetValueFromDictionary<CFNumberRef>(clean_aperture, key);
  EXPECT_NE(number, nullptr);
  if (number) {
    CFNumberGetValue(number, kCFNumberDoubleType, &value);
  }
  return value;
}

void ExpectCleanAperture(CVPixelBufferRef pb,
                         double expected_width,
                         double expected_height,
                         double expected_horizontal_offset,
                         double expected_vertical_offset) {
  CVAttachmentMode attachment_mode = kCVAttachmentMode_ShouldPropagate;
  base::apple::ScopedCFTypeRef<CFTypeRef> clean_aperture_ref(
      CVBufferCopyAttachment(pb, kCVImageBufferCleanApertureKey,
                             &attachment_mode));
  CFDictionaryRef clean_aperture =
      base::apple::CFCast<CFDictionaryRef>(clean_aperture_ref.get());
  ASSERT_NE(clean_aperture, nullptr);
  EXPECT_EQ(attachment_mode, kCVAttachmentMode_ShouldNotPropagate);
  EXPECT_EQ(GetCleanApertureNumber(clean_aperture,
                                   kCVImageBufferCleanApertureWidthKey),
            expected_width);
  EXPECT_EQ(GetCleanApertureNumber(clean_aperture,
                                   kCVImageBufferCleanApertureHeightKey),
            expected_height);
  EXPECT_EQ(GetCleanApertureNumber(
                clean_aperture, kCVImageBufferCleanApertureHorizontalOffsetKey),
            expected_horizontal_offset);
  EXPECT_EQ(GetCleanApertureNumber(
                clean_aperture, kCVImageBufferCleanApertureVerticalOffsetKey),
            expected_vertical_offset);
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
      {PIXEL_FORMAT_ARGB, kCVPixelFormatType_32BGRA, kCVPixelFormatType_32BGRA},
      {PIXEL_FORMAT_XRGB, kCVPixelFormatType_32BGRA, kCVPixelFormatType_32BGRA},
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
      PIXEL_FORMAT_I444, PIXEL_FORMAT_ABGR, PIXEL_FORMAT_XBGR,
      PIXEL_FORMAT_BGRA};

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

  // 2. Retrieve and verify the Clean Aperture crop dict. Offset is relative to
  // the image center: 8 - (64 - 48) / 2.0 = 0, 8 - (48 - 32) / 2.0 = 0.
  ExpectCleanAperture(pb.get(), visible_rect.width(), visible_rect.height(),
                      /*expected_horizontal_offset=*/0.0,
                      /*expected_vertical_offset=*/0.0);

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

TEST(VideoFrameMac, DoesNotPropagateCleanApertureToSharedIOSurface) {
  auto source_pb = CreateIOSurfaceBackedPixelBuffer();
  ASSERT_TRUE(source_pb.get());
  IOSurfaceRef io_surface = CVPixelBufferGetIOSurface(source_pb.get());
  ASSERT_TRUE(io_surface);

  const gfx::Size coded_size(kWidth, kHeight);
  const gfx::Rect crop_a(0, 0, 48, 32);
  const gfx::Rect crop_b(16, 16, 32, 16);
  auto frame_a = VideoFrame::CreateFrame(PIXEL_FORMAT_NV12, coded_size, crop_a,
                                         crop_a.size(), kTimestamp);
  auto frame_b = VideoFrame::CreateFrame(PIXEL_FORMAT_NV12, coded_size, crop_b,
                                         crop_b.size(), kTimestamp);
  ASSERT_TRUE(frame_a);
  ASSERT_TRUE(frame_b);

  auto pb_a = WrapIOSurfaceInCVPixelBuffer(*frame_a, io_surface);
  auto pb_b = WrapIOSurfaceInCVPixelBuffer(*frame_b, io_surface);
  ASSERT_TRUE(pb_a.get());
  ASSERT_TRUE(pb_b.get());

  // Each wrapper must keep its own crop even though they share an IOSurface.
  // Horizontal offset: x - (coded_width - crop_width) / 2.
  // crop_a: 0 - (64 - 48) / 2.0 = -8
  // crop_b: 16 - (64 - 32) / 2.0 = 0
  ExpectCleanAperture(pb_a.get(), crop_a.width(), crop_a.height(), -8.0, -8.0);
  ExpectCleanAperture(pb_b.get(), crop_b.width(), crop_b.height(), 0.0, 0.0);

  // A later wrap with no crop must not inherit a previous wrapper's aperture
  // from the shared IOSurface.
  auto frame_full =
      VideoFrame::CreateFrame(PIXEL_FORMAT_NV12, coded_size,
                              gfx::Rect(coded_size), coded_size, kTimestamp);
  ASSERT_TRUE(frame_full);
  auto pb_full = WrapIOSurfaceInCVPixelBuffer(*frame_full, io_surface);
  ASSERT_TRUE(pb_full.get());
  EXPECT_FALSE(
      CVBufferHasAttachment(pb_full.get(), kCVImageBufferCleanApertureKey));
}

}  // namespace media
