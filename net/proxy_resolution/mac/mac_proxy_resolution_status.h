// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_MAC_MAC_PROXY_RESOLUTION_STATUS_H_
#define NET_PROXY_RESOLUTION_MAC_MAC_PROXY_RESOLUTION_STATUS_H_

namespace net {

// Enumerates the macOS-specific outcomes produced by the system proxy resolver.
// Keep in sync with proxy_resolver::mojom::MacProxyStatus.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(MacProxyResolutionStatus)
enum class MacProxyResolutionStatus {
  kOk = 0,
  kSystemConfigurationError = 1,
  kCFNetworkExecutePacScriptFailed = 2,
  kPacScriptFetchFailed = 3,
  kPacScriptExecutionFailed = 4,
  kEmptyProxyList = 5,
  kCFNetworkResolutionError = 6,

  kMaxValue = kCFNetworkResolutionError,
};
// LINT.ThenChange(//services/proxy_resolver/public/mojom/proxy_resolver.mojom:
//                 MacProxyStatus)

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_MAC_MAC_PROXY_RESOLUTION_STATUS_H_
