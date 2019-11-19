// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/viz/test/test_context_provider.h"
#include "media/base/video_frame.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::AtLeast;

namespace media {

class GpuMemoryBufferVideoFramePoolTest : public ::testing::Test {
 public:
  GpuMemoryBufferVideoFramePoolTest() = default;
  void SetUp() override {
    // Seed test clock with some dummy non-zero value to avoid confusion with
    // empty base::TimeTicks values.
    test_clock_.Advance(base::TimeDelta::FromSeconds(1234));

    sii_.reset(new viz::TestSharedImageInterface);
    media_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    copy_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    media_task_runner_handle_.reset(
        new base::ThreadTaskRunnerHandle(media_task_runner_));
    mock_gpu_factories_.reset(new MockGpuVideoAcceleratorFactories(sii_.get()));
    gpu_memory_buffer_pool_.reset(new GpuMemoryBufferVideoFramePool(
        media_task_runner_, copy_task_runner_.get(),
        mock_gpu_factories_.get()));
    gpu_memory_buffer_pool_->SetTickClockForTesting(&test_clock_);
  }

  void TearDown() override {
    gpu_memory_buffer_pool_.reset();
    RunUntilIdle();
    mock_gpu_factories_.reset();
  }

  void RunUntilIdle() {
    media_task_runner_->RunUntilIdle();
    copy_task_runner_->RunUntilIdle();
    media_task_runner_->RunUntilIdle();
  }

  static scoped_refptr<VideoFrame> CreateTestYUVVideoFrame(
      int dimension,
      size_t bit_depth = 8,
      int visible_rect_crop = 0) {
    const int kDimension = 10;
    // Data buffers are overdimensioned to acommodate up to 16bpc samples.
    static uint8_t y_data[2 * kDimension * kDimension] = {0};
    static uint8_t u_data[2 * kDimension * kDimension / 4] = {0};
    static uint8_t v_data[2 * kDimension * kDimension / 4] = {0};

    const VideoPixelFormat format =
        (bit_depth > 8) ? PIXEL_FORMAT_YUV420P10 : PIXEL_FORMAT_I420;
    DCHECK_LE(dimension, kDimension);
    const gfx::Size size(dimension, dimension);

    scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalYuvData(
        format,  // format
        size,    // coded_size
        gfx::Rect(visible_rect_crop, visible_rect_crop,
                  size.width() - visible_rect_crop,
                  size.height() - visible_rect_crop),  // visible_rect
        size,                                          // natural_size
        size.width(),                                  // y_stride
        size.width() / 2,                              // u_stride
        size.width() / 2,                              // v_stride
        y_data,                                        // y_data
        u_data,                                        // u_data
        v_data,                                        // v_data
        base::TimeDelta());                            // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  static scoped_refptr<VideoFrame> CreateTestYUVAVideoFrame(int dimension) {
    const int kDimension = 10;
    static uint8_t y_data[kDimension * kDimension] = {0};
    static uint8_t u_data[kDimension * kDimension / 4] = {0};
    static uint8_t v_data[kDimension * kDimension / 4] = {0};
    static uint8_t a_data[kDimension * kDimension] = {0};

    constexpr VideoPixelFormat format = PIXEL_FORMAT_I420A;
    DCHECK_LE(dimension, kDimension);
    const gfx::Size size(dimension, dimension);

    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::WrapExternalYuvaData(format,              // format
                                         size,                // coded_size
                                         gfx::Rect(size),     // visible_rect
                                         size,                // natural_size
                                         size.width(),        // y_stride
                                         size.width() / 2,    // u_stride
                                         size.width() / 2,    // v_stride
                                         size.width(),        // a_stride
                                         y_data,              // y_data
                                         u_data,              // u_data
                                         v_data,              // v_data
                                         a_data,              // a_data
                                         base::TimeDelta());  // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  // Note, the X portion is set to 1 since it may use ARGB instead of
  // XRGB on some platforms.
  uint32_t as_xr30(uint32_t r, uint32_t g, uint32_t b) {
    return (3 << 30) | (r << 20) | (g << 10) | b;
  }
  uint32_t as_xb30(uint32_t r, uint32_t g, uint32_t b) {
    return (3 << 30) | (b << 20) | (g << 10) | r;
  }

 protected:
  base::SimpleTestTickClock test_clock_;
  std::unique_ptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  std::unique_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_;
  scoped_refptr<base::TestSimpleTaskRunner> media_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> copy_task_runner_;
  // GpuMemoryBufferVideoFramePool uses BindToCurrentLoop(), which requires
  // ThreadTaskRunnerHandle initialization.
  std::unique_ptr<base::ThreadTaskRunnerHandle> media_task_runner_handle_;
  std::unique_ptr<viz::TestSharedImageInterface> sii_;
};

void MaybeCreateHardwareFrameCallback(
    scoped_refptr<VideoFrame>* video_frame_output,
    scoped_refptr<VideoFrame> video_frame) {
  *video_frame_output = std::move(video_frame);
}

void MaybeCreateHardwareFrameCallbackAndTrackTime(
    scoped_refptr<VideoFrame>* video_frame_output,
    base::TimeTicks* output_time,
    scoped_refptr<VideoFrame> video_frame) {
  *video_frame_output = std::move(video_frame);
  *output_time = base::TimeTicks::Now();
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, VideoFrameOutputFormatUnknown) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_EQ(software_frame.get(), frame.get());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareFrame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ(3u, frame->NumTextures());
  EXPECT_EQ(3u, sii_->shared_image_count());
}

// Tests the current workaround for odd positioned video frame input. Once
// https://crbug.com/638906 is fixed, output should be different.
TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareFrameWithOddOrigin) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(9, 8, 1);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_EQ(software_frame.get(), frame.get());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOne10BppHardwareFrame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10, 10);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ(3u, frame->NumTextures());
  EXPECT_EQ(3u, sii_->shared_image_count());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, ReuseFirstResource) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  gpu::Mailbox mailbox = frame->mailbox_holder(0).mailbox;
  const gpu::SyncToken sync_token = frame->mailbox_holder(0).sync_token;
  EXPECT_EQ(3u, sii_->shared_image_count());

  scoped_refptr<VideoFrame> frame2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame2));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame2.get());
  EXPECT_NE(mailbox, frame2->mailbox_holder(0).mailbox);
  EXPECT_EQ(6u, sii_->shared_image_count());

  frame = nullptr;
  frame2 = nullptr;
  RunUntilIdle();

  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(6u, sii_->shared_image_count());
  EXPECT_EQ(frame->mailbox_holder(0).mailbox, mailbox);
  EXPECT_NE(frame->mailbox_holder(0).sync_token, sync_token);
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, DropResourceWhenSizeIsDifferent) {
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      CreateTestYUVVideoFrame(10),
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_EQ(3u, sii_->shared_image_count());
  // Check that the mailboxes in the VideoFrame were properly created.
  gpu::Mailbox old_mailboxes[3];
  for (size_t i = 0; i < 3; ++i) {
    old_mailboxes[i] = frame->mailbox_holder(i).mailbox;
    EXPECT_TRUE(sii_->CheckSharedImageExists(old_mailboxes[i]));
  }

  frame = nullptr;
  RunUntilIdle();
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      CreateTestYUVVideoFrame(4),
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();
  // Check that the mailboxes in the old VideoFrame were properly destroyed.
  for (const auto& mailbox : old_mailboxes)
    EXPECT_FALSE(sii_->CheckSharedImageExists(mailbox));
  EXPECT_EQ(3u, sii_->shared_image_count());
  // Check that the mailboxes in the new VideoFrame were properly created.
  for (size_t i = 0; i < 3; ++i)
    EXPECT_TRUE(sii_->CheckSharedImageExists(frame->mailbox_holder(i).mailbox));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareNV12Frame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_NV12, frame->format());
  EXPECT_EQ(1u, frame->NumTextures());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareNV12Frame2) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_NV12, frame->format());
  EXPECT_EQ(2u, frame->NumTextures());
  EXPECT_EQ(2u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareXR30Frame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10, 10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::XR30);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_XR30, frame->format());
  EXPECT_EQ(1u, frame->NumTextures());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));

  EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
  mock_gpu_factories_->created_memory_buffers()[0]->Map();

  void* memory = mock_gpu_factories_->created_memory_buffers()[0]->memory(0);
  EXPECT_EQ(as_xr30(0, 311, 0), *static_cast<uint32_t*>(memory));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareXR30FrameBT709) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10, 10);
  software_frame->set_color_space(gfx::ColorSpace::CreateREC709());
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::XR30);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_XR30, frame->format());
  EXPECT_EQ(1u, frame->NumTextures());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));

  EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
  mock_gpu_factories_->created_memory_buffers()[0]->Map();

  void* memory = mock_gpu_factories_->created_memory_buffers()[0]->memory(0);
  EXPECT_EQ(as_xr30(0, 311, 0), *static_cast<uint32_t*>(memory));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareXR30FrameBT601) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10, 10);
  software_frame->set_color_space(gfx::ColorSpace::CreateREC601());
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::XR30);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_XR30, frame->format());
  EXPECT_EQ(1u, frame->NumTextures());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));

  EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
  mock_gpu_factories_->created_memory_buffers()[0]->Map();

  void* memory = mock_gpu_factories_->created_memory_buffers()[0]->memory(0);
  EXPECT_EQ(as_xr30(0, 543, 0), *static_cast<uint32_t*>(memory));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareXB30Frame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10, 10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::XB30);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_XB30, frame->format());
  EXPECT_EQ(1u, frame->NumTextures());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareRGBAFrame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVAVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::RGBA);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_ABGR, frame->format());
  EXPECT_EQ(1u, frame->NumTextures());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, PreservesMetadata) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  software_frame->metadata()->SetBoolean(
      media::VideoFrameMetadata::END_OF_STREAM, true);
  base::TimeTicks kTestReferenceTime =
      base::TimeDelta::FromMilliseconds(12345) + base::TimeTicks();
  software_frame->metadata()->SetTimeTicks(VideoFrameMetadata::REFERENCE_TIME,
                                           kTestReferenceTime);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  bool end_of_stream = false;
  EXPECT_TRUE(frame->metadata()->GetBoolean(
      media::VideoFrameMetadata::END_OF_STREAM, &end_of_stream));
  EXPECT_TRUE(end_of_stream);
  base::TimeTicks render_time;
  EXPECT_TRUE(frame->metadata()->GetTimeTicks(
      VideoFrameMetadata::REFERENCE_TIME, &render_time));
  EXPECT_EQ(kTestReferenceTime, render_time);
}

// CreateGpuMemoryBuffer can return null (e.g: when the GPU process is down).
// This test checks that in that case we don't crash and don't create the
// textures.
TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateGpuMemoryBufferFail) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetFailToAllocateGpuMemoryBufferForTesting(true);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(0u, sii_->shared_image_count());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, ShutdownReleasesUnusedResources) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_1;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_1));

  RunUntilIdle();
  EXPECT_NE(software_frame.get(), frame_1.get());

  scoped_refptr<VideoFrame> frame_2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_2));
  RunUntilIdle();
  EXPECT_NE(software_frame.get(), frame_2.get());
  EXPECT_NE(frame_1.get(), frame_2.get());

  EXPECT_EQ(6u, sii_->shared_image_count());

  // Drop frame and verify that resources are still available for reuse.
  frame_1 = nullptr;
  RunUntilIdle();
  EXPECT_EQ(6u, sii_->shared_image_count());

  // While still holding onto the second frame, destruct the frame pool and
  // verify that the inner pool releases the resources for the first frame.
  gpu_memory_buffer_pool_.reset();
  RunUntilIdle();

  EXPECT_EQ(3u, sii_->shared_image_count());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, StaleFramesAreExpired) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_1;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_1));

  RunUntilIdle();
  EXPECT_NE(software_frame.get(), frame_1.get());

  scoped_refptr<VideoFrame> frame_2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_2));
  RunUntilIdle();
  EXPECT_NE(software_frame.get(), frame_2.get());
  EXPECT_NE(frame_1.get(), frame_2.get());

  EXPECT_EQ(6u, sii_->shared_image_count());

  // Drop frame and verify that resources are still available for reuse.
  frame_1 = nullptr;
  RunUntilIdle();
  EXPECT_EQ(6u, sii_->shared_image_count());

  // Advance clock far enough to hit stale timer; ensure only frame_1 has its
  // resources released.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  frame_2 = nullptr;
  RunUntilIdle();
  EXPECT_EQ(3u, sii_->shared_image_count());
}

// Test when we request two copies in a row, there should be at most one frame
// copy in flight at any time.
TEST_F(GpuMemoryBufferVideoFramePoolTest, AtMostOneCopyInFlight) {
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB);

  scoped_refptr<VideoFrame> software_frame_1 = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_1;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_1,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_1));

  scoped_refptr<VideoFrame> software_frame_2 = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_2,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_2));

  media_task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, copy_task_runner_->NumPendingTasks());
  copy_task_runner_->RunUntilIdle();
  media_task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, copy_task_runner_->NumPendingTasks());
  RunUntilIdle();
}

// Tests that adding a frame that the pool doesn't handle does not break the
// FIFO order in tasks.
TEST_F(GpuMemoryBufferVideoFramePoolTest, PreservesOrder) {
  std::vector<scoped_refptr<VideoFrame>> frame_outputs;

  scoped_refptr<VideoFrame> software_frame_1 = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_1 = nullptr;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_1,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_1));

  scoped_refptr<VideoFrame> software_frame_2 = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_2 = nullptr;
  base::TimeTicks time_2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_2,
      base::BindOnce(MaybeCreateHardwareFrameCallbackAndTrackTime, &frame_2,
                     &time_2));

  scoped_refptr<VideoFrame> software_frame_3 = VideoFrame::CreateEOSFrame();
  scoped_refptr<VideoFrame> frame_3 = nullptr;
  base::TimeTicks time_3;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_3,
      base::BindOnce(MaybeCreateHardwareFrameCallbackAndTrackTime, &frame_3,
                     &time_3));

  // Queue all the tasks |media_task_runner_|. Make sure that none is early
  // returned.
  media_task_runner_->RunUntilIdle();
  EXPECT_FALSE(frame_1.get());
  EXPECT_FALSE(frame_2.get());
  EXPECT_FALSE(frame_3.get());
  EXPECT_EQ(1u, copy_task_runner_->NumPendingTasks());

  RunUntilIdle();
  EXPECT_TRUE(frame_1.get());
  EXPECT_NE(software_frame_1.get(), frame_1.get());
  EXPECT_FALSE(frame_2.get());
  EXPECT_FALSE(frame_3.get());

  RunUntilIdle();
  EXPECT_TRUE(frame_2.get());
  EXPECT_TRUE(frame_3.get());
  EXPECT_NE(software_frame_2.get(), frame_2.get());
  EXPECT_EQ(software_frame_3.get(), frame_3.get());
  EXPECT_LE(time_2, time_3);
}

// Test that Abort() stops any pending copies.
TEST_F(GpuMemoryBufferVideoFramePoolTest, AbortCopies) {
  scoped_refptr<VideoFrame> software_frame_1 = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_1;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_1,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_1));

  scoped_refptr<VideoFrame> software_frame_2 = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_2,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_2));

  media_task_runner_->RunUntilIdle();
  EXPECT_GE(1u, copy_task_runner_->NumPendingTasks());
  copy_task_runner_->RunUntilIdle();

  gpu_memory_buffer_pool_->Abort();
  media_task_runner_->RunUntilIdle();
  EXPECT_EQ(0u, copy_task_runner_->NumPendingTasks());
  RunUntilIdle();
  ASSERT_FALSE(frame_2);
}

// Tests that an I420 VideoFrame after an I420A is ignored, i.e. passed through.
// See e.g. https://crbug.com/875158.
TEST_F(GpuMemoryBufferVideoFramePoolTest, VideoFrameChangesPixelFormat) {
  scoped_refptr<VideoFrame> software_frame_1 = CreateTestYUVAVideoFrame(10);
  scoped_refptr<VideoFrame> frame_1;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::RGBA);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_1,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_1));
  RunUntilIdle();

  EXPECT_NE(software_frame_1.get(), frame_1.get());
  EXPECT_EQ(PIXEL_FORMAT_ABGR, frame_1->format());
  EXPECT_EQ(1u, frame_1->NumTextures());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame_1->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));

  scoped_refptr<VideoFrame> software_frame_2 = CreateTestYUVVideoFrame(10);
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::I420);
  scoped_refptr<VideoFrame> frame_2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_2,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_2));
  RunUntilIdle();

  EXPECT_EQ(software_frame_2.get(), frame_2.get());
}

}  // namespace media
