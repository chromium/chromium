// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/fake_video_capture_device.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Combine;
using ::testing::Values;

namespace media {

bool operator==(const FakePhotoDeviceConfig& lhs,
                const FakePhotoDeviceConfig& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

namespace {

class ImageCaptureClient : public base::RefCounted<ImageCaptureClient> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

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
      : client_(CreateClient()),
        image_capture_client_(base::MakeRefCounted<ImageCaptureClient>()),
        video_capture_device_factory_(new FakeVideoCaptureDeviceFactory()) {}

  void SetUp() override { EXPECT_CALL(*client_, OnError(_, _, _)).Times(0); }

  std::unique_ptr<MockVideoCaptureDeviceClient> CreateClient() {
    return MockVideoCaptureDeviceClient::CreateMockClientWithBufferAllocator(
        base::BindPostTaskToCurrentDefault(base::BindRepeating(
            &FakeVideoCaptureDeviceTestBase::OnFrameCaptured,
            base::Unretained(this))));
  }

  void GetDevicesInfo() {
    devices_info_.clear();
    base::RunLoop run_loop;
    video_capture_device_factory_->GetDevicesInfo(base::BindLambdaForTesting(
        [this, &run_loop](std::vector<VideoCaptureDeviceInfo> result) {
          devices_info_ = std::move(result);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void OnFrameCaptured(const VideoCaptureFormat& format) {
    last_format_ = format;
    run_loop_->Quit();
  }

  void WaitForCapturedFrame() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  const VideoCaptureFormat& last_format() const { return last_format_; }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::vector<VideoCaptureDeviceInfo> devices_info_;
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
  const auto pixel_format = testing::get<0>(GetParam());
  const auto delivery_mode = testing::get<1>(GetParam());
  const auto frame_rate = testing::get<2>(GetParam());
  if (delivery_mode ==
          FakeVideoCaptureDevice::DeliveryMode::USE_CLIENT_PROVIDED_BUFFERS &&
      pixel_format == PIXEL_FORMAT_MJPEG) {
    // Unsupported case
    return;
  }

  GetDevicesInfo();
  ASSERT_FALSE(devices_info_.empty());

  std::unique_ptr<VideoCaptureDevice> device =
      FakeVideoCaptureDeviceFactory::CreateDeviceWithDefaultResolutions(
          pixel_format, delivery_mode, frame_rate,
          std::make_unique<FakeGpuMemoryBufferSupport>());
  ASSERT_TRUE(device);

  // First: Requested, Second: Expected
  std::vector<std::pair<gfx::Size, gfx::Size>> resolutions_to_test;
  resolutions_to_test.emplace_back(gfx::Size(640, 480), gfx::Size(640, 480));
  resolutions_to_test.emplace_back(gfx::Size(104, 105), gfx::Size(320, 240));
  resolutions_to_test.emplace_back(gfx::Size(0, 0), gfx::Size(96, 96));
  resolutions_to_test.emplace_back(gfx::Size(0, 720), gfx::Size(96, 96));
  resolutions_to_test.emplace_back(gfx::Size(1920, 1080),
                                   gfx::Size(1920, 1080));
  resolutions_to_test.emplace_back(gfx::Size(3840, 2160),
                                   gfx::Size(3840, 2160));

  for (const auto& resolution : resolutions_to_test) {
    std::unique_ptr<MockVideoCaptureDeviceClient> client = CreateClient();
    EXPECT_CALL(*client, OnError(_, _, _)).Times(0);
    EXPECT_CALL(*client, OnStarted());

    VideoCaptureParams capture_params;
    capture_params.requested_format.frame_size = resolution.first;
    capture_params.requested_format.frame_rate = frame_rate;
    if (delivery_mode ==
        FakeVideoCaptureDevice::DeliveryMode::USE_GPU_MEMORY_BUFFERS) {
      capture_params.buffer_type = VideoCaptureBufferType::kGpuMemoryBuffer;
    }
    device->AllocateAndStart(capture_params, std::move(client));

    WaitForCapturedFrame();
    EXPECT_EQ(resolution.second.width(), last_format().frame_size.width());
    EXPECT_EQ(resolution.second.height(), last_format().frame_size.height());
    if (delivery_mode ==
        FakeVideoCaptureDevice::DeliveryMode::USE_GPU_MEMORY_BUFFERS) {
      // NV12 is the only opaque format backing GpuMemoryBuffer.
      EXPECT_EQ(last_format().pixel_format, PIXEL_FORMAT_NV12);
    } else {
      EXPECT_EQ(last_format().pixel_format, pixel_format);
    }
    EXPECT_EQ(last_format().frame_rate, testing::get<2>(GetParam()));
    device->StopAndDeAllocate();
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FakeVideoCaptureDeviceTest,
    Combine(
        Values(PIXEL_FORMAT_I420, PIXEL_FORMAT_Y16, PIXEL_FORMAT_MJPEG),
        Values(
            FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS,
            FakeVideoCaptureDevice::DeliveryMode::USE_CLIENT_PROVIDED_BUFFERS,
            FakeVideoCaptureDevice::DeliveryMode::USE_GPU_MEMORY_BUFFERS),
        Values(20, 29.97, 30, 50, 60)));

TEST_F(FakeVideoCaptureDeviceTest, GetDeviceSupportedFormats) {
  video_capture_device_factory_->SetToDefaultDevicesConfig(4);
  GetDevicesInfo();
  ASSERT_EQ(4u, devices_info_.size());
  const VideoPixelFormat expected_format_by_device_index[] = {
      PIXEL_FORMAT_I420, PIXEL_FORMAT_Y16, PIXEL_FORMAT_MJPEG,
      PIXEL_FORMAT_I420};

  int device_index = 0;
  for (const auto& device : devices_info_) {
    const VideoCaptureFormats& supported_formats = device.supported_formats;
    ASSERT_EQ(6u, supported_formats.size());
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
    EXPECT_EQ(3840, supported_formats[5].frame_size.width());
    EXPECT_EQ(2160, supported_formats[5].frame_size.height());
    EXPECT_EQ(expected_format, supported_formats[5].pixel_format);
    EXPECT_GE(supported_formats[5].frame_rate, 20.0);
    device_index++;
  }
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
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();

  const mojom::PhotoState* state = image_capture_client_->state();
  ASSERT_TRUE(state);
  EXPECT_EQ(mojom::MeteringMode::NONE, state->current_white_balance_mode);
  EXPECT_EQ(mojom::MeteringMode::MANUAL, state->current_exposure_mode);
  EXPECT_EQ(mojom::MeteringMode::MANUAL, state->current_focus_mode);

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

  EXPECT_EQ(10, state->focus_distance->min);
  EXPECT_EQ(100, state->focus_distance->max);
  EXPECT_EQ(50, state->focus_distance->current);
  EXPECT_EQ(5, state->focus_distance->step);

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

  EXPECT_EQ(100, state->pan->min);
  EXPECT_EQ(400, state->pan->max);
  EXPECT_EQ(1, state->pan->step);
  EXPECT_GE(state->pan->current, state->pan->min);
  EXPECT_GE(state->pan->max, state->pan->current);
  EXPECT_EQ(100, state->tilt->min);
  EXPECT_EQ(400, state->tilt->max);
  EXPECT_EQ(1, state->tilt->step);
  EXPECT_GE(state->tilt->current, state->tilt->min);
  EXPECT_GE(state->tilt->max, state->tilt->current);
  EXPECT_EQ(100, state->zoom->min);
  EXPECT_EQ(400, state->zoom->max);
  EXPECT_EQ(1, state->zoom->step);
  EXPECT_GE(state->zoom->current, state->zoom->min);
  EXPECT_GE(state->zoom->max, state->zoom->current);
  EXPECT_TRUE(state->fill_light_mode.empty());

  ASSERT_TRUE(state->supported_background_blur_modes);
  EXPECT_EQ(2u, state->supported_background_blur_modes->size());
  EXPECT_EQ(1, base::ranges::count(*state->supported_background_blur_modes,
                                   mojom::BackgroundBlurMode::OFF));
  EXPECT_EQ(1, base::ranges::count(*state->supported_background_blur_modes,
                                   mojom::BackgroundBlurMode::BLUR));
  EXPECT_EQ(mojom::BackgroundBlurMode::OFF, state->background_blur_mode);

  ASSERT_TRUE(state->supported_background_segmentation_mask_states);
  EXPECT_EQ(2u, state->supported_background_segmentation_mask_states->size());
  EXPECT_EQ(1,
            base::ranges::count(
                *state->supported_background_segmentation_mask_states, false));
  EXPECT_EQ(1,
            base::ranges::count(
                *state->supported_background_segmentation_mask_states, true));
  EXPECT_FALSE(state->current_background_segmentation_mask_state);

  ASSERT_TRUE(state->supported_eye_gaze_correction_modes);
  EXPECT_EQ(2u, state->supported_eye_gaze_correction_modes->size());
  EXPECT_EQ(1, base::ranges::count(*state->supported_eye_gaze_correction_modes,
                                   mojom::EyeGazeCorrectionMode::OFF));
  EXPECT_EQ(1, base::ranges::count(*state->supported_eye_gaze_correction_modes,
                                   mojom::EyeGazeCorrectionMode::ON));
  EXPECT_EQ(mojom::EyeGazeCorrectionMode::OFF,
            state->current_eye_gaze_correction_mode);

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
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();

  // Retrieve Capabilities again and check against the set values.
  VideoCaptureDevice::GetPhotoStateCallback scoped_get_callback2 =
      base::BindOnce(&ImageCaptureClient::DoOnGetPhotoState,
                     image_capture_client_);

  EXPECT_CALL(*image_capture_client_.get(), OnCorrectGetPhotoState()).Times(1);
  device->GetPhotoState(std::move(scoped_get_callback2));
  run_loop_ = std::make_unique<base::RunLoop>();
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

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  device->StopAndDeAllocate();
}

struct CommandLineTestData {
  std::string switch_value_string;
  float expected_fps;
  size_t expected_device_count;
  FakeVideoCaptureDevice::DisplayMediaType expected_display_media_type;
  std::vector<VideoPixelFormat> expected_pixel_formats;
  FakePhotoDeviceConfig expected_photo_device_config;
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
  GetDevicesInfo();
  EXPECT_EQ(1u, devices_info_.size());
  VideoCaptureFormats& supported_formats = devices_info_[0].supported_formats;
  EXPECT_EQ(0u, supported_formats.size());

  VideoCaptureErrorOrDevice device_status =
      video_capture_device_factory_->CreateDevice(devices_info_[0].descriptor);
  ASSERT_TRUE(device_status.ok());
  auto device = device_status.ReleaseDevice();

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
  for (const auto& settings : config) {
    EXPECT_EQ(GetParam().expected_photo_device_config,
              settings.photo_device_config);
    EXPECT_EQ(GetParam().expected_display_media_type,
              settings.display_media_type);
  }

  video_capture_device_factory_->SetToCustomDevicesConfig(config);
  GetDevicesInfo();
  EXPECT_EQ(GetParam().expected_device_count, devices_info_.size());

  int device_index = 0;
  for (const auto& device_info : devices_info_) {
    const VideoCaptureFormats& supported_formats =
        device_info.supported_formats;
    for (const auto& supported_formats_entry : supported_formats) {
      EXPECT_EQ(GetParam().expected_pixel_formats[device_index],
                supported_formats_entry.pixel_format);
    }

    VideoCaptureErrorOrDevice device_status =
        video_capture_device_factory_->CreateDevice(device_info.descriptor);
    ASSERT_TRUE(device_status.ok());
    std::unique_ptr<VideoCaptureDevice> device = device_status.ReleaseDevice();

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

INSTANTIATE_TEST_SUITE_P(
    All,
    FakeVideoCaptureDeviceFactoryTest,
    Values(CommandLineTestData{"fps=-1",
                               5,
                               1u,
                               FakeVideoCaptureDevice::DisplayMediaType::ANY,
                               {PIXEL_FORMAT_I420},
                               {{true, true, true}, false, false, false}},
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
           CommandLineTestData{"hardware-support=none",
                               20,
                               1u,
                               FakeVideoCaptureDevice::DisplayMediaType::ANY,
                               {PIXEL_FORMAT_I420},
                               {{false, false, false}}},
           CommandLineTestData{"hardware-support=zoom,fps=60",
                               60,
                               1u,
                               FakeVideoCaptureDevice::DisplayMediaType::ANY,
                               {PIXEL_FORMAT_I420},
                               {{false, false, true}}},
           CommandLineTestData{"hardware-support=pan-tilt-zoom,fps=60",
                               60,
                               1u,
                               FakeVideoCaptureDevice::DisplayMediaType::ANY,
                               {PIXEL_FORMAT_I420},
                               {{true, true, true}}},
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
}  // namespace media
