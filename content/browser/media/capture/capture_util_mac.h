// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_CAPTURE_UTIL_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_CAPTURE_UTIL_MAC_H_

#include "content/browser/media/capture/capture_util.h"
#include "content/browser/media/capture/pip_screen_capture_coordinator_proxy.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"

namespace media {
class VideoCaptureDevice;
}  // namespace media

namespace content {
class WebContents;

std::unique_ptr<media::VideoCaptureDevice> CONTENT_EXPORT
CreateScreenCaptureKitDeviceMac(
    const DesktopMediaID& source,
    std::unique_ptr<PipScreenCaptureCoordinatorProxy>
        pip_screen_capture_coordinator_proxy);

// Returns the windowNumber prorperty of the window associated to
// |web_contents| if there is an associated window with a positive
// windowNumber, or nullopt otherwise.
std::optional<NativeWindowId> GetNativeWindowIdMac(WebContents& web_contents);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_CAPTURE_UTIL_MAC_H_
