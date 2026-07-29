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
  // Refresh was successful and was triggered by this specific request.
  kRefreshed = 0,
  // Service is now initialized, refresh may still be needed.
  kInitializedService = 1,
  // Refresh endpoint was unreachable.
  kUnreachable = 2,
  // Refresh endpoint served a transient error.
  kServerError = 3,

  // kRefreshQuotaExceeded = 4,  // Replaced by `kSigningQuotaExceeded`.

  // Refresh failed and session was terminated. No further refresh needed.
  kFatalError = 5,
  // Signing quota exceeded.
  kSigningQuotaExceeded = 6,
  // Refresh was successful for the session, but this specific request did not
  // trigger it (it was a waiter), a new refresh may still be needed.
  kRefreshedAsWaiter = 7,
  // Transient local signing failure. Examples include canceled key operations.
  kTransientSigningError = 8,
  // Prewarm check found in-scope session does not need a refresh yet.
  kInScopeRefreshNotYetNeeded = 9,
  kMaxValue = kInScopeRefreshNotYetNeeded
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:DeviceBoundSessionRefreshResult,//services/network/public/mojom/device_bound_sessions.mojom:DeviceBoundSessionRefreshResult,//third_party/blink/public/devtools_protocol/domains/Network.pdl:DeviceBoundSessionRefreshResult,//content/browser/devtools/protocol/network_handler.cc:DeviceBoundSessionRefreshResult)

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_REFRESH_RESULT_H_
