// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/v4l2_capture_delegate_gpu_helper.h"

#include <ranges>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "media/base/test_data_util.h"
#include "media/capture/video/video_capture_gpu_channel_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::A;
using testing::AtLeast;
using testing::AtMost;
using testing::InvokeWithoutArgs;

namespace media {

namespace {

class MockV4l2GpuClient : public VideoCaptureDevice::Client {
 public:
  void OnIncomingCapturedData(const uint8_t* data,
                              int length,
                              const VideoCaptureFormat& frame_format,
                              const gfx::ColorSpace& color_space,
                              int clockwise_rotation,
                              bool flip_y,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp,
                              std::optional<base::TimeTicks> capture_begin_time,
                              const std::optional<VideoFrameMetadata>& metadata,
                              int frame_feedback_id = 0) override {}

  void OnIncomingCapturedImage(
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      const VideoCaptureFormat& frame_format,
      int clockwise_rotation,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      std::optional<base::TimeTicks> capture_begin_time,
      const std::optional<VideoFrameMetadata>& metadata,
      int frame_feedback_id = 0) override {}

  void OnIncomingCapturedExternalBuffer(
      CapturedExternalVideoBuffer buffer,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      std::optional<base::TimeTicks> capture_begin_time,
      const gfx::Rect& visible_rect,
      const std::optional<VideoFrameMetadata>& metadata) override {}

  void OnCaptureConfigurationChanged() override {}

  MOCK_METHOD6(ReserveOutputBuffer,
               ReserveResult(const gfx::Size&,
                             VideoPixelFormat,
                             int,
                             Buffer*,
                             int*,
                             int*));

  void OnIncomingCapturedBuffer(
      Buffer buffer,
      const VideoCaptureFormat& format,
      base::TimeTicks reference_,
      base::TimeDelta timestamp,
      std::optional<base::TimeTicks> capture_begin_time,
      const std::optional<VideoFrameMetadata>& metadata) override {}

  MOCK_METHOD8(OnIncomingCapturedBufferExt,
               void(Buffer,
                    const VideoCaptureFormat&,
                    const gfx::ColorSpace&,
                    base::TimeTicks,
                    base::TimeDelta,
                    std::optional<base::TimeTicks> capture_begin_time,
                    gfx::Rect,
                    const std::optional<VideoFrameMetadata>&));

  MOCK_METHOD3(OnError,
               void(VideoCaptureError,
                    const base::Location&,
                    const std::string&));

  MOCK_METHOD1(OnFrameDropped, void(VideoCaptureFrameDropReason));

  double GetBufferPoolUtilization() const override { return 0.0; }

  void OnStarted() override {}
};

class MockCaptureHandleProvider
    : public VideoCaptureDevice::Client::Buffer::HandleProvider {
 public:
  MockCaptureHandleProvider(const gfx::Size& size,
                            viz::SharedImageFormat format) {
    gmb_handle_ = gpu::TestSharedImageInterface::CreateGMBHandle(format, size);
  }
  // Duplicate as an writable (unsafe) shared memory region.
  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override {
    return base::UnsafeSharedMemoryRegion();
  }

  // Access a |VideoCaptureBufferHandle| for local, writable memory.
  std::unique_ptr<VideoCaptureBufferHandle> GetHandleForInProcessAccess()
      override {
    return nullptr;
  }

  // Clone a |GpuMemoryBufferHandle| for IPC.
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override {
    return gmb_handle_.Clone();
  }
  gfx::GpuMemoryBufferHandle gmb_handle_;
};

}  // namespace

class V4l2CaptureDelegateGpuHelperTest
    : public ::testing::TestWithParam<VideoCaptureFormat> {
 public:
  V4l2CaptureDelegateGpuHelperTest() {}
  ~V4l2CaptureDelegateGpuHelperTest() override = default;

 public:
  void SetUp() override {
    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(
        test_sii_);
    v4l2_gpu_helper_ = std::make_unique<V4L2CaptureDelegateGpuHelper>();
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  void SetUpNullSharedImageInterface() {
    VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(nullptr);
  }

  std::vector<uint8_t> ReadSampleData(VideoCaptureFormat format) {
    if (format.pixel_format == VideoPixelFormat::PIXEL_FORMAT_MJPEG) {
      auto file_path = media::GetTestDataFilePath("one_frame_1280x720.mjpeg");
      CHECK(!file_path.empty());
      return base::ReadFileToBytes(file_path).value();
    }

    auto size =
        VideoFrame::AllocationSize(format.pixel_format, format.frame_size);
    return std::vector<uint8_t>(size);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<V4L2CaptureDelegateGpuHelper> v4l2_gpu_helper_;
  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;
};

TEST_F(V4l2CaptureDelegateGpuHelperTest, FailureAsInvalidClient) {
  constexpr int kRotation = 0;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat capture_format = VideoCaptureFormat(
      gfx::Size(1280, 720), 30.0f, VideoPixelFormat::PIXEL_FORMAT_YUY2);
  std::vector<uint8_t> sample = ReadSampleData(capture_format);

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      nullptr, sample.data(), sample.size(), capture_format, gfx::ColorSpace(),
      kRotation, reference_time, timestamp);
  EXPECT_NE(status, 0);
}

TEST_F(V4l2CaptureDelegateGpuHelperTest,
       FailureAsInvalidMJPEGCaptureSampleData) {
  constexpr int kRotation = 0;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat capture_format = VideoCaptureFormat(
      gfx::Size(1280, 720), 30.0f, VideoPixelFormat::PIXEL_FORMAT_MJPEG);
  std::vector<uint8_t> sample = ReadSampleData(capture_format);
  MockV4l2GpuClient client;

  // corrupt the sample data
  std::ranges::fill(sample, 0xff);

  EXPECT_CALL(client, ReserveOutputBuffer)
      .WillRepeatedly(
          [](const gfx::Size& size, VideoPixelFormat pixel_format,
             int feedback_id,
             VideoCaptureDevice::Client::Buffer* capture_buffer,
             int* require_new_buffer_id, int* retire_old_buffer_id) {
            EXPECT_EQ(pixel_format, PIXEL_FORMAT_NV12);
            capture_buffer->handle_provider =
                std::make_unique<MockCaptureHandleProvider>(
                    size, viz::MultiPlaneFormat::kNV12);
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          });
  EXPECT_CALL(client, OnFrameDropped(_))
      .WillRepeatedly([](VideoCaptureFrameDropReason reason) {
        EXPECT_EQ(reason, VideoCaptureFrameDropReason::kNone);
      });

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      &client, sample.data(), sample.size(), capture_format, gfx::ColorSpace(),
      kRotation, reference_time, timestamp);
  EXPECT_NE(status, 0);
}

TEST_F(V4l2CaptureDelegateGpuHelperTest, FailureAsReserveOutputBufferErr) {
  constexpr int kRotation = 0;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat capture_format = VideoCaptureFormat(
      gfx::Size(1280, 720), 30.0f, VideoPixelFormat::PIXEL_FORMAT_YUY2);
  std::vector<uint8_t> sample = ReadSampleData(capture_format);
  MockV4l2GpuClient client;

  EXPECT_CALL(client, ReserveOutputBuffer)
      .WillRepeatedly([](const gfx::Size& size, VideoPixelFormat pixel_format,
                         int feedback_id,
                         VideoCaptureDevice::Client::Buffer* capture_buffer,
                         int* require_new_buffer_id,
                         int* retire_old_buffer_id) {
        return VideoCaptureDevice::Client::ReserveResult::kAllocationFailed;
      });
  EXPECT_CALL(client, OnFrameDropped(_))
      .WillRepeatedly([](VideoCaptureFrameDropReason reason) {
        EXPECT_EQ(
            reason,
            VideoCaptureFrameDropReason::kBufferPoolBufferAllocationFailed);
      });

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      &client, sample.data(), sample.size(), capture_format, gfx::ColorSpace(),
      kRotation, reference_time, timestamp);
  EXPECT_NE(status, 0);
}

TEST_F(V4l2CaptureDelegateGpuHelperTest, FailureAsInvalidSharedImageInterface) {
  SetUpNullSharedImageInterface();
  constexpr int kRotation = 0;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat capture_format = VideoCaptureFormat(
      gfx::Size(1280, 720), 30.0f, VideoPixelFormat::PIXEL_FORMAT_YUY2);
  std::vector<uint8_t> sample = ReadSampleData(capture_format);
  MockV4l2GpuClient client;

  EXPECT_CALL(client, ReserveOutputBuffer)
      .WillRepeatedly(
          [](const gfx::Size& size, VideoPixelFormat pixel_format,
             int feedback_id,
             VideoCaptureDevice::Client::Buffer* capture_buffer,
             int* require_new_buffer_id, int* retire_old_buffer_id) {
            EXPECT_EQ(pixel_format, PIXEL_FORMAT_NV12);
            capture_buffer->handle_provider =
                std::make_unique<MockCaptureHandleProvider>(
                    size, viz::MultiPlaneFormat::kNV12);
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          });
  EXPECT_CALL(client, OnFrameDropped(_))
      .WillRepeatedly([](VideoCaptureFrameDropReason reason) {
        EXPECT_EQ(
            reason,
            VideoCaptureFrameDropReason::kBufferPoolBufferAllocationFailed);
      });

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      &client, sample.data(), sample.size(), capture_format, gfx::ColorSpace(),
      kRotation, reference_time, timestamp);
  EXPECT_NE(status, 0);
}

TEST_F(V4l2CaptureDelegateGpuHelperTest, SuccessRotationIsNotZero) {
  constexpr int kRotation = 180;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat capture_format = VideoCaptureFormat(
      gfx::Size(1280, 720), 30.0f, VideoPixelFormat::PIXEL_FORMAT_YUY2);
  std::vector<uint8_t> sample = ReadSampleData(capture_format);
  MockV4l2GpuClient client;

  EXPECT_CALL(client, ReserveOutputBuffer)
      .WillRepeatedly(
          [](const gfx::Size& size, VideoPixelFormat pixel_format,
             int feedback_id,
             VideoCaptureDevice::Client::Buffer* capture_buffer,
             int* require_new_buffer_id, int* retire_old_buffer_id) {
            EXPECT_EQ(pixel_format, PIXEL_FORMAT_NV12);
            capture_buffer->handle_provider =
                std::make_unique<MockCaptureHandleProvider>(
                    size, viz::MultiPlaneFormat::kNV12);
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          });
  EXPECT_CALL(client, OnIncomingCapturedBufferExt)
      .WillRepeatedly(InvokeWithoutArgs([]() {}));

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      &client, sample.data(), sample.size(), capture_format, gfx::ColorSpace(),
      kRotation, reference_time, timestamp);

  EXPECT_EQ(status, 0);
}

TEST_P(V4l2CaptureDelegateGpuHelperTest, SuccessConvertWithCaptureParam) {
  constexpr int kRotation = 0;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat& capture_format = GetParam();
  const std::vector<uint8_t> sample = ReadSampleData(capture_format);
  MockV4l2GpuClient client;

  EXPECT_CALL(client, ReserveOutputBuffer)
      .WillRepeatedly(
          [](const gfx::Size& size, VideoPixelFormat pixel_format,
             int feedback_id,
             VideoCaptureDevice::Client::Buffer* capture_buffer,
             int* require_new_buffer_id, int* retire_old_buffer_id) {
            EXPECT_EQ(pixel_format, PIXEL_FORMAT_NV12);
            capture_buffer->handle_provider =
                std::make_unique<MockCaptureHandleProvider>(
                    size, viz::MultiPlaneFormat::kNV12);
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          });
  EXPECT_CALL(client, OnIncomingCapturedBufferExt)
      .WillRepeatedly(InvokeWithoutArgs([]() {}));

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      &client, sample.data(), sample.size(), capture_format, gfx::ColorSpace(),
      kRotation, reference_time, timestamp);
  EXPECT_EQ(status, 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    V4l2CaptureDelegateGpuHelperTest,
    ::testing::Values(
        VideoCaptureFormat(gfx::Size(1280, 720),
                           30.0f,
                           VideoPixelFormat::PIXEL_FORMAT_NV12),
        VideoCaptureFormat(gfx::Size(1280, 720),
                           30.0f,
                           VideoPixelFormat::PIXEL_FORMAT_YUY2),
        VideoCaptureFormat(gfx::Size(1280, 720),
                           30.0f,
                           VideoPixelFormat::PIXEL_FORMAT_MJPEG),
        VideoCaptureFormat(gfx::Size(1280, 720),
                           30.0f,
                           VideoPixelFormat::PIXEL_FORMAT_RGB24)));

}  // namespace media
