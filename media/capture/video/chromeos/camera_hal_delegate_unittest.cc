// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/chromeos/camera_hal_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/viz/test/test_context_provider.h"
#include "media/capture/video/chromeos/mock_camera_module.h"
#include "media/capture/video/chromeos/mock_vendor_tag_ops.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::A;
using testing::Invoke;
using testing::Return;

namespace {

constexpr uint32_t kDevicePathTag = 0x80000000;
constexpr char kFakeDevicePath[] = "/dev/video5566";

}  // namespace

namespace media {

class CameraHalDelegateTest : public ::testing::Test {
 public:
  CameraHalDelegateTest() {}

  CameraHalDelegateTest(const CameraHalDelegateTest&) = delete;
  CameraHalDelegateTest& operator=(const CameraHalDelegateTest&) = delete;

  void SetUp() override {
    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    test_sii_->UseTestGMBInSharedImageCreationWithBufferUsage();
    VideoCaptureDeviceFactoryChromeOS::SetSharedImageInterface(test_sii_);
    camera_hal_delegate_ = std::make_unique<CameraHalDelegate>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    if (!camera_hal_delegate_->Init()) {
      LOG(ERROR) << "Failed to initialize CameraHalDelegate";
      camera_hal_delegate_.reset();
      return;
    }
    camera_hal_delegate_->SetCameraModule(
        mock_camera_module_.GetPendingRemote());
  }

  void TearDown() override {
    VideoCaptureDeviceFactoryChromeOS::SetSharedImageInterface(nullptr);
    camera_hal_delegate_.reset();
    task_environment_.RunUntilIdle();
  }

  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<CameraHalDelegate> camera_hal_delegate_;
  testing::StrictMock<unittest_internal::MockCameraModule> mock_camera_module_;
  testing::StrictMock<unittest_internal::MockVendorTagOps> mock_vendor_tag_ops_;
  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST_F(CameraHalDelegateTest, GetBuiltinCameraInfo) {
  auto get_number_of_cameras_cb =
      [](cros::mojom::CameraModule::GetNumberOfCamerasCallback& cb) {
        std::move(cb).Run(2);
      };

  auto get_camera_info_cb = [](uint32_t camera_id,
                               cros::mojom::CameraModule::GetCameraInfoCallback&
                                   cb) {
    cros::mojom::CameraInfoPtr camera_info = cros::mojom::CameraInfo::New();
    cros::mojom::CameraMetadataPtr static_metadata =
        cros::mojom::CameraMetadata::New();
    static_metadata->entry_count = 2;
    static_metadata->entry_capacity = 2;
    static_metadata->entries =
        std::vector<cros::mojom::CameraMetadataEntryPtr>();

    cros::mojom::CameraMetadataEntryPtr entry =
        cros::mojom::CameraMetadataEntry::New();
    entry->index = 0;
    entry->tag = cros::mojom::CameraMetadataTag::
        ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS;
    entry->type = cros::mojom::EntryType::TYPE_INT64;
    entry->count = 8;
    std::vector<int64_t> min_frame_durations(8);
    min_frame_durations[0] = static_cast<int64_t>(
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
    min_frame_durations[1] = 1280;
    min_frame_durations[2] = 720;
    min_frame_durations[3] = 33333333;
    min_frame_durations[4] = static_cast<int64_t>(
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888);
    min_frame_durations[5] = 1280;
    min_frame_durations[6] = 720;
    min_frame_durations[7] = 16666666;
    uint8_t* as_int8 = reinterpret_cast<uint8_t*>(min_frame_durations.data());
    entry->data.assign(as_int8, as_int8 + entry->count * sizeof(int64_t));
    static_metadata->entries->push_back(std::move(entry));

    entry = cros::mojom::CameraMetadataEntry::New();
    entry->index = 1;
    entry->tag = cros::mojom::CameraMetadataTag::
        ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES;
    entry->type = cros::mojom::EntryType::TYPE_INT32;
    entry->count = 4;
    std::vector<int32_t> default_fps_range{30, 30, 60, 60};
    as_int8 = reinterpret_cast<uint8_t*>(default_fps_range.data());
    entry->data.assign(as_int8, as_int8 + entry->count * sizeof(int32_t));
    static_metadata->entries->push_back(std::move(entry));

    switch (camera_id) {
      case 0:
        camera_info->facing = cros::mojom::CameraFacing::CAMERA_FACING_BACK;
        camera_info->orientation = 0;
        camera_info->static_camera_characteristics = std::move(static_metadata);
        break;
      case 1:
        camera_info->facing = cros::mojom::CameraFacing::CAMERA_FACING_FRONT;
        camera_info->orientation = 0;
        camera_info->static_camera_characteristics = std::move(static_metadata);
        break;
      case 2:
        entry = cros::mojom::CameraMetadataEntry::New();
        entry->index = static_metadata->entry_count;
        entry->tag =
            static_cast<cros::mojom::CameraMetadataTag>(kDevicePathTag);
        entry->type = cros::mojom::EntryType::TYPE_BYTE;
        entry->count = sizeof(kFakeDevicePath);
        entry->data.assign(std::begin(kFakeDevicePath),
                           std::end(kFakeDevicePath));

        static_metadata->entry_count++;
        static_metadata->entry_capacity++;
        static_metadata->entries->push_back(std::move(entry));

        camera_info->facing = cros::mojom::CameraFacing::CAMERA_FACING_EXTERNAL;
        camera_info->orientation = 0;
        camera_info->static_camera_characteristics = std::move(static_metadata);
        break;
      default:
        FAIL() << "Invalid camera id";
    }

    std::move(cb).Run(0, std::move(camera_info));
  };

  auto get_vendor_tag_ops_cb =
      [&](mojo::PendingReceiver<cros::mojom::VendorTagOps>
              vendor_tag_ops_receiver,
          cros::mojom::CameraModule::GetVendorTagOpsCallback&) {
        mock_vendor_tag_ops_.Bind(std::move(vendor_tag_ops_receiver));
      };

  auto set_callbacks_cb =
      [&](mojo::PendingAssociatedRemote<cros::mojom::CameraModuleCallbacks>&
              callbacks,
          cros::mojom::CameraModule::SetCallbacksAssociatedCallback&) {
        mock_camera_module_.NotifyCameraDeviceChange(
            2, cros::mojom::CameraDeviceStatus::CAMERA_DEVICE_STATUS_PRESENT);
      };

  EXPECT_CALL(mock_camera_module_, DoGetNumberOfCameras(_))
      .Times(1)
      .WillOnce(Invoke(get_number_of_cameras_cb));
  EXPECT_CALL(
      mock_camera_module_,
      DoSetCallbacksAssociated(
          A<mojo::PendingAssociatedRemote<
              cros::mojom::CameraModuleCallbacks>&>(),
          A<cros::mojom::CameraModule::SetCallbacksAssociatedCallback&>()))
      .Times(1)
      .WillOnce(Invoke(set_callbacks_cb));
  EXPECT_CALL(mock_camera_module_,
              DoGetVendorTagOps(
                  A<mojo::PendingReceiver<cros::mojom::VendorTagOps>>(),
                  A<cros::mojom::CameraModule::GetVendorTagOpsCallback&>()))
      .Times(1)
      .WillOnce(Invoke(get_vendor_tag_ops_cb));
  EXPECT_CALL(mock_camera_module_,
              DoGetCameraInfo(
                  0, A<cros::mojom::CameraModule::GetCameraInfoCallback&>()))
      .Times(1)
      .WillOnce(Invoke(get_camera_info_cb));
  EXPECT_CALL(mock_camera_module_,
              DoGetCameraInfo(
                  1, A<cros::mojom::CameraModule::GetCameraInfoCallback&>()))
      .Times(1)
      .WillOnce(Invoke(get_camera_info_cb));
  EXPECT_CALL(mock_camera_module_,
              DoGetCameraInfo(
                  2, A<cros::mojom::CameraModule::GetCameraInfoCallback&>()))
      .Times(1)
      .WillOnce(Invoke(get_camera_info_cb));

  EXPECT_CALL(mock_vendor_tag_ops_, DoGetTagCount())
      .Times(1)
      .WillOnce(Return(1));

  EXPECT_CALL(mock_vendor_tag_ops_, DoGetAllTags())
      .Times(1)
      .WillOnce(Return(std::vector<uint32_t>{kDevicePathTag}));

  EXPECT_CALL(mock_vendor_tag_ops_, DoGetSectionName(kDevicePathTag))
      .Times(1)
      .WillOnce(Return("com.google"));

  EXPECT_CALL(mock_vendor_tag_ops_, DoGetTagName(kDevicePathTag))
      .Times(1)
      .WillOnce(Return("usb.devicePath"));

  EXPECT_CALL(mock_vendor_tag_ops_, DoGetTagType(kDevicePathTag))
      .Times(1)
      .WillOnce(
          Return(static_cast<int32_t>(cros::mojom::EntryType::TYPE_BYTE)));

  EXPECT_CALL(*test_sii_,
              DoCreateSharedImage(
                  _, viz::MultiPlaneFormat::kNV12, gpu::kNullSurfaceHandle,
                  gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE))
      .Times(1);

  std::vector<VideoCaptureDeviceInfo> devices_info;
  base::RunLoop run_loop;
  camera_hal_delegate_->GetDevicesInfo(base::BindLambdaForTesting(
      [&devices_info, &run_loop](std::vector<VideoCaptureDeviceInfo> result) {
        devices_info = std::move(result);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_EQ(3u, devices_info.size());
  // We have workaround to always put front camera at first.
  ASSERT_EQ("1", devices_info[0].descriptor.device_id);
  ASSERT_EQ(VideoFacingMode::MEDIA_VIDEO_FACING_USER,
            devices_info[0].descriptor.facing);
  ASSERT_EQ("0", devices_info[1].descriptor.device_id);
  ASSERT_EQ(VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT,
            devices_info[1].descriptor.facing);
  ASSERT_EQ(kFakeDevicePath, devices_info[2].descriptor.device_id);
  ASSERT_EQ(VideoFacingMode::MEDIA_VIDEO_FACING_NONE,
            devices_info[2].descriptor.facing);

  // TODO(shik): Test external camera. Check the fields |display_name| and
  // |model_id| are set properly according to the vendor tags.

  const VideoCaptureFormats& supported_formats =
      devices_info[0].supported_formats;

  // IMPLEMENTATION_DEFINED format should be filtered; currently YCbCr_420_888
  // format corresponds to NV12 in Chrome.
  ASSERT_GE(supported_formats.size(), 1U);
  for (auto& format : supported_formats) {
    ASSERT_EQ(gfx::Size(1280, 720), format.frame_size);
    ASSERT_TRUE(format.frame_rate == 60.0 || format.frame_rate == 30.0);
    ASSERT_EQ(PIXEL_FORMAT_NV12, format.pixel_format);
  }
}

}  // namespace media
