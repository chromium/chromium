// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_UTIL_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_UTIL_MAC_H_

#include "content/common/content_export.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"

namespace content {

// Retrieves the ApplicationAudioCaptureId for the process identified by `pid`.
//
// For a specific list of Chromium-based browsers and PWAs installed by them,
// this function returns a ApplicationAudioCaptureId that contains both a Bundle
// ID and a PID. The Bundle ID is truncated to its base prefix (removing
// components like development variants or PWA identifiers). For browsers, the
// returned PID is the browser's main process PID. For PWAs, the returned PID is
// the PID of the browser that installed the PWA. For example:
// "com.google.Chrome.beta" will return
// ApplicationAudioCaptureId{"com.google.Chrome", PID_of_Chrome_beta_process}.
// "org.chromium.Chromium.app.a1b2c3" (a PWA installed by Chromium) will return
// ApplicationAudioCaptureId{"org.chromium.Chromium", PID_of_Chromium_process}.
//
// For other non-Chromium applications, it returns the application's unchanged
// Bundle ID and an empty pid.
//
// Returns std::nullopt if the process does not exist or is not a bundled
// application, or if `pid` is a PWA, and there are no, or more than one,
// running apps with the PWA's browser Bundle ID.
// TODO(crbug.com/507803904): Add RTC logs.
CONTENT_EXPORT std::optional<desktop_capture::ApplicationAudioCaptureId>
GetApplicationAudioCaptureIdForProcess(pid_t pid);

// Resolves a DesktopMediaID into its main ApplicationAudioCaptureId.
// Must be called from a sequenced thread. Callback will be invoked on the
// calling sequence.
// This is the content-internal implementation of the public function
// content::desktop_capture::GetApplicationAudioCaptureId.
CONTENT_EXPORT void GetApplicationAudioCaptureIdInternal(
    DesktopMediaID desktop_media_id,
    desktop_capture::GetApplicationAudioCaptureIdCallback callback);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_UTIL_MAC_H_
