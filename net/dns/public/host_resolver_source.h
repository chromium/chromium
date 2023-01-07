// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_HOST_RESOLVER_SOURCE_H_
#define NET_DNS_PUBLIC_HOST_RESOLVER_SOURCE_H_

namespace net {

// Enumeration to specify the allowed results source for HostResolver
// requests.
enum class HostResolverSource {
  // Resolver will pick an appropriate source. Results could come from DNS,
  // MulticastDNS, HOSTS file, etc.
  ANY,

  // Results will only be retrieved from the system or OS, eg via the
  // getaddrinfo() system call.
  SYSTEM,

  // Results will only come from DNS queries.
  DNS,

  // Results will only come from Multicast DNS queries.
  MULTICAST_DNS,

  // No external sources will be used. Results will only come from fast local
  // sources that are available no matter the source setting, e.g. cache, hosts
  // file, IP literal resolution, etc. Resolves with this setting are guaranteed
  // to finish synchronously.
  LOCAL_ONLY,

  MAX = LOCAL_ONLY
};

const HostResolverSource kHostResolverSources[] = {
    HostResolverSource::ANY, HostResolverSource::SYSTEM,
    HostResolverSource::DNS, HostResolverSource::MULTICAST_DNS,
    HostResolverSource::LOCAL_ONLY};

static_assert(
    std::size(kHostResolverSources) ==
        static_cast<unsigned>(HostResolverSource::MAX) + 1,
    "All HostResolverSource values should be in kHostResolverSources.");

}  // namespace net

#endif  // NET_DNS_PUBLIC_HOST_RESOLVER_SOURCE_H_
