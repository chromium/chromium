// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/capture_util_mac.h"

#include <AppKit/AppKit.h>

#include <optional>

#include "content/browser/media/capture/screen_capture_kit_device_mac.h"
#include "content/public/browser/web_contents.h"
#include "media/capture/video/video_capture_device.h"

namespace content {

std::unique_ptr<media::VideoCaptureDevice> CreateScreenCaptureKitDeviceMac(
    const DesktopMediaID& source,
    std::unique_ptr<PipScreenCaptureCoordinatorProxy>
        pip_screen_capture_coordinator_proxy) {
  // Although ScreenCaptureKit is available in 12.3 there were some bugs that
  // were not fixed until 13.2.
  if (@available(macOS 13.2, *)) {
    return CreateScreenCaptureKitDeviceMac(
        source, /*filter=*/nullptr,
        /*callback=*/base::DoNothing(),
        std::move(pip_screen_capture_coordinator_proxy));
  } else {
    return nullptr;
  }
}

std::optional<NativeWindowId> GetNativeWindowIdMac(WebContents& web_contents) {
  gfx::NativeView native_view = web_contents.GetNativeView();
  NSView* ns_view = native_view.GetNativeNSView();
  if (!ns_view) {
    return std::nullopt;
  }

  NSWindow* ns_window = [ns_view window];
  if (!ns_window) {
    return std::nullopt;
  }

  int64_t window_number = [ns_window windowNumber];
  return window_number > 0 ? std::make_optional(window_number) : std::nullopt;
}

}  // namespace content
