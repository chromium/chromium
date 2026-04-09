// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_RESOLUTION_DETAILS_H_
#define NET_DNS_PUBLIC_RESOLUTION_DETAILS_H_

#include <optional>

#include "base/time/time.h"
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

enum class SessionSource;
enum class HttpConnectionInfoCoarse;

// Contains information about a successful DoH (DNS-over-HTTPS) session.
// LINT.IfChange(DohResolutionDetails)
struct NET_EXPORT DohResolutionDetails {
  // Indicates how the DoH session was established (e.g., new session vs.
  // reused).
  SessionSource session_source;

  // Coarse representation of the protocol used for the DoH connection (e.g.,
  // HTTP/2, HTTP/3).
  HttpConnectionInfoCoarse connection_info;

  bool operator==(const DohResolutionDetails& other) const = default;
};
// LINT.ThenChange(//services/network/public/mojom/load_timing_internal_info.mojom:DohResolutionDetails)

// Details about how the host resolution was performed. Should be only used
// for logging and recording histograms.
// TODO(crbug.com/485672648): This struct exposes internal implementation
// details so it should be removed in the future.
// LINT.IfChange(ResolutionDetails)
struct NET_EXPORT ResolutionDetails {
  ResolutionSource source = ResolutionSource::kUnknown;

  // The time spent executing the resolver task that successfully completed the
  // request. This is set only when an async task (such as DNS, mDNS, or
  // system resolution) is executed and completes successfully. It remains
  // std::nullopt for cache hits or local synchronous resolutions. If multiple
  // tasks were attempted and fallback occurred, this represents the duration of
  // the single task that ultimately succeeded.
  std::optional<base::TimeDelta> task_completion_delay;

  // True if a Secure DNS (DoH) task was attempted as part of this request. This
  // is set to true regardless of whether the DoH query ultimately succeeded,
  // timed out, or failed and fell back to insecure DNS/system resolution.
  bool secure_dns_attempted = false;

  // Details of the DoH (DNS-over-HTTPS) session, if DoH was used. If multiple
  // DoH queries are executed concurrently, details from the first successfully
  // reported transaction are retained. Returns std::nullopt if DoH was not
  // used, or if DoH failed before session details could be retrieved.
  std::optional<DohResolutionDetails> doh_details;

  bool operator==(const ResolutionDetails& other) const = default;
};
// LINT.ThenChange(//services/network/public/mojom/load_timing_internal_info.mojom:ResolutionDetails)

}  // namespace net

#endif  // NET_DNS_PUBLIC_RESOLUTION_DETAILS_H_
