// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_UTIL_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_UTIL_MAC_H_

#include "content/common/content_export.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"

namespace content {

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
