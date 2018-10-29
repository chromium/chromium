// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_device_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_hal_delegate.h"
#include "media/capture/video/chromeos/mock_camera_module.h"
#include "media/capture/video/chromeos/mock_video_capture_client.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#include "media/capture/video/mock_gpu_memory_buffer_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::A;
using testing::AtLeast;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace media {

namespace {

class MockCameraDevice : public cros::mojom::Camera3DeviceOps {
 public:
  MockCameraDevice() = default;

  ~MockCameraDevice() = default;

  void Initialize(cros::mojom::Camera3CallbackOpsPtr callback_ops,
                  InitializeCallback callback) override {
    DoInitialize(callback_ops, callback);
  }
  MOCK_METHOD2(DoInitialize,
               void(cros::mojom::Camera3CallbackOpsPtr& callback_ops,
                    InitializeCallback& callback));

  void ConfigureStreams(cros::mojom::Camera3StreamConfigurationPtr config,
                        ConfigureStreamsCallback callback) override {
    DoConfigureStreams(config, callback);
  }
  MOCK_METHOD2(DoConfigureStreams,
               void(cros::mojom::Camera3StreamConfigurationPtr& config,
                    ConfigureStreamsCallback& callback));

  void ConstructDefaultRequestSettings(
      cros::mojom::Camera3RequestTemplate type,
      ConstructDefaultRequestSettingsCallback callback) override {
    DoConstructDefaultRequestSettings(type, callback);
  }
  MOCK_METHOD2(DoConstructDefaultRequestSettings,
               void(cros::mojom::Camera3RequestTemplate type,
                    ConstructDefaultRequestSettingsCallback& callback));

  void ProcessCaptureRequest(cros::mojom::Camera3CaptureRequestPtr request,
                             ProcessCaptureRequestCallback callback) override {
    DoProcessCaptureRequest(request, callback);
  }
  MOCK_METHOD2(DoProcessCaptureRequest,
               void(cros::mojom::Camera3CaptureRequestPtr& request,
                    ProcessCaptureRequestCallback& callback));

  void Dump(mojo::ScopedHandle fd) override { DoDump(fd); }
  MOCK_METHOD1(DoDump, void(mojo::ScopedHandle& fd));

  void Flush(FlushCallback callback) override { DoFlush(callback); }
  MOCK_METHOD1(DoFlush, void(FlushCallback& callback));

  void RegisterBuffer(uint64_t buffer_id,
                      cros::mojom::Camera3DeviceOps::BufferType type,
                      std::vector<mojo::ScopedHandle> fds,
                      uint32_t drm_format,
                      cros::mojom::HalPixelFormat hal_pixel_format,
                      uint32_t width,
                      uint32_t height,
                      const std::vector<uint32_t>& strides,
                      const std::vector<uint32_t>& offsets,
                      RegisterBufferCallback callback) override {
    DoRegisterBuffer(buffer_id, type, fds, drm_format, hal_pixel_format, width,
                     height, strides, offsets, callback);
  }
  MOCK_METHOD10(DoRegisterBuffer,
                void(uint64_t buffer_id,
                     cros::mojom::Camera3DeviceOps::BufferType type,
                     std::vector<mojo::ScopedHandle>& fds,
                     uint32_t drm_format,
                     cros::mojom::HalPixelFormat hal_pixel_format,
                     uint32_t width,
                     uint32_t height,
                     const std::vector<uint32_t>& strides,
                     const std::vector<uint32_t>& offsets,
                     RegisterBufferCallback& callback));

  void Close(CloseCallback callback) override { DoClose(callback); }
  MOCK_METHOD1(DoClose, void(CloseCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCameraDevice);
};

constexpr int32_t kJpegMaxBufferSize = 1024;
constexpr size_t kDefaultWidth = 1280, kDefaultHeight = 720;
const VideoCaptureDeviceDescriptor kDefaultDescriptor("Fake device", "0");
const VideoCaptureFormat kDefaultCaptureFormat(gfx::Size(kDefaultWidth,
                                                         kDefaultHeight),
                                               30.0,
                                               PIXEL_FORMAT_I420);

}  // namespace

class CameraDeviceDelegateTest : public ::testing::Test {
 public:
  CameraDeviceDelegateTest()
      : mock_camera_device_binding_(&mock_camera_device_),
        device_delegate_thread_("DeviceDelegateThread"),
        hal_delegate_thread_("HalDelegateThread") {}

  void SetUp() override {
    VideoCaptureDeviceFactoryChromeOS::SetGpuBufferManager(
        &mock_gpu_memory_buffer_manager_);
    hal_delegate_thread_.Start();
    camera_hal_delegate_ =
        new CameraHalDelegate(hal_delegate_thread_.task_runner());
    camera_hal_delegate_->SetCameraModule(
        mock_camera_module_.GetInterfacePtrInfo());
  }

  void TearDown() override {
    camera_hal_delegate_->Reset();
    hal_delegate_thread_.Stop();
  }

  void AllocateDeviceWithDescriptor(VideoCaptureDeviceDescriptor descriptor) {
    ASSERT_FALSE(device_delegate_thread_.IsRunning());
    ASSERT_FALSE(camera_device_delegate_);
    device_delegate_thread_.Start();
    camera_device_delegate_ = std::make_unique<CameraDeviceDelegate>(
        descriptor, camera_hal_delegate_,
        device_delegate_thread_.task_runner());
    num_streams_ = 0;
  }

  void GetFakeCameraInfo(uint32_t camera_id,
                         cros::mojom::CameraModule::GetCameraInfoCallback& cb) {
    cros::mojom::CameraInfoPtr camera_info = cros::mojom::CameraInfo::New();
    cros::mojom::CameraMetadataPtr static_metadata =
        cros::mojom::CameraMetadata::New();

    static_metadata->entry_count = 3;
    static_metadata->entry_capacity = 3;
    static_metadata->entries =
        std::vector<cros::mojom::CameraMetadataEntryPtr>();

    cros::mojom::CameraMetadataEntryPtr entry =
        cros::mojom::CameraMetadataEntry::New();
    entry->index = 0;
    entry->tag = cros::mojom::CameraMetadataTag::
        ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS;
    entry->type = cros::mojom::EntryType::TYPE_INT32;
    entry->count = 12;
    std::vector<int32_t> stream_configurations(entry->count);
    stream_configurations[0] = static_cast<int32_t>(
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
    stream_configurations[1] = kDefaultWidth;
    stream_configurations[2] = kDefaultHeight;
    stream_configurations[3] = static_cast<int32_t>(
        cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT);
    stream_configurations[4] = static_cast<int32_t>(
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888);
    stream_configurations[5] = kDefaultWidth;
    stream_configurations[6] = kDefaultHeight;
    stream_configurations[7] = static_cast<int32_t>(
        cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT);
    stream_configurations[8] = static_cast<int32_t>(
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB);
    stream_configurations[9] = kDefaultWidth;
    stream_configurations[10] = kDefaultHeight;
    stream_configurations[11] = static_cast<int32_t>(
        cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT);
    uint8_t* as_int8 = reinterpret_cast<uint8_t*>(stream_configurations.data());
    entry->data.assign(as_int8, as_int8 + entry->count * sizeof(int32_t));
    static_metadata->entries->push_back(std::move(entry));

    entry = cros::mojom::CameraMetadataEntry::New();
    entry->index = 1;
    entry->tag = cros::mojom::CameraMetadataTag::ANDROID_SENSOR_ORIENTATION;
    entry->type = cros::mojom::EntryType::TYPE_INT32;
    entry->count = 1;
    entry->data = std::vector<uint8_t>(4, 0);
    static_metadata->entries->push_back(std::move(entry));

    entry = cros::mojom::CameraMetadataEntry::New();
    entry->index = 2;
    entry->tag = cros::mojom::CameraMetadataTag::ANDROID_JPEG_MAX_SIZE;
    entry->type = cros::mojom::EntryType::TYPE_INT32;
    entry->count = 1;
    int32_t jpeg_max_size = kJpegMaxBufferSize;
    as_int8 = reinterpret_cast<uint8_t*>(&jpeg_max_size);
    entry->data.assign(as_int8, as_int8 + entry->count * sizeof(int32_t));
    static_metadata->entries->push_back(std::move(entry));

    switch (camera_id) {
      case 0:
        camera_info->facing = cros::mojom::CameraFacing::CAMERA_FACING_FRONT;
        camera_info->orientation = 0;
        camera_info->static_camera_characteristics = std::move(static_metadata);
        break;
      default:
        FAIL() << "Invalid camera id";
    }
    std::move(cb).Run(0, std::move(camera_info));
  }

  void OpenMockCameraDevice(
      int32_t camera_id,
      cros::mojom::Camera3DeviceOpsRequest& device_ops_request,
      base::OnceCallback<void(int32_t)>& callback) {
    mock_camera_device_binding_.Bind(std::move(device_ops_request));
    std::move(callback).Run(0);
  }

  void InitializeMockCameraDevice(
      cros::mojom::Camera3CallbackOpsPtr& callback_ops,
      base::OnceCallback<void(int32_t)>& callback) {
    callback_ops_ = std::move(callback_ops);
    std::move(callback).Run(0);
  }

  void ConfigureFakeStreams(
      cros::mojom::Camera3StreamConfigurationPtr& config,
      base::OnceCallback<void(int32_t,
                              cros::mojom::Camera3StreamConfigurationPtr)>&
          callback) {
    ASSERT_GE(2u, config->streams.size());
    ASSERT_LT(0u, config->streams.size());
    for (size_t i = 0; i < config->streams.size(); ++i) {
      config->streams[i]->usage = 0;
      config->streams[i]->max_buffers = 1;
    }
    num_streams_ = config->streams.size();
    std::move(callback).Run(0, std::move(config));
  }

  void ConstructFakeRequestSettings(
      cros::mojom::Camera3RequestTemplate type,
      base::OnceCallback<void(cros::mojom::CameraMetadataPtr)>& callback) {
    cros::mojom::CameraMetadataPtr fake_settings =
        cros::mojom::CameraMetadata::New();
    fake_settings->entry_count = 1;
    fake_settings->entry_capacity = 1;
    fake_settings->entries = std::vector<cros::mojom::CameraMetadataEntryPtr>();
    std::move(callback).Run(std::move(fake_settings));
  }

  void RegisterBuffer(uint64_t buffer_id,
                      cros::mojom::Camera3DeviceOps::BufferType type,
                      std::vector<mojo::ScopedHandle>& fds,
                      uint32_t drm_format,
                      cros::mojom::HalPixelFormat hal_pixel_format,
                      uint32_t width,
                      uint32_t height,
                      const std::vector<uint32_t>& strides,
                      const std::vector<uint32_t>& offsets,
                      base::OnceCallback<void(int32_t)>& callback) {
    std::move(callback).Run(0);
  }

  void ProcessCaptureRequest(cros::mojom::Camera3CaptureRequestPtr& request,
                             base::OnceCallback<void(int32_t)>& callback) {
    std::move(callback).Run(0);

    cros::mojom::Camera3NotifyMsgPtr msg = cros::mojom::Camera3NotifyMsg::New();
    msg->type = cros::mojom::Camera3MsgType::CAMERA3_MSG_SHUTTER;
    msg->message = cros::mojom::Camera3NotifyMsgMessage::New();
    cros::mojom::Camera3ShutterMsgPtr shutter_msg =
        cros::mojom::Camera3ShutterMsg::New();
    shutter_msg->timestamp = base::TimeTicks::Now().ToInternalValue();
    msg->message->set_shutter(std::move(shutter_msg));
    callback_ops_->Notify(std::move(msg));

    cros::mojom::Camera3CaptureResultPtr result =
        cros::mojom::Camera3CaptureResult::New();
    result->frame_number = request->frame_number;
    result->result = cros::mojom::CameraMetadata::New();
    result->output_buffers = std::move(request->output_buffers);
    result->partial_result = 1;
    callback_ops_->ProcessCaptureResult(std::move(result));
  }

  void CloseMockCameraDevice(base::OnceCallback<void(int32_t)>& callback) {
    if (mock_camera_device_binding_.is_bound()) {
      mock_camera_device_binding_.Close();
    }
    callback_ops_.reset();
    std::move(callback).Run(0);
  }

  void SetUpExpectationUntilInitialized() {
    EXPECT_CALL(mock_camera_module_, DoGetCameraInfo(0, _))
        .Times(1)
        .WillOnce(Invoke(this, &CameraDeviceDelegateTest::GetFakeCameraInfo));
    EXPECT_CALL(mock_camera_module_, DoOpenDevice(0, _, _))
        .Times(1)
        .WillOnce(
            Invoke(this, &CameraDeviceDelegateTest::OpenMockCameraDevice));
    EXPECT_CALL(mock_camera_device_, DoInitialize(_, _))
        .Times(1)
        .WillOnce(Invoke(
            this, &CameraDeviceDelegateTest::InitializeMockCameraDevice));
  }

  void SetUpExpectationUntilStreamConfigured() {
    SetUpExpectationUntilInitialized();
    EXPECT_CALL(mock_camera_device_, DoConfigureStreams(_, _))
        .Times(1)
        .WillOnce(
            Invoke(this, &CameraDeviceDelegateTest::ConfigureFakeStreams));
    EXPECT_CALL(
        mock_gpu_memory_buffer_manager_,
        CreateGpuMemoryBuffer(_, gfx::BufferFormat::YUV_420_BIPLANAR,
                              gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
                              gpu::kNullSurfaceHandle))
        .Times(1)
        .WillOnce(Invoke(&unittest_internal::MockGpuMemoryBufferManager::
                             CreateFakeGpuMemoryBuffer));
    if (num_streams_ == 2) {
      EXPECT_CALL(
          mock_gpu_memory_buffer_manager_,
          CreateGpuMemoryBuffer(_, gfx::BufferFormat::R_8,
                                gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
                                gpu::kNullSurfaceHandle))
          .Times(1)
          .WillOnce(Invoke(&unittest_internal::MockGpuMemoryBufferManager::
                               CreateFakeGpuMemoryBuffer));
    }
    EXPECT_CALL(
        mock_gpu_memory_buffer_manager_,
        CreateGpuMemoryBuffer(gfx::Size(kDefaultWidth, kDefaultHeight),
                              gfx::BufferFormat::YUV_420_BIPLANAR,
                              gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
                              gpu::kNullSurfaceHandle))
        .Times(1)
        .WillOnce(Invoke(&unittest_internal::MockGpuMemoryBufferManager::
                             CreateFakeGpuMemoryBuffer));
    if (num_streams_ == 2) {
      EXPECT_CALL(mock_gpu_memory_buffer_manager_,
                  CreateGpuMemoryBuffer(
                      gfx::Size(kJpegMaxBufferSize, 1), gfx::BufferFormat::R_8,
                      gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
                      gpu::kNullSurfaceHandle))
          .Times(1)
          .WillOnce(Invoke(&unittest_internal::MockGpuMemoryBufferManager::
                               CreateFakeGpuMemoryBuffer));
    }
  }

  void SetUpExpectationUntilCapturing(
      unittest_internal::MockVideoCaptureClient* mock_client) {
    SetUpExpectationUntilStreamConfigured();
    EXPECT_CALL(mock_camera_device_, DoConstructDefaultRequestSettings(_, _))
        .Times(1)
        .WillOnce(Invoke(
            this, &CameraDeviceDelegateTest::ConstructFakeRequestSettings));
    EXPECT_CALL(*mock_client, OnStarted()).Times(1);
  }

  void SetUpExpectationForCaptureLoop() {
    EXPECT_CALL(mock_camera_device_,
                DoRegisterBuffer(_, _, _, _, _, _, _, _, _, _))
        .Times(AtLeast(1))
        .WillOnce(Invoke(this, &CameraDeviceDelegateTest::RegisterBuffer))
        .WillRepeatedly(
            Invoke(this, &CameraDeviceDelegateTest::RegisterBuffer));
    EXPECT_CALL(mock_camera_device_, DoProcessCaptureRequest(_, _))
        .Times(AtLeast(1))
        .WillOnce(
            Invoke(this, &CameraDeviceDelegateTest::ProcessCaptureRequest))
        .WillRepeatedly(
            Invoke(this, &CameraDeviceDelegateTest::ProcessCaptureRequest));
  }

  void SetUpExpectationForClose() {
    EXPECT_CALL(mock_camera_device_, DoClose(_))
        .Times(1)
        .WillOnce(
            Invoke(this, &CameraDeviceDelegateTest::CloseMockCameraDevice));
  }

  void WaitForDeviceToClose() {
    base::WaitableEvent device_closed(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    device_delegate_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&CameraDeviceDelegate::StopAndDeAllocate,
                                  camera_device_delegate_->GetWeakPtr(),
                                  base::BindOnce(
                                      [](base::WaitableEvent* device_closed) {
                                        device_closed->Signal();
                                      },
                                      base::Unretained(&device_closed))));
    base::TimeDelta kWaitTimeoutSecs = base::TimeDelta::FromSeconds(3);
    EXPECT_TRUE(device_closed.TimedWait(kWaitTimeoutSecs));
    EXPECT_EQ(CameraDeviceContext::State::kStopped, GetState());
  }

  unittest_internal::MockVideoCaptureClient* ResetDeviceContext() {
    auto mock_client =
        std::make_unique<unittest_internal::MockVideoCaptureClient>();
    auto* client_ptr = mock_client.get();
    device_context_ =
        std::make_unique<CameraDeviceContext>(std::move(mock_client));
    return client_ptr;
  }

  void ResetDevice() {
    ASSERT_TRUE(device_delegate_thread_.IsRunning());
    ASSERT_TRUE(camera_device_delegate_);
    device_delegate_thread_.Stop();
    camera_device_delegate_.reset();
    num_streams_ = 0;
  }

  void DoLoop() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void QuitRunLoop() {
    VLOG(2) << "quit!";
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  CameraDeviceContext::State GetState() {
    if (camera_device_delegate_->device_context_) {
      return camera_device_delegate_->device_context_->GetState();
    } else {
      // No device context means the VCD is either not started yet or already
      // stopped.
      return CameraDeviceContext::State::kStopped;
    }
  }

 protected:
  scoped_refptr<CameraHalDelegate> camera_hal_delegate_;
  std::unique_ptr<CameraDeviceDelegate> camera_device_delegate_;

  testing::StrictMock<unittest_internal::MockCameraModule> mock_camera_module_;
  unittest_internal::MockGpuMemoryBufferManager mock_gpu_memory_buffer_manager_;

  testing::StrictMock<MockCameraDevice> mock_camera_device_;
  mojo::Binding<cros::mojom::Camera3DeviceOps> mock_camera_device_binding_;
  cros::mojom::Camera3CallbackOpsPtr callback_ops_;

  base::Thread device_delegate_thread_;

  std::unique_ptr<CameraDeviceContext> device_context_;

  size_t num_streams_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::Thread hal_delegate_thread_;
  std::unique_ptr<base::RunLoop> run_loop_;
  DISALLOW_COPY_AND_ASSIGN(CameraDeviceDelegateTest);
};

// Test the complete capture flow: initialize, configure stream, capture one
// frame, and close the device.
TEST_F(CameraDeviceDelegateTest, AllocateCaptureAndStop) {
  AllocateDeviceWithDescriptor(kDefaultDescriptor);

  VideoCaptureParams params;
  params.requested_format = kDefaultCaptureFormat;

  auto* mock_client = ResetDeviceContext();
  mock_client->SetFrameCb(BindToCurrentLoop(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  mock_client->SetQuitCb(BindToCurrentLoop(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationUntilCapturing(mock_client);
  SetUpExpectationForCaptureLoop();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                                camera_device_delegate_->GetWeakPtr(), params,
                                base::Unretained(device_context_.get())));

  // Wait until a frame is received.  MockVideoCaptureClient calls QuitRunLoop()
  // to stop the run loop.
  DoLoop();
  EXPECT_EQ(CameraDeviceContext::State::kCapturing, GetState());

  SetUpExpectationForClose();

  WaitForDeviceToClose();

  ResetDevice();
}

// Test that the camera device delegate closes properly when StopAndDeAllocate
// is called right after the device is initialized.
TEST_F(CameraDeviceDelegateTest, StopAfterInitialized) {
  AllocateDeviceWithDescriptor(kDefaultDescriptor);

  VideoCaptureParams params;
  params.requested_format = kDefaultCaptureFormat;

  auto* mock_client = ResetDeviceContext();
  mock_client->SetQuitCb(BindToCurrentLoop(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationUntilInitialized();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                                camera_device_delegate_->GetWeakPtr(), params,
                                base::Unretained(device_context_.get())));

  EXPECT_CALL(mock_camera_device_, DoConfigureStreams(_, _))
      .Times(1)
      .WillOnce(Invoke(
          [this](cros::mojom::Camera3StreamConfigurationPtr& config,
                 base::OnceCallback<void(
                     int32_t, cros::mojom::Camera3StreamConfigurationPtr)>&
                     callback) {
            EXPECT_EQ(CameraDeviceContext::State::kInitialized,
                      this->GetState());
            this->QuitRunLoop();
            std::move(callback).Run(
                0, cros::mojom::Camera3StreamConfiguration::New());
          }));

  // Wait until the QuitRunLoop call in |mock_camera_device_->ConfigureStreams|.
  DoLoop();

  SetUpExpectationForClose();

  WaitForDeviceToClose();

  ResetDevice();
}

// Test that the camera device delegate closes properly when StopAndDeAllocate
// is called right after the stream is configured.
TEST_F(CameraDeviceDelegateTest, StopAfterStreamConfigured) {
  AllocateDeviceWithDescriptor(kDefaultDescriptor);

  VideoCaptureParams params;
  params.requested_format = kDefaultCaptureFormat;

  auto* mock_client = ResetDeviceContext();
  mock_client->SetQuitCb(BindToCurrentLoop(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationUntilStreamConfigured();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                                camera_device_delegate_->GetWeakPtr(), params,
                                base::Unretained(device_context_.get())));

  EXPECT_CALL(mock_camera_device_, DoConstructDefaultRequestSettings(_, _))
      .Times(1)
      .WillOnce(Invoke(
          [this](cros::mojom::Camera3RequestTemplate type,
                 base::OnceCallback<void(cros::mojom::CameraMetadataPtr)>&
                     callback) {
            EXPECT_EQ(CameraDeviceContext::State::kStreamConfigured,
                      this->GetState());
            this->QuitRunLoop();
            std::move(callback).Run(cros::mojom::CameraMetadataPtr());
          }));

  // Wait until the QuitRunLoop call in |mock_camera_device_->ConfigureStreams|.
  DoLoop();

  SetUpExpectationForClose();

  WaitForDeviceToClose();

  ResetDevice();
}

// Test that the camera device delegate handles camera device open failures
// correctly.
TEST_F(CameraDeviceDelegateTest, FailToOpenDevice) {
  AllocateDeviceWithDescriptor(kDefaultDescriptor);

  VideoCaptureParams params;
  params.requested_format = kDefaultCaptureFormat;

  auto* mock_client = ResetDeviceContext();

  auto stop_on_error = [&]() {
    device_delegate_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&CameraDeviceDelegate::StopAndDeAllocate,
                                  camera_device_delegate_->GetWeakPtr(),
                                  BindToCurrentLoop(base::BindOnce(
                                      &CameraDeviceDelegateTest::QuitRunLoop,
                                      base::Unretained(this)))));
  };
  EXPECT_CALL(*mock_client, OnError(_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(InvokeWithoutArgs(stop_on_error));

  EXPECT_CALL(mock_camera_module_, DoGetCameraInfo(0, _))
      .Times(1)
      .WillOnce(Invoke(this, &CameraDeviceDelegateTest::GetFakeCameraInfo));

  auto open_device_with_error_cb =
      [](int32_t camera_id,
         cros::mojom::Camera3DeviceOpsRequest& device_ops_request,
         base::OnceCallback<void(int32_t)>& callback) {
        std::move(callback).Run(-ENODEV);
        device_ops_request.ResetWithReason(-ENODEV,
                                           "Failed to open camera device");
      };
  EXPECT_CALL(mock_camera_module_, DoOpenDevice(0, _, _))
      .Times(1)
      .WillOnce(Invoke(open_device_with_error_cb));

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                                camera_device_delegate_->GetWeakPtr(), params,
                                base::Unretained(device_context_.get())));

  // Wait unitl |camera_device_delegate_->StopAndDeAllocate| calls the
  // QuitRunLoop callback.
  DoLoop();

  ResetDevice();
}

// Test that the class handles it correctly when StopAndDeAllocate is called
// multiple times.
TEST_F(CameraDeviceDelegateTest, DoubleStopAndDeAllocate) {
  AllocateDeviceWithDescriptor(kDefaultDescriptor);

  VideoCaptureParams params;
  params.requested_format = kDefaultCaptureFormat;

  auto* mock_client = ResetDeviceContext();
  mock_client->SetFrameCb(BindToCurrentLoop(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  mock_client->SetQuitCb(BindToCurrentLoop(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationUntilCapturing(mock_client);
  SetUpExpectationForCaptureLoop();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                                camera_device_delegate_->GetWeakPtr(), params,
                                base::Unretained(device_context_.get())));

  // Wait until a frame is received.  MockVideoCaptureClient calls QuitRunLoop()
  // to stop the run loop.
  DoLoop();

  EXPECT_EQ(CameraDeviceContext::State::kCapturing, GetState());

  SetUpExpectationForClose();

  WaitForDeviceToClose();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::StopAndDeAllocate,
                                camera_device_delegate_->GetWeakPtr(),
                                BindToCurrentLoop(base::BindOnce(
                                    &CameraDeviceDelegateTest::QuitRunLoop,
                                    base::Unretained(this)))));
  DoLoop();

  ResetDevice();
}

}  // namespace media
