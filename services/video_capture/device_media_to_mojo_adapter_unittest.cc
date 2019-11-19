// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_media_to_mojo_adapter.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/capture/video/mock_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::_;

namespace video_capture {

class DeviceMediaToMojoAdapterTest : public ::testing::Test {
 public:
  DeviceMediaToMojoAdapterTest() = default;
  ~DeviceMediaToMojoAdapterTest() override = default;

  void SetUp() override {
    mock_video_frame_handler_ = std::make_unique<MockVideoFrameHandler>(
        video_frame_handler_.InitWithNewPipeAndPassReceiver());
    auto mock_device = std::make_unique<media::MockDevice>();
    mock_device_ptr_ = mock_device.get();
#if defined(OS_CHROMEOS)
    adapter_ = std::make_unique<DeviceMediaToMojoAdapter>(
        std::move(mock_device), base::DoNothing(),
        base::ThreadTaskRunnerHandle::Get());
#else
    adapter_ = std::make_unique<DeviceMediaToMojoAdapter>(
        std::move(mock_device));
#endif  // defined(OS_CHROMEOS)
  }

  void TearDown() override {
    // The internals of ReceiverOnTaskRunner perform a DeleteSoon().
    adapter_.reset();
    base::RunLoop wait_loop;
    wait_loop.RunUntilIdle();
  }

 protected:
  media::MockDevice* mock_device_ptr_;
  std::unique_ptr<DeviceMediaToMojoAdapter> adapter_;
  std::unique_ptr<MockVideoFrameHandler> mock_video_frame_handler_;
  mojo::PendingRemote<mojom::VideoFrameHandler> video_frame_handler_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DeviceMediaToMojoAdapterTest,
       DeviceIsStoppedWhenReceiverClosesConnection) {
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_device_ptr_, DoAllocateAndStart(_, _))
        .WillOnce(Invoke(
            [](const media::VideoCaptureParams& params,
               std::unique_ptr<media::VideoCaptureDevice::Client>* client) {
              (*client)->OnStarted();
            }));
    EXPECT_CALL(*mock_video_frame_handler_, OnStarted())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));

    const media::VideoCaptureParams kArbitrarySettings;
    adapter_->Start(kArbitrarySettings, std::move(video_frame_handler_));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_device_ptr_, DoStopAndDeAllocate())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    mock_video_frame_handler_.reset();
    run_loop.Run();
  }
}

// Triggers a condition that caused a use-after-free reported in
// https://crbug.com/807887. The use-after-free happened because the connection
// lost event handler got invoked on a base::Unretained() pointer to |adapter_|
// after |adapter_| was released.
TEST_F(DeviceMediaToMojoAdapterTest,
       ReleaseInstanceSynchronouslyAfterReceiverClosedConnection) {
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_device_ptr_, DoAllocateAndStart(_, _))
        .WillOnce(Invoke(
            [](const media::VideoCaptureParams& params,
               std::unique_ptr<media::VideoCaptureDevice::Client>* client) {
              (*client)->OnStarted();
            }));
    EXPECT_CALL(*mock_video_frame_handler_, OnStarted())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));

    const media::VideoCaptureParams kArbitrarySettings;
    adapter_->Start(kArbitrarySettings, std::move(video_frame_handler_));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;

    // This posts invocation of the error event handler to the end of the
    // current sequence.
    mock_video_frame_handler_.reset();

    // This destroys the DeviceMediaToMojoAdapter, which in turn posts a
    // DeleteSoon in ~ReceiverOnTaskRunner() to the end of the current sequence.
    adapter_.reset();

    // Give error handle chance to get invoked
    run_loop.RunUntilIdle();
  }
}

}  // namespace video_capture
