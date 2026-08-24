// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_METRICS_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_METRICS_H_

namespace webauthn {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DeviceAuthorizationFetchResultForUMA)
enum class DeviceAuthorizationFetchResultForUMA {
  kKeysFetched = 0,
  kReAuthChallenge = 1,
  kNetworkError = 2,
  kHttpError = 3,
  kProtoParseError = 4,
  kAlreadyInProgress = 5,
  kMaxValue = kAlreadyInProgress,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:DeviceAuthorizationFetchResult)

// Records the outcome of fetching device authorization keys.
void RecordDeviceAuthorizationFetchResult(
    DeviceAuthorizationFetchResultForUMA result);

// Records the HTTP status code or net error when fetching device
// authorization keys.
void RecordDeviceAuthorizationHttpStatusOrNetError(
    int http_status_or_net_error);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_METRICS_H_
