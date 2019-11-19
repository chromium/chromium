// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_EVENT_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_EVENT_H_

#include "base/time/time.h"
#include "build/build_config.h"

namespace video_capture {
namespace uma {

// Used for logging capture events.
// Elements in this enum should not be deleted or rearranged; the only
// permitted operation is to add new elements before
// NUM_VIDEO_CAPTURE_SERVICE_EVENT.
enum VideoCaptureServiceEvent {
  BROWSER_USING_LEGACY_CAPTURE = 0,
  BROWSER_CONNECTING_TO_SERVICE = 1,
  SERVICE_STARTED = 2,
  SERVICE_SHUTTING_DOWN_BECAUSE_NO_CLIENT = 3,
  SERVICE_LOST_CONNECTION_TO_BROWSER = 4,
  BROWSER_LOST_CONNECTION_TO_SERVICE = 5,
  BROWSER_CLOSING_CONNECTION_TO_SERVICE = 6,  // No longer in use
  BROWSER_CLOSING_CONNECTION_TO_SERVICE_AFTER_ENUMERATION_ONLY = 7,
  BROWSER_CLOSING_CONNECTION_TO_SERVICE_AFTER_CAPTURE = 8,
  SERVICE_SHUTDOWN_TIMEOUT_CANCELED = 9,
  NUM_VIDEO_CAPTURE_SERVICE_EVENT
};

#if defined(OS_MACOSX)
enum MacbookRetryGetDeviceInfosEvent {
  PROVIDER_RECEIVED_ZERO_INFOS_STOPPING_SERVICE = 0,
  PROVIDER_SERVICE_STOPPED_ISSUING_RETRY = 1,
  PROVIDER_RECEIVED_ZERO_INFOS_FROM_RETRY_GIVING_UP = 2,
  PROVIDER_RECEIVED_NONZERO_INFOS_FROM_RETRY = 3,
  PROVIDER_NOT_ATTEMPTING_RETRY_BECAUSE_ALREADY_PENDING = 4,
  AVF_RECEIVED_ZERO_INFOS_FIRST_TRY_FIRST_ATTEMPT = 5,
  AVF_RECEIVED_NONZERO_INFOS_FIRST_TRY_FIRST_ATTEMPT = 6,
  AVF_RECEIVED_ZERO_INFOS_FIRST_TRY_NONFIRST_ATTEMPT = 7,
  AVF_RECEIVED_NONZERO_INFOS_FIRST_TRY_NONFIRST_ATTEMPT = 8,
  AVF_RECEIVED_ZERO_INFOS_RETRY = 9,
  AVF_RECEIVED_NONZERO_INFOS_RETRY = 10,
  AVF_DEVICE_COUNT_CHANGED_FROM_ZERO_TO_POSITIVE = 11,
  AVF_DEVICE_COUNT_CHANGED_FROM_POSITIVE_TO_ZERO = 12,
  AVF_DROPPED_DESCRIPTORS_AT_FACTORY = 13,
  SERVICE_DROPPED_DEVICE_INFOS_REQUEST_ON_FIRST_TRY = 14,
  SERVICE_DROPPED_DEVICE_INFOS_REQUEST_ON_RETRY = 15,
  NUM_RETRY_GET_DEVICE_INFOS_EVENT
};
#endif

void LogVideoCaptureServiceEvent(VideoCaptureServiceEvent event);

void LogDurationFromLastConnectToClosingConnectionAfterEnumerationOnly(
    base::TimeDelta duration);
void LogDurationFromLastConnectToClosingConnectionAfterCapture(
    base::TimeDelta duration);
void LogDurationFromLastConnectToConnectionLost(base::TimeDelta duration);
void LogDurationUntilReconnectAfterEnumerationOnly(base::TimeDelta duration);
void LogDurationUntilReconnectAfterCapture(base::TimeDelta duration);

#if defined(OS_MACOSX)
void LogMacbookRetryGetDeviceInfosEvent(MacbookRetryGetDeviceInfosEvent event);
#endif

}  // namespace uma
}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_EVENT_H_
