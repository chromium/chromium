// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_METRICS_H_
#define MEDIA_CAPTURE_VIDEO_WIN_METRICS_H_

#include "media/capture/video_capture_types.h"

namespace media {

enum class VideoCaptureWinBackend { kDirectShow, kMediaFoundation };

// These values are persisted to logs.
enum class VideoCaptureWinBackendUsed : int {
  kUsingDirectShowAsDefault = 0,
  kUsingMediaFoundationAsDefault = 1,
  kUsingDirectShowAsFallback = 2,
  kCount
};

// These values are persisted to logs.
enum class ImageCaptureOutcome : int {
  kSucceededUsingVideoStream = 0,
  kSucceededUsingPhotoStream = 1,
  kFailedUsingVideoStream = 2,
  kFailedUsingPhotoStream = 3,
  kCount
};

enum class MediaFoundationFunctionRequiringRetry {
  kGetDeviceStreamCount,
  kGetDeviceStreamCategory,
  kGetAvailableDeviceMediaType
};

void LogNumberOfRetriesNeededToWorkAroundMFInvalidRequest(
    MediaFoundationFunctionRequiringRetry function,
    int retry_count);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_METRICS_H_
