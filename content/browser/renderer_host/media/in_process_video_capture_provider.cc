// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/in_process_video_capture_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/renderer_host/media/in_process_video_capture_device_launcher.h"

namespace content {

InProcessVideoCaptureProvider::InProcessVideoCaptureProvider(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner)
    : device_task_runner_(std::move(device_task_runner)) {}

InProcessVideoCaptureProvider::~InProcessVideoCaptureProvider() = default;

// static
std::unique_ptr<VideoCaptureProvider>
InProcessVideoCaptureProvider::CreateInstanceForScreenCapture(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner) {
  // Using base::WrapUnique<>(new ...) to access private constructor.
  return base::WrapUnique<InProcessVideoCaptureProvider>(
      new InProcessVideoCaptureProvider(std::move(device_task_runner)));
}

void InProcessVideoCaptureProvider::GetDeviceInfosAsync(
    GetDeviceInfosCallback result_callback) {
  NOTREACHED_IN_MIGRATION();
}

std::unique_ptr<VideoCaptureDeviceLauncher>
InProcessVideoCaptureProvider::CreateDeviceLauncher() {
  return std::make_unique<InProcessVideoCaptureDeviceLauncher>(
      device_task_runner_);
}

}  // namespace content
