// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/video_capture_device_launcher.h"

#include "base/task/single_thread_task_runner.h"
#include "content/browser/renderer_host/media/in_process_video_capture_device_launcher.h"

namespace content {

// static
std::unique_ptr<VideoCaptureDeviceLauncher>
VideoCaptureDeviceLauncher::CreateInProcessVideoCaptureDeviceLauncher(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner) {
  return std::make_unique<InProcessVideoCaptureDeviceLauncher>(
      device_task_runner, /*picker=*/nullptr);
}

}  // namespace content
