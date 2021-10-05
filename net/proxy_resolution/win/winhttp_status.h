// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_WINHTTP_STATUS_H_
#define NET_PROXY_RESOLUTION_WIN_WINHTTP_STATUS_H_

namespace net {

// TODO(niarci): Add an equivalent enum in the proxy_resolver_win mojo
//
// This describes the full set of failure points that could occur when calling
// into the proxy_resolver_win service. Further detail is additionally provided
// by the Windows error code, which will be supplied alongside this enum.
enum class WinHttpStatus {
  // No Error
  kOk,

  // Aborted by caller
  kAborted,

  // WinHttp binary failed to load
  kFunctionsNotLoaded,

  // WinHttpOpen() API failed
  kWinHttpOpenFailed,

  // WinHttpSetTimeouts() API failed
  kWinHttpSetTimeoutsFailed,

  // WinHttpSetStatusCallback() API failed
  kWinHttpSetStatusCallbackFailed,

  // WinHttpGetIEProxyConfigForCurrentUser() API failed
  kWinHttpGetIEProxyConfigForCurrentUserFailed,

  // WinHttpCreateProxyResolver() API failed
  kWinHttpCreateProxyResolverFailed,

  // WinHttpGetProxyForURLEx() failed
  kWinHttpGetProxyForURLExFailed,

  // Proxy resolution callback returned an error
  kStatusCallbackFailed,

  // WinHttpGetProxyResult() API failed
  kWinHttpGetProxyResultFailed,

  // WinHttpGetProxyResult() API unexpectedly returned an empty list
  kEmptyProxyList,
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_WINHTTP_STATUS_H_
