// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/capture/video/mac/video_capture_device_factory_mac.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/media_switches.h"
#import "media/capture/video/mac/test/video_capture_test_utils_mac.h"
#include "media/capture/video/mac/video_capture_device_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

enum class AVFoundationCaptureV2 { kEnabled, kDisabled };

class VideoCaptureDeviceFactoryMacTest
    : public ::testing::TestWithParam<AVFoundationCaptureV2> {
 public:
  VideoCaptureDeviceFactoryMacTest() {
    scoped_feature_list_.InitWithFeatureState(
        media::kAVFoundationCaptureV2,
        /*enabled=*/GetParam() == AVFoundationCaptureV2::kEnabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(VideoCaptureDeviceFactoryMacTest, ListDevicesAVFoundation) {
  RunTestCase(base::BindOnce([]() {
    VideoCaptureDeviceFactoryMac video_capture_device_factory;

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

INSTANTIATE_TEST_SUITE_P(,
                         VideoCaptureDeviceFactoryMacTest,
                         ::testing::Values(AVFoundationCaptureV2::kEnabled,
                                           AVFoundationCaptureV2::kDisabled));

}  // namespace media
