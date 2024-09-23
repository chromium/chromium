// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/device_factory.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "services/video_capture/video_capture_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_capture {

using testing::_;
using testing::Exactly;
using testing::Invoke;
using testing::InvokeWithoutArgs;

// Test fixture that creates a video_capture::ServiceImpl and sets up a
// local service_manager::Connector through which client code can connect to
// it.
class VideoCaptureServiceLifecycleTest : public ::testing::Test {
 public:
  VideoCaptureServiceLifecycleTest() = default;

  VideoCaptureServiceLifecycleTest(const VideoCaptureServiceLifecycleTest&) =
      delete;
  VideoCaptureServiceLifecycleTest& operator=(
      const VideoCaptureServiceLifecycleTest&) = delete;

  ~VideoCaptureServiceLifecycleTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    service_impl_ = std::make_unique<VideoCaptureServiceImpl>(
        service_remote_.BindNewPipeAndPassReceiver(),
        task_environment_.GetMainThreadTaskRunner(),
        /*create_system_monitor=*/true);
    service_remote_.set_idle_handler(
        base::TimeDelta(),
        base::BindRepeating(&VideoCaptureServiceLifecycleTest::OnServiceIdle,
                            base::Unretained(this)));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<VideoCaptureServiceImpl> service_impl_;
  mojo::Remote<mojom::VideoCaptureService> service_remote_;
  base::MockCallback<mojom::VideoSourceProvider::GetSourceInfosCallback>
      device_info_receiver_;
  base::RunLoop service_idle_wait_loop_;

 private:
  void OnServiceIdle() { service_idle_wait_loop_.Quit(); }
};

// Tests that the service quits when the only client disconnects after not
// having done anything other than obtaining a connection to the video source
// provider.
TEST_F(VideoCaptureServiceLifecycleTest,
       ServiceQuitsWhenSingleVideoSourceProviderClientDisconnected) {
  mojo::Remote<mojom::VideoSourceProvider> source_provider;
  service_remote_->ConnectToVideoSourceProvider(
      source_provider.BindNewPipeAndPassReceiver());
  source_provider.reset();
  service_idle_wait_loop_.Run();
}

// Tests that the service quits when the only client disconnects after
// enumerating devices via the video source provider.
TEST_F(VideoCaptureServiceLifecycleTest, ServiceQuitsAfterEnumeratingDevices) {
  mojo::Remote<mojom::VideoSourceProvider> source_provider;
  service_remote_->ConnectToVideoSourceProvider(
      source_provider.BindNewPipeAndPassReceiver());

  base::RunLoop wait_loop;
  EXPECT_CALL(device_info_receiver_, Run)
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  source_provider->GetSourceInfos(device_info_receiver_.Get());
  wait_loop.Run();

  source_provider.reset();

  service_idle_wait_loop_.Run();
}

// Tests that enumerating devices works after the only client disconnects and
// reconnects via the video source provider.
TEST_F(VideoCaptureServiceLifecycleTest, EnumerateDevicesAfterReconnect) {
  // Connect |source_provider|.
  mojo::Remote<mojom::VideoSourceProvider> source_provider;
  service_remote_->ConnectToVideoSourceProvider(
      source_provider.BindNewPipeAndPassReceiver());

  // Disconnect |source_provider| and wait for the disconnect to propagate to
  // the service.
  {
    base::RunLoop wait_loop;
    source_provider->Close(base::BindOnce(
        [](base::RunLoop* wait_loop) { wait_loop->Quit(); }, &wait_loop));
    wait_loop.Run();
    source_provider.reset();
  }

  // Reconnect |source_provider|.
  service_remote_->ConnectToVideoSourceProvider(
      source_provider.BindNewPipeAndPassReceiver());

  // Enumerate devices.
  base::RunLoop wait_loop;
  EXPECT_CALL(device_info_receiver_, Run)
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  source_provider->GetSourceInfos(device_info_receiver_.Get());
  wait_loop.Run();

  source_provider.reset();

  service_idle_wait_loop_.Run();
}
}  // namespace video_capture
