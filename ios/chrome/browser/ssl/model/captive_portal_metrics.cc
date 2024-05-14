// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/model/captive_portal_metrics.h"

#include "base/notreached.h"

CaptivePortalStatus CaptivePortalStatusFromDetectionResult(
    captive_portal::CaptivePortalResult result) {
  CaptivePortalStatus status;
  switch (result) {
    case captive_portal::RESULT_INTERNET_CONNECTED:
      status = CaptivePortalStatus::ONLINE;
      break;
    case captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL:
      status = CaptivePortalStatus::PORTAL;
      break;
    case captive_portal::RESULT_NO_RESPONSE:
      status = CaptivePortalStatus::UNKNOWN;
      break;
    case captive_portal::RESULT_COUNT:
      NOTREACHED_IN_MIGRATION();
      status = CaptivePortalStatus::UNKNOWN;
      break;
  }
  return status;
}
