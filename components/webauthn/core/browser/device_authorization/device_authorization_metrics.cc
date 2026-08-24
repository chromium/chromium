// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/device_authorization/device_authorization_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace webauthn {

void RecordDeviceAuthorizationFetchResult(
    DeviceAuthorizationFetchResultForUMA result) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.DeviceAuthorization.FetchResult", result);
}

void RecordDeviceAuthorizationHttpStatusOrNetError(
    int http_status_or_net_error) {
  base::UmaHistogramSparse(
      "WebAuthentication.DeviceAuthorization.HttpStatusOrNetError",
      http_status_or_net_error);
}

}  // namespace webauthn
