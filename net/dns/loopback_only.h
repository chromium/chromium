// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_LOOPBACK_ONLY_H_
#define NET_DNS_LOOPBACK_ONLY_H_

#include <type_traits>
#include "base/functional/callback.h"
#include "net/base/net_export.h"

namespace net {

// Results in true if it can determine that only loopback addresses are
// configured. I.e. if at most 127.0.0.1 and ::1 are routable. Note this results
// in true as long as there are no non-loopback, active internet connections.
// There do not have to be any loopback interfaces for this to result in true.
// Also results in false if it cannot determine this.
//
// The result is always passed to `finished_cb`, which is posted to the current
// thread.
//
// If the result cannot be computed without blocking, this will post a
// CONTINUE_ON_SHUTDOWN task to a thread pool which can take 40-100ms on some
// systems.
//
// IMPORTANT NOTE: the Posix (except Android) and Fuchsia implementations
// consider IPv6 link-local addresses to be loopback, because network interfaces
// may be configured with IPv6 link-local addresses regardless of network
// connectivity and are not used for network connections. IPv4 link-local
// addresses are part of APIPA, can be used for network connections, and are not
// typically configured automatically for network interfaces. See
// https://codereview.chromium.org/3331024 when this behavior was originally
// added, and the linked bug https://crbug.com/55041 for an example. Otherwise,
// if IPv6 link-local addresses are not considered loopback, then
// host_resolver_system_task.cc will always use AI_ADDRCONFIG for getaddrinfo()
// on a system with link-local IPv6 addresses, and then because there are no
// non-loopback IPv4 addresses configured, getaddrinfo() will refuse to resolve
// any name to any IPv4 address. This is problematic because then localhost will
// not resolve to 127.0.0.1.
//
// See https://fedoraproject.org/wiki/QA/Networking/NameResolution/ADDRCONFIG
// for a writeup on the issues that AI_ADDRCONFIG, as well as its handling of
// IPv6 link-local addresses, can cause.
void NET_EXPORT
RunHaveOnlyLoopbackAddressesJob(base::OnceCallback<void(bool)> finished_cb);

}  // namespace net

#endif  // NET_DNS_LOOPBACK_ONLY_H_
