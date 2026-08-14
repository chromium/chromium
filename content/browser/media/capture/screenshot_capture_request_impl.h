// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_SCREENSHOT_CAPTURE_REQUEST_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_SCREENSHOT_CAPTURE_REQUEST_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace content::desktop_capture {

// Internal factory that creates an in-flight screenshot capture request.
CONTENT_EXPORT std::unique_ptr<ScreenshotCaptureRequest>
CreateScreenshotCaptureRequest(
    DesktopMediaID source,
    base::OnceCallback<void(const ::SkBitmap&)> callback);

// Sets a custom DesktopCapturer implementation for testing.
CONTENT_EXPORT void SetDesktopCapturerForTesting(  // IN-TEST
    std::unique_ptr<webrtc::DesktopCapturer> capturer);

}  // namespace content::desktop_capture

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_SCREENSHOT_CAPTURE_REQUEST_IMPL_H_
