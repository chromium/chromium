// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

namespace media {

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
