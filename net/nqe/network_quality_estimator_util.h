// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_NETWORK_QUALITY_ESTIMATOR_UTIL_H_
#define NET_NQE_NETWORK_QUALITY_ESTIMATOR_UTIL_H_

#include <stdint.h>

#include "net/base/net_export.h"

namespace net {

class HostPortPair;
class HostResolver;
class NetworkIsolationKey;
class URLRequest;

namespace nqe {

namespace internal {

// A unified compact representation of an IPv6 or an IPv4 address.
typedef uint64_t IPHash;

// Returns true if the host contained of |request.url()| is a host in a
// private Internet as defined by RFC 1918 or if the requests to it are not
// expected to generate useful network quality information. This includes
// localhost, hosts on private subnets, and hosts on subnets that are reserved
// for specific usage, and are unlikely to be used by public web servers.
//
// To make this determination, this method makes the best effort estimate
// including trying to resolve the host from the HostResolver's cache. This
// method is synchronous.
NET_EXPORT_PRIVATE bool IsRequestForPrivateHost(const URLRequest& request);

// Provides access to the method used internally by IsRequestForPrivateHost(),
// for testing.
NET_EXPORT_PRIVATE bool IsPrivateHostForTesting(
    HostResolver* host_resolver,
    const HostPortPair& host_port_pair,
    const NetworkIsolationKey& network_isolation_key);

}  // namespace internal

}  // namespace nqe

}  // namespace net

#endif  // NET_NQE_NETWORK_QUALITY_ESTIMATOR_UTIL_H_
