// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/mojo_shared_buffer_video_frame.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

TEST(MojoSharedBufferVideoFrameTest, CreateFrameWithSharedMemoryI420) {
  const int kWidth = 16;
  const int kHeight = 9;
  const base::TimeDelta kTimestamp = base::Microseconds(1337);

  // Create a MojoSharedBufferVideoFrame which will allocate enough space
  // to hold a 16x9 video frame.
  gfx::Size size(kWidth, kHeight);
  scoped_refptr<MojoSharedBufferVideoFrame> frame =
      MojoSharedBufferVideoFrame::CreateDefaultForTesting(PIXEL_FORMAT_I420,
                                                          size, kTimestamp);
  ASSERT_TRUE(frame.get());

  // Verify that the correct frame was allocated.
  EXPECT_EQ(media::PIXEL_FORMAT_I420, frame->format());

  // The offsets should be set appropriately.
  EXPECT_EQ(frame->PlaneOffset(VideoFrame::kYPlane), 0u);
  EXPECT_GT(frame->PlaneOffset(VideoFrame::kUPlane), 0u);
  EXPECT_GT(frame->PlaneOffset(VideoFrame::kVPlane), 0u);

  // The strides should be set appropriately.
  EXPECT_EQ(frame->stride(VideoFrame::kYPlane), kWidth);
  EXPECT_EQ(frame->stride(VideoFrame::kUPlane), kWidth / 2);
  EXPECT_EQ(frame->stride(VideoFrame::kVPlane), kWidth / 2);

  // The data pointers for each plane should be set.
  EXPECT_TRUE(frame->data(VideoFrame::kYPlane));
  EXPECT_TRUE(frame->data(VideoFrame::kUPlane));
  EXPECT_TRUE(frame->data(VideoFrame::kVPlane));
}

TEST(MojoSharedBufferVideoFrameTest, CreateFrameWithSharedMemoryNV12) {
  const int kWidth = 16;
  const int kHeight = 9;
  const base::TimeDelta kTimestamp = base::Microseconds(1337);

  // Create a MojoSharedBufferVideoFrame which will allocate enough space
  // to hold a 16x9 video frame.
  gfx::Size size(kWidth, kHeight);
  scoped_refptr<MojoSharedBufferVideoFrame> frame =
      MojoSharedBufferVideoFrame::CreateDefaultForTesting(PIXEL_FORMAT_NV12,
                                                          size, kTimestamp);
  ASSERT_TRUE(frame.get());

  // Verify that the correct frame was allocated.
  EXPECT_EQ(media::PIXEL_FORMAT_NV12, frame->format());

  // The offsets should be set appropriately.
  EXPECT_EQ(frame->PlaneOffset(VideoFrame::kYPlane), 0u);
  EXPECT_GT(frame->PlaneOffset(VideoFrame::kUVPlane), 0u);

  // The strides should be set appropriately.
  EXPECT_EQ(frame->stride(VideoFrame::kYPlane), kWidth);
  EXPECT_EQ(frame->stride(VideoFrame::kUVPlane), kWidth);

  // The data pointers for each plane should be set.
  EXPECT_TRUE(frame->data(VideoFrame::kYPlane));
  EXPECT_TRUE(frame->data(VideoFrame::kUVPlane));
}

TEST(MojoSharedBufferVideoFrameTest, CreateFrameAndPassSharedMemoryI420) {
  const int kWidth = 32;
  const int kHeight = 18;
  const base::TimeDelta kTimestamp = base::Microseconds(1338);

  // Some random values to use. Since we actually don't use the data inside the
  // frame, random values are fine (as long as the offsets are within the
  // memory size allocated).
  const VideoPixelFormat format = PIXEL_FORMAT_I420;
  const size_t y_offset = kWidth * 2;
  const size_t u_offset = kWidth * 3;
  const size_t v_offset = kWidth * 5;
  const int32_t y_stride = kWidth;
  const int32_t u_stride = kWidth - 1;
  const int32_t v_stride = kWidth - 2;

  // Allocate some shared memory.
  gfx::Size size(kWidth, kHeight);
  gfx::Rect visible_rect(size);
  size_t requested_size = VideoFrame::AllocationSize(format, size);
  ASSERT_LT(y_offset, requested_size);
  auto mapped_region = base::ReadOnlySharedMemoryRegion::Create(requested_size);
  ASSERT_TRUE(mapped_region.IsValid());

  // Allocate frame.
  const uint32_t offsets[] = {y_offset, u_offset, v_offset};
  const int32_t strides[] = {y_stride, u_stride, v_stride};
  scoped_refptr<MojoSharedBufferVideoFrame> frame =
      MojoSharedBufferVideoFrame::Create(format, size, visible_rect, size,
                                         std::move(mapped_region.region),
                                         offsets, strides, kTimestamp);
  ASSERT_TRUE(frame.get());
  EXPECT_EQ(frame->format(), format);

  // The offsets should be set appropriately.
  EXPECT_EQ(frame->PlaneOffset(VideoFrame::kYPlane), y_offset);
  EXPECT_EQ(frame->PlaneOffset(VideoFrame::kUPlane), u_offset);
  EXPECT_EQ(frame->PlaneOffset(VideoFrame::kVPlane), v_offset);

  // The strides should be set appropriately.
  EXPECT_EQ(frame->stride(VideoFrame::kYPlane), y_stride);
  EXPECT_EQ(frame->stride(VideoFrame::kUPlane), u_stride);
  EXPECT_EQ(frame->stride(VideoFrame::kVPlane), v_stride);

  // The data pointers for each plane should be set.
  EXPECT_TRUE(frame->data(VideoFrame::kYPlane));
  EXPECT_TRUE(frame->data(VideoFrame::kUPlane));
  EXPECT_TRUE(frame->data(VideoFrame::kVPlane));
}

TEST(MojoSharedBufferVideoFrameTest, CreateFrameAndPassSharedMemoryNV12) {
  const int kWidth = 32;
  const int kHeight = 18;
  const base::TimeDelta kTimestamp = base::Microseconds(1338);

  // Some random values to use. Since we actually don't use the data inside the
  // frame, random values are fine (as long as the offsets are within the
  // memory size allocated).
  const VideoPixelFormat format = PIXEL_FORMAT_NV12;
  const size_t y_offset = kWidth * 2;
  const size_t uv_offset = kWidth * 3;
  const int32_t y_stride = kWidth;
  const int32_t uv_stride = kWidth + 1;

  // Allocate some shared memory.
  gfx::Size size(kWidth, kHeight);
  gfx::Rect visible_rect(size);
  size_t requested_size = VideoFrame::AllocationSize(format, size);
  ASSERT_LT(y_offset, requested_size);
  auto mapped_region = base::ReadOnlySharedMemoryRegion::Create(requested_size);
  ASSERT_TRUE(mapped_region.IsValid());

  // Allocate frame.
  const uint32_t offsets[] = {y_offset, uv_offset};
  const int32_t strides[] = {y_stride, uv_stride};
  scoped_refptr<MojoSharedBufferVideoFrame> frame =
      MojoSharedBufferVideoFrame::Create(format, size, visible_rect, size,
                                         std::move(mapped_region.region),
                                         offsets, strides, kTimestamp);
  ASSERT_TRUE(frame.get());
  EXPECT_EQ(frame->format(), format);

  // The offsets should be set appropriately.
  EXPECT_EQ(frame->PlaneOffset(VideoFrame::kYPlane), y_offset);
  EXPECT_EQ(frame->PlaneOffset(VideoFrame::kUVPlane), uv_offset);

  // The strides should be set appropriately.
  EXPECT_EQ(frame->stride(VideoFrame::kYPlane), y_stride);
  EXPECT_EQ(frame->stride(VideoFrame::kUVPlane), uv_stride);

  // The data pointers for each plane should be set.
  EXPECT_TRUE(frame->data(VideoFrame::kYPlane));
  EXPECT_TRUE(frame->data(VideoFrame::kUVPlane));
}

TEST(MojoSharedBufferVideoFrameTest, CreateFrameOddWidth) {
  const int kWidth = 15;
  const int kHeight = 9;
  const base::TimeDelta kTimestamp = base::Microseconds(1337);

  VideoPixelFormat formats[] = {PIXEL_FORMAT_I420, PIXEL_FORMAT_NV12};
  for (auto format : formats) {
    // Create a MojoSharedBufferVideoFrame which will allocate enough space
    // to hold the video frame. Size should be adjusted.
    gfx::Size size(kWidth, kHeight);
    scoped_refptr<MojoSharedBufferVideoFrame> frame =
        MojoSharedBufferVideoFrame::CreateDefaultForTesting(format, size,
                                                            kTimestamp);
    ASSERT_TRUE(frame.get());

    // Verify that the correct frame was allocated.
    EXPECT_EQ(format, frame->format());

    // The size should be >= 15x9.
    EXPECT_GE(frame->coded_size().width(), kWidth);
    EXPECT_GE(frame->coded_size().height(), kHeight);
  }
}

TEST(MojoSharedBufferVideoFrameTest, InterleavedData) {
  const VideoPixelFormat format = PIXEL_FORMAT_I420;
  const int kWidth = 32;
  const int kHeight = 18;
  const base::TimeDelta kTimestamp = base::Microseconds(1338);
  gfx::Size size(kWidth, kHeight);
  gfx::Rect visible_rect(size);

  // Create interlaced UV data, which are each 1/4 the size of the Y data.
  const uint32_t y_offset = 0;
  const uint32_t u_offset =
      VideoFrame::PlaneSize(format, VideoFrame::kYPlane, size).GetArea();
  const uint32_t v_offset =
      u_offset + VideoFrame::RowBytes(VideoFrame::kUPlane, format, kWidth);
  const int32_t y_stride =
      VideoFrame::RowBytes(VideoFrame::kYPlane, format, kWidth);
  const int32_t u_stride = y_stride;
  const int32_t v_stride = y_stride;

  // Allocate some shared memory.
  size_t requested_size = VideoFrame::AllocationSize(format, size);
  auto mapped_region = base::ReadOnlySharedMemoryRegion::Create(requested_size);
  ASSERT_TRUE(mapped_region.IsValid());

  // Allocate frame.
  const uint32_t kOffsets[] = {y_offset, u_offset, v_offset};
  const int32_t kStrides[] = {y_stride, u_stride, v_stride};
  scoped_refptr<MojoSharedBufferVideoFrame> frame =
      MojoSharedBufferVideoFrame::Create(format, size, visible_rect, size,
                                         std::move(mapped_region.region),
                                         kOffsets, kStrides, kTimestamp);
  ASSERT_TRUE(frame.get());
  EXPECT_EQ(frame->format(), format);

  // The offsets should be set appropriately.
  EXPECT_EQ(frame->PlaneOffset(VideoFrame::kYPlane), y_offset);
  EXPECT_EQ(frame->PlaneOffset(VideoFrame::kUPlane), u_offset);
  EXPECT_EQ(frame->PlaneOffset(VideoFrame::kVPlane), v_offset);

  // The strides should be set appropriately.
  EXPECT_EQ(frame->stride(VideoFrame::kYPlane), y_stride);
  EXPECT_EQ(frame->stride(VideoFrame::kUPlane), u_stride);
  EXPECT_EQ(frame->stride(VideoFrame::kVPlane), v_stride);

  // The data pointers for each plane should be set.
  EXPECT_TRUE(frame->data(VideoFrame::kYPlane));
  EXPECT_TRUE(frame->data(VideoFrame::kUPlane));
  EXPECT_TRUE(frame->data(VideoFrame::kVPlane));
}

TEST(MojoSharedBufferVideoFrameTest, I420FrameToMojoFrame) {
  std::vector<uint8_t> data = std::vector<uint8_t>(12, 1u);
  const auto pixel_format = VideoPixelFormat::PIXEL_FORMAT_I420;
  const auto size = gfx::Size(1, 1);
  const int32_t stride = 3;

  // The YUV frame only has 1 pixel. But each plane are not in consecutive
  // memory block, also stride is 3 bytes that contains 1 byte image data and 2
  // bytes padding.
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      pixel_format, size, gfx::Rect(1, 1), size, stride, stride, stride,
      &data[0], &data[4], &data[8], base::TimeDelta());
  auto mojo_frame = MojoSharedBufferVideoFrame::CreateFromYUVFrame(*frame);
  EXPECT_TRUE(mojo_frame);

  const size_t y_stride = frame->stride(VideoFrame::kYPlane);
  const size_t u_stride = frame->stride(VideoFrame::kUPlane);

  // Verifies mapped size and offset.
  EXPECT_EQ(mojo_frame->shmem_region().GetSize(),
            static_cast<size_t>(3 * stride));
  EXPECT_EQ(mojo_frame->PlaneOffset(VideoFrame::kYPlane), 0u);
  EXPECT_EQ(mojo_frame->PlaneOffset(VideoFrame::kUPlane), y_stride);
  EXPECT_EQ(mojo_frame->PlaneOffset(VideoFrame::kVPlane), y_stride + u_stride);
}

TEST(MojoSharedBufferVideoFrameTest, NV12FrameToMojoFrame) {
  std::vector<uint8_t> data = std::vector<uint8_t>(12, 1u);
  const auto pixel_format = VideoPixelFormat::PIXEL_FORMAT_NV12;
  const auto size = gfx::Size(1, 1);
  const int32_t stride = 3;

  // The YUV frame only has 1 pixel. But each plane are not in consecutive
  // memory block, also stride is 3 bytes that contains 1 byte image data and 2
  // bytes padding in Y plane and 2 bytes image data and 1 byte padding in UV
  // plane.
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      pixel_format, size, gfx::Rect(1, 1), size, stride, stride, stride,
      &data[0], &data[4], &data[4], base::TimeDelta());
  auto mojo_frame = MojoSharedBufferVideoFrame::CreateFromYUVFrame(*frame);
  EXPECT_TRUE(mojo_frame);

  const size_t y_stride = frame->stride(VideoFrame::kYPlane);

  // Verifies mapped size and offset.
  EXPECT_EQ(mojo_frame->shmem_region().GetSize(),
            static_cast<size_t>(2 * stride));
  EXPECT_EQ(mojo_frame->PlaneOffset(VideoFrame::kYPlane), 0u);
  EXPECT_EQ(mojo_frame->PlaneOffset(VideoFrame::kUVPlane), y_stride);
}

TEST(MojoSharedBufferVideoFrameTest, I420SharedMemoryFrameToMojoFrame) {
  const auto pixel_format = VideoPixelFormat::PIXEL_FORMAT_I420;
  const auto size = gfx::Size(1, 1);
  const int32_t stride = 3;
  const size_t kAllocationSize = 12;
  auto region = base::UnsafeSharedMemoryRegion::Create(kAllocationSize);
  ASSERT_TRUE(region.IsValid());
  auto mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  uint8_t* data = static_cast<uint8_t*>(mapping.memory());
  // The YUV frame only has 1 pixel. But each plane are not in consecutive
  // memory block, also stride is 3 bytes that contains 1 byte image data and 2
  // bytes padding.
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      pixel_format, size, gfx::Rect(1, 1), size, stride, stride, stride, data,
      data + 4, data + 8, base::TimeDelta());
  frame->BackWithSharedMemory(&region);

  auto mojo_frame = MojoSharedBufferVideoFrame::CreateFromYUVFrame(*frame);
  EXPECT_TRUE(mojo_frame);

  const size_t y_stride = frame->stride(VideoFrame::kYPlane);
  const size_t u_stride = frame->stride(VideoFrame::kUPlane);

  // Verifies mapped size and offset.
  EXPECT_EQ(mojo_frame->shmem_region().GetSize(),
            static_cast<size_t>(3 * stride));
  EXPECT_EQ(mojo_frame->PlaneOffset(VideoFrame::kYPlane), 0u);
  EXPECT_EQ(mojo_frame->PlaneOffset(VideoFrame::kUPlane), y_stride);
  EXPECT_EQ(mojo_frame->PlaneOffset(VideoFrame::kVPlane), y_stride + u_stride);
}
}  // namespace media
