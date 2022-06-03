// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

namespace media {

namespace {
std::string VideoCaptureWinBackendEnumToString(
    VideoCaptureWinBackend backend_type) {
  switch (backend_type) {
    case VideoCaptureWinBackend::kDirectShow:
      return "DirectShow";
    case VideoCaptureWinBackend::kMediaFoundation:
      return "MediaFoundation";
    default:
      // The default case is only needed to avoid a compiler warning.
      NOTREACHED();
      return "Unknown";
  }
}

}  // anonymous namespace

bool IsHighResolution(const VideoCaptureFormat& format) {
  return format.frame_size.width() > 1920;
}

void LogVideoCaptureWinBackendUsed(VideoCaptureWinBackendUsed value) {
  base::UmaHistogramEnumeration("Media.VideoCapture.Windows.BackendUsed", value,
                                VideoCaptureWinBackendUsed::kCount);
}

void LogWindowsImageCaptureOutcome(VideoCaptureWinBackend backend_type,
                                   ImageCaptureOutcome value,
                                   bool is_high_res) {
  static const std::string kHistogramPrefix(
      "Media.VideoCapture.Windows.ImageCaptureOutcome.");
  static const std::string kAnyResSuffix("AnyRes");
  static const std::string kHighResSuffix("HighRes");
  const std::string backend_string =
      VideoCaptureWinBackendEnumToString(backend_type);
  base::UmaHistogramEnumeration(
      kHistogramPrefix + backend_string + kAnyResSuffix, value,
      ImageCaptureOutcome::kCount);
  if (is_high_res) {
    base::UmaHistogramEnumeration(
        kHistogramPrefix + backend_string + kHighResSuffix, value,
        ImageCaptureOutcome::kCount);
  }
}

void LogNumberOfRetriesNeededToWorkAroundMFInvalidRequest(
    MediaFoundationFunctionRequiringRetry function,
    int retry_count) {
  switch (function) {
    case MediaFoundationFunctionRequiringRetry::kGetDeviceStreamCount:
      UMA_HISTOGRAM_COUNTS_1000(
          "Media.VideoCapture.Windows."
          "NumberOfRetriesNeededForMFGetDeviceStreamCount",
          retry_count);
      break;
    case MediaFoundationFunctionRequiringRetry::kGetDeviceStreamCategory:
      UMA_HISTOGRAM_COUNTS_1000(
          "Media.VideoCapture.Windows."
          "NumberOfRetriesNeededForMFGetDeviceStreamCategory",
          retry_count);
      break;
    case MediaFoundationFunctionRequiringRetry::kGetAvailableDeviceMediaType:
      UMA_HISTOGRAM_COUNTS_1000(
          "Media.VideoCapture.Windows."
          "NumberOfRetriesNeededForMFGetAvailableDeviceMediaType",
          retry_count);
      break;
  }
}

}  // namespace media
