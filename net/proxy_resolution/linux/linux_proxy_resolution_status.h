// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_LINUX_LINUX_PROXY_RESOLUTION_STATUS_H_
#define NET_PROXY_RESOLUTION_LINUX_LINUX_PROXY_RESOLUTION_STATUS_H_

namespace net {

// Enumerates the outcomes produced by the Linux system proxy resolver, which
// is backed by the org.freedesktop.portal.ProxyResolver XDG desktop portal.
// Keep in sync with proxy_resolver::mojom::LinuxProxyStatus.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(LinuxProxyResolutionStatus)
enum class LinuxProxyResolutionStatus {
  kOk = 0,
  // The XDG desktop portal service is not available.
  kPortalUnavailable = 1,
  // The ProxyResolver.Lookup D-Bus call failed.
  kDBusError = 2,
  // The portal returned an empty proxy list.
  kEmptyProxyList = 3,
  // The portal returned entries, but none of them could be parsed.
  kInvalidResponse = 4,
  // The request was aborted before completion (e.g. during shutdown).
  kAborted = 5,

  kMaxValue = kAborted,
};
// LINT.ThenChange(//services/proxy_resolver/public/mojom/system_proxy_resolver.mojom:LinuxProxyStatus,
// //tools/metrics/histograms/metadata/net/enums.xml:LinuxProxyResolutionStatus)

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_LINUX_LINUX_PROXY_RESOLUTION_STATUS_H_
