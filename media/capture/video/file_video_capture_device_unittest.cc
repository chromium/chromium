// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/test/task_environment.h"
#include "media/base/test_data_util.h"
#include "media/capture/video/file_video_capture_device.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;

namespace media {

namespace {

const base::TimeDelta kWaitTimeoutSecs = base::Seconds(3);

class MockImageCaptureClient {
 public:
  MockImageCaptureClient()
      : wait_get_photo_state_(base::WaitableEvent::ResetPolicy::AUTOMATIC),
        wait_set_photo_state_(base::WaitableEvent::ResetPolicy::AUTOMATIC) {}

  // GMock doesn't support move-only arguments, so we use this forward method.
  void DoOnGetPhotoState(mojom::PhotoStatePtr state) {
    state_ = std::move(state);
    wait_get_photo_state_.Signal();
  }

  const mojom::PhotoState* State() {
    EXPECT_TRUE(wait_get_photo_state_.TimedWait(kWaitTimeoutSecs));
    return state_.get();
  }

  void DoOnSetPhotoOptions(bool success) {
    EXPECT_TRUE(success);
    wait_set_photo_state_.Signal();
  }

  void WaitSetPhotoOptions() {
    EXPECT_TRUE(wait_set_photo_state_.TimedWait(kWaitTimeoutSecs));
  }

  // GMock doesn't support move-only arguments, so we use this forward method.
  void DoOnPhotoTaken(mojom::BlobPtr blob) {
    EXPECT_TRUE(blob);
    OnCorrectPhotoTaken();
  }
  MOCK_METHOD0(OnCorrectPhotoTaken, void(void));

 private:
  base::WaitableEvent wait_get_photo_state_;
  mojom::PhotoStatePtr state_;

  base::WaitableEvent wait_set_photo_state_;
};

}  // namespace

class FileVideoCaptureDeviceTest : public ::testing::Test {
 protected:
  FileVideoCaptureDeviceTest()
      : client_(new NiceMockVideoCaptureDeviceClient()) {}

  void SetUp() override {
    EXPECT_CALL(*client_, OnError(_, _, _)).Times(0);
    EXPECT_CALL(*client_, OnStarted());
    device_ = std::make_unique<FileVideoCaptureDevice>(
        GetTestDataFilePath("bear.mjpeg"),
        std::make_unique<FakeGpuMemoryBufferSupport>());
    device_->AllocateAndStart(VideoCaptureParams(), std::move(client_));
  }

  void TearDown() override { device_->StopAndDeAllocate(); }

  std::unique_ptr<MockVideoCaptureDeviceClient> CreateClient() {
    return MockVideoCaptureDeviceClient::CreateMockClientWithBufferAllocator(
        base::BindPostTaskToCurrentDefault(
            base::BindRepeating(&FileVideoCaptureDeviceTest::OnFrameCaptured,
                                base::Unretained(this))));
  }

  void OnFrameCaptured(const VideoCaptureFormat& format) {
    last_format_ = format;
    run_loop_->Quit();
  }

  void WaitForCapturedFrame() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void SetPhotoOptions(double pan, double tilt, double zoom) {
    mojom::PhotoSettingsPtr photo_settings = mojom::PhotoSettings::New();
    photo_settings->has_pan = photo_settings->has_tilt =
        photo_settings->has_zoom = true;
    photo_settings->pan = pan;
    photo_settings->tilt = tilt;
    photo_settings->zoom = zoom;

    VideoCaptureDevice::SetPhotoOptionsCallback scoped_set_callback =
        base::BindOnce(&MockImageCaptureClient::DoOnSetPhotoOptions,
                       base::Unretained(&image_capture_client_));
    device_->SetPhotoOptions(std::move(photo_settings),
                             std::move(scoped_set_callback));
    image_capture_client_.WaitSetPhotoOptions();
  }

  std::unique_ptr<NiceMockVideoCaptureDeviceClient> client_;
  MockImageCaptureClient image_capture_client_;
  std::unique_ptr<VideoCaptureDevice> device_;
  VideoCaptureFormat last_format_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST_F(FileVideoCaptureDeviceTest, GetPhotoState) {
  VideoCaptureDevice::GetPhotoStateCallback scoped_get_callback =
      base::BindOnce(&MockImageCaptureClient::DoOnGetPhotoState,
                     base::Unretained(&image_capture_client_));

  device_->GetPhotoState(std::move(scoped_get_callback));

  const mojom::PhotoState* state = image_capture_client_.State();
  EXPECT_TRUE(state);

  // From gcd of "bear.mjpeg" width=320, height=192.
  const int kZoomMaxLevels = 64;

  const mojom::RangePtr& pan = state->pan;
  EXPECT_TRUE(pan);
  EXPECT_EQ(pan->current, 0);
  EXPECT_EQ(pan->max, kZoomMaxLevels - 1);
  EXPECT_EQ(pan->min, 0);
  EXPECT_EQ(pan->step, 1);

  const mojom::RangePtr& tilt = state->tilt;
  EXPECT_TRUE(tilt);
  EXPECT_EQ(tilt->current, kZoomMaxLevels - 1);
  EXPECT_EQ(tilt->max, kZoomMaxLevels - 1);
  EXPECT_EQ(tilt->min, 0);
  EXPECT_EQ(tilt->step, 1);

  const mojom::RangePtr& zoom = state->zoom;
  EXPECT_TRUE(zoom);
  EXPECT_EQ(zoom->current, 0);
  EXPECT_EQ(zoom->max, kZoomMaxLevels - 1);
  EXPECT_EQ(zoom->min, 0);
  EXPECT_EQ(zoom->step, 1);
}

TEST_F(FileVideoCaptureDeviceTest, SetPhotoOptions) {
  SetPhotoOptions(1.0, 1.0, 1.0);
}

TEST_F(FileVideoCaptureDeviceTest, TakePhoto) {
  SetPhotoOptions(1.0, 1.0, 1.0);

  VideoCaptureDevice::TakePhotoCallback scoped_callback =
      base::BindOnce(&MockImageCaptureClient::DoOnPhotoTaken,
                     base::Unretained(&image_capture_client_));

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure =
      base::BindPostTaskToCurrentDefault(run_loop.QuitClosure());
  EXPECT_CALL(image_capture_client_, OnCorrectPhotoTaken())
      .Times(1)
      .WillOnce(InvokeWithoutArgs([quit_closure]() { quit_closure.Run(); }));
  device_->TakePhoto(std::move(scoped_callback));
  run_loop.Run();
}

TEST_F(FileVideoCaptureDeviceTest, CaptureWithGpuMemoryBuffer) {
  auto client = CreateClient();
  VideoCaptureParams params;
  params.buffer_type = VideoCaptureBufferType::kGpuMemoryBuffer;
  auto device = std::make_unique<FileVideoCaptureDevice>(
      GetTestDataFilePath("bear.mjpeg"),
      std::make_unique<FakeGpuMemoryBufferSupport>());
  device->AllocateAndStart(params, std::move(client));
  WaitForCapturedFrame();
  EXPECT_EQ(last_format_.pixel_format, PIXEL_FORMAT_NV12);
  device->StopAndDeAllocate();
}

}  // namespace media
