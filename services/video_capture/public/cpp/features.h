// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_FEATURES_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace video_capture::features {

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(VIDEO_CAPTURE)
BASE_DECLARE_FEATURE(kCameraMonitoringInVideoCaptureService);
#endif

}  // namespace video_capture::features

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_FEATURES_H_
