// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_provider.h"

#include "content/public/browser/video_capture_device_launcher.h"

namespace content {

std::unique_ptr<VideoCaptureDeviceLauncher>
VideoCaptureProvider::CreateDeviceLauncherWithSharedMemoryBuffers() {
  return CreateDeviceLauncher();
}

}  // namespace content
