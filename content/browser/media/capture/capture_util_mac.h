// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_CAPTURE_UTIL_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_CAPTURE_UTIL_MAC_H_

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
    bool is_native_picker,
    std::unique_ptr<PipScreenCaptureCoordinatorProxy>
        pip_screen_capture_coordinator_proxy);

// Returns the windowNumber property of the window associated to
// |web_contents| if there is an associated window with a positive
// windowNumber, or nullopt otherwise.
std::optional<DesktopMediaID::Id> GetNativeWindowIdMac(
    WebContents& web_contents);

// Traverses the process hierarchy to find the "root" bundle identifier
// associated with the owner of window `id`.
//
// In macOS, a single logical application often consists of multiple
// processes. For example:
// - A main app (com.example.App)
// - Helper processes (com.example.App.Helper)
// - App Extensions (com.example.App.ShareExtension)
//
// This function identifies the top-most ancestor in the process tree where
// the ancestor's bundle ID is a dot-delimited prefix of the child's
// bundle ID.
//
// For example:
// - "com.example.App" is a prefix of "com.example.App.Helper".
// - "com.example.App-Foo" is NOT a prefix of "com.example.App.Helper" because
//   it does not follow the dot-delimited hierarchy.
//
// This allows callers to attribute helper processes back to the
// main application.
//
// Returns:
// - The bundle ID of the highest ancestor where the ancestor's ID is
//   a prefix of (or identical to) the window owner process's ID.
// - std::nullopt if the `id` is an invalid window id or the window owner
//   process does not have a bundle identifier, or the process running this
//   function does not have permission to access bundle IDs of other processes.
CONTENT_EXPORT std::optional<std::string> GetMainBundleIdForNativeWindowId(
    DesktopMediaID::Id id);

using GetParentPidForTestingCallback = pid_t (*)(pid_t);
using GetBundleIdForProcessForTestingCallback =
    std::optional<std::string> (*)(pid_t);
using GetWindowOwnerPidForTestingCallback = pid_t (*)(DesktopMediaID::Id);

CONTENT_EXPORT void SetGetParentPidForTesting(
    GetParentPidForTestingCallback func);
CONTENT_EXPORT void SetGetBundleIdForProcessForTesting(
    GetBundleIdForProcessForTestingCallback func);
CONTENT_EXPORT void SetGetWindowOwnerPidForTesting(
    GetWindowOwnerPidForTestingCallback func);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_CAPTURE_UTIL_MAC_H_
