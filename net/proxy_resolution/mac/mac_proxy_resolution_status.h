// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_MAC_MAC_PROXY_RESOLUTION_STATUS_H_
#define NET_PROXY_RESOLUTION_MAC_MAC_PROXY_RESOLUTION_STATUS_H_

namespace net {

// Enumerates the macOS-specific outcomes produced by the system proxy resolver.
enum class MacProxyResolutionStatus {
  kOk = 0,
  kSystemConfigurationError = 1,
  kCFNetworkExecutePacScriptFailed = 2,
  kPacScriptFetchFailed = 3,
  kPacScriptExecutionFailed = 4,
  kEmptyProxyList = 5,
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_MAC_MAC_PROXY_RESOLUTION_STATUS_H_
