// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/mac/video_frame_mac.h"

#include <stddef.h>

#include <utility>

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

struct FormatPair {
  VideoPixelFormat chrome;
  OSType corevideo;
};

void Increment(int* i) {
  ++(*i);
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
  // CreateFrame() does not support non planar YUV, e.g. NV12.
  const FormatPair format_pairs[] = {
      {PIXEL_FORMAT_I420, kCVPixelFormatType_420YpCbCr8Planar},
      {PIXEL_FORMAT_YV12, 0},
      {PIXEL_FORMAT_I422, 0},
      {PIXEL_FORMAT_I420A, 0},
      {PIXEL_FORMAT_I444, 0},
  };

  gfx::Size size(kWidth, kHeight);
  for (const auto& format_pair : format_pairs) {
    auto frame = VideoFrame::CreateFrame(format_pair.chrome, size,
                                         gfx::Rect(size), size, kTimestamp);
    ASSERT_TRUE(frame.get());
    auto pb = WrapVideoFrameInCVPixelBuffer(frame);
    if (format_pair.corevideo) {
      EXPECT_EQ(format_pair.corevideo,
                CVPixelBufferGetPixelFormatType(pb.get()));
    } else {
      EXPECT_EQ(nullptr, pb.get());
    }
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

TEST(VideoFrameMac, CheckWrapperFrame) {
  const FormatPair format_pairs[] = {
      {PIXEL_FORMAT_I420, kCVPixelFormatType_420YpCbCr8Planar},
      {PIXEL_FORMAT_NV12, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange},
  };

  for (const auto& format_pair : format_pairs) {
    base::apple::ScopedCFTypeRef<CVPixelBufferRef> pb;
    CVPixelBufferCreate(nullptr, kWidth, kHeight, format_pair.corevideo,
                        nullptr, pb.InitializeInto());
    ASSERT_TRUE(pb.get());

    auto frame = VideoFrame::WrapCVPixelBuffer(pb.get(), kTimestamp);
    ASSERT_TRUE(frame.get());
    EXPECT_EQ(pb.get(), frame->CvPixelBuffer());
    EXPECT_EQ(format_pair.chrome, frame->format());

    frame = nullptr;
    EXPECT_EQ(1, CFGetRetainCount(pb.get()));
  }
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
        plane_ptr[index] = static_cast<uint8_t>(w ^ h);
      }
    }
  }
}

TEST(VideoFrameMac, CorrectlyWrapsFramesWithPadding) {
  const gfx::Size coded_size(kWidth, kHeight);
  const gfx::Rect visible_rect(kVisibleRectOffset, kVisibleRectOffset,
                               kWidth - 2 * kVisibleRectOffset,
                               kHeight - 2 * kVisibleRectOffset);
  auto frame =
      VideoFrame::CreateFrame(PIXEL_FORMAT_I420, coded_size, visible_rect,
                              visible_rect.size(), kTimestamp);
  ASSERT_TRUE(frame.get());
  FillFrameWithPredictableValues(*frame);

  auto pb = WrapVideoFrameInCVPixelBuffer(frame);
  ASSERT_TRUE(pb.get());
  EXPECT_EQ(kCVPixelFormatType_420YpCbCr8Planar,
            CVPixelBufferGetPixelFormatType(pb.get()));
  EXPECT_EQ(visible_rect.width(),
            static_cast<int>(CVPixelBufferGetWidth(pb.get())));
  EXPECT_EQ(visible_rect.height(),
            static_cast<int>(CVPixelBufferGetHeight(pb.get())));

  CVPixelBufferLockBaseAddress(pb.get(), 0);
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame->format()); ++i) {
    const gfx::Size plane_size =
        VideoFrame::PlaneSize(frame->format(), i, visible_rect.size());
    EXPECT_EQ(plane_size.width(),
              static_cast<int>(CVPixelBufferGetWidthOfPlane(pb.get(), i)));
    EXPECT_EQ(plane_size.height(),
              static_cast<int>(CVPixelBufferGetHeightOfPlane(pb.get(), i)));

    uint8_t* plane_ptr = reinterpret_cast<uint8_t*>(
        CVPixelBufferGetBaseAddressOfPlane(pb.get(), i));
    EXPECT_EQ(frame->visible_data(i), plane_ptr);
    const int stride =
        static_cast<int>(CVPixelBufferGetBytesPerRowOfPlane(pb.get(), i));
    EXPECT_EQ(frame->stride(i), stride);
    const int offset = kVisibleRectOffset / ((i == 0) ? 1 : 2);
    for (int h = 0; h < plane_size.height(); ++h) {
      const int row_index = h * stride;
      for (int w = 0; w < plane_size.width(); ++w) {
        const int index = row_index + w;
        EXPECT_EQ(static_cast<uint8_t>((w + offset) ^ (h + offset)),
                  plane_ptr[index]);
      }
    }
  }
  CVPixelBufferUnlockBaseAddress(pb.get(), 0);
}

}  // namespace media
