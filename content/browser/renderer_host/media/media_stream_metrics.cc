// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/origin.h"

namespace content::media_stream_metrics {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MediaStreamRequestResult2 {
  kOk = 0,
  kPermissionDenied = 1,
  kPermissionDismissed = 2,
  kInvalidState = 3,
  kNoHardware = 4,
  kInvalidSecurityOrigin = 5,
  kTabCaptureFailure = 6,
  kScreenCaptureFailure = 7,
  kCaptureFailure = 8,
  kConstraintNotSatisfied = 9,
  kTrackStartFailureAudio = 10,
  kTrackStartFailureVideo = 11,
  kNotSupported = 12,
  kFailedDueToShutdown = 13,
  kKillSwitchOn = 14,
  kSystemPermissionDenied = 15,
  kDeviceInUse = 16,
  kRequestCancelled = 17,
  kMaxValue = kRequestCancelled
};

MediaStreamRequestResult2 MapResultToResult2(
    blink::mojom::MediaStreamRequestResult result) {
  using blink::mojom::MediaStreamRequestResult;
  switch (result) {
    case MediaStreamRequestResult::OK:
      return MediaStreamRequestResult2::kOk;
    case MediaStreamRequestResult::PERMISSION_DENIED:
      return MediaStreamRequestResult2::kPermissionDenied;
    case MediaStreamRequestResult::PERMISSION_DISMISSED:
      return MediaStreamRequestResult2::kPermissionDismissed;
    case MediaStreamRequestResult::INVALID_STATE:
      return MediaStreamRequestResult2::kInvalidState;
    case MediaStreamRequestResult::NO_HARDWARE:
      return MediaStreamRequestResult2::kNoHardware;
    case MediaStreamRequestResult::INVALID_SECURITY_ORIGIN:
      return MediaStreamRequestResult2::kInvalidSecurityOrigin;
    case MediaStreamRequestResult::TAB_CAPTURE_FAILURE:
      return MediaStreamRequestResult2::kTabCaptureFailure;
    case MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE:
      return MediaStreamRequestResult2::kScreenCaptureFailure;
    case MediaStreamRequestResult::CAPTURE_FAILURE:
      return MediaStreamRequestResult2::kCaptureFailure;
    case MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED:
      return MediaStreamRequestResult2::kConstraintNotSatisfied;
    case MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO:
      return MediaStreamRequestResult2::kTrackStartFailureAudio;
    case MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO:
      return MediaStreamRequestResult2::kTrackStartFailureVideo;
    case MediaStreamRequestResult::NOT_SUPPORTED:
      return MediaStreamRequestResult2::kNotSupported;
    case MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN:
      return MediaStreamRequestResult2::kFailedDueToShutdown;
    case MediaStreamRequestResult::KILL_SWITCH_ON:
      return MediaStreamRequestResult2::kKillSwitchOn;
    case MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED:
      return MediaStreamRequestResult2::kSystemPermissionDenied;
    case MediaStreamRequestResult::DEVICE_IN_USE:
      return MediaStreamRequestResult2::kDeviceInUse;
    case MediaStreamRequestResult::REQUEST_CANCELLED:
      return MediaStreamRequestResult2::kRequestCancelled;
    case MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS:
      break;
  }
  NOTREACHED();
}

void RecordMediaDeviceUpdateResponseMetric(
    blink::mojom::MediaStreamType video_type,
    blink::mojom::MediaStreamRequestResult result) {
  MediaStreamRequestResult2 result2 = MapResultToResult2(result);
  switch (video_type) {
    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
      base::UmaHistogramEnumeration(
          "Media.MediaStreamManager.DesktopVideoDeviceUpdate", result2);
      return;
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
      base::UmaHistogramEnumeration(
          "Media.MediaStreamManager.DisplayVideoDeviceUpdate", result2);
      return;
    default:
      return;
  }
}

}  // namespace

void RecordMediaStreamRequestResponseMetric(
    blink::mojom::MediaStreamType video_type,
    blink::MediaStreamRequestType request_type,
    blink::mojom::MediaStreamRequestResult result) {
  switch (request_type) {
    case blink::MEDIA_DEVICE_UPDATE:
      RecordMediaDeviceUpdateResponseMetric(video_type, result);
      return;
    case blink::MEDIA_DEVICE_ACCESS:
    case blink::MEDIA_GENERATE_STREAM:
    case blink::MEDIA_GET_OPEN_DEVICE:
    case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY:
      return;
  }
}

void RecordMediaStreamRequestResponseUKM(
    ukm::SourceId ukm_source_id,
    blink::mojom::MediaStreamType video_type,
    blink::MediaStreamRequestType request_type,
    blink::mojom::MediaStreamRequestResult result) {
  MediaStreamRequestResult2 result2 = MapResultToResult2(result);
  ukm::UkmRecorder* const recorder = ukm::UkmRecorder::Get();
  if (video_type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE &&
      request_type == blink::MEDIA_GENERATE_STREAM) {
    ukm::builders::MediaStream_Device(ukm_source_id)
        .SetVideoCaptureGenerateStreamResult(static_cast<int64_t>(result2))
        .Record(recorder);
  }
}

}  // namespace content::media_stream_metrics
