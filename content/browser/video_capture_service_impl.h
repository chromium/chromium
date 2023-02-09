// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_VIDEO_CAPTURE_SERVICE_IMPL_H_
#define CONTENT_BROWSER_VIDEO_CAPTURE_SERVICE_IMPL_H_

#include "content/common/content_export.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom-forward.h"

namespace content {

// Enables a safe-mode VideoCaptureService.
// On macOS, this disables 3rd-party DAL plugins from being loaded.
// It currently has no effect on other platforms.
void EnableVideoCaptureServiceSafeMode();

}  // namespace content

#endif  // CONTENT_BROWSER_VIDEO_CAPTURE_SERVICE_IMPL_H_
