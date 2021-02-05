// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fuchsia/video_capture_device_fuchsia.h"

#include "base/fuchsia/test_component_context_for_process.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/capture/video/fuchsia/video_capture_device_factory_fuchsia.h"
#include "media/fuchsia/camera/fake_fuchsia_camera.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

struct ReceivedFrame {
  VideoCaptureDevice::Client::Buffer buffer;
  VideoCaptureFormat format;
  gfx::ColorSpace color_space;
  base::TimeTicks reference_time;
  base::TimeDelta timestamp;
  gfx::Rect visible_rect;
};

void ValidateReceivedFrame(const ReceivedFrame& frame,
                           gfx::Size expected_size,
                           uint8_t salt) {
  gfx::Size coded_size((expected_size.width() + 1) & ~1,
                       (expected_size.height() + 1) & ~1);
  ASSERT_EQ(frame.format.frame_size, coded_size);
  EXPECT_EQ(frame.format.pixel_format, PIXEL_FORMAT_I420);
  EXPECT_GT(frame.format.frame_rate, 0.0);
  EXPECT_EQ(frame.visible_rect, gfx::Rect(expected_size));
  EXPECT_EQ(frame.color_space, gfx::ColorSpace());

  auto handle = frame.buffer.handle_provider->GetHandleForInProcessAccess();

  FakeCameraStream::ValidateFrameData(handle->data(), coded_size, salt);
}

// VideoCaptureBufferHandle implementation that references memory allocated on
// the heap.
class HeapBufferHandle : public VideoCaptureBufferHandle {
 public:
  HeapBufferHandle(size_t size, uint8_t* data) : size_(size), data_(data) {}

  size_t mapped_size() const final { return size_; }
  uint8_t* data() const final { return data_; }
  const uint8_t* const_data() const final { return data_; }

 private:
  const size_t size_;
  uint8_t* const data_;
};

// VideoCaptureDevice::Client::Buffer::HandleProvider implementation that
// allocates memory on the heap.
class HeapBufferHandleProvider
    : public VideoCaptureDevice::Client::Buffer::HandleProvider {
 public:
  HeapBufferHandleProvider(size_t size) : data_(size) {}
  ~HeapBufferHandleProvider() final = default;

  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() final {
    NOTREACHED();
    return {};
  }

  mojo::ScopedSharedBufferHandle DuplicateAsMojoBuffer() final {
    NOTREACHED();
    return {};
  }

  std::unique_ptr<VideoCaptureBufferHandle> GetHandleForInProcessAccess()
      override {
    return std::make_unique<HeapBufferHandle>(data_.size(), data_.data());
  }

  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override {
    return gfx::GpuMemoryBufferHandle();
  }

 private:
  std::vector<uint8_t> data_;
};

class TestVideoCaptureClient : public VideoCaptureDevice::Client {
 public:
  ~TestVideoCaptureClient() final = default;

  void WaitFrame() {
    EXPECT_FALSE(wait_frame_run_loop_);

    wait_frame_run_loop_.emplace();
    wait_frame_run_loop_->Run();
    wait_frame_run_loop_.reset();
  }

  const std::vector<ReceivedFrame>& received_frames() {
    return received_frames_;
  }

 private:
  // VideoCaptureDevice::Client implementation.
  void OnStarted() final {
    EXPECT_FALSE(started_);
    started_ = true;
  }

  ReserveResult ReserveOutputBuffer(const gfx::Size& dimensions,
                                    VideoPixelFormat format,
                                    int frame_feedback_id,
                                    Buffer* buffer) final {
    EXPECT_TRUE(started_);
    EXPECT_EQ(format, PIXEL_FORMAT_I420);
    EXPECT_EQ(dimensions.width() % 2, 0);
    EXPECT_EQ(dimensions.height() % 2, 0);

    size_t size = dimensions.width() * dimensions.height() * 3 / 2;
    buffer->handle_provider = std::make_unique<HeapBufferHandleProvider>(size);
    return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
  }

  void OnIncomingCapturedBufferExt(
      Buffer buffer,
      const VideoCaptureFormat& format,
      const gfx::ColorSpace& color_space,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      gfx::Rect visible_rect,
      const VideoFrameMetadata& additional_metadata) final {
    EXPECT_TRUE(started_);

    received_frames_.push_back(ReceivedFrame{std::move(buffer), format,
                                             color_space, reference_time,
                                             timestamp, visible_rect});

    if (wait_frame_run_loop_)
      wait_frame_run_loop_->Quit();
  }

  void OnIncomingCapturedData(const uint8_t* data,
                              int length,
                              const VideoCaptureFormat& frame_format,
                              const gfx::ColorSpace& color_space,
                              int clockwise_rotation,
                              bool flip_y,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp,
                              int frame_feedback_id) final {
    NOTREACHED();
  }
  void OnIncomingCapturedGfxBuffer(gfx::GpuMemoryBuffer* buffer,
                                   const VideoCaptureFormat& frame_format,
                                   int clockwise_rotation,
                                   base::TimeTicks reference_time,
                                   base::TimeDelta timestamp,
                                   int frame_feedback_id) final {
    NOTREACHED();
  }
  void OnIncomingCapturedExternalBuffer(
      CapturedExternalVideoBuffer buffer,
      std::vector<CapturedExternalVideoBuffer> scaled_buffers,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp) override {
    NOTREACHED();
  }
  void OnIncomingCapturedBuffer(Buffer buffer,
                                const VideoCaptureFormat& format,
                                base::TimeTicks reference_time,
                                base::TimeDelta timestamp) final {
    NOTREACHED();
  }
  void OnError(VideoCaptureError error,
               const base::Location& from_here,
               const std::string& reason) final {
    NOTREACHED();
  }
  void OnFrameDropped(VideoCaptureFrameDropReason reason) final {
    NOTREACHED();
  }
  void OnLog(const std::string& message) final { NOTREACHED(); }
  double GetBufferPoolUtilization() const final {
    NOTREACHED();
    return 0;
  }

  bool started_ = false;
  std::vector<ReceivedFrame> received_frames_;
  base::Optional<base::RunLoop> wait_frame_run_loop_;
};

}  // namespace

class VideoCaptureDeviceFuchsiaTest : public testing::Test {
 public:
  VideoCaptureDeviceFuchsiaTest() {
    test_context_.AddService("fuchsia.sysmem.Allocator");
  }

  ~VideoCaptureDeviceFuchsiaTest() override {
    if (device_)
      device_->StopAndDeAllocate();
  }

  std::vector<VideoCaptureDeviceInfo> GetDevicesInfo() {
    std::vector<VideoCaptureDeviceInfo> devices_info;
    base::RunLoop run_loop;
    device_factory_.GetDevicesInfo(base::BindLambdaForTesting(
        [&devices_info, &run_loop](std::vector<VideoCaptureDeviceInfo> result) {
          devices_info = std::move(result);
          run_loop.Quit();
        }));
    run_loop.Run();
    return devices_info;
  }

  void CreateDevice() {
    auto devices_info = GetDevicesInfo();
    ASSERT_EQ(devices_info.size(), 1U);
    device_ = device_factory_.CreateDevice(devices_info[0].descriptor);
  }

  void StartCapturer() {
    if (!device_)
      CreateDevice();

    VideoCaptureParams params;
    params.requested_format.frame_size = FakeCameraStream::kDefaultFrameSize;
    params.requested_format.frame_rate = 30.0;
    params.requested_format.pixel_format = PIXEL_FORMAT_I420;

    auto client = std::make_unique<TestVideoCaptureClient>();
    client_ = client.get();
    device_->AllocateAndStart(params, std::move(client));

    EXPECT_TRUE(fake_device_watcher_.stream()->WaitBuffersAllocated());
  }

  void ProduceAndCaptureFrame() {
    const uint8_t kFrameSalt = 1;

    auto frame_timestamp = base::TimeTicks::Now();
    fake_device_watcher_.stream()->ProduceFrame(frame_timestamp, kFrameSalt);
    client_->WaitFrame();

    ASSERT_EQ(client_->received_frames().size(), 1U);
    EXPECT_EQ(client_->received_frames()[0].reference_time, frame_timestamp);
    ValidateReceivedFrame(client_->received_frames()[0],
                          FakeCameraStream::kDefaultFrameSize, kFrameSalt);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::TestComponentContextForProcess test_context_;

  FakeCameraDeviceWatcher fake_device_watcher_{
      test_context_.additional_services()};

  VideoCaptureDeviceFactoryFuchsia device_factory_;
  std::unique_ptr<VideoCaptureDevice> device_;
  TestVideoCaptureClient* client_ = nullptr;
};

TEST_F(VideoCaptureDeviceFuchsiaTest, Initialize) {
  StartCapturer();
}

TEST_F(VideoCaptureDeviceFuchsiaTest, SendFrame) {
  StartCapturer();
  ProduceAndCaptureFrame();
}

// Verifies that VideoCaptureDevice can recover from failed Sync() on the sysmem
// buffer collection.
TEST_F(VideoCaptureDeviceFuchsiaTest, FailBufferCollectionSync) {
  fake_device_watcher_.stream()->SetFirstBufferCollectionFailMode(
      FakeCameraStream::SysmemFailMode::kFailSync);

  StartCapturer();
  ProduceAndCaptureFrame();
}

// Verifies that VideoCaptureDevice can recover from sysmem buffer allocation
// failures..
TEST_F(VideoCaptureDeviceFuchsiaTest, FailBufferCollectionAllocation) {
  fake_device_watcher_.stream()->SetFirstBufferCollectionFailMode(
      FakeCameraStream::SysmemFailMode::kFailAllocation);

  StartCapturer();
  ProduceAndCaptureFrame();
}

TEST_F(VideoCaptureDeviceFuchsiaTest, MultipleFrames) {
  StartCapturer();

  EXPECT_TRUE(fake_device_watcher_.stream()->WaitBuffersAllocated());

  auto start_timestamp = base::TimeTicks::Now();

  for (size_t i = 0; i < 10; ++i) {
    ASSERT_TRUE(fake_device_watcher_.stream()->WaitFreeBuffer());

    auto frame_timestamp =
        start_timestamp + base::TimeDelta::FromMilliseconds(i * 16);
    fake_device_watcher_.stream()->ProduceFrame(frame_timestamp, i);
    client_->WaitFrame();

    ASSERT_EQ(client_->received_frames().size(), i + 1);
    EXPECT_EQ(client_->received_frames()[i].reference_time, frame_timestamp);
    ValidateReceivedFrame(client_->received_frames()[i],
                          FakeCameraStream::kDefaultFrameSize, i);
  }
}

TEST_F(VideoCaptureDeviceFuchsiaTest, FrameRotation) {
  const gfx::Size kResolution(4, 2);
  fake_device_watcher_.stream()->SetFakeResolution(kResolution);

  StartCapturer();

  EXPECT_TRUE(fake_device_watcher_.stream()->WaitBuffersAllocated());

  for (int i = static_cast<int>(fuchsia::camera3::Orientation::UP);
       i <= static_cast<int>(fuchsia::camera3::Orientation::RIGHT_FLIPPED);
       ++i) {
    SCOPED_TRACE(testing::Message() << "Orientation " << i);

    auto orientation = static_cast<fuchsia::camera3::Orientation>(i);

    ASSERT_TRUE(fake_device_watcher_.stream()->WaitFreeBuffer());
    fake_device_watcher_.stream()->SetFakeOrientation(orientation);
    fake_device_watcher_.stream()->ProduceFrame(base::TimeTicks::Now(), i);
    client_->WaitFrame();

    gfx::Size expected_size = kResolution;
    if (orientation == fuchsia::camera3::Orientation::LEFT ||
        orientation == fuchsia::camera3::Orientation::LEFT_FLIPPED ||
        orientation == fuchsia::camera3::Orientation::RIGHT ||
        orientation == fuchsia::camera3::Orientation::RIGHT_FLIPPED) {
      expected_size = gfx::Size(expected_size.height(), expected_size.width());
    }
    ValidateReceivedFrame(client_->received_frames().back(), expected_size, i);
  }
}

TEST_F(VideoCaptureDeviceFuchsiaTest, FrameDimensionsNotDivisibleBy2) {
  const gfx::Size kOddResolution(21, 7);
  fake_device_watcher_.stream()->SetFakeResolution(kOddResolution);

  StartCapturer();

  fake_device_watcher_.stream()->ProduceFrame(base::TimeTicks::Now(), 1);
  client_->WaitFrame();

  ASSERT_EQ(client_->received_frames().size(), 1U);
  ValidateReceivedFrame(client_->received_frames()[0], kOddResolution, 1);
}

TEST_F(VideoCaptureDeviceFuchsiaTest, MidStreamResolutionChange) {
  StartCapturer();

  // Capture the first frame at the default resolution.
  fake_device_watcher_.stream()->ProduceFrame(base::TimeTicks::Now(), 1);
  client_->WaitFrame();
  ASSERT_TRUE(fake_device_watcher_.stream()->WaitFreeBuffer());

  // Update resolution and produce another frames.
  const gfx::Size kUpdatedResolution(3, 14);
  fake_device_watcher_.stream()->SetFakeResolution(kUpdatedResolution);
  fake_device_watcher_.stream()->ProduceFrame(base::TimeTicks::Now(), 1);
  client_->WaitFrame();

  // Verify that we get captured frames with correct resolution.
  ASSERT_EQ(client_->received_frames().size(), 2U);
  ValidateReceivedFrame(client_->received_frames()[0],
                        FakeCameraStream::kDefaultFrameSize, 1);
  ValidateReceivedFrame(client_->received_frames()[1], kUpdatedResolution, 1);
}

TEST_F(VideoCaptureDeviceFuchsiaTest,
       CreateDeviceAfterDeviceWatcherDisconnect) {
  auto devices_info = GetDevicesInfo();
  ASSERT_EQ(devices_info.size(), 1U);

  // Disconnect DeviceWatcher and run the run loop so |device_factory_| can
  // handle the disconnect.
  fake_device_watcher_.DisconnectClients();
  base::RunLoop().RunUntilIdle();

  // The factory is expected to reconnect DeviceWatcher.
  device_ = device_factory_.CreateDevice(devices_info[0].descriptor);

  StartCapturer();

  fake_device_watcher_.stream()->ProduceFrame(base::TimeTicks::Now(), 1);
  client_->WaitFrame();

  ASSERT_EQ(client_->received_frames().size(), 1U);
}

}  // namespace media
