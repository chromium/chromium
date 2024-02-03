// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_METRICS_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_METRICS_H_

#include "base/containers/span.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "media/capture/video/video_capture_device_info.h"

namespace media {

CAPTURE_EXPORT
void LogCaptureDeviceMetrics(
    base::span<const media::VideoCaptureDeviceInfo> devices_info);

void LogCaptureDeviceHashedModelId(
    const media::VideoCaptureDeviceDescriptor& descriptor);

CAPTURE_EXPORT
void LogCaptureDeviceEffects(mojom::PhotoStatePtr photo_state);

CAPTURE_EXPORT
void LogCaptureCurrentDeviceResolution(int width, int height);

CAPTURE_EXPORT
void LogCaptureCurrentDevicePixelFormat(
    const media::VideoPixelFormat pixel_format);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_METRICS_H_
