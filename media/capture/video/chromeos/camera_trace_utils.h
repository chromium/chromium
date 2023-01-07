// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_TRACE_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_TRACE_UTILS_H_

#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace media {

enum class CameraTraceEvent {
  kJpegDecoding,
  kStabilize3A,
  kConfigureStreams,
  kCaptureStream,
  kCaptureRequest,
};

// Generates unique track by given |event|, |primary_id| and |secondary_id|. For
// |secondary_id|, only the last 16 bits will be used.
perfetto::Track GetTraceTrack(CameraTraceEvent event,
                              int primary_id = 0,
                              int secondary_id = 0);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_TRACE_UTILS_H_
