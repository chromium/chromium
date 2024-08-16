// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screen_capture_kit_device_utils_mac.h"

#include "content/browser/media/capture/screen_capture_kit_device_mac.h"
#include "media/capture/video/video_capture_device.h"

namespace content {

std::unique_ptr<media::VideoCaptureDevice> CreateScreenCaptureKitDeviceMac(
    const DesktopMediaID& source) {
  // Although ScreenCaptureKit is available in 12.3 there were some bugs that
  // were not fixed until 13.2.
  if (@available(macOS 13.2, *)) {
    return CreateScreenCaptureKitDeviceMac(source, nullptr);
  } else {
    return nullptr;
  }
}

}  // namespace content
