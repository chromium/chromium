// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SESSION_USAGE_H_
#define NET_BASE_SESSION_USAGE_H_

namespace net {

// This type distinguishes sessions carrying traffic through the destination
// host from sessions carrying traffic directly to the host. Credentials such
// as cookies are attached to `kDestination` sessions, but not to `kProxy`
// sessions. This type is used in QUIC and SPDY session keys, together with a
// proxy chain and host-port pair, to prevent pooling such sessions together.
//
// Examples:
//
// A session with no proxies at all will have a direct proxy chain and
// `session_usage = kDestination`.
//
// A session to "dest" carried over one or more proxies will have those
// proxies in its proxy chain, "dest" in its host-port pair, and `session_usage
// = kDestination`.
//
// A session over "proxyA" to "proxyB" which is carrying tunneled traffic to
// "dest" will have "proxyA" in its proxy chain, "proxyB in
// its host-port pair, and `session_usage = kProxy`.
enum class SessionUsage {
  // This session is used for a connection to the destination host.
  kDestination,
  // This session is used to proxy traffic to other destinations.
  kProxy,
};

}  // namespace net

#endif  // NET_BASE_SESSION_USAGE_H_
