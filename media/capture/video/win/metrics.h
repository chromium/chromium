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

// Outcome of attempting to deliver a captured D3D11 frame as a
// gpu::ClientSharedImage from the capture buffer pool. Anything other than
// kDeliveredSharedImage means the frame was delivered from system memory
// instead, and names the reason the texture path did not produce it.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(MediaFoundationSharedImageDeliveryResult)
enum class MediaFoundationSharedImageDeliveryResult {
  kDeliveredSharedImage = 0,
  kSlotCreationFailed = 1,
  kSlotCopyFailed = 2,
  kPoolExhausted = 3,
  kMaxValue = kPoolExhausted,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/media/enums.xml:MediaFoundationSharedImageDeliveryResult)

void LogNumberOfRetriesNeededToWorkAroundMFInvalidRequest(
    MediaFoundationFunctionRequiringRetry function,
    int retry_count);

// Records the shared-image delivery outcome.
void LogMediaFoundationSharedImageDeliveryResult(
    MediaFoundationSharedImageDeliveryResult result);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_METRICS_H_
