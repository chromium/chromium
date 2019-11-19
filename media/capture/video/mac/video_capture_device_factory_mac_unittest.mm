// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/capture/video/mac/video_capture_device_factory_mac.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/capture/video/mac/video_capture_device_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Video capture code on MacOSX must run on a CFRunLoop enabled thread
// for interaction with AVFoundation.
// In order to make the test case run on the actual message loop that has
// been created for this thread, we need to run it inside a RunLoop. This is
// required, because on MacOS the capture code must run on a CFRunLoop
// enabled message loop.
void RunTestCase(base::OnceClosure test_case) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::RunLoop* run_loop, base::OnceClosure* test_case) {
                       std::move(*test_case).Run();
                       run_loop->Quit();
                     },
                     &run_loop, &test_case));
  run_loop.Run();
}

TEST(VideoCaptureDeviceFactoryMacTest, ListDevicesAVFoundation) {
  RunTestCase(base::BindOnce([]() {
    VideoCaptureDeviceFactoryMac video_capture_device_factory;

    VideoCaptureDeviceDescriptors descriptors;
    video_capture_device_factory.GetDeviceDescriptors(&descriptors);
    if (descriptors.empty()) {
      DVLOG(1) << "No camera available. Exiting test.";
      return;
    }
    for (const auto& descriptor : descriptors)
      EXPECT_EQ(VideoCaptureApi::MACOSX_AVFOUNDATION, descriptor.capture_api);
  }));
}

}  // namespace media
