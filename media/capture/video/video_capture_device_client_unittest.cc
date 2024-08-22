// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device_client.h"

#include <stddef.h>

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "media/capture/video/mock_gpu_memory_buffer_manager.h"
#include "media/capture/video/mock_video_frame_receiver.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/capture/video/video_capture_buffer_tracker_factory.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_frame_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/test/fake_video_effects_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/video_capture_jpeg_decoder.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::SaveArg;

namespace media {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<VideoCaptureJpegDecoder> ReturnNullPtrAsJpecDecoder() {
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class FakeVideoEffectsManagerImpl : public media::mojom::VideoEffectsManager {
  void GetConfiguration(GetConfigurationCallback callback) override {}
  void SetConfiguration(
      media::mojom::VideoEffectsConfigurationPtr configuration,
      SetConfigurationCallback callback) override {}
  void AddObserver(
      ::mojo::PendingRemote<media::mojom::VideoEffectsConfigurationObserver>
          observer) override {}
};

class FakeVideoCaptureBufferHandle : public VideoCaptureBufferHandle {
 public:
  size_t mapped_size() const override { return 1024; }
  uint8_t* data() const override { return nullptr; }
  const uint8_t* const_data() const override { return nullptr; }
};

class FakeVideoCaptureBufferTracker : public VideoCaptureBufferTracker {
 public:
  bool Init(const gfx::Size& dimensions,
            VideoPixelFormat format,
            const mojom::PlaneStridesPtr& strides) override {
    return true;
  }
  bool IsReusableForFormat(const gfx::Size& dimensions,
                           VideoPixelFormat format,
                           const mojom::PlaneStridesPtr& strides) override {
    return true;
  }
  uint32_t GetMemorySizeInBytes() override { return 1024; }
  std::unique_ptr<VideoCaptureBufferHandle> GetMemoryMappedAccess() override {
    return std::make_unique<FakeVideoCaptureBufferHandle>();
  }
  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override {
    return base::UnsafeSharedMemoryRegion();
  }
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override {
    return gfx::GpuMemoryBufferHandle();
  }
  VideoCaptureBufferType GetBufferType() override {
    return VideoCaptureBufferType::kGpuMemoryBuffer;
  }
};

class FakeVideoCaptureBufferTrackerFactory
    : public VideoCaptureBufferTrackerFactory {
 public:
  std::unique_ptr<VideoCaptureBufferTracker> CreateTracker(
      VideoCaptureBufferType buffer_type) override {
    return std::make_unique<FakeVideoCaptureBufferTracker>();
  }
  std::unique_ptr<VideoCaptureBufferTracker>
  CreateTrackerForExternalGpuMemoryBuffer(
      gfx::GpuMemoryBufferHandle handle) override {
    return std::make_unique<FakeVideoCaptureBufferTracker>();
  }
};

}  // namespace

// Test fixture for testing a unit consisting of an instance of
// VideoCaptureDeviceClient connected to a partly-mocked instance of
// VideoCaptureController, and an instance of VideoCaptureBufferPoolImpl
// as well as related threading glue that replicates how it is used in
// production.
class VideoCaptureDeviceClientTest : public ::testing::Test {
 public:
  void InitWithSharedMemoryBufferPool() {
    scoped_refptr<VideoCaptureBufferPoolImpl> buffer_pool(
        new VideoCaptureBufferPoolImpl(VideoCaptureBufferType::kSharedMemory,
                                       2));
    Init(std::move(buffer_pool));
  }

  void InitWithGmbBufferPool() {
    scoped_refptr<VideoCaptureBufferPoolImpl> buffer_pool(
        new VideoCaptureBufferPoolImpl(
            VideoCaptureBufferType::kSharedMemory, 2,
            std::make_unique<FakeVideoCaptureBufferTrackerFactory>()));
    Init(std::move(buffer_pool));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<unittest_internal::MockGpuMemoryBufferManager>
      gpu_memory_buffer_manager_;
  // Will be nullopt until `Init()` has been called:
  std::optional<video_effects::FakeVideoEffectsProcessor>
      fake_video_effects_processor_;
  FakeVideoEffectsManagerImpl fake_video_effects_manager_;

  mojo::Receiver<media::mojom::VideoEffectsManager>
      video_effects_manager_receiver_{&fake_video_effects_manager_};

  // Must outlive `receiver_`.
  std::unique_ptr<VideoCaptureDeviceClient> device_client_;
  raw_ptr<NiceMock<MockVideoFrameReceiver>> receiver_;

 private:
  void Init(scoped_refptr<VideoCaptureBufferPoolImpl> buffer_pool) {
    auto controller = std::make_unique<NiceMock<MockVideoFrameReceiver>>();
    receiver_ = controller.get();
    gpu_memory_buffer_manager_ =
        std::make_unique<unittest_internal::MockGpuMemoryBufferManager>();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    device_client_ = std::make_unique<VideoCaptureDeviceClient>(
        std::move(controller), buffer_pool,
        base::BindRepeating(&ReturnNullPtrAsJpecDecoder));
#else

    mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
        processor_receiver;
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
        processor_remote(processor_receiver.InitWithNewPipeAndPassRemote());

    mojo::PendingRemote<mojom::VideoEffectsManager> manager_remote =
        video_effects_manager_receiver_.BindNewPipeAndPassRemote();

    fake_video_effects_processor_.emplace(std::move(processor_receiver),
                                          std::move(manager_remote));

    device_client_ = std::make_unique<VideoCaptureDeviceClient>(
        std::move(controller), buffer_pool,
        media::VideoEffectsContext(std::move(processor_remote)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
};

// A small test for reference and to verify VideoCaptureDeviceClient is
// minimally functional.
TEST_F(VideoCaptureDeviceClientTest, Minimal) {
  InitWithSharedMemoryBufferPool();
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
    EXPECT_CALL(*receiver_,
                MockOnFrameReadyInBuffer(
                    Field(&ReadyFrameInBuffer::buffer_id, expected_buffer_id)));
  }
  device_client_->VideoCaptureDevice::Client::OnIncomingCapturedData(
      data, kScratchpadSizeInBytes, kFrameFormat, kColorSpace,
      0 /* clockwise rotation */, false /* flip_y */, base::TimeTicks(),
      base::TimeDelta(), std::nullopt);

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
    EXPECT_CALL(*receiver_,
                MockOnFrameReadyInBuffer(
                    Field(&ReadyFrameInBuffer::buffer_id, expected_buffer_id)));
    EXPECT_CALL(*receiver_, OnBufferRetired(expected_buffer_id));
  }
  device_client_->VideoCaptureDevice::Client::OnIncomingCapturedGfxBuffer(
      buffer.get(), kFrameFormatNV12, 0 /*clockwise rotation*/,
      base::TimeTicks(), base::TimeDelta(), std::nullopt);

  // Releasing |device_client_| will also release |receiver_|.
  receiver_ = nullptr;  // Avoid dangling reference.
  device_client_.reset();
}

TEST_F(VideoCaptureDeviceClientTest,
       ProgressesCaptureBeginTimestampsForOnIncomingCapturedData) {
  InitWithSharedMemoryBufferPool();
  auto expected_timestamp = base::TimeTicks() + base::Seconds(66);
  EXPECT_CALL(
      *receiver_,
      MockOnFrameReadyInBuffer(Field(
          &ReadyFrameInBuffer::frame_info,
          Pointee(Field(&mojom::VideoFrameInfo::metadata,
                        Field(&media::VideoFrameMetadata::capture_begin_time,
                              Optional(expected_timestamp)))))));
  constexpr size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  device_client_->VideoCaptureDevice::Client::OnIncomingCapturedData(
      data, kScratchpadSizeInBytes,
      VideoCaptureFormat(gfx::Size(10, 10), 30.0f, PIXEL_FORMAT_I420),
      gfx::ColorSpace::CreateREC601(), 0, false, base::TimeTicks(),
      base::TimeDelta(), expected_timestamp);
}

TEST_F(VideoCaptureDeviceClientTest,
       ProgressesCaptureBeginTimestampsForOnIncomingCapturedGfxBuffer) {
  InitWithSharedMemoryBufferPool();
  auto expected_timestamp = base::TimeTicks() + base::Seconds(77);
  EXPECT_CALL(
      *receiver_,
      MockOnFrameReadyInBuffer(Field(
          &ReadyFrameInBuffer::frame_info,
          Pointee(Field(&mojom::VideoFrameInfo::metadata,
                        Field(&media::VideoFrameMetadata::capture_begin_time,
                              Optional(expected_timestamp)))))));
  auto resolution = gfx::Size(32, 32);
  auto buffer = gpu_memory_buffer_manager_->CreateFakeGpuMemoryBuffer(
      resolution, gfx::BufferFormat::YUV_420_BIPLANAR,
      gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE, gpu::kNullSurfaceHandle,
      nullptr);
  device_client_->VideoCaptureDevice::Client::OnIncomingCapturedGfxBuffer(
      buffer.get(), VideoCaptureFormat(resolution, 30.0f, PIXEL_FORMAT_NV12), 0,
      base::TimeTicks(), base::TimeDelta(), expected_timestamp);
}

TEST_F(VideoCaptureDeviceClientTest,
       ProgressesCaptureBeginTimestampsForOnIncomingCapturedExternalBuffer) {
  InitWithGmbBufferPool();
  auto expected_timestamp = base::TimeTicks() + base::Seconds(88);
  EXPECT_CALL(
      *receiver_,
      MockOnFrameReadyInBuffer(Field(
          &ReadyFrameInBuffer::frame_info,
          Pointee(Field(&mojom::VideoFrameInfo::metadata,
                        Field(&media::VideoFrameMetadata::capture_begin_time,
                              Optional(expected_timestamp)))))));
  auto resolution = gfx::Size(32, 32);
  device_client_->OnIncomingCapturedExternalBuffer(
      CapturedExternalVideoBuffer(
          gfx::GpuMemoryBufferHandle(),
          VideoCaptureFormat(resolution, 30, PIXEL_FORMAT_NV12),
          gfx::ColorSpace::CreateREC601()),
      base::TimeTicks(), base::TimeDelta(), expected_timestamp,
      gfx::Rect(resolution));
}

// Tests that we fail silently if no available buffers to use.
TEST_F(VideoCaptureDeviceClientTest, DropsFrameIfNoBuffer) {
  InitWithSharedMemoryBufferPool();
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
  EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer)
      .Times(2)
      .WillRepeatedly(Invoke([&read_permission](ReadyFrameInBuffer frame) {
        read_permission.push_back(std::move(frame.buffer_read_permission));
      }));
  // Pass three frames. The third will be dropped.
  device_client_->VideoCaptureDevice::Client::OnIncomingCapturedData(
      data, kScratchpadSizeInBytes, kFrameFormat, kColorSpace,
      0 /* clockwise rotation */, false /* flip_y */, base::TimeTicks(),
      base::TimeDelta(), std::nullopt);
  device_client_->VideoCaptureDevice::Client::OnIncomingCapturedData(
      data, kScratchpadSizeInBytes, kFrameFormat, kColorSpace,
      0 /* clockwise rotation */, false /* flip_y */, base::TimeTicks(),
      base::TimeDelta(), std::nullopt);
  device_client_->VideoCaptureDevice::Client::OnIncomingCapturedData(
      data, kScratchpadSizeInBytes, kFrameFormat, kColorSpace,
      0 /* clockwise rotation */, false /* flip_y */, base::TimeTicks(),
      base::TimeDelta(), std::nullopt);
  Mock::VerifyAndClearExpectations(receiver_);
}

// Tests that buffer-based capture API accepts some memory-backed pixel formats.
TEST_F(VideoCaptureDeviceClientTest, DataCaptureGoodPixelFormats) {
  InitWithSharedMemoryBufferPool();
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
    EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer).Times(1);
    device_client_->VideoCaptureDevice::Client::OnIncomingCapturedData(
        data,
        media::VideoFrame::AllocationSize(params.requested_format.pixel_format,
                                          params.requested_format.frame_size),
        params.requested_format, kColorSpace, 0 /* clockwise_rotation */,
        false /* flip_y */, base::TimeTicks(), base::TimeDelta(), std::nullopt);
    Mock::VerifyAndClearExpectations(receiver_);
  }
}

// Test that we receive the expected resolution for a given captured frame
// resolution and rotation. Odd resolutions are also cropped.
TEST_F(VideoCaptureDeviceClientTest, CheckRotationsAndCrops) {
  InitWithSharedMemoryBufferPool();
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
    EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer)
        .Times(1)
        .WillOnce(Invoke([&coded_size](ReadyFrameInBuffer frame) {
          coded_size = frame.frame_info->coded_size;
        }));
    device_client_->VideoCaptureDevice::Client::OnIncomingCapturedData(
        data,
        media::VideoFrame::AllocationSize(params.requested_format.pixel_format,
                                          params.requested_format.frame_size),
        params.requested_format, gfx::ColorSpace(), size_and_rotation.rotation,
        false /* flip_y */, base::TimeTicks(), base::TimeDelta(), std::nullopt);

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
    EXPECT_CALL(*receiver_, MockOnFrameReadyInBuffer)
        .Times(1)
        .WillOnce(Invoke([&coded_size](ReadyFrameInBuffer frame) {
          coded_size = frame.frame_info->coded_size;
        }));
    device_client_->VideoCaptureDevice::Client::OnIncomingCapturedGfxBuffer(
        buffer.get(), params.requested_format, size_and_rotation.rotation,
        base::TimeTicks(), base::TimeDelta(), std::nullopt);

    EXPECT_EQ(coded_size.width(), size_and_rotation.output_resolution.width());
    EXPECT_EQ(coded_size.height(),
              size_and_rotation.output_resolution.height());

    Mock::VerifyAndClearExpectations(receiver_);
  }
}

#if !BUILDFLAG(IS_CHROMEOS)
// Tests that the VideoEffectsManager remote is closed on the correct task
// runner. Destruction on the wrong task runner will cause a crash.
TEST_F(VideoCaptureDeviceClientTest, DestructionClosesVideoEffectsManager) {
  InitWithSharedMemoryBufferPool();
  base::RunLoop run_loop;
  video_effects_manager_receiver_.set_disconnect_handler(
      run_loop.QuitClosure());
  receiver_ = nullptr;
  EXPECT_NO_FATAL_FAILURE(device_client_.reset());
  run_loop.Run();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
}  // namespace media
