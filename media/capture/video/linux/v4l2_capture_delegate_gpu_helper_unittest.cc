// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
#include "media/capture/video/linux/v4l2_capture_delegate_gpu_helper.h"

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/base/test_data_util.h"
#include "media/capture/video/mock_gpu_memory_buffer_manager.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::A;
using testing::AtLeast;
using testing::AtMost;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace media {

namespace {

constexpr char kMjpegFrameFile[] = "one_frame_1280x720.mjpeg";
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
                              int frame_feedback_id = 0) override {}

  void OnIncomingCapturedGfxBuffer(
      gfx::GpuMemoryBuffer* buffer,
      const VideoCaptureFormat& frame_format,
      int clockwise_rotation,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      std::optional<base::TimeTicks> capture_begin_time,
      int frame_feedback_id = 0) override {}

  void OnIncomingCapturedExternalBuffer(
      CapturedExternalVideoBuffer buffer,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      std::optional<base::TimeTicks> capture_begin_time,
      const gfx::Rect& visible_rect) override {}

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
      std::optional<base::TimeTicks> capture_begin_time) override {}

  MOCK_METHOD8(OnIncomingCapturedBufferExt,
               void(Buffer,
                    const VideoCaptureFormat&,
                    const gfx::ColorSpace&,
                    base::TimeTicks,
                    base::TimeDelta,
                    std::optional<base::TimeTicks> capture_begin_time,
                    gfx::Rect,
                    const VideoFrameMetadata&));

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
  MockCaptureHandleProvider(const gfx::Size& size, gfx::BufferFormat format) {
    gmb_ = std::make_unique<FakeGpuMemoryBuffer>(size, format);
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
    gfx::GpuMemoryBufferHandle handle;
    return gmb_->CloneHandle();
  }
  std::unique_ptr<FakeGpuMemoryBuffer> gmb_;
};

class InvalidGpuMemoryBufferSupport : public gpu::GpuMemoryBufferSupport {
 public:
  std::unique_ptr<gpu::GpuMemoryBufferImpl> CreateGpuMemoryBufferImplFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::GpuMemoryBufferImpl::DestructionCallback callback,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager = nullptr,
      scoped_refptr<base::UnsafeSharedMemoryPool> pool = nullptr,
      base::span<uint8_t> premapped_memory = base::span<uint8_t>()) override {
    return nullptr;
  }
};
}  // namespace

class V4l2CaptureDelegateGpuHelperTest
    : public ::testing::TestWithParam<VideoCaptureFormat> {
 public:
  V4l2CaptureDelegateGpuHelperTest() {}
  ~V4l2CaptureDelegateGpuHelperTest() override = default;

 public:
  void SetUp() override {}

  void TearDown() override { task_environment_.RunUntilIdle(); }

  void SetUpWithFakeGpuMemoryBufferSupport() {
    std::unique_ptr<FakeGpuMemoryBufferSupport> gbm_support =
        std::make_unique<FakeGpuMemoryBufferSupport>();
    v4l2_gpu_helper_ =
        std::make_unique<V4L2CaptureDelegateGpuHelper>(std::move(gbm_support));
  }
  void SetUpWithInvalidGpuMemoryBufferSupport() {
    std::unique_ptr<InvalidGpuMemoryBufferSupport> gbm_support =
        std::make_unique<InvalidGpuMemoryBufferSupport>();
    v4l2_gpu_helper_ =
        std::make_unique<V4L2CaptureDelegateGpuHelper>(std::move(gbm_support));
  }

  std::unique_ptr<std::vector<uint8_t>> ReadSampleData(
      VideoCaptureFormat format) {
    std::unique_ptr<std::vector<uint8_t>> sample =
        std::make_unique<std::vector<uint8_t>>();
    if (format.pixel_format == VideoPixelFormat::PIXEL_FORMAT_MJPEG) {
      auto file_path = media::GetTestDataFilePath(kMjpegFrameFile);
      if (!file_path.empty()) {
        FILE* fp = fopen(file_path.value().c_str(), "rb");
        if (fp) {
          fseek(fp, 0, SEEK_END);
          size_t size = ftell(fp);
          sample->resize(size);
          fseek(fp, 0, SEEK_SET);
          size_t read_size = fread(sample->data(), 1, size, fp);
          EXPECT_EQ(size, read_size);
          fclose(fp);
        }
      }
    } else {
      auto size =
          VideoFrame::AllocationSize(format.pixel_format, format.frame_size);
      sample->resize(size);
    }
    return sample;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<V4L2CaptureDelegateGpuHelper> v4l2_gpu_helper_;
};

TEST_F(V4l2CaptureDelegateGpuHelperTest, FailureAsInvalidClient) {
  SetUpWithFakeGpuMemoryBufferSupport();
  constexpr int kRotation = 0;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat capture_format = VideoCaptureFormat(
      gfx::Size(1280, 720), 30.0f, VideoPixelFormat::PIXEL_FORMAT_YUY2);
  std::unique_ptr<std::vector<uint8_t>> sample = ReadSampleData(capture_format);

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      nullptr, sample->data(), sample->size(), capture_format,
      gfx::ColorSpace(), kRotation, reference_time, timestamp);
  EXPECT_NE(status, 0);
}

TEST_F(V4l2CaptureDelegateGpuHelperTest,
       FailureAsInvalidMJPEGCaptureSampleData) {
  SetUpWithFakeGpuMemoryBufferSupport();
  constexpr int kRotation = 0;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat capture_format = VideoCaptureFormat(
      gfx::Size(1280, 720), 30.0f, VideoPixelFormat::PIXEL_FORMAT_MJPEG);
  std::unique_ptr<std::vector<uint8_t>> sample = ReadSampleData(capture_format);
  MockV4l2GpuClient client;

  if (sample) {
    // corrupt the sample data
    uint8_t* data = sample->data();
    for (size_t i = 0; i < 0xff && i < sample->size(); i++) {
      data[i] = 0xff;
    }
  }

  EXPECT_CALL(client, ReserveOutputBuffer)
      .WillRepeatedly(
          Invoke([](const gfx::Size& size, VideoPixelFormat pixel_format,
                    int feedback_id,
                    VideoCaptureDevice::Client::Buffer* capture_buffer,
                    int* require_new_buffer_id, int* retire_old_buffer_id) {
            EXPECT_EQ(pixel_format, PIXEL_FORMAT_NV12);
            capture_buffer->handle_provider =
                std::make_unique<MockCaptureHandleProvider>(
                    size, gfx::BufferFormat::YUV_420_BIPLANAR);
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          }));
  EXPECT_CALL(client, OnFrameDropped(_))
      .WillRepeatedly(Invoke([](VideoCaptureFrameDropReason reason) {
        EXPECT_EQ(reason, VideoCaptureFrameDropReason::kNone);
      }));

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      &client, sample->data(), sample->size(), capture_format,
      gfx::ColorSpace(), kRotation, reference_time, timestamp);
  EXPECT_NE(status, 0);
}

TEST_F(V4l2CaptureDelegateGpuHelperTest, FailureAsReserveOutputBufferErr) {
  SetUpWithFakeGpuMemoryBufferSupport();
  constexpr int kRotation = 0;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat capture_format = VideoCaptureFormat(
      gfx::Size(1280, 720), 30.0f, VideoPixelFormat::PIXEL_FORMAT_YUY2);
  std::unique_ptr<std::vector<uint8_t>> sample = ReadSampleData(capture_format);
  MockV4l2GpuClient client;

  EXPECT_CALL(client, ReserveOutputBuffer)
      .WillRepeatedly(
          Invoke([](const gfx::Size& size, VideoPixelFormat pixel_format,
                    int feedback_id,
                    VideoCaptureDevice::Client::Buffer* capture_buffer,
                    int* require_new_buffer_id, int* retire_old_buffer_id) {
            return VideoCaptureDevice::Client::ReserveResult::kAllocationFailed;
          }));
  EXPECT_CALL(client, OnFrameDropped(_))
      .WillRepeatedly(Invoke([](VideoCaptureFrameDropReason reason) {
        EXPECT_EQ(
            reason,
            VideoCaptureFrameDropReason::kBufferPoolBufferAllocationFailed);
      }));

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      &client, sample->data(), sample->size(), capture_format,
      gfx::ColorSpace(), kRotation, reference_time, timestamp);
  EXPECT_NE(status, 0);
}

TEST_F(V4l2CaptureDelegateGpuHelperTest,
       FailureAsInvalidCreateGpuMemoryBuffer) {
  SetUpWithInvalidGpuMemoryBufferSupport();
  constexpr int kRotation = 0;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat capture_format = VideoCaptureFormat(
      gfx::Size(1280, 720), 30.0f, VideoPixelFormat::PIXEL_FORMAT_YUY2);
  std::unique_ptr<std::vector<uint8_t>> sample = ReadSampleData(capture_format);
  MockV4l2GpuClient client;

  EXPECT_CALL(client, ReserveOutputBuffer)
      .WillRepeatedly(
          Invoke([](const gfx::Size& size, VideoPixelFormat pixel_format,
                    int feedback_id,
                    VideoCaptureDevice::Client::Buffer* capture_buffer,
                    int* require_new_buffer_id, int* retire_old_buffer_id) {
            EXPECT_EQ(pixel_format, PIXEL_FORMAT_NV12);
            capture_buffer->handle_provider =
                std::make_unique<MockCaptureHandleProvider>(
                    size, gfx::BufferFormat::YUV_420_BIPLANAR);
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          }));
  EXPECT_CALL(client, OnFrameDropped(_))
      .WillRepeatedly(Invoke([](VideoCaptureFrameDropReason reason) {
        EXPECT_EQ(
            reason,
            VideoCaptureFrameDropReason::kBufferPoolBufferAllocationFailed);
      }));

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      &client, sample->data(), sample->size(), capture_format,
      gfx::ColorSpace(), kRotation, reference_time, timestamp);
  EXPECT_NE(status, 0);
}

TEST_F(V4l2CaptureDelegateGpuHelperTest, SuccessRotationIsNotZero) {
  SetUpWithFakeGpuMemoryBufferSupport();
  constexpr int kRotation = 180;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat capture_format = VideoCaptureFormat(
      gfx::Size(1280, 720), 30.0f, VideoPixelFormat::PIXEL_FORMAT_YUY2);
  std::unique_ptr<std::vector<uint8_t>> sample = ReadSampleData(capture_format);
  MockV4l2GpuClient client;

  EXPECT_CALL(client, ReserveOutputBuffer)
      .WillRepeatedly(
          Invoke([](const gfx::Size& size, VideoPixelFormat pixel_format,
                    int feedback_id,
                    VideoCaptureDevice::Client::Buffer* capture_buffer,
                    int* require_new_buffer_id, int* retire_old_buffer_id) {
            EXPECT_EQ(pixel_format, PIXEL_FORMAT_NV12);
            capture_buffer->handle_provider =
                std::make_unique<MockCaptureHandleProvider>(
                    size, gfx::BufferFormat::YUV_420_BIPLANAR);
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          }));
  EXPECT_CALL(client, OnIncomingCapturedBufferExt)
      .WillRepeatedly(InvokeWithoutArgs([]() {}));

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      &client, sample->data(), sample->size(), capture_format,
      gfx::ColorSpace(), kRotation, reference_time, timestamp);

  EXPECT_EQ(status, 0);
}

TEST_P(V4l2CaptureDelegateGpuHelperTest, SuccessConvertWithCaptureParam) {
  SetUpWithFakeGpuMemoryBufferSupport();
  constexpr int kRotation = 0;
  const base::TimeTicks reference_time = base::TimeTicks::Now();
  const base::TimeDelta timestamp = reference_time - reference_time;
  const VideoCaptureFormat& capture_format = GetParam();
  const std::unique_ptr<std::vector<uint8_t>> sample =
      ReadSampleData(capture_format);
  MockV4l2GpuClient client;

  EXPECT_CALL(client, ReserveOutputBuffer)
      .WillRepeatedly(
          Invoke([](const gfx::Size& size, VideoPixelFormat pixel_format,
                    int feedback_id,
                    VideoCaptureDevice::Client::Buffer* capture_buffer,
                    int* require_new_buffer_id, int* retire_old_buffer_id) {
            EXPECT_EQ(pixel_format, PIXEL_FORMAT_NV12);
            capture_buffer->handle_provider =
                std::make_unique<MockCaptureHandleProvider>(
                    size, gfx::BufferFormat::YUV_420_BIPLANAR);
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          }));
  EXPECT_CALL(client, OnIncomingCapturedBufferExt)
      .WillRepeatedly(InvokeWithoutArgs([]() {}));

  int status = v4l2_gpu_helper_->OnIncomingCapturedData(
      &client, sample->data(), sample->size(), capture_format,
      gfx::ColorSpace(), kRotation, reference_time, timestamp);
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
