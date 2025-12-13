// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_REFRESH_RESULT_H_
#define NET_DEVICE_BOUND_SESSIONS_REFRESH_RESULT_H_

namespace net::device_bound_sessions {

// Records the outcome of an attempt to refresh.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DeviceBoundSessionRefreshResult)
enum class RefreshResult {
  kRefreshed = 0,             // Refresh was successful.
  kInitializedService = 1,    // Service is now initialized, refresh may still
                              // be needed.
  kUnreachable = 2,           // Refresh endpoint was unreachable.
  kServerError = 3,           // Refresh endpoint served a transient error.
  kRefreshQuotaExceeded = 4,  // Refresh quota exceeded. This is being
                              // replaced with `kSigningQuotaExceeded`.
  kFatalError = 5,            // Refresh failed and session was terminated. No
                              // further refresh needed.
  kSigningQuotaExceeded = 6,  // Signing quota exceeded.
  kMaxValue = kSigningQuotaExceeded
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:DeviceBoundSessionRefreshResult)

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_REFRESH_RESULT_H_
