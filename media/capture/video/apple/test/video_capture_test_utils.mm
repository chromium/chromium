// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/capture/video/apple/test/video_capture_test_utils.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/capture/video/apple/video_capture_device_factory_apple.h"

namespace media {

void RunTestCase(base::OnceClosure test_case) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::RunLoop* run_loop, base::OnceClosure* test_case) {
                       std::move(*test_case).Run();
                       run_loop->Quit();
                     },
                     &run_loop, &test_case));
  run_loop.Run();
}

std::vector<VideoCaptureDeviceInfo> GetDevicesInfo(
    VideoCaptureDeviceFactoryApple* video_capture_device_factory) {
  std::vector<VideoCaptureDeviceInfo> descriptors;
  base::RunLoop run_loop;
  video_capture_device_factory->GetDevicesInfo(base::BindLambdaForTesting(
      [&descriptors, &run_loop](std::vector<VideoCaptureDeviceInfo> result) {
        descriptors = std::move(result);
        run_loop.Quit();
      }));
  run_loop.Run();
  return descriptors;
}

NSString* GetFirstDeviceId() {
  VideoCaptureDeviceFactoryApple video_capture_device_factory;
  std::vector<VideoCaptureDeviceInfo> devices_info =
      GetDevicesInfo(&video_capture_device_factory);
  if (devices_info.empty()) {
    return nil;
  }
  return base::SysUTF8ToNSString(devices_info.front().descriptor.device_id);
}

}  // namespace media
