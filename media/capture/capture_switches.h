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

#if defined(WEBRTC_USE_PIPEWIRE)
CAPTURE_EXPORT BASE_DECLARE_FEATURE(kWebRtcPipeWireCamera);
#endif  // defined(WEBRTC_USE_PIPEWIRE)

}  // namespace features

#endif  // MEDIA_CAPTURE_CAPTURE_SWITCHES_H_
