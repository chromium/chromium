// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_WINHTTP_STATUS_H_
#define NET_PROXY_RESOLUTION_WIN_WINHTTP_STATUS_H_

namespace net {

// This describes the full set of failure points that could occur when calling
// into the proxy_resolver_win service. Further detail is additionally provided
// by the Windows error code, which will be supplied alongside this enum.
//
// Keep in sync with proxy_resolver.mojom.WinHttpStatus.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(WinHttpStatus)
enum class WinHttpStatus {
  // No Error.
  kOk = 0,

  // Aborted by caller.
  kAborted = 1,

  // WinHttpOpen() API failed.
  kWinHttpOpenFailed = 2,

  // WinHttpSetTimeouts() API failed.
  kWinHttpSetTimeoutsFailed = 3,

  // WinHttpSetStatusCallback() API failed.
  kWinHttpSetStatusCallbackFailed = 4,

  // WinHttpGetIEProxyConfigForCurrentUser() API failed.
  kWinHttpGetIEProxyConfigForCurrentUserFailed = 5,

  // WinHttpCreateProxyResolver() API failed.
  kWinHttpCreateProxyResolverFailed = 6,

  // WinHttpGetProxyForURLEx() API failed.
  kWinHttpGetProxyForURLExFailed = 7,

  // Proxy resolution callback returned an error.
  kStatusCallbackFailed = 8,

  // WinHttpGetProxyResult() API failed.
  kWinHttpGetProxyResultFailed = 9,

  // WinHttpGetProxyResult() API unexpectedly returned an empty list.
  kEmptyProxyList = 10,

  kMaxValue = kEmptyProxyList,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:WinHttpStatus)

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_WINHTTP_STATUS_H_
