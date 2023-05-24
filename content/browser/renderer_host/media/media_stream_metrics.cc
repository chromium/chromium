// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
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
  kMaxValue = kDeviceInUse
};

void RecordMediaStreamRequestResult2(blink::mojom::MediaStreamType video_type,
                                     MediaStreamRequestResult2 result2) {
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

void RecordMediaDeviceUpdateResponseMetric(
    blink::mojom::MediaStreamType video_type,
    blink::mojom::MediaStreamRequestResult result) {
  using blink::mojom::MediaStreamRequestResult;
  switch (result) {
    case MediaStreamRequestResult::OK:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kOk);
      return;
    case MediaStreamRequestResult::PERMISSION_DENIED:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kPermissionDenied);
      return;
    case MediaStreamRequestResult::PERMISSION_DISMISSED:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kPermissionDismissed);
      return;
    case MediaStreamRequestResult::INVALID_STATE:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kInvalidState);
      return;
    case MediaStreamRequestResult::NO_HARDWARE:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kNoHardware);
      return;
    case MediaStreamRequestResult::INVALID_SECURITY_ORIGIN:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kInvalidSecurityOrigin);
      return;
    case MediaStreamRequestResult::TAB_CAPTURE_FAILURE:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kTabCaptureFailure);
      return;
    case MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kScreenCaptureFailure);
      return;
    case MediaStreamRequestResult::CAPTURE_FAILURE:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kCaptureFailure);
      return;
    case MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kConstraintNotSatisfied);
      return;
    case MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kTrackStartFailureAudio);
      return;
    case MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kTrackStartFailureVideo);
      return;
    case MediaStreamRequestResult::NOT_SUPPORTED:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kNotSupported);
      return;
    case MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kFailedDueToShutdown);
      return;
    case MediaStreamRequestResult::KILL_SWITCH_ON:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kKillSwitchOn);
      return;
    case MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kSystemPermissionDenied);
      return;
    case MediaStreamRequestResult::DEVICE_IN_USE:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kDeviceInUse);
      return;
    case MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS:
      break;
  }
  NOTREACHED();
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
    const url::Origin& main_frame_origin,
    blink::mojom::MediaStreamType video_type,
    blink::MediaStreamRequestType request_type,
    blink::mojom::MediaStreamRequestResult result) {
  NOTIMPLEMENTED();
}

}  // namespace content::media_stream_metrics
