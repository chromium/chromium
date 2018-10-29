// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fake_video_capture_device.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Invoke;
using ::testing::SaveArg;
using ::testing::Values;

namespace media {

namespace {

class StubBufferHandle : public VideoCaptureBufferHandle {
 public:
  StubBufferHandle(size_t mapped_size, uint8_t* data)
      : mapped_size_(mapped_size), data_(data) {}

  size_t mapped_size() const override { return mapped_size_; }
  uint8_t* data() const override { return data_; }
  const uint8_t* const_data() const override { return data_; }

 private:
  const size_t mapped_size_;
  uint8_t* const data_;
};

class StubBufferHandleProvider
    : public VideoCaptureDevice::Client::Buffer::HandleProvider {
 public:
  StubBufferHandleProvider(size_t mapped_size, uint8_t* data)
      : mapped_size_(mapped_size), data_(data) {}

  ~StubBufferHandleProvider() override = default;

  mojo::ScopedSharedBufferHandle GetHandleForInterProcessTransit(
      bool read_only) override {
    NOTREACHED();
    return mojo::ScopedSharedBufferHandle();
  }

  base::SharedMemoryHandle GetNonOwnedSharedMemoryHandleForLegacyIPC()
      override {
    NOTREACHED();
    return base::SharedMemoryHandle();
  }

  std::unique_ptr<VideoCaptureBufferHandle> GetHandleForInProcessAccess()
      override {
    return std::make_unique<StubBufferHandle>(mapped_size_, data_);
  }

 private:
  const size_t mapped_size_;
  uint8_t* const data_;
};

class StubReadWritePermission
    : public VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  StubReadWritePermission(uint8_t* data) : data_(data) {}
  ~StubReadWritePermission() override { delete[] data_; }

 private:
  uint8_t* const data_;
};

VideoCaptureDevice::Client::Buffer CreateStubBuffer(int buffer_id,
                                                    size_t mapped_size) {
  auto* buffer = new uint8_t[mapped_size];
  const int arbitrary_frame_feedback_id = 0;
  return VideoCaptureDevice::Client::Buffer(
      buffer_id, arbitrary_frame_feedback_id,
      std::make_unique<StubBufferHandleProvider>(mapped_size, buffer),
      std::make_unique<StubReadWritePermission>(buffer));
};

class ImageCaptureClient : public base::RefCounted<ImageCaptureClient> {
 public:
  // GMock doesn't support move-only arguments, so we use this forward method.
  void DoOnGetPhotoState(mojom::PhotoStatePtr state) {
    state_ = std::move(state);
    OnCorrectGetPhotoState();
  }
  MOCK_METHOD0(OnCorrectGetPhotoState, void(void));

  const mojom::PhotoState* state() { return state_.get(); }

  MOCK_METHOD1(OnCorrectSetPhotoOptions, void(bool));

  // GMock doesn't support move-only arguments, so we use this forward method.
  void DoOnPhotoTaken(mojom::BlobPtr blob) {
    // Only PNG images are supported right now.
    EXPECT_STREQ("image/png", blob->mime_type.c_str());
    // Not worth decoding the incoming data. Just check that the header is PNG.
    // http://www.libpng.org/pub/png/spec/1.2/PNG-Rationale.html#R.PNG-file-signature
    ASSERT_GT(blob->data.size(), 4u);
    EXPECT_EQ('P', blob->data[1]);
    EXPECT_EQ('N', blob->data[2]);
    EXPECT_EQ('G', blob->data[3]);
    OnCorrectPhotoTaken();
  }
  MOCK_METHOD0(OnCorrectPhotoTaken, void(void));

 private:
  friend class base::RefCounted<ImageCaptureClient>;
  virtual ~ImageCaptureClient() = default;

  mojom::PhotoStatePtr state_;
};

}  // namespace

class FakeVideoCaptureDeviceTestBase : public ::testing::Test {
 protected:
  FakeVideoCaptureDeviceTestBase()
      : descriptors_(new VideoCaptureDeviceDescriptors()),
        client_(CreateClient()),
        image_capture_client_(new ImageCaptureClient()),
        video_capture_device_factory_(new FakeVideoCaptureDeviceFactory()) {}

  void SetUp() override { EXPECT_CALL(*client_, OnError(_, _, _)).Times(0); }

  std::unique_ptr<MockVideoCaptureDeviceClient> CreateClient() {
    auto result = std::make_unique<MockVideoCaptureDeviceClient>();
    ON_CALL(*result, ReserveOutputBuffer(_, _, _, _))
        .WillByDefault(
            Invoke([](const gfx::Size& dimensions, VideoPixelFormat format, int,
                      VideoCaptureDevice::Client::Buffer* buffer) {
              EXPECT_GT(dimensions.GetArea(), 0);
              const VideoCaptureFormat frame_format(dimensions, 0.0, format);
              *buffer = CreateStubBuffer(0, frame_format.ImageAllocationSize());
              return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
            }));
    ON_CALL(*result, OnIncomingCapturedData(_, _, _, _, _, _, _))
        .WillByDefault(
            Invoke([this](const uint8_t*, int,
                          const media::VideoCaptureFormat& frame_format, int,
                          base::TimeTicks, base::TimeDelta,
                          int) { OnFrameCaptured(frame_format); }));
    ON_CALL(*result, OnIncomingCapturedGfxBuffer(_, _, _, _, _, _))
        .WillByDefault(
            Invoke([this](gfx::GpuMemoryBuffer*,
                          const media::VideoCaptureFormat& frame_format, int,
                          base::TimeTicks, base::TimeDelta,
                          int) { OnFrameCaptured(frame_format); }));
    ON_CALL(*result, DoOnIncomingCapturedBuffer(_, _, _, _))
        .WillByDefault(
            Invoke([this](media::VideoCaptureDevice::Client::Buffer&,
                          const media::VideoCaptureFormat& frame_format,
                          base::TimeTicks,
                          base::TimeDelta) { OnFrameCaptured(frame_format); }));
    ON_CALL(*result, DoOnIncomingCapturedBufferExt(_, _, _, _, _, _))
        .WillByDefault(
            Invoke([this](media::VideoCaptureDevice::Client::Buffer&,
                          const media::VideoCaptureFormat& frame_format,
                          base::TimeTicks, base::TimeDelta, gfx::Rect,
                          const media::VideoFrameMetadata&) {
              OnFrameCaptured(frame_format);
            }));
    return result;
  }

  void OnFrameCaptured(const VideoCaptureFormat& format) {
    last_format_ = format;
    run_loop_->QuitClosure().Run();
  }

  void WaitForCapturedFrame() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  const VideoCaptureFormat& last_format() const { return last_format_; }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<VideoCaptureDeviceDescriptors> descriptors_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<MockVideoCaptureDeviceClient> client_;
  const scoped_refptr<ImageCaptureClient> image_capture_client_;
  VideoCaptureFormat last_format_;
  const std::unique_ptr<FakeVideoCaptureDeviceFactory>
      video_capture_device_factory_;
};

class FakeVideoCaptureDeviceTest
    : public FakeVideoCaptureDeviceTestBase,
      public ::testing::WithParamInterface<
          ::testing::tuple<VideoPixelFormat,
                           FakeVideoCaptureDevice::DeliveryMode,
                           float>> {};

// Tests that a frame is delivered with the expected settings.
// Sweeps through a fixed set of requested/expected resolutions.
TEST_P(FakeVideoCaptureDeviceTest, CaptureUsing) {
  if (testing::get<1>(GetParam()) ==
          FakeVideoCaptureDevice::DeliveryMode::USE_CLIENT_PROVIDED_BUFFERS &&
      testing::get<0>(GetParam()) == PIXEL_FORMAT_MJPEG) {
    // Unsupported case
    return;
  }

  video_capture_device_factory_->GetDeviceDescriptors(descriptors_.get());
  ASSERT_FALSE(descriptors_->empty());

  std::unique_ptr<VideoCaptureDevice> device =
      FakeVideoCaptureDeviceFactory::CreateDeviceWithDefaultResolutions(
          testing::get<0>(GetParam()), testing::get<1>(GetParam()),
          testing::get<2>(GetParam()));
  ASSERT_TRUE(device);

  // First: Requested, Second: Expected
  std::vector<std::pair<gfx::Size, gfx::Size>> resolutions_to_test;
  resolutions_to_test.emplace_back(gfx::Size(640, 480), gfx::Size(640, 480));
  resolutions_to_test.emplace_back(gfx::Size(104, 105), gfx::Size(320, 240));
  resolutions_to_test.emplace_back(gfx::Size(0, 0), gfx::Size(96, 96));
  resolutions_to_test.emplace_back(gfx::Size(0, 720), gfx::Size(96, 96));
  resolutions_to_test.emplace_back(gfx::Size(1920, 1080),
                                   gfx::Size(1920, 1080));

  for (const auto& resolution : resolutions_to_test) {
    std::unique_ptr<MockVideoCaptureDeviceClient> client = CreateClient();
    EXPECT_CALL(*client, OnError(_, _, _)).Times(0);
    EXPECT_CALL(*client, OnStarted());

    VideoCaptureParams capture_params;
    capture_params.requested_format.frame_size = resolution.first;
    capture_params.requested_format.frame_rate = testing::get<2>(GetParam());
    device->AllocateAndStart(capture_params, std::move(client));

    WaitForCapturedFrame();
    EXPECT_EQ(resolution.second.width(), last_format().frame_size.width());
    EXPECT_EQ(resolution.second.height(), last_format().frame_size.height());
    EXPECT_EQ(last_format().pixel_format, testing::get<0>(GetParam()));
    EXPECT_EQ(last_format().frame_rate, testing::get<2>(GetParam()));
    device->StopAndDeAllocate();
  }
}

INSTANTIATE_TEST_CASE_P(
    ,
    FakeVideoCaptureDeviceTest,
    Combine(
        Values(PIXEL_FORMAT_I420, PIXEL_FORMAT_Y16, PIXEL_FORMAT_MJPEG),
        Values(
            FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS,
            FakeVideoCaptureDevice::DeliveryMode::USE_CLIENT_PROVIDED_BUFFERS),
        Values(20, 29.97, 30, 50, 60)));

TEST_F(FakeVideoCaptureDeviceTest, GetDeviceSupportedFormats) {
  video_capture_device_factory_->SetToDefaultDevicesConfig(4);
  video_capture_device_factory_->GetDeviceDescriptors(descriptors_.get());
  ASSERT_EQ(4u, descriptors_->size());
  const VideoPixelFormat expected_format_by_device_index[] = {
      PIXEL_FORMAT_I420, PIXEL_FORMAT_Y16, PIXEL_FORMAT_MJPEG,
      PIXEL_FORMAT_I420};

  int device_index = 0;
  for (const auto& descriptors_iterator : *descriptors_) {
    VideoCaptureFormats supported_formats;
    video_capture_device_factory_->GetSupportedFormats(descriptors_iterator,
                                                       &supported_formats);
    ASSERT_EQ(5u, supported_formats.size());
    VideoPixelFormat expected_format =
        expected_format_by_device_index[device_index];
    EXPECT_EQ(96, supported_formats[0].frame_size.width());
    EXPECT_EQ(96, supported_formats[0].frame_size.height());
    EXPECT_EQ(expected_format, supported_formats[0].pixel_format);
    EXPECT_GE(supported_formats[0].frame_rate, 20.0);
    EXPECT_EQ(320, supported_formats[1].frame_size.width());
    EXPECT_EQ(240, supported_formats[1].frame_size.height());
    EXPECT_EQ(expected_format, supported_formats[1].pixel_format);
    EXPECT_GE(supported_formats[1].frame_rate, 20.0);
    EXPECT_EQ(640, supported_formats[2].frame_size.width());
    EXPECT_EQ(480, supported_formats[2].frame_size.height());
    EXPECT_EQ(expected_format, supported_formats[2].pixel_format);
    EXPECT_GE(supported_formats[2].frame_rate, 20.0);
    EXPECT_EQ(1280, supported_formats[3].frame_size.width());
    EXPECT_EQ(720, supported_formats[3].frame_size.height());
    EXPECT_EQ(expected_format, supported_formats[3].pixel_format);
    EXPECT_GE(supported_formats[3].frame_rate, 20.0);
    EXPECT_EQ(1920, supported_formats[4].frame_size.width());
    EXPECT_EQ(1080, supported_formats[4].frame_size.height());
    EXPECT_EQ(expected_format, supported_formats[4].pixel_format);
    EXPECT_GE(supported_formats[4].frame_rate, 20.0);
    device_index++;
  }
}

TEST_F(FakeVideoCaptureDeviceTest, GetCameraCalibration) {
  const size_t device_count = 2;
  video_capture_device_factory_->SetToDefaultDevicesConfig(device_count);
  video_capture_device_factory_->GetDeviceDescriptors(descriptors_.get());
  ASSERT_EQ(device_count, descriptors_->size());
  ASSERT_FALSE(descriptors_->at(0).camera_calibration);
  const VideoCaptureDeviceDescriptor& depth_device = descriptors_->at(1);
  EXPECT_EQ("/dev/video1", depth_device.device_id);
  ASSERT_TRUE(depth_device.camera_calibration);
  EXPECT_EQ(135.0, depth_device.camera_calibration->focal_length_x);
  EXPECT_EQ(135.6, depth_device.camera_calibration->focal_length_y);
  EXPECT_EQ(0.0, depth_device.camera_calibration->depth_near);
  EXPECT_EQ(65.535, depth_device.camera_calibration->depth_far);
}

TEST_F(FakeVideoCaptureDeviceTest, ErrorDeviceReportsError) {
  auto device = FakeVideoCaptureDeviceFactory::CreateErrorDevice();
  ASSERT_TRUE(device);
  EXPECT_CALL(*client_, OnError(_, _, _));
  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30.0;
  device->AllocateAndStart(capture_params, std::move(client_));
}

TEST_F(FakeVideoCaptureDeviceTest, GetAndSetCapabilities) {
  std::unique_ptr<VideoCaptureDevice> device =
      FakeVideoCaptureDeviceFactory::CreateDeviceWithDefaultResolutions(
          PIXEL_FORMAT_I420,
          FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS,
          30.0);
  ASSERT_TRUE(device);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30.0;
  EXPECT_CALL(*client_, OnStarted());
  device->AllocateAndStart(capture_params, std::move(client_));

  VideoCaptureDevice::GetPhotoStateCallback scoped_get_callback =
      base::BindOnce(&ImageCaptureClient::DoOnGetPhotoState,
                     image_capture_client_);

  EXPECT_CALL(*image_capture_client_.get(), OnCorrectGetPhotoState()).Times(1);
  device->GetPhotoState(std::move(scoped_get_callback));
  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();

  const mojom::PhotoState* state = image_capture_client_->state();
  ASSERT_TRUE(state);
  EXPECT_EQ(mojom::MeteringMode::NONE, state->current_white_balance_mode);
  EXPECT_EQ(mojom::MeteringMode::MANUAL, state->current_exposure_mode);
  EXPECT_EQ(mojom::MeteringMode::NONE, state->current_focus_mode);

  EXPECT_EQ(0, state->exposure_compensation->min);
  EXPECT_EQ(0, state->exposure_compensation->max);
  EXPECT_EQ(0, state->exposure_compensation->current);
  EXPECT_EQ(0, state->exposure_compensation->step);
  EXPECT_EQ(10, state->exposure_time->min);
  EXPECT_EQ(100, state->exposure_time->max);
  EXPECT_EQ(50, state->exposure_time->current);
  EXPECT_EQ(5, state->exposure_time->step);
  EXPECT_EQ(0, state->color_temperature->min);
  EXPECT_EQ(0, state->color_temperature->max);
  EXPECT_EQ(0, state->color_temperature->current);
  EXPECT_EQ(0, state->color_temperature->step);
  EXPECT_EQ(100, state->iso->min);
  EXPECT_EQ(100, state->iso->max);
  EXPECT_EQ(100, state->iso->current);
  EXPECT_EQ(0, state->iso->step);

  EXPECT_EQ(0, state->brightness->min);
  EXPECT_EQ(0, state->brightness->max);
  EXPECT_EQ(0, state->brightness->current);
  EXPECT_EQ(0, state->brightness->step);
  EXPECT_EQ(0, state->contrast->min);
  EXPECT_EQ(0, state->contrast->max);
  EXPECT_EQ(0, state->contrast->current);
  EXPECT_EQ(0, state->contrast->step);
  EXPECT_EQ(0, state->saturation->min);
  EXPECT_EQ(0, state->saturation->max);
  EXPECT_EQ(0, state->saturation->current);
  EXPECT_EQ(0, state->saturation->step);
  EXPECT_EQ(0, state->sharpness->min);
  EXPECT_EQ(0, state->sharpness->max);
  EXPECT_EQ(0, state->sharpness->current);
  EXPECT_EQ(0, state->sharpness->step);

  EXPECT_FALSE(state->supports_torch);
  EXPECT_FALSE(state->torch);

  EXPECT_EQ(1.0, state->focus_distance->min);
  EXPECT_EQ(5.0, state->focus_distance->max);
  EXPECT_EQ(3.0, state->focus_distance->current);
  EXPECT_EQ(1.0, state->focus_distance->step);

  EXPECT_EQ(mojom::RedEyeReduction::NEVER, state->red_eye_reduction);
  EXPECT_EQ(capture_params.requested_format.frame_size.height(),
            state->height->current);
  EXPECT_EQ(96, state->height->min);
  EXPECT_EQ(1080, state->height->max);
  EXPECT_EQ(1, state->height->step);
  EXPECT_EQ(capture_params.requested_format.frame_size.width(),
            state->width->current);
  EXPECT_EQ(96, state->width->min);
  EXPECT_EQ(1920, state->width->max);
  EXPECT_EQ(1, state->width->step);
  EXPECT_EQ(100, state->zoom->min);
  EXPECT_EQ(400, state->zoom->max);
  EXPECT_EQ(1, state->zoom->step);
  EXPECT_GE(state->zoom->current, state->zoom->min);
  EXPECT_GE(state->zoom->max, state->zoom->current);
  EXPECT_TRUE(state->fill_light_mode.empty());

  // Set options: zoom to the maximum value.
  const int max_zoom_value = state->zoom->max;
  VideoCaptureDevice::SetPhotoOptionsCallback scoped_set_callback =
      base::BindOnce(&ImageCaptureClient::OnCorrectSetPhotoOptions,
                     image_capture_client_);

  mojom::PhotoSettingsPtr settings = mojom::PhotoSettings::New();
  settings->zoom = max_zoom_value;
  settings->has_zoom = true;

  EXPECT_CALL(*image_capture_client_.get(), OnCorrectSetPhotoOptions(true))
      .Times(1);
  device->SetPhotoOptions(std::move(settings), std::move(scoped_set_callback));
  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();

  // Retrieve Capabilities again and check against the set values.
  VideoCaptureDevice::GetPhotoStateCallback scoped_get_callback2 =
      base::BindOnce(&ImageCaptureClient::DoOnGetPhotoState,
                     image_capture_client_);

  EXPECT_CALL(*image_capture_client_.get(), OnCorrectGetPhotoState()).Times(1);
  device->GetPhotoState(std::move(scoped_get_callback2));
  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();
  EXPECT_EQ(max_zoom_value, image_capture_client_->state()->zoom->current);

  device->StopAndDeAllocate();
}

TEST_F(FakeVideoCaptureDeviceTest, TakePhoto) {
  std::unique_ptr<VideoCaptureDevice> device =
      FakeVideoCaptureDeviceFactory::CreateDeviceWithDefaultResolutions(
          PIXEL_FORMAT_I420,
          FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS,
          30.0);
  ASSERT_TRUE(device);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30.0;
  EXPECT_CALL(*client_, OnStarted());
  device->AllocateAndStart(capture_params, std::move(client_));

  VideoCaptureDevice::TakePhotoCallback scoped_callback = base::BindOnce(
      &ImageCaptureClient::DoOnPhotoTaken, image_capture_client_);

  EXPECT_CALL(*image_capture_client_.get(), OnCorrectPhotoTaken()).Times(1);
  device->TakePhoto(std::move(scoped_callback));

  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();
  device->StopAndDeAllocate();
}

struct CommandLineTestData {
  std::string switch_value_string;
  float expected_fps;
  size_t expected_device_count;
  FakeVideoCaptureDevice::DisplayMediaType expected_display_media_type;
  std::vector<VideoPixelFormat> expected_pixel_formats;
};

class FakeVideoCaptureDeviceFactoryTest
    : public FakeVideoCaptureDeviceTestBase,
      public ::testing::WithParamInterface<CommandLineTestData> {};

TEST_F(FakeVideoCaptureDeviceFactoryTest, DeviceWithNoSupportedFormats) {
  std::vector<FakeVideoCaptureDeviceSettings> config;
  FakeVideoCaptureDeviceSettings device_setting;
  device_setting.device_id = "Device with no supported formats";
  config.emplace_back(device_setting);
  video_capture_device_factory_->SetToCustomDevicesConfig(config);
  video_capture_device_factory_->GetDeviceDescriptors(descriptors_.get());
  EXPECT_EQ(1u, descriptors_->size());
  VideoCaptureFormats supported_formats;
  video_capture_device_factory_->GetSupportedFormats(descriptors_->at(0),
                                                     &supported_formats);
  EXPECT_EQ(0u, supported_formats.size());
  auto device =
      video_capture_device_factory_->CreateDevice(descriptors_->at(0));
  EXPECT_TRUE(device.get());

  auto client = CreateClient();
  EXPECT_CALL(*client, OnError(_, _, _));
  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(1280, 720);
  device->AllocateAndStart(capture_params, std::move(client));
}

// Tests that the FakeVideoCaptureDeviceFactory delivers the expected number
// of devices and formats when being configured using command-line switches.
TEST_P(FakeVideoCaptureDeviceFactoryTest,
       FrameRateAndDeviceCountAndDisplayMediaType) {
  std::vector<FakeVideoCaptureDeviceSettings> config;
  FakeVideoCaptureDeviceFactory::ParseFakeDevicesConfigFromOptionsString(
      GetParam().switch_value_string, &config);
  for (const auto settings : config) {
    EXPECT_EQ(GetParam().expected_display_media_type,
              settings.display_media_type);
  }

  video_capture_device_factory_->SetToCustomDevicesConfig(config);
  video_capture_device_factory_->GetDeviceDescriptors(descriptors_.get());
  EXPECT_EQ(GetParam().expected_device_count, descriptors_->size());

  int device_index = 0;
  for (const auto& descriptors_iterator : *descriptors_) {
    VideoCaptureFormats supported_formats;
    video_capture_device_factory_->GetSupportedFormats(descriptors_iterator,
                                                       &supported_formats);
    for (const auto& supported_formats_entry : supported_formats) {
      EXPECT_EQ(GetParam().expected_pixel_formats[device_index],
                supported_formats_entry.pixel_format);
    }

    std::unique_ptr<VideoCaptureDevice> device =
        video_capture_device_factory_->CreateDevice(descriptors_iterator);
    ASSERT_TRUE(device);

    VideoCaptureParams capture_params;
    capture_params.requested_format.frame_size.SetSize(1280, 720);
    capture_params.requested_format.frame_rate = GetParam().expected_fps;
    capture_params.requested_format.pixel_format =
        GetParam().expected_pixel_formats[device_index];
    std::unique_ptr<MockVideoCaptureDeviceClient> client = CreateClient();
    EXPECT_CALL(*client, OnStarted());
    device->AllocateAndStart(capture_params, std::move(client));
    WaitForCapturedFrame();
    EXPECT_EQ(1280, last_format().frame_size.width());
    EXPECT_EQ(720, last_format().frame_size.height());
    EXPECT_EQ(GetParam().expected_fps, last_format().frame_rate);
    EXPECT_EQ(GetParam().expected_pixel_formats[device_index],
              last_format().pixel_format);
    device->StopAndDeAllocate();

    device_index++;
  }
}

INSTANTIATE_TEST_CASE_P(
    ,
    FakeVideoCaptureDeviceFactoryTest,
    Values(CommandLineTestData{"fps=-1",
                               5,
                               1u,
                               FakeVideoCaptureDevice::DisplayMediaType::ANY,
                               {PIXEL_FORMAT_I420}},
           CommandLineTestData{"fps=29.97,device-count=1",
                               29.97f,
                               1u,
                               FakeVideoCaptureDevice::DisplayMediaType::ANY,
                               {PIXEL_FORMAT_I420}},
           CommandLineTestData{"fps=60,device-count=2",
                               60,
                               2u,
                               FakeVideoCaptureDevice::DisplayMediaType::ANY,
                               {PIXEL_FORMAT_I420, PIXEL_FORMAT_Y16}},
           CommandLineTestData{"fps=1000,device-count=-1",
                               60,
                               1u,
                               FakeVideoCaptureDevice::DisplayMediaType::ANY,
                               {PIXEL_FORMAT_I420}},
           CommandLineTestData{"device-count=4",
                               20,
                               4u,
                               FakeVideoCaptureDevice::DisplayMediaType::ANY,
                               {PIXEL_FORMAT_I420, PIXEL_FORMAT_Y16,
                                PIXEL_FORMAT_MJPEG, PIXEL_FORMAT_I420}},
           CommandLineTestData{"device-count=4,ownership=client",
                               20,
                               4u,
                               FakeVideoCaptureDevice::DisplayMediaType::ANY,
                               {PIXEL_FORMAT_I420, PIXEL_FORMAT_Y16,
                                PIXEL_FORMAT_MJPEG, PIXEL_FORMAT_I420}},
           CommandLineTestData{"device-count=0",
                               20,
                               0u,
                               FakeVideoCaptureDevice::DisplayMediaType::ANY,
                               {PIXEL_FORMAT_I420}},
           CommandLineTestData{"display-media-type=window",
                               20,
                               1u,
                               FakeVideoCaptureDevice::DisplayMediaType::WINDOW,
                               {PIXEL_FORMAT_I420}},
           CommandLineTestData{
               "display-media-type=browser,fps=60",
               60,
               1u,
               FakeVideoCaptureDevice::DisplayMediaType::BROWSER,
               {PIXEL_FORMAT_I420}}));
};  // namespace media
