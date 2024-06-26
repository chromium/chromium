// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/features.h"

namespace video_capture::features {

// Enables video capture device monitoring in video capture service instead of
// the browser process. Implementing now for Windows.
// Using a different feature name so as not to confuse with the old one used for
// Mac.
#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kWinCameraMonitoringInVideoCaptureService,
             "WinCameraMonitoringInVideoCaptureService",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace video_capture::features
