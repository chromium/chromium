// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_MODEL_CAPTIVE_PORTAL_METRICS_H_
#define IOS_CHROME_BROWSER_SSL_MODEL_CAPTIVE_PORTAL_METRICS_H_

#include "components/captive_portal/core/captive_portal_types.h"

// Enum used to record the captive portal detection result.
enum class CaptivePortalStatus {
  UNKNOWN = 0,
  OFFLINE = 1,
  ONLINE = 2,
  PORTAL = 3,
  PROXY_AUTH_REQUIRED = 4,
  COUNT
};

// Returns the associated CaptivePortalStatus value for logging to UMA metrics
// based on detection `result`.
CaptivePortalStatus CaptivePortalStatusFromDetectionResult(
    captive_portal::CaptivePortalResult result);

#endif  // IOS_CHROME_BROWSER_SSL_MODEL_CAPTIVE_PORTAL_METRICS_H_
