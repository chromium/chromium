// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/uma/video_capture_service_event.h"

#include "base/metrics/histogram_macros.h"

namespace video_capture {
namespace uma {

void LogVideoCaptureServiceEvent(VideoCaptureServiceEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCaptureService.Event", event,
                            NUM_VIDEO_CAPTURE_SERVICE_EVENT);
  DVLOG(4) << "Logged VideoCaptureServiceEvent " << event;
}

void LogDurationFromLastConnectToClosingConnectionAfterEnumerationOnly(
    base::TimeDelta duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.VideoCaptureService."
      "DurationFromLastConnectToClosingConnectionAfterEnumerationOnly",
      duration, base::TimeDelta(), base::TimeDelta::FromMinutes(1), 50);
  DVLOG(4) << "Logged "
              "DurationFromLastConnectToClosingConnectionAfterEnumerationOnl"
              "y";
}

void LogDurationFromLastConnectToClosingConnectionAfterCapture(
    base::TimeDelta duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.VideoCaptureService."
      "DurationFromLastConnectToClosingConnectionAfterCapture",
      duration, base::TimeDelta(), base::TimeDelta::FromDays(21), 50);
  DVLOG(4) << "Logged DurationFromLastConnectToClosingConnectionAfterCapture";
}

void LogDurationFromLastConnectToConnectionLost(base::TimeDelta duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.VideoCaptureService.DurationFromLastConnectToConnectionLost",
      duration, base::TimeDelta(), base::TimeDelta::FromDays(21), 50);
  DVLOG(4) << "Logged DurationFromLastConnectToConnectionLost";
}

void LogDurationUntilReconnectAfterEnumerationOnly(base::TimeDelta duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.VideoCaptureService.DurationUntilReconnectAfterEnumerationOnly",
      duration, base::TimeDelta(), base::TimeDelta::FromDays(7), 50);
  DVLOG(4) << "Logged DurationUntilReconnectAfterEnumerationOnly";
}

void LogDurationUntilReconnectAfterCapture(base::TimeDelta duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.VideoCaptureService.DurationUntilReconnectAfterCapture", duration,
      base::TimeDelta(), base::TimeDelta::FromDays(7), 50);
  DVLOG(4) << "Logged DurationUntilReconnectAfterCapture";
}

#if defined(OS_MACOSX)
void LogMacbookRetryGetDeviceInfosEvent(MacbookRetryGetDeviceInfosEvent event) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.VideoCapture.MacBook.RetryGetDeviceInfosEvent", event,
      NUM_RETRY_GET_DEVICE_INFOS_EVENT);
  DVLOG(4) << "Logged MacbookRetryGetDeviceInfosEvent " << event;
}
#endif

}  // namespace uma
}  // namespace video_capture
