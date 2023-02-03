// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device_client.h"

#include <stddef.h>

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/capture/video/mock_gpu_memory_buffer_manager.h"
#include "media/capture/video/mock_video_frame_receiver.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_frame_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/video_capture_jpeg_decoder.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::SaveArg;

namespace media {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<VideoCaptureJpegDecoder> ReturnNullPtrAsJpecDecoder() {
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

// Test fixture for testing a unit consisting of an instance of
// VideoCaptureDeviceClient connected to a partly-mocked instance of
// VideoCaptureController, and an instance of VideoCaptureBufferPoolImpl
// as well as related threading glue that replicates how it is used in
// production.
class VideoCaptureDeviceClientTest : public ::testing::Test {
 public:
  VideoCaptureDeviceClientTest() {
    scoped_refptr<VideoCaptureBufferPoolImpl> buffer_pool(
        new VideoCaptureBufferPoolImpl(VideoCaptureBufferType::kSharedMemory,
                                       2));
    auto controller = std::make_unique<NiceMock<MockVideoFrameReceiver>>();
    receiver_ = controller.get();
    gpu_memory_buffer_manager_ =
        std::make_unique<unittest_internal::MockGpuMemoryBufferManager>();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    device_client_ = std::make_unique<VideoCaptureDeviceClient>(
        VideoCaptureBufferType::kSharedMemory, std::move(controller),
        buffer_pool, base::BindRepeating(&ReturnNullPtrAsJpecDecoder));
#else
    device_client_ = std::make_unique<VideoCaptureDeviceClient>(
        VideoCaptureBufferType::kSharedMemory, std::move(controller),
        buffer_pool);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  VideoCaptureDeviceClientTest(const VideoCaptureDeviceClientTest&) = delete;
  VideoCaptureDeviceClientTest& operator=(const VideoCaptureDeviceClientTest&) =
      delete;

  ~VideoCaptureDeviceClientTest() override = default;

 protected:
  std::unique_ptr<unittest_internal::MockGpuMemoryBufferManager>
      gpu_memory_buffer_manager_;

  // Must outlive `receiver_`.
  std::unique_ptr<VideoCaptureDeviceClient> device_client_;
  raw_ptr<NiceMock<MockVideoFrameReceiver>> receiver_;
};

// A small test for reference and to verify VideoCaptureDeviceClient is
// minimally functional.
TEST_F(VideoCaptureDeviceClientTest, Minimal) {
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  const VideoCaptureFormat kFrameFormat(gfx::Size(10, 10), 30.0f /*frame_rate*/,
                                        PIXEL_FORMAT_I420);
  const gfx::ColorSpace kColorSpace = gfx::ColorSpace::CreateREC601();
  DCHECK(device_client_.get());
  {
    InSequence s;
    const int expected_buffer_id = 0;
    EXPECT_CALL(*receiver_, OnLog(_));
    EXPECT_CALL(*receiver_, MockOnNewBufferHandle(expected_buffer_id));
    EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer(expected_buffer_id, _, _));
  }
  device_client_->OnIncomingCapturedData(
      data, kScratchpadSizeInBytes, kFrameFormat, kColorSpace,
      0 /* clockwise rotation */, false /* flip_y */, base::TimeTicks(),
      base::TimeDelta());

  const gfx::Size kBufferDimensions(10, 10);
  const VideoCaptureFormat kFrameFormatNV12(
      kBufferDimensions, 30.0f /*frame_rate*/, PIXEL_FORMAT_NV12);
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer =
      gpu_memory_buffer_manager_->CreateFakeGpuMemoryBuffer(
          kBufferDimensions, gfx::BufferFormat::YUV_420_BIPLANAR,
          gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
          gpu::kNullSurfaceHandle, nullptr);
  {
    InSequence s;
    const int expected_buffer_id = 0;
    EXPECT_CALL(*receiver_, OnLog(_));
    EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer(expected_buffer_id, _, _));
    EXPECT_CALL(*receiver_, OnBufferRetired(expected_buffer_id));
  }
  device_client_->OnIncomingCapturedGfxBuffer(
      buffer.get(), kFrameFormatNV12, 0 /*clockwise rotation*/,
      base::TimeTicks(), base::TimeDelta());

  // Releasing |device_client_| will also release |receiver_|.
  receiver_ = nullptr;  // Avoid dangling reference.
  device_client_.reset();
}

// Tests that we don't try to pass on frames with an invalid frame format.
TEST_F(VideoCaptureDeviceClientTest, FailsSilentlyGivenInvalidFrameFormat) {
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  // kFrameFormat is invalid in a number of ways.
  const VideoCaptureFormat kFrameFormat(
      gfx::Size(limits::kMaxDimension + 1, limits::kMaxDimension),
      limits::kMaxFramesPerSecond + 1, VideoPixelFormat::PIXEL_FORMAT_I420);
  const gfx::ColorSpace kColorSpace = gfx::ColorSpace::CreateREC601();
  DCHECK(device_client_.get());
  // Expect the the call to fail silently inside the VideoCaptureDeviceClient.
  EXPECT_CALL(*receiver_, OnLog(_)).Times(AtLeast(1));
  EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer(_, _, _)).Times(0);
  device_client_->OnIncomingCapturedData(
      data, kScratchpadSizeInBytes, kFrameFormat, kColorSpace,
      0 /* clockwise rotation */, false /* flip_y */, base::TimeTicks(),
      base::TimeDelta());

  const gfx::Size kBufferDimensions(10, 10);
  const VideoCaptureFormat kFrameFormatNV12(
      kBufferDimensions, 30.0f /*frame_rate*/, PIXEL_FORMAT_NV12);
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer =
      gpu_memory_buffer_manager_->CreateFakeGpuMemoryBuffer(
          kBufferDimensions, gfx::BufferFormat::YUV_420_BIPLANAR,
          gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE, gpu::kNullSurfaceHandle,
          nullptr);
  EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer(_, _, _)).Times(0);
  device_client_->OnIncomingCapturedGfxBuffer(
      buffer.get(), kFrameFormat, 0 /*clockwise rotation*/, base::TimeTicks(),
      base::TimeDelta());

  Mock::VerifyAndClearExpectations(receiver_);
}

// Tests that we fail silently if no available buffers to use.
TEST_F(VideoCaptureDeviceClientTest, DropsFrameIfNoBuffer) {
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  const VideoCaptureFormat kFrameFormat(gfx::Size(10, 10), 30.0f /*frame_rate*/,
                                        PIXEL_FORMAT_I420);
  const gfx::ColorSpace kColorSpace = gfx::ColorSpace::CreateREC601();
  EXPECT_CALL(*receiver_, OnLog(_)).Times(1);
  // Simulate that receiver still holds |buffer_read_permission| for the first
  // two buffers when the third call to OnIncomingCapturedData comes in.
  // Since we set up the buffer pool to max out at 2 buffer, this should cause
  // |device_client_| to drop the frame.
  std::vector<std::unique_ptr<
      VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>>
      read_permission;
  EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer(_, _, _))
      .Times(2)
      .WillRepeatedly(Invoke(
          [&read_permission](
              int buffer_id,
              std::unique_ptr<
                  VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>*
                  buffer_read_permission,
              const gfx::Size&) {
            read_permission.push_back(std::move(*buffer_read_permission));
          }));
  // Pass three frames. The third will be dropped.
  device_client_->OnIncomingCapturedData(
      data, kScratchpadSizeInBytes, kFrameFormat, kColorSpace,
      0 /* clockwise rotation */, false /* flip_y */, base::TimeTicks(),
      base::TimeDelta());
  device_client_->OnIncomingCapturedData(
      data, kScratchpadSizeInBytes, kFrameFormat, kColorSpace,
      0 /* clockwise rotation */, false /* flip_y */, base::TimeTicks(),
      base::TimeDelta());
  device_client_->OnIncomingCapturedData(
      data, kScratchpadSizeInBytes, kFrameFormat, kColorSpace,
      0 /* clockwise rotation */, false /* flip_y */, base::TimeTicks(),
      base::TimeDelta());
  Mock::VerifyAndClearExpectations(receiver_);
}

// Tests that buffer-based capture API accepts some memory-backed pixel formats.
TEST_F(VideoCaptureDeviceClientTest, DataCaptureGoodPixelFormats) {
  // The usual ReserveOutputBuffer() -> OnIncomingCapturedVideoFrame() cannot
  // be used since it does not accept all pixel formats. The memory backed
  // buffer OnIncomingCapturedData() is used instead, with a dummy scratchpad
  // buffer.
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  const gfx::Size kCaptureResolution(10, 10);
  ASSERT_GE(kScratchpadSizeInBytes, kCaptureResolution.GetArea() * 4u)
      << "Scratchpad is too small to hold the largest pixel format (ARGB).";

  VideoCaptureParams params;
  params.requested_format =
      VideoCaptureFormat(kCaptureResolution, 30.0f, PIXEL_FORMAT_UNKNOWN);

  // Only use the VideoPixelFormats that we know supported. Do not add
  // PIXEL_FORMAT_MJPEG since it would need a real JPEG header.
  const VideoPixelFormat kSupportedFormats[] = {
    PIXEL_FORMAT_I420,
    PIXEL_FORMAT_YV12,
    PIXEL_FORMAT_NV12,
    PIXEL_FORMAT_NV21,
    PIXEL_FORMAT_YUY2,
    PIXEL_FORMAT_UYVY,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    PIXEL_FORMAT_RGB24,
#endif
    PIXEL_FORMAT_ARGB,
    PIXEL_FORMAT_Y16,
  };
  const gfx::ColorSpace kColorSpace = gfx::ColorSpace::CreateSRGB();

  for (VideoPixelFormat format : kSupportedFormats) {
    params.requested_format.pixel_format = format;

    EXPECT_CALL(*receiver_, OnLog(_)).Times(1);
    EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer(_, _, _)).Times(1);
    device_client_->OnIncomingCapturedData(
        data,
        media::VideoFrame::AllocationSize(params.requested_format.pixel_format,
                                          params.requested_format.frame_size),
        params.requested_format, kColorSpace, 0 /* clockwise_rotation */,
        false /* flip_y */, base::TimeTicks(), base::TimeDelta());
    Mock::VerifyAndClearExpectations(receiver_);
  }
}

// Test that we receive the expected resolution for a given captured frame
// resolution and rotation. Odd resolutions are also cropped.
TEST_F(VideoCaptureDeviceClientTest, CheckRotationsAndCrops) {
  const struct SizeAndRotation {
    gfx::Size input_resolution;
    int rotation;
    gfx::Size output_resolution;
  } kSizeAndRotations[] = {{{6, 4}, 0, {6, 4}},   {{6, 4}, 90, {4, 6}},
                           {{6, 4}, 180, {6, 4}}, {{6, 4}, 270, {4, 6}},
                           {{7, 4}, 0, {6, 4}},   {{7, 4}, 90, {4, 6}},
                           {{7, 4}, 180, {6, 4}}, {{7, 4}, 270, {4, 6}}};

  // The usual ReserveOutputBuffer() -> OnIncomingCapturedVideoFrame() cannot
  // be used since it does not resolve rotations or crops. The memory backed
  // buffer OnIncomingCapturedData() is used instead, with a dummy scratchpad
  // buffer.
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};

  EXPECT_CALL(*receiver_, OnLog(_)).Times(1);

  VideoCaptureParams params;
  for (const auto& size_and_rotation : kSizeAndRotations) {
    ASSERT_GE(kScratchpadSizeInBytes,
              size_and_rotation.input_resolution.GetArea() * 4u)
        << "Scratchpad is too small to hold the largest pixel format (ARGB).";
    params.requested_format = VideoCaptureFormat(
        size_and_rotation.input_resolution, 30.0f, PIXEL_FORMAT_ARGB);
    gfx::Size coded_size;
    EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer(_, _, _))
        .Times(1)
        .WillOnce(SaveArg<2>(&coded_size));
    device_client_->OnIncomingCapturedData(
        data,
        media::VideoFrame::AllocationSize(params.requested_format.pixel_format,
                                          params.requested_format.frame_size),
        params.requested_format, gfx::ColorSpace(), size_and_rotation.rotation,
        false /* flip_y */, base::TimeTicks(), base::TimeDelta());

    EXPECT_EQ(coded_size.width(), size_and_rotation.output_resolution.width());
    EXPECT_EQ(coded_size.height(),
              size_and_rotation.output_resolution.height());

    Mock::VerifyAndClearExpectations(receiver_);
  }

  SizeAndRotation kSizeAndRotationsNV12[] = {{{6, 4}, 0, {6, 4}},
                                             {{6, 4}, 90, {4, 6}},
                                             {{6, 4}, 180, {6, 4}},
                                             {{6, 4}, 270, {4, 6}}};
  EXPECT_CALL(*receiver_, OnLog(_)).Times(1);

  for (const auto& size_and_rotation : kSizeAndRotationsNV12) {
    params.requested_format = VideoCaptureFormat(
        size_and_rotation.input_resolution, 30.0f, PIXEL_FORMAT_NV12);
    std::unique_ptr<gfx::GpuMemoryBuffer> buffer =
        gpu_memory_buffer_manager_->CreateFakeGpuMemoryBuffer(
            size_and_rotation.input_resolution,
            gfx::BufferFormat::YUV_420_BIPLANAR,
            gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
            gpu::kNullSurfaceHandle, nullptr);

    gfx::Size coded_size;
    EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer(_, _, _))
        .Times(1)
        .WillOnce(SaveArg<2>(&coded_size));
    device_client_->OnIncomingCapturedGfxBuffer(
        buffer.get(), params.requested_format, size_and_rotation.rotation,
        base::TimeTicks(), base::TimeDelta());

    EXPECT_EQ(coded_size.width(), size_and_rotation.output_resolution.width());
    EXPECT_EQ(coded_size.height(),
              size_and_rotation.output_resolution.height());

    Mock::VerifyAndClearExpectations(receiver_);
  }
}

}  // namespace media
