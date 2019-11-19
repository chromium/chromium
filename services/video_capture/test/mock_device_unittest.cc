// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"

#include "services/video_capture/test/mock_device_test.h"

using testing::_;
using testing::Invoke;

namespace video_capture {

// This alias ensures test output is easily attributed to this service's tests.
// TODO(rockot/chfremer): Consider just renaming the type.
using MockVideoCaptureDeviceTest = MockDeviceTest;

// Tests that the service stops the capture device when the client closes the
// connection to the device proxy.
TEST_F(MockVideoCaptureDeviceTest, DeviceIsStoppedWhenDiscardingDeviceProxy) {
  {
    base::RunLoop wait_loop;

    EXPECT_CALL(mock_device_, DoStopAndDeAllocate())
        .WillOnce(Invoke([&wait_loop]() { wait_loop.Quit(); }));

    device_remote_->Start(requested_settings_, std::move(mock_subscriber_));
    device_remote_.reset();

    wait_loop.Run();
  }

  // The internals of ReceiverOnTaskRunner perform a DeleteSoon().
  {
    base::RunLoop wait_loop;
    wait_loop.RunUntilIdle();
  }
}

// Tests that the service stops the capture device when the client closes the
// connection to the client proxy it provided to the service.
TEST_F(MockVideoCaptureDeviceTest, DeviceIsStoppedWhenDiscardingDeviceClient) {
  {
    base::RunLoop wait_loop;

    EXPECT_CALL(mock_device_, DoStopAndDeAllocate())
        .WillOnce(Invoke([&wait_loop]() { wait_loop.Quit(); }));

    device_remote_->Start(requested_settings_, std::move(mock_subscriber_));
    mock_video_frame_handler_.reset();

    wait_loop.Run();
  }

  // The internals of ReceiverOnTaskRunner perform a DeleteSoon().
  {
    base::RunLoop wait_loop;
    wait_loop.RunUntilIdle();
  }
}

}  // namespace video_capture
