// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_SOURCE_H_
#define NET_DNS_HOST_RESOLVER_SOURCE_H_

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
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_SOURCE_H_
