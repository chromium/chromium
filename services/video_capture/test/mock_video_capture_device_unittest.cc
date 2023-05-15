// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"

#include "services/video_capture/test/mock_video_capture_device_test.h"

using testing::_;
using testing::Invoke;

namespace video_capture {

// Tests that the service stops the capture device when the client closes the
// connection to the client proxy it provided to the service.
TEST_F(MockVideoCaptureDeviceTest, DeviceIsStoppedWhenDiscardingDeviceClient) {
  {
    base::RunLoop wait_loop;

    EXPECT_CALL(mock_device_, DoStopAndDeAllocate())
        .WillOnce(Invoke([&wait_loop]() { wait_loop.Quit(); }));

    device_->Start(requested_settings_, std::move(mock_subscriber_));
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
