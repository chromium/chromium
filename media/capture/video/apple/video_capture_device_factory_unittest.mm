// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/video_capture_device_factory_apple.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/base/media_switches.h"
#import "media/capture/video/apple/test/video_capture_test_utils.h"
#include "media/capture/video/apple/video_capture_device_apple.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(VideoCaptureDeviceFactoryMacTest, ListDevicesAVFoundation) {
  RunTestCase(base::BindOnce([]() {
    VideoCaptureDeviceFactoryApple video_capture_device_factory;

    std::vector<VideoCaptureDeviceInfo> devices_info =
        GetDevicesInfo(&video_capture_device_factory);
    if (devices_info.empty()) {
      DVLOG(1) << "No camera available. Exiting test.";
      return;
    }
    for (const auto& device : devices_info) {
      EXPECT_EQ(VideoCaptureApi::MACOSX_AVFOUNDATION,
                device.descriptor.capture_api);
    }
  }));
}

}  // namespace media
