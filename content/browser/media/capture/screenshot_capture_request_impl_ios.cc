// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screenshot_capture_request_impl.h"

namespace content::desktop_capture {

// Desktop/window screenshot capture isn't implemented on iOS/tvOS.
std::unique_ptr<ScreenshotCaptureRequest> CreateScreenshotCaptureRequest(
    DesktopMediaID source,
    base::OnceCallback<void(const ::SkBitmap&)> callback) {
  return nullptr;
}

void SetDesktopCapturerForTesting(  // IN-TEST
    std::unique_ptr<webrtc::DesktopCapturer> capturer) {}

}  // namespace content::desktop_capture
