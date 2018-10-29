// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/mojo_shared_buffer_video_frame.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

void CompareDestructionCallbackValues(
    mojo::SharedBufferHandle expected_handle,
    size_t expected_handle_size,
    bool* callback_called,
    mojo::ScopedSharedBufferHandle actual_handle,
    size_t actual_handle_size) {
  // Compare expected vs actual. Ownership of the memory is transferred with
  // |actual_handle|, thus it is a ScopedSharedBufferHandle.
  EXPECT_EQ(expected_handle, actual_handle.get());
  EXPECT_EQ(expected_handle_size, actual_handle_size);
  *callback_called = true;
}

}  // namespace

TEST(MojoSharedBufferVideoFrameTest, CreateFrameWithSharedMemory) {
  const int kWidth = 16;
  const int kHeight = 9;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  // Create a MojoSharedBufferVideoFrame which will allocate enough space
  // to hold a 16x9 video frame.
  gfx::Size size(kWidth, kHeight);
  scoped_refptr<MojoSharedBufferVideoFrame> frame =
      MojoSharedBufferVideoFrame::CreateDefaultI420ForTesting(size, kTimestamp);
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

TEST(MojoSharedBufferVideoFrameTest, CreateFrameAndPassSharedMemory) {
  const int kWidth = 32;
  const int kHeight = 18;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1338);

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
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(requested_size);
  ASSERT_TRUE(handle.is_valid());

  // Allocate frame.
  scoped_refptr<MojoSharedBufferVideoFrame> frame =
      MojoSharedBufferVideoFrame::Create(format, size, visible_rect, size,
                                         std::move(handle), requested_size,
                                         y_offset, u_offset, v_offset, y_stride,
                                         u_stride, v_stride, kTimestamp);
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

TEST(MojoSharedBufferVideoFrameTest, CreateFrameOddWidth) {
  const int kWidth = 15;
  const int kHeight = 9;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  // Create a MojoSharedBufferVideoFrame which will allocate enough space
  // to hold the video frame. Size should be adjusted.
  gfx::Size size(kWidth, kHeight);
  scoped_refptr<MojoSharedBufferVideoFrame> frame =
      MojoSharedBufferVideoFrame::CreateDefaultI420ForTesting(size, kTimestamp);
  ASSERT_TRUE(frame.get());

  // Verify that the correct frame was allocated.
  EXPECT_EQ(media::PIXEL_FORMAT_I420, frame->format());

  // The size should be >= 15x9.
  EXPECT_GE(frame->coded_size().width(), kWidth);
  EXPECT_GE(frame->coded_size().height(), kHeight);
}

TEST(MojoSharedBufferVideoFrameTest, TestDestructionCallback) {
  const VideoPixelFormat format = PIXEL_FORMAT_I420;
  const int kWidth = 32;
  const int kHeight = 18;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1338);

  // Allocate some shared memory.
  gfx::Size size(kWidth, kHeight);
  gfx::Rect visible_rect(size);
  size_t requested_size = VideoFrame::AllocationSize(format, size);
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(requested_size);
  ASSERT_TRUE(handle.is_valid());

  // Keep track of the original handle. MojoSharedBufferVideoFrame::Create()
  // will get ownership of the memory.
  mojo::SharedBufferHandle original_handle = handle.get();

  // Allocate frame.
  scoped_refptr<MojoSharedBufferVideoFrame> frame =
      MojoSharedBufferVideoFrame::Create(
          format, size, visible_rect, size, std::move(handle), requested_size,
          0, 0, 0, kWidth, kWidth, kWidth, kTimestamp);
  ASSERT_TRUE(frame.get());
  EXPECT_EQ(frame->format(), format);

  // Set the destruction callback.
  bool callback_called = false;
  frame->SetMojoSharedBufferDoneCB(base::Bind(&CompareDestructionCallbackValues,
                                              original_handle, requested_size,
                                              &callback_called));
  EXPECT_FALSE(callback_called);

  // Force destruction of |frame|.
  frame = nullptr;
  EXPECT_TRUE(callback_called);
}

TEST(MojoSharedBufferVideoFrameTest, InterleavedData) {
  const VideoPixelFormat format = PIXEL_FORMAT_I420;
  const int kWidth = 32;
  const int kHeight = 18;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1338);
  gfx::Size size(kWidth, kHeight);
  gfx::Rect visible_rect(size);

  // Create interlaced UV data, which are each 1/4 the size of the Y data.
  const size_t y_offset = 0;
  const size_t u_offset =
      VideoFrame::PlaneSize(format, VideoFrame::kYPlane, size).GetArea();
  const size_t v_offset =
      u_offset + VideoFrame::RowBytes(VideoFrame::kUPlane, format, kWidth);
  const int32_t y_stride =
      VideoFrame::RowBytes(VideoFrame::kYPlane, format, kWidth);
  const int32_t u_stride = y_stride;
  const int32_t v_stride = y_stride;

  // Allocate some shared memory.
  size_t requested_size = VideoFrame::AllocationSize(format, size);
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(requested_size);
  ASSERT_TRUE(handle.is_valid());

  // Allocate frame.
  scoped_refptr<MojoSharedBufferVideoFrame> frame =
      MojoSharedBufferVideoFrame::Create(format, size, visible_rect, size,
                                         std::move(handle), requested_size,
                                         y_offset, u_offset, v_offset, y_stride,
                                         u_stride, v_stride, kTimestamp);
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

TEST(MojoSharedBufferVideoFrameTest, YUVFrameToMojoFrame) {
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
  EXPECT_EQ(mojo_frame->MappedSize(), static_cast<size_t>(3 * stride));
  EXPECT_EQ(mojo_frame->PlaneOffset(VideoFrame::kYPlane), 0u);
  EXPECT_EQ(mojo_frame->PlaneOffset(VideoFrame::kUPlane), y_stride);
  EXPECT_EQ(mojo_frame->PlaneOffset(VideoFrame::kVPlane), y_stride + u_stride);
}

}  // namespace media
