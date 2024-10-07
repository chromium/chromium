// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/viz/test/test_context_provider.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/buffer_format_util.h"

using ::testing::_;
using ::testing::AtLeast;

namespace media {

// Note that we are continuing to skip some tests when MappableSI is enabled
// until VideoFrame::WrapSharedImage() and ::WrapMappableSharedImage() is merged
// into one. Currently we use VideoFrame::WrapSharedImage() in
// GpuMemoryBufferVideoFramePool::PoolImpl::
// BindAndCreateMailboxHardwareFrameResource() and do not use
// VideoFrame::WrapMappableSharedImage() as the VideoFrame's underlying shared
// image will never be mapped and will be used as native texture shared image.
// This is keeping the legacy behavior will GpuMemoryBuffer intact.
// Using ::WrapSharedImage() doesn't tag the VideoFrame as memory mappable and
// hence tests which were creating and mapping GpuMemoryBuffers could not Map()
// the shared image obtained from VideoFrame.
// TODO(crbug.com/366375486): Convert the currently skipped tests when
// VideoFrame::WrapSharedImage() and ::WrapMappableSharedImage() is merged
// into one.
const bool SkipTestWithMappableSI = true;

class GpuMemoryBufferVideoFramePoolTest : public ::testing::Test {
 public:
  GpuMemoryBufferVideoFramePoolTest() = default;
  void SetUp() override {
    // Seed test clock with some dummy non-zero value to avoid confusion with
    // empty base::TimeTicks values.
    test_clock_.Advance(base::Seconds(1234));

    sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    sii_->UseTestGMBInSharedImageCreationWithBufferUsage();
    media_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    copy_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    media_task_runner_handle_ =
        std::make_unique<base::SingleThreadTaskRunner::CurrentDefaultHandle>(
            media_task_runner_);
    mock_gpu_factories_ =
        std::make_unique<MockGpuVideoAcceleratorFactories>(sii_.get());
    gpu_memory_buffer_pool_ = std::make_unique<GpuMemoryBufferVideoFramePool>(
        media_task_runner_, copy_task_runner_.get(), mock_gpu_factories_.get());
    gpu_memory_buffer_pool_->SetTickClockForTesting(&test_clock_);
  }

  void TearDown() override {
    gpu_memory_buffer_pool_.reset();
    RunUntilIdle();
    mock_gpu_factories_.reset();

    if (y_data_)
      delete[] y_data_;
    if (u_data_)
      delete[] u_data_;
    if (v_data_)
      delete[] v_data_;
    if (uv_data_)
      delete[] uv_data_;
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
    // Data buffers are overdimensioned to accommodate up to 16bpc samples.
    static uint8_t y_data[2 * kDimension * kDimension] = {0};
    static uint8_t u_data[2 * kDimension * kDimension / 4] = {0};
    static uint8_t v_data[2 * kDimension * kDimension / 4] = {0};

    const VideoPixelFormat format =
        (bit_depth > 8) ? PIXEL_FORMAT_YUV420P10 : PIXEL_FORMAT_I420;
    const int multiplier = format == PIXEL_FORMAT_YUV420P10 ? 2 : 1;
    DCHECK_LE(dimension, kDimension);
    const gfx::Size size(dimension, dimension);

    scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalYuvData(
        format,  // format
        size,    // coded_size
        gfx::Rect(visible_rect_crop, visible_rect_crop,
                  size.width() - visible_rect_crop,
                  size.height() - visible_rect_crop),  // visible_rect
        size,                                          // natural_size
        size.width() * multiplier,                     // y_stride
        size.width() * multiplier / 2,                 // u_stride
        size.width() * multiplier / 2,                 // v_stride
        y_data,                                        // y_data
        u_data,                                        // u_data
        v_data,                                        // v_data
        base::TimeDelta());                            // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateTestYUVVideoFrameWithOddSize(
      int dimension,
      size_t bit_depth = 8,
      int visible_rect_crop = 0) {
    const VideoPixelFormat format =
        (bit_depth > 8) ? PIXEL_FORMAT_YUV420P10 : PIXEL_FORMAT_I420;
    const int multiplier = format == PIXEL_FORMAT_YUV420P10 ? 2 : 1;

    int dimension_aligned = (dimension + 1) & ~1;
    y_data_ = new uint8_t[multiplier * dimension * dimension]();
    u_data_ =
        new uint8_t[multiplier * dimension_aligned * dimension_aligned / 4]();
    v_data_ =
        new uint8_t[multiplier * dimension_aligned * dimension_aligned / 4]();

    // Initialize the last pixel of each plane
    int y_size = multiplier * dimension * dimension;
    y_data_[y_size - multiplier] = kYValue;
    int u_v_size = multiplier * dimension_aligned * dimension_aligned / 4;
    u_data_[u_v_size - multiplier] = kUValue;
    v_data_[u_v_size - multiplier] = kVValue;

    const gfx::Size size(dimension, dimension);
    scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalYuvData(
        format,  // format
        size,    // coded_size
        gfx::Rect(visible_rect_crop, visible_rect_crop,
                  size.width() - visible_rect_crop * 2,
                  size.height() - visible_rect_crop * 2),  // visible_rect
        size,                                              // natural_size
        size.width() * multiplier,                         // y_stride
        dimension_aligned * multiplier / 2,                // u_stride
        dimension_aligned * multiplier / 2,                // v_stride
        y_data_,                                           // y_data
        u_data_,                                           // u_data
        v_data_,                                           // v_data
        base::TimeDelta());                                // timestamp
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

  static scoped_refptr<VideoFrame> CreateTestNV12VideoFrame(int dimension) {
    // Set the video buffer memory dimension default to 10.
    const int kDimension = 10;
    static uint8_t y_data[kDimension * kDimension] = {0};
    // Subsampled by 2x2, two components.
    static uint8_t uv_data[kDimension * kDimension / 2] = {0};

    const VideoPixelFormat format = PIXEL_FORMAT_NV12;
    DCHECK_LE(dimension, kDimension);
    const gfx::Size size(dimension, dimension);

    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::WrapExternalYuvData(format,              // format
                                        size,                // coded_size
                                        gfx::Rect(size),     // visible_rect
                                        size,                // natural_size
                                        size.width(),        // y_stride
                                        size.width(),        // uv_stride
                                        y_data,              // y_data
                                        uv_data,             // uv_data
                                        base::TimeDelta());  // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateTestNV12VideoFrameWithOddSize(int dimension) {
    // Set the video buffer memory dimension to the same size of the requested
    // dimension.
    int dimension_aligned = (dimension + 1) & ~1;
    y_data_ = new uint8_t[dimension * dimension]();
    // Subsampled by 2x2, two components.
    uv_data_ = new uint8_t[dimension_aligned * dimension_aligned / 2]();

    // Initialize the last pixel of each plane
    y_data_[dimension * dimension - 1] = kYValue;
    uv_data_[(dimension_aligned * dimension_aligned / 2) - 2] = kUValue;
    uv_data_[(dimension_aligned * dimension_aligned / 2) - 1] = kVValue;

    const VideoPixelFormat format = PIXEL_FORMAT_NV12;
    const gfx::Size size(dimension, dimension);

    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::WrapExternalYuvData(format,              // format
                                        size,                // coded_size
                                        gfx::Rect(size),     // visible_rect
                                        size,                // natural_size
                                        size.width(),        // y_stride
                                        dimension_aligned,   // uv_stride
                                        y_data_,             // y_data
                                        uv_data_,            // uv_data
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
  static constexpr uint8_t kYValue = 210;
  static constexpr uint8_t kUValue = 50;
  static constexpr uint8_t kVValue = 150;

  raw_ptr<uint8_t, DanglingUntriaged | AllowPtrArithmetic> y_data_ = nullptr;
  raw_ptr<uint8_t, DanglingUntriaged | AllowPtrArithmetic> u_data_ = nullptr;
  raw_ptr<uint8_t, DanglingUntriaged | AllowPtrArithmetic> v_data_ = nullptr;
  raw_ptr<uint8_t, DanglingUntriaged | AllowPtrArithmetic> uv_data_ = nullptr;

  base::SimpleTestTickClock test_clock_;
  std::unique_ptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  std::unique_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_;
  scoped_refptr<base::TestSimpleTaskRunner> media_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> copy_task_runner_;
  // GpuMemoryBufferVideoFramePool uses base::BindPostTaskToCurrentDefault(),
  // which requires SingleThreadTaskRunner::CurrentDefaultHandle initialization.
  std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle>
      media_task_runner_handle_;
  scoped_refptr<gpu::TestSharedImageInterface> sii_;
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
  EXPECT_EQ(PIXEL_FORMAT_YV12, frame->format());
  EXPECT_TRUE(frame->HasSharedImage());
  EXPECT_EQ(1u, sii_->shared_image_count());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareFrameWithOddSize) {
  if (SkipTestWithMappableSI) {
    return;
  }
  scoped_refptr<VideoFrame> software_frame =
      CreateTestYUVVideoFrameWithOddSize(9);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  if (gfx::IsOddWidthMultiPlanarBuffersAllowed() &&
      gfx::IsOddHeightMultiPlanarBuffersAllowed()) {
    EXPECT_NE(software_frame.get(), frame.get());
    EXPECT_EQ(PIXEL_FORMAT_YV12, frame->format());
    EXPECT_TRUE(frame->HasSharedImage());
    EXPECT_EQ(1u, sii_->shared_image_count());

    EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
    mock_gpu_factories_->created_memory_buffers()[0]->Map();

    const auto* y_memory = reinterpret_cast<uint8_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(0));
    const auto* u_memory = reinterpret_cast<uint8_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(1));
    const auto* v_memory = reinterpret_cast<uint8_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(2));

    // Y plane = 9x9, U and V plan = 5x5.
    EXPECT_EQ(kYValue, software_frame->visible_data(VideoFrame::Plane::kY)[80]);
    EXPECT_EQ(kUValue, software_frame->visible_data(VideoFrame::Plane::kU)[24]);
    EXPECT_EQ(kVValue, software_frame->visible_data(VideoFrame::Plane::kV)[24]);

    // Compare the last pixel of each plane in |software_frame| and |frame|.
    auto y_stride = mock_gpu_factories_->created_memory_buffers()[0]->stride(0);
    EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kY)[80],
              y_memory[y_stride * 8 + 8]);
    auto u_stride = mock_gpu_factories_->created_memory_buffers()[0]->stride(1);
    EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kU)[24],
              u_memory[u_stride * 4 + 4]);
    auto v_stride = mock_gpu_factories_->created_memory_buffers()[0]->stride(2);
    EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kV)[24],
              v_memory[v_stride * 4 + 4]);

    mock_gpu_factories_->created_memory_buffers()[0]->Unmap();

  } else {
    EXPECT_EQ(software_frame.get(), frame.get());
  }
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

TEST_F(GpuMemoryBufferVideoFramePoolTest,
       CreateOneHardwareFrameWithOddOriginOddSize) {
  scoped_refptr<VideoFrame> software_frame =
      CreateTestYUVVideoFrameWithOddSize(11, 8, 1);
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
  EXPECT_EQ(PIXEL_FORMAT_YV12, frame->format());
  EXPECT_TRUE(frame->HasSharedImage());
  EXPECT_EQ(1u, sii_->shared_image_count());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest,
       CreateOne10BppHardwareFrameWithOddSize) {
  if (SkipTestWithMappableSI) {
    return;
  }
  scoped_refptr<VideoFrame> software_frame =
      CreateTestYUVVideoFrameWithOddSize(17, 10);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  if (gfx::IsOddWidthMultiPlanarBuffersAllowed() &&
      gfx::IsOddHeightMultiPlanarBuffersAllowed()) {
    EXPECT_NE(software_frame.get(), frame.get());
    EXPECT_EQ(PIXEL_FORMAT_YV12, frame->format());
    EXPECT_TRUE(frame->HasSharedImage());
    EXPECT_EQ(1u, sii_->shared_image_count());

    EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
    mock_gpu_factories_->created_memory_buffers()[0]->Map();

    // Copy 10 bpp to I420.
    const auto* y_memory = reinterpret_cast<uint8_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(0));
    const auto* u_memory = reinterpret_cast<uint8_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(1));
    const auto* v_memory = reinterpret_cast<uint8_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(2));

    const uint16_t* y_plane_data = reinterpret_cast<const uint16_t*>(
        software_frame->visible_data(VideoFrame::Plane::kY));
    const uint16_t* u_plane_data = reinterpret_cast<const uint16_t*>(
        software_frame->visible_data(VideoFrame::Plane::kU));
    const uint16_t* v_plane_data = reinterpret_cast<const uint16_t*>(
        software_frame->visible_data(VideoFrame::Plane::kV));

    // Y plane = 17x17 = 289, U and V plan = 9x9.
    EXPECT_EQ(kYValue, y_plane_data[288]);
    EXPECT_EQ(kUValue, u_plane_data[80]);
    EXPECT_EQ(kVValue, v_plane_data[80]);

    // Compare the last pixel of each plane in |software_frame| and |frame|.
    //  y_memory = 17x17, u_memory/v_memory = 9x9. scale = 10 bits - 8 bits  = 2
    auto y_stride = mock_gpu_factories_->created_memory_buffers()[0]->stride(0);
    EXPECT_EQ(y_plane_data[288] >> 2, y_memory[y_stride * 16 + 16]);
    auto u_stride = mock_gpu_factories_->created_memory_buffers()[0]->stride(1);
    EXPECT_EQ(u_plane_data[80] >> 2, u_memory[u_stride * 8 + 8]);
    auto v_stride = mock_gpu_factories_->created_memory_buffers()[0]->stride(2);
    EXPECT_EQ(v_plane_data[80] >> 2, v_memory[v_stride * 8 + 8]);

    mock_gpu_factories_->created_memory_buffers()[0]->Unmap();

  } else {
    EXPECT_EQ(software_frame.get(), frame.get());
  }
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, ReuseFirstResource) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  gpu::Mailbox mailbox = frame->shared_image()->mailbox();
  const gpu::SyncToken sync_token = frame->acquire_sync_token();
  EXPECT_EQ(1u, sii_->shared_image_count());

  scoped_refptr<VideoFrame> frame2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame2));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame2.get());
  EXPECT_NE(mailbox, frame2->shared_image()->mailbox());
  EXPECT_EQ(2u, sii_->shared_image_count());

  frame = nullptr;
  frame2 = nullptr;
  RunUntilIdle();

  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(2u, sii_->shared_image_count());
  EXPECT_EQ(frame->shared_image()->mailbox(), mailbox);
  EXPECT_NE(frame->acquire_sync_token(), sync_token);
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, DropResourceWhenSizeIsDifferent) {
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      CreateTestYUVVideoFrame(10),
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_EQ(1u, sii_->shared_image_count());
  // Check that the mailbox in VideoFrame is properly created.
  gpu::Mailbox old_mailbox = frame->shared_image()->mailbox();
  EXPECT_TRUE(sii_->CheckSharedImageExists(old_mailbox));

  frame = nullptr;
  RunUntilIdle();
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      CreateTestYUVVideoFrame(4),
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();
  // Check that the mailbox in old VideoFrame is properly destroyed.
  EXPECT_FALSE(sii_->CheckSharedImageExists(old_mailbox));
  EXPECT_EQ(1u, sii_->shared_image_count());
  // Check that the mailbox in new VideoFrame is properly created.
  EXPECT_TRUE(sii_->CheckSharedImageExists(frame->shared_image()->mailbox()));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareNV12Frame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_NV12, frame->format());
  EXPECT_TRUE(frame->HasSharedImage());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata().read_lock_fences_enabled);
}

TEST_F(GpuMemoryBufferVideoFramePoolTest,
       CreateOneHardwareNV12FrameWithOddSize) {
  if (SkipTestWithMappableSI) {
    return;
  }
  scoped_refptr<VideoFrame> software_frame =
      CreateTestYUVVideoFrameWithOddSize(5);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  if (gfx::IsOddWidthMultiPlanarBuffersAllowed() &&
      gfx::IsOddHeightMultiPlanarBuffersAllowed()) {
    EXPECT_NE(software_frame.get(), frame.get());
    EXPECT_EQ(PIXEL_FORMAT_NV12, frame->format());
    EXPECT_TRUE(frame->HasSharedImage());
    EXPECT_EQ(1u, sii_->shared_image_count());
    EXPECT_TRUE(frame->metadata().read_lock_fences_enabled);

    EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
    mock_gpu_factories_->created_memory_buffers()[0]->Map();

    const auto* y_memory = reinterpret_cast<uint8_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(0));
    const auto* uv_memory = reinterpret_cast<uint8_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(1));

    // Y plane = 5x5, U and V plan = 3x3.
    EXPECT_EQ(kYValue, software_frame->visible_data(VideoFrame::Plane::kY)[24]);
    EXPECT_EQ(kUValue, software_frame->visible_data(VideoFrame::Plane::kU)[8]);
    EXPECT_EQ(kVValue, software_frame->visible_data(VideoFrame::Plane::kV)[8]);

    // Compare the last pixel of each plane in |software_frame| and |frame|.
    // y_memory = 5x5, uv_memory = 6x3.
    auto y_stride = mock_gpu_factories_->created_memory_buffers()[0]->stride(0);
    EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kY)[24],
              y_memory[y_stride * 4 + 4]);
    auto uv_stride =
        mock_gpu_factories_->created_memory_buffers()[0]->stride(1);
    EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kU)[8],
              uv_memory[uv_stride * 2 + 4]);
    EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kV)[8],
              uv_memory[uv_stride * 2 + 5]);

    mock_gpu_factories_->created_memory_buffers()[0]->Unmap();
  } else {
    EXPECT_EQ(software_frame.get(), frame.get());
  }
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareFrameForNV12Input) {
  scoped_refptr<VideoFrame> software_frame = CreateTestNV12VideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_NV12, frame->format());
  EXPECT_TRUE(frame->HasSharedImage());
  EXPECT_EQ(1u, sii_->shared_image_count());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest,
       CreateOneHardwareFrameForNV12InputWithOddSize) {
  if (SkipTestWithMappableSI) {
    return;
  }
  scoped_refptr<VideoFrame> software_frame =
      CreateTestNV12VideoFrameWithOddSize(135);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  if (gfx::IsOddWidthMultiPlanarBuffersAllowed() &&
      gfx::IsOddHeightMultiPlanarBuffersAllowed()) {
    EXPECT_NE(software_frame.get(), frame.get());
    EXPECT_EQ(PIXEL_FORMAT_NV12, frame->format());
    EXPECT_TRUE(frame->HasSharedImage());
    EXPECT_EQ(1u, sii_->shared_image_count());

    EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
    mock_gpu_factories_->created_memory_buffers()[0]->Map();

    const auto* y_memory = reinterpret_cast<uint8_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(0));
    const auto* uv_memory = reinterpret_cast<uint8_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(1));

    // Y plane = 135x135 = 18225, UV plan = 136x68 = 9248.
    EXPECT_EQ(kYValue,
              software_frame->visible_data(VideoFrame::Plane::kY)[18224]);
    EXPECT_EQ(kUValue,
              software_frame->visible_data(VideoFrame::Plane::kUV)[9246]);
    EXPECT_EQ(kVValue,
              software_frame->visible_data(VideoFrame::Plane::kUV)[9247]);

    // Compare the last pixel of each plane in |software_frame| and |frame|.
    // y_memory = 135x135, uv_memory = 136x68.
    auto y_stride = mock_gpu_factories_->created_memory_buffers()[0]->stride(0);
    EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kY)[18224],
              y_memory[y_stride * 134 + 134]);
    auto uv_stride =
        mock_gpu_factories_->created_memory_buffers()[0]->stride(1);
    EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kUV)[9246],
              uv_memory[uv_stride * 67 + 134]);
    EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kUV)[9247],
              uv_memory[uv_stride * 67 + 135]);

    mock_gpu_factories_->created_memory_buffers()[0]->Unmap();

  } else {
    EXPECT_EQ(software_frame.get(), frame.get());
  }
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareXR30Frame) {
  if (SkipTestWithMappableSI) {
    return;
  }
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10, 10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::XR30);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_XR30, frame->format());
  EXPECT_TRUE(frame->HasSharedImage());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata().read_lock_fences_enabled);

  EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
  mock_gpu_factories_->created_memory_buffers()[0]->Map();

  void* memory = mock_gpu_factories_->created_memory_buffers()[0]->memory(0);
  EXPECT_EQ(as_xr30(0, 311, 0), *static_cast<uint32_t*>(memory));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareP010Frame) {
  if (SkipTestWithMappableSI) {
    return;
  }
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10, 10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::P010);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_P010LE, frame->format());
  EXPECT_TRUE(frame->HasSharedImage());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata().read_lock_fences_enabled);

  EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
  mock_gpu_factories_->created_memory_buffers()[0]->Map();

  const uint16_t* y_memory = reinterpret_cast<uint16_t*>(
      mock_gpu_factories_->created_memory_buffers()[0]->memory(0));
  EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kY)[0] << 6,
            y_memory[0]);
  const uint16_t* uv_memory = reinterpret_cast<uint16_t*>(
      mock_gpu_factories_->created_memory_buffers()[0]->memory(1));
  EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kU)[0] << 6,
            uv_memory[0]);
  EXPECT_EQ(software_frame->visible_data(VideoFrame::Plane::kV)[0] << 6,
            uv_memory[1]);
}

TEST_F(GpuMemoryBufferVideoFramePoolTest,
       CreateOneHardwareP010FrameWithOddSize) {
  if (SkipTestWithMappableSI) {
    return;
  }
  scoped_refptr<VideoFrame> software_frame =
      CreateTestYUVVideoFrameWithOddSize(7, 10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::P010);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  if (gfx::IsOddWidthMultiPlanarBuffersAllowed() &&
      gfx::IsOddHeightMultiPlanarBuffersAllowed()) {
    EXPECT_NE(software_frame.get(), frame.get());
    EXPECT_EQ(PIXEL_FORMAT_P010LE, frame->format());
    EXPECT_TRUE(frame->HasSharedImage());
    EXPECT_EQ(1u, sii_->shared_image_count());
    EXPECT_TRUE(frame->metadata().read_lock_fences_enabled);

    EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
    mock_gpu_factories_->created_memory_buffers()[0]->Map();

    // Copy I010 To P010.
    const uint16_t* y_memory = reinterpret_cast<uint16_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(0));
    const uint16_t* uv_memory = reinterpret_cast<uint16_t*>(
        mock_gpu_factories_->created_memory_buffers()[0]->memory(1));

    const uint16_t* y_plane_data = reinterpret_cast<const uint16_t*>(
        software_frame->visible_data(VideoFrame::Plane::kY));
    const uint16_t* u_plane_data = reinterpret_cast<const uint16_t*>(
        software_frame->visible_data(VideoFrame::Plane::kU));
    const uint16_t* v_plane_data = reinterpret_cast<const uint16_t*>(
        software_frame->visible_data(VideoFrame::Plane::kV));

    // Y plane = 7x7 = 49, U and V plan = 4x4 = 16.
    EXPECT_EQ(kYValue, y_plane_data[48]);
    EXPECT_EQ(kUValue, u_plane_data[15]);
    EXPECT_EQ(kVValue, v_plane_data[15]);
    // Compare the last pixel of each plane in |software_frame| and |frame|.
    // y_memory = 7x7, uv_memory = 8x4, scale = 16-10 = 6.
    auto y_stride = mock_gpu_factories_->created_memory_buffers()[0]->stride(0);
    EXPECT_EQ(y_plane_data[48], y_memory[y_stride / 2 * 6 + 6] >> 6);
    auto uv_stride =
        mock_gpu_factories_->created_memory_buffers()[0]->stride(1);
    EXPECT_EQ(u_plane_data[15], uv_memory[uv_stride / 2 * 3 + 6] >> 6);
    EXPECT_EQ(v_plane_data[15], uv_memory[uv_stride / 2 * 3 + 7] >> 6);

    mock_gpu_factories_->created_memory_buffers()[0]->Unmap();
  } else {
    EXPECT_EQ(software_frame.get(), frame.get());
  }
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareXR30FrameBT709) {
  if (SkipTestWithMappableSI) {
    return;
  }
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
  EXPECT_TRUE(frame->HasSharedImage());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata().read_lock_fences_enabled);

  EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
  mock_gpu_factories_->created_memory_buffers()[0]->Map();

  void* memory = mock_gpu_factories_->created_memory_buffers()[0]->memory(0);
  EXPECT_EQ(as_xr30(0, 311, 0), *static_cast<uint32_t*>(memory));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareXR30FrameBT601) {
  if (SkipTestWithMappableSI) {
    return;
  }
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
  EXPECT_TRUE(frame->HasSharedImage());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata().read_lock_fences_enabled);

  EXPECT_EQ(1u, mock_gpu_factories_->created_memory_buffers().size());
  mock_gpu_factories_->created_memory_buffers()[0]->Map();

  void* memory = mock_gpu_factories_->created_memory_buffers()[0]->memory(0);
  EXPECT_EQ(as_xr30(0, 543, 0), *static_cast<uint32_t*>(memory));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareXB30Frame) {
  if (SkipTestWithMappableSI) {
    return;
  }
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10, 10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::XB30);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_XB30, frame->format());
  EXPECT_TRUE(frame->HasSharedImage());
  EXPECT_EQ(1u, sii_->shared_image_count());
  EXPECT_TRUE(frame->metadata().read_lock_fences_enabled);
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareRGBAFrame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVAVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_EQ(software_frame.get(), frame.get());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, PreservesMetadata) {
  gfx::HDRMetadata hdr_metadata;
  hdr_metadata.cta_861_3 = gfx::HdrMetadataCta861_3(5000, 1000);

  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  software_frame->metadata().end_of_stream = true;
  software_frame->set_hdr_metadata(hdr_metadata);

  base::TimeTicks kTestReferenceTime =
      base::Milliseconds(12345) + base::TimeTicks();
  software_frame->metadata().reference_time = kTestReferenceTime;
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_TRUE(frame->metadata().end_of_stream);
  EXPECT_EQ(hdr_metadata, frame->hdr_metadata());
  EXPECT_EQ(kTestReferenceTime, *frame->metadata().reference_time);
}

// CreateGpuMemoryBuffer can return null (e.g: when the GPU process is down).
// This test checks that in that case we don't crash and don't create the
// textures.
TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateGpuMemoryBufferFail) {
  if (SkipTestWithMappableSI) {
    return;
  }
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetFailToAllocateGpuMemoryBufferForTesting(true);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  // Software frame should be returned if mapping fails.
  EXPECT_EQ(software_frame.get(), frame.get());
  EXPECT_EQ(0u, sii_->shared_image_count());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest,
       CreateGpuMemoryBufferFailAfterShutdown) {
  if (SkipTestWithMappableSI) {
    return;
  }
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetFailToMapGpuMemoryBufferForTesting(true);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  gpu_memory_buffer_pool_.reset();
  RunUntilIdle();

  // Software frame should be returned if mapping fails.
  EXPECT_EQ(software_frame.get(), frame.get());
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

  EXPECT_EQ(2u, sii_->shared_image_count());

  // Drop frame and verify that resources are still available for reuse.
  frame_1 = nullptr;
  RunUntilIdle();
  EXPECT_EQ(2u, sii_->shared_image_count());

  // While still holding onto the second frame, destruct the frame pool and
  // verify that the inner pool releases the resources for the first frame.
  gpu_memory_buffer_pool_.reset();
  RunUntilIdle();

  EXPECT_EQ(1u, sii_->shared_image_count());
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

  EXPECT_EQ(2u, sii_->shared_image_count());

  // Drop frame and verify that resources are still available for reuse.
  frame_1 = nullptr;
  RunUntilIdle();
  EXPECT_EQ(2u, sii_->shared_image_count());

  // Advance clock far enough to hit stale timer; ensure only frame_1 has its
  // resources released.
  test_clock_.Advance(base::Minutes(1));
  frame_2 = nullptr;
  RunUntilIdle();
  EXPECT_EQ(1u, sii_->shared_image_count());
}

// Test when we request two copies in a row, there should be at most one frame
// copy in flight at any time.
TEST_F(GpuMemoryBufferVideoFramePoolTest, AtMostOneCopyInFlight) {
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12);

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
  scoped_refptr<VideoFrame> frame_1;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_1,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_1));

  scoped_refptr<VideoFrame> software_frame_2 = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_2;
  base::TimeTicks time_2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_2,
      base::BindOnce(MaybeCreateHardwareFrameCallbackAndTrackTime, &frame_2,
                     &time_2));

  scoped_refptr<VideoFrame> software_frame_3 = VideoFrame::CreateEOSFrame();
  scoped_refptr<VideoFrame> frame_3;
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

}  // namespace media
