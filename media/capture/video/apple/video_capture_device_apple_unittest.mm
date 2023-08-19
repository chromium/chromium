// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/video_capture_device_apple.h"

#include "base/apple/scoped_cftyperef.h"
#import "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "media/capture/video/apple/test/fake_av_capture_device_format.h"
#import "media/capture/video/apple/test/video_capture_test_utils.h"
#include "media/capture/video/apple/video_capture_device_avfoundation_utils.h"
#include "media/capture/video/apple/video_capture_device_factory_apple.h"
#include "media/capture/video/apple/video_capture_device_frame_receiver.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Test the behavior of the function FindBestCaptureFormat which is used to
// determine the capture format.
TEST(VideoCaptureDeviceMacTest, FindBestCaptureFormat) {
  FakeAVCaptureDeviceFormat* fmt_320_240_xyzw_30 =
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:320
                                                height:240
                                                fourCC:'xyzw'
                                             frameRate:30];

  FakeAVCaptureDeviceFormat* fmt_320_240_yuvs_30 =
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:320
                                                height:240
                                                fourCC:'yuvs'
                                             frameRate:30];
  FakeAVCaptureDeviceFormat* fmt_640_480_yuvs_30 =
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:640
                                                height:480
                                                fourCC:'yuvs'
                                             frameRate:30];

  FakeAVCaptureDeviceFormat* fmt_320_240_2vuy_30 =
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:320
                                                height:240
                                                fourCC:'2vuy'
                                             frameRate:30];
  FakeAVCaptureDeviceFormat* fmt_640_480_2vuy_30 =
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:640
                                                height:480
                                                fourCC:'2vuy'
                                             frameRate:30];
  FakeAVCaptureDeviceFormat* fmt_640_480_2vuy_60 =
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:640
                                                height:480
                                                fourCC:'2vuy'
                                             frameRate:30];
  FakeAVCaptureDeviceFormat* fmt_640_480_2vuy_30_60 =
      [[FakeAVCaptureDeviceFormat alloc] initWithWidth:640
                                                height:480
                                                fourCC:'2vuy'
                                             frameRate:30];
  [fmt_640_480_2vuy_30_60 setSecondFrameRate:60];

  // We'll be using this for the result in all of the below tests. Note that
  // in all of the tests, the test is run with the candidate capture functions
  // in two orders (forward and reversed). This is to avoid having the traversal
  // order of FindBestCaptureFormat affect the result.
  AVCaptureDeviceFormat* result = nil;

  // If we can't find a valid format, we should return nil;
  result = FindBestCaptureFormat(@[ fmt_320_240_xyzw_30 ], 320, 240, 30);
  EXPECT_EQ(result, nil);

  // Can't find a matching resolution
  result = FindBestCaptureFormat(@[ fmt_320_240_yuvs_30, fmt_320_240_2vuy_30 ],
                                 640, 480, 30);
  EXPECT_EQ(result, nil);
  result = FindBestCaptureFormat(@[ fmt_320_240_2vuy_30, fmt_320_240_yuvs_30 ],
                                 640, 480, 30);
  EXPECT_EQ(result, nil);

  // Simple exact match.
  result = FindBestCaptureFormat(@[ fmt_640_480_yuvs_30, fmt_320_240_yuvs_30 ],
                                 320, 240, 30);
  EXPECT_EQ(result, fmt_320_240_yuvs_30);
  result = FindBestCaptureFormat(@[ fmt_320_240_yuvs_30, fmt_640_480_yuvs_30 ],
                                 320, 240, 30);
  EXPECT_EQ(result, fmt_320_240_yuvs_30);

  // Different frame rate.
  result = FindBestCaptureFormat(@[ fmt_640_480_2vuy_30 ], 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_30);

  // Prefer the same frame rate.
  result = FindBestCaptureFormat(@[ fmt_640_480_yuvs_30, fmt_640_480_2vuy_60 ],
                                 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_60);
  result = FindBestCaptureFormat(@[ fmt_640_480_2vuy_60, fmt_640_480_yuvs_30 ],
                                 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_60);

  // Prefer version with matching frame rate.
  result = FindBestCaptureFormat(@[ fmt_640_480_yuvs_30, fmt_640_480_2vuy_60 ],
                                 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_60);
  result = FindBestCaptureFormat(@[ fmt_640_480_2vuy_60, fmt_640_480_yuvs_30 ],
                                 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_60);

  // Prefer version with matching frame rate when there are multiple framerates.
  result = FindBestCaptureFormat(
      @[ fmt_640_480_yuvs_30, fmt_640_480_2vuy_30_60 ], 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_30_60);
  result = FindBestCaptureFormat(
      @[ fmt_640_480_2vuy_30_60, fmt_640_480_yuvs_30 ], 640, 480, 60);
  EXPECT_EQ(result, fmt_640_480_2vuy_30_60);

  // Prefer version with the lower maximum framerate when there are multiple
  // framerates.
  result = FindBestCaptureFormat(
      @[ fmt_640_480_2vuy_30, fmt_640_480_2vuy_30_60 ], 640, 480, 30);
  EXPECT_EQ(result, fmt_640_480_2vuy_30);
  result = FindBestCaptureFormat(
      @[ fmt_640_480_2vuy_30_60, fmt_640_480_2vuy_30 ], 640, 480, 30);
  EXPECT_EQ(result, fmt_640_480_2vuy_30);

  // Prefer the Chromium format order.
  result = FindBestCaptureFormat(@[ fmt_640_480_yuvs_30, fmt_640_480_2vuy_30 ],
                                 640, 480, 30);
  EXPECT_EQ(result, fmt_640_480_2vuy_30);
  result = FindBestCaptureFormat(@[ fmt_640_480_2vuy_30, fmt_640_480_yuvs_30 ],
                                 640, 480, 30);
  EXPECT_EQ(result, fmt_640_480_2vuy_30);
}

class MockImageCaptureClient
    : public base::RefCountedThreadSafe<MockImageCaptureClient> {
 public:
  // GMock doesn't support move-only arguments, so we use this forward method.
  void DoOnGetPhotoState(mojom::PhotoStatePtr received_state) {
    state = std::move(received_state);
  }

  mojom::PhotoStatePtr state;

 private:
  friend class base::RefCountedThreadSafe<MockImageCaptureClient>;
  virtual ~MockImageCaptureClient() = default;
};

class VideoCaptureDeviceMacWithImageCaptureTest : public ::testing::Test {
 public:
  void RunCheckBackgroundBlurTestCase();
  void RunCaptureConfigurationChangeTestCase();

 protected:
  VideoCaptureDeviceMacWithImageCaptureTest()
      : image_capture_client_(new MockImageCaptureClient()) {}

  VideoCaptureDeviceApple* GetFirstAvailableDevice() {
    VideoCaptureDeviceFactoryApple video_capture_device_factory;
    std::vector<VideoCaptureDeviceInfo> devices_info =
        GetDevicesInfo(&video_capture_device_factory);
    if (devices_info.empty()) {
      return nullptr;
    }
    const auto& info = devices_info[0];
    auto* device = new VideoCaptureDeviceApple(info.descriptor);
    if (!device->Init(info.descriptor.capture_api)) {
      return nullptr;
    }
    return device;
  }

  mojom::PhotoState* GetPhotoState(VideoCaptureDeviceApple* device) {
    VideoCaptureDevice::GetPhotoStateCallback get_photo_state_callback =
        base::BindOnce(&MockImageCaptureClient::DoOnGetPhotoState,
                       image_capture_client_);

    device->GetPhotoState(std::move(get_photo_state_callback));
    mojom::PhotoState* photo_state = image_capture_client_->state.get();
    return photo_state;
  }

  const scoped_refptr<MockImageCaptureClient> image_capture_client_;
};

TEST_F(VideoCaptureDeviceMacWithImageCaptureTest, CheckBackgroundBlur) {
  RunTestCase(base::BindOnce(&VideoCaptureDeviceMacWithImageCaptureTest::
                                 RunCheckBackgroundBlurTestCase,
                             base::Unretained(this)));
}

void VideoCaptureDeviceMacWithImageCaptureTest::
    RunCheckBackgroundBlurTestCase() {
  auto* device = GetFirstAvailableDevice();
  if (!device) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }

  {
    device->SetIsPortraitEffectSupportedForTesting(false);
    mojom::PhotoState* photo_state = GetPhotoState(device);

    ASSERT_FALSE(photo_state->supported_background_blur_modes);
  }
  {
    device->SetIsPortraitEffectSupportedForTesting(true);
    device->SetIsPortraitEffectActiveForTesting(false);
    mojom::PhotoState* photo_state = GetPhotoState(device);

    ASSERT_TRUE(photo_state->supported_background_blur_modes);
    EXPECT_EQ(photo_state->supported_background_blur_modes->size(), 1u);
    EXPECT_EQ(photo_state->supported_background_blur_modes.value()[0],
              mojom::BackgroundBlurMode::OFF);
    EXPECT_EQ(photo_state->background_blur_mode,
              mojom::BackgroundBlurMode::OFF);
  }
  {
    device->SetIsPortraitEffectSupportedForTesting(true);
    device->SetIsPortraitEffectActiveForTesting(true);
    mojom::PhotoState* photo_state = GetPhotoState(device);

    ASSERT_TRUE(photo_state->supported_background_blur_modes);
    EXPECT_EQ(photo_state->supported_background_blur_modes->size(), 1u);
    EXPECT_EQ(photo_state->supported_background_blur_modes.value()[0],
              mojom::BackgroundBlurMode::BLUR);
    EXPECT_EQ(photo_state->background_blur_mode,
              mojom::BackgroundBlurMode::BLUR);
  }
}

TEST_F(VideoCaptureDeviceMacWithImageCaptureTest,
       CheckCaptureConfigurationChange) {
  RunTestCase(base::BindOnce(&VideoCaptureDeviceMacWithImageCaptureTest::
                                 RunCaptureConfigurationChangeTestCase,
                             base::Unretained(this)));
}

void VideoCaptureDeviceMacWithImageCaptureTest::
    RunCaptureConfigurationChangeTestCase() {
  auto* device = GetFirstAvailableDevice();
  if (!device) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }

  VideoCaptureParams arbitrary_params;
  arbitrary_params.requested_format.frame_size = gfx::Size(1280, 720);
  arbitrary_params.requested_format.frame_rate = 30.0f;
  arbitrary_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  auto client = std::make_unique<NiceMockVideoCaptureDeviceClient>();
  MockVideoCaptureDeviceClient* client_ptr = client.get();

  device->AllocateAndStart(arbitrary_params, std::move(client));

  {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(*client_ptr, OnCaptureConfigurationChanged())
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

    device->SetIsPortraitEffectActiveForTesting(false);
    run_loop.Run();
  }
  {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(*client_ptr, OnCaptureConfigurationChanged())
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

    device->SetIsPortraitEffectActiveForTesting(true);
    run_loop.Run();
  }

  device->StopAndDeAllocate();
}

}  // namespace media
