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

void RecordMediaDeviceUpdateResponseMetric(
    blink::mojom::MediaStreamType video_type,
    blink::mojom::MediaStreamRequestResult result) {
  switch (video_type) {
    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
      base::UmaHistogramEnumeration(
          "Media.MediaStreamManager.DesktopVideoDeviceUpdate2", result);
      return;
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
      base::UmaHistogramEnumeration(
          "Media.MediaStreamManager.DisplayVideoDeviceUpdate2", result);
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
  ukm::UkmRecorder* const recorder = ukm::UkmRecorder::Get();
  if (video_type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE &&
      request_type == blink::MEDIA_GENERATE_STREAM) {
    ukm::builders::MediaStream_Device(ukm_source_id)
        .SetVideoCaptureGenerateStreamResult2(static_cast<int64_t>(result))
        .Record(recorder);
  }
}

}  // namespace content::media_stream_metrics
