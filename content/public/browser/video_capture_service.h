// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_VIDEO_CAPTURE_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_VIDEO_CAPTURE_SERVICE_H_

#include "content/common/content_export.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

namespace content {

// Acquires a VideoCaptureService interface connected either to an in-process
// instance or an out-of-process instance. If out-of-process, the service
// process is launched lazily as needed and shut down when idle.
//
// This is callable from any thread, though when called from off of the UI
// thread, messages sent on the interface will incur an extra thread hop before
// going to the service.
CONTENT_EXPORT video_capture::mojom::VideoCaptureService&
GetVideoCaptureService();

// Provides an override for the reference returned by
// |GetVideoCaptureService()|. Call again with null to cancel the override
// before |service| is destroyed.
CONTENT_EXPORT void OverrideVideoCaptureServiceForTesting(
    video_capture::mojom::VideoCaptureService* service);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_VIDEO_CAPTURE_SERVICE_H_
