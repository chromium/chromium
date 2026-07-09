// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/video/video_frame_writer_queue.h"

#include <lib/zx/vmo.h>

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "media/base/video_frame.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/vmo_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class FuchsiaVideoFrameWriterQueueTest : public testing::Test {
 protected:
  FuchsiaVideoFrameWriterQueueTest() = default;
  ~FuchsiaVideoFrameWriterQueueTest() override = default;

  VmoBuffer CreateBuffer(size_t size) {
    zx::vmo vmo;
    zx_status_t status = zx::vmo::create(size, 0, &vmo);
    EXPECT_EQ(status, ZX_OK);
    VmoBuffer buffer;
    bool ok = buffer.Initialize(std::move(vmo), /*writable=*/true, /*offset=*/0,
                                size, fuchsia::sysmem2::CoherencyDomain::CPU);
    EXPECT_TRUE(ok);
    return buffer;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(FuchsiaVideoFrameWriterQueueTest, BasicCopy) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  const size_t buffer_size = coded_size.width() * coded_size.height() * 3 / 2;
  VmoBuffer buffer = CreateBuffer(buffer_size);
  zx::vmo duplicated_vmo = buffer.Duplicate(false);
  ASSERT_TRUE(duplicated_vmo.is_valid());

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  bool processed = false;
  auto process_cb = base::BindRepeating(
      [](bool& processed, StreamProcessorHelper::IoPacket packet) {
        processed = true;
        EXPECT_EQ(packet.buffer_index(), 0u);
      },
      std::ref(processed));

  EXPECT_TRUE(queue
                  .Initialize(std::move(buffers), std::move(settings),
                              std::move(format_details), coded_size, process_cb)
                  .is_ok());

  scoped_refptr<VideoFrame> frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, coded_size, gfx::Rect(coded_size), coded_size,
      base::TimeDelta());

  // Fill frame with some pattern.
  std::fill(frame->writable_span(VideoFrame::Plane::kY).begin(),
            frame->writable_span(VideoFrame::Plane::kY).end(), 1);
  std::fill(frame->writable_span(VideoFrame::Plane::kU).begin(),
            frame->writable_span(VideoFrame::Plane::kU).end(), 2);
  std::fill(frame->writable_span(VideoFrame::Plane::kV).begin(),
            frame->writable_span(VideoFrame::Plane::kV).end(), 3);

  EXPECT_TRUE(queue.Enqueue(frame, /*force_keyframe=*/false));

  // The queue should process the new frame immediately since it was already
  // initialized with a free buffer.
  EXPECT_TRUE(processed);

  // Verify buffer content using duplicated VMO.
  VmoBuffer verify_buffer;
  bool ok = verify_buffer.Initialize(std::move(duplicated_vmo),
                                     /*writable=*/false, 0, buffer_size,
                                     fuchsia::sysmem2::CoherencyDomain::CPU);
  ASSERT_TRUE(ok);

  base::span<const uint8_t> mem = verify_buffer.GetMemory();

  for (size_t i = 0; i < 10000; ++i) {
    EXPECT_EQ(mem[i], 1) << "at index " << i;
  }
  for (size_t i = 10000; i < 12500; ++i) {
    EXPECT_EQ(mem[i], 2) << "at index " << i;
  }
  for (size_t i = 12500; i < 15000; ++i) {
    EXPECT_EQ(mem[i], 3) << "at index " << i;
  }
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, EvenDivisor12) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  // With min_bytes_per_row=100 and divisor=12, Y-stride is aligned to 108.
  // U/V-stride is aligned to 54.
  // Y-plane: 108 * 100 = 10800 bytes.
  // U/V-planes: 54 * 50 * 2 = 5400 bytes.
  // Total size: 16200 bytes.
  const size_t buffer_size = 16200;
  VmoBuffer buffer = CreateBuffer(buffer_size);
  zx::vmo duplicated_vmo = buffer.Duplicate(false);
  ASSERT_TRUE(duplicated_vmo.is_valid());

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(12);

  fuchsia::media::FormatDetails format_details;

  bool processed = false;
  auto process_cb = base::BindRepeating(
      [](bool& processed, StreamProcessorHelper::IoPacket packet) {
        processed = true;
        EXPECT_EQ(packet.buffer_index(), 0u);
        EXPECT_EQ(packet.size(), 16200u);
      },
      std::ref(processed));

  EXPECT_TRUE(queue
                  .Initialize(std::move(buffers), std::move(settings),
                              std::move(format_details), coded_size, process_cb)
                  .is_ok());

  scoped_refptr<VideoFrame> frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, coded_size, gfx::Rect(coded_size), coded_size,
      base::TimeDelta());

  // Fill frame with some pattern.
  std::fill(frame->writable_span(VideoFrame::Plane::kY).begin(),
            frame->writable_span(VideoFrame::Plane::kY).end(), 1);
  std::fill(frame->writable_span(VideoFrame::Plane::kU).begin(),
            frame->writable_span(VideoFrame::Plane::kU).end(), 2);
  std::fill(frame->writable_span(VideoFrame::Plane::kV).begin(),
            frame->writable_span(VideoFrame::Plane::kV).end(), 3);

  EXPECT_TRUE(queue.Enqueue(frame, /*force_keyframe=*/false));

  EXPECT_TRUE(base::test::RunUntil([&]() { return processed; }));

  // Verify buffer content using duplicated VMO.
  VmoBuffer verify_buffer;
  bool ok = verify_buffer.Initialize(std::move(duplicated_vmo),
                                     /*writable=*/false, 0, buffer_size,
                                     fuchsia::sysmem2::CoherencyDomain::CPU);
  ASSERT_TRUE(ok);

  base::span<const uint8_t> mem = verify_buffer.GetMemory();

  // Verify Y plane.
  for (size_t row = 0; row < 100; ++row) {
    size_t row_offset = row * 108;
    for (size_t col = 0; col < 100; ++col) {
      EXPECT_EQ(mem[row_offset + col], 1)
          << "Y at row " << row << ", col " << col;
    }
  }

  // Verify U plane.
  size_t u_plane_offset = 10800;
  for (size_t row = 0; row < 50; ++row) {
    size_t row_offset = u_plane_offset + row * 54;
    for (size_t col = 0; col < 50; ++col) {
      EXPECT_EQ(mem[row_offset + col], 2)
          << "U at row " << row << ", col " << col;
    }
  }

  // Verify V plane.
  size_t v_plane_offset = 13500;
  for (size_t row = 0; row < 50; ++row) {
    size_t row_offset = v_plane_offset + row * 54;
    for (size_t col = 0; col < 50; ++col) {
      EXPECT_EQ(mem[row_offset + col], 3)
          << "V at row " << row << ", col " << col;
    }
  }
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, LargeStrideAndBuffer) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  // With min_bytes_per_row=200, Y-stride is 200, U/V-stride is 100.
  // Y-plane: 200 * 100 = 20000 bytes.
  // U/V-planes: 100 * 50 * 2 = 10000 bytes.
  // Minimum required size: 30000 bytes.
  // We allocate a larger buffer (40000 bytes).
  const size_t buffer_size = 40000;
  VmoBuffer buffer = CreateBuffer(buffer_size);
  zx::vmo duplicated_vmo = buffer.Duplicate(false);
  ASSERT_TRUE(duplicated_vmo.is_valid());

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(200);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  bool processed = false;
  auto process_cb = base::BindRepeating(
      [](bool& processed, StreamProcessorHelper::IoPacket packet) {
        processed = true;
        EXPECT_EQ(packet.buffer_index(), 0u);
        // The packet size should be the calculated minimum size, not the buffer
        // size.
        EXPECT_EQ(packet.size(), 30000u);
      },
      std::ref(processed));

  EXPECT_TRUE(queue
                  .Initialize(std::move(buffers), std::move(settings),
                              std::move(format_details), coded_size, process_cb)
                  .is_ok());

  scoped_refptr<VideoFrame> frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, coded_size, gfx::Rect(coded_size), coded_size,
      base::TimeDelta());

  // Fill frame with some pattern.
  std::fill(frame->writable_span(VideoFrame::Plane::kY).begin(),
            frame->writable_span(VideoFrame::Plane::kY).end(), 1);
  std::fill(frame->writable_span(VideoFrame::Plane::kU).begin(),
            frame->writable_span(VideoFrame::Plane::kU).end(), 2);
  std::fill(frame->writable_span(VideoFrame::Plane::kV).begin(),
            frame->writable_span(VideoFrame::Plane::kV).end(), 3);

  EXPECT_TRUE(queue.Enqueue(frame, /*force_keyframe=*/false));

  EXPECT_TRUE(base::test::RunUntil([&]() { return processed; }));

  // Verify buffer content using duplicated VMO.
  VmoBuffer verify_buffer;
  bool ok = verify_buffer.Initialize(std::move(duplicated_vmo),
                                     /*writable=*/false, 0, buffer_size,
                                     fuchsia::sysmem2::CoherencyDomain::CPU);
  ASSERT_TRUE(ok);

  base::span<const uint8_t> mem = verify_buffer.GetMemory();

  // Verify Y plane (100 rows, 200 stride, 100 active).
  for (size_t row = 0; row < 100; ++row) {
    size_t row_offset = row * 200;
    for (size_t col = 0; col < 100; ++col) {
      EXPECT_EQ(mem[row_offset + col], 1)
          << "Y at row " << row << ", col " << col;
    }
  }

  // Verify U plane (50 rows, 100 stride, 50 active).
  size_t u_plane_offset = 20000;
  for (size_t row = 0; row < 50; ++row) {
    size_t row_offset = u_plane_offset + row * 100;
    for (size_t col = 0; col < 50; ++col) {
      EXPECT_EQ(mem[row_offset + col], 2)
          << "U at row " << row << ", col " << col;
    }
  }

  // Verify V plane (50 rows, 100 stride, 50 active).
  size_t v_plane_offset = 25000;
  for (size_t row = 0; row < 50; ++row) {
    size_t row_offset = v_plane_offset + row * 100;
    for (size_t col = 0; col < 50; ++col) {
      EXPECT_EQ(mem[row_offset + col], 3)
          << "V at row " << row << ", col " << col;
    }
  }
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectSmallBuffer) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  // For a 100x100 I420 frame with Y-stride 100, the required buffer size is:
  // Y-plane: 100 * 100 = 10000 bytes.
  // U/V-planes: 50 * 50 * 2 = 5000 bytes.
  // Total: 15000 bytes.
  // 9000 bytes is insufficient.
  VmoBuffer buffer = CreateBuffer(9000);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  EncoderStatus status =
      queue.Initialize(std::move(buffers), std::move(settings),
                       std::move(format_details), coded_size, process_cb);
  EXPECT_FALSE(status.is_ok());
  EXPECT_EQ(status.code(), EncoderStatus::Codes::kEncoderInitializationError);
  EXPECT_NE(status.message().find("9000"), std::string::npos);
  EXPECT_NE(status.message().find("15000"), std::string::npos);
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectOverflowStride) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  VmoBuffer buffer =
      CreateBuffer(coded_size.width() * coded_size.height() * 3 / 2);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  // min_bytes_per_row is set to INT32_MAX + 11 to ensure the calculated stride
  // overflows INT32_MAX and is also even (since INT32_MAX is odd, adding 11
  // makes it even, avoiding "Calculated destination Y stride is odd" error).
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(11u +
                                                                     INT32_MAX);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  EncoderStatus status =
      queue.Initialize(std::move(buffers), std::move(settings),
                       std::move(format_details), coded_size, process_cb);
  // Initialize should fail because min_bytes_per_row is set to a value
  // that causes the calculated stride to overflow INT32_MAX.
  EXPECT_FALSE(status.is_ok());
  EXPECT_EQ(status.code(), EncoderStatus::Codes::kEncoderInitializationError);
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectLargeFrameSize) {
  VideoFrameWriterQueue queue;

  // Choose dimensions such that:
  // 1. Stride (50000) <= INT32_MAX.
  // 2. Height (40000) is even.
  // 3. Total size (50000 * 40000 * 1.5 = 3GB) exceeds INT32_MAX.
  const gfx::Size coded_size(50000, 40000);
  VmoBuffer buffer = CreateBuffer(15000);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(50000);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  // Initialize should fail because the calculated frame size exceeds INT32_MAX.
  EncoderStatus status =
      queue.Initialize(std::move(buffers), std::move(settings),
                       std::move(format_details), coded_size, process_cb);
  EXPECT_FALSE(status.is_ok());
  EXPECT_EQ(status.code(), EncoderStatus::Codes::kEncoderInitializationError);
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectOddStride) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);

  fuchsia::sysmem2::SingleBufferSettings settings;
  // Force odd stride by setting min_bytes_per_row to odd and divisor to 1.
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(101);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  uint32_t divisor =
      settings.image_format_constraints().bytes_per_row_divisor();
  uint32_t stride =
      ((settings.image_format_constraints().min_bytes_per_row() + divisor - 1) /
       divisor) *
      divisor;
  VmoBuffer buffer = CreateBuffer(stride * coded_size.height() * 3 / 2);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  EncoderStatus status =
      queue.Initialize(std::move(buffers), std::move(settings),
                       std::move(format_details), coded_size, process_cb);
  EXPECT_FALSE(status.is_ok());
  EXPECT_EQ(status.code(), EncoderStatus::Codes::kEncoderInitializationError);
  EXPECT_NE(status.message().find("Failed to calculate frame size"),
            std::string::npos);
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectOddDivisor) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);

  fuchsia::sysmem2::SingleBufferSettings settings;
  // Force odd divisor (e.g. 3).
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(3);

  uint32_t divisor =
      settings.image_format_constraints().bytes_per_row_divisor();
  uint32_t stride =
      ((settings.image_format_constraints().min_bytes_per_row() + divisor - 1) /
       divisor) *
      divisor;
  VmoBuffer buffer = CreateBuffer(stride * coded_size.height() * 3 / 2);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  EncoderStatus status =
      queue.Initialize(std::move(buffers), std::move(settings),
                       std::move(format_details), coded_size, process_cb);
  EXPECT_FALSE(status.is_ok());
  EXPECT_EQ(status.code(), EncoderStatus::Codes::kEncoderInitializationError);
  EXPECT_NE(status.message().find("Failed to calculate frame size"),
            std::string::npos);
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectOddHeight) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  VmoBuffer buffer =
      CreateBuffer(coded_size.width() * coded_size.height() * 3 / 2);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  EXPECT_TRUE(queue
                  .Initialize(std::move(buffers), std::move(settings),
                              std::move(format_details), coded_size, process_cb)
                  .is_ok());

  // Create frame with odd height (99). YUV 4:2:0 requires height to be even
  // for chroma planes (UV rows are height/2).
  gfx::Size odd_height_size(100, 99);
  size_t y_size = 100 * 99;
  size_t uv_size = 50 * 50;
  std::vector<uint8_t> data(y_size + uv_size * 2);
  auto data_span = base::span(data);
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      PIXEL_FORMAT_I420, odd_height_size, gfx::Rect(odd_height_size),
      odd_height_size, /*y_stride=*/100, /*u_stride=*/50, /*v_stride=*/50,
      data_span.first(y_size), data_span.subspan(y_size, uv_size),
      data_span.subspan(y_size + uv_size, uv_size), base::TimeDelta());

  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->coded_size().height(), 99);
  EXPECT_FALSE(queue.Enqueue(frame, /*force_keyframe=*/false));
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectOddCodedHeight) {
  VideoFrameWriterQueue queue;

  // Coded height 99 is odd, which is invalid for YUV 4:2:0 format.
  const gfx::Size coded_size(100, 99);
  VmoBuffer buffer =
      CreateBuffer(coded_size.width() * coded_size.height() * 3 / 2);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  EncoderStatus status =
      queue.Initialize(std::move(buffers), std::move(settings),
                       std::move(format_details), coded_size, process_cb);
  EXPECT_FALSE(status.is_ok());
  EXPECT_EQ(status.code(), EncoderStatus::Codes::kEncoderInitializationError);
  EXPECT_NE(status.message().find("Failed to calculate frame size"),
            std::string::npos);
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectInvalidFormat) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  VmoBuffer buffer =
      CreateBuffer(coded_size.width() * coded_size.height() * 3 / 2);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  EXPECT_TRUE(queue
                  .Initialize(std::move(buffers), std::move(settings),
                              std::move(format_details), coded_size, process_cb)
                  .is_ok());

  // Create NV12 frame (unsupported, should be I420).
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_NV12, coded_size, gfx::Rect(coded_size), coded_size,
      base::TimeDelta());

  EXPECT_FALSE(queue.Enqueue(frame, /*force_keyframe=*/false));
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectInvalidStride) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  VmoBuffer buffer =
      CreateBuffer(coded_size.width() * coded_size.height() * 3 / 2);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  EXPECT_TRUE(queue
                  .Initialize(std::move(buffers), std::move(settings),
                              std::move(format_details), coded_size, process_cb)
                  .is_ok());

  // Create frame with invalid Y stride (50, should be at least 100).
  size_t y_size = 100 * 100;
  size_t uv_size = 50 * 50;
  std::vector<uint8_t> data(y_size + uv_size * 2);
  auto data_span = base::span(data);
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      PIXEL_FORMAT_I420, coded_size, gfx::Rect(coded_size), coded_size,
      /*y_stride=*/50, /*u_stride=*/50, /*v_stride=*/50,
      data_span.first(y_size), data_span.subspan(y_size, uv_size),
      data_span.subspan(y_size + uv_size, uv_size), base::TimeDelta());

  EXPECT_FALSE(queue.Enqueue(frame, /*force_keyframe=*/false));
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectSmallDataSpan) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  VmoBuffer buffer =
      CreateBuffer(coded_size.width() * coded_size.height() * 3 / 2);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  EXPECT_TRUE(queue
                  .Initialize(std::move(buffers), std::move(settings),
                              std::move(format_details), coded_size, process_cb)
                  .is_ok());

  // Create frame where Y data span is too small (only 10 bytes, should be
  // 10000).
  size_t y_size = 100 * 100;
  size_t uv_size = 50 * 50;
  std::vector<uint8_t> data(y_size + uv_size * 2);
  auto data_span = base::span(data);
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      PIXEL_FORMAT_I420, coded_size, gfx::Rect(coded_size), coded_size,
      /*y_stride=*/100, /*u_stride=*/50, /*v_stride=*/50,
      data_span.first(10u),  // Small Y span.
      data_span.subspan(y_size, uv_size),
      data_span.subspan(y_size + uv_size, uv_size), base::TimeDelta());

  EXPECT_FALSE(queue.Enqueue(frame, /*force_keyframe=*/false));
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectZeroDimensions) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  VmoBuffer buffer =
      CreateBuffer(coded_size.width() * coded_size.height() * 3 / 2);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  EXPECT_TRUE(queue
                  .Initialize(std::move(buffers), std::move(settings),
                              std::move(format_details), coded_size, process_cb)
                  .is_ok());

  // Create frame with 0 width.
  gfx::Size zero_width_size(0, 100);
  scoped_refptr<VideoFrame> frame_zero_width = VideoFrame::WrapExternalYuvData(
      PIXEL_FORMAT_I420, zero_width_size, gfx::Rect(zero_width_size),
      zero_width_size, /*y_stride=*/0, /*u_stride=*/0, /*v_stride=*/0,
      base::span<const uint8_t>(), base::span<const uint8_t>(),
      base::span<const uint8_t>(), base::TimeDelta());
  // WrapExternalYuvData might return nullptr if validation fails there.
  // If it returns nullptr, then we can't even enqueue it, which is also a form
  // of rejection, but we want to test Enqueue's validation. Let's assume it
  // returns a valid frame object but with 0 size.
  if (frame_zero_width) {
    EXPECT_FALSE(queue.Enqueue(frame_zero_width, /*force_keyframe=*/false));
  }

  // Create frame with 0 height.
  gfx::Size zero_height_size(100, 0);
  scoped_refptr<VideoFrame> frame_zero_height = VideoFrame::WrapExternalYuvData(
      PIXEL_FORMAT_I420, zero_height_size, gfx::Rect(zero_height_size),
      zero_height_size, /*y_stride=*/100, /*u_stride=*/50, /*v_stride=*/50,
      base::span<const uint8_t>(), base::span<const uint8_t>(),
      base::span<const uint8_t>(), base::TimeDelta());
  if (frame_zero_height) {
    EXPECT_FALSE(queue.Enqueue(frame_zero_height, /*force_keyframe=*/false));
  }
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, RejectZeroCodedWidth) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(0, 100);  // Zero width.
  // Use a standard buffer size for a 100x100 frame as a non-zero placeholder.
  VmoBuffer buffer = CreateBuffer(15000);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(0);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  auto process_cb =
      base::BindRepeating([](StreamProcessorHelper::IoPacket packet) {});

  EncoderStatus status =
      queue.Initialize(std::move(buffers), std::move(settings),
                       std::move(format_details), coded_size, process_cb);
  EXPECT_FALSE(status.is_ok());
  EXPECT_EQ(status.code(), EncoderStatus::Codes::kEncoderInitializationError);
  EXPECT_NE(status.message().find("Failed to calculate frame size"),
            std::string::npos);
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, QueueFramesBeforeInitialize) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  const size_t buffer_size = coded_size.width() * coded_size.height() * 3 / 2;
  VmoBuffer buffer1 = CreateBuffer(buffer_size);
  VmoBuffer buffer2 = CreateBuffer(buffer_size);

  // Enqueue two frames before Initialize.
  scoped_refptr<VideoFrame> frame1 = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, coded_size, gfx::Rect(coded_size), coded_size,
      base::Milliseconds(10));
  scoped_refptr<VideoFrame> frame2 = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, coded_size, gfx::Rect(coded_size), coded_size,
      base::Milliseconds(20));

  EXPECT_TRUE(queue.Enqueue(frame1, /*force_keyframe=*/false));
  EXPECT_TRUE(queue.Enqueue(frame2, /*force_keyframe=*/false));

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer1));
  buffers.push_back(std::move(buffer2));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  std::vector<base::TimeDelta> processed_timestamps;
  auto process_cb = base::BindRepeating(
      [](std::vector<base::TimeDelta>& timestamps,
         StreamProcessorHelper::IoPacket packet) {
        timestamps.push_back(packet.timestamp());
      },
      std::ref(processed_timestamps));

  EXPECT_TRUE(queue
                  .Initialize(std::move(buffers), std::move(settings),
                              std::move(format_details), coded_size, process_cb)
                  .is_ok());

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return processed_timestamps.size() == 2; }));
  EXPECT_EQ(processed_timestamps[0], base::Milliseconds(10));
  EXPECT_EQ(processed_timestamps[1], base::Milliseconds(20));
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, BufferThrottling) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  VmoBuffer buffer =
      CreateBuffer(coded_size.width() * coded_size.height() * 3 / 2);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  fuchsia::media::FormatDetails format_details;

  std::vector<StreamProcessorHelper::IoPacket> processed_packets;
  auto process_cb = base::BindRepeating(
      [](std::vector<StreamProcessorHelper::IoPacket>& packets,
         StreamProcessorHelper::IoPacket packet) {
        packets.push_back(std::move(packet));
      },
      std::ref(processed_packets));

  EXPECT_TRUE(queue
                  .Initialize(std::move(buffers), std::move(settings),
                              std::move(format_details), coded_size, process_cb)
                  .is_ok());

  scoped_refptr<VideoFrame> frame1 = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, coded_size, gfx::Rect(coded_size), coded_size,
      base::Milliseconds(10));
  scoped_refptr<VideoFrame> frame2 = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, coded_size, gfx::Rect(coded_size), coded_size,
      base::Milliseconds(20));

  // Enqueue first frame. It should be processed immediately because buffer is
  // free.
  EXPECT_TRUE(queue.Enqueue(frame1, /*force_keyframe=*/false));
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return processed_packets.size() == 1; }));
  EXPECT_EQ(processed_packets[0].timestamp(), base::Milliseconds(10));

  // Enqueue second frame. It should NOT be processed because buffer is busy.
  EXPECT_TRUE(queue.Enqueue(frame2, /*force_keyframe=*/false));

  // The frame should not be processed because the buffer is busy.
  EXPECT_EQ(processed_packets.size(), 1u);

  // Release the first buffer by destroying the packet.
  // Move it out of the vector first to avoid re-entrancy issues where clearing
  // the vector triggers a synchronous callback that modifies the same vector.
  {
    StreamProcessorHelper::IoPacket packet_to_destroy =
        std::move(processed_packets[0]);
    processed_packets.clear();
  }

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return processed_packets.size() == 1; }));
  EXPECT_EQ(processed_packets[0].timestamp(), base::Milliseconds(20));
}

TEST_F(FuchsiaVideoFrameWriterQueueTest, ForceKeyframe) {
  VideoFrameWriterQueue queue;

  const gfx::Size coded_size(100, 100);
  VmoBuffer buffer =
      CreateBuffer(coded_size.width() * coded_size.height() * 3 / 2);

  std::vector<VmoBuffer> buffers;
  buffers.push_back(std::move(buffer));

  fuchsia::sysmem2::SingleBufferSettings settings;
  settings.mutable_image_format_constraints()->set_min_bytes_per_row(100);
  settings.mutable_image_format_constraints()->set_bytes_per_row_divisor(1);

  // Initialize format details with H264 settings.
  fuchsia::media::FormatDetails format_details;
  fuchsia::media::H264EncoderSettings h264_settings;
  h264_settings.set_force_key_frame(false);
  fuchsia::media::EncoderSettings encoder_settings;
  encoder_settings.set_h264(std::move(h264_settings));
  format_details.set_encoder_settings(std::move(encoder_settings));

  bool processed = false;
  auto process_cb = base::BindRepeating(
      [](bool& processed, StreamProcessorHelper::IoPacket packet) {
        processed = true;
        EXPECT_TRUE(packet.format().has_encoder_settings());
        EXPECT_TRUE(
            packet.format().encoder_settings().h264().has_force_key_frame());
        EXPECT_TRUE(
            packet.format().encoder_settings().h264().force_key_frame());
      },
      std::ref(processed));

  EXPECT_TRUE(queue
                  .Initialize(std::move(buffers), std::move(settings),
                              std::move(format_details), coded_size, process_cb)
                  .is_ok());

  scoped_refptr<VideoFrame> frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, coded_size, gfx::Rect(coded_size), coded_size,
      base::TimeDelta());

  EXPECT_TRUE(queue.Enqueue(frame, /*force_keyframe=*/true));

  EXPECT_TRUE(base::test::RunUntil([&]() { return processed; }));
}

}  // namespace media
