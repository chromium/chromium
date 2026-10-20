// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_ENUMERATION_MODE_UTIL_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_ENUMERATION_MODE_UTIL_H_

#include "build/build_config.h"
#include "media/capture/capture_switches.h"
#include "media/capture/mojom/video_capture_types.mojom-shared.h"

namespace media {

// Returns the default video enumeration mode: `kDevicesOnly` on Windows when
// `kWinDeviceEnumerationWithoutActivation` is enabled, and
// `kDevicesWithCapabilities` otherwise.
inline mojom::VideoCaptureDeviceEnumerationMode
GetDefaultVideoEnumerationMode() {
#if BUILDFLAG(IS_WIN)
  if (switches::IsWinDeviceEnumerationWithoutActivationEnabled()) {
    return mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly;
  }
#endif
  return mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities;
}

// Returns whether a result produced with `result_mode` can satisfy a request
// for `requested_mode`. A devices-with-capabilities enumeration is a superset
// of a devices-only enumeration, so it satisfies either request.
inline bool CanEnumerationResultSatisfyRequest(
    mojom::VideoCaptureDeviceEnumerationMode result_mode,
    mojom::VideoCaptureDeviceEnumerationMode requested_mode) {
  return result_mode == requested_mode ||
         result_mode ==
             mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities;
}

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_ENUMERATION_MODE_UTIL_H_
