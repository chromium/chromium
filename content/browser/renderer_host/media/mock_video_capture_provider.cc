// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/mock_video_capture_provider.h"

#include <utility>

namespace content {

MockVideoCaptureProvider::MockVideoCaptureProvider() = default;

void MockVideoCaptureProvider::GetDeviceInfosAsync(
    media::mojom::VideoCaptureDeviceEnumerationMode enumeration_mode,
    GetDeviceInfosCallback result_callback) {
  enumeration_modes_.push_back(enumeration_mode);
  DoGetDeviceInfosAsync(enumeration_mode, std::move(result_callback));
}

MockVideoCaptureProvider::~MockVideoCaptureProvider() = default;

MockVideoCaptureDeviceLauncher::MockVideoCaptureDeviceLauncher() = default;

MockVideoCaptureDeviceLauncher::~MockVideoCaptureDeviceLauncher() = default;

MockLaunchedVideoCaptureDevice::MockLaunchedVideoCaptureDevice() = default;

MockLaunchedVideoCaptureDevice::~MockLaunchedVideoCaptureDevice() = default;

}  // namespace content
