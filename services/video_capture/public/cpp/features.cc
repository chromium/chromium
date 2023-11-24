// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/features.h"

namespace video_capture::features {

// Enables video capture device monitoring in video capture service instead of
// the browser process. Currently implemented only for mac.
#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kCameraMonitoringInVideoCaptureService,
             "CameraMonitoringInVideoCaptureService",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace video_capture::features
