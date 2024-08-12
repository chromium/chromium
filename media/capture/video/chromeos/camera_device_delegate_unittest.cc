// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/chromeos/camera_device_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/viz/test/test_context_provider.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_hal_delegate.h"
#include "media/capture/video/chromeos/mock_camera_module.h"
#include "media/capture/video/chromeos/mock_vendor_tag_ops.h"
#include "media/capture/video/chromeos/mock_video_capture_client.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#include "media/capture/video/mock_gpu_memory_buffer_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
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

class MockCameraDevice : public cros::mojom::Camera3DeviceOps {
 public:
  MockCameraDevice() = default;

  MockCameraDevice(const MockCameraDevice&) = delete;
  MockCameraDevice& operator=(const MockCameraDevice&) = delete;

  ~MockCameraDevice() override = default;

  void Initialize(
      mojo::PendingRemote<cros::mojom::Camera3CallbackOps> callback_ops,
      InitializeCallback callback) override {
    DoInitialize(std::move(callback_ops), callback);
  }
  MOCK_METHOD2(
      DoInitialize,
      void(mojo::PendingRemote<cros::mojom::Camera3CallbackOps> callback_ops,
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
                      RegisterBufferCallback callback) override {}

  void Close(CloseCallback callback) override { DoClose(callback); }
  MOCK_METHOD1(DoClose, void(CloseCallback& callback));

  void ConfigureStreamsAndGetAllocatedBuffers(
      cros::mojom::Camera3StreamConfigurationPtr config,
      ConfigureStreamsAndGetAllocatedBuffersCallback callback) override {
    DoConfigureStreamsAndGetAllocatedBuffers(config, callback);
  }
  MOCK_METHOD2(DoConfigureStreamsAndGetAllocatedBuffers,
               void(cros::mojom::Camera3StreamConfigurationPtr& config,
                    ConfigureStreamsAndGetAllocatedBuffersCallback& callback));

  void SignalStreamFlush(const std::vector<uint64_t>& stream_ids) override {
    DoSignalStreamFlush(stream_ids);
  }
  MOCK_METHOD1(DoSignalStreamFlush, void(std::vector<uint64_t> stream_ids));

  void OnNewBuffer(cros::mojom::CameraBufferHandlePtr buffer,
                   OnNewBufferCallback callback) override {
    DoOnNewBuffer(std::move(buffer), std::move(callback));
  }
  MOCK_METHOD2(DoOnNewBuffer,
               void(cros::mojom::CameraBufferHandlePtr buffer,
                    OnNewBufferCallback callback));

  void OnBufferRetired(uint64_t buffer_id) override {
    DoOnBufferRetired(buffer_id);
  }
  MOCK_METHOD1(DoOnBufferRetired, void(uint64_t buffer_id));
};

constexpr int32_t kJpegMaxBufferSize = 1024;
constexpr size_t kDefaultWidth = 1280, kDefaultHeight = 720;
constexpr int32_t kDefaultMinFrameRate = 1, kDefaultMaxFrameRate = 30;

base::flat_map<ClientType, VideoCaptureParams> GetDefaultCaptureParams() {
  VideoCaptureParams params;
  base::flat_map<ClientType, VideoCaptureParams> capture_params;
  params.requested_format = {gfx::Size(kDefaultWidth, kDefaultHeight),
                             float{kDefaultMaxFrameRate}, PIXEL_FORMAT_I420};
  capture_params[ClientType::kPreviewClient] = params;
  return capture_params;
}

}  // namespace

class CameraDeviceDelegateTest : public ::testing::Test {
 public:
  CameraDeviceDelegateTest()
      : mock_camera_device_receiver_(&mock_camera_device_),
        device_delegate_thread_("DeviceDelegateThread"),
        ui_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

  CameraDeviceDelegateTest(const CameraDeviceDelegateTest&) = delete;
  CameraDeviceDelegateTest& operator=(const CameraDeviceDelegateTest&) = delete;

  void SetUp() override {
    VideoCaptureDeviceFactoryChromeOS::SetGpuBufferManager(
        &mock_gpu_memory_buffer_manager_);
    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    test_sii_->UseTestGMBInSharedImageCreationWithBufferUsage();
    VideoCaptureDeviceFactoryChromeOS::SetSharedImageInterface(test_sii_);
    camera_hal_delegate_ = std::make_unique<CameraHalDelegate>(ui_task_runner_);
    if (!camera_hal_delegate_->Init()) {
      LOG(ERROR) << "Failed to initialize CameraHalDelegate";
      camera_hal_delegate_.reset();
      return;
    }
    auto get_camera_info =
        base::BindRepeating(&CameraHalDelegate::GetCameraInfoFromDeviceId,
                            base::Unretained(camera_hal_delegate_.get()));
    camera_hal_delegate_->SetCameraModule(
        mock_camera_module_.GetPendingRemote());
  }

  void TearDown() override {
    VideoCaptureDeviceFactoryChromeOS::SetSharedImageInterface(nullptr);
    camera_device_delegate_.reset();
    camera_hal_delegate_.reset();
    task_environment_.RunUntilIdle();
  }

  void AllocateDevice() {
    ASSERT_FALSE(device_delegate_thread_.IsRunning());
    ASSERT_FALSE(camera_device_delegate_);

    std::vector<VideoCaptureDeviceInfo> devices_info;
    base::RunLoop run_loop;
    camera_hal_delegate_->GetDevicesInfo(base::BindLambdaForTesting(
        [&devices_info, &run_loop](std::vector<VideoCaptureDeviceInfo> result) {
          devices_info = std::move(result);
          run_loop.Quit();
        }));
    run_loop.Run();

    ASSERT_EQ(devices_info.size(), 1u);
    device_delegate_thread_.Start();

    camera_device_delegate_ = std::make_unique<CameraDeviceDelegate>(
        devices_info[0].descriptor, camera_hal_delegate_.get(),
        device_delegate_thread_.task_runner(), ui_task_runner_);
  }

  void GetNumberOfFakeCameras(
      cros::mojom::CameraModule::GetNumberOfCamerasCallback& cb) {
    std::move(cb).Run(1);
  }

  void GetFakeVendorTagOps(
      mojo::PendingReceiver<cros::mojom::VendorTagOps> vendor_tag_ops_receiver,
      cros::mojom::CameraModule::GetVendorTagOpsCallback& cb) {
    mock_vendor_tag_ops_.Bind(std::move(vendor_tag_ops_receiver));
  }

  void GetFakeCameraInfo(uint32_t camera_id,
                         cros::mojom::CameraModule::GetCameraInfoCallback& cb) {
    cros::mojom::CameraInfoPtr camera_info = cros::mojom::CameraInfo::New();
    cros::mojom::CameraMetadataPtr static_metadata =
        cros::mojom::CameraMetadata::New();

    static_metadata->entry_count = 5;
    static_metadata->entry_capacity = 5;
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

    entry = cros::mojom::CameraMetadataEntry::New();
    entry->index = 3;
    entry->tag =
        cros::mojom::CameraMetadataTag::ANDROID_REQUEST_PIPELINE_MAX_DEPTH;
    entry->type = cros::mojom::EntryType::TYPE_BYTE;
    entry->count = 1;
    uint8_t pipeline_max_depth = 1;
    entry->data.assign(&pipeline_max_depth,
                       &pipeline_max_depth + entry->count * sizeof(uint8_t));
    static_metadata->entries->push_back(std::move(entry));

    entry = cros::mojom::CameraMetadataEntry::New();
    entry->index = 4;
    entry->tag = cros::mojom::CameraMetadataTag::
        ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES;
    entry->type = cros::mojom::EntryType::TYPE_INT32;
    entry->count = 2;
    std::vector<int32_t> available_fps_ranges = {kDefaultMinFrameRate,
                                                 kDefaultMaxFrameRate};
    as_int8 = reinterpret_cast<uint8_t*>(available_fps_ranges.data());
    entry->data.assign(as_int8, as_int8 + entry->count * sizeof(int32_t));
    static_metadata->entries->push_back(std::move(entry));

    entry = cros::mojom::CameraMetadataEntry::New();
    entry->index = 5;
    entry->tag =
        cros::mojom::CameraMetadataTag::ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE;
    entry->type = cros::mojom::EntryType::TYPE_INT32;
    entry->count = 4;
    std::vector<int32_t> active_array_size = {0, 0, 1920, 1080};
    as_int8 = reinterpret_cast<uint8_t*>(active_array_size.data());
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
      mojo::PendingReceiver<cros::mojom::Camera3DeviceOps> device_ops_receiver,
      base::OnceCallback<void(int32_t)>& callback) {
    mock_camera_device_receiver_.Bind(std::move(device_ops_receiver));
    std::move(callback).Run(0);
  }

  void InitializeMockCameraDevice(
      mojo::PendingRemote<cros::mojom::Camera3CallbackOps> callback_ops,
      base::OnceCallback<void(int32_t)>& callback) {
    callback_ops_.Bind(std::move(callback_ops));
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

  void ProcessCaptureRequest(cros::mojom::Camera3CaptureRequestPtr& request,
                             base::OnceCallback<void(int32_t)>& callback) {
    std::move(callback).Run(0);

    cros::mojom::Camera3NotifyMsgPtr msg = cros::mojom::Camera3NotifyMsg::New();
    msg->type = cros::mojom::Camera3MsgType::CAMERA3_MSG_SHUTTER;
    cros::mojom::Camera3ShutterMsgPtr shutter_msg =
        cros::mojom::Camera3ShutterMsg::New();
    shutter_msg->timestamp = base::TimeTicks::Now().ToInternalValue();
    msg->message = cros::mojom::Camera3NotifyMsgMessage::NewShutter(
        std::move(shutter_msg));
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
    mock_camera_device_receiver_.reset();
    callback_ops_.reset();
    std::move(callback).Run(0);
  }

  void SetUpExpectationForHalDelegate() {
    EXPECT_CALL(mock_camera_module_, DoGetNumberOfCameras(_))
        .Times(1)
        .WillOnce(
            Invoke(this, &CameraDeviceDelegateTest::GetNumberOfFakeCameras));
    EXPECT_CALL(mock_camera_module_, DoSetCallbacksAssociated(_, _)).Times(1);
    EXPECT_CALL(mock_camera_module_, DoGetVendorTagOps(_, _))
        .Times(1)
        .WillOnce(Invoke(this, &CameraDeviceDelegateTest::GetFakeVendorTagOps));
    EXPECT_CALL(mock_camera_module_, DoGetCameraInfo(0, _))
        .Times(1)
        .WillOnce(Invoke(this, &CameraDeviceDelegateTest::GetFakeCameraInfo));
  }

  void SetUpExpectationUntilInitialized() {
    SetUpExpectationForHalDelegate();
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

    // CameraBufferFactory::ResolveStreamBufferFormat() is now using
    // ::CreateSharedImage() instead of ::CreateGpuMemoryBuffer(). Hence adding
    // some expectations here.
    EXPECT_CALL(*test_sii_,
                DoCreateSharedImage(
                    _, viz::MultiPlaneFormat::kNV12, gpu::kNullSurfaceHandle,
                    gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE))
        .Times(1);
    EXPECT_CALL(*test_sii_,
                DoCreateSharedImage(
                    _, viz::SinglePlaneFormat::kR_8, gpu::kNullSurfaceHandle,
                    gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE))
        .Times(AtMost(1));
    EXPECT_CALL(*test_sii_,
                DoCreateSharedImage(
                    gfx::Size(kJpegMaxBufferSize, 1),
                    viz::SinglePlaneFormat::kR_8, gpu::kNullSurfaceHandle,
                    gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE))
        .Times(AtMost(1));

    // Note that ::CreateGpuMemoryBuffer() is currently being used by
    // StreamBufferManager.
    ON_CALL(mock_gpu_memory_buffer_manager_,
            CreateGpuMemoryBuffer(_, gfx::BufferFormat::R_8,
                                  gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
                                  gpu::kNullSurfaceHandle, nullptr))
        .WillByDefault(Invoke(&unittest_internal::MockGpuMemoryBufferManager::
                                  CreateFakeGpuMemoryBuffer));
    EXPECT_CALL(mock_gpu_memory_buffer_manager_,
                CreateGpuMemoryBuffer(
                    gfx::Size(kDefaultWidth, kDefaultHeight),
                    gfx::BufferFormat::YUV_420_BIPLANAR,
                    gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
                    gpu::kNullSurfaceHandle, nullptr))
        .Times(1)
        .WillOnce(Invoke(&unittest_internal::MockGpuMemoryBufferManager::
                             CreateFakeGpuMemoryBuffer));
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
    base::TimeDelta kWaitTimeoutSecs = base::Seconds(3);
    EXPECT_TRUE(device_closed.TimedWait(kWaitTimeoutSecs));
    EXPECT_EQ(CameraDeviceContext::State::kStopped, GetState());
  }

  unittest_internal::NiceMockVideoCaptureClient* ResetDeviceContext() {
    client_type_ = ClientType::kPreviewClient;
    auto mock_client =
        std::make_unique<unittest_internal::NiceMockVideoCaptureClient>();
    auto* client_ptr = mock_client.get();
    device_context_ = std::make_unique<CameraDeviceContext>();
    device_context_->AddClient(client_type_, std::move(mock_client));
    return client_ptr;
  }

  void ResetDevice() {
    device_context_->RemoveClient(client_type_);
    ASSERT_TRUE(device_delegate_thread_.IsRunning());
    ASSERT_TRUE(camera_device_delegate_);
    ASSERT_TRUE(device_delegate_thread_.task_runner()->DeleteSoon(
        FROM_HERE, std::move(camera_device_delegate_)));
    device_delegate_thread_.Stop();
  }

  void DoLoop() {
    run_loop_ = std::make_unique<base::RunLoop>();
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
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<CameraHalDelegate> camera_hal_delegate_;
  std::unique_ptr<CameraDeviceDelegate> camera_device_delegate_;

  testing::StrictMock<unittest_internal::MockCameraModule> mock_camera_module_;
  testing::NiceMock<unittest_internal::MockVendorTagOps> mock_vendor_tag_ops_;
  unittest_internal::MockGpuMemoryBufferManager mock_gpu_memory_buffer_manager_;
  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;

  testing::StrictMock<MockCameraDevice> mock_camera_device_;
  mojo::Receiver<cros::mojom::Camera3DeviceOps> mock_camera_device_receiver_;
  mojo::Remote<cros::mojom::Camera3CallbackOps> callback_ops_;

  base::Thread device_delegate_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  std::unique_ptr<CameraDeviceContext> device_context_;
  ClientType client_type_;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Test the complete capture flow: initialize, configure stream, capture one
// frame, and close the device.
TEST_F(CameraDeviceDelegateTest, AllocateCaptureAndStop) {
  auto* mock_client = ResetDeviceContext();
  mock_client->SetFrameCb(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  mock_client->SetQuitCb(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationUntilCapturing(mock_client);
  SetUpExpectationForCaptureLoop();

  AllocateDevice();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                                camera_device_delegate_->GetWeakPtr(),
                                GetDefaultCaptureParams(),
                                base::Unretained(device_context_.get())));

  // Wait until a frame is received.  MockVideoCaptureClient calls QuitRunLoop()
  // to stop the run loop.
  DoLoop();
  EXPECT_EQ(CameraDeviceContext::State::kCapturing, GetState());

  SetUpExpectationForClose();

  WaitForDeviceToClose();

  ResetDevice();
}

// Test that the camera device delegate closes properly when StopAndDeAllocate()
// is called when the device is opening. The timeline is roughly:
// 1. AllocateAndStart()
// 2. Async IPC call OpenDevice() started
// 3. StopAndDeAllocate()
// 4. Async IPC call OpenDevice() finished
TEST_F(CameraDeviceDelegateTest, StopBeforeOpened) {
  auto* mock_client = ResetDeviceContext();
  mock_client->SetQuitCb(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationForHalDelegate();

  AllocateDevice();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                                camera_device_delegate_->GetWeakPtr(),
                                GetDefaultCaptureParams(),
                                base::Unretained(device_context_.get())));

  base::WaitableEvent stop_posted;
  auto open_device_quit_loop_cb =
      [&](int32_t camera_id,
          mojo::PendingReceiver<cros::mojom::Camera3DeviceOps>
              device_ops_receiver,
          base::OnceCallback<void(int32_t)>& callback) {
        QuitRunLoop();
        // Make sure StopAndDeAllocate() is called before the device opened
        // callback.
        stop_posted.Wait();
        OpenMockCameraDevice(camera_id, std::move(device_ops_receiver),
                             callback);
      };
  EXPECT_CALL(mock_camera_module_, DoOpenDevice(0, _, _))
      .Times(1)
      .WillOnce(Invoke(open_device_quit_loop_cb));

  // Wait until the QuitRunLoop() call in |mock_camera_module_->OpenDevice()|.
  DoLoop();

  SetUpExpectationForClose();

  base::WaitableEvent device_closed;
  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraDeviceDelegate::StopAndDeAllocate,
                     camera_device_delegate_->GetWeakPtr(),
                     base::BindOnce(&base::WaitableEvent::Signal,
                                    base::Unretained(&device_closed))));
  stop_posted.Signal();
  EXPECT_TRUE(device_closed.TimedWait(base::Seconds(3)));
  EXPECT_EQ(CameraDeviceContext::State::kStopped, GetState());

  ResetDevice();
}

// Test that the camera device delegate closes properly when StopAndDeAllocate
// is called right after the device is initialized.
TEST_F(CameraDeviceDelegateTest, StopAfterInitialized) {
  auto* mock_client = ResetDeviceContext();
  mock_client->SetQuitCb(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationUntilInitialized();

  AllocateDevice();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                                camera_device_delegate_->GetWeakPtr(),
                                GetDefaultCaptureParams(),
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
            std::move(callback).Run(-ENODEV, {});
            this->QuitRunLoop();
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
  auto* mock_client = ResetDeviceContext();
  mock_client->SetQuitCb(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationUntilStreamConfigured();

  AllocateDevice();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                                camera_device_delegate_->GetWeakPtr(),
                                GetDefaultCaptureParams(),
                                base::Unretained(device_context_.get())));

  EXPECT_CALL(mock_camera_device_, DoConstructDefaultRequestSettings(_, _))
      .Times(1)
      .WillOnce(Invoke(
          [this](cros::mojom::Camera3RequestTemplate type,
                 base::OnceCallback<void(cros::mojom::CameraMetadataPtr)>&
                     callback) {
            EXPECT_EQ(CameraDeviceContext::State::kStreamConfigured,
                      this->GetState());
            std::move(callback).Run({});
            this->QuitRunLoop();
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
  SetUpExpectationForHalDelegate();

  AllocateDevice();

  auto* mock_client = ResetDeviceContext();

  auto stop_on_error = [&]() {
    device_delegate_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CameraDeviceDelegate::StopAndDeAllocate,
                       camera_device_delegate_->GetWeakPtr(),
                       base::BindPostTaskToCurrentDefault(base::BindOnce(
                           &CameraDeviceDelegateTest::QuitRunLoop,
                           base::Unretained(this)))));
  };
  EXPECT_CALL(*mock_client, OnError(_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(InvokeWithoutArgs(stop_on_error));

  // Hold the |device_ops_receiver| to make the behavior of CameraDeviceDelegate
  // deterministic. Otherwise the connection error handler would race with the
  // callback of OpenDevice(), because they are in different mojo channels.
  mojo::PendingReceiver<cros::mojom::Camera3DeviceOps>
      device_ops_receiver_holder;
  auto open_device_with_error_cb =
      [&](int32_t camera_id,
          mojo::PendingReceiver<cros::mojom::Camera3DeviceOps>
              device_ops_receiver,
          base::OnceCallback<void(int32_t)>& callback) {
        device_ops_receiver_holder = std::move(device_ops_receiver);
        std::move(callback).Run(-ENODEV);
      };
  EXPECT_CALL(mock_camera_module_, DoOpenDevice(0, _, _))
      .Times(1)
      .WillOnce(Invoke(open_device_with_error_cb));

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                                camera_device_delegate_->GetWeakPtr(),
                                GetDefaultCaptureParams(),
                                base::Unretained(device_context_.get())));

  // Wait unitl |camera_device_delegate_->StopAndDeAllocate| calls the
  // QuitRunLoop callback.
  DoLoop();

  ResetDevice();
}

// Test that the class handles it correctly when StopAndDeAllocate is called
// multiple times.
TEST_F(CameraDeviceDelegateTest, DoubleStopAndDeAllocate) {
  auto* mock_client = ResetDeviceContext();
  mock_client->SetFrameCb(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  mock_client->SetQuitCb(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationUntilCapturing(mock_client);
  SetUpExpectationForCaptureLoop();

  AllocateDevice();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                                camera_device_delegate_->GetWeakPtr(),
                                GetDefaultCaptureParams(),
                                base::Unretained(device_context_.get())));

  // Wait until a frame is received.  MockVideoCaptureClient calls QuitRunLoop()
  // to stop the run loop.
  DoLoop();

  EXPECT_EQ(CameraDeviceContext::State::kCapturing, GetState());

  SetUpExpectationForClose();

  WaitForDeviceToClose();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraDeviceDelegate::StopAndDeAllocate,
                     camera_device_delegate_->GetWeakPtr(),
                     base::BindPostTaskToCurrentDefault(
                         base::BindOnce(&CameraDeviceDelegateTest::QuitRunLoop,
                                        base::Unretained(this)))));
  DoLoop();

  ResetDevice();
}

}  // namespace media
