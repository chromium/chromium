// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_RESOLUTION_DETAILS_H_
#define NET_DNS_PUBLIC_RESOLUTION_DETAILS_H_

#include "net/base/net_export.h"

namespace net {

// The source of the host resolution result.
enum class ResolutionSource {
  kUnknown = 0,
  // Hit in the host cache.
  kCache = 1,
  // Resolved from a local file or synchronous resolution (e.g. hosts file,
  // IP literals, localhost, config presets).
  kLocal = 2,
  // Resolved via insecure DNS queries.
  kInsecure = 3,
  // Resolved via secure DNS queries (DoH).
  kSecure = 4,
  // Resolved via the system resolver (e.g. getaddrinfo).
  kSystem = 5,
  // Resolved via the platform specific DNS API.
  kPlatform = 6,
  // Resolved via Multicast DNS.
  kMdns = 7,
  // Resolve resulted from a NAT64 translation.
  kNat64 = 8,
};

// Details about how the host resolution was performed. Should be only used
// for logging and recording histograms.
struct NET_EXPORT ResolutionDetails {
  ResolutionSource source = ResolutionSource::kUnknown;

  bool operator==(const ResolutionDetails& other) const = default;
};

}  // namespace net

#endif  // NET_DNS_PUBLIC_RESOLUTION_DETAILS_H_
