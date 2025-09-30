// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_CAPTURE_SWITCHES_H_
#define MEDIA_CAPTURE_CAPTURE_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/capture/capture_export.h"

namespace switches {

CAPTURE_EXPORT extern const char kVideoCaptureUseGpuMemoryBuffer[];
CAPTURE_EXPORT extern const char kDisableVideoCaptureUseGpuMemoryBuffer[];

CAPTURE_EXPORT bool IsVideoCaptureUseGpuMemoryBufferEnabled();

#if BUILDFLAG(IS_WIN)
CAPTURE_EXPORT bool IsMediaFoundationCameraUsageMonitoringEnabled();
#endif

}  // namespace switches

namespace features {

CAPTURE_EXPORT BASE_DECLARE_FEATURE(kExcludePipFromScreenCapture);

#if !BUILDFLAG(IS_ANDROID)
CAPTURE_EXPORT BASE_DECLARE_FEATURE(kTabCaptureInfobarLinks);
#endif  // !BUILDFLAG(IS_ANDROID)

#if defined(WEBRTC_USE_PIPEWIRE)
CAPTURE_EXPORT BASE_DECLARE_FEATURE(kWebRtcPipeWireCamera);
#endif  // defined(WEBRTC_USE_PIPEWIRE)

#if BUILDFLAG(IS_WIN)
CAPTURE_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationCameraUsageMonitoring);
#endif

}  // namespace features

#endif  // MEDIA_CAPTURE_CAPTURE_SWITCHES_H_
